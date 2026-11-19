/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hugelton Instruments
 * From Felucca-Engines (https://github.com/hugelton/Felucca-Engines @ d0260074, src/eng_phase.c), MIT; full text
 * in docs/THIRD_PARTY.md. Adapted for PING FM-1, 2026: the three macro knobs below, no mutable statics (the
 * cross-fade buffers are on the stack), names prefixed. */
/* PHASE: phase distortion. Felucca's own design.
 *
 * Waveforms: a -cos table read through a bent phase whose bend is DCW (SAW, SQR, PLS, DSIN, SPLS), or a sine
 * windowed by a saw, a triangle or a trapezoid whose pitch rises with DCW (RSAW, RTRI, RTRP). A second wave can
 * alternate with the first every other cycle. A second line (detuned) can be mixed or ring-modulated. The host
 * envelope moves DCW (ENV). FOLD: a sine wavefolder after the lines (0 = off), its gain falling above A4.
 *
 * PING FM-1 knobs: WAVE (8 waves and 4 alternating pairs), DCW (the bend; past 80 % the folder comes in),
 * DETUNE (a second line from unison to 30 cents mixed, the top quarter ring-modulated). */
#define CH_FE_INTERNAL 1
#include "ch_fe.h"

CH_FE_STATIC_ASSERT(phase_fits, sizeof(ch_fe_osc) <= sizeof(((ch_fe_voice *)0)->st));

static const uint8_t PD_SETS[12][2] = {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}, {6, 0}, {7, 0},
                                       {0, 3}, {1, 6}, {3, 7}, {0, 8}};
static const char *const N_PD_SET[12] = {"SAW", "SQR", "PLS", "DSIN", "SPLS", "RSAW", "RTRI", "RTRP",
                                         "SAW+PLS", "SQR+RSAW", "DSN+RTRI", "SAW+RTRP"};

/* the knobs as Felucca's E0..E7: WAVE WAVE2 DCW ENV DTN LINE SUB FOLD */
static void pd_params(const float *knob, int16_t *p)
{
    const uint8_t *s = PD_SETS[ch_fe_step(knob[0], 12)];
    float d = knob[1], t = knob[2];
    p[0] = s[0];
    p[1] = s[1];
    p[2] = (int16_t)ch_fe_range(d < 0.8f ? d * 1.25f : 1.0f, 127);
    p[3] = 48;
    p[7] = (int16_t)(d > 0.8f ? 1 + ch_fe_range((d - 0.8f) * 5.0f, 89) : 0);
    if (t < 0.05f) {
        p[4] = 0;
        p[5] = 0;
    } else if (t < 0.75f) {
        float u = (t - 0.05f) * (1.0f / 0.7f);
        p[4] = (int16_t)(1 + ch_fe_range(u * u, 29));
        p[5] = 0;
    } else {
        p[4] = (int16_t)(2 + ch_fe_range((t - 0.75f) * 4.0f, 125));
        p[5] = 1;
    }
    p[6] = 0;
}

static inline int32_t pd_cos(uint32_t ph16) { return -ch_fe_sine_i(((ph16 & 0xFFFFu) << 16) + 0x40000000u); }

typedef struct {
    uint32_t w, x, peak, rez;
    uint32_t k0, k1;
} pd_t;

static inline uint32_t pd_slope(uint32_t span, uint32_t len) { return (span << 16) / (len ? len : 1u); }

static void pd_setup(pd_t *b, uint32_t w, uint32_t dcw)
{
    uint32_t x;
    b->w = w;
    b->k0 = b->k1 = b->peak = b->rez = 0;
    switch (w) {
    case 0:
        x = 32768u + ((dcw * 32767u) >> 16);
        b->k0 = pd_slope(32768u, x);
        b->k1 = pd_slope(32767u, 65535u - x);
        break;
    case 1:
        x = 32768u - ((dcw * 31120u) >> 16);
        b->k0 = pd_slope(32768u, x);
        b->k1 = pd_slope(32767u, x);
        break;
    case 2:
        x = 65535u - ((dcw * 63487u) >> 16);
        b->k0 = pd_slope(65535u, x);
        break;
    case 3:
        x = 65535u - ((dcw * 49151u) >> 16);
        b->k0 = pd_slope(65535u, x);
        b->k1 = pd_slope(65535u, 65535u - x);
        break;
    case 4:
        x = 65535u - ((dcw * 32767u) >> 16);
        b->peak = (x * (32768u + ((dcw * 29491u) >> 16))) >> 16;
        if (!b->peak)
            b->peak = 1;
        b->k0 = pd_slope(32768u, b->peak);
        b->k1 = pd_slope(32767u, x - b->peak);
        break;
    default:
        x = 0;
        b->rez = (dcw * 44000u) >> 16;
        break;
    }
    b->x = x;
}

