/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2012 Google Inc.
 * Copyright 2016-2025 Pascal Gauthier.
 * C port for Felucca (integer only, no float), 2026: Leo Kuroshita (@kurogedelic), Hugelton Instruments
 * Adapted for PING FM-1, 2026 (see "Changed for PING FM-1" below).
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/* FM6 core: msfa (the 6-operator FM synthesis core of Dexed, Source/msfa) ported to C. From Felucca
 * (https://github.com/hugelton/Felucca, firmware/src/fm6_core.c at commit 63f88ec1, the parent of f9a98fc).
 * Kept from msfa: its envelope (env.cc, with ACCURATE_ENVELOPE), pitch envelope (pitchenv.cc), LFO (lfo.cc),
 * note set-up (level / rate scaling, velocity, operator pitch) and its renderer (fm_core.cc, fm_op_kernel.cc):
 * one operator at a time over a block, two buses, the 32 algorithm tables, feedback on the operator that has it.
 * Changed for Felucca: no float (msfa's start-up tables are const tables, ch_fe_fm6tab.c), Felucca's 1024-point
 * sine, 32-sample blocks, no portamento / operator switches / controllers / pitch bend / microtuning.
 * Changed for PING FM-1, 2026:
 *   - phases are unsigned (uint32_t) throughout, so a phase plus a modulation wraps modulo 2^32 with no signed
 *     overflow; no left shift of a negative value (a multiply instead);
 *   - the operator loops multiply the Q15 sine by the Q24 gain in two 32-bit halves (no 64-bit multiply in a
 *     loop); the other 64-bit products sit in two small noinline helpers (the target compiler's need);
 *   - an envelope rate offset per note (roff: added to the decay rates of every operator, and to the release
 *     rate when it shortens it: the DECAY macro) and the note's LFO kept with the note (one voice at a time);
 *   - the API below (ch_fe_fm6_core_*) on an opaque state; the output DC blocker lives in ch_fe_fm6.c.
 * The patch is the 155-byte single-voice layout (FP_* below), operator 0 = the sixth operator, as msfa. */
#define CH_FE_INTERNAL 1
#include "ch_fe.h"

#define FM6_N CH_FE_BLOCK
#define FM6_LG_N 5

enum {
    FP_R1 = 0, FP_L1 = 4, FP_BP = 8, FP_LD, FP_RD, FP_LC, FP_RC, FP_RS, FP_AMS, FP_KVS, FP_OL, FP_MODE, FP_FC, FP_FF,
    FP_DET, FP_OP = 21,
    FP_PR1 = 126, FP_PL1 = 130, FP_ALG = 134, FP_FB, FP_OKS, FP_LFS, FP_LFD, FP_LPMD, FP_LAMD, FP_LKS, FP_LFW,
    FP_LPMS, FP_TRNSP, FP_NAME, FP_SIZE = 155
};

/* ------------------------------------------------------------ 64-bit products --- */
static CH_FE_NOINLINE uint32_t fm6_mulu(uint32_t a, uint32_t b, uint32_t sh)
{
    return (uint32_t)(((uint64_t)a * b) >> sh);
}
static CH_FE_NOINLINE int32_t fm6_muls(uint32_t a, int32_t b, uint32_t sh)
{
    return (int32_t)(((int64_t)a * b) >> sh);
}

