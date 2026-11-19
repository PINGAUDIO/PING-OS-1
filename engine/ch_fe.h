/* ch_fe: the Felucca sound engines for PING FM-1's SYNTH device.
 *
 * Five engines ported from Felucca / Felucca-Engines by Leo Kuroshita (Hugelton Instruments), fixed point
 * inside, float at this API's edge. Licences per file (docs/THIRD_PARTY.md):
 *   ch_fe.h (internal part), ch_fe_tables.c, ch_fe_wheel.c, ch_fe_phase.c, ch_fe_trio.c, ch_fe_lofi.c   MIT
 *   ch_fe_fm6core.c, ch_fe_fm6tab.c   Apache-2.0 (msfa, the DX-style core of Dexed, ported by Leo Kuroshita)
 *   ch_fe_fm6.c, ch_fe_fm6pat.c       MPL-2.0 (Leo Kuroshita's FM6 engine and patches, by his grant)
 *   ch_fe.c                            PING FM-1's own glue (envelope, block buffer, knob mapping host side)
 *
 * --------------------------------------------------------------------------------------------- the API ---
 * One ch_fe_voice is one voice's complete state (fixed size, no heap, no pointers into other voices, no
 * mutable static state anywhere: voices may be rendered in any order, also from two cores).
 *
 *   ch_fe_init(v, seed)        once, before first use (seed: anything, e.g. track * 4 + voice; it spreads
 *                              the free-running organ wheels and the noise seeds between voices)
 *   ch_fe_note_on(v, model, hz, vel, knob)
 *                              starts (or retriggers, keeping phases) a note. model: CH_FE_*; hz: the pitch
 *                              (8.2 Hz .. 12.5 kHz used); vel 0..1; knob[3] 0..1. The model is latched
 *                              here; FM6 also latches its patch (knob 0) here.
 *   ch_fe_note_off(v)          releases (the engine's / the host envelope's release; FM6: its operator EGs)
 *   ch_fe_active(v)            nonzero while the voice still has sound to give; 0 once silent after release
 *   ch_fe_render(v, model, hz, knob, out, n)
 *                              ADDS n mono samples (about -1..1; one voice at full velocity peaks around
 *                              0.3..0.5) to out[]. Any n; internally the engines run in 32-sample blocks
 *                              (Felucca's control rate) through a per-voice buffer, so the output is up to 31
 *                              samples behind the call and hz / knob are taken once per 32 samples. If model
 *                              differs from the one latched at note-on, the voice fades out within one block
 *                              and ends (start the new model with a note_on). An inactive voice adds nothing.
 *
 * The three knobs are each engine's most musical controls (ch_fe_label for the screen); ch_fe_value names
 * the value of a stepped knob (registration, wave, patch: NULL when the knob is continuous).
 * Each engine has its own amplitude envelope (fixed per engine, ch_fe.c), FM6 its six operator EGs.
 * Audio rate 44.1 kHz only (CH_RATE). */
#ifndef CH_FE_H
#define CH_FE_H
#include <stdint.h>

enum { CH_FE_WHEEL, CH_FE_PHASE, CH_FE_TRIO, CH_FE_LOFI, CH_FE_FM6, CH_FE_MODELS };

#define CH_FE_BLOCK 32           /* the engines' block (Felucca CTL) */
#define CH_FE_STATE_WORDS 84     /* the largest engine state, 32-bit words (FM6; checked in each engine file) */

typedef struct {
    uint32_t st[CH_FE_STATE_WORDS];   /* the engine's own state (opaque) */
    int32_t buf[CH_FE_BLOCK];         /* the block being handed out */
    float env, vgain;                 /* host envelope 0..1, velocity gain */
    float fade, fade_to;              /* kill fade (model switch) */
    uint32_t seed, age;
    int32_t dc;                       /* output DC blocker (its mean, Q10) */
    uint8_t model, active, gate, stage;   /* stage: 0 off, 1 attack, 2 decay / sustain, 3 release */
    uint8_t pos, vel127, kill, rsv;
} ch_fe_voice;

extern const char *const ch_fe_names[CH_FE_MODELS];
const char *ch_fe_label(unsigned model, unsigned knob);
const char *ch_fe_value(unsigned model, unsigned knob, float x);
void ch_fe_init(ch_fe_voice *v, unsigned seed);
void ch_fe_note_on(ch_fe_voice *v, unsigned model, float hz, float vel, const float knob[3]);
void ch_fe_note_off(ch_fe_voice *v);
int ch_fe_active(const ch_fe_voice *v);
void ch_fe_render(ch_fe_voice *v, unsigned model, float hz, const float knob[3], float *out, unsigned n);

/* ======================================================================================== internal ===
 * Only the ch_fe*.c files define CH_FE_INTERNAL. What follows is the engines' interface and the shared
 * fixed-point helpers of Felucca-Engines' dsp.c:
 *
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hugelton Instruments
 * (from https://github.com/hugelton/Felucca-Engines @ d0260074, src/dsp.c and src/felucca_engine.h, adapted
 *  for PING FM-1, 2026: names prefixed, 44.1 kHz only, one voice at a time; full MIT text in
 *  docs/THIRD_PARTY.md) */
