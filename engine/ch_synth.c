/* SYNTH: a macro oscillator for the MATERIAL slot, after the idea of a
 * many-model voice with three timbre controls (HARMONICS, TIMBRE, MORPH)
 * and a low-pass gate. Its own design: fifteen models, each a small
 * synthesis technique, all played from notes or the sequencer. Then five
 * engines from Felucca (ch_fe.c: ORGAN, PHASE, TRIO, LOFI, FM6, by Leo
 * Kuroshita; docs/THIRD_PARTY.md): HARMONICS TIMBRE MORPH are their three
 * knobs, they keep their own envelopes (DECAY, LPG, MODE and ATTACK are ours only).
 *
 * MODE: PING strikes the gate and lets it decay; GATE holds while the note
 * is held; DRONE keeps sounding (notes just set the pitch). Two voices per
 * track take turns so tails ring under the next note. */
#include "ch_internal.h"

#ifndef LPG_STRIDE
#define LPG_STRIDE 8u
#endif
enum { S_MODEL, S_PITCH, S_HARM, S_TIMBRE, S_MORPH, S_DECAY, S_LPG, S_MODE, S_ATTACK, S_GLIDE, S_LEVEL, S_VEL, S_CUTOFF, S_RESO, S_FENV, S_FATK,
       S_FDEC, S_AOCT, S_ASEMI, S_AFINE, S_BOCT, S_BSEMI, S_BFINE,
       S_NOISE, S_WIN, S_AMR, S_AMD, S_AMF, S_JIT, S_JRATE, S_VOICES, S_WTRIG, S_COUNT };   /* from S_FDEC: the extras, in CH_SYNTH_XROW (RIPPLE's from S_NOISE) */
#define PX(t, p) ((t)->base[CH_SYNTH_XROW][(p) - CH_PARAMS])
enum { VA, FOLD, FM2, FORMANT, ADDITIVE, WAVES, CHORD, SWARM, STRING, MODAL, BUBBLES, ACID, NOISE, RIPPLE, WOBBLE, FE_BASE };   /* (FORMANT: VOICE; BUBBLES: BUBBLE) */
/* oct: the model's own octave (a bass sits below the lanes' C4) */
typedef struct { const char *name, *h, *t, *m; float gain; int8_t oct; } smodel_t;
static const smodel_t SM[CH_SYNTH_MODELS] = {
    {"ANALOG",    "A SHAPE", "B SHAPE", "MIX",      0.80f, -1},   /* the virtual analogue (va_run below): two drifting oscillators, a sub and noise, a mixer that overdrives, a 24 dB ladder, two ADSRs */
    {"FOLD",      "SHAPE",   "FOLD",    "ASYM",     1.24f, 0},   /* Plaits' waveshaping engine (ch_mi.c) */
    {"FM2",       "RATIO",   "INDEX",   "FEEDBACK", 0.98f, 0},
    {"VOICE",     "VOWEL",   "SHIFT",   "BREATH",   2.00f, 0},   /* FORMANT and VOWEL merged: Plaits' grainlets on a vowel's formants (ch_mi.c) */
    {"ADDITIVE",  "CENTRE",  "SPREAD",  "ODD/EVEN", 1.08f, 0},
    {"WAVES",     "CRUSH",   "WAVE",    "SMOOTH",   1.14f, 0},
    {"CHORD",     "CHORD",   "INVERSION", "REGISTER", 1.45f, 0},   /* Plaits' chord engine, its divide-down organ (ch_mi.c) */
    {"SWARM",     "DETUNE",  "BRIGHT",  "JITTER",   1.20f, -1},
    {"STRING",    "DAMP",    "BRIGHT",  "STRUCTURE", 1.60f, 0},  /* Plaits' string voice (ch_mi.c): STRUCTURE a sitar's buzz below a quarter, plain at it, stiff above */
    {"MODAL",     "MATERIAL","BRIGHT",  "DAMP",     1.00f, 0},   /* Plaits' modal voice (ch_mi.c): MATERIAL the stiffness, harmonic to bell */
    {"BUBBLE",    "SPREAD",  "DENSITY", "RING",    0.62f, 0},  /* bubbles in water (below): decaying sines whose pitch rises; one a note, or a stream of them */
    {"ACID",      "WAVE",    "ACCENT",  "DRIVE",    1.00f, -2},   /* the 303 (acid_* below): its diode ladder, envelopes, accent sweep and slide */
    {"NOISE",     "RESO",    "COLOR",   "LP>BP>HP", 12.0f, 0},
    {"RIPPLE",    "SYNC",    "FOLD",    "BEND",     0.72f, 0},   /* a sine whose phase is folded, bent and synced (ripple_tick) */
    {"WOBBLE",    "RATE",    "DEPTH",   "SHAPE",    1.45f, -2},   /* a bass whose filter is swept in time with the tempo: ANALOG's voice (va_run: its oscillators, mixer and ladder) under the sweep */
    {"ORGAN",     0, 0, 0, 1.63f, 0},   /* Felucca's: labels from ch_fe_label; gains: a single note peaks ~0.5 like ours */
    {"PHASE",     0, 0, 0, 2.50f, 0},
    {"TRIO",      0, 0, 0, 1.45f, 0},
    {"LOFI",      0, 0, 0, 3.60f, 0},
    {"FM6",       0, 0, 0, 4.20f, 0},
};
const char *const ch_synth_names[CH_SYNTH_MODELS + 1] = {
    "ANALOG", "FOLD", "FM2", "VOICE", "ADDITIVE", "WAVES", "CHORD", "SWARM", "STRING", "MODAL",
    "BUBBLE", "ACID", "NOISE", "RIPPLE", "WOBBLE", "ORGAN", "PHASE", "TRIO", "LOFI", "FM6", 0};
const char *ch_synth_label(unsigned model, unsigned param)
{
    if (model >= CH_SYNTH_MODELS) return 0;
    if (model >= FE_BASE) {                               /* their own knobs; our envelope and gate on top (no DRONE) */
        if (param == S_HARM || param == S_TIMBRE || param == S_MORPH) return ch_fe_label(model - FE_BASE, param - S_HARM);
        return 0;
    }
    if (model == VA) {                                    /* its mixer, age and envelopes, on RIPPLE's extras, the LPG and WOBBLE's TRIG (which it has no other use for) */
        if (param == S_AMF) return "SUB";
        if (param == S_NOISE) return "NOISE";
        if (param == S_AMD) return "COLOR";
        if (param == S_AMR) return "FILTER";
        if (param == S_WTRIG) return "SYNC";
        if (param == S_LPG) return "SUSTAIN";
        if (param == S_WIN) return "RELEASE";
        if (param == S_JIT) return "F SUS";
        if (param == S_JRATE) return "F REL";
    }
    if (model == WOBBLE) {                                /* its oscillators, on RIPPLE's extras (which it has no other use for) */
        if (param == S_NOISE) return "A SHAPE";
        if (param == S_AMR) return "B SHAPE";
        if (param == S_AMD) return "MIX";
        if (param == S_AMF) return "SUB";
        if (param == S_WIN) return "TARGET";
        if (param == S_JIT) return "FADE IN";
        if (param == S_JRATE) return "OFFSET";
    }
    return param == S_HARM ? SM[model].h : param == S_TIMBRE ? SM[model].t : param == S_MORPH ? SM[model].m : 0;
}
static void ratio_text(float r, char *out)            /* RIPPLE's AM RATIO: 0.125 steps, as "1.25X" */
{
    int m = (int)(r * 1000 + 0.5f), w = m / 1000, f = m % 1000, k = 0;
    out[k++] = (char)('0' + w);
    if (f) { out[k++] = '.'; int d[3] = {f / 100, f / 10 % 10, f % 10}, last = d[2] ? 3 : d[1] ? 2 : 1; for (int i = 0; i < last; ++i) out[k++] = (char)('0' + d[i]); }
    out[k++] = 'X'; out[k] = 0;
}

/* the values with names: the choice is (int)(v * (n - 1) + 0.5), as the models read it */
static const char *const ACID_W[] = {"SAW", "SQUARE"};
static const char *const OFF_ON[] = {"OFF", "ON"};
static const char *const VA_FLT[] = {"LP 24", "LP 12", "BAND", "HIGH"};   /* ANALOG's FILTER: what is taken from its ladder */
static const char *const FMR_N[] = {"0.5", "1", "1.5", "2", "2.5", "3", "3.5", "4", "7"};
/* RATE: nine lengths in time with the tempo, then twenty of its own, in Hz (so one knob goes from locked to free) */
enum { WOB_SYNC = 9, WOB_RATES = 29 };
static const char *const WOB_R[WOB_RATES] = {"1 BAR", "1/2", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32",
    "0.1HZ", "0.2HZ", "0.3HZ", "0.5HZ", "0.75HZ", "1HZ", "1.5HZ", "2HZ", "3HZ", "4HZ", "5HZ", "6HZ", "8HZ", "10HZ", "12HZ", "16HZ", "20HZ", "25HZ", "32HZ", "40HZ"};
