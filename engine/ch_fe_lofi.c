/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hugelton Instruments
 * From Felucca-Engines (https://github.com/hugelton/Felucca-Engines @ d0260074, src/eng_lofi.c), MIT; full text
 * in docs/THIRD_PARTY.md. Adapted for PING FM-1, 2026: the three macro knobs below, the host's own envelope
 * (the CHIP = STEP stepped envelope is not ported), 44.1 kHz constants, names prefixed. */
/* LOFI: a chip voice. Pulse / triangle / saw / noise / wave RAM (16 tables of 32 4-bit samples), quantised
 * amplitude and a held (downsampled) output; SWEEP bends the pitch down after note-on, ARP steps a chord at
 * ~50 Hz, VIB a vibrato.
 *
 * PING FM-1 knobs: WAVE (pulse 12.5 / 25 / 37.5 / 50 %, triangle, saw, noise, nine wave-RAM tables),
 * ARP/SWP (off, a pitch sweep down growing, a vibrato, then the arpeggios OCT, MAJ, MIN),
 * CRUSH (8-bit clean, then 4-bit with the sample hold and the amplitude grid growing, the top 1-bit). */
#define CH_FE_INTERNAL 1
#include "ch_fe.h"

CH_FE_STATIC_ASSERT(lofi_fits, sizeof(ch_fe_osc) <= sizeof(((ch_fe_voice *)0)->st));

enum { CHIP_4BIT, CHIP_4B2, CHIP_8BIT, CHIP_1BIT };
enum { RW_PLS, RW_TRI, RW_SAW, RW_NOIS, RW_WRAM };

static const uint8_t WRAM[16][16] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x89, 0xAC, 0xDE, 0xEF, 0xFF, 0xEE, 0xDC, 0xA9, 0x86, 0x53, 0x21, 0x10, 0x00, 0x11, 0x23, 0x56},   /* SINE */
    {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10},   /* TRI */
    {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},   /* SAW */
    {0x01, 0x12, 0x33, 0x45, 0x56, 0x77, 0x89, 0x9A, 0x56, 0x67, 0x88, 0x9A, 0xAB, 0xCC, 0xDE, 0xEF},   /* SAW2 */
    {0xEE, 0xEF, 0xFF, 0xFF, 0xFF, 0xEE, 0xED, 0xDC, 0x11, 0x10, 0x00, 0x00, 0x00, 0x11, 0x12, 0x23},   /* BAS1 */
    {0xCE, 0xFF, 0xED, 0xBA, 0x9A, 0xBD, 0xEF, 0xFE, 0xC9, 0x64, 0x21, 0x00, 0x00, 0x01, 0x24, 0x69},   /* BAS2 */
    {0x8B, 0xEF, 0xED, 0xBA, 0x9A, 0xAA, 0x98, 0x77, 0x88, 0x87, 0x65, 0x55, 0x65, 0x42, 0x10, 0x14},   /* ORGN */
    {0x8D, 0xFE, 0xDD, 0xDD, 0xCD, 0xDD, 0xDE, 0xFD, 0x82, 0x01, 0x22, 0x22, 0x32, 0x22, 0x21, 0x02},   /* HOLW */
    {0x8F, 0xFC, 0xA7, 0x67, 0x89, 0x88, 0x88, 0x87, 0x88, 0x77, 0x77, 0x76, 0x78, 0x98, 0x53, 0x00},   /* VOXA */
    {0x8F, 0xEE, 0xFD, 0xDB, 0x98, 0x66, 0x66, 0x77, 0x88, 0x89, 0x99, 0x97, 0x64, 0x22, 0x01, 0x10},   /* VOXO */
    {0x7F, 0x96, 0xCF, 0xBB, 0xCC, 0xBB, 0xAA, 0x98, 0x77, 0x65, 0x54, 0x43, 0x34, 0x40, 0x39, 0x60},   /* VOXE */
    {0xF3, 0x00, 0x12, 0x22, 0x11, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22},   /* BUZZ */
    {0xFF, 0xFF, 0xFF, 0xFF, 0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0x00, 0x00, 0x00, 0x00},   /* STAIR */
};
#define Q4 2184
#define LOFI_SWEEP_Q8 256
#define LOFI_VIB_INC 0x01000000u

