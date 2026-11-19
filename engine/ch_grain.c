/* GRAIN: a granular engine on the sample bank. A note starts a cloud of grains at its pitch, read from the sample
 * around POSITION (SPRAY scatters them), SIZE long, spread across the stereo field; the cloud rises over ATTACK,
 * goes on while the key is held and dies over RELEASE. One cloud a track (the last note), six grains at a time.
 * DENSITY is how many grains sound at once, half a grain (gaps between them) to five: a grain starts every
 * SIZE / DENSITY, so from 2 up their bell-shaped windows add to a level that holds, whatever SIZE is: a pad.
 * Each grain decodes the ADPCM block it reads from into its own small cache, so grains at any place cost the same.
 * Cost (the FM-1 misses its deadline as fuzz and clicks when an engine is dear): nothing is divided and no sine is
 * taken per frame; a grain's window is a slow quadrature pair stepped along, its frames one grain at a time. */
#include "ch_internal.h"

enum { R_SAMPLE, R_PITCH, R_POS, R_SIZE, R_DENS, R_SPRAY, R_SPREAD, R_REL, R_LEVEL, R_VEL, R_ATTACK };

uint8_t ch_grain_cap = CH_GRAINS1;                      /* the load governor's: the grains a cloud may sound at once (ch_render lowers it when the render runs late) */

void ch_grain_note(ch_trk *t, unsigned ti, unsigned note, unsigned vel)
{
    (void)ti;
    ch_graineng *e = &t->grain;
    if (!vel) { if (e->note == note) e->gate = 0; return; }
    if (!e->on) { e->env = 0; e->env_out = 0; e->next = 0; }   /* a new cloud from silence; a note into a sounding one carries its level on */
    e->note = (uint8_t)note; e->gate = 1; e->on = 1; e->vel = (float)vel * (1.0f / 127);
}

/* the block a grain reads from, decoded; and the frame after its last (the next block's first), so the read
 * between two frames never stops at a block's edge */
static void grain_block(ch_grain1 *g, const ch_sample *s, int32_t blk)
{
    const uint8_t *b = s->adpcm + (size_t)blk * CH_ADPCM_BLOCK_BYTES;
    ch_adpcm_decode_block(b, g->pcm);
    g->pcm[CH_ADPCM_BLOCK] = (uint32_t)(blk + 1) * CH_ADPCM_BLOCK < s->frames ? ch_adpcm_first(b + CH_ADPCM_BLOCK_BYTES) : g->pcm[CH_ADPCM_BLOCK - 1];
    g->blk = blk;
}