static const float WOB_BEATS[WOB_SYNC] = {4, 2, 1, 2.0f / 3, 0.5f, 1.0f / 3, 0.25f, 1.0f / 6, 0.125f};   /* a sweep's length, in beats */
static const float WOB_HZ[WOB_RATES - WOB_SYNC] = {0.1f, 0.2f, 0.3f, 0.5f, 0.75f, 1, 1.5f, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 25, 32, 40};
static const char *const WOB_T[] = {"FILTER", "AMP", "BOTH"};   /* what the sweep moves */
static const char *const WOB_W[] = {"SINE", "SIN/TRI", "TRI", "TRI/SAW", "SAW", "SAW/SQR", "SQUARE"};   /* the sweep's shape, and the ones between */
/* the sweep at p of its length, 1 open .. 0 shut: a sine, a triangle, a saw falling from open, a square */
static float wob_shape(int k, float p)
{
    return k <= 0 ? 0.5f + 0.5f * ch_sin_turns(p - 0.25f) : k == 1 ? (p < 0.5f ? 2 * p : 2 - 2 * p) : k == 2 ? 1 - p : p < 0.5f ? 1.0f : 0.0f;
}
static const char *const WAVES_N[] = {"SINE", "SIN/TRI", "TRI", "TRI/SAW", "SAW", "SAW/SQR", "SQUARE", "SQR/PLS", "PULSE"};
int ch_synth_value_text(unsigned model, unsigned param, float v, char *out)
{
    const char *const *n = 0; int c = 0;
    if (model == RIPPLE && param == S_WIN) {             /* the sync window: 1..16 cycles of it a cycle */
        int i = 1 + (int)(ch_clamp(v, 0, 1) * 15 + 0.5f); if (i >= 10) { out[0] = '1'; out[1] = (char)('0' + i - 10); out[2] = 0; } else { out[0] = (char)('0' + i); out[1] = 0; }
        return 16;
    }
    if (model == WOBBLE && param == S_TIMBRE) {           /* DEPTH: both ways, -100 .. +100 */
        int d = (int)(ch_clamp(v, 0, 1) * 200 + 0.5f) - 100, a = d < 0 ? -d : d, k = 0;
        if (d) out[k++] = d < 0 ? '-' : '+';
        if (a >= 100) out[k++] = '1';
        if (a >= 10) out[k++] = (char)('0' + a / 10 % 10);
        out[k++] = (char)('0' + a % 10); out[k] = 0;
        return 201;
    }
    if (model == WOBBLE && param == S_JRATE) {            /* OFFSET: where in the sweep it starts, 15 degrees a step */
        int d = (int)(ch_clamp(v, 0, 1) * 23 + 0.5f) * 15, k = 0;
        if (d >= 100) out[k++] = (char)('0' + d / 100);
        if (d >= 10) out[k++] = (char)('0' + d / 10 % 10);
        out[k++] = (char)('0' + d % 10); out[k] = 0;
        return 24;
    }
    if (model == RIPPLE && param == S_AMR) { ratio_text((float)(int)(ch_clamp(v, 0, 1) * 32 + 0.5f) * 0.125f, out); return 33; }
    if ((model == VA && (param == S_HARM || param == S_TIMBRE)) || (model == WOBBLE && (param == S_NOISE || param == S_AMR))) {   /* a SHAPE: TRI, SAW, SQUARE and all between, then the pulse narrowing (its width, per cent) */
        int i = (int)(ch_clamp(v, 0, 1) * 80 + 0.5f), k = 0;   /* (SINE first, since: twenty steps a stretch, four stretches) */
        const char *t = i == 0 ? "SINE" : i < 20 ? "SIN-TRI " : i == 20 ? "TRI" : i < 40 ? "TRI-SAW " : i == 40 ? "SAW" : i < 60 ? "SAW-SQR " : i == 60 ? "SQUARE" : "PULSE ";
        while (t[k]) { out[k] = t[k]; ++k; }
        if (i % 20 || i == 80) {
            int q = i < 60 ? (i % 20) * 5 : 50 - (i - 60) * 42 / 20;
            if (q >= 10) out[k++] = (char)('0' + q / 10);
            out[k++] = (char)('0' + q % 10);
        }
        out[k] = 0;
        return 81;
    }
    if (model == VA && param == S_WTRIG) { n = OFF_ON; c = 2; }
    else if (model == VA && param == S_AMR) { n = VA_FLT; c = 4; }
    else if (model == ACID && param == S_HARM) { n = ACID_W; c = 2; }
    else if (model == CHORD && param == S_HARM) { n = mi_chord_names; c = 11; }
    else if (model == FM2 && param == S_HARM) { n = FMR_N; c = 9; }
    else if (model == WAVES && param == S_TIMBRE) { n = WAVES_N; c = 9; }
    else if (model == WOBBLE && param == S_HARM) { n = WOB_R; c = WOB_RATES; }
    else if (model == WOBBLE && param == S_MORPH) { n = WOB_W; c = 7; }
    else if (model == WOBBLE && param == S_WIN) { n = WOB_T; c = 3; }
    if (!n) return 0;
    int i = (int)(ch_clamp(v, 0, 1) * (float)(c - 1) + 0.5f);
    unsigned k = 0; while (n[i][k]) { out[k] = n[i][k]; ++k; } out[k] = 0;
    return c;
}
/* where each model starts: its three knobs, DECAY, LPG, CUTOFF, RESO, ENV>FILT (-1: leave the parameter alone) */
typedef struct { float h, t, m, dec, lpg, cut, res, fenv, fatk, fdec; } spreset_t;
static const spreset_t SP[CH_SYNTH_MODELS] = {
    {0.5f,     0.5f,     0.5f, 0.5f, 0.7f, 0.5f,  0.2f,  0.45f, 0, 0.45f},  /* ANALOG: two saws, B a little sharp (B FINE), SUSTAIN at 70, the ladder half open and moving */
    {0.5f,  0.4f,     0.3f,  0.5f,  0.3f,  1,     0,     0, 0, 0.5f},
    {0.125f, 0.4f,    0.1f,  0.5f,  0.2f,  1,     0,     0, 0, 0.5f},       /* FM: ratio 1 */
    {0,     0.5f,     0.15f, 0.5f,  0.2f,  1,     0,     0, 0, 0.5f},       /* VOICE: A, as sung, a little breath */
    {0.4f,  0.5f,     0.5f,  0.6f,  0.2f,  1,     0,     0, 0, 0.5f},
    {0,     0.5f,     0.3f,  0.5f,  0.3f,  1,     0,     0, 0, 0.5f},       /* WAVES: a saw, unbroken */
    {0.4f,  0.45f,    0.15f, 0.6f,  0.3f,  0.8f,  0.1f,  0.2f, 0, 0.6f},    /* CHORD: minor 7, at the note, saws */
    {0.5f,  0.6f,     0.1f,  0.6f,  0.2f,  0.9f,  0,     0.2f, 0, 0.5f},
    {0.4f,  0.6f,     0.25f, 0.7f,  0,     1,     0,     0, 0, 0.5f},       /* STRING: plain (STRUCTURE at a quarter) */
    {0.5f,  0.6f,     0.5f,  0.6f,  0,     1,     0,     0, 0, 0.5f},
    {0.35f, 0.4f,     0.5f,  0.5f,  0,     1,     0,     0, 0, 0.5f},       /* BUBBLE: a little spread, ten a second */
    {0,     0.6f,     0.15f, 0.12f, 0,     0.35f, 0.6f,  0.65f, 0, 0.4f},   /* ACID: saw, ACCENT 60, a touch of DRIVE, a short release; the filter low, resonant, swept by its envelope */
    {0.3f,  0.5f,     0.3f,  0.35f, 0,     1,     0,     0, 0, 0.5f},
    {0.15f, 0.3f,     0.25f, 0.6f,  0.2f,  0.85f, 0.1f,  0.25f, 0, 0.5f},   /* RIPPLE: a little sync (the window in, 1.5x), some fold and bend, the filter moving */
    {4.0f / 28, 0.925f, 0,   0.7f,  0,     0.72f, 0.5f,  0, 0, 0.5f},       /* WOBBLE (RATE 1/8, DEPTH +85): an eighth, deep, a sine's sweep; the filter's top at about 3 kHz, resonant */
    {0.5f,  0.5f,     0.5f,  0.5f,  0,     1,     0,     0, 0, 0.5f},
    {0.5f,  0.5f,     0.5f,  0.5f,  0,     1,     0,     0, 0, 0.5f},
    {0.5f,  0.5f,     0.5f,  0.5f,  0,     1,     0,     0, 0, 0.5f},
    {0.5f,  0.5f,     0.5f,  0.5f,  0,     1,     0,     0, 0, 0.5f},
    {0,     0.5f,     0.5f,  0.7f,  0,     1,     0,     0, 0, 0.7f},       /* FM6: TINE EP */
};
/* WOBBLE's pages: the sweep (RATE DEPTH SHAPE), what it does (TARGET FADE IN OFFSET TRIG), the oscillators, then as the
 * others. Every parameter is in it; ch_synth_param_shown still hides what it has no use for */
const uint8_t *ch_synth_order(unsigned model)
{
    static const uint8_t WOB_ORDER[S_COUNT] = {S_MODEL, S_HARM, S_TIMBRE, S_MORPH, S_WIN, S_JIT, S_JRATE, S_WTRIG, S_NOISE, S_AMR, S_AMD, S_AMF,
        S_CUTOFF, S_RESO, S_FENV, S_DECAY, S_FATK, S_FDEC, S_PITCH, S_LPG, S_MODE, S_VOICES, S_ATTACK, S_GLIDE, S_LEVEL, S_VEL,
        S_AOCT, S_ASEMI, S_AFINE, S_BOCT, S_BSEMI, S_BFINE};
    /* ANALOG's: the oscillators' shapes; the filter (second, where the other models have theirs); the level's envelope; A's tuning and SYNC; B's tuning and COLOR; the mixer and the level; the filter's envelope; the rest */
    static const uint8_t VA_ORDER[S_COUNT] = {S_MODEL, S_HARM, S_TIMBRE, S_MORPH, S_CUTOFF, S_RESO, S_FENV, S_AMR,
        S_ATTACK, S_DECAY, S_LPG, S_WIN, S_AOCT, S_ASEMI, S_AFINE, S_WTRIG, S_BOCT, S_BSEMI, S_BFINE, S_AMD, S_AMF, S_NOISE, S_LEVEL, S_VEL,
        S_FATK, S_FDEC, S_JIT, S_JRATE, S_PITCH, S_GLIDE, S_MODE, S_VOICES};
    /* RIPPLE's: everything that shapes the wave on its first three pages (the sync, fold and bend; the noise and the AM;
     * the window and the jitter, with LEVEL), then the filter and the rest as the other models have them */
    static const uint8_t RIP_ORDER[S_COUNT] = {S_MODEL, S_HARM, S_TIMBRE, S_MORPH, S_NOISE, S_AMR, S_AMD, S_AMF, S_WIN, S_JIT, S_JRATE, S_LEVEL,
        S_CUTOFF, S_RESO, S_FENV, S_DECAY, S_FATK, S_FDEC, S_PITCH, S_LPG, S_MODE, S_VOICES, S_ATTACK, S_GLIDE, S_VEL,
        S_WTRIG, S_AOCT, S_ASEMI, S_AFINE, S_BOCT, S_BSEMI, S_BFINE};
    return model == WOBBLE ? WOB_ORDER : model == VA ? VA_ORDER : model == RIPPLE ? RIP_ORDER : 0;
}
const char *const *ch_synth_modes(unsigned model, unsigned param)
{
    static const char *const WOB_D[] = {"UNIPOLAR", "BIPOLAR", 0};
    /* CUTOFF's mode: how far the filter follows the keyboard (C4 is where CUTOFF reads true). ANALOG and WOBBLE start
     * at half, as their ladder always did; the other models start with none, as they always were */
    static const char *const KT_VA[] = {"KEY HALF", "KEY FULL", "KEY OFF", 0};
    static const char *const KT[] = {"KEY OFF", "KEY HALF", "KEY FULL", 0};
    if (param == S_CUTOFF && model < FE_BASE && model != ACID) return model == VA || model == WOBBLE ? KT_VA : KT;
    return model == WOBBLE && param == S_TIMBRE ? WOB_D : 0;
}
int ch_synth_param_shown(unsigned model, unsigned param)
{
    if (param < S_AOCT) return 1;
    if (param <= S_BFINE) return model == VA || model == WOBBLE;   /* the oscillators' tuning: ANALOG's, and WOBBLE's */
    if (param == S_VOICES) return model != ACID;            /* (the 303 is one voice already) */
    if (param == S_WTRIG) return model == WOBBLE || model == VA;   /* (ANALOG: its SYNC) */
    if ((model == WOBBLE || model == VA) && param >= S_NOISE && param <= S_JRATE) return 1;   /* (its OSC A, TARGET, OSC B, MIX, SUB, FADE IN, OFFSET) */
    return model == RIPPLE;                              /* NOISE .. J RATE: RIPPLE's */
}
int ch_synth_preset(unsigned model, unsigned param, float *v)
{
    if (model >= CH_SYNTH_MODELS) return 0;
    const spreset_t *p = &SP[model];
    switch (param) {
    case S_HARM: *v = p->h; return 1;
    case S_TIMBRE: *v = p->t; return 1;
    case S_MORPH: *v = p->m; return 1;
    case S_DECAY: *v = p->dec; return 1;
    case S_LPG: *v = p->lpg; return 1;
    case S_CUTOFF: *v = p->cut; return 1;
    case S_RESO: *v = p->res; return 1;
    case S_FENV: *v = p->fenv; return 1;
    case S_FATK: *v = p->fatk; return 1;
    case S_FDEC: *v = p->fdec; return 1;
    case S_AOCT: case S_BOCT: *v = 2; return 1;           /* the octave selects: 0 */
    case S_ASEMI: case S_BSEMI: *v = 12; return 1;        /* 0 semitones */
    case S_AFINE: *v = 0; return 1;
    case S_BFINE: *v = model == VA ? 0.2f : model == WOBBLE ? 0.24f : 0; return 1;  /* ANALOG: B 10 cents sharp; WOBBLE: 12 */
    case S_MODE: *v = 1; return 1;                        /* GATE for every model: a note lasts as long as its key or step */
    case S_ATTACK: case S_GLIDE: *v = 0; return 1;
    case S_NOISE: *v = model == WOBBLE ? 0.5f : 0; return 1;   /* (WOBBLE: A SHAPE a saw) */
    case S_WIN: *v = model == VA ? 0.3f : 0; return 1;    /* (ANALOG: its RELEASE) */
    case S_AMD: *v = model == WOBBLE ? 0.5f : model == VA ? 0.35f : 0; return 1;   /* (WOBBLE: its MIX, A and B alike; ANALOG: its COLOR) */
    case S_AMF: *v = model == WOBBLE ? 0.45f : model == VA ? 0.3f : 0; return 1;  /* (WOBBLE, ANALOG: the SUB) */
    case S_AMR: *v = model == WOBBLE ? 0.5f : model == VA ? 0 : 0.25f; return 1;   /* AM at the note (1X), off until AM DEPTH (WOBBLE: OSC B a saw; ANALOG: its FILTER, LP 24) */
    case S_JIT: *v = model == RIPPLE ? 0.1f : model == VA ? 0.25f : 0; return 1;   /* (ANALOG: F SUS) */
    case S_JRATE: *v = model == WOBBLE ? 0 : model == VA ? 0.3f : 0.4f; return 1;   /* (WOBBLE: its OFFSET; ANALOG: F REL) */
    case S_WTRIG: *v = 0; return 1;
    default: return 0;
    }
}
static const float FM_RATIO[9] = {0.5f, 1, 1.5f, 2, 2.5f, 3, 3.5f, 4, 7};