/* ---------------------------------------------------------------- math --- */
/* 2^(x / 2^24), Q24 (msfa Exp2::lookup): the Q30 mantissa, linear between 1024 points */
static inline uint32_t fm6_mant(int32_t x)
{
    uint32_t f = (uint32_t)x & 0xFFFFFFu, k = f >> 14, a = ch_fe_fm6_exp2[k], d = ch_fe_fm6_exp2[k + 1u] - a;
    uint32_t t = f & 0x3FFFu;                          /* d < 2^21: d * t in two 7-bit halves */
    return (1u << 30) + a + ((d * (t >> 7)) >> 7) + ((d * (t & 127u)) >> 14);
}
static inline int32_t fm6_exp2(int32_t x)
{
    int32_t i = x >> 24;
    uint32_t m = fm6_mant(x);
    return i < -24 ? 0 : (int32_t)(m >> (i > 5 ? 1 : 6 - i));
}
/* phase increment (Q24 cycles per sample) of log2 frequency x (Q24) (msfa Freqlut::lookup) */
static inline int32_t fm6_freq(int32_t x)
{
    int32_t i = x >> 24;
    uint32_t q;
    if (i < 0)
        return 0;
    q = fm6_mulu(fm6_mant(x), CH_FE_FM6_FREQ_R, 25);
    return (int32_t)(q >> (21 - (i > 21 ? 21 : i)));
}
/* sine of a Q24 phase (one cycle = 2^24), Q15 */
static inline int32_t fm6_sin(uint32_t ph)
{
    const int16_t *s = &ch_fe_sine[(ph >> 14) & 1023u];
    return s[0] + (((s[1] - s[0]) * (int32_t)(ph & 0x3FFFu)) >> 14);
}
/* (s * g) >> 15 for |s| <= 32767, 0 <= g < 2^28 + 2^12, in 32-bit halves (within 1 of the 64-bit product) */
static inline int32_t fm6_mulg(int32_t s, int32_t g)
{
    return ((s * (g >> 12)) >> 3) + ((s * (g & 4095)) >> 15);
}

/* ------------------------------------------------------------ envelope --- */
typedef struct {
    int32_t level, target, inc, statc;
    int16_t outlevel;
    uint8_t rscale, ix, rising, down;
    int8_t roff;                 /* PING FM-1: added to the rates of stages 1..3 */
    uint8_t rsv;
} fm6_env_t;

static const uint8_t FM6_LEVELLUT[20] = {0, 5, 9, 13, 17, 20, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 42, 43, 45, 46};
static int32_t fm6_scaleout(int32_t ol) { return ol >= 20 ? 28 + ol : FM6_LEVELLUT[ol < 0 ? 0 : ol]; }

static const int32_t FM6_STATICS[77] = {
    1764000, 1764000, 1411200, 1411200, 1190700, 1014300, 992250, 882000, 705600, 705600, 584325, 507150, 502740,
    441000, 418950, 352800, 308700, 286650, 253575, 220500, 220500, 176400, 145530, 145530, 125685, 110250, 110250,
    88200, 88200, 74970, 61740, 61740, 55125, 48510, 44100, 37485, 31311, 30870, 27562, 27562, 22050, 18522, 17640,
    15435, 14112, 13230, 11025, 9261, 9261, 7717, 6615, 6615, 5512, 5512, 4410, 3969, 3969, 3439, 2866, 2690, 2249,
    1984, 1896, 1808, 1411, 1367, 1234, 1146, 926, 837, 837, 705, 573, 573, 529, 441, 441};

static void fm6_env_advance(fm6_env_t *e, const uint8_t *op, uint32_t ix)
{
    int32_t lv, actual, qrate, rate;
    e->ix = (uint8_t)ix;
    if (ix >= 4u)
        return;
    lv = op[FP_L1 + ix];
    rate = op[FP_R1 + ix];
    if (ix && (ix < 3u || e->roff > 0))      /* PING FM-1: the decays; the release only made shorter */
        rate = ch_fe_clamp(rate + e->roff, 0, 99);
    actual = fm6_scaleout(lv) >> 1;
    actual = actual * 64 + e->outlevel - 4256;
    actual = actual < 16 ? 16 : actual;
    e->target = actual * 65536;
    e->rising = e->target > e->level;
    qrate = ((rate * 41) >> 6) + e->rscale;
    qrate = qrate > 63 ? 63 : qrate;
    if (e->target == e->level || (ix == 0u && lv == 0)) {
        int32_t sr = rate + e->rscale;
        sr = sr > 99 ? 99 : sr;
        e->statc = sr < 77 ? FM6_STATICS[sr] : 20 * (99 - sr);
        if (sr < 77 && ix == 0u && lv == 0)
            e->statc /= 20;
    } else {
        e->statc = 0;
    }
    e->inc = (4 + (qrate & 3)) << (2 + FM6_LG_N + (qrate >> 2));
}