#ifdef CH_FE_INTERNAL
#include "ch_dsp.h"              /* memset / memcpy (the FM-1 is freestanding) */

#if CH_RATE != 44100
#error "ch_fe: the Felucca engines are built for 44.1 kHz here"
#endif

#ifdef __GNUC__
#define CH_FE_NOINLINE __attribute__((noinline))
#else
#define CH_FE_NOINLINE
#endif
#define CH_FE_STATIC_ASSERT(name, cond) typedef char ch_fe_assert_##name[(cond) ? 1 : -1]

/* the tables (ch_fe_tables.c, ch_fe_fm6tab.c, ch_fe_fm6pat.c) */
extern const int16_t ch_fe_sine[1025];
extern const uint32_t ch_fe_pitch_oct[192];
extern const uint16_t ch_fe_svf_g[128];
extern const int16_t ch_fe_tanh_q15[257];
#define CH_FE_FM6_NPATCH 8
#define CH_FE_FM6_LFO_UNIT 18279
#define CH_FE_FM6_PENV_UNIT 572
#define CH_FE_FM6_FREQ_R 24932236u
#define CH_FE_FM6_AMS_C0 295293757
#define CH_FE_FM6_AMS_K 6618
extern const uint32_t ch_fe_fm6_exp2[1025];
extern const uint32_t ch_fe_fm6_lfo_delta[100];
extern const uint16_t ch_fe_fm6_detune[128];
extern const int32_t ch_fe_fm6_fine[100];
extern const uint8_t ch_fe_fm6_patch[CH_FE_FM6_NPATCH][156];

/* the FM6 core (ch_fe_fm6core.c, msfa): one note on an opaque state of CH_FE_FM6_CORE_WORDS words; p = a
 * 155-byte patch; note = MIDI note; vel 0..127; roff: added to the decay / release rates (0 = the patch) */
#define CH_FE_FM6_CORE_WORDS 80
void ch_fe_fm6_core_on(uint32_t *core, const uint8_t *p, int32_t note, int32_t vel, int32_t roff, int fresh);
void ch_fe_fm6_core_key(uint32_t *core, const uint8_t *p, int down);
void ch_fe_fm6_core_roff(uint32_t *core, int32_t roff);
int ch_fe_fm6_core_block(uint32_t *core, const uint8_t *p, int32_t *out, int32_t logfreq, int32_t modlvl);
int ch_fe_fm6_core_done(uint32_t *core, const uint8_t *p);

/* a block's modulation, as Felucca's vmod_t (+ what the host knows of the note) */
typedef struct {
    uint32_t inc;                /* phase increment of the pitch (2^32 = a cycle a sample) */
    int32_t pitch16;             /* 1/16 semitone, MIDI note 0 = 0 (floor of the pitch) */
    int32_t fine;                /* the rest of the pitch: inc = pitch_inc(pitch16) * (1 + fine / 4096) */
    int32_t amp0, amp1;          /* Q15 amplitude ramp over the block */
    int32_t cutoff;              /* added to a cutoff in 0..127 << 8 (0 = none) */
    int32_t shape;               /* 64 << 8 = none */
    int32_t envq15;              /* the host envelope, Q15 */
    float hz;
    int32_t vel127;              /* the note's velocity 0..127 */
    int32_t gate;                /* 1 while the key is held */
} ch_fe_mod;

/* the generic oscillator voice (PHASE, TRIO, LOFI): Felucca voice_t's ph[] and s[] */
typedef struct { uint32_t ph[3]; int32_t s[8]; } ch_fe_osc;

typedef struct {
    const char *name;
    const char *label[3];
    /* note_on: fresh = the voice was silent (else a retrigger: the engine keeps its phases and filters) */
    void (*note_on)(void *st, const ch_fe_mod *m, const float *knob, uint32_t seed, int fresh);
    /* render one block of CH_FE_BLOCK samples, ADDED to out (zeroed by the host), Felucca's scale
     * (one oscillator at full amplitude ~ 24000) */
    void (*render)(void *st, int32_t *out, const ch_fe_mod *m, const float *knob);
    /* optional: the engine's own envelope ends the voice (FM6): 1 = done */
    int (*done)(void *st);
    const char *(*value)(unsigned knob, float x);   /* optional: the stepped value's name */
    float atk, dec, sus, rel;    /* host envelope, seconds / level (ownenv: atk < 0) */
    float gain;                  /* output scale to the API's float (Felucca's 32768 = 1.0 times this) */
} ch_fe_engine;
extern const ch_fe_engine ch_fe_eng_wheel, ch_fe_eng_phase, ch_fe_eng_trio, ch_fe_eng_lofi, ch_fe_eng_fm6;