static inline float dcoef(float sec) { return ch_exp2(-9.966f / (ch_max(sec, 0.0003f) * (float)CH_RATE)); }
static inline float wrap1(float p) { p -= (float)(int)p; return p < 0 ? p + 1 : p; }   /* (a negative phase too: SWARM's jitter can step one back) */
CH_SI float blep(float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1; }
    if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
    return 0;
}
static inline float saw(float *ph, float inc)
{
    float p = *ph, s = 2 * p - 1 - blep(p, inc);
    p += inc; if (p >= 1) p -= 1; *ph = p;
    return s;
}
void ch_synth_init(ch_trk *t, unsigned ti)
{
    (void)ti;
    for (unsigned i = 0; i < CH_SVOICES; ++i) {
        ch_svoice *v = &t->synth.v[i];
        int16_t *ks = t->drum.v[i].ks;                   /* DRUM and SYNTH never share a track: one buffer */
        memset(v, 0, sizeof *v);
        v->ks = ks;
        v->rng.s = 0x9E3779B1u * (ti * 2 + i + 7);
        ch_fe_init(&v->fe, 0xFE11u + ti * 16u + i);
    }
}

static void acid_note(ch_synth *s, ch_svoice *v, const ch_trk *t, unsigned vel, int cont);
/* ANALOG, a note's start (in the flash: it runs once a note): its oscillators from anywhere in their cycle, as ones that
 * never stopped; each voice a little off the others (AGE) */
static CH_NOINLINE void va_note(ch_svoice *v, unsigned vi)
{
    static const float OFF[4][3] = {{0.3f, -0.5f, 0.2f}, {-0.7f, 0.4f, -0.6f}, {0.6f, 0.8f, 0.7f}, {-0.2f, -0.9f, -0.3f}};
    v->ph[0] = ch_randf(&v->rng); v->ph[1] = ch_randf(&v->rng); v->ph[2] = v->ph[0] * 0.5f;
    for (unsigned k = 3; k < 7; ++k) v->ph[k] = 0;       /* the slopes' blep state */
    v->ph[7] = 1;
    for (unsigned k = 0; k < 4; ++k) v->ry2[k] = ch_rands(&v->rng);   /* the drift: where each is, where each is going */
    v->ry2[4] = 0;
    for (unsigned k = 0; k < 3; ++k) v->rb1[k] = OFF[vi & 3u][k];
}
/* (in the flash since ANALOG's voice took its place in the RAM: a note's start runs once a note, the voice every frame) */
void ch_synth_note(ch_trk *t, unsigned ti, unsigned note, unsigned vel)
{
    (void)ti;
    ch_synth *s = &t->synth;
    unsigned mode = (unsigned)(PV(t, CH_MATERIAL, S_MODE) + 0.5f);
    if (!vel) {
        for (unsigned i = 0; i < CH_SVOICES; ++i) {
            if (s->v[i].on && s->v[i].note == note) { s->v[i].gate = 0; if (s->v[i].model >= FE_BASE) ch_fe_note_off(&s->v[i].fe); }
            if (s->v[i].pend_vel && s->v[i].pend_note == note) { s->v[i].pend_vel = 0; ++ch_dbg.pend_lost; }   /* let go before it began (under 3 ms): it never starts */
        }
        return;
    }
    unsigned model = (unsigned)(PV(t, CH_MATERIAL, S_MODEL) + 0.5f);
    if (model >= CH_SYNTH_MODELS) model = CH_SYNTH_MODELS - 1;
    ch_svoice *v;
    float glide_from = s->last_hz;
    /* the 303 is one voice: a note over a held one slides to it, nothing retriggers. VOICES at LEGATO: the same for any
     * of our models held by a GATE (a PING has rung away by then, and starts again): the note takes the held voice on,
     * its envelopes where they are, its pitch moving there at once or by GLIDE */
    if (model == ACID || (model < FE_BASE && mode == 1 && PX(t, S_VOICES) > 2.5f)) {
        float hz = 261.63f * ch_semis((float)note - 60 + 12 * SM[model].oct);
        for (unsigned k = 0; k < CH_SVOICES; ++k) {
            ch_svoice *w = &s->v[k];
            if (w->on && w->gate && !w->kill && w->model == model) { w->note = (uint8_t)note; w->hz_target = hz; s->last_hz = hz; return; }
        }
    }
    if (model >= FE_BASE && mode == 2) mode = 1;          /* Felucca's engines: no DRONE */
    if (mode == 2) v = &s->v[s->cur];                    /* DRONE: one voice, retuned */
    else {                                               /* a free voice, else one fading out, else the quietest let go, else the oldest */
        unsigned pick = CH_SVOICES, best = 0;
        /* VOICES: the notes this track may sound at once, 4, 2 or 1. A voice is what a block's time goes to on the FM-1
         * (some 500 us of a core's 5800 for a RIPPLE or an ANALOG note): a bass or a lead needs one. Notes past the
         * count take the oldest, as they do past four; voices above it that still ring are left to finish */
        unsigned vsel = (unsigned)(PX(t, S_VOICES) + 0.5f), nv = vsel == 1 ? 2u : vsel >= 2 ? 1u : (unsigned)CH_SVOICES;
        /* the 303 is one voice, always: a new note takes the voice the last one is still ringing on (its oscillator and
         * filter run on, the envelopes start again, as the circuit does), where it used to take a free one and leave the
         * old note's release sounding under it: up to four ladders at once, on a chip where one is a third of a core */
        if (model == ACID) for (unsigned k = 0; k < CH_SVOICES && pick == CH_SVOICES; ++k) if (s->v[k].on && s->v[k].model == ACID && !s->v[k].kill) pick = k;
        for (unsigned k = 0; k < nv && pick == CH_SVOICES; ++k) if (!s->v[k].on) pick = k;
        if (pick == CH_SVOICES) { float lo = 1e30f; for (unsigned k = 0; k < nv; ++k) if (s->v[k].kill && !s->v[k].pend_vel && s->v[k].fade < lo) { lo = s->v[k].fade; pick = k; } }
        if (pick == CH_SVOICES) { float lo = 1e30f; for (unsigned k = 0; k < nv; ++k) if (!s->v[k].gate && !s->v[k].pend_vel && s->v[k].env < lo) { lo = s->v[k].env; pick = k; } }
        if (pick == CH_SVOICES) for (unsigned k = 0; k < nv; ++k) if (!s->v[k].pend_vel && s->v[k].n >= best) { best = s->v[k].n; pick = k; }
        if (pick == CH_SVOICES) pick = s->cur < nv ? s->cur : 0;   /* (all of them already waiting: the last one is taken again) */
        s->cur = (uint8_t)pick; v = &s->v[pick];
        if (v->on && (!v->kill || v->fade > 0.05f) && ch_max(v->env, ch_abs(v->last)) > 1e-3f && model != ACID) {
            /* taken while sounding: its note fades out first (3 ms on an S curve), this one starts when it is gone. A voice cut
             * mid-waveform clicks however its last value is carried; a short wait does not */
            if (!v->kill) { v->kill = 2; if (v->fade > 1) v->fade = 1; if (v->n < (uint32_t)(CH_RATE / 10)) ++ch_dbg.cut_steal; }
            v->pend_note = (uint8_t)note; v->pend_vel = (uint8_t)(vel ? vel : 1);
            return;
        }
    }
    if (model >= FE_BASE) {
        float pitch = PV(t, CH_MATERIAL, S_PITCH) * 24;
        if (PMODE(t, CH_DEV_SYNTH, S_PITCH) == 0) pitch = (float)(int)(pitch + (pitch < 0 ? -0.5f : 0.5f));
        float knob[3] = {PV(t, CH_MATERIAL, S_HARM), PV(t, CH_MATERIAL, S_TIMBRE), PV(t, CH_MATERIAL, S_MORPH)};
        v->dc = v->on ? v->last : 0; v->dcp = 1;          /* (a taken voice: its last sound let go, not cut) */
        v->on = 1; v->gate = 1; v->note = (uint8_t)note; v->model = (uint8_t)model; v->n = 0;
        v->kill = 0; v->fade = 1; v->quiet = 0;
        v->env = 0; v->stage = 0; v->lpg_g = 0; memset(v->f, 0, sizeof v->f);   /* our envelope and gate, over theirs */
        v->vel = (float)vel * (1.0f / 127);
        if (v->fe_dirty) { ch_fe_init(&v->fe, 0xFE11u + ti * 16u + (unsigned)(v - s->v)); v->fe_dirty = 0; }   /* (a Mutable model had its room) */
        ch_fe_note_on(&v->fe, model - FE_BASE, 261.63f * ch_semis((float)note - 60 + pitch + 12 * SM[model].oct), v->vel, knob);
        return;
    }
    int fresh = !v->on || mode != 2, was_acid = v->on && v->model == ACID;
    v->kill = 0; v->fade = 1; v->quiet = 0;
    v->dc = fresh && v->on && !(model == ACID && was_acid) ? v->last : 0; v->dcp = 1;    /* a taken voice: its last sound let go over 3 ms, not cut (the 303 taking its own voice plays on: nothing to hand over) */
    v->on = 1; v->gate = 1; v->note = (uint8_t)note; v->model = (uint8_t)model;
    v->vel = (float)vel * (1.0f / 127);
    if (fresh) {
        v->env = 0; v->stage = 0; v->n = 0;              /* (DRONE too: it rises in, not on at once) */
        memset(v->f, 0, sizeof v->f);
        if (!(model == ACID && was_acid)) for (unsigned k = 0; k < 6; ++k) v->ry1[k] = v->ry2[k] = 0;   /* (the 303's filter runs on between notes: no click) */
        if (!(model == ACID && was_acid)) for (unsigned k = 0; k < 8; ++k) v->ph[k] = (float)k * 0.125f;   /* the oscillators from the same place each note (spread, for the unison models); the 303's runs free */
        v->fb = 0;
        if (model == BUBBLES) memset(v->bub, 0, sizeof v->bub);   /* BUBBLE: no bubble sounding */
        if (model == FOLD) { mi_fold_reset(&v->mfold); v->fe_dirty = 1; }
        if (model == MODAL) { mi_modal_reset(&v->mmod); v->fe_dirty = 1; }
        if (model == FORMANT) { mi_grain_reset(&v->mgrain); v->fe_dirty = 1; }
        if (model == CHORD) { mi_chord_reset(&v->mchord); v->fe_dirty = 1; }
        if (model == RIPPLE) {                            /* its jitter's lines from somewhere new, the window as set */
            v->rip.jph = ch_randf(&v->rng); for (unsigned k = 0; k < 4; ++k) { v->rip.jf[k] = ch_rands(&v->rng); v->rip.jt[k] = ch_rands(&v->rng); }
            v->rip.win = 1 + (float)(int)(PX(t, S_WIN) * 15 + 0.5f); v->ph[0] = 0; v->fe_dirty = 1;
        }
        if (model == VA || model == WOBBLE) va_note(v, (unsigned)(v - s->v));
        if (model == STRING) { mi_string_reset(&v->mstr, v->ks, CH_KS_LEN); v->ks_w = 0; v->fe_dirty = 1; }
    }
    if (model == STRING) v->mstr.trig = 1;                /* the pluck */
    if (model == MODAL) v->mmod.trig = 1;                 /* the strike */
    float hz = 261.63f * ch_semis((float)note - 60 + 12 * SM[model].oct);
    v->hz_target = hz;
    v->flt_g = 0;                                         /* the filter's envelope: from the top, or rising through F ATTACK */
    v->fstage = PV(t, CH_MATERIAL, S_FATK) > 0.005f ? 0 : 1; v->fenv = v->fstage ? 1.0f : 0.0f;
    if (model == ACID) acid_note(s, v, t, vel, was_acid);
    v->hz = (glide_from > 0 && PV(t, CH_MATERIAL, S_GLIDE) > 0.01f) ? glide_from : hz;
    s->last_hz = hz;
}