static void fm6_env_init(fm6_env_t *e, const uint8_t *op, int32_t outlevel, int32_t rate_scaling, int32_t roff)
{
    e->outlevel = (int16_t)outlevel;
    e->rscale = (uint8_t)rate_scaling;
    e->roff = (int8_t)roff;
    e->level = 0;
    e->down = 1;
    fm6_env_advance(e, op, 0);
}

static int32_t fm6_env_get(fm6_env_t *e, const uint8_t *op)
{
    if (e->statc) {
        e->statc -= FM6_N;
        if (e->statc <= 0) {
            e->statc = 0;
            fm6_env_advance(e, op, e->ix + 1u);
        }
    }
    if (e->ix < 3u || (e->ix < 4u && !e->down)) {
        if (e->statc) {
            ;
        } else if (e->rising) {
            if (e->level < (1716 << 16))
                e->level = 1716 << 16;
            e->level += (((17 << 24) - e->level) >> 24) * e->inc;
            if (e->level >= e->target) {
                e->level = e->target;
                fm6_env_advance(e, op, e->ix + 1u);
            }
        } else {
            e->level -= e->inc;
            if (e->level <= e->target) {
                e->level = e->target;
                fm6_env_advance(e, op, e->ix + 1u);
            }
        }
    }
    return e->level;
}

static void fm6_env_key(fm6_env_t *e, const uint8_t *op, int down)
{
    if (e->down != (uint8_t)down) {
        e->down = (uint8_t)down;
        fm6_env_advance(e, op, down ? 0u : 3u);
    }
}

/* ------------------------------------------------------ pitch envelope --- */
typedef struct {
    int32_t level, target, inc;
    uint8_t ix, rising, down, rsv;
} fm6_penv_t;

static const uint8_t FM6_PENV_RATE[100] = {
    1, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 16, 16, 17, 18, 18,
    19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 30, 31, 33, 34, 36, 37, 38, 39, 41, 42, 44, 46, 47, 49, 51, 53, 54, 56,
    58, 60, 62, 64, 66, 68, 70, 72, 74, 76, 79, 82, 85, 88, 91, 94, 98, 102, 106, 110, 115, 120, 125, 130, 135, 141,
    147, 153, 159, 165, 171, 178, 185, 193, 202, 211, 232, 243, 254, 255};
static const int8_t FM6_PENV_TAB[100] = {
    -128, -116, -104, -95, -85, -76, -68, -61, -56, -52, -49, -46, -43, -41, -39, -37, -35, -33, -32, -31, -30, -29,
    -28, -27, -26, -25, -24, -23, -22, -21, -20, -19, -18, -17, -16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6,
    -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
    26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 38, 40, 43, 46, 49, 53, 58, 65, 73, 82, 92, 103, 115, 127};

static void fm6_penv_advance(fm6_penv_t *e, const uint8_t *p, uint32_t ix)
{
    e->ix = (uint8_t)ix;
    if (ix < 4u) {
        e->target = FM6_PENV_TAB[p[FP_PL1 + ix] % 100u] * (1 << 19);
        e->rising = e->target > e->level;
        e->inc = FM6_PENV_RATE[p[FP_PR1 + ix] % 100u] * CH_FE_FM6_PENV_UNIT;
    }
}
static void fm6_penv_init(fm6_penv_t *e, const uint8_t *p)
{
    e->level = FM6_PENV_TAB[p[FP_PL1 + 3] % 100u] * (1 << 19);
    e->down = 1;
    fm6_penv_advance(e, p, 0);
}
static int32_t fm6_penv_get(fm6_penv_t *e, const uint8_t *p)
{
    if (e->ix < 3u || (e->ix < 4u && !e->down)) {
        if (e->rising) {
            e->level += e->inc;
            if (e->level >= e->target) {
                e->level = e->target;
                fm6_penv_advance(e, p, e->ix + 1u);
            }
        } else {
            e->level -= e->inc;
            if (e->level <= e->target) {
                e->level = e->target;
                fm6_penv_advance(e, p, e->ix + 1u);
            }
        }
    }
    return e->level;
}
static void fm6_penv_key(fm6_penv_t *e, const uint8_t *p, int down)
{
    if (e->down != (uint8_t)down) {
        e->down = (uint8_t)down;
        fm6_penv_advance(e, p, down ? 0u : 3u);
    }
}