/* the WAVE knob: {wave, duty} (WRAM: duty / 8 = the table) */
static const uint8_t LOFI_WAVES[16][2] = {
    {RW_PLS, 0}, {RW_PLS, 32}, {RW_PLS, 64}, {RW_PLS, 96}, {RW_TRI, 0}, {RW_SAW, 0}, {RW_NOIS, 0},
    {RW_WRAM, 3 * 8}, {RW_WRAM, 6 * 8}, {RW_WRAM, 7 * 8}, {RW_WRAM, 8 * 8}, {RW_WRAM, 9 * 8}, {RW_WRAM, 10 * 8},
    {RW_WRAM, 11 * 8}, {RW_WRAM, 12 * 8}, {RW_WRAM, 14 * 8},
};
static const char *const N_LOFI_WAVE[16] = {"PLS12", "PLS25", "PLS37", "PLS50", "TRI", "SAW", "NOISE",
                                            "SINE", "SAW2", "BASS1", "BASS2", "ORGAN", "HOLLOW", "VOX-A",
                                            "VOX-O", "BUZZ"};

/* the knobs as Felucca's E0..E7: CHIP WAVE DUTY CRSH SWP VIB ARP TONE */
static void lofi_params(const float *knob, int16_t *p)
{
    const uint8_t *w = LOFI_WAVES[ch_fe_step(knob[0], 16)];
    float a = knob[1], c = knob[2];
    p[1] = w[0];
    p[2] = w[1];
    if (c < 0.1f) {
        p[0] = CHIP_8BIT;
        p[3] = 0;
    } else if (c < 0.85f) {
        p[0] = CHIP_4BIT;
        p[3] = (int16_t)ch_fe_range((c - 0.1f) * (1.0f / 0.75f), 127);
    } else {
        p[0] = CHIP_1BIT;
        p[3] = (int16_t)ch_fe_range((c - 0.85f) * (1.0f / 0.15f) * 0.5f, 127);
    }
    p[4] = p[5] = p[6] = 0;
    if (a < 0.1f)
        ;
    else if (a < 0.4f)
        p[4] = (int16_t)(8 + ch_fe_range((a - 0.1f) * (1.0f / 0.3f), 119));
    else if (a < 0.55f)
        p[5] = (int16_t)(10 + ch_fe_range((a - 0.4f) * (1.0f / 0.15f), 60));
    else
        p[6] = (int16_t)(a < 0.7f ? 1 : a < 0.85f ? 2 : 3);
    p[7] = 120;
}

static void lofi_note_on(void *st, const ch_fe_mod *m, const float *knob, uint32_t seed, int fresh)
{
    ch_fe_osc *v = (ch_fe_osc *)st;
    (void)m;
    (void)knob;
    (void)seed;
    if (fresh) {                 /* (the held sample and the low-pass are kept over a retrigger) */
        v->ph[0] = 0;
        v->s[0] = 0;
        v->s[4] = 0;
    }
    v->ph[1] = 0;
    v->s[1] = 0;
    v->s[2] = 0x7FFF;
    v->s[3] = 0;
    v->s[5] = 0;
    v->s[6] = 0;
    v->s[7] = 0;
}