void ch_grain_process(ch_trk *t, unsigned ti, float *L, float *R, unsigned n)
{
    (void)ti;
    ch_graineng *e = &t->grain;
    for (unsigned i = 0; i < n; ++i) L[i] = R[i] = 0;
    if (!e->on) return;
    unsigned si = (unsigned)(PV(t, CH_MATERIAL, R_SAMPLE) + 0.5f); if (si >= CH_SAMPLES) si = 0;
    const ch_sample *s = ch_smp[si];
    if (!s || s->frames < 4 * CH_ADPCM_BLOCK) { e->on = 0; for (unsigned q = 0; q < CH_GRAINS1; ++q) e->g[q].left = 0; return; }
    if (e->smp != s) { e->smp = s; for (unsigned q = 0; q < CH_GRAINS1; ++q) e->g[q].left = 0; }   /* another sample: the grains of the last one end */
    float pitch = PV(t, CH_MATERIAL, R_PITCH) * 24;
    if (PMODE(t, CH_DEV_GRAIN, R_PITCH) == 0) pitch = (float)(int)(pitch + (pitch < 0 ? -0.5f : 0.5f));
    float rate = ch_semis((float)e->note - 60 + pitch) * ((float)s->rate / (float)CH_RATE);
    float F = (float)s->frames;
    float centre = PV(t, CH_MATERIAL, R_POS) * F;
    float size = ch_exp_map(PV(t, CH_MATERIAL, R_SIZE), 0.01f, 2.0f) * (float)CH_RATE;   /* a grain, frames out */
    float room = (F - 4) / ch_max(rate, 1e-3f);          /* the longest the sample has to give at this pitch */
    if (size > room * 0.8f) size = room * 0.8f;
    if (size < 64) size = 64;
    float ov = ch_exp_map(PV(t, CH_MATERIAL, R_DENS), 0.5f, 5.0f);   /* grains at once */
    float gap = size / ov;
    float spray = ch_sq(PV(t, CH_MATERIAL, R_SPRAY)) * F * 0.5f, spread = PV(t, CH_MATERIAL, R_SPREAD);
    float att = ch_sq(PV(t, CH_MATERIAL, R_ATTACK)) * 6.0f, rel = ch_exp_map(PV(t, CH_MATERIAL, R_REL), 0.02f, 6.0f);
    /* the level: two windows at once add to 1; more grains from scattered places add as their power does */
    float gain = ch_level_gain(PV(t, CH_MATERIAL, R_LEVEL)) * (1 - PV(t, CH_MATERIAL, R_VEL) * (1 - e->vel * e->vel)) * 1.4f;
    if (ov > 2) gain *= ch_sqrt_fast(2.0f / ov);
    /* the cloud's level: up over ATTACK while the key is down, down over RELEASE after (a one-pole each, once a block) */
    if (e->gate) { if (att < 0.002f) e->env = 1; else { e->env += (1.02f - e->env) * (1 - ch_exp2(-4.0f * (float)n / (att * (float)CH_RATE))); if (e->env > 1) e->env = 1; } }
    else e->env *= ch_exp2(-1.4427f * (float)n / (rel * (float)CH_RATE));
    unsigned cap = ch_grain_cap < 2 ? 2 : ch_grain_cap > CH_GRAINS1 ? CH_GRAINS1 : ch_grain_cap;
    /* the grains that start in this block: one every gap (a sixth of it either way, so no pitch of its own) */
    e->next -= (float)n;
    while (e->next < 0) {
        float at = e->next + (float)n;                   /* its first frame in this block */
        e->next += gap * (0.84f + 0.32f * ch_randf(&e->rng));
        if (!e->gate && e->env < 0.002f) continue;
        ch_grain1 *g = 0; unsigned live = 0;
        for (unsigned q = 0; q < CH_GRAINS1; ++q) { if (e->g[q].left > 0) ++live; else if (!g) g = &e->g[q]; }
        if (!g || live >= cap) continue;                 /* all sounding: this one is passed over (none is cut short) */
        float len = size * (0.9f + 0.2f * ch_randf(&e->rng));
        float span = len * rate;                         /* what it reads, frames of the sample */
        g->pos = ch_clamp(centre + ch_rands(&e->rng) * spray, 0, ch_max(F - span - 3, 0));
        g->rate = rate; g->left = (int32_t)len; g->skip = (uint16_t)(at < 0 ? 0 : at >= (float)n ? n - 1 : (unsigned)at);
        float w = CH_TWO_PI / len;                       /* the window: 0.5 - 0.5 cos, the cosine a quadrature pair stepped by w a frame */
        g->wc = 1; g->ws = 0; g->we = w;
        float amp = 0.85f + 0.15f * ch_randf(&e->rng), pan = ch_rands(&e->rng) * spread;
        g->al = amp * (1 - pan * 0.5f); g->ar = amp * (1 + pan * 0.5f); g->blk = -1;
    }
    unsigned sounding = 0;
    const float k16 = 1.0f / 32768;
    for (unsigned q = 0; q < CH_GRAINS1; ++q) {
        ch_grain1 *g = &e->g[q];
        if (g->left <= 0) continue;
        unsigned i0 = g->skip; g->skip = 0;
        unsigned cnt = n - i0; if ((int32_t)cnt > g->left) cnt = (unsigned)g->left;
        float pos = g->pos, r = g->rate, wc = g->wc, ws = g->ws, we = g->we, al = g->al, ar = g->ar;
        const uint32_t last = s->frames - 2;
        for (unsigned i = i0; i < i0 + cnt; ++i) {
            uint32_t ip = (uint32_t)pos;
            if (ip > last) break;                        /* (never: its place was chosen with room to its end) */
            int32_t blk = (int32_t)(ip / CH_ADPCM_BLOCK);
            if (blk != g->blk) grain_block(g, s, blk);
            unsigned k = ip % CH_ADPCM_BLOCK;
            float a = (float)g->pcm[k], b = (float)g->pcm[k + 1];
            float v = (a + (b - a) * (pos - (float)ip)) * (0.5f - 0.5f * wc) * k16;
            wc -= we * ws; ws += we * wc;
            L[i] += v * al; R[i] += v * ar;
            pos += r;
        }
        g->pos = pos; g->wc = wc; g->ws = ws; g->left -= (int32_t)cnt;
        if (g->left > 0) ++sounding;
    }
    /* the cloud's level over the block, in a line from the last block's (no step at RELEASE or a short ATTACK) */
    float g0 = e->env_out, g1 = e->env * gain, dg = (g1 - g0) / (float)n;
    for (unsigned i = 0; i < n; ++i) { g0 += dg; L[i] *= g0; R[i] *= g0; }
    e->env_out = g1;
    if (!e->gate && e->env < 0.0005f) {                 /* faded out: the cloud ends, its grains with it */
        for (unsigned q = 0; q < CH_GRAINS1; ++q) e->g[q].left = 0;
        e->on = 0; e->env = 0; e->env_out = 0;
    }
    (void)sounding;
}