/* ----------------------------------------------------------------- LFO --- */
typedef struct {
    uint32_t phase, delta, dstate, dinc, dinc2;
    uint8_t wave, rnd, sync, rsv;
} fm6_lfo_t;

static void fm6_lfo_reset(fm6_lfo_t *l, const uint8_t *p)
{
    int32_t a = 99 - (p[FP_LFD] % 100u);
    l->delta = ch_fe_fm6_lfo_delta[p[FP_LFS] % 100u];
    if (a == 99) {
        l->dinc = l->dinc2 = ~0u;
    } else {
        a = (16 + (a & 15)) << (1 + (a >> 4));
        l->dinc = (uint32_t)(CH_FE_FM6_LFO_UNIT * a);
        a &= 0xff80;
        a = a < 0x80 ? 0x80 : a;
        l->dinc2 = (uint32_t)(CH_FE_FM6_LFO_UNIT * a);
    }
    l->wave = p[FP_LFW];
    l->sync = p[FP_LKS] != 0;
}

static int32_t fm6_lfo_sample(fm6_lfo_t *l)   /* 0..1, Q24; once a block */
{
    int32_t x;
    l->phase += l->delta;
    switch (l->wave) {
    case 0:
        x = (int32_t)(l->phase >> 7);
        x ^= -(int32_t)(l->phase >> 31);
        return x & ((1 << 24) - 1);
    case 1:
        return (int32_t)((~l->phase ^ (1u << 31)) >> 8);
    case 2:
        return (int32_t)((l->phase ^ (1u << 31)) >> 8);
    case 3:
        return (int32_t)(((~l->phase) >> 7) & (1u << 24));
    case 4:
        return (1 << 23) + fm6_sin(l->phase >> 8) * 256;
    case 5:
        if (l->phase < l->delta)
            l->rnd = (uint8_t)((l->rnd * 179 + 17) & 0xff);
        return ((l->rnd ^ 0x80) + 1) << 16;
    }
    return 1 << 23;
}

static int32_t fm6_lfo_delay(fm6_lfo_t *l)
{
    uint32_t d = l->dstate < (1u << 31) ? l->dinc : l->dinc2, s = l->dstate + d;
    if (s < l->dstate)
        return 1 << 24;
    l->dstate = s;
    return s < (1u << 31) ? 0 : (int32_t)((s >> 7) & ((1u << 24) - 1u));
}

static void fm6_lfo_key(fm6_lfo_t *l)
{
    if (l->sync)
        l->phase = (1u << 31) - 1u;
    l->dstate = 0;
}

