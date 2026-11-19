/* MATERIAL device: POLY, the 8-voice sampler (the SAMPLE engine). (DISC, an earlier sample player, and TAPE, a looper,
 * were retired: DISC's number is KIT's.) */
#include "ch_internal.h"

ch_voice ch_voices[CH_POLY_VOICES];

/* ================= POLY ================= */
enum { P_SAMPLE, P_PITCH, P_START, P_LENGTH, P_LEVEL, P_LOOP, P_XFADE, P_ATTACK, P_DECAY, P_SUSTAIN, P_CURVE, P_VELOCITY, P_FAMOUNT, P_FSHAPE };
static unsigned smp_of(const ch_trk *t) { unsigned i = (unsigned)(PV(t, CH_MATERIAL, P_SAMPLE) + 0.5f); return i < CH_SAMPLES ? i : 0; }
static uint32_t voice_clock;

void ch_poly_action(ch_trk *t, unsigned ti, unsigned a, unsigned v)
{
    if (a == CH_POLY_LOAD && v < CH_SAMPLES) t->sample[1] = (uint8_t)v;
    else if (a == CH_POLY_LOOPMODE) t->poly_loop = (uint8_t)((t->poly_loop + 1) % 3);
    else if (a == CH_POLY_TRIG) ch_poly_note(t, ti, 60, v ? 100 : 0);
}

void ch_poly_note(ch_trk *t, unsigned ti, unsigned note, unsigned vel)
{
    if (!vel) {
        for (unsigned i = 0; i < CH_POLY_VOICES; ++i) {
            ch_voice *v = &ch_voices[i];
            if (v->active && v->track == ti && v->note == note && v->stage < 4) { v->stage = 4; v->rel_from = v->env; v->env_lag = 0; }
        }
        return;
    }
    const ch_sample *s = ch_smp[smp_of(t)];
    if (!s || !s->frames) return;
    ch_voice *pick = 0;
    for (unsigned i = 0; i < CH_POLY_VOICES && !pick; ++i) if (!ch_voices[i].active) pick = &ch_voices[i];
    if (!pick) {      /* steal the oldest: its last sound handed over (3 ms), the new note from its own attack */
        pick = &ch_voices[0];
        for (unsigned i = 1; i < CH_POLY_VOICES; ++i) if (ch_voices[i].age < pick->age) pick = &ch_voices[i];
    }
    float keep = pick->active ? pick->last : 0;
    memset(pick, 0, sizeof *pick);
    ch_reader_reset(&pick->rd, s);
    pick->track = (uint8_t)ti; pick->note = (uint8_t)note; pick->active = 1; pick->stage = 1;
    pick->vel = (float)vel / 127.0f; pick->age = ++voice_clock; pick->dir = 1;
    pick->pos = PV(t, CH_MATERIAL, P_START) * (float)s->frames;
    pick->env = 0; pick->dc = keep; pick->dcp = keep != 0 ? 1.0f : 0;   /* (fade, n, kill: 0) */
}

static float env_curve(float x, float c) { return c == 0 ? x : ch_exp2(ch_exp2(c * 2.5f) * ch_log2(ch_max(x, 1e-6f))); }

/* the sample at a place between frames, cubic. Nearly always its four frames lie in one decoded block the reader
 * holds: read straight from it. At a block's edge, the sample's ends or a block not yet decoded: the reader's way */
CH_SI float poly_read(ch_reader *r, float pos)
{
    if (pos >= 1.0f) {
        uint32_t i = (uint32_t)pos; unsigned o = i % CH_ADPCM_BLOCK; int32_t b = (int32_t)(i / CH_ADPCM_BLOCK);
        if (o >= 1 && o <= CH_ADPCM_BLOCK - 3 && i + 2 < r->s->frames) {
            const int16_t *p = r->blk[0] == b ? r->pcm[0] : r->blk[1] == b ? r->pcm[1] : 0;
            if (p) { p += o; return ch_hermite((float)p[-1], (float)p[0], (float)p[1], (float)p[2], pos - (float)i) * (1.0f / 32768); }
        }
    }
    return ch_reader_cubic(r, pos, 0);
}