typedef struct { float pitch, harm, timbre, morph, dec, lpg, att, glide, lvl, vels, cut, res, fenv, fatk, fdec, aratio, bratio;
                 float rnoise, rwin, ramr, ramd, ramf, rjit, rjrate, wtrig; unsigned mode, wbi; } sparams;   /* r*: RIPPLE's extras */

/* ACID: the TB-303, after Tim Stinchcombe's analysis of its filter, and Robin Schmidt's Open303.
 *   VCO       a saw, or the square the 303 makes from it with a transistor shaper (WAVE), through a 44 Hz high-pass
 *   VCF       the four-stage diode ladder: unbuffered, so its poles spread to 0.13, 1.04, 2.33 and 3.24 times the
 *             cutoff (Stinchcombe's H(s) = 1 / (s^4 + 6.727 s^3 + 14.142 s^2 + 9.514 s + 1)); the resonance feeds the
 *             output back through a 150 Hz high-pass (Open303); it would oscillate at k = 17, and RESO reaches 16.8.
 *             Solved without a delay in the loop (TPT), the drive a tanh at the ladder's input. The passband falls
 *             as 1 / (1 + k), the 303's thinning at high resonance, made up in part.
 *   cutoff    CUTOFF 200 Hz to 4 kHz (the 303's knob spans ~310 Hz to 2.4 kHz), swept up to 4.5 octaves by the
 *             main envelope through ENV>FILT; the envelope sits 0.3 of its height below the knob at rest
 *   MEG       the main envelope: an exponential decay, F DECAY 200 ms to 2 s; 200 ms on an accented note
 *   accent    velocity above 100 (the keys play 100). ACCENT sets how much the MEG's pulse adds to the cutoff and the volume,
 *             through a lag that RESO lengthens (the "wow"); its charge (C13) remains for half a second, so a run of
 *             accents sweeps higher each time
 *   VCA       3 ms attack, held while the gate is, DECAY the release (2 ms to 1 s); the MEG adds to the level
 *   DRIVE     to half: the ladder's input driven, up to 6x; past half: an overdrive after the VCA, as a pedal after the
 *             303, hard, a little asymmetric, darkening as it is pushed
 *   slide     a note over a held one: no retrigger, 60 ms to the new pitch (GLIDE sets the time when it is up)
 *   out       a 24 Hz high-pass, the 303's loss of the lowest bass
 * per-voice state: ry1[0..3] the ladder stages, ry1[4] the feedback high-pass, ry1[5] the input high-pass,
 * ry2[0] the output high-pass, ry2[1] the accent lag, ry2[2] the VCA envelope, ry2[3] this note's accent,
 * ry2[4] the lag's coefficient; fenv the MEG; rb1[0..3] the stages' G this stride */
static const float ACID_POLE[4] = {0.13f, 1.04f, 2.33f, 3.24f};
static inline float acid_tpt_g(float hz) { float g = ch_svf_g(hz); return g / (1 + g); }
static void acid_note(ch_synth *s, ch_svoice *v, const ch_trk *t, unsigned vel, int cont)
{
    float res = PV(t, CH_MATERIAL, S_RESO), acc = PV(t, CH_MATERIAL, S_TIMBRE);
    if (!cont) { v->ry2[2] = 0; v->ry2[1] = 0; }       /* (taking its own ringing voice: the VCA and the accent's lag go on from where they are, no step) */
    v->stage = 1;        /* (the MEG starts as set before the call: at the top, or rising through F ATTACK) */
    v->ry2[3] = vel > 100 ? acc * (1 + 0.8f * res * s->c13) : 0;     /* the sweep: more with the charge left by the last accent */
    if (vel > 100) s->c13 = ch_min(1.5f, s->c13 + 0.6f);
    v->ry2[4] = 1 - ch_exp2(-1.4427f / ((0.005f + 0.05f * res) * (float)CH_RATE));   /* the lag: RESO up, the wow slower */
}
static void acid_block(ch_svoice *v, const sparams *P)
{
    float r = ch_clamp(P->res, 0, 1);
    v->kc[0] = ch_exp_map(P->cut, 200, 4000);                           /* the knob's cutoff */
    float rs = (1 - ch_exp2(-4.328f * r)) * 1.0524f;                     /* Open303's resonance skew: (1 - e^-3r) / (1 - e^-3) */
    v->kc[1] = 16.8f * rs;                                              /* the ladder oscillates at k = 17 */
    v->kc[2] = 1 + 2.5f * rs;                                           /* some of the passband back */
    v->kc[3] = 0.5f + (P->fenv >= 0 ? 4.0f : 4.5f) * P->fenv;           /* the MEG's reach, octaves (ENV>FILT under 0: the other way, to four octaves down; nothing at -11) */
    v->kc[4] = ch_exp2(-1.4427f / (ch_exp_map(P->fdec, 0.2f, 2.0f) * (float)CH_RATE));   /* the MEG's fall */
    v->kc[5] = ch_exp2(-1.4427f / (0.2f * (float)CH_RATE));             /* on an accent: 200 ms */
    v->kc[6] = ch_exp2(-1.4427f / (ch_exp_map(P->dec, 0.002f, 1.0f) * (float)CH_RATE));   /* the VCA's release */
    v->kc[7] = 1 + ch_min(P->morph, 0.5f) * 10;                         /* DRIVE's first half: into the ladder, up to 6x */
    v->kg[0] = acid_tpt_g(44.5f); v->kg[1] = acid_tpt_g(24.2f); v->kg[2] = acid_tpt_g(150);
    v->rb2[0] = P->fatk > 0.005f ? 1 - ch_exp2(-1.0f / (ch_exp_map(P->fatk, 0.001f, 2.0f) * (float)CH_RATE) * 1.44f * 4) : 1;   /* F ATTACK: the MEG's rise */
    v->rb2[1] = acid_tpt_g(3200);                                       /* the overdrive's tone */
}
static inline float acid_hp(float *st, float x, float G)   /* a TPT one-pole high-pass */
{
    float vv = (x - *st) * G, lp = vv + *st; *st = lp + vv; return x - lp;
}
static float acid_tick(ch_svoice *v, const sparams *P, float inc)
{
    /* the VCO: the saw; or the 303's square, made from it, a true square with the ramp leaking through as a tilt */
    float p = v->ph[0], sw = 2 * p - 1 - blep(p, inc), x = sw;
    if (P->harm >= 0.5f) {
        float q = p + 0.5f; if (q >= 1) q -= 1;
        x = ((p < 0.5f ? 1.0f : -1.0f) + blep(p, inc) - blep(q, inc)) * 0.8f + sw * 0.15f;
    }
    p += inc; if (p >= 1) p -= 1; v->ph[0] = p;
    x = acid_hp(&v->ry1[5], x, v->kg[0]);
    /* the envelopes: the MEG, its lagged copy for the accent, the VCA */
    float acc = v->ry2[3];
    if (v->fstage == 0) { v->fenv += (1.05f - v->fenv) * v->rb2[0]; if (v->fenv >= 1) { v->fenv = 1; v->fstage = 1; } }
    else v->fenv *= acc > 0 ? v->kc[5] : v->kc[4];
    v->ry2[1] += (v->fenv - v->ry2[1]) * v->ry2[4];
    int gated = v->gate || P->mode == 2;
    if (gated) v->ry2[2] += (1 - v->ry2[2]) * 0.0075f; else v->ry2[2] *= v->kc[6];
    v->env = v->ry2[2] * (0.45f + 0.45f * v->fenv + acc * 1.8f * v->ry2[1]);
    /* the cutoff, every 8 samples */
    if ((v->n & 7u) == 0) {
        float oct = v->kc[3] * (v->fenv - 0.3f) + acc * 1.6f * v->ry2[1];
        float fc = ch_clamp(v->kc[0] * ch_exp2(oct), 20, 9000);
        for (unsigned k = 0; k < 4; ++k) v->rb1[k] = acid_tpt_g(ch_min(fc * ACID_POLE[k], 18000));
    }
    /* the ladder: four one-poles at the spread poles, k of the output fed back through the 150 Hz high-pass */
    x = ch_tanh(x * v->kc[7]);
    float G1 = v->rb1[0], G2 = v->rb1[1], G3 = v->rb1[2], G4 = v->rb1[3], Gh = v->kg[2], k = v->kc[1];
    float S1 = (1 - G1) * v->ry1[0], S2 = (1 - G2) * v->ry1[1], S3 = (1 - G3) * v->ry1[2], S4 = (1 - G4) * v->ry1[3];
    float Gt = G1 * G2 * G3 * G4, St = ((S1 * G2 + S2) * G3 + S3) * G4 + S4;
    float kh = k * (1 - Gh), sh = v->ry1[4];
    float y4 = (Gt * x + Gt * kh * sh + St) / (1 + Gt * kh);
    float u = x - kh * (y4 - sh);
    float yy = u;
    for (unsigned s = 0; s < 4; ++s) { float vv = (yy - v->ry1[s]) * v->rb1[s]; yy = vv + v->ry1[s]; v->ry1[s] = yy + vv; }
    { float vv = (y4 - sh) * Gh, lp = vv + sh; v->ry1[4] = lp + vv; }
    float y = yy * v->kc[2];
    y = acid_hp(&v->ry2[0], y, v->kg[1]);
    y *= v->env;                                           /* the VCA, here: what follows is a pedal after the 303 */
    float od = ch_max(P->morph - 0.5f, 0) * 2;             /* DRIVE's second half: the overdrive stage */
    if (od > 0) {
        y = ch_tanh((y * (1 + od * 20) + od * 0.3f) * 0.8f) * 1.25f;   /* hard driven, a little asymmetric */
        y = acid_hp(&v->rb2[2], y, v->kg[1]);               /* the asymmetry's offset out */
        v->ry2[5] += (y - v->ry2[5]) * v->rb2[1];            /* its tone: darker as it is pushed */
        y = ch_lerp(y, v->ry2[5], od) * (1 - 0.6f * od);
        return y;
    }
    return ch_tanh(y * 0.8f) * 1.25f;
}
/* what a model needs that only changes with the knobs or the pitch: once a block (32 samples), not every sample
 * (on the FM-1 the exponentials and filter coefficients here were most of a voice's time) */