/* ------------------------------------------------------- the algorithms --- */
enum { FM6_OB1 = 1, FM6_OB2 = 2, FM6_OADD = 4, FM6_IB1 = 16, FM6_IB2 = 32, FM6_FBIN = 64, FM6_FBOUT = 128 };
static const uint8_t FM6_ALG[32][6] = {
    {0xc1, 0x11, 0x11, 0x14, 0x01, 0x14}, {0x01, 0x11, 0x11, 0x14, 0xc1, 0x14}, {0xc1, 0x11, 0x14, 0x01, 0x11, 0x14},
    {0xc1, 0x11, 0x94, 0x01, 0x11, 0x14}, {0xc1, 0x14, 0x01, 0x14, 0x01, 0x14}, {0xc1, 0x94, 0x01, 0x14, 0x01, 0x14},
    {0xc1, 0x11, 0x05, 0x14, 0x01, 0x14}, {0x01, 0x11, 0xc5, 0x14, 0x01, 0x14}, {0x01, 0x11, 0x05, 0x14, 0xc1, 0x14},
    {0x01, 0x05, 0x14, 0xc1, 0x11, 0x14}, {0xc1, 0x05, 0x14, 0x01, 0x11, 0x14}, {0x01, 0x05, 0x05, 0x14, 0xc1, 0x14},
    {0xc1, 0x05, 0x05, 0x14, 0x01, 0x14}, {0xc1, 0x05, 0x11, 0x14, 0x01, 0x14}, {0x01, 0x05, 0x11, 0x14, 0xc1, 0x14},
    {0xc1, 0x11, 0x02, 0x25, 0x05, 0x14}, {0x01, 0x11, 0x02, 0x25, 0xc5, 0x14}, {0x01, 0x11, 0x11, 0xc5, 0x05, 0x14},
    {0xc1, 0x14, 0x14, 0x01, 0x11, 0x14}, {0x01, 0x05, 0x14, 0xc1, 0x14, 0x14}, {0x01, 0x14, 0x14, 0xc1, 0x14, 0x14},
    {0xc1, 0x14, 0x14, 0x14, 0x01, 0x14}, {0xc1, 0x14, 0x14, 0x01, 0x14, 0x04}, {0xc1, 0x14, 0x14, 0x14, 0x04, 0x04},
    {0xc1, 0x14, 0x14, 0x04, 0x04, 0x04}, {0xc1, 0x05, 0x14, 0x01, 0x14, 0x04}, {0x01, 0x05, 0x14, 0xc1, 0x14, 0x04},
    {0x04, 0xc1, 0x11, 0x14, 0x01, 0x14}, {0xc1, 0x14, 0x01, 0x14, 0x04, 0x04}, {0x04, 0xc1, 0x11, 0x14, 0x04, 0x04},
    {0xc1, 0x14, 0x04, 0x04, 0x04, 0x04}, {0xc4, 0x04, 0x04, 0x04, 0x04, 0x04}};

static uint32_t fm6_carriers(uint32_t a)
{
    uint32_t k, c = 0;
    for (k = 0; k < 6u; k++)
        c |= ((FM6_ALG[a & 31u][k] & 3u) ? 0u : 1u) << k;
    return c;
}

/* ------------------------------------------------------- the operators --- */
typedef struct {
    int32_t level_in;            /* Q24 log2 */
    int32_t gain_out;            /* Q24 */
    int32_t freq;                /* Q24 cycles per sample */
    uint32_t phase;              /* Q24 (wraps modulo 2^32) */
} fm6_op_t;

#define FM6_THRESH 1120

#define FM6_OP_LOOP(PH, STORE)                                                    \
    for (i = 0; i < FM6_N; i++) {                                               \
        g += dg;                                                                \
        out[i] STORE fm6_mulg(fm6_sin(PH), g);                                  \
        phase += (uint32_t)freq;                                                \
    }
static CH_FE_NOINLINE void fm6_op_run(int32_t *out, const int32_t *in, uint32_t phase, int32_t freq, int32_t g1,
                                      int32_t g2, int add)
{
    int32_t dg = (g2 - g1 + (FM6_N >> 1)) >> FM6_LG_N, g = g1;
    uint32_t i;
    if (in && add)
        FM6_OP_LOOP(phase + (uint32_t)in[i], +=)
    else if (in)
        FM6_OP_LOOP(phase + (uint32_t)in[i], =)
    else if (add)
        FM6_OP_LOOP(phase, +=)
    else
        FM6_OP_LOOP(phase, =)
}

static CH_FE_NOINLINE void fm6_op_fb(int32_t *out, uint32_t phase, int32_t freq, int32_t g1, int32_t g2, int32_t *fb,
                                     int32_t shift, int add)
{
    int32_t dg = (g2 - g1 + (FM6_N >> 1)) >> FM6_LG_N, g = g1, y0 = fb[0], y = fb[1];
    uint32_t i;
    for (i = 0; i < FM6_N; i++) {
        int32_t s = (y0 + y) >> (shift + 1);
        g += dg;
        y0 = y;
        y = fm6_mulg(fm6_sin(phase + (uint32_t)s), g);
        out[i] = add ? out[i] + y : y;
        phase += (uint32_t)freq;
    }
    fb[0] = y0;
    fb[1] = y;
}

