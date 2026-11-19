/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hugelton Instruments
 * From Felucca-Engines (https://github.com/hugelton/Felucca-Engines @ d0260074, src/eng_trio.c), MIT; full text
 * in docs/THIRD_PARTY.md. Adapted for PING FM-1, 2026: the three macro knobs below, intervals in 1/16 semitone
 * (for sync sweeps), 44.1 kHz constants, names prefixed. */
/* TRIO: three oscillators with ring modulation and hard sync into a 12 dB/oct multimode filter, in the style of
 * the sound chips of early 8-bit home computers. Felucca's own design.
 *
 * Oscillators: triangle, saw, pulse, pitched noise and two combined waves (saw AND triangle, pulse AND saw).
 * WAVE picks one of 16 sets: the three waves, their levels and the interactions (RING: osc 1 flips sign with
 * osc 3's top phase bit; SYNC: osc 2 restarts with osc 1). Saw / pulse steps, sync restarts and ring flips get
 * a polyBLEP correction (the voice runs one sample late for it). Filter: trapezoidal SVF, a resonance short of
 * self-oscillation, an input that saturates above half scale, a compressed band-pass state and a slow random
 * drift of the cutoff.
 *
 * PING FM-1 knobs: WAVE (the 16 sets), CUTOFF (30 Hz .. 16 kHz, low-pass, the host envelope opens it a little),
 * INTERVAL (sync sets: osc 2 swept 0..+24 semitones, the sync sweep; ring sets: the ring modulator osc 3
 * 0..+24 semitones; the others: osc 2 at unison, a fifth, an octave, an octave + fifth, two octaves). */
#define CH_FE_INTERNAL 1
#include "ch_fe.h"

CH_FE_STATIC_ASSERT(trio_fits, sizeof(ch_fe_osc) <= sizeof(((ch_fe_voice *)0)->st));

enum { TW_OFF, TW_TRI, TW_SAW, TW_PLS, TW_NOI, TW_ST, TW_PS };
#define TRIO_RING 1u
#define TRIO_SYNC 2u
#define TRIO_G3 11000
#define TRIO_G2 15000
#define TRIO_G1 24000

typedef struct {
    uint8_t w[3];
    uint8_t flags;
    int16_t g[3];
} trio_set_t;

static const char *const N_TRIO_WAVE[16] = {"SAW3", "PLS3", "PPT", "SST", "TRI3", "S&T", "P&S", "P+N",
                                            "NOIS", "SYNC", "SYNCP", "SYNC3", "RING", "RING3", "R+S", "R+SP"};
static const trio_set_t TRIO_SETS[16] = {
    {{TW_SAW, TW_SAW, TW_SAW}, 0, {TRIO_G3, TRIO_G3, TRIO_G3}},
    {{TW_PLS, TW_PLS, TW_PLS}, 0, {TRIO_G3, TRIO_G3, TRIO_G3}},
    {{TW_PLS, TW_PLS, TW_TRI}, 0, {TRIO_G3, TRIO_G3, TRIO_G3 + 3000}},
    {{TW_SAW, TW_SAW, TW_TRI}, 0, {TRIO_G3, TRIO_G3, TRIO_G3 + 3000}},
    {{TW_TRI, TW_TRI, TW_TRI}, 0, {TRIO_G3 + 2000, TRIO_G3 + 2000, TRIO_G3 + 2000}},
    {{TW_ST, TW_ST, TW_TRI}, 0, {TRIO_G3 + 3000, TRIO_G3 + 3000, TRIO_G3}},
    {{TW_PS, TW_PS, TW_TRI}, 0, {TRIO_G3 + 3000, TRIO_G3 + 3000, TRIO_G3}},
    {{TW_PLS, TW_OFF, TW_NOI}, 0, {TRIO_G2, 0, TRIO_G2 - 4000}},
    {{TW_NOI, TW_OFF, TW_OFF}, 0, {TRIO_G1, 0, 0}},
    {{TW_SAW, TW_SAW, TW_OFF}, TRIO_SYNC, {TRIO_G2 - 5000, TRIO_G2 + 5000, 0}},
    {{TW_PLS, TW_PLS, TW_OFF}, TRIO_SYNC, {TRIO_G2 - 5000, TRIO_G2 + 5000, 0}},
    {{TW_SAW, TW_SAW, TW_TRI}, TRIO_SYNC, {TRIO_G3 - 3000, TRIO_G3 + 4000, TRIO_G3}},
    {{TW_TRI, TW_OFF, TW_OFF}, TRIO_RING, {TRIO_G1, 0, 0}},
    {{TW_TRI, TW_OFF, TW_TRI}, TRIO_RING, {TRIO_G2 + 2000, 0, TRIO_G2 - 3000}},
    {{TW_TRI, TW_SAW, TW_OFF}, TRIO_RING | TRIO_SYNC, {TRIO_G2 + 2000, TRIO_G2, 0}},
    {{TW_TRI, TW_PLS, TW_OFF}, TRIO_RING | TRIO_SYNC, {TRIO_G2 + 2000, TRIO_G2, 0}},
};
static const int16_t TRIO_MIX[4][3] = {{0, 0, 1024}, {0, 1400, 0}, {1024, 0, -1024}, {1024, 0, 0}};
static const int8_t TRIO_INTS[5] = {0, 7, 12, 19, 24};

/* the knobs: WAVE, INT2 and INT3 in 1/16 semitone, DTN (cents), MODE, CUT, RES, PW */
typedef struct { int32_t wave, int2, int3, dtn, mode, cut, res, pw; } trio_p;
static void trio_params(const float *knob, trio_p *p)
{
    const trio_set_t *ws;
    p->wave = ch_fe_step(knob[0], 16);
    ws = &TRIO_SETS[p->wave];
    p->int2 = 0;
    p->int3 = -12 * 16;
    p->dtn = 6;
    if (ws->flags & TRIO_SYNC)
        p->int2 = ch_fe_range(knob[2], 24 * 16);
    else if (ws->flags & TRIO_RING)
        p->int3 = ch_fe_range(knob[2], 24 * 16);
    else
        p->int2 = TRIO_INTS[ch_fe_step(knob[2], 5)] * 16;
    p->mode = 0;
    p->cut = ch_fe_range(knob[1], 127);
    p->res = 40;
    p->pw = 64;
}

static inline void trio_blep(int32_t j, int32_t x, int32_t g, int32_t *cr)
{
    int32_t h = ((j >> 1) * g) >> 15, y = 32767 - x;
    cr[0] += (h * ((x * x) >> 15)) >> 15;
    cr[1] -= (h * ((y * y) >> 15)) >> 15;
}
static inline int32_t trio_frac(uint32_t past, uint32_t inc)
{
    uint32_t d = inc >> 15, x;
    if (!d)
        return 0;
    x = past / d;
    return x > 32767u ? 32767 : (int32_t)x;
}

typedef struct {
    uint32_t pw;
    int32_t pdc, psm;
} trio_pw_t;

static inline int32_t trio_tri16(uint32_t ph) { return (int32_t)(((ph & 0x80000000u) ? ~ph : ph) >> 15) & 0xFFFF; }
static inline int32_t trio_st(uint32_t ph) { return ((int32_t)(ph >> 16) & trio_tri16(ph)) - 16384; }
static inline int32_t trio_ps(uint32_t ph, const trio_pw_t *q) { return (ph >= q->pw ? (int32_t)(ph >> 16) : 0) - q->psm; }

static int32_t trio_naive(uint32_t w, uint32_t ph, const trio_pw_t *q)
{
    switch (w) {
    case TW_TRI: return ch_fe_osc_tri(ph);
    case TW_SAW: return (int32_t)(ph >> 16) - 32768;
    case TW_PLS: return (ph < q->pw ? 32767 : -32768) - q->pdc;
    case TW_ST: return trio_st(ph);
    case TW_PS: return trio_ps(ph, q);
    default: return 0;
    }
}

static void trio_pass(uint32_t w, uint32_t *php, uint32_t inc, int32_t g, const trio_pw_t *q, int32_t *nz,
                      int32_t *nst, int32_t *mix, int32_t *cr, int16_t *wx, uint32_t n)
{
    uint32_t ph = *php, o, i, pw = q->pw;
    int32_t pdc = q->pdc, z = *nz;
    switch (w) {
    case TW_TRI:
        for (i = 0; i < n; i++) {
            ph += inc;
            mix[i] += (ch_fe_osc_tri(ph) * g) >> 15;
        }
        break;
    case TW_SAW:
        for (i = 0; i < n; i++) {
            o = ph;
            ph += inc;
            if (ph < o) {
                int32_t x = trio_frac(ph, inc);
                trio_blep(-65536, x, g, cr + i);
                if (wx)
                    wx[i] = (int16_t)x;
            }
            mix[i] += (((int32_t)(ph >> 16) - 32768) * g) >> 15;
        }
        break;
    case TW_PLS:
        for (i = 0; i < n; i++) {
            o = ph;
            ph += inc;
            if (ph < o) {
                int32_t x = trio_frac(ph, inc);
                trio_blep(65535, x, g, cr + i);
                if (wx)
                    wx[i] = (int16_t)x;
            }
            if (ph - pw < o - pw)
                trio_blep(-65535, trio_frac(ph - pw, inc), g, cr + i);
            mix[i] += (((ph < pw ? 32767 : -32768) - pdc) * g) >> 15;
        }
        break;
    case TW_NOI:
        for (i = 0; i < n; i++) {
            o = ph;
            ph += inc;
            if ((o ^ ph) & 0x08000000u)
                z = (int32_t)(ch_fe_noise32(nst) >> 16) - 32768;
            mix[i] += (z * g) >> 15;
        }
        break;
    case TW_ST:
        for (i = 0; i < n; i++) {
            ph += inc;
            mix[i] += (trio_st(ph) * g) >> 15;
        }
        break;
    case TW_PS:
        for (i = 0; i < n; i++) {
            ph += inc;
            mix[i] += (trio_ps(ph, q) * g) >> 15;
        }
        break;
    default:
        ph += inc * n;
        break;
    }
    *php = ph;
    *nz = z;
}

static void trio_ring(uint32_t *php, uint32_t inc, int32_t g, uint32_t p3, uint32_t inc3, int32_t *mix, int32_t *cr,
                      int16_t *wx, uint32_t n)
{
    uint32_t ph = *php, o, o3, i;
    for (i = 0; i < n; i++) {
        int32_t s, sg;
        o = ph;
        ph += inc;
        o3 = p3;
        p3 += inc3;
        if (wx && ph < o)
            wx[i] = (int16_t)trio_frac(ph, inc);
        s = ch_fe_osc_tri(ph);
        sg = (p3 & 0x80000000u) ? -g : g;
        if ((o3 ^ p3) & 0x80000000u)
            trio_blep(2 * s, trio_frac(p3 & 0x7FFFFFFFu, inc3), sg, cr + i);
        mix[i] += (s * sg) >> 15;
    }
    *php = ph;
}

static void trio_sync(uint32_t w, uint32_t *php, uint32_t inc, int32_t g, const trio_pw_t *q, const int16_t *wx,
                      int32_t *mix, int32_t *cr, uint32_t n)
{
    uint32_t ph = *php, o, i, pw = q->pw;
    int32_t pdc = q->pdc, pls = w == TW_PLS;
    for (i = 0; i < n; i++) {
        o = ph;
        ph += inc;
        if (wx[i] >= 0) {
            uint32_t back = (uint32_t)wx[i] * (inc >> 15);
            trio_blep(trio_naive(w, 0, q) - trio_naive(w, ph - back, q), wx[i], g, cr + i);
            ph = o = back;
        }
        if (pls) {
            if (ph < o)
                trio_blep(65535, trio_frac(ph, inc), g, cr + i);
            if (ph - pw < o - pw)
                trio_blep(-65535, trio_frac(ph - pw, inc), g, cr + i);
            mix[i] += (((ph < pw ? 32767 : -32768) - pdc) * g) >> 15;
        } else {
            if (ph < o)
                trio_blep(-65536, trio_frac(ph, inc), g, cr + i);
            mix[i] += (((int32_t)(ph >> 16) - 32768) * g) >> 15;
        }
    }
    *php = ph;
}

static void trio_note_on(void *st, const ch_fe_mod *m, const float *knob, uint32_t seed, int fresh)
{
    ch_fe_osc *v = (ch_fe_osc *)st;
    (void)m;
    (void)knob;
    if (fresh) {                 /* (a retrigger keeps the phases, as Felucca's host does) */
        v->ph[0] = 0;
        v->ph[1] = 0x1C000000u;
        v->ph[2] = 0x38000000u;
    }
    v->s[0] = v->s[1] = 0;
    if (!v->s[2])
        v->s[2] = 0x2468ACE + (int32_t)(seed * 2654435761u);
    if (!v->s[2])
        v->s[2] = 1;
    v->s[3] = 0;
    v->s[4] = 0;
    v->s[5] = 0;
}

static void trio_render(void *st, int32_t *out, const ch_fe_mod *m, const float *knob)
{
    ch_fe_osc *v = (ch_fe_osc *)st;
    trio_p p;
    const trio_set_t *ws;
    uint32_t w1, w2, w3, i, n = CH_FE_BLOCK;
    uint32_t ring, sync;
    int32_t g1, g2, g3, fine, duty, reso, k, ma, mb, mc, a1, a2, a3, cut, cur;
    uint32_t inc1, inc2, inc3, mode;
    trio_pw_t q;
    ch_fe_tsvf flt;
    int32_t mix[CH_FE_BLOCK], cr[CH_FE_BLOCK + 1];
    int16_t wx[CH_FE_BLOCK];
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2], ph3s = ph2;
    int32_t ic1 = v->s[0], ic2 = v->s[1], nst = v->s[2], nz = v->s[3], prev = v->s[4], drift = v->s[5];
    trio_params(knob, &p);
    ws = &TRIO_SETS[p.wave];
    w1 = ws->w[0];
    w2 = ws->w[1];
    w3 = ws->w[2];
    ring = ws->flags & TRIO_RING;
    sync = ws->flags & TRIO_SYNC;
    g1 = ws->g[0];
    g2 = ws->g[1];
    g3 = ws->g[2];
    fine = m->fine;
    inc1 = m->inc;
    inc2 = ch_fe_cents_inc(m->pitch16 + p.int2, p.dtn, fine);
    inc3 = ch_fe_cents_inc(m->pitch16 + p.int3, -p.dtn, fine);
    duty = ch_fe_clamp((p.pw << 8) + m->shape - (64 << 8), 768, 32000);
    mode = (uint32_t)p.mode & 3u;
    reso = p.res;
    k = 8192 - reso * 7168 / 127;
    ma = TRIO_MIX[mode][0];
    mb = TRIO_MIX[mode][1];
    mc = TRIO_MIX[mode][2];
    q.pw = (uint32_t)duty << 17;
    q.pdc = 2 * duty - 32768;
    q.psm = 32768 - ((duty * duty) >> 15);
    if (mode >= 2u)
        mb = -(k >> 2);
    drift += (((int32_t)(ch_fe_noise32(&nst) >> 22) - 512) - drift) >> 3;
    cut = ch_fe_clamp((p.cut << 8) + m->cutoff + ch_fe_mulq15(m->envq15, 20 << 8) + drift, 0, 127 << 8);
    ch_fe_tsvf_coef_k(&flt, cut, k);
    a1 = flt.a1;
    a2 = flt.a2;
    a3 = flt.a3;
    for (i = 0; i < n; i++) {
        mix[i] = cr[i] = 0;
        wx[i] = -1;
    }
    cr[n] = 0;
    trio_pass(w3, &ph2, inc3, g3, &q, &nz, &nst, mix, cr, 0, n);
    if (ring)
        trio_ring(&ph0, inc1, g1, ph3s, inc3, mix, cr, sync ? wx : 0, n);
    else
        trio_pass(w1, &ph0, inc1, g1, &q, &nz, &nst, mix, cr, sync ? wx : 0, n);
    if (sync)
        trio_sync(w2, &ph1, inc2, g2, &q, wx, mix, cr, n);
    else
        trio_pass(w2, &ph1, inc2, g2, &q, &nz, &nst, mix, cr, 0, n);
    cur = prev;
    for (i = 0; i < n; i++) {
        int32_t x = cur + cr[i], y, v1, v2, v3, a;
        cur = mix[i];
        x = ch_fe_soft_knee(x, 16000) >> 1;
        v3 = x - ic2;
        v1 = (a1 * ic1 + a2 * v3) >> 13;
        v2 = ic2 + ((a2 * ic1 + a3 * v3) >> 13);
        ic1 = 2 * v1 - ic1;
        ic2 = ch_fe_clamp(2 * v2 - ic2, -150000, 150000);
        a = ch_fe_clamp(ic1, -49152, 49152);
        ic1 = a + ((ic1 - a) >> 2);
        y = ch_fe_soft_knee((x * ma + v1 * mb + v2 * mc) >> 10, 16000);
        out[i] += ch_fe_shl(ch_fe_voice_amp(ch_fe_shl(y, 1), m, i), 1);
    }
    prev = cur + cr[n];
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = ph2;
    v->s[0] = ic1;
    v->s[1] = ic2;
    v->s[2] = nst;
    v->s[3] = nz;
    v->s[4] = prev;
    v->s[5] = drift;
}

static const char *trio_value(unsigned knob, float x)
{
    if (knob == 0u)
        return N_TRIO_WAVE[ch_fe_step(x, 16)];
    return 0;
}

const ch_fe_engine ch_fe_eng_trio = {
    "TRIO", {"WAVE", "CUTOFF", "INTERVAL"}, trio_note_on, trio_render, 0, trio_value,
    0.002f, 0.5f, 0.6f, 0.2f, 1.0f,
};