static void model_block(ch_svoice *v, const sparams *P, float hz, float inc, unsigned n)
{
    float h = P->harm, tm = P->timbre, mo = P->morph;
    switch (v->model) {
    case FORMANT: mi_grain_block(&v->mgrain, inc, h, tm, mo); break;   /* VOICE: VOWEL, SHIFT, BREATH */
    case MODAL: mi_modal_block(&v->mmod, inc, h, tm, 1 - mo, v->vel, P->mode == 2); break;   /* MATERIAL, BRIGHT, DAMP (inverted, as STRING) */
    case ADDITIVE: {                                    /* the 8 partials' levels, and their sum */
        float centre = 1 + h * 7, width = 0.5f + tm * 4, norm = 0;
        for (unsigned k = 1; k <= 8; ++k) {
            float a = 0;
            if (inc * (float)k <= 0.45f) {
                float d = ((float)k - centre) / width; a = ch_exp2(-d * d * 1.44f);
                if (k & 1) a *= 1 - ch_max(0, mo - 0.5f) * 2; else a *= 1 - ch_max(0, 0.5f - mo) * 2;
            }
            v->kc[k - 1] = a; norm += a;
        }
        v->kg[0] = norm > 0 ? 1 / ch_max(norm, 1) : 0;
        break;
    }
    case WAVES: v->kg[0] = ch_svf_g(ch_min(hz * 4, 16000)); break;
    case CHORD: mi_chord_block(&v->mchord, inc, h, tm, mo); break;
    case SWARM:
        for (unsigned k = 0; k < 6; ++k) v->kc[k] = ch_min(inc * (1 + ((float)k - 2.5f) * h * 0.012f), 0.45f);
        v->kg[0] = ch_svf_g(ch_min(ch_exp_map(tm, 300, 16000), 16000));
        break;
    case STRING: mi_string_block(&v->mstr, inc, mo, tm, 1 - h, v->vel, P->mode == 2); break;   /* STRUCTURE, BRIGHT, DAMP (inverted: Plaits' damping is how long it rings) */
    case FOLD: mi_fold_block(&v->mfold, inc, h, tm, mo); break;
    case BUBBLES: {                                     /* BUBBLE: SPREAD (h), DENSITY (tm), RING (mo) */
        v->kc[0] = tm < 0.02f ? -1.0f : (float)CH_RATE / (0.7f * ch_exp2(tm * 9.2f));   /* DENSITY: frames between bubbles, 0.7 .. 400 a second; at 0 none but the note's own */
        v->kc[1] = 0.12f * ch_exp2(-mo * 5.3f);          /* RING: what a bubble loses a radian (one that rings a cycle .. forty) */
        v->kc[2] = 0.04f + 1.4f * h * h;                 /* SPREAD: how far its pitch rises as it dies (the drop's "bloop") */
        v->kc[3] = h;
        v->kc[6] = 3 + 4 * h + 30 * h * h;               /* and how far from the note the stream's bubbles land, semitones either way (never none: two at one pitch beat) */
        v->kc[4] = v->kc[0] > 0 ? 1.15f / v->kc[0] : 0;  /* the least a bubble loses a frame: a dense stream's are short, so four or so sound at once, not more than there is room for */
        v->kc[5] = 0.6f - 0.25f * tm;                    /* the stream's level: more at once, each quieter */
        v->kc[7] = 1 - 0.5f * tm;                        /* and the note's own bubble comes down toward it */
        v->kg[1] = 14 * tm * tm;                         /* a dense stream is small bubbles: higher */
        if (v->kc[0] > 0 && v->ph[1] <= 0 && v->ph[0] > 2 * v->kc[0]) v->ph[0] = v->kc[0];   /* DENSITY turned up: the next is not a long wait away */
        for (unsigned q = 0; q < CH_BUBBLES; ++q)        /* one that has rung out is over */
            if (v->bub[q].e != 0 && ch_abs(v->bub[q].s) + ch_abs(v->bub[q].c) < 1e-4f) v->bub[q].e = v->bub[q].s = v->bub[q].c = 0;
        break;
    }
    case VA: case WOBBLE: v->kc[0] = P->aratio; v->kc[1] = P->bratio; break;   /* the oscillators' tuning (A OCT, A SEMI, A FINE; B likewise) */
    case ACID: acid_block(v, P); break;
    case NOISE: v->kg[0] = ch_svf_g(ch_min(hz * ch_exp2((tm - 0.5f) * 6), 16000)); break;
    case RIPPLE:                                        /* the jitter's speed, the AM's ratio, the window to take at the next cycle */
        v->kc[0] = ch_exp_map(P->rjrate, 0.1f, 30.0f) / (float)CH_RATE;
        v->kc[1] = (float)(int)(P->ramr * 32 + 0.5f) * 0.125f;
        v->kc[2] = 1 + (float)(int)(P->rwin * 15 + 0.5f);
        {   /* SYNC, FOLD, BEND and NOISE with their JITTER, and all that follows from them alone, once a block (ripple_tick
             * made them again every sample: four lines read and interpolated, a dozen clamps, two divides; the jitter is a
             * slow drift, a block is 0.7 ms of it). kc[3..7], rb1, rb2: this model's to use */
            float jw = v->rip.jph + v->kc[0] * (float)n;
            while (jw >= 1) { jw -= 1; for (unsigned k = 0; k < 4; ++k) { v->rip.jf[k] = v->rip.jt[k]; v->rip.jt[k] = ch_rands(&v->rng); } }
            v->rip.jph = jw;
            float ja = P->rjit * 0.25f, j[4];
            for (unsigned k = 0; k < 4; ++k) j[k] = ja * (v->rip.jf[k] + (v->rip.jt[k] - v->rip.jf[k]) * jw);
            float S = ch_clamp(10 * (P->harm + j[0]), 0, 10), F = ch_clamp(2 * (P->timbre + j[1]), 0, 2);
            float B = ch_clamp(4 * (P->morph + j[2]), 0, 4), N = ch_clamp(P->rnoise + j[3], 0, 1) * 0.75f;
            float k = 1 - ch_min(F, 1) * 0.5f;
            v->kc[3] = k; v->kc[4] = 1 / k; v->kc[5] = 1 / (1 - k + 1e-6f);
            v->kc[6] = ch_clamp(F, 1, 1.75f); v->kc[7] = ch_clamp(F, 1.3f, 2) - 0.3f;
            for (unsigned q = 0; q < 4; ++q) v->rb1[q] = ch_clamp(B - (float)q, 0, 1);
            v->rb2[0] = ch_min(S, 1); v->rb2[1] = ch_max(S, 1); v->rb2[2] = N;
        }
        break;
    default: break;
    }
}

/* RIPPLE: one sine, its phase shaped before it is read. FOLD mirrors the phase about a point that moves in from the
 * end and folds it back on itself twice; BEND squares it in up to four stages (the cycle bunched toward its start);
 * SYNC first fades in a window (WINDOW humps of a raised cosine across the cycle) then runs the shaped phase 1..10
 * times a cycle, hard sync without the click. NOISE shakes the phase, most at the middle of each half cycle. The
 * plain sine stays under it at half, so the note holds. AM: a sine at AM RATIO times the note, AM DEPTH of it;
 * AM FDBK feeds its last value back into its own level, roughening it. JITTER: a slow random line each for SYNC,
 * FOLD, BEND and NOISE (J RATE its speed), a voice its own. */
static inline float fold01(float x) { x = x * 0.5f; x -= (float)(int)x; x *= 2; return x > 1 ? 2 - x : x; }
static float ripple_tick(ch_svoice *v, const sparams *P, float inc)
{
    float p = v->ph[0];
    v->ph[0] = wrap1(p + inc);
    if (v->ph[0] < p) v->rip.win = v->kc[2];          /* a new cycle: the window as set now */
    /* FOLD (its slopes and gains from model_block) */
    float k = v->kc[3], x = p < k ? p * v->kc[4] : 1 - (p - k) * v->kc[5];
    x = fold01(x * v->kc[6]);
    x = fold01(x * v->kc[7]);
    /* BEND: the stages that are on (the others would multiply by 1) */
    { float c0 = v->rb1[0], c1 = v->rb1[1], c2 = v->rb1[2], c3 = v->rb1[3];
      if (c0 > 0) { x = x * (x * c0 + 1 - c0); if (c1 > 0) { x = x * (x * c1 + 1 - c1); if (c2 > 0) { x = x * (x * c2 + 1 - c2); if (c3 > 0) x = x * (x * c3 + 1 - c3); } } } }
    /* SYNC: the window, then the ratio */
    float c = v->rb2[0], wi = ch_sin_turns(0.5f * v->rip.win * x), win = 1 - c + c * wi * wi;
    float sp = v->rb2[1] * x, N = v->rb2[2];
    float s1 = ch_sin_turns(p);
    float nz = N > 0 ? ch_rands(&v->rng) * s1 * -0.5f * N : 0;
    float y = ch_sin_turns(sp + nz) * win + 0.5f * s1 + 0.75f * nz;
    /* AM */
    if (P->ramd > 0.001f) {
        float pm = v->ph[1]; v->ph[1] = wrap1(pm + ch_min(inc * v->kc[1], 0.45f));
        float fk = P->ramf * 0.85f;                    /* (at 1 its level would only ever feed back its own 0: silence) */
        float m = ch_sin_turns(pm) * (fk * v->fb + 1 - fk);
        v->fb = m;
        y *= 1 - P->ramd + P->ramd * m;
    }
    return y;
}
/* BUBBLE: the wait for the next bubble, in mean waits: mostly short, now and then long (they come in clusters, with no pulse of their own) */
static inline float bub_wait(ch_svoice *v) { float u = ch_randf(&v->rng); return 0.04f + 2.9f * u * u; }
/* BUBBLE: a bubble begins, semi semitones from the note: in a free place, or the faintest one's */
static void bub_spawn(ch_svoice *v, float inc, float semi, float amp)
{
    unsigned at = 0; float least = 1e9f;
    for (unsigned q = 0; q < CH_BUBBLES; ++q) {
        float m = v->bub[q].e == 0 ? -1.0f : ch_abs(v->bub[q].s) + ch_abs(v->bub[q].c);
        if (m < least) { least = m; at = q; }
    }
    float x = CH_TWO_PI * ch_min(semi == 0 ? inc : inc * ch_semis(semi), 0.18f);
    float e = x - x * x * x * (1.0f / 24);               /* 2 sin(x / 2): the angle a frame */
    v->bub[at].s = 0; v->bub[at].c = amp; v->bub[at].e = e;
    float dec = ch_max(v->kc[1] * e, v->kc[4]);          /* it dies by the radian, so a high one sooner (and in a dense stream sooner still) */
    v->bub[at].dk = 1 - dec;
    v->bub[at].de = v->kc[2] * dec * e;                  /* and rises as it dies */
}

static float model_tick(ch_svoice *v, const sparams *P, float hz, float inc)
{
    float h = P->harm, tm = P->timbre, mo = P->morph, y = 0, noise;
    switch (v->model) {
    case ACID: y = acid_tick(v, P, inc); break;
    case RIPPLE: y = ripple_tick(v, P, inc); break;
    case FOLD: y = mi_fold_tick(&v->mfold, inc, 0); break;
    case FM2: {
        int ri = (int)(h * 8 + 0.5f);
        float pc = v->ph[0], pm = v->ph[1];
        v->ph[0] = wrap1(pc + inc); v->ph[1] = wrap1(pm + ch_min(inc * FM_RATIO[ri], 0.45f));
        float m = ch_sin_turns(pm + mo * 0.25f * v->fb);
        v->fb = m;
        y = ch_sin_turns(pc + tm * tm * 4 * 0.159f * m);
        break;
    }
    case FORMANT: y = mi_grain_tick(&v->mgrain, inc, &v->rng.s); break;
    case ADDITIVE: {
        float p = v->ph[0]; v->ph[0] = wrap1(p + inc);
        float s1 = ch_sin_turns(p), c2 = 2 * ch_sin_turns(p + 0.25f), sp = 0, sk = s1, sum = 0;
        for (unsigned k = 0; k < 8; ++k) {
            sum += v->kc[k] * sk;
            float sn = c2 * sk - sp; sp = sk; sk = sn;
        }
        y = sum * v->kg[0];
        break;
    }
    case WAVES: {
        float p = v->ph[0]; v->ph[0] = wrap1(p + inc);
        float w[5] = {ch_sin_turns(p), 4 * ch_abs(p - 0.5f) - 1, 2 * p - 1, p < 0.5f ? 1.0f : -1.0f, p < 0.15f ? 1.0f : -0.4f};
        float pos = tm * 3.999f; int i = (int)pos; float fr = pos - (float)i;
        float x = ch_lerp(w[i], w[i + 1], fr);
        x = ch_lerp(x, ch_svf_tick(&v->f[0], x, v->kg[0], 1.2f).lp, mo);
        if (h > 0.02f) { float q = 2 + (1 - h) * 30; x = (float)(int)(x * q) / q; }
        y = x;
        break;
    }
    case CHORD: y = mi_chord_tick(&v->mchord); break;
    case SWARM: {
        for (unsigned k = 0; k < 6; ++k) {
            if (mo > 0.01f && ch_randf(&v->rng) < 0.0005f) v->ph[k] = wrap1(v->ph[k] + ch_rands(&v->rng) * mo * 0.2f);
            y += saw(&v->ph[k], v->kc[k]) * 0.22f;
        }
        y = ch_svf_tick(&v->f[0], y, v->kg[0], 1.2f).lp;
        break;
    }
    case STRING: y = mi_string_tick(&v->mstr, v->ks, &v->ks_w, &v->rng.s); break;
    case MODAL: y = mi_modal_tick(&v->mmod, &v->rng.s); break;
    case BUBBLES: {
        /* BUBBLE: bubbles. Each is a sine that starts at zero, dies away at a rate set by its own pitch (a small
         * bubble is a short one) and rises in pitch as it does: the sound of air in water (van den Doel's model).
         * A note is one bubble at its pitch; SPREAD adds the splash after it (a few small high ones) and DENSITY
         * a stream of them, each within SPREAD of the note. No filter and no sine a frame: a bubble is a pair of
         * numbers turned through its angle. */
        if (!v->n) {
            bub_spawn(v, inc, 0, v->kc[7]);
            v->ph[1] = (float)(int)(v->kc[3] * v->kc[3] * 7 + 0.5f);   /* the splash: how many */
            v->ph[0] = v->ph[1] > 0 ? 0.008f * (float)CH_RATE : v->kc[0] > 0 ? v->kc[0] * bub_wait(v) : 1e9f;
        } else if ((v->ph[0] -= 1) <= 0) {
            if (v->ph[1] > 0) {
                v->ph[1] -= 1;
                bub_spawn(v, inc, 10 + v->kc[6] * ch_randf(&v->rng), 0.2f + 0.3f * ch_randf(&v->rng));
                v->ph[0] = v->ph[1] > 0 ? (0.003f + 0.012f * ch_randf(&v->rng)) * (float)CH_RATE : v->kc[0] > 0 ? v->kc[0] * bub_wait(v) : 1e9f;
            } else if (v->kc[0] > 0) {
                bub_spawn(v, inc, v->kg[1] + v->kc[6] * ch_rands(&v->rng), v->kc[5] * (0.35f + 0.65f * ch_randf(&v->rng)));
                v->ph[0] = v->kc[0] * bub_wait(v);
            } else v->ph[0] = 1e9f;
        }
        for (unsigned q = 0; q < CH_BUBBLES; ++q) {
            float e = v->bub[q].e;
            if (e == 0) continue;
            float bs = v->bub[q].s, bc = v->bub[q].c, dk = v->bub[q].dk;
            bs += e * bc; bc -= e * bs;
            v->bub[q].s = bs * dk; v->bub[q].c = bc * dk;
            if (e < 1.2f) v->bub[q].e = e + v->bub[q].de;
            y += bs;
        }
        y = ch_clamp(y * 1.1f, -1.5f, 1.5f); y -= y * y * y * (4.0f / 27);   /* many at once: eased under 1 */
        break;
    }
    case NOISE: {
        noise = ch_rands(&v->rng);
        ch_svf_out_t o = ch_svf_tick(&v->f[0], noise, v->kg[0], 2.0f - h * 1.95f);
        y = mo < 0.5f ? ch_lerp(o.lp, o.bp, mo * 2) : ch_lerp(o.bp, o.hp, (mo - 0.5f) * 2);
        break;
    }
    }
    return y;
}