/* msfa FmCore::render into out (Q24); 0 = nothing sounded (out untouched). The buses are on the stack */
static int fm6_core_render(int32_t *out, fm6_op_t *op, uint32_t alg, int32_t *fb, int32_t fb_shift)
{
    int32_t bus[2][FM6_N];
    const uint8_t *a = FM6_ALG[alg & 31u];
    uint8_t has[3] = {0, 0, 0};
    uint32_t k;
    for (k = 0; k < 6u; k++) {
        uint32_t f = a[k], inbus = (f >> 4) & 3u, outbus = f & 3u;
        int add = (f & FM6_OADD) != 0;
        int32_t *o = outbus ? bus[outbus - 1u] : out;
        int32_t g1 = op[k].gain_out, g2 = fm6_exp2(op[k].level_in - (14 << 24));
        op[k].gain_out = g2;
        if (g1 >= FM6_THRESH || g2 >= FM6_THRESH) {
            if (!has[outbus])
                add = 0;
            if (inbus == 0u || !has[inbus]) {
                if ((f & 0xc0u) == 0xc0u && fb_shift < 16)
                    fm6_op_fb(o, op[k].phase, op[k].freq, g1, g2, fb, fb_shift, add);
                else
                    fm6_op_run(o, 0, op[k].phase, op[k].freq, g1, g2, add);
            } else {
                fm6_op_run(o, bus[inbus - 1u], op[k].phase, op[k].freq, g1, g2, add);
            }
            has[outbus] = 1;
        } else if (!add) {
            has[outbus] = 0;
        }
        op[k].phase += (uint32_t)op[k].freq << FM6_LG_N;
    }
    return has[0];
}

/* ------------------------------------------------------------ the note --- */
typedef struct {
    fm6_env_t env[6];
    fm6_op_t op[6];
    int32_t base[6];
    int32_t fb[2];
    fm6_penv_t penv;
    fm6_lfo_t lfo;
    uint8_t fixed, live, down, rsv;
} fm6_note_t;
CH_FE_STATIC_ASSERT(fm6core_fits, sizeof(fm6_note_t) <= CH_FE_FM6_CORE_WORDS * 4);

static const int32_t FM6_COARSE[32] = {
    -16777216, 0, 16777216, 26591258, 33554432, 38955489, 43368474, 47099600, 50331648, 53182516, 55732705,
    58039632, 60145690, 62083076, 63876816, 65546747, 67108864, 68576247, 69959732, 71268397, 72509921, 73690858,
    74816848, 75892776, 76922906, 77910978, 78860292, 79773775, 80654032, 81503396, 82323963, 83117622};
static const uint8_t FM6_VELDATA[64] = {
    0, 70, 86, 97, 106, 114, 121, 126, 132, 138, 142, 148, 152, 156, 160, 163, 166, 170, 173, 174, 178, 181, 184,
    186, 189, 190, 194, 196, 198, 200, 202, 205, 206, 209, 211, 214, 216, 218, 220, 222, 224, 225, 227, 229, 230,
    232, 233, 235, 237, 238, 240, 241, 242, 243, 244, 246, 246, 248, 249, 250, 251, 252, 253, 254};
static const uint8_t FM6_EXPSCALE[33] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 14, 16, 19, 23, 27, 33, 39, 47, 56, 66,
                                         80, 94, 110, 126, 142, 158, 174, 190, 206, 222, 238, 250};
static const uint8_t FM6_PMS[8] = {0, 10, 20, 33, 55, 92, 153, 255};
static const uint32_t FM6_AMSENS[4] = {0, 4342338, 7171437, 16777216};

