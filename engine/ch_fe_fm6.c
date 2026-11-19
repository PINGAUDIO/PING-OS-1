/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hugelton Instruments
 * After Felucca's FM6 engine (https://github.com/hugelton/Felucca, firmware/src/eng_fm6.c at commit 63f88ec1,
 * the parent of f9a98fc), used under MPL-2.0 by the author's grant (https://github.com/hugelton/felucca-bsp/issues/4); see
 * docs/THIRD_PARTY.md. Adapted for PING FM-1, 2026: one voice, the factory patches as they are, three macros.
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* FM6: classic 6-operator FM. The synthesis is msfa (Dexed's core) ported to integer C (ch_fe_fm6core.c,
 * Apache-2.0); this is the engine around it. The operator envelopes are the voice's amplitude and end it.
 *
 * PING FM-1 knobs: PATCH (Felucca's eight factory patches: TINE EP, BELL, FM BASS, BRASS, PAD, MARIMBA, ORGAN,
 * PLUCK; taken at note-on), BRIGHT (Felucca's MLVL: the output level of every operator that is not a carrier,
 * -36 .. +12 dB, the middle = the patch; Felucca's goes to +36), DECAY (rate steps added to the decay rates of all six operators: left
 * +24, shorter, also the release; the middle = the patch; right -14, about 4.5x longer, the release as the patch's).
 * It takes effect from each operator's next envelope stage. */
#define CH_FE_INTERNAL 1
#include "ch_fe.h"

typedef struct {
    uint32_t core[CH_FE_FM6_CORE_WORDS];
    int32_t dc;                  /* the output's DC blocker */
    uint8_t patch, rsv[3];
} fm6v_t;
CH_FE_STATIC_ASSERT(fm6_fits, sizeof(fm6v_t) <= sizeof(((ch_fe_voice *)0)->st));

static const char *const N_FM6_PATCH[CH_FE_FM6_NPATCH] = {"TINE EP", "BELL", "FM BASS", "BRASS", "PAD", "MARIMBA",
                                                          "ORGAN", "PLUCK"};

/* DECAY: rate steps, +24 (left: shorter) .. -14 (right: longer) */
static int32_t fm6_roff(float x) { return x < 0.5f ? ch_fe_range((0.5f - x) * 2.0f, 24) : -ch_fe_range((x - 0.5f) * 2.0f, 14); }

static void fm6_note_on(void *st, const ch_fe_mod *m, const float *knob, uint32_t seed, int fresh)
{
    fm6v_t *v = (fm6v_t *)st;
    int32_t note = (int32_t)(69.0f + 12.0f * ch_log2(m->hz * (1.0f / 440.0f)) + 0.5f);
    (void)seed;
    v->patch = (uint8_t)ch_fe_step(knob[0], CH_FE_FM6_NPATCH);
    if (fresh)
        v->dc = 0;
    ch_fe_fm6_core_on(v->core, ch_fe_fm6_patch[v->patch], note, m->vel127, fm6_roff(knob[2]), fresh);
}

static void fm6_render(void *st, int32_t *out, const ch_fe_mod *m, const float *knob)
{
    fm6v_t *v = (fm6v_t *)st;
    const uint8_t *p = ch_fe_fm6_patch[v->patch % CH_FE_FM6_NPATCH];
    int32_t bus[CH_FE_BLOCK], lf, lvl, dc = v->dc;
    uint32_t i;
    if (!m->gate)
        ch_fe_fm6_core_key(v->core, p, 0);
    ch_fe_fm6_core_roff(v->core, fm6_roff(knob[2]));
    lf = (int32_t)(ch_log2(m->hz) * 16777216.0f);
    /* BRIGHT, in microsteps (1536 = 36 dB). Down from the middle as Felucca's MLVL, to -36 dB. Up from it a third of that,
     * to +12 dB: at Felucca's +36 the modulators are sixty times what the patch was made for, and from a little over the
     * middle it was no brighter, only a wall of folded side bands (PING FM-1, 2026) */
    lvl = (int32_t)((knob[1] - 0.5f) * (knob[1] < 0.5f ? 3072.0f : 1024.0f));
    if (!ch_fe_fm6_core_block(v->core, p, bus, lf, lvl * 65536))
        return;
    for (i = 0; i < CH_FE_BLOCK; i++) {
        int32_t x = bus[i] - dc;
        dc += x >> 10;           /* a one-pole high-pass, ~7 Hz: FM puts sidebands at 0 Hz */
        out[i] += x >> 10;       /* Q24 -> the engines' scale (a full carrier ~ 16384) */
    }
    v->dc = dc;
}

static int fm6_done(void *st)
{
    fm6v_t *v = (fm6v_t *)st;
    return ch_fe_fm6_core_done(v->core, ch_fe_fm6_patch[v->patch % CH_FE_FM6_NPATCH]);
}

static const char *fm6_value(unsigned knob, float x)
{
    return knob == 0u ? N_FM6_PATCH[ch_fe_step(x, CH_FE_FM6_NPATCH)] : 0;
}

const ch_fe_engine ch_fe_eng_fm6 = {
    "FM6", {"PATCH", "BRIGHT", "DECAY"}, fm6_note_on, fm6_render, fm6_done, fm6_value,
    -1.0f, 0, 0, 0, 1.0f,
};