/* a knob 0..1 as an integer 0..n-1 (stepped) and as 0..max (continuous, rounded) */
static inline int32_t ch_fe_step(float x, int32_t n)
{
    int32_t i = (int32_t)(x * (float)n);
    return i < 0 ? 0 : i >= n ? n - 1 : i;
}
static inline int32_t ch_fe_range(float x, int32_t max)
{
    float y = x * (float)max + 0.5f;
    int32_t i = (int32_t)y;
    return i < 0 ? 0 : i > max ? max : i;
}

/* ------------------------------------------------------------- dsp.c (MIT) --- */
#define CH_FE_VOICE_FS 24000     /* per-voice level: a full-scale oscillator at amp 1.0 adds this */
static inline int32_t ch_fe_mulq15(int32_t a, int32_t b) { return (a * b) >> 15; }
static inline int32_t ch_fe_clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline int32_t ch_fe_shl(int32_t x, uint32_t n) { return (int32_t)((uint32_t)x << n); }

/* phase increment of pitch p in 1/16 semitone, 0..2047 (MIDI 0..127) */
static inline uint32_t ch_fe_pitch_inc(uint32_t p)
{
    uint32_t i = p + 64u, o = (i * 21846u) >> 22;      /* i / 192 */
    return ch_fe_pitch_oct[i - o * 192u] >> (10u - o);
}
/* sine, linearly interpolated between the 1024 table points */
static inline int32_t ch_fe_sine_i(uint32_t ph)
{
    uint32_t i = ph >> 22;
    int32_t a = ch_fe_sine[i], b = ch_fe_sine[(i + 1u) & 1023u];
    return a + (((b - a) * (int32_t)((ph >> 7) & 0x7FFFu)) >> 15);
}
static inline int32_t ch_fe_osc_tri(uint32_t ph)
{
    int32_t s = (int32_t)(ph >> 15);                   /* 0..131071 */
    return s < 65536 ? s - 32768 : 98303 - s;
}
/* tanh(x / 32768) * 32767, linearly interpolated */
static inline int32_t ch_fe_softclip(int32_t x)
{
    int32_t a = x < 0 ? -x : x, i = a >> 8, y;
    if (i >= 256)
        y = ch_fe_tanh_q15[256];
    else
        y = ch_fe_tanh_q15[i] + (((ch_fe_tanh_q15[i + 1] - ch_fe_tanh_q15[i]) * (a & 255)) >> 8);
    return x < 0 ? -y : y;
}
static inline uint32_t ch_fe_noise32(int32_t *st)    /* xorshift32 */
{
    uint32_t s = (uint32_t)*st;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    *st = (int32_t)s;
    return s;
}
/* trapezoidal SVF (A. Simper) coefficients, Q13 */
typedef struct { int32_t a1, a2, a3; } ch_fe_tsvf;
static inline void ch_fe_tsvf_coef_k(ch_fe_tsvf *c, int32_t cut, int32_t k)   /* cut 0..127 << 8, k Q12 */
{
    int32_t i, g, den;
    cut = ch_fe_clamp(cut, 0, 127 << 8);
    i = cut >> 8;
    g = ch_fe_svf_g[i];
    if (i < 127)
        g += ((ch_fe_svf_g[i + 1] - g) * (cut & 255)) >> 8;
    den = 4096 + ((g * (g + k)) >> 12);
    c->a1 = (int32_t)((4096u << 13) / (uint32_t)den);
    c->a2 = (c->a1 * g) >> 12;
    c->a3 = (c->a2 * g) >> 12;
}
/* the amplitude ramp at sample i of the block (CH_FE_BLOCK = 32) */
static inline int32_t ch_fe_amp_at(const ch_fe_mod *m, uint32_t i)
{
    int32_t x = (m->amp1 - m->amp0) * (int32_t)i;
    return m->amp0 + ((x + ((x >> 31) & (CH_FE_BLOCK - 1))) >> 5);
}
static inline int32_t ch_fe_voice_amp(int32_t s, const ch_fe_mod *m, uint32_t i)
{
    return ch_fe_mulq15(ch_fe_mulq15(s, ch_fe_amp_at(m, i)), CH_FE_VOICE_FS);
}
/* linear up to k, then a soft knee */
static inline int32_t ch_fe_soft_knee(int32_t y, int32_t k)
{
    int32_t a = y < 0 ? -y : y;
    if (a <= k)
        return y;
    a = k + (ch_fe_softclip((a - k) * 2) >> 1);
    return y < 0 ? -a : a;
}
/* phase increment of pitch16 plus ct cents, with the fine factor (1/4096) */
static inline uint32_t ch_fe_cents_inc(int32_t pitch16, int32_t ct, int32_t fine)
{
    int32_t d16 = ct * 16 / 100, rem = ct * 16 - d16 * 100;
    uint32_t inc = ch_fe_pitch_inc((uint32_t)ch_fe_clamp(pitch16 + d16, 0, 2047));
    return inc + (uint32_t)((int32_t)(inc >> 12) * (rem * 2367 / 16000 + fine));
}
#endif /* CH_FE_INTERNAL */
#endif