/* ANALOG: a virtual analogue voice.
 *   OSC A, B  each a SHAPE from sine through triangle, saw and square to a narrow pulse, every shape between (a sine
 *             faded into the slope oscillator's triangle; the slope oscillator from there to the saw; above it two saws, the second taken from the first: none of it a saw, all of it
 *             a pulse, its width narrowing past square). Each wanders a few cents on its own (AGE), starts each note
 *             anywhere in its cycle, and each voice sits a little off the others in tuning, cutoff and envelope time, as
 *             a polysynth's voice cards do. SYNC: B starts its cycle again whenever A does.
 *   MIXER     A and B (MIX: both whole at the middle), a SUB sine an octave under A, NOISE: added, not crossfaded, into
 *             a soft clip, so the more goes in the more it is pressed, as an analogue mixer driven into its filter. COLOR
 *             pushes what leaves the mixer up to six times harder into the ladder's own clip, and three into one after
 *             it, and is the AGE as well: one knob from exact and clean to worn and driven.
 *   FILTER    a four-pole ladder, 24 dB an octave, solved without a delay in its loop (four one-poles, the feedback
 *             taken out by algebra), the clip at its input inside the loop: RES to self-oscillation, the bass it takes
 *             partly made up. It follows the keyboard by half.
 *   ENVELOPES two ADSRs as a capacitor's: the attack toward a little past the top, decay toward SUSTAIN, release toward
 *             nothing. PING: attack and decay only (no sustain), whatever the key does; GATE: all four; DRONE: held.
 * per-voice state: ph[0..2] A, B, the sub; ph[3..6] the slopes' blep; ph[7] the size of B's step at a sync;
 * ry1[0..3] the ladder, ry1[4] the low cut; ry2[0..3] the drift now and where it goes, ry2[4] frames to its next turn;
 * rb1[0..2] this voice's own offsets (A, B and cutoff, envelope time) */
CH_SI float va_sin(float x)                             /* (ch_sin_turns, forced inline: for the voice in the RAM) */
{
    x -= (float)(int)x; if (x < 0) x += 1;
    float a = x < 0.5f ? x : x - 1.0f;
    a *= CH_TWO_PI;
    if (a > CH_PI * 0.5f) a = CH_PI - a; else if (a < -CH_PI * 0.5f) a = -CH_PI - a;
    const float q = a * a;
    return a * (1 + q * (-1.0f / 6 + q * (1.0f / 120 + q * (-1.0f / 5040 + q * (1.0f / 362880)))));
}
CH_SI float va_clip(float x) { x = ch_clamp(x, -1.5f, 1.5f); return x - x * x * x * (4.0f / 27); }
CH_SI float va_osc(float sh, float *ph, float *s1, float *s2, float inc, float jump)
{
    if (sh < 1) {                                        /* SINE to TRI: the sine in step with the slope's triangle (both at their foot where the cycle starts), one faded into the other */
        float p0 = *ph; p0 -= (float)(int)p0; if (p0 < 0) p0 += 1;
        const float sn = -va_sin(p0 + 0.25f);
        if (sh <= 0) { p0 += inc; if (p0 >= 1) p0 -= 1; *ph = p0; return sn; }   /* (all sine: the slope is not run) */
        return sn + (mi_slope_tick(ph, s1, s2, inc, 0.5f) - sn) * sh;
    }
    if (sh < 2) return mi_slope_tick(ph, s1, s2, inc, 0.5f + 0.5f * (sh - 1));
    float p = *ph, y = 2 * p - 1 - blep(p, inc) * jump;
    const float k = sh < 3 ? sh - 2 : 1, pw = sh < 3 ? 0.5f : 0.5f - (sh - 3) * 0.42f;
    if (k > 0) { float q = p + pw; if (q >= 1) q -= 1; y -= k * (2 * q - 1 - blep(q, inc)); }
    p += inc; if (p >= 1) p -= 1; *ph = p;
    return y;
}
/* wob: WOBBLE's voice (its shapes, MIX and SUB on its own knobs; no noise, no SYNC, COLOR fixed at 30, LP 24; its
 * envelopes as the other models' are: attack and DECAY, GATE holding at the top; the sweep, wb0 and wbd a frame, on CUTOFF) */
/* In the RAM (CH_AUDIO_CODE): from the flash a note of it cost some 2000 us of a core's 5800 on the FM-1, four times the
 * old ANALOG, and two tracks of it with effects overran the block. What it calls every frame is forced inline (CH_SI) */
static CH_AUDIO_CODE CH_NOINLINE void va_run(ch_svoice *v, const sparams *P, float hz, float inc, float *yb, float *eb, unsigned n, int wob, float wb0, float wbd)
{
    const float age = wob ? 0.3f : P->ramd, drive = age;   /* COLOR: the age and the drive on one knob (they were two, AGE and DRIVE) */
    v->ry2[4] -= (float)n;
    if (v->ry2[4] <= 0) { v->ry2[4] = (0.2f + 0.5f * ch_randf(&v->rng)) * (float)CH_RATE; v->ry2[2] = ch_rands(&v->rng); v->ry2[3] = ch_rands(&v->rng); }
    v->ry2[0] += (v->ry2[2] - v->ry2[0]) * 0.006f; v->ry2[1] += (v->ry2[3] - v->ry2[1]) * 0.006f;
    const float cent = 0.0005776f * age;                  /* (a cent is 0.058 per cent) */
    const float ia = ch_min(inc * v->kc[0] * (1 + cent * (14 * v->ry2[0] + 9 * v->rb1[0])), 0.45f);
    const float ib = ch_min(inc * v->kc[1] * (1 + cent * (14 * v->ry2[1] + 9 * v->rb1[1])), 0.45f);
    {   /* the oscillators and the mixer */
        const float sa = ch_clamp(wob ? P->rnoise : P->harm, 0, 1) * 4, sb = ch_clamp(wob ? P->ramr : P->timbre, 0, 1) * 4, mo = ch_clamp(wob ? P->ramd : P->morph, 0, 1);
        const float la = ch_min(1.0f, 2 * (1 - mo)), lb = ch_min(1.0f, 2 * mo), ls = P->ramf, sg = ls * 1.25f, ln = wob ? 0 : P->rnoise * 0.6f;   /* (the sine a little up: it has less in it than the square had) */
        const int sync = !wob && P->wtrig > 0.5f;
        const float dg = 0.35f, bias = 0.1f * drive, ratio = ib / ia;   /* (DRIVE itself: at the ladder's input and after it, below) */
        float pa = v->ph[0], pb = v->ph[1], ps = v->ph[2], jump = v->ph[7];
        for (unsigned i = 0; i < n; ++i) {
            const float p0 = pa;
            float y = va_osc(sa, &pa, &v->ph[3], &v->ph[4], ia, 1) * la;
            if (sync && pa < p0) { jump = pb; pb = pa * ratio; pb -= (float)(int)pb; }
            if (lb > 0) { y += va_osc(sb, &pb, &v->ph[5], &v->ph[6], ib, jump) * lb; jump = 1; }
            if (ls > 0) { y += va_sin(ps) * sg; ps += ia * 0.5f; if (ps >= 1) ps -= 1; }   /* the SUB: a sine (it was a square, as a divider makes; a sine is weight alone, and COLOR puts the edge on) */
            if (ln > 0) y += ch_rands(&v->rng) * ln;
            yb[i] = va_clip(y * dg + bias);
        }
        v->ph[0] = pa; v->ph[1] = pb; v->ph[2] = ps; v->ph[7] = jump;
    }
    {   /* the ladder, and the two envelopes */
        const float ts = 1 + 0.15f * age * v->rb1[2];
        const float cA = 1 - ch_exp2(-1.0f / (ch_max(P->att > 0.005f ? ch_exp_map(P->att, 0.001f, 2.0f) : 0, 0.0015f) * (float)CH_RATE) * 1.44f * 4);
        const float cD = 1 - dcoef(ch_exp_map(P->dec, 0.02f, 8.0f) * ts), cR = wob ? cD : 1 - dcoef(ch_exp_map(P->rwin, 0.01f, 8.0f) * ts);
        const float fA = P->fatk > 0.005f ? 1 - ch_exp2(-1.0f / (ch_exp_map(P->fatk, 0.001f, 2.0f) * (float)CH_RATE) * 1.44f * 4) : 1;
        const float fD = 1 - dcoef(ch_exp_map(P->fdec, 0.02f, 8.0f) * ts), fR = wob ? fD : 1 - dcoef(ch_exp_map(P->rjrate, 0.01f, 8.0f) * ts);
        const float sus = P->mode == 0 ? 0 : wob ? 1 : P->lpg, fsus = P->mode == 0 || wob ? 0 : P->rjit;
        const int rel = P->mode == 1 && !v->gate, drone = P->mode == 2;
        const float k = 4.1f * P->res, comp = (1 + 0.4f * k) * (1 + 5 * drive), pg = 1 + 2 * drive, q_ = (1 - drive) * (1 - drive), mk = 0.66f + 1.14f * q_ * q_ * q_;   /* DRIVE: up to six times into the ladder's clip, three into one after it; the level made up so it rises 5 dB across the knob, not 12 */
        const float kt = v->kg[2];                        /* how far the keyboard is followed (voice_run: CUTOFF's mode) */
        const float cut = P->cut + 0.08f * age * v->rb1[1], pfe = P->fenv * 0.9f * (1 - 0.5f * P->vels * (1 - v->vel));
        float s1 = v->ry1[0], s2 = v->ry1[1], s3 = v->ry1[2], s4 = v->ry1[3], hs = v->ry1[4];
        float env = v->env, fenv = v->fenv; int st = v->stage, fst = v->fstage;
        float G = 0, G2 = 0, G3 = 0, om = 1, inv = 1;
        /* FILTER: what is heard of the ladder, a sum of its input and its four stages: the fourth stage (24 dB), the second (12 dB), a band (4 (y2 - 2 y3 + y4)), a high pass (u - 2 y1 + y2).
         * RES feeds back from the fourth stage whatever is heard, so every mode rings at CUTOFF */
        static const float FM[4][5] = {{0, 0, 0, 0, 1}, {0, 0, 1, 0, 0}, {0, 0, 4, -8, 4}, {1, -2, 1, 0, 0}};
        const float *fm = FM[wob ? 0u : (unsigned)(ch_clamp(P->ramr, 0, 1) * 3 + 0.5f) & 3u];
        const float m0 = fm[0], m1 = fm[1], m2 = fm[2], m3 = fm[3], m4 = fm[4];
        for (unsigned i = 0; i < n; ++i) {
            if ((i & (LPG_STRIDE - 1u)) == 0) {
                /* (ch_exp_map over 30 .. 16000 Hz and ch_svf_g, written out: called, they were trips to the flash eight times a block) */
                const float w = (CH_PI / (float)CH_RATE) * ch_clamp(30.0f * ch_exp2(ch_clamp(cut + wb0 + wbd * (float)i + pfe * fenv, 0, 1) * 9.0588936f) * kt, 10, 16000), w2 = w * w;
                const float g = w * (1 + w2 * (1.0f / 3 + w2 * (2.0f / 15 + w2 * (17.0f / 315))));
                G = g / (1 + g); G2 = G * G; G3 = G2 * G; om = 1 - G; inv = 1 / (1 + k * G2 * G2);
            }
            const float S = om * (G3 * s1 + G2 * s2 + G * s3 + s4);
            float x = va_clip((yb[i] * comp - k * S) * inv), t, o;
            o = m0 * x;
            t = (x - s1) * G; x = t + s1; s1 = x + t; o += m1 * x;
            t = (x - s2) * G; x = t + s2; s2 = x + t; o += m2 * x;
            t = (x - s3) * G; x = t + s3; s3 = x + t; o += m3 * x;
            t = (x - s4) * G; x = t + s4; s4 = x + t; x = o + m4 * x;
            hs += (x - hs) * 0.0026f;                      /* (18 Hz: what the bias and the ladder leave standing) */
            yb[i] = va_clip((x - hs) * pg) * mk;
            if (rel) fenv -= fenv * fR;
            else if (fst == 0) { fenv += (1.2f - fenv) * fA; if (fenv >= 1) { fenv = 1; fst = 1; } }
            else fenv += (fsus - fenv) * fD;
            if (drone) env += (1 - env) * 0.001f;
            else if (rel) env -= env * cR;
            else if (st == 0) { env += (1.2f - env) * cA; if (env >= 1) { env = 1; st = 1; } }
            else env += (sus - env) * cD;
            eb[i] = env;
        }
        v->ry1[0] = ch_ftz(s1); v->ry1[1] = ch_ftz(s2); v->ry1[2] = ch_ftz(s3); v->ry1[3] = ch_ftz(s4); v->ry1[4] = ch_ftz(hs);
        v->env = env; v->fenv = fenv; v->stage = (uint8_t)st; v->fstage = (uint8_t)fst;
    }
    v->n += n;
}