static int32_t pd_wave(const pd_t *b, uint32_t ph)
{
    uint32_t pd, x = b->x;
    switch (b->w) {
    case 0:
        pd = ph < x ? (ph * b->k0) >> 16 : 32768u + (((ph - x) * b->k1) >> 16);
        break;
    case 1:
        if (ph < x)
            pd = (ph * b->k0) >> 16;
        else if (ph < 32768u)
            pd = 32768u;
        else if (ph < 32768u + x)
            pd = 32768u + (((ph - 32768u) * b->k1) >> 16);
        else
            pd = 65535u;
        break;
    case 2:
        pd = ph < x ? (ph * b->k0) >> 16 : 65535u;
        break;
    case 3:
        pd = ph < x ? (ph * b->k0) >> 16 : ((ph - x) * b->k1) >> 16;
        break;
    case 4:
        if (ph >= x)
            pd = 65535u;
        else if (ph < b->peak)
            pd = (ph * b->k0) >> 16;
        else
            pd = 32768u + (((ph - b->peak) * b->k1) >> 16);
        break;
    default: {
        uint32_t rp = (ph + ((ph * b->rez) >> 16) * 7u) & 0xFFFFu, win;
        int32_t core = (pd_cos(rp) * 5) >> 3;
        if (b->w == 5)
            win = 65535u - ph;
        else if (b->w == 6)
            win = ph < 32768u ? ph << 1 : (65535u - ph) << 1;
        else
            win = ph < 16384u ? ph << 2 : ph > 49152u ? (65535u - ph) << 2 : 65535u;
        if (win > 65535u)
            win = 65535u;
        return (core * (int32_t)win) >> 16;
    }
    }
    return pd_cos(pd);
}

#define PD_UNFOLDED (-2147483647 - 1)

static void phase_note_on(void *st, const ch_fe_mod *m, const float *knob, uint32_t seed, int fresh)
{
    ch_fe_osc *v = (ch_fe_osc *)st;
    int16_t p[8];
    (void)m;
    (void)seed;
    pd_params(knob, p);
    v->ph[1] = 0;
    v->ph[2] = 0;
    v->s[0] = 0;
    v->s[1] = 0;
    if (fresh) {                 /* FOLD's DC blocker is kept over a retrigger (Felucca .keep) */
        v->ph[0] = 0;
        v->s[4] = p[7] > 0 ? 0 : PD_UNFOLDED;
        v->s[5] = 0;
    }
    v->s[6] = 163;
}

#define PD_FOLD_G0 163u
#define PD_FOLD_REF 42852281u    /* 440 Hz as a phase increment */
static inline uint32_t pd_fold_gain(uint32_t fold, uint32_t inc)
{
    uint32_t ext = fold * ((2048u - PD_FOLD_G0) << 4) / 127u;
    if (inc > PD_FOLD_REF)
        ext = ext * (PD_FOLD_REF >> 12) / (inc >> 12);
    return PD_FOLD_G0 + (ext >> 4);
}