static void lofi_render(void *st, int32_t *out, const ch_fe_mod *m, const float *knob)
{
    ch_fe_osc *v = (ch_fe_osc *)st;
    static const int8_t ARPS[4][3] = {{0, 0, 0}, {0, 12, 0}, {0, 4, 7}, {0, 3, 7}};
    int16_t p[8];
    uint32_t chip, wave, i, duty, inc, ph0, lfsr, n = CH_FE_BLOCK;
    int32_t crush, bits, hold, lpk, ar, pitch, held, cnt, lp, wdc = 0;
    const uint8_t *wr;
    lofi_params(knob, p);
    chip = (uint32_t)p[0];
    wave = (uint32_t)p[1];
    duty = 0x20000000u * (1u + (uint32_t)p[2] / 32u);
    crush = p[3];
    bits = chip == CHIP_8BIT ? 8 : chip == CHIP_1BIT ? 1 : 4;
    hold = 1 + crush / 8 + (chip == 1u ? 1 : 0);
    lpk = 3000 + (p[7] << 8) + (m->cutoff > 0 ? m->cutoff : 0);
    ar = p[6];
    held = v->s[0];
    cnt = v->s[1];
    lp = v->s[4];
    ph0 = v->ph[0];
    lfsr = (uint32_t)v->s[2];
    wr = WRAM[((uint32_t)p[2] >> 3) & 15u];
    if (wave == RW_WRAM) {
        int32_t sum = 0;
        for (i = 0; i < 16u; i++)
            sum += bits == 1 ? ((wr[i] >> 7) + ((wr[i] >> 3) & 1)) * 2 - 2 : (wr[i] >> 4) + (wr[i] & 15) - 15;
        wdc = bits == 1 ? sum * 32767 / 32 : sum * 2 * Q4 / 32;
    }
    if (v->s[3] < (4096 << 8))
        v->s[3] += p[4] / 8 * LOFI_SWEEP_Q8;
    v->s[5]++;
    pitch = m->pitch16 - (v->s[3] >> 8) + ARPS[ar & 3][(v->s[5] / 28) % 3] * 16;
    inc = ch_fe_pitch_inc((uint32_t)ch_fe_clamp(pitch, 0, 2047));
    if (p[5])
        inc += (uint32_t)(((int32_t)(inc >> 12) * (((ch_fe_sine_i(v->ph[1]) >> 8) * p[5]) >> 4)) >> 4);
    v->ph[1] += LOFI_VIB_INC;
    if (lpk > 32767)
        lpk = 32767;
    for (i = 0; i < n; i++) {
        int32_t s;
        if (--cnt <= 0) {
            uint32_t q = bits < 16;
            cnt = hold;
            switch (wave) {
            case RW_TRI:
                s = ch_fe_osc_tri(ph0);
                break;
            case RW_SAW:
                s = (int32_t)(ph0 >> 16) - 32768;
                break;
            case RW_NOIS: {
                uint32_t l = lfsr;
                if ((ph0 + inc * (uint32_t)hold) < ph0 || chip == 3u)
                    l = (l >> 1) | (((l ^ (l >> 1)) & 1u) << 14);
                lfsr = l;
                s = (l & 1u) ? 32767 : -32768;
                break;
            }
            case RW_WRAM: {
                uint32_t k = ph0 >> 27, b = wr[k >> 1];
                b = (k & 1u) ? b & 15u : b >> 4;
                s = (bits == 1 ? (b >= 8u ? 32767 : -32767) : ((int32_t)b * 2 - 15) * Q4) - wdc;
                q = 0;
                break;
            }
            default:
                s = (ph0 < duty ? 32767 : -32768) - ((int32_t)(duty >> 16) - 32768);
                break;
            }
            if (q)
                s = ch_fe_shl((s + (1 << (15 - bits))) >> (16 - bits), (uint32_t)(16 - bits));
            if (crush > 64)
                s = ch_fe_shl(s >> 12, 12);
            held = s;
        }
        ph0 += inc;
        lp += ch_fe_mulq15(held - lp, lpk);
        out[i] += ch_fe_voice_amp(lp, m, i);
    }
    v->ph[0] = ph0;
    v->s[0] = held;
    v->s[1] = cnt;
    v->s[2] = (int32_t)lfsr;
    v->s[4] = lp;
}

static const char *lofi_value(unsigned knob, float x)
{
    if (knob == 0u)
        return N_LOFI_WAVE[ch_fe_step(x, 16)];
    if (knob == 1u)
        return x < 0.1f ? "OFF" : x < 0.4f ? "SWEEP" : x < 0.55f ? "VIB" : x < 0.7f ? "ARP OCT" : x < 0.85f ? "ARP MAJ"
                                                                                                          : "ARP MIN";
    return x < 0.1f ? "8BIT" : x < 0.85f ? "4BIT" : "1BIT";
}

const ch_fe_engine ch_fe_eng_lofi = {
    "LOFI", {"WAVE", "ARP/SWP", "CRUSH"}, lofi_note_on, lofi_render, 0, lofi_value,
    0.001f, 0.35f, 0.75f, 0.08f, 1.0f,
};