/* Cost (the FM-1 misses its deadline as fuzz when eight notes are dear): nothing is divided per frame, a curved
 * envelope and the filter's coefficient are worked out every 8th frame, and the read is poly_read. The load governor
 * (ch_core.c) counts these notes with the drums' and synths' and fades the least of them (kill) when the render runs late */
void ch_poly_process(ch_trk *t, unsigned ti, float *L, float *R, unsigned n)
{
    for (unsigned i = 0; i < n; ++i) L[i] = R[i] = 0;
    const ch_sample *s = ch_smp[smp_of(t)];
    if (!s) return;
    unsigned pm = PMODE(t, CH_DEV_POLY, P_PITCH);
    float pitch = PV(t, CH_MATERIAL, P_PITCH) * 48;
    if (pm == 0) pitch = (float)(int)(pitch + (pitch < 0 ? -0.5f : 0.5f));
    float F = (float)s->frames;
    float start = PV(t, CH_MATERIAL, P_START) * F;
    float end = start + ch_max(32, PV(t, CH_MATERIAL, P_LENGTH) * (F - start));
    float lstart = start + PV(t, CH_MATERIAL, P_LOOP) * (end - start - 32);
    float xf = ch_sq(PV(t, CH_MATERIAL, P_XFADE)) * (end - lstart) * 0.5f;
    xf = ch_min(ch_max(xf, 0.002f * (float)s->rate), (end - lstart) * 0.5f);   /* a loop always crosses over at least 2 ms: no jump at the seam */
    float ixf = xf > 0.5f ? 1.0f / xf : 0;
    const float efade = 1.0f / (0.003f * (float)CH_RATE);   /* a one-shot fades out over its last 3 ms, not cut at its end */
    const float kinc = 1.0f / (0.005f * (float)CH_RATE);    /* a note the governor takes: 5 ms */
    float level = ch_level_gain(PV(t, CH_MATERIAL, P_LEVEL)) * 0.7f;
    float att = ch_sq(PV(t, CH_MATERIAL, P_ATTACK)) * 4.0f, dec = ch_sq(PV(t, CH_MATERIAL, P_DECAY)) * 4.0f;
    float sus = PV(t, CH_MATERIAL, P_SUSTAIN), crv = PV(t, CH_MATERIAL, P_CURVE);
    float velo = PV(t, CH_MATERIAL, P_VELOCITY);
    float famt = PV(t, CH_MATERIAL, P_FAMOUNT), fshape = PV(t, CH_MATERIAL, P_FSHAPE);
    /* attack/decay steps per frame; 1 ms floor keeps short envelopes from aliasing clicks */
    float ainc = 1.0f / (ch_max(att, 0.001f) * CH_RATE), dinc = 1.0f / (ch_max(dec, 0.002f) * CH_RATE);
    float lag = fshape > 0 ? 1 - ch_exp2(-1.442695f / (fshape * 0.4f * CH_RATE + 1)) : 1;
    float pw = fshape < 0 ? 1 + (-fshape) * 4 : 1;
    float afamt = ch_abs(famt) * 9;
    unsigned lm = t->poly_loop;
    for (unsigned vi = 0; vi < CH_POLY_VOICES; ++vi) {
        ch_voice *v = &ch_voices[vi];
        if (!v->active || v->track != ti) continue;
        if (v->rd.s != s) ch_reader_reset(&v->rd, s);
        if (v->kill && v->stage != 5) { v->stage = 5; v->rel_from = v->env; v->env_lag = 0; }
        float rate = (float)s->rate / CH_RATE * ch_semis((float)v->note - 60 + pitch);
        float vg = (1 - velo + velo * v->vel * v->vel) * level;
        float tail = end - rate * (0.003f * (float)CH_RATE);   /* a one-shot's last 3 ms begin here */
        float fg = 0;                                    /* the filter's coefficient, every 8th frame */
        v->n += n;
        for (unsigned i = 0; i < n; ++i) {
            /* envelope: linear segments shaped by CURVE; DECAY is decay and release */
            int ev = crv == 0 || (i & 7u) == 0;
            switch (v->stage) {
            case 1: v->env_lag += ainc; if (v->env_lag >= 1) { v->env_lag = 0; v->stage = 2; ev = 1; } if (ev) v->envt = env_curve(v->stage == 1 ? v->env_lag : 1, crv); break;
            case 2: v->env_lag += dinc; if (v->env_lag >= 1) { v->env_lag = 0; v->stage = 3; ev = 1; } if (ev) v->envt = sus + (1 - sus) * (1 - env_curve(v->stage == 2 ? v->env_lag : 1, -crv)); break;
            case 3: v->envt = sus; if (sus <= 0.0005f) v->active = 0; break;
            case 4: v->env_lag += dinc; if (v->env_lag >= 1) { v->active = 0; ev = 1; } if (ev) v->envt = v->rel_from * (1 - env_curve(ch_min(v->env_lag, 1), -crv)); break;
            default: v->env_lag += kinc; if (v->env_lag >= 1) { v->env_lag = 1; v->active = 0; } v->envt = v->rel_from * (1 - v->env_lag); break;
            }
            if (!v->active) break;
            if (crv == 0 || v->stage == 5) v->env = v->envt; else v->env += (v->envt - v->env) * 0.3f;   /* (a curved one: eased between its 8th frames) */
            float y = poly_read(&v->rd, v->pos);
            if (lm == 1 && ixf > 0 && v->pos > end - xf) {
                float a = (v->pos - (end - xf)) * ixf;
                y = y * (1 - a) + poly_read(&v->rd, v->pos - (end - lstart)) * a;
            }
            v->pos += v->dir ? rate : -rate;
            if (v->pos >= end) {
                if (lm == 0) { if (v->pos >= F + 4) v->active = 0; }   /* (a one-shot ends by its fade below; this: LENGTH pulled back up under a note past the sample) */
                else if (lm == 1) { v->pos -= end - lstart; if (v->pos >= end || v->pos < lstart) v->pos = lstart; }   /* (the loop moved under it) */
                else { v->dir = 0; v->pos = end - (v->pos - end); if (v->pos < lstart) v->pos = lstart; }
            } else if (!v->dir && v->pos <= lstart) { v->dir = 1; v->pos = lstart + (lstart - v->pos); if (v->pos > end) v->pos = lstart; }
            /* filter: fixed open base; the envelope (shaped) sweeps it */
            if (famt != 0) {
                if ((i & 7u) == 0 || fg == 0) {
                    float e = pw == 1 ? v->env : ch_exp2(pw * ch_log2(ch_max(v->env, 1e-6f)));
                    v->flt_env += (e - v->flt_env) * ch_min(1, lag * 8);
                    float fe = famt > 0 ? v->flt_env : 1 - v->flt_env;
                    fg = ch_svf_g(20000 * ch_exp2(-(1 - fe) * afamt));
                }
                y = ch_svf_tick(&v->f, y, fg, 1.2f).lp;
            } else if (v->stage < 4) v->flt_env = v->env;
            float g = v->env;
            if (lm == 0 && (v->fade > 0 || v->pos >= tail)) {   /* the one-shot's end (or LENGTH or START pulled its end under it): 3 ms down from here */
                v->fade += efade;
                if (v->fade >= 1) { v->fade = 1; v->active = 0; }
                g *= ch_fade_gain(1 - v->fade);
            }
            float o = y * g * vg;
            v->last = o;
            if (v->dcp > 0) { o += v->dc * ch_fade_gain(v->dcp); v->dcp -= efade; }   /* a taken voice's last sound, let go */
            L[i] += o; R[i] += o;
            if (!v->active) break;
        }
    }
}