static void voice_run(ch_svoice *v, const sparams *P, float *out, unsigned n)
{
    const smodel_t *m = &SM[v->model];
    /* glide toward the note */
    float gk = P->glide > 0.01f ? 1 - ch_exp2(-(float)n / (ch_exp_map(P->glide, 0.005f, 1.0f) * (float)CH_RATE))
             : v->model == ACID ? 1 - ch_exp2(-(float)n / (0.06f * (float)CH_RATE)) : 1;   /* the 303's slide: 60 ms */
    v->hz += (v->hz_target - v->hz) * gk;
    float hz = ch_clamp(v->hz * ch_semis(P->pitch), 10, 16000), inc = ch_min(hz / (float)CH_RATE, 0.45f);
    float ca = 1 - ch_exp2(-1.0f / (ch_max(P->att > 0.005f ? ch_exp_map(P->att, 0.001f, 2.0f) : 0, 0.0015f) * (float)CH_RATE) * 1.44f * 4);   /* (1.5 ms at least: no step at the start) */
    float cd = dcoef(ch_exp_map(P->dec, 0.02f, 8.0f));
    float vg = v->model == ACID ? 1 : 1 - P->vels * (1 - v->vel * v->vel);   /* (the 303 has no velocity: its accent) */
    float gain = m->gain * P->lvl * vg;
    float pk = 0;
    int flt = P->cut < 0.995f || P->res > 0.005f || ch_abs(P->fenv) > 0.005f || v->model == WOBBLE;   /* (open and unmoved: not run, whatever KEY TRACK says: there is nothing to follow) */
    /* WOBBLE: the filter's CUTOFF swept down and back, RATE to the tempo (on the transport's own beat while it plays, so
     * every note and every track's wobble falls on the grid; by its own count when stopped), DEPTH how far down, SHAPE
     * the sweep's shape, morphed from a sine through a triangle and a saw to a square */
    float wb0 = 0, wbd = 0, wa0 = 1, wad = 0;             /* the sweep on CUTOFF (added to it) and on the level (times it), at the block's start, and a frame's step */
    const int wamp = v->model == WOBBLE && (int)(P->rwin * 2 + 0.5f) >= 1;   /* TARGET: AMP or BOTH */
    if (v->model == WOBBLE) {
        const int ri = (int)(ch_clamp(P->harm, 0, 1) * (WOB_RATES - 1) + 0.5f), sync = ri < WOB_SYNC, retrig = P->wtrig > 0.5f;
        const float bpc = sync ? WOB_BEATS[ri] : 1;
        const float dph = sync ? (float)n * ch_t.beats_per_frame / bpc : (float)n * WOB_HZ[ri - WOB_SYNC] * (1.0f / (float)CH_RATE);
        float p0, wf[2], wa[2];
        const float sx = (float)(int)(ch_clamp(P->morph, 0, 1) * 6 + 0.5f) * 0.5f;   /* SHAPE: 0 sine, 1 triangle, 2 saw, 3 square, and half way between each */
        const int k0 = sx >= 3 ? 2 : (int)sx; const float fr = sx - (float)k0;
        /* DEPTH, both ways. Up from the middle: the sweep pulls down (CUTOFF is its top; the level dips). Down from it: it
         * pushes up (CUTOFF is its floor, the filter opens toward the top; the level dips the other half of the sweep).
         * FADE IN: the depth rises from nothing over up to four seconds of the note */
        float d = (float)((int)(ch_clamp(P->timbre, 0, 1) * 200 + 0.5f) - 100) * 0.01f;
        if (P->rjit > 0.005f) d *= ch_min(1.0f, ((float)v->n + 0.5f * (float)n) / (P->rjit * P->rjit * 4.0f * (float)CH_RATE));
        const int tg = (int)(P->rwin * 2 + 0.5f);          /* TARGET: 0 the filter, 1 the level, 2 both */
        const float off = (float)(int)(ch_clamp(P->rjrate, 0, 1) * 23 + 0.5f) * (1.0f / 24);   /* OFFSET, turns */
        /* where the sweep is. TRIG FREE, a synced RATE, the sequencer playing: on the transport's own beat, so every note's
         * wobble falls on the grid. Otherwise by the voice's own count: from the top at each note (RETRIG), or running on */
        if (retrig && v->n == 0) v->fb = 0;
        if (!retrig && sync && ch_t.playing) p0 = (float)ch_t.beat / bpc;   /* (wrapped below; in single precision, as the chip has it) */
        else p0 = v->fb;
        p0 = wrap1(p0); v->fb = wrap1(p0 + dph);
        for (unsigned k = 0; k < 2; ++k) {
            float p = wrap1(p0 + off + (k ? dph : 0)), a = wob_shape(k0, p);
            a += (wob_shape(k0 + 1, p) - a) * fr;           /* 1 open .. 0 shut */
            if (P->wbi) {                                /* BIPOLAR: the sweep swings both ways round CUTOFF (and the level round its own), half of DEPTH each way; under 0 it starts the other way */
                wf[k] = tg == 1 ? 0 : d * 0.5f * (2 * a - 1) * ch_min(P->cut, 1 - P->cut) * 2;
                wa[k] = tg == 0 ? 1 : 1 + d * 0.5f * (2 * a - 1);
                continue;
            }
            wf[k] = tg == 1 ? 0 : d >= 0 ? -d * P->cut * (1 - a) : -d * (1 - P->cut) * (1 - a);
            wa[k] = tg == 0 ? 1 : d >= 0 ? 1 - d * (1 - a) : 1 + d * a;
        }
        wb0 = wf[0]; wbd = (wf[1] - wf[0]) / (float)n;
        wa0 = wa[0]; wad = (wa[1] - wa[0]) / (float)n;
    }
    float cf = dcoef(ch_exp_map(P->fdec, 0.02f, 8.0f));
    float cfa = P->fatk > 0.005f ? 1 - ch_exp2(-1.0f / (ch_exp_map(P->fatk, 0.001f, 2.0f) * (float)CH_RATE) * 1.44f * 4) : 1;
    float yb[CH_SUB], eb[CH_SUB];                        /* the model's sound, and the envelope, a frame each */
    /* PING and a step's GATE. A PING rings out at DECAY whatever becomes of its note, so a step's GATE had nothing to
     * shorten. Now: when its note is let go, the lane that played it is looked up (the voice is in its track: which one
     * is where it lies in ch_tr; the lane is the one on its note, in any octave), and if that step's GATE is under the
     * whole step the voice is stopped there (the 8 ms fade every stopped voice has). At the whole step, as it was: it
     * rings. Judged once a note (stage 2: let go, left to ring) */
    if (P->mode == 0 && !v->gate && !v->kill && v->stage <= 1 && v->model != ACID) {
        const unsigned ti = (unsigned)(((const char *)v - (const char *)ch_tr) / sizeof(ch_trk));
        if (ti < CH_TRACKS) {
            const ch_seq_state *q = ch_seq_get(ti); const ch_seq *sq = &ch_tr[ti].seq;
            int best = -1;                                 /* (of the lanes on its note: the one let go last, which is the one that played it) */
            for (unsigned l = 0; l < CH_LANES; ++l)
                if ((q->note[l] + 120u - v->note) % 12u == 0 && !sq->ln[l].gate_note && (best < 0 || sq->ln[l].gate_off > sq->ln[best].gate_off)) best = (int)l;
            if (best >= 0 && sq->ln[best].gate < 97u) v->kill = 1;
        }
        if (!v->kill && v->stage == 1) v->stage = 2;
    }
    model_block(v, P, hz, inc, n);
    if (v->model != ACID) {                              /* KEY TRACK, CUTOFF's mode (ch_synth_modes): the cutoff times (note / C4) to the power of none, a half or one */
        const unsigned ti = (unsigned)(((const char *)v - (const char *)ch_tr) / sizeof(ch_trk));
        unsigned km = ti < CH_TRACKS ? ch_tr[ti].mode[CH_DEV_SYNTH][S_CUTOFF] : 0;
        const int va = v->model == VA || v->model == WOBBLE;
        const float a = km > 2 ? 0 : va ? (km == 0 ? 0.5f : km == 1 ? 1.0f : 0.0f) : (float)km * 0.5f;
        const float r = ch_clamp(hz, 20, 8000) * (1.0f / 261.63f);
        v->kg[2] = a >= 1 ? r : a > 0 ? ch_sqrt_fast(r) : 1;
    }
    if (v->model == VA || v->model == WOBBLE) va_run(v, P, hz, inc, yb, eb, n, v->model == WOBBLE, wb0, wbd);   /* ANALOG, and WOBBLE on its voice: the oscillators, mixer, ladder and envelopes are its own */
    else for (unsigned i = 0; i < n; ++i) {
        /* the envelope: PING attack then decay; GATE holds; DRONE stays up */
        if (v->model == ACID) { }                        /* its VCA envelope is acid_tick's */
        else if (P->mode == 2) v->env += (1 - v->env) * 0.001f;
        else if (v->stage == 0) { v->env += (1.05f - v->env) * ca; if (v->env >= 1) { v->env = 1; v->stage = 1; } }
        else if (P->mode == 1 && v->gate) { }
        else v->env *= cd;
        yb[i] = model_tick(v, P, hz, inc);
        eb[i] = v->env;
        ++v->n;
    }
    /* The rest in passes over the block, each with its few values in locals (the FM-1: memory is what costs, and one
     * loop holding everything read its own stack all the time). The same sums in the same order */
    /* the filter: CUTOFF, RESO, and ENV>FILT of its own envelope (ACID's FILT DEC sets that; the others fall with
     * DECAY). Nothing is done while it is open and unmoved. ACID's is its own ladder, in acid_tick */
    if (flt && v->model != ACID && v->model != VA && v->model != WOBBLE) {
        ch_svf_t f2 = v->f[2];
        float fenv = v->fenv, fg = v->flt_g; int fst = v->fstage;
        const float kq = 2.0f - P->res * 1.85f, cut = P->cut, pfe = P->fenv;
        for (unsigned i = 0; i < n; ++i) {
            if ((i & (LPG_STRIDE - 1u)) == 0) fg = ch_svf_g(ch_min(ch_exp_map(ch_clamp(cut + wb0 + wbd * (float)i + pfe * fenv * 0.9f, 0, 1), 30, 16000) * v->kg[2], 16000));
            yb[i] = ch_svf_tick(&f2, yb[i], fg, kq).lp;
            if (fst == 0) { fenv += (1.05f - fenv) * cfa; if (fenv >= 1) { fenv = 1; fst = 1; } }
            else fenv *= cf;
        }
        v->f[2] = f2; v->fenv = fenv; v->flt_g = fg; v->fstage = (uint8_t)fst;
    }
    if (wamp) for (unsigned i = 0; i < n; ++i) yb[i] *= wa0 + wad * (float)i;   /* WOBBLE on the level */
    /* the low-pass gate: with LPG a filter that closes as the envelope fades (it follows it LPG_STRIDE samples at a time) */
    if (P->lpg > 0.01f && v->model != VA) {   /* (ANALOG: that knob is its SUSTAIN) */
        ch_svf_t f3 = v->f[3];
        float lg = v->lpg_g; const float la = 0.3f + 0.7f * (1 - P->lpg), lb = (1 - P->lpg) * 0.5f;
        for (unsigned i = 0; i < n; ++i) {
            if ((i & (LPG_STRIDE - 1u)) == 0) lg = ch_svf_g(ch_exp_map(ch_clamp(eb[i] * la + lb, 0, 1), 80, 18000));
            yb[i] = ch_svf_tick(&f3, yb[i], lg, 1.3f).lp;
        }
        v->f[3] = f3; v->lpg_g = lg;
    }
    {   /* the VCA (ACID's is inside acid_tick, before its overdrive), the fade of a stopped voice, the hand-over */
        const int acid = v->model == ACID;
        const float kstep = v->kill ? (v->kill == 2 ? CH_KILL_STEP * (CH_KILL_S / 0.003f) : CH_KILL_STEP) : 0;   /* stopped: out over 8 ms on an S curve (taken: 3) */
        float fade = v->fade, dcp = v->dcp, last = v->last; const float dc = v->dc;
        if (kstep == 0 && dcp <= 0) {                    /* the usual case: no fade running, nothing handed over */
            const float fg = ch_fade_gain(fade);
            if (acid) for (unsigned i = 0; i < n; ++i) { float y = yb[i] * (gain * fg); out[i] += y; last = y; pk = ch_max(pk, ch_abs(y)); }
            else for (unsigned i = 0; i < n; ++i) { float y = yb[i] * (eb[i] * gain * fg); out[i] += y; last = y; pk = ch_max(pk, ch_abs(y)); }
        } else
        for (unsigned i = 0; i < n; ++i) {
            float y = yb[i] * ((acid ? 1.0f : eb[i]) * gain * ch_fade_gain(fade));
            if (kstep > 0) fade = ch_max(fade - kstep, 0);
            if (dcp > 0) { y += dc * ch_fade_gain(dcp); dcp -= 1.0f / (0.003f * (float)CH_RATE); }
            out[i] += y; last = y;
            pk = ch_max(pk, ch_abs(y));
        }
        v->fade = fade; v->dcp = dcp; v->last = last;
    }
    if (P->mode != 2 && v->stage >= 1 && v->env < 1e-4f && pk < 3e-5f) v->on = 0;
    /* a note let go (or a PING decaying) whose sound is under -66 dB for 23 ms is let go, as a drum's tail is */
    if (!v->kill && P->mode != 2 && v->stage >= 1 && !(P->mode == 1 && v->gate) && pk < CH_TAIL_END && v->env < 0.02f) {   /* (its envelope down too: a filter still opening is not a tail) */ if (++v->quiet >= CH_TAIL_SUBS) v->kill = 1; } else v->quiet = 0;
    if (v->kill && v->fade <= 0) v->on = 0;
    v->env = ch_ftz(v->env); v->fenv = ch_ftz(v->fenv);
}

