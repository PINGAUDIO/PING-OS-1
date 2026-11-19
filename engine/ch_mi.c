/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits, stmlib); C port for PING-1, 2026.
 * See ch_mi.h and docs/THIRD_PARTY.md.
 * Sources (https://github.com/pichenettes/eurorack at f4cfa769; stmlib at its head):
 *   stmlib/dsp/polyblep.h, filter.h (Svf, OnePole::tan, DCBlocker), delay_line.h, dsp.h (InterpolateHermite)
 *   plaits/dsp/oscillator/oscillator.h (the SLOPE shape), plaits/dsp/engine/waveshaping_engine.cc (Tame, Render)
 *   plaits/dsp/physical_modelling/string.cc, string_voice.cc, modal_voice.cc, resonator.cc, plaits/dsp/noise/dust.h
 *   plaits/dsp/engine/grain_engine.cc, oscillator/grainlet_oscillator.h (VOICE: three grainlets on a vowel's formants)
 *   plaits/dsp/engine/chord_engine.cc, chords/chord_bank.cc, oscillator/string_synth_oscillator.h (CHORD)
 * Changed for PING-1: C, a sample at a time; 44.1 kHz; the string's line int16 (the voice's buffer) and its
 * all-pass line 160 long (Plaits: 256), the dispersion left out where it would not fit (the lowest notes);
 * svf_shift worked out instead of read from a table; parameter interpolation left to the host's smoothing;
 * MODAL 16 modes (Plaits 24), those above 0.45 of the rate not run; CHORD's divide-down half only (no wavetables),
 * MORPH the registration directly. */
#include "ch_mi.h"
#include "ch_internal.h"

#define MI_PI 3.14159265358979f

/* ---- stmlib pieces ---- */
static inline float blep_this(float t) { return 0.5f * t * t; }
static inline float blep_next(float t) { t = 1.0f - t; return -0.5f * t * t; }
static inline float iblep_next(float t) { float t1 = 0.5f * t, t2 = t1 * t1, t4 = t2 * t2; return 0.1875f - t1 + 1.5f * t2 - t4; }
static inline float iblep_this(float t) { return iblep_next(1.0f - t); }
static inline float tan_dirty(float f) { return f * (MI_PI + 3.736e-01f * MI_PI * MI_PI * MI_PI * f * f); }
static inline float tan_fast(float f)
{
    const float a = 3.260e-01f * MI_PI * MI_PI * MI_PI, b = 1.823e-01f * MI_PI * MI_PI * MI_PI * MI_PI * MI_PI;
    float f2 = f * f; return f * (MI_PI + f2 * (a + b * f2));
}
static inline void svf_set(mi_svf *s, float g, float q) { s->g = g; s->r = 1.0f / q; s->h = 1.0f / (1.0f + s->r * g + g * g); }
static inline float svf_lp(mi_svf *s, float in)
{
    float hp = (in - s->r * s->s1 - s->g * s->s1 - s->s2) * s->h;
    float bp = s->g * hp + s->s1; s->s1 = s->g * hp + bp;
    float lp = s->g * bp + s->s2; s->s2 = s->g * bp + lp;
    return lp;
}
static inline float hermite(float xm1, float x0, float x1, float x2, float f)
{
    float c = (x1 - xm1) * 0.5f, v = x0 - x1, w = c + v, a = w + v + (x2 - x0) * 0.5f, b_neg = w + a;
    return (((a * f) - b_neg) * f + c) * f + x0;
}
static inline float semis(float s) { return ch_exp2(s * (1.0f / 12)); }
static inline float atan01(float x) { return 0.7853982f * x - x * (x - 1.0f) * (0.2447f + 0.0663f * x); }   /* atan on 0..1, within 0.0015 */

/* ---- FOLD (waveshaping_engine.cc) ---- */
static float tame(float f0, float harmonics, float order)
{
    f0 *= harmonics;
    float max_f = 0.5f / order, a = 1.0f - (f0 - max_f) / (0.5f - max_f);
    a = ch_clamp(a, 0, 1);
    return a * a * a;
}
void mi_fold_reset(mi_fold *s) { s->phase = 0.5f; s->next = 0; s->high = 1; }
void mi_fold_block(mi_fold *s, float f0, float harmonics, float timbre, float morph)
{
    float pw = morph * 0.45f + 0.5f;
    float slope = 3.0f + ch_abs(morph - 0.5f) * 5.0f;
    float shape_amount = ch_abs(harmonics - 0.5f) * 2.0f;
    float shape_att = tame(f0, slope, 16.0f);
    float gain_att = tame(f0, slope * (3.0f + shape_amount * shape_att * 5.0f), 12.0f);
    s->shape = (0.5f + (harmonics - 0.5f) * shape_att) * 3.9999f;
    s->gain = 0.03f + 0.46f * timbre * gain_att;
    f0 = ch_clamp(f0, 0.000001f, 0.25f);
    s->pw = ch_clamp(pw, f0 * 2.0f, 1.0f - 2.0f * f0);
}
float mi_fold_tick(mi_fold *s, float f0, float frac)
{
    (void)frac;
    float f = ch_clamp(f0, 0.000001f, 0.25f), pw = s->pw;
    /* the band-limited slope (Oscillator<OSCILLATOR_SHAPE_SLOPE>) */
    float this_s = s->next, next_s = 0;
    s->phase += f;
    float up = 1.0f / pw, down = 1.0f / (1.0f - pw), d = (up + down) * f;
    if (s->high ^ (s->phase < pw)) {
        float t = (s->phase - pw) / f;
        this_s -= iblep_this(t) * d; next_s -= iblep_next(t) * d;
        s->high = s->phase < pw;
    }
    if (s->phase >= 1.0f) {
        s->phase -= 1.0f;
        float t = s->phase / f;
        this_s += iblep_this(t) * d; next_s += iblep_next(t) * d;
        s->high = 1;
    }
    next_s += s->high ? s->phase * up : 1.0f - (s->phase - pw) * down;
    s->next = next_s;
    float x = 2.0f * this_s - 1.0f;
    /* the waveshaper blend, then the folder */
    float sh = s->shape; int si = (int)sh; float sf = sh - (float)si;
    if (si > 3) { si = 3; sf = 1; }
    const int16_t *t1 = ch_mi_ws + si * 257, *t2 = t1 + 257;
    float wi = 127.0f * ch_clamp(x, -1, 1) + 128.0f; int ii = (int)wi; float wf = wi - (float)ii; ii &= 255;
    float a = ((float)t1[ii] + ((float)t1[ii + 1] - (float)t1[ii]) * wf) * (1.0f / 32768);
    float b = ((float)t2[ii] + ((float)t2[ii + 1] - (float)t2[ii]) * wf) * (1.0f / 32768);
    float idx = ((a + (b - a) * sf) * s->gain + 0.5f) * 512.0f;
    int k = (int)idx; float kf = idx - (float)k;
    if (k < 0) { k = 0; kf = 0; } else if (k > 511) { k = 511; kf = 1; }
    const int16_t *fo = ch_mi_fold + 1;
    return hermite((float)fo[k - 1], (float)fo[k], (float)fo[k + 1], (float)fo[k + 2], kf) * (1.0f / 32768);
}

/* ---- STRING (string.cc, string_voice.cc) ---- */
#define LINE_SCALE 4096.0f                       /* the int16 line holds +-8 */
#define LINE_MASK (CH_KS_LEN - 1u)
static inline void line_write(int16_t *line, uint16_t *wp, float s)
{
    float q = ch_clamp(s * LINE_SCALE, -32767, 32767);
    line[*wp] = (int16_t)(q + (q < 0 ? -0.5f : 0.5f));
    *wp = (uint16_t)((*wp - 1u) & LINE_MASK);
}
static inline float line_at(const int16_t *line, uint32_t i) { return (float)line[i & LINE_MASK] * (1.0f / LINE_SCALE); }
static inline float line_read(const int16_t *line, uint16_t wp, float d)
{
    int di = (int)d; float f = d - (float)di;
    float a = line_at(line, wp + (uint32_t)di), b = line_at(line, wp + (uint32_t)di + 1);
    return a + (b - a) * f;
}
static inline float line_hermite(const int16_t *line, uint16_t wp, float d)
{
    int di = (int)d; float f = d - (float)di; uint32_t t = wp + (uint32_t)di + CH_KS_LEN;
    return hermite(line_at(line, t - 1), line_at(line, t), line_at(line, t + 1), line_at(line, t + 2), f);
}
static inline float stretch_allpass(mi_string *s, float in, unsigned delay, float g)
{
    unsigned r = s->sw + delay; if (r >= MI_STRETCH) r -= MI_STRETCH;
    float read = (float)s->stretch[r] * (1.0f / LINE_SCALE);
    float w = in + g * read, q = ch_clamp(w * LINE_SCALE, -32767, 32767);
    s->stretch[s->sw] = (int16_t)(q + (q < 0 ? -0.5f : 0.5f));
    s->sw = (uint16_t)(s->sw ? s->sw - 1u : MI_STRETCH - 1u);
    return -w * g + read;
}
void mi_string_reset(mi_string *s, int16_t *line, unsigned len)
{
    memset(line, 0, len * sizeof *line);
    memset(s, 0, sizeof *s);
    s->delay = s->delay_to = 100.0f;
    svf_set(&s->damp, 0.1f, 0.5f); svf_set(&s->exc, 0.1f, 0.5f);
}
void mi_string_block(mi_string *s, float f0, float structure, float brightness, float damping, float accent, int sustain)
{
    /* (ours) the decaying states flushed once a block: a denormal is slow on some FPUs, and a ring decays into them */
    s->dc_x = ch_ftz(s->dc_x); s->dc_y = ch_ftz(s->dc_y); s->out0 = ch_ftz(s->out0); s->out1 = ch_ftz(s->out1); s->curved = ch_ftz(s->curved);
    s->damp.s1 = ch_ftz(s->damp.s1); s->damp.s2 = ch_ftz(s->damp.s2); s->exc.s1 = ch_ftz(s->exc.s1); s->exc.s2 = ch_ftz(s->exc.s2);
    f0 = ch_clamp(f0, 0.0001f, 0.24f);
    float density = brightness * brightness;
    brightness += 0.25f * accent * (1.0f - brightness);
    damping += 0.25f * accent * (1.0f - damping);
    s->accent = accent; s->sustain = (uint8_t)(sustain != 0);
    if (s->trig || sustain) {                            /* the exciter: a pluck's noise burst, or a bow's dust */
        float cutoff = ch_min(4.0f * f0 * semis((brightness * (2.0f - brightness) - 0.5f) * 72.0f), 0.499f);
        svf_set(&s->exc, tan_dirty(cutoff), sustain ? 1.0f : 0.5f);
        if (s->trig) { s->noise_left = (uint32_t)(1.0f / f0); s->trig = 0; }
    }
    s->dust_f = 0.00005f + 0.99995f * density * density;
    float nl = structure < 0.24f ? (structure - 0.24f) * 4.166f : structure > 0.26f ? (structure - 0.26f) * 1.35135f : 0.0f;
    s->nonlin = nl;
    float amount = nl < 0 ? -nl : nl;
    /* the string (String::ProcessInternal) */
    float delay = ch_clamp(1.0f / f0, 4.0f, (float)CH_KS_LEN - 4.0f);
    s->src_ratio = delay * f0;
    if (s->src_ratio >= 0.9999f) { s->src_phase = 1.0f; s->src_ratio = 1.0f; }
    float cut = ch_min(12.0f + damping * damping * 60.0f + brightness * 24.0f, 84.0f);
    float df = ch_min(f0 * semis(cut), 0.499f);
    if (damping >= 0.95f) {
        float inf = 20.0f * (damping - 0.95f);
        df += inf * (0.4999f - df); cut += inf * (128.0f - cut);
    }
    svf_set(&s->damp, tan_fast(df), 0.5f);
    /* lut_svf_shift: the loop filter's delay at the pitch (two real poles: atan(f0 / fc) / pi of a period), taken off
     * the line. Plaits reads it at the cutoff it asked for; here at the cutoff it got, the same unless the 0.499
     * ceiling clipped it */
    float comp = 1.0f - atan01(ch_min(f0 / df, 1.0f)) * (1.0f / MI_PI);
    (void)cut;
    s->delay_to = delay * comp;
    s->stretch_point = amount * (2.0f - amount) * 0.225f;
    s->stretch_corr = ch_clamp((160.0f / (float)CH_RATE) * delay, 1.0f, 2.1f);
    float ns = amount > 0.75f ? 4.0f * (amount - 0.75f) : 0.0f;
    s->noise_amt = ns * ns * 0.1f;
    s->noise_filter = 0.06f + 0.94f * brightness * brightness;
    s->bridge = amount * amount * 0.01f;
    s->ap_gain = -0.618f * amount / (0.15f + amount);
}
float mi_string_tick(mi_string *s, int16_t *line, uint16_t *wp, uint32_t *rng)
{
    /* the excitation */
    ch_rng_t r = {*rng};
    float in = 0;
    if (s->sustain) {
        float u = ch_randf(&r);
        if (u < s->dust_f) in = u / s->dust_f * (8.0f - s->dust_f * 6.0f) * s->accent;   /* Dust */
    } else if (s->noise_left) { --s->noise_left; in = 2.0f * ch_randf(&r) - 1.0f; }
    in = svf_lp(&s->exc, in);
    /* the string, run at its own rate below ~43 Hz (the line is a period long at most) */
    s->src_phase += s->src_ratio;
    if (s->src_phase > 1.0f) {
        s->src_phase -= 1.0f;
        s->delay += (s->delay_to - s->delay) * 0.05f;
        float delay = s->delay, x;
        if (s->nonlin > 0) {                             /* dispersion: a stiff string */
            s->disp_noise += ((ch_randf(&r) - 0.5f) - s->disp_noise) * s->noise_filter;
            delay *= 1.0f + s->disp_noise * s->noise_amt;
            float ap = delay * s->stretch_point;
            float main = delay - ap * (0.408f - s->stretch_point * 0.308f) * s->stretch_corr;
            if (ap >= 4.0f && main >= 4.0f && ap < (float)MI_STRETCH - 1) {
                x = line_read(line, *wp, main);
                x = stretch_allpass(s, x, (unsigned)ap, s->ap_gain);
            } else x = line_hermite(line, *wp, delay);
        } else {                                         /* the curved bridge: a sitar's buzz (none at the centre) */
            delay *= 1.0f - s->curved * s->bridge;
            x = line_hermite(line, *wp, delay);
            if (s->nonlin < 0) { float v = ch_abs(x) - 0.025f; s->curved = (ch_abs(v) + v) * (x > 0 ? 1.0f : -1.5f); }
        }
        x = ch_clamp(x + in, -20.0f, 20.0f);
        float y = x - s->dc_x + s->dc_y * (1.0f - 20.0f / (float)CH_RATE);   /* the DC blocker */
        s->dc_x = x; s->dc_y = ch_ftz(y);
        y = svf_lp(&s->damp, y);
        s->damp.s1 = ch_ftz(s->damp.s1); s->damp.s2 = ch_ftz(s->damp.s2);
        line_write(line, wp, y);
        s->out1 = s->out0; s->out0 = y;
    }
    *rng = r.s;
    return s->out1 + (s->out0 - s->out1) * s->src_phase;
}

/* ---- the slope oscillator alone (oscillator.h, SLOPE): ANALOG's triangle ---- */
float mi_slope_tick(float *phase, float *next, float *high, float f, float pw)
{
    f = ch_clamp(f, 0.000001f, 0.25f);
    pw = ch_clamp(pw, f * 2.0f, 1.0f - 2.0f * f);
    /* (ours: robust to a state it did not leave, a note's fresh phase, a wave changed mid-note, as Plaits never meets: the
     * phase brought back into its cycle, and a corner smoothed only when it really fell inside this sample, t in 0..1.
     * Outside it the correction is a polynomial far off its range: a first sample of 1.8 million, and the engine's
     * guard silencing the track) */
    float p0 = *phase; p0 -= (float)(int)p0; if (p0 < 0) p0 += 1.0f;
    float this_s = ch_clamp(*next, -0.5f, 1.5f), next_s = 0, ph = p0 + f;
    int hi = *high > 0.5f;
    float up = 1.0f / pw, down = 1.0f / (1.0f - pw), d = (up + down) * f;
    if (hi ^ (ph < pw)) { float t = (ph - pw) / f; if (t >= 0 && t < 1) { this_s -= iblep_this(t) * d; next_s -= iblep_next(t) * d; } hi = ph < pw; }
    if (ph >= 1.0f) { ph -= 1.0f; float t = ph / f; if (t < 1) { this_s += iblep_this(t) * d; next_s += iblep_next(t) * d; } hi = 1; }
    next_s += hi ? ph * up : 1.0f - (ph - pw) * down;
    *phase = ph; *next = next_s; *high = hi ? 1.0f : 0.0f;
    return 2.0f * this_s - 1.0f;
}

/* ---- MODAL (modal_voice.cc, resonator.cc) ---- */
void mi_modal_reset(mi_modal *s) { memset(s, 0, sizeof *s); svf_set(&s->exc, 0.1f, 1.5f); }
void mi_modal_block(mi_modal *s, float f0, float structure, float brightness, float damping, float accent, int sustain)
{
    for (unsigned k = 0; k < MI_MODES; ++k) { s->s1[k] = ch_ftz(s->s1[k]); s->s2[k] = ch_ftz(s->s2[k]); }   /* (ours: denormals flushed once a block) */
    s->exc.s1 = ch_ftz(s->exc.s1); s->exc.s2 = ch_ftz(s->exc.s2); s->imp = ch_ftz(s->imp);
    f0 = ch_clamp(f0, 0.00001f, 0.24f);
    float density = brightness * brightness;
    brightness += 0.25f * accent * (1.0f - brightness);
    damping += 0.25f * accent * (1.0f - damping);
    float range = sustain ? 36.0f : 60.0f, fx = sustain ? 4.0f * f0 : 2.0f * f0;
    float cutoff = ch_min(fx * semis((brightness * (2.0f - brightness) - 0.5f) * range), 0.499f);
    svf_set(&s->exc, tan_dirty(cutoff), sustain ? 0.7f : 1.5f);
    s->sustain = (uint8_t)(sustain != 0); s->accent = accent;
    s->dust_f = 0.00005f + 0.99995f * density * density;
    if (s->trig && !sustain) s->imp = (0.12f + 0.08f * accent) * (1.0f - damping * 0.5f) * semis(cutoff * cutoff * 24.0f) / cutoff;
    s->trig = 0;
    /* the resonator: the stiffness curve, the third partial kept in tune, Q falling up the partials */
    float sp = ch_clamp(structure, 0, 1) * 64.0f; int si = (int)sp; if (si > 63) si = 63;
    float stiff = ch_mi_stiffness[si] + (ch_mi_stiffness[si + 1] - ch_mi_stiffness[si]) * (sp - (float)si);
    {   float st = 1.0f, k = stiff;                      /* NthHarmonicCompensation(3) */
        for (int i = 0; i < 2; ++i) { st += k; k *= k < 0 ? 0.93f : 0.98f; }
        f0 /= st; }
    float harmonic = f0, stretch = 1.0f, qs = semis(damping * 79.7f), q = 500.0f * qs * qs;
    brightness *= 1.0f - structure * 0.3f;
    brightness *= 1.0f - damping * 0.3f;
    float q_loss = brightness * (2.0f - brightness) * 0.85f + 0.15f;
    unsigned n = 0;
    for (unsigned i = 0; i < MI_MODES; ++i) {
        float mf = harmonic * stretch;
        if (mf >= 0.45f) break;                          /* (Plaits clips these at 0.499, nearly silent: not run here) */
        float g = tan_fast(mf), r = 1.0f / (1.0f + mf * q);
        s->g[i] = g; s->rg[i] = r + g; s->h[i] = 1.0f / (1.0f + r * g + g * g);
        s->a[i] = 0.25f * (0.5f + 0.5f * ch_sin_turns(0.015f * (float)i + 0.25f)) * (1.0f - mf * 2.0f);   /* struck near an end (position 0.015) */
        stretch += stiff; stiff *= stiff < 0 ? 0.93f : 0.98f;
        harmonic += f0; q *= q_loss;
        n = i + 1;
    }
    for (unsigned i = n; i < s->n; ++i) s->s1[i] = s->s2[i] = 0;
    s->n = (uint8_t)n;
    for (unsigned i = 0; i < n; ++i) { s->s1[i] = ch_ftz(s->s1[i]); s->s2[i] = ch_ftz(s->s2[i]); }
}
float mi_modal_tick(mi_modal *s, uint32_t *rng)
{
    float in = 0;
    if (s->sustain) {
        ch_rng_t r = {*rng}; float u = ch_randf(&r); *rng = r.s;
        if (u < s->dust_f) in = u / s->dust_f * (4.0f - s->dust_f * 3.0f) * s->accent;
    } else if (s->imp != 0) { in = s->imp; s->imp = 0; }
    in = svf_lp(&s->exc, in);
    float out = 0;
    for (unsigned i = 0; i < s->n; ++i) {
        float hp = (in - s->rg[i] * s->s1[i] - s->s2[i]) * s->h[i];
        float bp = s->g[i] * hp + s->s1[i];
        s->s1[i] = s->g[i] * hp + bp;
        float lp = s->g[i] * bp + s->s2[i];
        s->s2[i] = s->g[i] * bp + lp;
        out += s->a[i] * bp;
    }
    return out;
}

/* ---- FORMANT (grain_engine.cc, grainlet_oscillator.h) ---- */
void mi_grain_reset(mi_grain *s) { memset(s, 0, sizeof *s); }
/* the vowels' first three formants, Hz (A E I O U, an adult voice) */
static const float VOWELS[5][3] = {{730, 1090, 2440}, {530, 1840, 2480}, {270, 2290, 3010}, {570, 840, 2410}, {300, 870, 2240}};
static const float VGAIN[3] = {1.0f, 0.6f, 0.35f};
void mi_grain_block(mi_grain *s, float f0, float vowel, float shift, float breath)
{
    float vp = ch_clamp(vowel, 0, 1) * 3.999f; int vi = (int)vp; float fr = vp - (float)vi;
    float sc = ch_exp2((ch_clamp(shift, 0, 1) - 0.5f) * 4.0f) / (float)CH_RATE;   /* two octaves either way */
    for (unsigned k = 0; k < MI_FORMANTS; ++k) {
        float hz = VOWELS[vi][k] + (VOWELS[vi + 1][k] - VOWELS[vi][k]) * fr;
        s->f[k] = ch_min(hz * sc, 0.25f);
        s->gain[k] = VGAIN[k] * (s->f[k] < 0.24f ? 1.0f : 0.0f);
    }
    s->breath = ch_clamp(breath, 0, 1);
    s->shape = 0.33f + 0.4f * s->breath * ch_max(1.0f - f0 * 24.0f, 0.0f);   /* the window widens with BREATH, back to plain on high notes (as Plaits) */
    float g = tan_dirty(ch_min(0.3f * f0, 0.45f)); s->hp_g = g / (1.0f + g);
}
static inline float carrier(float phase, float shape)
{
    shape *= 3.0f; int si = (int)shape; float t = 1.0f - (shape - (float)si);
    if (si <= 0) { phase *= 1.0f + t * t * t * 15.0f; if (phase >= 1.0f) phase = 1.0f; phase += 0.75f; }
    else if (si == 1) {
        float bp = 0.001f + 0.499f * t * t * t;
        phase = phase < bp ? phase * (0.5f / bp) : 0.5f + (phase - bp) * 0.5f / (1.0f - bp);
        phase += 0.75f;
    } else { t = 1.0f - t; phase = 0.25f + phase * (0.5f + t * t * t * 14.5f); if (phase >= 0.75f) phase = 0.75f; }
    return (ch_sin_turns(phase) + 1.0f) * 0.25f;
}
static inline float grainlet(float cph, float fph, float shape, float bleed) { return carrier(cph, shape) * (ch_sin_turns(fph) + bleed) / (1.0f + bleed); }
float mi_grain_tick(mi_grain *s, float f0, uint32_t *rng)
{
    f0 = ch_min(f0, 0.125f);
    float sum = 0;
    for (unsigned k = 0; k < MI_FORMANTS; ++k) {
        float f1 = s->f[k];
        float this_s = s->next[k], next_s = 0;
        s->cph[k] += f0;
        if (s->cph[k] >= 1.0f) {
            s->cph[k] -= 1.0f;
            float rt = s->cph[k] / f0;
            float before = grainlet(1.0f, s->fph[k] + (1.0f - rt) * f1, s->shape, 0);
            float after = grainlet(0.0f, 0.0f, s->shape, 0);
            float disc = after - before;
            this_s += disc * blep_this(rt); next_s += disc * blep_next(rt);
            s->fph[k] = rt * f1;
        } else { s->fph[k] += f1; if (s->fph[k] >= 1.0f) s->fph[k] -= 1.0f; }
        next_s += grainlet(s->cph[k], s->fph[k], s->shape, 0);
        s->next[k] = next_s;
        sum += this_s * s->gain[k];
    }
    if (s->breath > 0.001f) {                            /* breath: noise in the carrier's window */
        ch_rng_t r = {*rng};
        float b = s->breath * s->breath;
        sum = sum * (1.0f - 0.5f * b) + ch_rands(&r) * carrier(s->cph[0], s->shape) * b * 1.2f;
        *rng = r.s;
    }
    float v = (sum - s->hp_s) * s->hp_g, lp = v + s->hp_s; s->hp_s = ch_ftz(lp + v);   /* the DC blocker: a one-pole high-pass */
    return sum - lp;
}

/* ---- CHORD (chord_engine.cc, chord_bank.cc, string_synth_oscillator.h) ---- */
static const float CHORDS[11][4] = {
    {0.00f, 0.01f, 11.99f, 12.00f}, {0.00f, 7.00f, 7.01f, 12.00f}, {0.00f, 5.00f, 7.00f, 12.00f}, {0.00f, 3.00f, 7.00f, 12.00f},
    {0.00f, 3.00f, 7.00f, 10.00f}, {0.00f, 3.00f, 10.00f, 14.00f}, {0.00f, 3.00f, 10.00f, 17.00f}, {0.00f, 2.00f, 9.00f, 16.00f},
    {0.00f, 4.00f, 11.00f, 14.00f}, {0.00f, 4.00f, 7.00f, 11.00f}, {0.00f, 4.00f, 7.00f, 12.00f}};
const char *const mi_chord_names[11] = {"OCT", "5TH", "SUS 4", "MINOR", "MIN 7", "MIN 9", "MIN 11", "6/9", "MAJ 9", "MAJ 7", "MAJOR"};
static const float REGS[8][6] = {
    {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, {0.5f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f},
    {0.33f, 0.0f, 0.33f, 0.0f, 0.33f, 0.0f}, {0.33f, 0.0f, 0.0f, 0.33f, 0.0f, 0.33f}, {0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f}, {0.0f, 0.1f, 0.1f, 0.0f, 0.2f, 0.6f}};
void mi_chord_reset(mi_chord *s) { memset(s, 0, sizeof *s); }
void mi_chord_block(mi_chord *s, float f0, float chord, float inversion, float registration)
{
    int ci = (int)(ch_clamp(chord, 0, 1) * 10.0f + 0.5f);
    float base[4];
    for (unsigned i = 0; i < 4; ++i) base[i] = semis(CHORDS[ci][i]);
    /* the registration: which saws and squares (rows of REGS, blended) */
    float reg = ch_clamp(registration, 0, 1) * (8 - 1.001f); int ri = (int)reg; float rf = reg - (float)ri;
    float harm[7];
    for (unsigned i = 0; i < 6; ++i) harm[i] = REGS[ri][i] + (REGS[ri + 1][i] - REGS[ri][i]) * rf;
    harm[6] = 0;
    /* the inversion across five voices (ChordBank::ComputeChordInversion) */
    float ratios[MI_CHORD_VOICES] = {0}, amps[MI_CHORD_VOICES] = {0};
    float inv = ch_clamp(inversion, 0, 0.9999f) * 20.0f; int ii = (int)inv; float iff = inv - (float)ii;
    int rot = ii / 4, rn = ii % 4;
    for (int i = 0; i < 4; ++i) {
        float tr = 0.25f * (float)(1 << ((3 + ii - i) / 4));
        int tv = (i - rot + MI_CHORD_VOICES) % MI_CHORD_VOICES, pv = (tv - 1 + MI_CHORD_VOICES) % MI_CHORD_VOICES;
        if (i == rn) { ratios[tv] = base[i] * tr; ratios[pv] = ratios[tv] * 2.0f; amps[pv] = 0.25f * iff; amps[tv] = 0.25f * (1.0f - iff); }
        else if (i < rn) { ratios[pv] = base[i] * tr; amps[pv] = 0.25f; }
        else { ratios[tv] = base[i] * tr; amps[tv] = 0.25f; }
    }
    for (unsigned v = 0; v < MI_CHORD_VOICES; ++v) {
        float nf = f0 * 0.998f * ratios[v];
        float gain = amps[v] * ch_clamp(4.0f - nf * 32.0f, 0, 1);
        float fr = nf * 8.0f; unsigned shift = 0;
        while (fr > 0.5f && shift < 8) { shift += 2; fr *= 0.5f; }
        if (shift >= 8 || gain <= 0 || nf <= 0) { s->g8[v] = s->g4[v] = s->g2[v] = s->g1[v] = 0; s->f[v] = 0; continue; }
        float r[7];
        for (unsigned i = 0; i < 7; ++i) r[i] = i < shift ? 0.0f : harm[i - shift];
        s->f[v] = fr;
        s->g8[v] = (r[0] + 2.0f * r[1]) * gain;
        s->g4[v] = (r[2] - r[1] + 2.0f * r[3]) * gain;
        s->g2[v] = (r[4] - r[3] + 2.0f * r[5]) * gain;
        s->g1[v] = (r[6] - r[5]) * gain;
    }
}
float mi_chord_tick(mi_chord *s)
{
    float out = 0;
    for (unsigned v = 0; v < MI_CHORD_VOICES; ++v) {
        float f = s->f[v];
        if (f <= 0) continue;
        float this_s = s->next[v], next_s = 0, ph = s->ph[v] + f;
        int seg = s->seg[v], nseg = (int)ph;
        if (nseg != seg) {
            float disc = 0;
            if (nseg == 8) { ph -= 8.0f; nseg -= 8; disc -= s->g8[v]; }
            if ((nseg & 3) == 0) disc -= s->g4[v];
            if ((nseg & 1) == 0) disc -= s->g2[v];
            disc -= s->g1[v];
            if (disc != 0) { float t = (ph - (float)nseg) / f; this_s += blep_this(t) * disc; next_s += blep_next(t) * disc; }
        }
        seg = nseg;
        next_s += (ph - 4.0f) * s->g8[v] * 0.125f;
        next_s += (ph - (float)(seg & 4) - 2.0f) * s->g4[v] * 0.25f;
        next_s += (ph - (float)(seg & 6) - 1.0f) * s->g2[v] * 0.5f;
        next_s += (ph - (float)(seg & 7) - 0.5f) * s->g1[v];
        s->ph[v] = ph; s->next[v] = next_s; s->seg[v] = (int8_t)seg;
        out += 2.0f * this_s;
    }
    return out;
}