static int32_t fm6_scale_vel(int32_t vel, int32_t sens)
{
    int32_t v = FM6_VELDATA[(vel < 0 ? 0 : vel > 127 ? 127 : vel) >> 1] - 239;
    return ((sens * v + 7) >> 3) * 16;
}
static int32_t fm6_scale_rate(int32_t note, int32_t sens)
{
    int32_t x = note / 3 - 7;
    x = x < 0 ? 0 : x > 31 ? 31 : x;
    return (sens * x) >> 3;
}
static int32_t fm6_scale_curve(int32_t group, int32_t depth, int32_t curve)
{
    int32_t s;
    if (curve == 0 || curve == 3)
        s = (group * depth * 329) >> 12;
    else
        s = (FM6_EXPSCALE[group > 32 ? 32 : group] * depth * 329) >> 15;
    return curve < 2 ? -s : s;
}
static int32_t fm6_scale_level(int32_t note, const uint8_t *op)
{
    int32_t off = note - op[FP_BP] - 17;
    return off >= 0 ? fm6_scale_curve((off + 1) / 3, op[FP_RD], op[FP_RC])
                    : fm6_scale_curve(-(off - 1) / 3, op[FP_LD], op[FP_LC]);
}

static int32_t fm6_osc_base(const uint8_t *op, int32_t note)
{
    int32_t det = op[FP_DET];
    if (!op[FP_MODE]) {
        int32_t lf = (int32_t)ch_fe_fm6_detune[note] * (det - 7) + FM6_COARSE[op[FP_FC] & 31u];
        return op[FP_FF] ? lf + ch_fe_fm6_fine[op[FP_FF] % 100u] : lf;
    }
    return ((4458616 * ((op[FP_FC] & 3) * 100 + (int32_t)(op[FP_FF] % 100u))) >> 3) + (det > 7 ? 13457 * (det - 7) : 0);
}

/* ------------------------------------------------------------- the API --- */
void ch_fe_fm6_core_on(uint32_t *core, const uint8_t *p, int32_t note, int32_t vel, int32_t roff, int fresh)
{
    fm6_note_t *n = (fm6_note_t *)core;
    uint32_t k;
    note += p[FP_TRNSP] - 24;
    note = note < 0 ? 0 : note > 127 ? 127 : note;
    if (fresh || !n->live) {
        for (k = 0; k < 6u; k++) {
            n->op[k].phase = 0;
            n->op[k].gain_out = 0;
        }
        n->fb[0] = n->fb[1] = 0;
        n->lfo.phase = 0;
        n->lfo.rnd = 0;
    }
    n->fixed = 0;
    for (k = 0; k < 6u; k++) {
        const uint8_t *op = p + k * FP_OP;
        int32_t ol = fm6_scaleout(op[FP_OL]) + fm6_scale_level(note, op);
        ol = (ol > 127 ? 127 : ol) * 32;
        ol += fm6_scale_vel(vel, op[FP_KVS]);
        fm6_env_init(&n->env[k], op, ol < 0 ? 0 : ol, fm6_scale_rate(note, op[FP_RS]), roff);
        n->base[k] = fm6_osc_base(op, note);
        n->fixed |= (uint8_t)((op[FP_MODE] & 1u) << k);
    }
    fm6_penv_init(&n->penv, p);
    fm6_lfo_reset(&n->lfo, p);
    fm6_lfo_key(&n->lfo);
    n->live = n->down = 1;
}

void ch_fe_fm6_core_key(uint32_t *core, const uint8_t *p, int down)
{
    fm6_note_t *n = (fm6_note_t *)core;
    uint32_t k;
    if (n->down == (uint8_t)down)
        return;
    for (k = 0; k < 6u; k++)
        fm6_env_key(&n->env[k], p + k * FP_OP, down);
    fm6_penv_key(&n->penv, p, down);
    n->down = (uint8_t)down;
}

void ch_fe_fm6_core_roff(uint32_t *core, int32_t roff)
{
    fm6_note_t *n = (fm6_note_t *)core;
    uint32_t k;
    for (k = 0; k < 6u; k++)
        n->env[k].roff = (int8_t)roff;
}