/* the loop: fg 0 unfolded, fg > 0 the fold gain */
static void phase_loop(const int16_t *p, ch_fe_osc *v, int32_t *out, uint32_t n, const ch_fe_mod *m, uint32_t fg)
{
    uint32_t w1 = (uint32_t)p[0] & 7u, w2 = (uint32_t)p[1], i;
    int32_t depth = (p[2] << 8) + m->cutoff + ch_fe_mulq15(m->envq15, p[3] * 256);
    uint32_t dcw, inc = m->inc, inc2;
    int32_t det = p[4], line2 = det != 0 || p[5], ring = p[5], sub = p[6] * 200;
    int32_t d16 = det * 16 / 100, rem = det * 16 - d16 * 100;
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2];
    int32_t tg0 = v->s[0], tg1 = v->s[1];
    int32_t fx1 = fg ? v->s[4] : 0, fhp = fg ? v->s[5] : 0;
    pd_t b1, b2;
    depth = ch_fe_clamp(depth + (m->shape - (64 << 8)), 0, 127 << 8);
    dcw = (uint32_t)depth * 65535u / (127u << 8);
    dcw = (dcw * 56000u) >> 16;
    inc2 = ch_fe_pitch_inc((uint32_t)ch_fe_clamp(m->pitch16 + d16, 0, 2047));
    inc2 += (uint32_t)((int32_t)(inc2 >> 12) * (rem * 2367 / 16000 + m->fine));
    pd_setup(&b1, w1, dcw);
    pd_setup(&b2, w2 ? w2 - 1u : w1, dcw);
    for (i = 0; i < n; i++) {
        uint32_t old = ph0;
        int32_t s;
        s = pd_wave(tg0 ? &b2 : &b1, ph0 >> 16);
        ph0 += inc;
        if (ph0 < old)
            tg0 ^= 1;
        if (line2) {
            uint32_t old2 = ph1;
            int32_t s2 = pd_wave(tg1 ? &b2 : &b1, ph1 >> 16);
            ph1 += inc2;
            if (ph1 < old2)
                tg1 ^= 1;
            s = ring ? ch_fe_mulq15(s, s2) : (s + s2) >> 1;
        }
        if (fg) {
            int32_t f = ch_fe_sine_i((uint32_t)(s * (int32_t)fg) << 7);
            fhp += f - fx1 - (fhp >> 9);
            fx1 = f;
            s = fhp;
        }
        if (sub) {
            ph2 += inc >> 1;
            s += ch_fe_mulq15(ch_fe_sine_i(ph2), sub);
        }
        if (fg)
            s = ch_fe_clamp(s, -65535, 65535);
        out[i] += ch_fe_voice_amp(s, m, i) * 2;
    }
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = ph2;
    v->s[0] = tg0;
    v->s[1] = tg1;
    if (fg) {
        v->s[4] = fx1;
        v->s[5] = fhp;
        v->s[6] = (int32_t)fg;
    }
}

/* FOLD going on or off: that block is rendered both ways and cross-faded */
static CH_FE_NOINLINE void phase_xfade(const int16_t *p, ch_fe_osc *v, int32_t *out, uint32_t n, const ch_fe_mod *m,
                                       uint32_t on)
{
    int32_t xa[CH_FE_BLOCK], xb[CH_FE_BLOCK];
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2], i;
    int32_t tg0 = v->s[0], tg1 = v->s[1], k;
    for (i = 0; i < n; i++)
        xa[i] = xb[i] = 0;
    phase_loop(p, v, xa, n, m, 0);
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = ph2;
    v->s[0] = tg0;
    v->s[1] = tg1;
    if (on) {
        v->s[4] = v->s[5] = 0;
        phase_loop(p, v, xb, n, m, pd_fold_gain((uint32_t)p[7], m->inc));
    } else {
        phase_loop(p, v, xb, n, m, (uint32_t)v->s[6]);
        v->s[4] = PD_UNFOLDED;
    }
    for (i = 0; i < n; i++) {
        k = (int32_t)(on ? i + 1u : n - 1u - i);
        out[i] += xa[i] + (xb[i] - xa[i]) * k / (int32_t)n;
    }
}

static void phase_render(void *st, int32_t *out, const ch_fe_mod *m, const float *knob)
{
    ch_fe_osc *v = (ch_fe_osc *)st;
    int16_t p[8];
    pd_params(knob, p);
    if (p[7] > 0) {
        if (v->s[4] == PD_UNFOLDED)
            phase_xfade(p, v, out, CH_FE_BLOCK, m, 1);
        else
            phase_loop(p, v, out, CH_FE_BLOCK, m, pd_fold_gain((uint32_t)p[7], m->inc));
    } else if (v->s[4] != PD_UNFOLDED) {
        phase_xfade(p, v, out, CH_FE_BLOCK, m, 0);
    } else {
        phase_loop(p, v, out, CH_FE_BLOCK, m, 0);
    }
}

static const char *phase_value(unsigned knob, float x)
{
    if (knob == 0u)
        return N_PD_SET[ch_fe_step(x, 12)];
    if (knob == 2u)
        return x < 0.05f ? "OFF" : x < 0.75f ? "MIX" : "RING";
    return 0;
}

const ch_fe_engine ch_fe_eng_phase = {
    "PHASE", {"WAVE", "DCW", "DETUNE"}, phase_note_on, phase_render, 0, phase_value,
    0.004f, 0.9f, 0.55f, 0.3f, 1.0f,
};
