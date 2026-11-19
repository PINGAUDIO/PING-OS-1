/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hugelton Instruments
 * From Felucca-Engines (https://github.com/hugelton/Felucca-Engines @ d0260074, src/eng_wheel.c), MIT; full text
 * in docs/THIRD_PARTY.md. Adapted for PING FM-1, 2026: one voice at a time (the part's rotor and bar gains are
 * kept per voice), the three macro knobs below, 44.1 kHz constants, names prefixed. */
/* WHEEL: a tonewheel-style additive organ. Felucca's own design.
 *
 * Nine sines per voice at the drawbar footages 16' 5 1/3' 8' 4' 2 2/3' 2' 1 3/5' 1 1/3' 1', each 0..8 (about
 * 3 dB a step), a soft normalisation so full bars do not stack up to clipping; a partial above ~6 kHz folds
 * back an octave (as the top of a tonewheel set). PERC: a decaying 2nd or 3rd harmonic, normal or soft, fast or
 * slow (with PERC on the 1' bar is silent and the bars drop ~3 dB at normal level). CLICK: a burst of
 * high-passed noise at the key contact (half at the release). DRIVE: tanh overdrive. ROTOR: two rotors (low /
 * high, split at ~800 Hz) spin SLOW or FAST with amplitude and Doppler modulation folded into the per-block
 * partial gains and increments.
 *
 * PING FM-1 knobs: REG (16 registrations), PERC (OFF 2SOFT 3SOFT 2ND 3RD 2SLOW 3SLOW; it also raises the key
 * click), ROT/DRV (rotor off, SLOW with a little drive, FAST with more). Felucca's single-trigger percussion
 * (none while another key is held) needs the other voices: here every note strikes it. */
#define CH_FE_INTERNAL 1
#include "ch_fe.h"

#define DRW_NP 9
#define DRW_FOLD 584366000u      /* ~6 kHz as a phase increment */
#define DRW_XOVER 77915000u      /* ~800 Hz */
#define DRW_PDEC_SLOW 65457u     /* percussion decay a block, Q16: tau 0.6 s */
#define DRW_PDEC_FAST 65219u     /* tau 0.15 s */
#define DRW_CLKDEC 24576         /* key click decay a block, Q15: tau ~2.5 ms */

static const uint16_t DRW_LV[9] = {0, 2920, 4125, 5827, 8231, 11627, 16423, 23198, 32767};
static const char *const N_DRW_REG[16] = {"FLUTE", "MELLO", "HOLLW", "SMOOT", "3BAR", "BLUES", "GOSPL", "ROCK",
                                          "TOPS", "CLARI", "REED", "STRNG", "CHAPL", "BRITE", "BASS", "FULL"};
static const char DRW_REG[16][DRW_NP + 1] = {
    "008000000", "008400000", "800800000", "838000000", "888000000", "888800000", "888600006", "888000888",
    "800000888", "008080800", "008765432", "006876540", "004432000", "008888000", "858000000", "888888888",
};
/* PERC knob order -> Felucca's PERC value (OFF 2ND 3RD 2SOFT 3SOFT 2SLOW 3SLOW) */
static const uint8_t DRW_PERC_K[7] = {0, 3, 4, 1, 2, 5, 6};
static const char *const N_DRW_PERC[7] = {"OFF", "2SOFT", "3SOFT", "2ND", "3RD", "2SLOW", "3SLOW"};
static const uint32_t DRW_FOLD_LIM[DRW_NP] = {
    DRW_FOLD * 2u, DRW_FOLD / 3u * 2u, DRW_FOLD, DRW_FOLD / 2u, DRW_FOLD / 3u, DRW_FOLD / 4u, DRW_FOLD / 5u,
    DRW_FOLD / 6u, DRW_FOLD / 8u,
};
static inline uint32_t drw_ratio(uint32_t k, uint32_t x)
{
    switch (k) {
    case 0: return x >> 1;
    case 1: return x + (x >> 1);
    case 2: return x;
    case 3: return x << 1;
    case 4: return x * 3u;
    case 5: return x << 2;
    case 6: return x * 5u;
    case 7: return x * 6u;
    default: return x << 3;
    }
}
/* rotor speeds, phase increments a block: Hz * 32 / 44100 * 2^32 */
static const int32_t DRW_ROT_SPD[2][3] = {
    {0, 2493165, 20880261},      /* high rotor: OFF, SLOW 0.8 Hz, FAST 6.7 Hz */
    {0, 2181519, 18075450},      /* low rotor: 0.7 Hz, 5.8 Hz */
};

typedef struct {
    /* the part's (Felucca drw_trk_t), here the voice's */
    int32_t g[DRW_NP];
    int32_t pk, plev;
    uint32_t pdec;
    int32_t clkl;
    int32_t dg, dcomp;
    uint32_t rph[2];
    int32_t rsp[2];
    int32_t rwet;
    int32_t am[2], fm[2];
    /* the voice's (drw_vc_t) */
    uint32_t ph[DRW_NP];
    int32_t gp[DRW_NP];
    int32_t perc, clk, nz, hp;
    uint8_t gate, init, rsv[2];
} drw_t;
CH_FE_STATIC_ASSERT(wheel_fits, sizeof(drw_t) <= sizeof(((ch_fe_voice *)0)->st));

/* the knobs as Felucca's E0..E7: REG SUB BODY TOP PERC CLICK DRV ROTR */
static void drw_params(const float *knob, int16_t *p)
{
    float r = knob[2];
    p[0] = (int16_t)ch_fe_step(knob[0], 16);
    p[1] = p[2] = p[3] = 0;
    p[4] = (int16_t)DRW_PERC_K[ch_fe_step(knob[1], 7)];
    p[5] = (int16_t)ch_fe_range(0.25f + 0.6f * knob[1], 127);
    if (r < 0.2f) {
        p[6] = 0;
        p[7] = 0;
    } else if (r < 0.6f) {
        p[6] = (int16_t)ch_fe_range((r - 0.2f) * 2.5f * 0.25f, 127);
        p[7] = 1;
    } else {
        p[6] = (int16_t)ch_fe_range(0.25f + (r - 0.6f) * 2.5f * 0.75f, 127);
        p[7] = 2;
    }
}

static int32_t drw_level(const int16_t *p, uint32_t k)
{
    int32_t d = DRW_REG[(uint32_t)p[0] & 15u][k] - '0';
    d += k < 2u ? p[1] : k < 4u ? p[2] : p[3];
    return ch_fe_clamp(d, 0, 8);
}

static void wheel_block(drw_t *T, const int16_t *p)
{
    uint32_t k, perc = (uint32_t)p[4] % 7u, rot = (uint32_t)p[7] % 3u;
    int32_t s = 0, lv[DRW_NP];
    uint32_t norm;
    for (k = 0; k < DRW_NP; k++) {
        lv[k] = perc && k == DRW_NP - 1u ? 0 : DRW_LV[drw_level(p, k)];
        s += lv[k];
    }
    norm = (uint32_t)(2u * 39322u * 32768u) / (uint32_t)(s + 49152);
    if (perc == 1u || perc == 2u || perc == 5u || perc == 6u)
        norm = (norm * 23170u) >> 15;
    for (k = 0; k < DRW_NP; k++)
        T->g[k] = (int32_t)(((uint32_t)lv[k] * norm) >> 15);
    T->pk = perc ? (int32_t)(3u + ((perc - 1u) & 1u)) : DRW_NP;
    T->plev = perc == 3u || perc == 4u ? 12000 : 24000;
    T->pdec = perc >= 5u ? DRW_PDEC_SLOW : DRW_PDEC_FAST;
    T->clkl = p[5] * 180;
    if (p[6]) {
        T->dg = 256 + p[6] * 6;
        T->dcomp = (6 * 256 * 256) / (512 + T->dg);
    } else {
        T->dg = 0;
    }
    for (k = 0; k < 2u; k++) {
        int32_t sp, c, sn, dep;
        T->rsp[k] += (DRW_ROT_SPD[k][rot] - T->rsp[k]) >> (k ? 11 : 9);
        sp = T->rsp[k];
        T->rph[k] += (uint32_t)sp;
        sn = ch_fe_sine_i(T->rph[k]);
        c = ch_fe_sine_i(T->rph[k] + 0x40000000u);
        dep = k ? 7000 : 13000;
        T->am[k] = 32767 - ch_fe_mulq15(dep, (32767 - c) >> 1);
        T->fm[k] = (sn * (sp >> (k ? 17 : 16))) >> 15;
    }
    T->rwet += ((rot ? 32767 : 0) - T->rwet) >> 5;
    for (k = 0; k < 2u; k++) {
        T->am[k] = 32767 - ch_fe_mulq15(T->rwet, 32767 - T->am[k]);
        T->fm[k] = ch_fe_mulq15(T->fm[k], T->rwet);
    }
}

static void wheel_note_on(void *st, const ch_fe_mod *m, const float *knob, uint32_t seed, int fresh)
{
    drw_t *V = (drw_t *)st;
    uint32_t k;
    (void)m;
    if (!V->init) {              /* the wheels: spread start phases, then free running */
        for (k = 0; k < DRW_NP; k++)
            V->ph[k] = (k * 0x9E3779B9u) ^ (seed * 0x7F4A7C15u);
        V->nz = 0x13579BDF + (int32_t)seed;
        V->rph[0] = seed * 0x6A09E667u;
        V->rph[1] = seed * 0xBB67AE85u;
        V->init = 1;
    }
    if (fresh) {                 /* the rotors already at the knob's speed: no spin-up on every note */
        int16_t p[8];
        drw_params(knob, p);
        V->rsp[0] = DRW_ROT_SPD[0][p[7] % 3];
        V->rsp[1] = DRW_ROT_SPD[1][p[7] % 3];
        V->rwet = p[7] ? 32767 : 0;
        for (k = 0; k < DRW_NP; k++)
            V->gp[k] = 0;
    }
    V->perc = 32767;
    V->clk = 32767;
    V->hp = 0;
    V->gate = 1;
}

static void wheel_render(void *st, int32_t *out, const ch_fe_mod *m, const float *knob)
{
    drw_t *V = (drw_t *)st;
    const drw_t *T = V;
    int32_t acc[CH_FE_BLOCK], perc1, clk0, clk1;
    uint32_t i, k, n = CH_FE_BLOCK;
    int16_t p[8];
    drw_params(knob, p);
    wheel_block(V, p);
    for (i = 0; i < n; i++)
        acc[i] = 0;
    perc1 = (int32_t)(((uint32_t)V->perc * T->pdec) >> 16);
    for (k = 0; k < DRW_NP; k++) {
        uint32_t x = m->inc, inc, ph = V->ph[k];
        int32_t g1 = T->g[k], g0 = V->gp[k], r;
        if ((int32_t)k == T->pk)
            g1 += ch_fe_mulq15(T->plev, perc1);
        while (x > DRW_FOLD_LIM[k])
            x >>= 1;
        inc = drw_ratio(k, x);
        r = inc < DRW_XOVER;
        g1 = ch_fe_mulq15(g1, T->am[r]);
        inc += (uint32_t)((int32_t)(inc >> 16) * T->fm[r]);
        V->gp[k] = g1;
        if (!g0 && !g1) {
            V->ph[k] = ph + inc * n;
            continue;
        }
        if (g0 == g1) {
            for (i = 0; i < n; i++) {
                ph += inc;
                acc[i] += (ch_fe_sine[ph >> 22] * g1) >> 15;
            }
        } else {
            int32_t gq = g0 * 32, dg = g1 - g0;
            for (i = 0; i < n; i++) {
                ph += inc;
                gq += dg;
                acc[i] += (ch_fe_sine[ph >> 22] * (gq >> 5)) >> 15;
            }
        }
        V->ph[k] = ph;
    }
    V->perc = perc1;
    if (V->gate && !m->gate) {
        V->gate = 0;
        V->clk = 16384;
    }
    clk0 = ch_fe_mulq15(V->clk, T->clkl);
    if (clk0 > 16) {
        int32_t nst = V->nz, hp = V->hp, dc;
        V->clk = (V->clk * DRW_CLKDEC) >> 15;
        clk1 = ch_fe_mulq15(V->clk, T->clkl);
        dc = clk1 - clk0;
        clk0 *= 32;
        for (i = 0; i < n; i++) {
            int32_t z = (int32_t)(ch_fe_noise32(&nst) >> 17) - 16384, y = z - hp;
            hp += y >> 3;
            clk0 += dc;
            acc[i] += (y * (clk0 >> 5)) >> 15;
        }
        V->nz = nst;
        V->hp = hp;
    }
    if (T->dg) {
        int32_t dg = T->dg, dcomp = T->dcomp;
        for (i = 0; i < n; i++)
            acc[i] = (ch_fe_softclip((acc[i] * dg) >> 9) * dcomp) >> 8;
    }
    for (i = 0; i < n; i++)
        out[i] += ch_fe_shl(ch_fe_voice_amp(acc[i] >> 1, m, i), 1);
}

static const char *wheel_value(unsigned knob, float x)
{
    if (knob == 0u)
        return N_DRW_REG[ch_fe_step(x, 16)];
    if (knob == 1u)
        return N_DRW_PERC[ch_fe_step(x, 7)];
    return x < 0.2f ? "OFF" : x < 0.6f ? "SLOW" : "FAST";
}

const ch_fe_engine ch_fe_eng_wheel = {
    "WHEEL", {"REG", "PERC", "ROT/DRV"}, wheel_note_on, wheel_render, 0, wheel_value,
    0.002f, 0.1f, 1.0f, 0.06f, 1.0f,
};