/* one block (msfa note compute): logfreq the note's log2 Hz (Q24, before the patch's transpose), modlvl a
 * level offset of the operators that are not carriers (Q24 log2). Writes out (Q24); 0 = silent (out untouched) */
int ch_fe_fm6_core_block(uint32_t *core, const uint8_t *p, int32_t *out, int32_t logfreq, int32_t modlvl)
{
    fm6_note_t *n = (fm6_note_t *)core;
    uint32_t alg = p[FP_ALG] & 31u, fbv = p[FP_FB] & 7u;
    int32_t lfo_val = fm6_lfo_sample(&n->lfo), lfo_delay = fm6_lfo_delay(&n->lfo);
    uint32_t pmd = (uint32_t)((p[FP_LPMD] % 100u) * 165u >> 6) * (uint32_t)lfo_delay;   /* Q32 */
    int32_t senslfo = FM6_PMS[p[FP_LPMS] & 7u] * (lfo_val - (1 << 23));
    int32_t pmod = fm6_muls(pmd, senslfo, 39), pitch_mod;
    uint32_t amd = (uint32_t)((p[FP_LAMD] % 100u) * 165u >> 6), amod, car = fm6_carriers(alg), k;
    if (!n->live)
        return 0;
    logfreq += (p[FP_TRNSP] - 24) * ((1 << 24) / 12);
    pmod = pmod < 0 ? -pmod : pmod;
    pitch_mod = fm6_penv_get(&n->penv, p) + (senslfo < 0 ? -pmod : pmod);
    amod = fm6_mulu(amd, (uint32_t)lfo_delay, 8);
    amod = fm6_mulu(amod, (uint32_t)((1 << 24) - lfo_val), 24);
    for (k = 0; k < 6u; k++) {
        const uint8_t *op = p + k * FP_OP;
        int32_t level = fm6_env_get(&n->env[k], op);
        uint32_t ams = FM6_AMSENS[op[FP_AMS] & 3u];
        n->op[k].freq = fm6_freq((n->fixed >> k) & 1u ? n->base[k] : logfreq + n->base[k] + pitch_mod);
        if (ams) {
            uint32_t sensamp = fm6_mulu(amod, ams, 24);
            int32_t lg = CH_FE_FM6_AMS_C0 + (int32_t)fm6_mulu(sensamp, CH_FE_FM6_AMS_K, 10);
            uint32_t pt = fm6_mant(lg) >> (30 - (lg >> 24));   /* 2^17.6 .. 2^24.1; the mantissa is Q30 (Felucca's fix, its #304, commit 79ece4f:
                                                                * it was shifted left, 2^14 too far, and overflowed. The factory patches all have AMS 0) */
            level -= (int32_t)fm6_mulu((uint32_t)level, pt << 4, 28);
        }
        if (!((car >> k) & 1u))
            level += modlvl;
        n->op[k].level_in = level < 0 ? 0 : level > (17 << 24) ? 17 << 24 : level;
    }
    return fm6_core_render(out, n->op, alg, n->fb, fbv ? 8 - (int32_t)fbv : 16);
}

/* every carrier silent for good */
int ch_fe_fm6_core_done(uint32_t *core, const uint8_t *p)
{
    fm6_note_t *n = (fm6_note_t *)core;
    uint32_t car = fm6_carriers(p[FP_ALG] & 31u), k;
    if (!n->live)
        return 1;
    for (k = 0; k < 6u; k++) {
        const fm6_env_t *e = &n->env[k];
        if (!((car >> k) & 1u))
            continue;
        if (n->op[k].gain_out >= FM6_THRESH)
            return 0;
        if (e->ix < 3u)
            return 0;
        if (e->ix == 3u && !e->down && e->rising)
            return 0;
        if (e->ix == 3u && e->down) {
            int32_t a = (fm6_scaleout(p[k * FP_OP + FP_L1 + 3]) >> 1) * 64 + e->outlevel - 4256;
            if ((a < 16 ? 16 : a) * 65536 > e->level)
                return 0;
        }
    }
    n->live = 0;
    return 1;
}
