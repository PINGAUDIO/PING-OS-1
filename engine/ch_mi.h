/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits, stmlib); C port for PING-1, 2026.
 * PING-1's ports of Mutable Instruments DSP (https://github.com/pichenettes/eurorack, plaits/ and stmlib/ at
 * f4cfa769), MIT: full text and the list of what came from where in docs/THIRD_PARTY.md.
 *
 * Plaits renders in blocks at 48 kHz in C++; these are C, a sample at a time, at 44.1 kHz: a model's knobs are
 * worked out once a block (the *_block functions) and the samples only run the loop (the *_tick functions).
 * Their state lives in the synth voice, in the room the Felucca engines use when the voice plays one of those
 * (the two never sound on one voice at once), so it must stay within that: see ch_internal.h. */
#pragma once
#include <stdint.h>

/* stmlib's state-variable filter (Svf): g the warped frequency, r the damping (1 / Q), h their normaliser */
typedef struct { float g, r, h, s1, s2; } mi_svf;

/* FOLD: Plaits' waveshaping engine. A band-limited slope (MORPH its asymmetry) through one of five waveshapers
 * (HARMONICS, centre plain) into a wavefolder (TIMBRE), each held back as the note's harmonics near Nyquist */
typedef struct {
    float phase, next; uint8_t high;     /* the slope oscillator */
    float shape, gain, pw;               /* this block: the waveshaper blend (0..4), the folder's gain, the slope's width */
    float shape0, gain0, pw0;            /* the last block's, to glide from */
} mi_fold;
void mi_fold_reset(mi_fold *s);
void mi_fold_block(mi_fold *s, float f0, float harmonics, float timbre, float morph);
float mi_fold_tick(mi_fold *s, float f0, float frac);   /* frac: how far into the block, 0..1 */

/* STRING: Plaits' string voice (Rings' string, lightened). A delay line one period long with a low-pass in the loop
 * (its delay made up, so the pitch holds however dark), a DC blocker, and either a curved bridge (a sitar's buzz)
 * or a dispersion all-pass (a stiff, piano-like string). Plucked by a burst of filtered noise a period long, or
 * bowed by dust (sustain). The line is the voice's int16 buffer (ks); the all-pass line is here. */
enum { MI_STRETCH = 160 };
typedef struct {
    int16_t stretch[MI_STRETCH];         /* the dispersion all-pass's line */
    uint16_t sw;                         /* its write place (the main line's is the voice's ks_w) */
    uint8_t sustain, trig;
    uint32_t noise_left;                 /* the pluck: samples of noise still to feed in */
    float delay, delay_to;               /* the loop's length, gliding to this block's */
    float src_phase, src_ratio, out0, out1;
    float dc_x, dc_y, disp_noise, curved;
    mi_svf damp, exc;                    /* the loop's low-pass; the excitation's */
    float stretch_point, stretch_corr, noise_amt, noise_filter, bridge, ap_gain, nonlin, accent, dust_f;
} mi_string;
void mi_string_reset(mi_string *s, int16_t *line, unsigned len);
void mi_string_block(mi_string *s, float f0, float structure, float brightness, float damping, float accent, int sustain);
float mi_string_tick(mi_string *s, int16_t *line, uint16_t *wp, uint32_t *rng);

/* the band-limited slope (Plaits' Oscillator, SLOPE shape: pw 0.5 a triangle), its state three floats a voice can lend */
float mi_slope_tick(float *phase, float *next, float *high, float f, float pw);

/* MODAL: Plaits' modal voice. A bank of band-pass modes (16 here; Plaits 24) at the partials of a struck or bowed
 * object, STRUCTURE (Plaits' stiffness curve) stretching them from harmonic to bell-like, BRIGHT and DAMP their losses;
 * struck by a filtered impulse, or bowed by dust (sustain) */
enum { MI_MODES = 16 };
typedef struct {
    float s1[MI_MODES], s2[MI_MODES];                     /* the modes' states */
    float g[MI_MODES], rg[MI_MODES], h[MI_MODES], a[MI_MODES];   /* this block's coefficients and gains */
    mi_svf exc;
    uint8_t n, trig, sustain;
    float accent, dust_f, imp;
} mi_modal;
void mi_modal_reset(mi_modal *s);
void mi_modal_block(mi_modal *s, float f0, float structure, float brightness, float damping, float accent, int sustain);
float mi_modal_tick(mi_modal *s, uint32_t *rng);

/* VOICE (FORMANT and VOWEL merged): Plaits' grainlets (a sine formant windowed by a shaped carrier, reset each period,
 * band-limited at the reset), three of them, tuned to a vowel's first three formants: VOWEL morphs A E I O U, SHIFT
 * moves all three two octaves either way (the formant sweep), BREATH adds noise shaped by the carrier's window and
 * widens the window. A DC blocker at 0.3 f0 after */
enum { MI_FORMANTS = 3 };
typedef struct {
    float cph[MI_FORMANTS], fph[MI_FORMANTS], next[MI_FORMANTS];
    float f[MI_FORMANTS], gain[MI_FORMANTS], shape, breath, hp_g, hp_s;
} mi_grain;
void mi_grain_reset(mi_grain *s);
void mi_grain_block(mi_grain *s, float f0, float vowel, float shift, float breath);
float mi_grain_tick(mi_grain *s, float f0, uint32_t *rng);

/* CHORD: Plaits' chord engine, its divide-down organ / string machine (the wavetable half left out): eleven chords
 * (HARMONICS), their inversion across five voices (TIMBRE), and the registration, which of four octave-divided saws
 * and squares sound (MORPH) */
enum { MI_CHORD_VOICES = 5 };
typedef struct {
    float ph[MI_CHORD_VOICES], next[MI_CHORD_VOICES], f[MI_CHORD_VOICES];
    float g8[MI_CHORD_VOICES], g4[MI_CHORD_VOICES], g2[MI_CHORD_VOICES], g1[MI_CHORD_VOICES];
    int8_t seg[MI_CHORD_VOICES];
} mi_chord;
void mi_chord_reset(mi_chord *s);
void mi_chord_block(mi_chord *s, float f0, float chord, float inversion, float registration);
float mi_chord_tick(mi_chord *s);
extern const char *const mi_chord_names[11];

/* the tables (ch_mi_tables.c, generated by tools/gen_mi_tables.py) */
extern const int16_t ch_mi_ws[1285], ch_mi_fold[516], ch_mi_fold2[516];
extern const float ch_mi_stiffness[65];