CH_AUDIO_CODE void ch_synth_process(ch_trk *t, unsigned ti, float *L, float *R, unsigned n)
{
    (void)ti;
    ch_synth *s = &t->synth;
    sparams P;
    P.pitch = PV(t, CH_MATERIAL, S_PITCH) * 24;
    if (PMODE(t, CH_DEV_SYNTH, S_PITCH) == 0) P.pitch = (float)(int)(P.pitch + (P.pitch < 0 ? -0.5f : 0.5f));
    P.harm = PV(t, CH_MATERIAL, S_HARM); P.timbre = PV(t, CH_MATERIAL, S_TIMBRE); P.morph = PV(t, CH_MATERIAL, S_MORPH);
    P.dec = PV(t, CH_MATERIAL, S_DECAY); P.lpg = PV(t, CH_MATERIAL, S_LPG);
    P.mode = (unsigned)(PV(t, CH_MATERIAL, S_MODE) + 0.5f);
    P.wbi = PMODE(t, CH_DEV_SYNTH, S_TIMBRE) != 0;      /* (WOBBLE's DEPTH: its mode) */
    P.att = PV(t, CH_MATERIAL, S_ATTACK); P.glide = PV(t, CH_MATERIAL, S_GLIDE);
    P.lvl = ch_level_gain(PV(t, CH_MATERIAL, S_LEVEL)); P.vels = PV(t, CH_MATERIAL, S_VEL);
    P.cut = PV(t, CH_MATERIAL, S_CUTOFF); P.res = PV(t, CH_MATERIAL, S_RESO); P.fenv = PV(t, CH_MATERIAL, S_FENV);
    P.fatk = PV(t, CH_MATERIAL, S_FATK); P.fdec = PX(t, S_FDEC);
    P.aratio = ch_semis((PX(t, S_AOCT) - 2) * 12 + PX(t, S_ASEMI) - 12 + PX(t, S_AFINE) * 0.5f);   /* the oscillators' tuning: octave, semitone, +-50 cents */
    P.bratio = ch_semis((PX(t, S_BOCT) - 2) * 12 + PX(t, S_BSEMI) - 12 + PX(t, S_BFINE) * 0.5f);
    P.rnoise = PX(t, S_NOISE); P.rwin = PX(t, S_WIN); P.ramr = PX(t, S_AMR); P.ramd = PX(t, S_AMD); P.ramf = PX(t, S_AMF);
    P.rjit = PX(t, S_JIT); P.rjrate = PX(t, S_JRATE); P.wtrig = PX(t, S_WTRIG);
    unsigned cur_model = (unsigned)(PV(t, CH_MATERIAL, S_MODEL) + 0.5f);
    if (cur_model >= FE_BASE && P.mode == 2) P.mode = 1;     /* no DRONE for Felucca's engines */
    if (P.mode == 2 && !s->v[s->cur].on) {               /* DRONE sounds without a note */
        ch_svoice *v = &s->v[s->cur];
        v->on = 1; v->env = 0; v->stage = 1; v->note = 48; v->vel = 1; v->kill = 0; v->fade = 1;
        v->model = (uint8_t)(PV(t, CH_MATERIAL, S_MODEL) + 0.5f);
        v->hz = v->hz_target = 130.81f; v->fenv = 0;
    }
    if (P.mode == 2) s->v[s->cur].model = (uint8_t)ch_min(PV(t, CH_MATERIAL, S_MODEL) + 0.5f, CH_SYNTH_MODELS - 1);
    s->c13 *= ch_exp2(-1.4427f * (float)n / (0.4f * (float)CH_RATE));   /* ACID's accent sweep charge: gone in half a second or so */
    float buf[CH_SUB];
    for (unsigned i = 0; i < n; ++i) buf[i] = 0;
    for (unsigned k = 0; k < CH_SVOICES; ++k) {
        ch_svoice *v = &s->v[k];
        if (!v->on) continue;
        if (v->model >= FE_BASE) {
            float knob[3] = {P.harm, P.timbre, P.morph}, tmp[CH_SUB];
            for (unsigned i = 0; i < n; ++i) tmp[i] = 0;
            ch_fe_render(&v->fe, v->model - FE_BASE, 261.63f * ch_semis((float)v->note - 60 + P.pitch + 12 * SM[v->model].oct), knob, tmp, n);
            float g = P.lvl * SM[v->model].gain * (1 - P.vels * (1 - v->vel * v->vel));   /* VELOCITY, as ours */
            /* our ATTACK / DECAY envelope (PING: it decays at once; GATE: it holds while the key does) and the
             * low-pass gate, over the engine's own voice, as for our models */
            float ca = 1 - ch_exp2(-1.0f / (ch_max(P.att > 0.005f ? ch_exp_map(P.att, 0.001f, 2.0f) : 0, 0.0015f) * (float)CH_RATE) * 1.44f * 4);
            float cd = dcoef(ch_exp_map(P.dec, 0.02f, 8.0f));
            float pk = 0;
            for (unsigned i = 0; i < n; ++i) {
                if (v->stage == 0) { v->env += (1.05f - v->env) * ca; if (v->env >= 1) { v->env = 1; v->stage = 1; } }
                else if (P.mode == 1 && v->gate) { }
                else v->env *= cd;
                float y = tmp[i], e = v->env;
                if (P.lpg > 0.01f) {
                    if ((i & (LPG_STRIDE - 1u)) == 0) {
                        float fc = ch_exp_map(ch_clamp(e * (0.3f + 0.7f * (1 - P.lpg)) + (1 - P.lpg) * 0.5f, 0, 1), 80, 18000);
                        v->lpg_g = ch_svf_g(fc);
                    }
                    y = ch_svf_tick(&v->f[3], y, v->lpg_g, 1.3f).lp;
                }
                y *= e * g * ch_fade_gain(v->fade);
                if (v->kill) v->fade = ch_max(v->fade - (v->kill == 2 ? CH_KILL_STEP * (CH_KILL_S / 0.003f) : CH_KILL_STEP), 0);
                if (v->dcp > 0) { y += v->dc * ch_fade_gain(v->dcp); v->dcp -= 1.0f / (0.003f * (float)CH_RATE); }
                buf[i] += y; pk = ch_max(pk, ch_abs(y));
            }
            if (v->kill && v->fade <= 0) { v->on = 0; continue; }
            v->env = ch_ftz(v->env);
            v->n += n;
            if (!ch_fe_active(&v->fe)) v->on = 0;
            if (v->stage == 1 && !(P.mode == 1 && v->gate) && v->env < 1e-4f && pk < 3e-5f) v->on = 0;
            continue;
        }
        if (P.mode == 2 && k != s->cur && !v->kill) v->kill = 1;   /* DRONE: only the current voice sounds; another fades out (8 ms) */
        voice_run(v, &P, buf, n);
    }
    for (unsigned i = 0; i < n; ++i) { L[i] += buf[i]; R[i] += buf[i]; }
    for (unsigned k = 0; k < CH_SVOICES; ++k) {          /* a note waiting for its voice to fade out: now it is free */
        ch_svoice *v = &s->v[k];
        if (v->pend_vel && !v->on) { unsigned nn = v->pend_note, vv = v->pend_vel; v->pend_vel = 0; ch_synth_note(t, ti, nn, vv); }
    }
}
