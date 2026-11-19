/* The effects: ten plain ones, each doing one thing, any of them in any of a track's four FX slots (the same one in
 * more than one slot too), run in slot order 1 -> 4.
 *   FILTER    CUTOFF RESONANCE TYPE SLOPE           a state-variable filter: low, high, band pass, notch; 12 or 24 dB
 *   DRIVE     DRIVE TONE LEVEL MIX                  soft saturation, a tilt after it
 *   CHORUS    RATE DEPTH MIX                        one swept voice a side, the sides a quarter turn apart
 *   UNISON    DETUNE VOICES SPREAD MIX              2, 4 or 6 copies swaying off pitch, across the two sides
 *   FLANGER   RATE DEPTH FEEDBACK MIX               a short swept delay into itself
 *   PHASER    RATE DEPTH FEEDBACK MIX               six swept all-pass stages
 *   TREMOLO   RATE DEPTH SHAPE STEREO               level wobble; STEREO turns it into auto-pan
 *   DELAY     TIME FEEDBACK MIX TONE | PING PONG    stereo delay, TIME synced (a note value) or FREE (ms): its mode
 *                                                   (at half rate: 0.74 s at most)
 *   ECHO      TIME FEEDBACK MIX WOW | TONE          a tape echo: free time, wow and flutter, darker and saturating
 *                                                   repeats (at a quarter rate: 1.49 s at most)
 *   DUB DELAY TIME FEEDBACK MIX CUTOFF | SWEEP      tempo-synced, a resonant band-pass in its feedback that sweeps
 *                                                   on its own (SWEEP), and moves under a modulator on CUTOFF; the
 *                                                   feedback saturates and runs away past 100 % (a quarter rate)
 *   REVERB    SIZE DECAY DAMP MIX                   a four-line feedback delay network (at half rate)
 *   DJ FILTER FILTER RESONANCE                      one knob: left a low pass closing, right a high pass rising
 *   SIDECHAIN SOURCE AMOUNT ATTACK RELEASE | SOURCE TOO   ducks on the chosen track's hits: the pump. On the master
 *                                                   the source track passes untouched unless SOURCE TOO is on
 *   TAPE      WOW SATURATE TONE HISS | MIX            a tape's wow and flutter (a swept short delay), its saturation,
 *                                                   a high roll-off and a little hiss
 *   VINYL     CRACKLE POPS DUST TONE | MIX           a record's surface: crackle, the odd pop, a dust bed, their tone
 *   SPRING    TENSION DECAY TONE MIX                a spring: a chain of all-passes (the dispersion, the boing) in a
 *                                                   short feedback loop, at half rate
 *   REPEAT    WINDOW EVERY REPEATS GATE | CHANCE DECAY MIX HOLD | OFFSET   a beat repeater: at the start of every EVERY (a beat to
 *                                                   four bars) a WINDOW of the input is taken and then played REPEATS times,
 *                                                   each for GATE of its length, fading by DECAY; CHANCE: how often a
 *                                                   start is taken; HOLD keeps the window. 4 line blocks
 *   RESONATOR PITCH RESONANCE SPREAD MIX            four tuned bands ringing at a note (in the master key when one is
 *                                                   on), harmonic or stretched (SPREAD), on the mono sum
 * An effect with a MIX at 0 does nothing, and like a bypassed one holds no line memory: turned up, it takes memory
 * again (cleared a block a call: a moment dry). So the master's idle DELAY and REVERB cost the tracks nothing.
 * The ones with delay lines borrow blocks of the line memory (CHORUS, FLANGER 1; ECHO, REVERB 4; DELAY 8). With
 * none left the effect passes the sound dry and the screen says NO MEMORY. Memory is taken and given back on the
 * first core only (ch_fx_prepare, ch_fx_release), before the render; the effects themselves only use it. Taken
 * memory is cleared one block a render call (a 64 KB clear inside the audio interrupt would miss the deadline),
 * the effect passing the sound dry until it is clean.
 * Cost: nothing per sample that only changes with a knob (rates, coefficients, LFO points: once a sub-block). */
#include "ch_internal.h"

static union { int16_t m[CH_FX_BLOCKS][CH_FX_BLOCK]; float al; } fx_pool CH_POOL;   /* (a float's alignment: DISPERSE keeps its filters' memories in its block) */
#define fx_mem fx_pool.m
static uint8_t fx_owner[CH_FX_BLOCKS];                  /* 0 free, else 1 + track * CH_FX + place */

void ch_mem_init(void)
{
    for (unsigned i = 0; i < CH_FX_BLOCKS; ++i) fx_owner[i] = 0;
    for (unsigned t = 0; t < CH_TRACKS; ++t) {
        for (unsigned k = 0; k < CH_FX; ++k) { ch_tr[t].fx[k].mem = 0; ch_tr[t].fx[k].blocks = 0; }
    }
}


/* DELAY's TIME: tempo divisions, in beats, or free (its mode: SYNC, FREE) */
static const float DLY_BEATS[CH_DELAY_DIVS] = {0.125f, 1.0f / 6, 0.25f, 1.0f / 3, 0.375f, 0.5f, 2.0f / 3, 0.75f, 1.0f, 1.5f};
static const char *const DLY_NAME[CH_DELAY_DIVS] = {"1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D", "1/4", "1/4D"};
const char *ch_delay_div_name(unsigned i) { return i < CH_DELAY_DIVS ? DLY_NAME[i] : ""; }
static unsigned dly_div(float v) { unsigned i = (unsigned)(ch_clamp(v, 0, 1) * ((float)CH_DELAY_DIVS - 0.001f)); return i < CH_DELAY_DIVS ? i : CH_DELAY_DIVS - 1; }
static unsigned fx_mode(const ch_trk *t, unsigned s, unsigned p) { return t->mode[ch_row(s, t->device[s])][p]; }
/* the delay time in line frames at `rate`: synced to the tempo, or free, 20 ms .. maxs */
static float dly_frames(const ch_trk *t, unsigned s, unsigned p, float rate, float maxs)
{
    float v = PV(t, s, p);
    float sec = fx_mode(t, s, p) ? ch_exp_map(v, 0.02f, maxs) : DLY_BEATS[dly_div(v)] * 60.0f / (ch_t.bpm > 1 ? ch_t.bpm : 120);
    return ch_min(sec, maxs) * rate;
}

/* ---- parameters (ch_params.c lays out the same) ---- */
enum { F_CUT, F_RES, F_TYPE, F_SLOPE };
enum { D_DRIVE, D_TONE, D_LEVEL, D_MIX, D_TYPE, D_CHAR, D_CLIP, D_COMP };
enum { C_RATE, C_DEPTH, C_MIX, C_SHAPE };
enum { L_RATE, L_DEPTH, L_FB, L_MIX, L_SHAPE };          /* FLANGER, PHASER */
enum { T_RATE, T_DEPTH, T_SHAPE, T_STEREO };
enum { Y_TIME, Y_FB, Y_MIX, Y_TONE, Y_PING };
enum { E_TIME, E_FB, E_MIX, E_WOW, E_TONE };
enum { R_SIZE, R_DECAY, R_DAMP, R_MIX, R_DUCK };
enum { U_TIME, U_FB, U_MIX, U_CUT, U_SWEEP, U_FTYPE, U_RES };
enum { J_AMOUNT, J_RES };
enum { S_SRC, S_AMT, S_ATT, S_REL, S_ALL };
enum { Z_PITCH, Z_RES, Z_SPREAD, Z_MIX };
enum { P_WOW, P_SAT, P_TONE, P_HISS, P_MIX };
enum { V_CRACK, V_POPS, V_DUST, V_TONE, V_MIX };
enum { W_TENSION, W_DECAY, W_TONE, W_MIX };
enum { M_DIV, M_EVERY, M_LENGTH, M_GATE, M_CHANCE, M_DECAY, M_MIX, M_HOLD, M_OFFSET };
enum { O_FREQ, O_WIDTH, O_AMT, O_MODE };
enum { H_FREQ, H_PINCH, H_AMT, H_MIX, H_MAX = 16 };
/* the MIX parameter of each effect, -1 none: at 0 the effect is skipped */
/* a master effect as a send's return (set by the master chain round its call): the sends' sum, to go into the effect
 * whole (0: none), and MIX applied to the mix on its way in, not to what comes out */
const float *ch_fx_el_g, *ch_fx_er_g; uint8_t ch_fx_pre_g;
/* A master effect as a send's return: what the master chain sets for its own effects. They are read through the
 * track in hand (t, in every effect here), and are nothing for any track but the master: the master chain runs
 * while the second core's tracks do, and a track's CHORUS must not find the master's flag up */
#define ch_fx_pre (t == &ch_mst && ch_fx_pre_g)
#define ch_fx_el (t == &ch_mst ? ch_fx_el_g : (const float *)0)
#define ch_fx_er (t == &ch_mst ? ch_fx_er_g : (const float *)0)
uint8_t ch_fx_sent;   /* sends are in use (a track's FX SEND up, or a step with a SEND, since the start) */
uint8_t ch_fx_retmask;   /* the master's slots that work as returns (a bit a slot: a send can reach it; set once, by the master chain). The others are as they ever were. A return keeps its lines with MIX at 0 */
static int fx_is_return(unsigned d)
{
    return d == CH_DEV_DELAY || d == CH_DEV_ECHO || d == CH_DEV_DUB || d == CH_DEV_REVERB || d == CH_DEV_SPRING || d == CH_DEV_CRYSTAL
        || d == CH_DEV_UNISON || d == CH_DEV_CHORUS || d == CH_DEV_FLANGER || d == CH_DEV_PHASER || d == CH_DEV_COMB || d == CH_DEV_RESO;
}
/* the effects that treat the two sides alike, so that one sound on both can be worked once and copied (track_sub looks
 * whether it is, and sets f->mono): FILTER, DRIVE, DISPERSE, DJ FILTER, and COMB while its SPREAD is at nothing */
int ch_fx_mono_ok(const ch_trk *t, unsigned s)
{
    switch (t->device[s]) {
    case CH_DEV_FILTER: case CH_DEV_DRIVE: case CH_DEV_DISPERSE: case CH_DEV_DJ: return 1;
    case CH_DEV_COMB: return PV(t, s, 6) < 0.0005f;       /* (K_SPREAD) */
    default: return 0;
    }
}
static int mix_param(unsigned dev)
{
    switch (dev) {
    case CH_DEV_DRIVE: return D_MIX; case CH_DEV_CHORUS: return C_MIX;
    case CH_DEV_FLANGER: case CH_DEV_PHASER: return L_MIX; case CH_DEV_DELAY: return Y_MIX; case CH_DEV_ECHO: return E_MIX;
    case CH_DEV_DUB: return U_MIX; case CH_DEV_REVERB: return R_MIX; case CH_DEV_SIDE: return S_AMT; case CH_DEV_RESO: return Z_MIX;
    case CH_DEV_COMB: return 3; case CH_DEV_TAPE_FX: return P_MIX; case CH_DEV_VINYL: return V_MIX; case CH_DEV_SPRING: return W_MIX; case CH_DEV_REPEAT: return M_MIX;
    case CH_DEV_CRYSTAL: return 3; case CH_DEV_TRANCE: return 3; case CH_DEV_ERODE: return O_AMT; case CH_DEV_DISPERSE: return H_MIX; case CH_DEV_UNISON: return 3;
    default: return -1;
    }
}

static unsigned blocks_of(unsigned dev)
{
    switch (dev) {
    case CH_DEV_CHORUS: case CH_DEV_FLANGER: case CH_DEV_TAPE_FX: case CH_DEV_SPRING: case CH_DEV_COMB: case CH_DEV_ERODE: case CH_DEV_DISPERSE: case CH_DEV_UNISON: return 1;
    case CH_DEV_ECHO: case CH_DEV_DUB: case CH_DEV_REVERB: case CH_DEV_REPEAT: case CH_DEV_CRYSTAL: return 4;
    case CH_DEV_DELAY: return 8;
    default: return 0;
    }
}

unsigned ch_fx_blocks_used(void) { unsigned n = 0; for (unsigned i = 0; i < CH_FX_BLOCKS; ++i) n += fx_owner[i] != 0; return n; }
unsigned ch_fx_blocks_held(unsigned ti) { unsigned n = 0; for (unsigned i = 0; i < CH_FX_BLOCKS; ++i) if (fx_owner[i] && (fx_owner[i] - 1u) / CH_FX == ti) ++n; return n; }
/* the effect's state cleared (its memory kept, and silenced) */
void ch_fx_reset(ch_trk *t, unsigned k)
{
    ch_fx *f = &t->fx[k];
    int16_t *mem = f->mem; uint8_t b = f->blocks, nm = f->nomem;
    memset(f, 0, sizeof *f);
    f->mem = mem; f->blocks = b; f->nomem = nm;
    f->unclean = b;                                      /* (cleared a block a call, dry meanwhile) */
}
void ch_fx_release(ch_trk *t, unsigned ti, unsigned k)
{
    uint8_t id = (uint8_t)(1 + ti * CH_FX + k);
    for (unsigned i = 0; i < CH_FX_BLOCKS; ++i) if (fx_owner[i] == id) fx_owner[i] = 0;
    t->fx[k].mem = 0; t->fx[k].blocks = 0; t->fx[k].nomem = 0; t->fx[k].unclean = 0;
}
/* the slot's memory: taken if its effect needs lines and has none (the first free run of blocks), cleared */
void ch_fx_prepare(ch_trk *t, unsigned ti, unsigned k)
{
    ch_fx *f = &t->fx[k];
    unsigned dev = t->device[CH_FX1 + k], need = t->bypass[CH_FX1 + k] ? 0 : blocks_of(dev);
    { int mp = mix_param(dev); if (need && mp >= 0 && t->v[CH_FX1 + k][mp].l < 0.001f && t->base[ch_row(CH_FX1 + k, dev)][mp] < 0.001f && !(ti == CH_MASTER && (ch_fx_retmask >> k & 1u) && fx_is_return(dev))) need = 0; }
    if (f->mem && f->blocks != need) ch_fx_release(t, ti, k);
    if (f->mem && f->unclean) {                          /* one block of a fresh line cleared each call */
        memset(f->mem + (size_t)(f->blocks - f->unclean) * CH_FX_BLOCK, 0, CH_FX_BLOCK * sizeof(int16_t));
        --f->unclean;
    }
    if (!need || f->mem) return;
    for (unsigned i = 0; i + need <= CH_FX_BLOCKS; ++i) {
        unsigned j = 0;
        while (j < need && !fx_owner[i + j]) ++j;
        if (j < need) { i += j; continue; }
        for (j = 0; j < need; ++j) fx_owner[i + j] = (uint8_t)(1 + ti * CH_FX + k);
        f->mem = fx_mem[i]; f->blocks = (uint8_t)need; f->nomem = 0; f->unclean = (uint8_t)need;
        return;
    }
    f->nomem = 1;
}

/* ---- small parts ---- */
static inline int16_t s16(float x) { x = ch_clamp(x, -1, 1) * 32767.0f; return (int16_t)(x + (x < 0 ? -0.5f : 0.5f)); }
static inline float f16(int16_t v) { return (float)v * (1.0f / 32768.0f); }
/* a soft clip that reaches 1 at 3 (the tanh's Pade form) */
static inline float sat(float x) { x = ch_clamp(x, -3, 3); return x * (27 + x * x) / (27 + 9 * x * x); }
/* a line read `d` frames back (fractional), `mask` its power-of-two length less one */
static inline float tap(const int16_t *b, uint32_t mask, uint32_t w, float d)
{
    int32_t di = (int32_t)d; float fr = d - (float)di;
    uint32_t i0 = (w - (uint32_t)di) & mask, i1 = (i0 - 1u) & mask;
    float a = f16(b[i0]);
    return a + (f16(b[i1]) - a) * fr;
}
/* an LFO's value at the start and end of the sub-block (sine, or triangle, or a soft square), then advanced */
static float lfo_at(float ph, unsigned shape)
{
    ph -= (float)(int)ph;
    if (shape == 1) { float x = ph - (float)(int)ph; return x < 0.5f ? 4 * x - 1 : 3 - 4 * x; }
    float s = ch_sin_turns(ph);
    if (shape == 2) return ch_clamp(s * 6, -1, 1);
    return s;
}
static inline float wrap1(float x) { x -= (float)(int)x; return x < 0 ? x + 1 : x; }
/* CHORUS, FLANGER and PHASER's SHAPE: SINE TRIANGLE SAW UP SAW DOWN SQUARE RANDOM. x is the phase (it may run past 1:
 * the second side's quarter turn); cyc counts the effect's whole cycles (in ph2), so RANDOM holds a value a cycle, the same on
 * every read of that cycle, and moves on when it turns over. Between blocks the sweep is drawn as a line, so the edges of SAW
 * and SQUARE take a block (under 6 ms) rather than a click. */
static float fx_wave(float x, unsigned shape, float cyc)
{
    float fl = (float)(int)x; x -= fl;
    switch (shape) {
    case 1: return x < 0.5f ? 4 * x - 1 : 3 - 4 * x;
    case 2: return 2 * x - 1;
    case 3: return 1 - 2 * x;
    case 4: return x < 0.5f ? 1.0f : -1.0f;
    case 5: {
        uint32_t h = (uint32_t)(cyc + fl) * 2654435761u + 0x9E3779B9u;
        h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12;
        return (float)(h & 0xFFFFu) * (2.0f / 65535.0f) - 1;
    }
    default: return ch_sin_turns(x);
    }
}
/* the next block's phase, and its cycle count (ph2) moved on when the phase turned over */
static float fx_turn(ch_fx *f, float rate, unsigned n, float *c0, float *c1)
{
    float ph1 = wrap1(f->ph + rate * (float)n * (1.0f / (float)CH_RATE));
    *c0 = f->ph2; *c1 = ph1 < f->ph ? f->ph2 + 1 : f->ph2;
    if (*c1 > 1e6f) *c1 = 0;
    f->ph2 = *c1;
    return ph1;
}

/* ---- FILTER ---- */
static void fx_filter(ch_fx *f, ch_trk *t, unsigned s, float *L, float *R, unsigned n)
{
    float g = ch_svf_g(ch_exp_map(PV(t, s, F_CUT), 20, 18000));
    float k = 2 - 1.92f * ch_clamp(PV(t, s, F_RES), 0, 1);
    unsigned type = (unsigned)(PV(t, s, F_TYPE) + 0.5f), two = PV(t, s, F_SLOPE) > 0.5f;
    float *ch[2] = {L, R};
    unsigned nch = f->mono ? 1u : 2u;                     /* the same sound both sides (a drum, a synth: one voice path): once, and copied */
    for (unsigned c = 0; c < nch; ++c) {
        float *x = ch[c];
        for (unsigned i = 0; i < n; ++i) {
            float y = x[i];
            for (unsigned st = 0; st <= two; ++st) {
                ch_svf_out_t o = ch_svf_tick(&f->u.a.f[c][st], y, g, st == two ? k : 1.414f);
                y = type == 0 ? o.lp : type == 1 ? o.hp : type == 2 ? o.bp : o.lp + o.hp;
            }
            x[i] = y;
        }
    }
    if (f->mono) { for (unsigned i = 0; i < n; ++i) R[i] = L[i]; f->u.a.f[1][0] = f->u.a.f[0][0]; f->u.a.f[1][1] = f->u.a.f[0][1]; }   /* (the right's memories follow, for when the sides part) */
}

/* ---- DRIVE ---- */
/* ---- DRIVE: the input, 0 to 30 dB of
 * GAIN, into one of five curves at the plain sample rate, then a three-band TONE, then a CLIP that holds the peaks, then
 * MIX with the dry. No level matching: each curve speaks at its own level, by design. And CRUSH.
 *   TUBE        1.2 x (1 - x^2 / 3): a cubic, unbounded (the CLIP after it is what holds it).
 *   DIODE       0.707 sgn(x) (|2x| - x^2)
 *   NONLINEAR   0.26 x into a piecewise asymmetric curve: a parabola, flat on top, a twelfth-power knee below
 *   OVERDRIVE   2 x / (1 + |x|)
 *   SINUSOIDAL  sin(x r) / sin(min(r, pi/2)), r = (g^1.5 + 2) pi / 10: folds more as the GAIN rises
 *   CRUSH       DRIVE is the RATE it is held at (1 to 32 samples), CHARACTER the BITS (16 down to 2)
 * TONE: a phase-matched crossover at 350 Hz and 1.8 kHz (two-pole low-passes and first-order all-passes, after Graham
 * Wakefield's gen crossover), the bands summing flat: TONE down takes the highs to a quarter, up the lows; the mids stay.
 * CLIP: CUBIC 1.5 (x - x^3 / 3), flat past 1; SIMPLE x / (x^2 + 1/4) on a quarter of x (it falls back past its peak, as
 * there); HARD a clamp. CHARACTER: GATE, the device's noise gate (the input's level averaged over ~1000 samples, the input
 * shut below the threshold; 50 % its own 0.1), 0 off, here with a 2 ms glide so it opens and shuts without a click ---- */
static const char *const DRIVE_CHAR[6] = {"GATE", "GATE", "GATE", "GATE", "GATE", "BITS"};
const char *ch_fx_label(unsigned dev, unsigned type, unsigned p)
{
    if (dev != CH_DEV_DRIVE || type > 5) return 0;
    if (p == D_DRIVE && type == 5) return "RATE";
    if (p == D_CHAR) return DRIVE_CHAR[type];
    return 0;
}
static inline float fz_pow12(float a) { float a2 = a * a, a4 = a2 * a2; return a4 * a4 * a4; }
static inline float fz_curve(unsigned type, float x, float g, float r, float rn)
{
    switch (type) {
    case 1: { x *= g; float y = 2 * ch_abs(x) - x * x; return (x < 0 ? -y : y) * 0.707f; }   /* DIODE */
    case 2: {                                                                                 /* NONLINEAR */
        x *= 0.26f * g;
        if (x < -0.08905f) { float a = ch_abs(x) - 0.032847f; return -0.75f * (1 - fz_pow12(1 - a) + a * (1.0f / 3)) + 0.01f; }
        if (x < 0.320018f) return -6.153f * x * x + 3.9375f * x;
        return 0.630035f;
    }
    case 3: x *= g; return x / (ch_abs(x) + 1) * 2;                                           /* OVERDRIVE */
    case 4: return ch_sin_turns(x * r * (0.5f / CH_PI)) * rn;                                 /* SINUSOIDAL */
    default: x *= g; return 1.5f * x * (1 - x * x * (1.0f / 3)) * 0.8f;                     /* TUBE */
    }
}
static inline float fz_clip(unsigned m, float x)
{
    if (m == 1) { x *= 0.25f; return x / (x * x + 0.25f); }                                  /* SIMPLE */
    if (m == 2) return ch_clamp(x, -1, 1);                                                    /* HARD */
    return x < -1 ? -1.0f : x > 1 ? 1.0f : 1.5f * (x - x * x * x * (1.0f / 3));             /* CUBIC */
}
/* one first-order all-pass (the device's: y = a v + h, v = x - a h) and one two-pole low-pass (two one-poles, s = y + v) */
static inline float fz_ap(float *h, float a, float x) { float v = x - a * *h, y = a * v + *h; *h = v; return y; }
static inline float fz_lp2(float *h, float c, float x)
{
    float v = (x - h[0]) * c, y = v + h[0]; h[0] = y + v;
    float w = (y - h[1]) * c, z = w + h[1]; h[1] = z + w;
    return z;
}
static void fz_coef(float hz, float *lp, float *ap)
{
    float w = CH_TWO_PI * hz / (float)CH_RATE, sn = ch_sin_turns(w * (0.5f / CH_PI)), cs = ch_sin_turns(w * (0.5f / CH_PI) + 0.25f);
    *ap = (sn - 1) / cs; *lp = (*ap + 1) * 0.5f;
}
static void fx_drive(ch_fx *f, ch_trk *t, unsigned s, float *L, float *R, unsigned n)
{
    unsigned type = (unsigned)(PV(t, s, D_TYPE) + 0.5f); if (type > 5) type = 0;
    float c = ch_clamp(PV(t, s, D_CHAR), 0, 1);
    float tone = PV(t, s, D_TONE), lvl = ch_level_gain(PV(t, s, D_LEVEL)), mix = PV(t, s, D_MIX);
    float *ch[2] = {L, R};
    if (type == 5) {                                     /* CRUSH: held, then cut to BITS (TONE: a tilt) */
        const float a = 0.19f;
        float hold = ch_exp_map(PV(t, s, D_DRIVE), 1, 32), q = ch_exp2(15 - 13 * c), iq = 1 / q;
        for (unsigned i = 0; i < n; ++i) {
            f->acc[0] += 1;
            if (f->acc[0] >= hold) { f->acc[0] -= hold; f->hold[0] = L[i]; f->hold[1] = R[i]; }
            for (unsigned k = 0; k < 2; ++k) {
                float y = (float)(int)(f->hold[k] * q + (f->hold[k] < 0 ? -0.5f : 0.5f)) * iq;
                float *lp = &f->u.d.z[k][0];
                *lp += a * (y - *lp);
                y = tone < 0 ? y + (*lp - y) * -tone : y + (y - *lp) * tone;
                ch[k][i] += (y * lvl - ch[k][i]) * mix;
            }
        }
        for (unsigned k = 0; k < 2; ++k) f->u.d.z[k][0] = ch_ftz(f->u.d.z[k][0]);
        return;
    }
    unsigned clip = (unsigned)(PV(t, s, D_CLIP) + 0.5f); if (clip > 2) clip = 0;
    float g = ch_exp2(PV(t, s, D_DRIVE) * 30.0f * 0.16609640f);   /* 0..30 dB */
    /* the gain glides to its new value across the block (the device's: over 128 samples), so a turn of DRIVE never steps */
    float g0 = f->u.d.gp > 0 ? f->u.d.gp : g, dg = (g - g0) / (float)n; f->u.d.gp = g;
    float r = (g * ch_sqrt_fast(g) + 1.9953f) * (CH_PI / 10), rn = 1 / ch_sin_turns(ch_clamp(r, 0.01f, CH_PI * 0.5f) * (0.5f / CH_PI));
    float r0 = (g0 * ch_sqrt_fast(g0) + 1.9953f) * (CH_PI / 10), rn0 = 1 / ch_sin_turns(ch_clamp(r0, 0.01f, CH_PI * 0.5f) * (0.5f / CH_PI));
    float dr = (r - r0) / (float)n, drn = (rn - rn0) / (float)n;
    float lp1, ap1, lp2, ap2; fz_coef(1800, &lp1, &ap1); fz_coef(350, &lp2, &ap2);
    /* COMP: the level DRIVE adds, taken off again by a gain worked out from the settings alone (nothing listens to the
     * sound). A sine at half scale is put through this TYPE at this gain and this CLIP, sixteen points of its cycle, and
     * the output is scaled so its RMS is the sine's: a sound at that level comes out as loud as it went in at any DRIVE,
     * a louder one a little louder, a quieter one a little quieter. Made again only when DRIVE, TYPE or CLIP has moved.
     * OFF: the fixed 6 dB under the device, as before */
    float olvl = lvl * 0.5f;
    if (PV(t, s, D_COMP) > 0.5f) {
        float key = g + (float)(type * 64u + clip * 8u);
        if (key != f->u.d.ck) {
            static const float SN[16] = {0.19509f, 0.55557f, 0.83147f, 0.98079f, 0.98079f, 0.83147f, 0.55557f, 0.19509f,
                                         -0.19509f, -0.55557f, -0.83147f, -0.98079f, -0.98079f, -0.83147f, -0.55557f, -0.19509f};
            float sum = 0;
            for (unsigned q = 0; q < 16; ++q) { float y = fz_clip(clip, fz_curve(type, 0.5f * SN[q], g, r, rn)); sum += y * y; }
            float rms = ch_sqrt_fast(sum * (1.0f / 16));
            f->u.d.cv = ch_clamp(0.35355f / ch_max(rms, 0.02f), 0.05f, 2.0f);   /* (0.3536: the half-scale sine's RMS) */
            f->u.d.ck = key;
        }
        olvl = lvl * f->u.d.cv;
    }
    float tn = ch_clamp(0.5f + 0.5f * tone, 0, 1);         /* TONE: 0 dark .. 0.5 flat .. 1 thin */
    float hg = tn <= 0.5f ? tn * 1.5f + 0.25f : 1, bg = tn > 0.5f ? 1 - (tn - 0.5f) * 1.5f : 1;
    /* GATE: the input's level (L + R halved, rectified, ~1000 samples' average) against the threshold, the gain gliding */
    float th = c * 0.2f, gk = 1 - ch_exp2(-1.4427f / 500.0f), gg = 1 - ch_exp2(-1.4427f / (0.002f * (float)CH_RATE));
    float lev = f->u.d.gl, gs = f->u.d.gs;
    if (th <= 0) gs = 1;
    /* In passes, each a short loop over the block with a few values in hand. On the FM-1 arithmetic is cheap and memory is
     * not (a read or a write ~18 ns, a multiply-add ~10; its LOAD page's speed test): as one loop this held some thirty
     * values at once, more than the chip's registers, so nearly every use was a read from the stack, 4.3 us a frame.
     * The same sums in the same order, so the same sound */
    float gb[CH_SUB];                                      /* the GATE's gain, a frame each */
    int gated = th > 0;
    if (gated) for (unsigned i = 0; i < n; ++i) {
        lev += (ch_abs(0.5f * (L[i] + R[i])) - lev) * gk;
        gs += ((lev > th ? 1.0f : 0.0f) - gs) * gg;
        gb[i] = gs;
    }
    unsigned nch = f->mono ? 1u : 2u;                     /* the same sound both sides: once, and copied (half the work on a drum or a synth) */
    for (unsigned k = 0; k < nch; ++k) {
        float *x = ch[k], *st = f->u.d.xo[k];
        float a[CH_SUB], b[CH_SUB];
        /* 1: into the curve. The input, gated, times the gain as it glides (SINUSOIDAL: its r in the gain's place) */
        {
            float gi = type == 4 ? r0 : g0, di = type == 4 ? dr : dg;
            if (type == 2) { gi *= 0.26f; di *= 0.26f; }
            if (gated) for (unsigned i = 0; i < n; ++i) { gi += di; a[i] = x[i] * gb[i] * gi; }
            else for (unsigned i = 0; i < n; ++i) { gi += di; a[i] = x[i] * gi; }
        }
        switch (type) {
        case 1: for (unsigned i = 0; i < n; ++i) { float v = a[i], y = 2 * ch_abs(v) - v * v; a[i] = (v < 0 ? -y : y) * 0.707f; } break;   /* DIODE */
        case 2:                                                                                                                         /* NONLINEAR */
            for (unsigned i = 0; i < n; ++i) {
                float v = a[i];
                if (v < -0.08905f) { float q = ch_abs(v) - 0.032847f; v = -0.75f * (1 - fz_pow12(1 - q) + q * (1.0f / 3)) + 0.01f; }
                else if (v < 0.320018f) v = -6.153f * v * v + 3.9375f * v;
                else v = 0.630035f;
                a[i] = v;
            }
            break;
        case 3: for (unsigned i = 0; i < n; ++i) { float v = a[i]; a[i] = v / (ch_abs(v) + 1) * 2; } break;                              /* OVERDRIVE */
        case 4: { float ri = rn0; for (unsigned i = 0; i < n; ++i) { ri += drn; a[i] = ch_sin_turns(a[i] * (0.5f / CH_PI)) * ri; } break; }   /* SINUSOIDAL */
        default: for (unsigned i = 0; i < n; ++i) { float v = a[i]; a[i] = 1.5f * v * (1 - v * v * (1.0f / 3)) * 0.8f; } break;            /* TUBE */
        }
        /* 2: the crossover's first split, at 1.8 kHz: a the lows, b the highs */
        {
            float h0 = st[0], h1 = st[1], h2 = st[2];
            for (unsigned i = 0; i < n; ++i) {
                float y = a[i];
                float v = (y - h0) * lp1, y1 = v + h0; h0 = y1 + v;
                float w = (y1 - h1) * lp1, lo1 = w + h1; h1 = lo1 + w;
                float u = y - ap1 * h2, hp = ap1 * u + h2; h2 = u;
                a[i] = lo1; b[i] = hp - lo1;
            }
            st[0] = h0; st[1] = h1; st[2] = h2;
        }
        /* 3: the second, at 350 Hz, and the bands back together at TONE's gains */
        {
            float h3 = st[3], h4 = st[4], h5 = st[5], h6 = st[6];
            for (unsigned i = 0; i < n; ++i) {
                float lo1 = a[i], hi = b[i];
                float u = hi - ap2 * h3; hi = ap2 * u + h3; h3 = u;
                float v = (lo1 - h4) * lp2, y1 = v + h4; h4 = y1 + v;
                float w = (y1 - h5) * lp2, lo = w + h5; h5 = lo + w;
                float q = lo1 - ap2 * h6, mid = ap2 * q + h6 - lo; h6 = q;
                a[i] = lo * bg + mid + hi * hg;
            }
            st[3] = h3; st[4] = h4; st[5] = h5; st[6] = h6;
        }
        /* 4: the CLIP, LEVEL and MIX */
        if (clip == 1) for (unsigned i = 0; i < n; ++i) { float v = a[i] * 0.25f, dry = x[i]; v = v / (v * v + 0.25f); x[i] = dry + (v * olvl - dry) * mix; }
        else if (clip == 2) for (unsigned i = 0; i < n; ++i) { float v = ch_clamp(a[i], -1, 1), dry = x[i]; x[i] = dry + (v * olvl - dry) * mix; }
        else for (unsigned i = 0; i < n; ++i) { float v = a[i], dry = x[i]; v = v < -1 ? -1.0f : v > 1 ? 1.0f : 1.5f * (v - v * v * v * (1.0f / 3)); x[i] = dry + (v * olvl - dry) * mix; }
    }
    if (f->mono) { for (unsigned i = 0; i < n; ++i) R[i] = L[i]; for (unsigned q = 0; q < 7; ++q) f->u.d.xo[1][q] = f->u.d.xo[0][q]; }
    f->u.d.gl = ch_ftz(lev); f->u.d.gs = gs;
    for (unsigned k = 0; k < 2; ++k) for (unsigned q = 0; q < 7; ++q) f->u.d.xo[k][q] = ch_ftz(f->u.d.xo[k][q]);
}

/* ---- CHORUS, FLANGER: a swept short delay a side (the block: two 2048-frame lines) ---- */
static void fx_sweep(ch_fx *f, ch_trk *t, unsigned s, float *L, float *R, unsigned n, int flanger)
{
    if (!f->mem) return;
    float rate = flanger ? ch_exp_map(PV(t, s, L_RATE), 0.03f, 3) : ch_exp_map(PV(t, s, C_RATE), 0.05f, 5);
    float depth = PV(t, s, flanger ? L_DEPTH : C_DEPTH), mix = PV(t, s, flanger ? L_MIX : C_MIX);
    float fb = flanger ? PV(t, s, L_FB) * 0.9f : 0;
    float base = flanger ? 13.0f : 309.0f, span = flanger ? 265.0f * depth : 530.0f * depth;   /* frames */
    unsigned shape = (unsigned)(PV(t, s, flanger ? L_SHAPE : C_SHAPE) + 0.5f); float c0, c1;
    float ph1 = fx_turn(f, rate, n, &c0, &c1);
    float a0[2] = {fx_wave(f->ph, shape, c0), fx_wave(f->ph + 0.25f, shape, c0)}, a1[2] = {fx_wave(ph1, shape, c1), fx_wave(ph1 + 0.25f, shape, c1)};
    f->ph = ph1;
    float in = 1.0f / (float)n;
    for (unsigned c = 0; c < 2; ++c) {
        float *x = c ? R : L;
        int16_t *b = f->mem + c * 2048;
        uint32_t w = f->w;
        float d = base + span * (0.5f + 0.5f * a0[c]), dd = span * 0.5f * (a1[c] - a0[c]) * in;
        float last = f->fb[c];
        if (ch_fx_pre) {                                 /* a master return: MIX of the mix and the sends whole go in, what comes out is added */
            const float *e = ch_fx_el ? (c ? ch_fx_er : ch_fx_el) : 0; const float dry = 1 - mix;
            for (unsigned i = 0; i < n; ++i) {
                float u = x[i] * mix; if (e) u += e[i];
                b[w & 2047u] = s16(sat(u + fb * last));
                float y = tap(b, 2047u, w, d);
                last = y;
                x[i] = x[i] * dry + y;
                d += dd; ++w;
            }
        } else
        for (unsigned i = 0; i < n; ++i) {
            b[w & 2047u] = s16(sat(x[i] + fb * last));
            float y = tap(b, 2047u, w, d);
            last = y;
            x[i] += (y - x[i]) * mix;
            d += dd; ++w;
        }
        f->fb[c] = ch_ftz(last);
    }
    f->w += n;
}

/* ---- UNISON: copies of the sound, each a little off pitch, across the two sides (the block: one 2048-frame line) ----
 * A unison stack played by one voice: the two sides summed into a line, and two, four or six taps on it, each tap's
 * time swaying on a slow sine of its own (0.31 to 1.03 Hz, none a multiple of another, so they never line up), which
 * bends its pitch up and down by DETUNE, 2 to 25 cents at the most. The taps alternate left and right, further out
 * the later (SPREAD). The copies are of one sound, so they sway where a real stack's oscillators beat; what it costs
 * is the same for one note or a chord. All the taps' phases come from one clock (f->ph, a turn every 100 s) */
enum { UN_DETUNE, UN_VOICES, UN_SPREAD, UN_MIX };
float ch_fx_unison_cents(float v) { return 2 + 23 * ch_clamp(v, 0, 1); }
static void fx_unison(ch_fx *f, ch_trk *t, unsigned s, float *L, float *R, unsigned n)
{
    static const uint8_t M[6] = {43, 57, 31, 71, 89, 103};   /* each tap's rate, hundredths of a Hz */
    static const float OFF[6] = {0, 0.37f, 0.71f, 0.13f, 0.55f, 0.89f};
    if (!f->mem) return;
    const float cents = ch_fx_unison_cents(PV(t, s, UN_DETUNE)), spread = PV(t, s, UN_SPREAD), mix = PV(t, s, UN_MIX);
    const unsigned nv = 2 + 2 * (unsigned)ch_clamp(PV(t, s, UN_VOICES) + 0.5f, 0, 2);
    const float ph0 = f->ph, ph1 = ph0 + 0.01f * (float)n * (1.0f / (float)CH_RATE), in = 1.0f / (float)n;
    const float g = nv == 2 ? 0.53f : nv == 4 ? 0.375f : 0.31f, dry = 1 - mix;
    int16_t *b = f->mem; const uint32_t w0 = f->w;
    float wl[CH_SUB], wr[CH_SUB];
    if (ch_fx_pre) {                                     /* a master return: MIX of the mix and the sends whole go in */
        for (unsigned i = 0; i < n; ++i) {
            float u = (L[i] + R[i]) * (0.5f * mix); if (ch_fx_el) u += 0.5f * (ch_fx_el[i] + ch_fx_er[i]);
            b[(w0 + i) & 2047u] = s16(sat(u)); wl[i] = wr[i] = 0;
        }
    } else for (unsigned i = 0; i < n; ++i) { b[(w0 + i) & 2047u] = s16(sat(0.5f * (L[i] + R[i]))); wl[i] = wr[i] = 0; }
    for (unsigned k = 0; k < nv; ++k) {
        /* a sway of A frames at hz bends the pitch by 2 pi hz A / rate at the most: A for `cents` (a cent: 0.0578 per cent) */
        const float A = cents * (0.0005776f * (float)CH_RATE / 6.2831853f) * (100.0f / (float)M[k]), base = A + 8;
        float d = base + A * ch_sin_turns(ph0 * (float)M[k] + OFF[k]);
        const float dd = (base + A * ch_sin_turns(ph1 * (float)M[k] + OFF[k]) - d) * in;
        const float p = ((k & 1u) ? 1.0f : -1.0f) * (float)(k / 2u + 1u) / (float)(nv / 2u) * spread;
        const float gl = g * (1 - p), gr = g * (1 + p);
        for (unsigned i = 0; i < n; ++i) { const float y = tap(b, 2047u, w0 + i, d); wl[i] += y * gl; wr[i] += y * gr; d += dd; }
    }
    if (ch_fx_pre) for (unsigned i = 0; i < n; ++i) { L[i] = L[i] * dry + wl[i]; R[i] = R[i] * dry + wr[i]; }
    else for (unsigned i = 0; i < n; ++i) { L[i] += (wl[i] - L[i]) * mix; R[i] += (wr[i] - R[i]) * mix; }
    f->ph = ph1 >= 1 ? ph1 - 1 : ph1;
    f->w += n;
}

/* ---- ERODE: a short delay a side whose time is shaken at audio rate (the block: two 2048-frame lines) ----
 * What shakes it: white noise through a band-pass at FREQ, WIDTH its width (NOISE: one noise for both sides; WIDE: one
 * each, so the grit spreads), or a sine at FREQ (SINE: side bands, a ring; WIDTH there parts it into a sine a side, the
 * left below FREQ and the right above, up to an octave between them). AMOUNT is how far the time moves, up to
 * 1.4 ms; the sound is all the shaken one, so at nothing it is the sound as it came (two frames late) */
float ch_fx_erode_hz(float v) { return ch_exp_map(v, 100, 7000); }
static void fx_erode(ch_fx *f, ch_trk *t, unsigned s, float *L, float *R, unsigned n)
{
    if (!f->mem) return;
    const float amt = PV(t, s, O_AMT), hz = ch_fx_erode_hz(PV(t, s, O_FREQ));
    const unsigned mode = (unsigned)(PV(t, s, O_MODE) + 0.5f);
    const float span = amt * amt * 60.0f, base = span + 2.0f;   /* frames */
    float m[2][CH_SUB];
    if (mode >= 2) {                                     /* SINE */
        /* WIDTH parts the sine in two, one a side: the left below FREQ, the right above, up to an octave between them
         * at the most, so each side rings at its own pitch and the two beat across the middle. At nothing: one sine,
         * as it was (the second sine's phase is in lp[0], the noise's memory, which SINE has no other use for) */
        const float sp = ch_exp2(0.5f * ch_clamp(PV(t, s, O_WIDTH), 0, 1)), inc = hz * (1.0f / (float)CH_RATE);
        const float i0 = inc / sp, i1 = ch_min(inc * sp, 0.45f);
        float ph = f->ph, p2 = f->lp[0]; p2 -= (float)(int)p2; if (p2 < 0) p2 += 1;
        if (sp < 1.0001f) { for (unsigned i = 0; i < n; ++i) { m[0][i] = m[1][i] = ch_sin_turns(ph); ph += inc; if (ph >= 1) ph -= 1; } p2 = ph; }
        else for (unsigned i = 0; i < n; ++i) {
            m[0][i] = ch_sin_turns(ph); m[1][i] = ch_sin_turns(p2);
            ph += i0; if (ph >= 1) ph -= 1;
            p2 += i1; if (p2 >= 1) p2 -= 1;
        }
        f->ph = ph; f->lp[0] = p2;
    } else {                                             /* a band of noise: a state-variable band-pass, its level evened across FREQ and WIDTH */
        /* (this filter holds only while 4 - 2 fc q - fc fc > 0: at its widest, q 2, that failed above 6 kHz, the state ran
         * away to infinity and the guard muted the effect, hundreds of times with a modulator on FREQ. The width is held
         * a tenth inside what FREQ allows; under 6 kHz nothing changes) */
        const float fc = 2 * ch_sin_turns(hz * (0.5f / (float)CH_RATE)), q = ch_min(1 / ch_exp_map(1 - PV(t, s, O_WIDTH), 0.5f, 12.0f), 0.9f * (4 - fc * fc) / (2 * fc));
        const float g = 0.5f * ch_exp2(0.5f * ch_log2(6 * q / fc));
        for (unsigned c = 0; c < 2; ++c) {
            if (c && mode == 0) { for (unsigned i = 0; i < n; ++i) m[1][i] = m[0][i]; break; }
            float lp = f->lp[c * 2], bp = f->lp[c * 2 + 1];
            for (unsigned i = 0; i < n; ++i) {
                lp += fc * bp;
                bp += fc * (ch_rands(&f->rng) - lp - q * bp);
                m[c][i] = ch_clamp(bp * g, -1, 1);
            }
            f->lp[c * 2] = ch_ftz(lp); f->lp[c * 2 + 1] = ch_ftz(bp);
        }
    }
    for (unsigned c = 0; c < 2; ++c) {
        float *x = c ? R : L; const float *mm = m[c];
        int16_t *b = f->mem + c * 2048;
        uint32_t w = f->w;
        for (unsigned i = 0; i < n; ++i) {
            b[w & 2047u] = s16(sat(x[i]));
            x[i] = tap(b, 2047u, w, base + span * mm[i]);
            ++w;
        }
    }
    f->w += n;
}

/* ---- DISPERSE: up to sixteen second-order all-passes one after another, all at FREQ (the block: their memories) ----
 * An all-pass changes no level, only when each part of the sound arrives: round FREQ it comes late, by more the more of
 * them there are (AMOUNT) and the narrower they are (PINCH: Q 0.3 .. 8). A hit goes in, a chirp comes out. MIX under
 * 100 lets the sound as it came back in beside it: notches, like a phaser standing still.
 * The coefficients glide over the block, so the knobs do not zip */
float ch_fx_disp_hz(float v) { return ch_exp_map(v, 40, 8000); }
float ch_fx_disp_q(float v) { return ch_exp_map(v, 0.3f, 8.0f); }
unsigned ch_fx_disp_stages(float v) { return (unsigned)(ch_clamp(v, 0, 1) * (float)H_MAX + 0.5f); }
/* (in the RAM, CH_AUDIO_CODE: sixteen filters a side a frame, the heaviest of the effects) */
static CH_AUDIO_CODE CH_NOINLINE void fx_disperse(ch_fx *f, ch_trk *t, unsigned s, float *L, float *R, unsigned n)
{
    if (!f->mem) return;
    const unsigned ns = ch_fx_disp_stages(PV(t, s, H_AMT));
    const float mix = PV(t, s, H_MIX), in = 1.0f / (float)n;
    const float w = ch_fx_disp_hz(PV(t, s, H_FREQ)) * (1.0f / (float)CH_RATE);
    const float al = ch_sin_turns(w) / (2 * ch_fx_disp_q(PV(t, s, H_PINCH)));
    const float c1 = (1 - al) / (1 + al), d1 = -2 * ch_sin_turns(w + 0.25f) / (1 + al);
    if (f->hold[0] == 0) { f->prev[0] = c1; f->prev[1] = d1; f->hold[0] = 1; }
    const float c0 = f->prev[0], d0 = f->prev[1], dc = (c1 - c0) * in, dd = (d1 - d0) * in;
    f->prev[0] = c1; f->prev[1] = d1;
    if (!ns) return;
    float *st = (float *)(void *)f->mem;                 /* [side][stage][2] */
    const unsigned nch = f->mono ? 1u : 2u;              /* the same sound both sides: once, and copied */
    for (unsigned ch = 0; ch < nch; ++ch) {
        float *x = ch ? R : L, *z = st + ch * (2 * H_MAX);
        /* A stage at a time over the whole block, its two memories held in registers, where it was a frame at a time
         * through every stage, reading and writing thirty-two memories a frame: the same sums in the same order (each
         * stage's coefficients glide the same way), a fraction of the memory traffic, which is what costs on the FM-1 */
        float y[CH_SUB];
        for (unsigned i = 0; i < n; ++i) y[i] = x[i];
        for (unsigned k = 0; k < ns; ++k) {
            float z0 = z[2 * k], z1 = z[2 * k + 1], c = c0, d = d0;
            for (unsigned i = 0; i < n; ++i) {
                const float v = y[i], o = c * v + z0;
                z0 = d * (v - o) + z1;
                z1 = v - c * o;
                y[i] = o;
                c += dc; d += dd;
            }
            z[2 * k] = ch_ftz(z0); z[2 * k + 1] = ch_ftz(z1);
        }
        for (unsigned i = 0; i < n; ++i) x[i] += (y[i] - x[i]) * mix;
    }
    if (f->mono) { for (unsigned i = 0; i < n; ++i) R[i] = L[i]; for (unsigned k = 0; k < 2 * ns; ++k) st[2 * H_MAX + k] = st[k]; }
}

/* ---- PHASER ---- */
static void fx_phaser(ch_fx *f, ch_trk *t, unsigned s, float *L, float *R, unsigned n)
{
    float rate = ch_exp_map(PV(t, s, L_RATE), 0.03f, 4), depth = PV(t, s, L_DEPTH);
    float fb = PV(t, s, L_FB) * 0.85f, mix = PV(t, s, L_MIX);
    unsigned shape = (unsigned)(PV(t, s, L_SHAPE) + 0.5f); float c0, c1;
    const unsigned np = 2u + 2u * (unsigned)ch_clamp(PV(t, s, 5) + 0.5f, 0, 3);   /* POLES: 2, 4, 6 or 8 all-passes, a notch a pair */
    float ph1 = fx_turn(f, rate, n, &c0, &c1);
    float a0[2], a1[2], in = 1.0f / (float)n;
    for (unsigned c = 0; c < 2; ++c) {
        float off = c ? 0.25f : 0;
        float g0 = ch_svf_g(ch_exp_map(0.5f + 0.5f * depth * fx_wave(f->ph + off, shape, c0), 150, 5000));
        float g1 = ch_svf_g(ch_exp_map(0.5f + 0.5f * depth * fx_wave(ph1 + off, shape, c1), 150, 5000));
        a0[c] = (g0 - 1) / (g0 + 1); a1[c] = (g1 - 1) / (g1 + 1);
    }
    f->ph = ph1;
    for (unsigned c = 0; c < 2; ++c) {
        float *x = c ? R : L, *z = f->u.a.z[c];
        float a = a0[c], da = (a1[c] - a0[c]) * in, last = f->fb[c];
        if (ch_fx_pre) {                                 /* a master return */
            const float *e = ch_fx_el ? (c ? ch_fx_er : ch_fx_el) : 0; const float dry = 1 - 0.5f * mix;
            for (unsigned i = 0; i < n; ++i) {
                float y = x[i] * mix + fb * last; if (e) y += e[i];
                for (unsigned k = 0; k < np; ++k) { float o = a * y + z[k]; z[k] = y - a * o; y = o; }
                last = y;
                x[i] = x[i] * dry + y * 0.5f;
                a += da;
            }
        } else
        for (unsigned i = 0; i < n; ++i) {
            float y = x[i] + fb * last;
            for (unsigned k = 0; k < np; ++k) { float o = a * y + z[k]; z[k] = y - a * o; y = o; }
            last = y;
            x[i] = x[i] * (1 - 0.5f * mix) + y * 0.5f * mix;
            a += da;
        }
        for (unsigned k = 0; k < np; ++k) z[k] = ch_ftz(z[k]);
        for (unsigned k = np; k < 8; ++k) z[k] = 0;      /* (the ones not in use: empty for when POLES is turned up) */
        f->fb[c] = ch_ftz(last);
    }
}

/* ---- TREMOLO ---- */
static void fx_tremolo(ch_fx *f, ch_trk *t, unsigned s, float *L, float *R, unsigned n)
{
    float rate = ch_exp_map(PV(t, s, T_RATE), 0.2f, 16), depth = PV(t, s, T_DEPTH), st = PV(t, s, T_STEREO);
    unsigned shape = (unsigned)(PV(t, s, T_SHAPE) + 0.5f);
    float ph1 = wrap1(f->ph + rate * (float)n * (1.0f / (float)CH_RATE));
    float w0 = lfo_at(f->ph, shape), w1 = lfo_at(ph1, shape);
    f->ph = ph1;
    float in = 1.0f / (float)n;
    for (unsigned c = 0; c < 2; ++c) {
        float *x = c ? R : L;
        float s0 = c ? w0 + (-w0 - w0) * st : w0, s1 = c ? w1 + (-w1 - w1) * st : w1;
        float g = 1 - depth * (0.5f - 0.5f * s0), dg = -depth * 0.5f * (s1 - s0) * in;
        for (unsigned i = 0; i < n; ++i) { x[i] *= g; g -= dg; }
    }
}

/* ---- the decimated lines' in and out: input summed `D` frames, one line step, out interpolated ---- */
#define DECIM_STEP(D)  (++f->dn >= (D))

/* ---- DELAY: stereo, tempo-synced, at half rate; two 16384-frame lines ---- */
static void fx_delay(ch_fx *f, ch_trk *t, unsigned s, float *L, float *R, unsigned n)
{
    if (!f->mem) return;
    enum { D = 2, LEN = 16384 };
    float target = ch_min(dly_frames(t, s, Y_TIME, (float)CH_RATE / D, 0.74f), (float)LEN - 4);
    if (f->t_l <= 0) f->t_l = target;
    float fb = PV(t, s, Y_FB) * 0.95f, mix = PV(t, s, Y_MIX), tone = PV(t, s, Y_TONE);
    int ping = PV(t, s, Y_PING) > 0.5f;
    const float *eL = ch_fx_el, *eR = ch_fx_er; float pre = 1; if (ch_fx_pre) { pre = mix; mix = 1; }   /* (a master effect: MIX is what of the mix goes in, the sends go in whole, and what comes out is heard whole) */
    float ta = tone < 0 ? 0.08f + 0.9f * (1 + tone) : 1;  /* TONE down: the repeats darker (a low pass) */
    float th = tone > 0 ? tone * 0.6f : 0;               /* TONE up: thinner (a high pass) */
    int16_t *bl = f->mem, *br = f->mem + LEN;
    for (unsigned i = 0; i < n; ++i) {
        f->acc[0] += L[i] * pre; f->acc[1] += R[i] * pre;
        if (eL) { f->acc[0] += eL[i]; f->acc[1] += eR[i]; }
        if (DECIM_STEP(D)) {
            f->dn = 0;
            float xl = f->acc[0] * (1.0f / D), xr = f->acc[1] * (1.0f / D);
            f->acc[0] = f->acc[1] = 0;
            f->t_l += (target - f->t_l) * 0.0015f;          /* a new time glides in */
            float yl = tap(bl, LEN - 1u, f->w, f->t_l), yr = tap(br, LEN - 1u, f->w, f->t_l);
            /* the tone in the feedback: a one-pole each side, low or high */
            f->u.a.z[0][0] += ta * (yl - f->u.a.z[0][0]); f->u.a.z[1][0] += ta * (yr - f->u.a.z[1][0]);
            float fl = f->u.a.z[0][0] - th * f->u.a.z[0][1], fr = f->u.a.z[1][0] - th * f->u.a.z[1][1];
            f->u.a.z[0][1] += 0.05f * (f->u.a.z[0][0] - f->u.a.z[0][1]); f->u.a.z[1][1] += 0.05f * (f->u.a.z[1][0] - f->u.a.z[1][1]);
            uint32_t wi = (f->w + 1u) & (LEN - 1u);
            if (ping) { bl[wi] = s16(sat((xl + xr) * 0.5f + fb * fr)); br[wi] = s16(sat(fb * fl)); }
            else { bl[wi] = s16(sat(xl + fb * fl)); br[wi] = s16(sat(xr + fb * fr)); }
            f->w = wi;
            f->prev[0] = f->cur[0]; f->prev[1] = f->cur[1]; f->cur[0] = yl; f->cur[1] = yr;
        }
        float fr = (float)f->dn * (1.0f / D);
        float wl = f->prev[0] + (f->cur[0] - f->prev[0]) * fr, wr = f->prev[1] + (f->cur[1] - f->prev[1]) * fr;
        L[i] += wl * mix; R[i] += wr * mix;
    }
    for (unsigned c = 0; c < 2; ++c) { f->u.a.z[c][0] = ch_ftz(f->u.a.z[c][0]); f->u.a.z[c][1] = ch_ftz(f->u.a.z[c][1]); }
}

/* ---- ECHO: a tape echo, mono, at a quarter rate; one 16384-frame line ---- */
static void fx_echo(ch_fx *f, ch_trk *t, unsigned s, float *L, float *R, unsigned n)
{
    if (!f->mem) return;
    enum { D = 4, LEN = 16384 };
    const float rate = (float)CH_RATE / D;
    float target = ch_min(ch_exp_map(PV(t, s, E_TIME), 0.05f, 1.4f) * rate, (float)LEN - 100);
    if (f->t_l <= 0) f->t_l = target;
    float fb = PV(t, s, E_FB) * 1.05f, mix = PV(t, s, E_MIX), wow = PV(t, s, E_WOW);
    float lp = 1 - ch_exp2(-6.2832f * ch_exp_map(1 - PV(t, s, E_TONE), 700, 5000) / rate * 1.4427f);
    const float *eL = ch_fx_el, *eR = ch_fx_er; float pre = 1; if (ch_fx_pre) { pre = mix; mix = 1; }   /* (a master effect: MIX is what of the mix goes in, the sends go in whole, and what comes out is heard whole) */
    /* wow (0.6 Hz) and flutter (7 Hz): their points at the sub-block's ends, in line frames */
    float steps = (float)n / D;
    float p1 = wrap1(f->ph + 0.6f * (float)n / (float)CH_RATE), q1 = wrap1(f->ph2 + 7.0f * (float)n / (float)CH_RATE);
    float m0 = wow * (40 * ch_sin_turns(f->ph) + 3 * ch_sin_turns(f->ph2));
    float m1 = wow * (40 * ch_sin_turns(p1) + 3 * ch_sin_turns(q1));
    f->ph = p1; f->ph2 = q1;
    float m = m0, dm = (m1 - m0) / (steps > 0 ? steps : 1);
    int16_t *b = f->mem;
    for (unsigned i = 0; i < n; ++i) {
        f->acc[0] += (L[i] + R[i]) * pre; if (eL) f->acc[0] += eL[i] + eR[i];
        if (DECIM_STEP(D)) {
            f->dn = 0;
            float x = f->acc[0] * (0.5f / D); f->acc[0] = 0;
            f->t_l += (target - f->t_l) * 0.0008f;          /* TIME turned: the tape speeds up or slows */
            float y = tap(b, LEN - 1u, f->w, f->t_l + 44 + m);
            m += dm;
            f->u.a.z[0][0] += lp * (y - f->u.a.z[0][0]);              /* the head's darkening */
            uint32_t wi = (f->w + 1u) & (LEN - 1u);
            b[wi] = s16(sat(x + fb * f->u.a.z[0][0]) * 0.9f);   /* saturating: high FEEDBACK runs away, gently */
            f->w = wi;
            f->prev[0] = f->cur[0]; f->cur[0] = f->u.a.z[0][0];
        }
        float wv = f->prev[0] + (f->cur[0] - f->prev[0]) * ((float)f->dn * (1.0f / D));
        L[i] += wv * mix; R[i] += wv * mix;
    }
    f->u.a.z[0][0] = ch_ftz(f->u.a.z[0][0]);
}

/* ---- DUB DELAY: mono, tempo-synced, at a quarter rate; a resonant filter in the feedback (FILTER: band-pass, low pass, high
 * pass or notch; RESONANCE), its cutoff swept by a slow sine ---- */

static void fx_dub(ch_fx *f, ch_trk *t, unsigned s, float *L, float *R, unsigned n)
{
    if (!f->mem) return;
    enum { D = 4, LEN = 16384 };
    const float rate = (float)CH_RATE / D;
    float target = ch_min(dly_frames(t, s, U_TIME, rate, 1.45f), (float)LEN - 100);
    if (f->t_l <= 0) f->t_l = target;
    float fb = PV(t, s, U_FB) * 1.15f, mix = PV(t, s, U_MIX), sweep = PV(t, s, U_SWEEP);
    const float *eL = ch_fx_el, *eR = ch_fx_er; float pre = 1; if (ch_fx_pre) { pre = mix; mix = 1; }   /* (a master effect: MIX is what of the mix goes in, the sends go in whole, and what comes out is heard whole) */
    /* the band-pass's centre: CUTOFF, swept up to two octaves either way by a slow sine (0.08 Hz) */
    float ph1 = wrap1(f->ph + 0.08f * (float)n / (float)CH_RATE);
    float oct = sweep * 2.0f * ch_sin_turns(f->ph);
    f->ph = ph1;
    unsigned ft = (unsigned)(PV(t, s, U_FTYPE) + 0.5f) & 3u;
    float k = 2 - 1.92f * ch_clamp(PV(t, s, U_RES), 0, 1);   /* RESONANCE: 0 gentle, near full a ringing peak (the feedback's saturation holds it) */
    float hz = ch_exp_map(PV(t, s, U_CUT), 100, 3000) * ch_exp2(oct);
    float g = (CH_PI * ch_clamp(hz, 60, 4500) / rate);
    g = g * (1 + g * g * (1.0f / 3));                      /* tan, at the quarter rate */
    int16_t *b = f->mem;
    for (unsigned i = 0; i < n; ++i) {
        f->acc[0] += (L[i] + R[i]) * pre; if (eL) f->acc[0] += eL[i] + eR[i];
        if (DECIM_STEP(D)) {
            f->dn = 0;
            float x = f->acc[0] * (0.5f / D); f->acc[0] = 0;
            f->t_l += (target - f->t_l) * 0.0015f;
            float y = tap(b, LEN - 1u, f->w, f->t_l);
            ch_svf_out_t o = ch_svf_tick(&f->u.a.f[0][0], y, g, k);
            float bp = ft == 0 ? o.bp * k * 2.29f : ft == 1 ? o.lp : ft == 2 ? o.hp : o.lp + o.hp;   /* (BAND made up for its loss: at the old resonance, as it was) */
            uint32_t wi = (f->w + 1u) & (LEN - 1u);
            b[wi] = s16(sat(x + fb * bp) * 0.9f);
            f->w = wi;
            f->prev[0] = f->cur[0]; f->cur[0] = bp;
        }
        float wv = f->prev[0] + (f->cur[0] - f->prev[0]) * ((float)f->dn * (1.0f / D));
        L[i] += wv * mix; R[i] += wv * mix;
    }
}

/* ---- DJ FILTER: one knob, a low pass closing to the left, a high pass rising to the right; flat in the middle ---- */
static void fx_dj(ch_fx *f, ch_trk *t, unsigned s, float *L, float *R, unsigned n)
{
    float a = PV(t, s, J_AMOUNT);
    if (a > -0.01f && a < 0.01f) return;
    float hz = a < 0 ? ch_exp_map(1 + a, 60, 18000) : ch_exp_map(a, 20, 7000);
    float g = ch_svf_g(hz), k = 2 - 1.75f * ch_clamp(PV(t, s, J_RES), 0, 1);
    float *ch[2] = {L, R};
    const unsigned nch = f->mono ? 1u : 2u;              /* the same sound both sides: once, and copied */
    for (unsigned c = 0; c < nch; ++c) {
        float *x = ch[c];
        if (a < 0) for (unsigned i = 0; i < n; ++i) x[i] = ch_svf_tick(&f->u.a.f[c][0], x[i], g, k).lp;
        else for (unsigned i = 0; i < n; ++i) x[i] = ch_svf_tick(&f->u.a.f[c][0], x[i], g, k).hp;
    }
    if (f->mono) { for (unsigned i = 0; i < n; ++i) R[i] = L[i]; f->u.a.f[1][0] = f->u.a.f[0][0]; }   /* (the right's memory follows, for when the sides part) */
}

/* ---- SIDECHAIN: a hit on the source track drops the gain by AMOUNT (over ATTACK), back over RELEASE ---- */
static void fx_side(ch_fx *f, ch_trk *t, unsigned s, float *L, float *R, unsigned n)
{
    unsigned sel = (unsigned)(PV(t, s, S_SRC) + 0.5f) % (CH_TRACKS + CH_TRACKS * CH_KIT_PARTS);   /* a track, or (past the four) one lane of one */
    unsigned src = sel < CH_TRACKS ? sel : (sel - CH_TRACKS) / CH_KIT_PARTS;
    uint16_t hits = sel < CH_TRACKS ? ch_hits[src] : ch_lane_hits[src][(sel - CH_TRACKS) % CH_KIT_PARTS];
    float amt = PV(t, s, S_AMT);
    float att = ch_exp_map(PV(t, s, S_ATT), 0.001f, 0.1f), rel = ch_exp_map(PV(t, s, S_REL), 0.008f, 1.5f);
    if (hits != (uint16_t)f->wi[0]) { f->wi[0] = hits; f->fb[0] = 1; }   /* a hit: the pump from the top */
    float dt = (float)n * (1.0f / (float)CH_RATE);
    float env = f->fb[0], g0 = f->cur[0] > 0 ? f->cur[0] : 1;
    float target = 1 - amt * env;
    float k = target < g0 ? 1 - ch_exp2(-1.4427f * dt / att) : 1 - ch_exp2(-1.4427f * dt / rel);
    float g1 = g0 + (target - g0) * k;
    f->fb[0] = ch_ftz(env * ch_exp2(-1.4427f * dt / rel));
    f->cur[0] = g1;
    float g = g0, dg = (g1 - g0) / (float)n;
    if (t == &ch_mst && PV(t, s, S_ALL) < 0.5f) {        /* the master: the source's own output is left out of the duck */
        const float *sL = ch_tmix[src][0], *sR = ch_tmix[src][1];
        for (unsigned i = 0; i < n; ++i) { L[i] = (L[i] - sL[i]) * g + sL[i]; R[i] = (R[i] - sR[i]) * g + sR[i]; g += dg; }
    } else for (unsigned i = 0; i < n; ++i) { L[i] *= g; R[i] *= g; g += dg; }
}

/* ---- RESONATOR: four band-passes at a note's partials, the band gain made up for the resonance ---- */
static float reso_note(float v) { float n = 24 + v * 72; return ch_key_mask ? ch_key_quantize(n, ch_key_root) : (float)(int)(n + 0.5f); }
static void fx_reso(ch_fx *f, ch_trk *t, unsigned s, float *L, float *R, unsigned n)
{
    static const float HARM[4] = {1, 2, 3, 4}, STR[4] = {1, 2.41f, 4.07f, 6.3f}, GN[4] = {1, 0.6f, 0.42f, 0.3f};
    float hz = 440 * ch_exp2((reso_note(PV(t, s, Z_PITCH)) - 69) * (1.0f / 12));
    float res = ch_clamp(PV(t, s, Z_RES), 0, 1), spread = PV(t, s, Z_SPREAD), mix = PV(t, s, Z_MIX);
    float k = 2 - 1.96f * res, make = k * (1 + (1 - k) * 0.6f);
    float g[4];
    for (unsigned b = 0; b < 4; ++b) g[b] = ch_svf_g(hz * ch_lerp(HARM[b], STR[b], spread));
    if (ch_fx_pre) {                                     /* a master return */
        const float *eL = ch_fx_el, *eR = ch_fx_er; const float dry = 1 - mix;
        for (unsigned i = 0; i < n; ++i) {
            float x = (L[i] + R[i]) * 0.5f * mix, y = 0; if (eL) x += (eL[i] + eR[i]) * 0.5f;
            for (unsigned b = 0; b < 4; ++b) y += ch_svf_tick(&f->u.a.f[b >> 1][b & 1], x, g[b], k).bp * GN[b];
            y *= make;
            L[i] = L[i] * dry + y; R[i] = R[i] * dry + y;
        }
    } else
    for (unsigned i = 0; i < n; ++i) {
        float x = (L[i] + R[i]) * 0.5f, y = 0;
        for (unsigned b = 0; b < 4; ++b) y += ch_svf_tick(&f->u.a.f[b >> 1][b & 1], x, g[b], k).bp * GN[b];
        y *= make;
        L[i] += (y - L[i]) * mix; R[i] += (y - R[i]) * mix;
    }
}

/* ---- COMB: a delay of one period, fed back (peaks at the pitch and its harmonics) or fed forward (notches between).
 * POLARITY - flips the delayed copy: odd harmonics only, the pitch an octave down. DAMP darkens the loop, SPREAD
 * detunes the right side up to a semitone. FREQ in Hz (FREE), on notes (NOTE: the master key's when one is on), or
 * set on a note and moved by a track's incoming notes (FOLLOW: OWN or T1..T4), by their distance from C4; until a
 * note comes it sits on the knob's note. The block: two
 * 2048-frame lines (down to 22 Hz). ---- */
enum { K_FREQ, K_FB, K_DAMP, K_MIX, K_POL, K_TYPE, K_SPREAD, K_SRC };
static float comb_note_of(const ch_trk *t, unsigned s, float v, unsigned mode)
{
    if (mode == 1) return reso_note(v);
    unsigned src = (unsigned)(PV(t, s, K_SRC) + 0.5f);
    int ti = src ? (int)src - 1 : (t >= ch_tr && t < ch_tr + CH_TRACKS) ? (int)(t - ch_tr) : -1;
    return reso_note(v) + (ti >= 0 && ti < CH_TRACKS && ch_last_note[ti] ? (float)ch_last_note[ti] - 60 : 0);
}
static float comb_hz(const ch_trk *t, unsigned s)
{
    float v = PV(t, s, K_FREQ); unsigned mode = fx_mode(t, s, K_FREQ);
    float hz = mode ? 440 * ch_exp2((comb_note_of(t, s, v, mode) - 69) * (1.0f / 12)) : ch_exp_map(v, 30, 8000);
    return ch_clamp(hz, 22, 10000);
}
static void fx_comb(ch_fx *f, ch_trk *t, unsigned s, float *L, float *R, unsigned n)
{
    if (!f->mem) return;
    float hz = comb_hz(t, s), g = ch_clamp(PV(t, s, K_FB), 0, 1), mix = PV(t, s, K_MIX);
    float pol = PV(t, s, K_POL) > 0.5f ? -1.0f : 1.0f; int ff = PV(t, s, K_TYPE) > 0.5f;
    float spread = PV(t, s, K_SPREAD), damp = PV(t, s, K_DAMP);
    float a = 1 - ch_exp2(-6.2832f * ch_exp_map(1 - damp, 700, 20000) / (float)CH_RATE * 1.4427f);   /* the loop's low pass */
    float gk = ff ? g * pol : 0.985f * g * pol;
    float comp = ff ? 1 / (1 + 0.5f * g) : ch_sqrt_fast(1 - gk * gk);    /* about level on broad sound; the peaks still ring out */
    float in = 1.0f / (float)n;
    const unsigned nch = f->mono ? 1u : 2u;              /* the same sound both sides and no SPREAD: one comb, and copied (its line too) */
    for (unsigned c = 0; c < nch; ++c) {
        float *x = c ? R : L;
        int16_t *b = f->mem + c * 2048;
        uint32_t w = f->w;
        float target = ch_clamp((float)CH_RATE / (hz * (c ? 1 + 0.0595f * spread : 1)), 2, 2040);
        float d = f->acc[c] > 1 ? f->acc[c] : target, dd = (target - d) * in;
        float lp = f->lp[c];
        if (ch_fx_pre) {                                 /* a master return */
            const float *e = ch_fx_el ? (c ? ch_fx_er : ch_fx_el) : 0; const float dry = 1 - mix;
            for (unsigned i = 0; i < n; ++i) {
                float del = tap(b, 2047u, w, d), y, v = x[i] * mix; if (e) v += e[i];
                lp += (del - lp) * a;
                if (ff) { b[w & 2047u] = s16(v); y = (v + gk * lp) * comp; }
                else { float u = sat(v + gk * lp); b[w & 2047u] = s16(u); y = u * comp; }
                x[i] = x[i] * dry + y;
                d += dd; ++w;
            }
        } else
        for (unsigned i = 0; i < n; ++i) {
            float del = tap(b, 2047u, w, d), y;
            lp += (del - lp) * a;
            if (ff) { b[w & 2047u] = s16(x[i]); y = (x[i] + gk * lp) * comp; }
            else { float u = sat(x[i] + gk * lp); b[w & 2047u] = s16(u); y = u * comp; }
            x[i] += (y - x[i]) * mix;
            d += dd; ++w;
        }
        f->acc[c] = d; f->lp[c] = ch_ftz(lp);
    }
    if (f->mono) {
        for (unsigned i = 0; i < n; ++i) { R[i] = L[i]; f->mem[2048 + ((f->w + i) & 2047u)] = f->mem[(f->w + i) & 2047u]; }
        f->acc[1] = f->acc[0]; f->lp[1] = f->lp[0];
    }
    f->w += n;
}

/* ---- TAPE: wow (0.6 Hz) and flutter (5.5 Hz) on a short delay, saturation, a roll-off, hiss; the block's two lines ---- */
static void fx_tape(ch_fx *f, ch_trk *t, unsigned s, float *L, float *R, unsigned n)
{
    if (!f->mem) return;
    float wow = PV(t, s, P_WOW), drive = 1 + PV(t, s, P_SAT) * 5, comp = 1.1f / drive;   /* unity for quiet signals: the loud ones round off */
    float lp = 1 - ch_exp2(-6.2832f * ch_exp_map(PV(t, s, P_TONE), 2500, 16000) / (float)CH_RATE * 1.4427f);
    float hiss = PV(t, s, P_HISS) * PV(t, s, P_HISS) * 0.012f, mix = PV(t, s, P_MIX);
    float p1 = wrap1(f->ph + 0.6f * (float)n / (float)CH_RATE), q1 = wrap1(f->ph2 + 5.5f * (float)n / (float)CH_RATE);
    float m0 = wow * (160 * ch_sin_turns(f->ph) + 14 * ch_sin_turns(f->ph2)), m1 = wow * (160 * ch_sin_turns(p1) + 14 * ch_sin_turns(q1));
    f->ph = p1; f->ph2 = q1;
    float d = 300 + m0, dd = (m1 - m0) / (float)n;
    for (unsigned i = 0; i < n; ++i) {
        float yl = 0, yr = 0;
        for (unsigned c = 0; c < 2; ++c) {
            int16_t *b = f->mem + c * 2048;
            float *x = c ? R : L;
            b[f->w & 2047u] = s16(x[i]);
            float y = tap(b, 2047u, f->w, d);
            y = sat(y * drive) * comp;                   /* the tape's gentle saturation */
            f->u.a.z[c][0] += lp * (y - f->u.a.z[c][0]);         /* its top end */
            y = f->u.a.z[c][0] + ch_rands(&f->rng) * hiss;
            if (c) yr = y; else yl = y;
        }
        L[i] += (yl - L[i]) * mix; R[i] += (yr - R[i]) * mix;
        d += dd; ++f->w;
    }
    f->u.a.z[0][0] = ch_ftz(f->u.a.z[0][0]); f->u.a.z[1][0] = ch_ftz(f->u.a.z[1][0]);
}

/* ---- VINYL: crackle (small clicks), pops (rare, big, low), a dust bed, all through one tone; added to the sound ---- */
static void fx_vinyl(ch_fx *f, ch_trk *t, unsigned s, float *L, float *R, unsigned n)
{
    float cr = PV(t, s, V_CRACK), po = PV(t, s, V_POPS), dust = PV(t, s, V_DUST), mix = PV(t, s, V_MIX);
    uint32_t p_cr = (uint32_t)(cr * cr * 0.004f * 4294967296.0f), p_po = (uint32_t)(po * po * 0.00012f * 4294967296.0f);
    float g = ch_svf_g(ch_exp_map(PV(t, s, V_TONE), 300, 5000)), k = 1.1f;   /* the band-pass: a speaker's worth of it, not a hiss */
    float bed = dust * dust * 0.02f;
    for (unsigned i = 0; i < n; ++i) {
        uint32_t r = ch_rand(&f->rng);
        if (r < p_cr) { float a = (0.08f + 0.25f * ch_randf(&f->rng)) * (r & 1u ? 1.0f : -1.0f); f->hold[0] = a; f->acc[0] = 0.55f + 0.35f * ch_randf(&f->rng); }
        if (ch_rand(&f->rng) < p_po) { f->hold[1] = (0.4f + 0.5f * ch_randf(&f->rng)) * (r & 2u ? 1.0f : -1.0f); f->acc[1] = 0.985f + 0.012f * ch_randf(&f->rng); }
        float c = f->hold[0] + f->hold[1];
        f->hold[0] *= f->acc[0]; f->hold[1] *= f->acc[1];  /* a click dies in a few samples, a pop over ~10 ms */
        float noise = ch_rands(&f->rng) * bed;
        f->u.a.z[0][1] += 0.02f * (noise - f->u.a.z[0][1]);          /* the dust: a low rumble under the hiss */
        float y = ch_svf_tick(&f->u.a.f[0][0], c * 1.6f + noise * 0.5f, g, k).bp + f->u.a.z[0][1] * 2.5f;
        y *= mix;
        L[i] += y; R[i] += y;
    }
    f->hold[0] = ch_ftz(f->hold[0]); f->hold[1] = ch_ftz(f->hold[1]); f->u.a.z[0][1] = ch_ftz(f->u.a.z[0][1]);
    f->u.a.f[0][0].ic1 = ch_ftz(f->u.a.f[0][0].ic1); f->u.a.f[0][0].ic2 = ch_ftz(f->u.a.f[0][0].ic2);
}

/* ---- SPRING: two springs (left and right, a little apart in length), each a feedback loop of 35..70 ms through
 * twenty first-order all-passes: the dispersion that makes the chirp (high frequencies arrive later each time
 * round) and the boing on a transient. At half rate; the block's 4096 frames are the two loops' lines. ---- */
static void fx_spring(ch_fx *f, ch_trk *t, unsigned s, float *L, float *R, unsigned n)
{
    if (!f->mem) return;
    enum { D = 2, LEN = 2048, STAGES = 20 };
    float tension = PV(t, s, W_TENSION), mix = PV(t, s, W_MIX);
    const float *eL = ch_fx_el, *eR = ch_fx_er; float pre = 1; if (ch_fx_pre) { pre = mix; mix = 1; }   /* (a master effect: MIX is what of the mix goes in, the sends go in whole, and what comes out is heard whole) */
    float a = 0.45f + 0.4f * tension;                      /* the all-pass coefficient: more, a longer chirp */
    float fb = 0.5f + 0.46f * PV(t, s, W_DECAY);
    float lp = 1 - ch_exp2(-6.2832f * ch_exp_map(PV(t, s, W_TONE), 700, 5000) / ((float)CH_RATE / D) * 1.4427f);
    float base = 1540 - 770 * tension;                     /* the loop in line frames: a tighter spring is shorter */
    for (unsigned i = 0; i < n; ++i) {
        f->acc[0] += (L[i] + R[i]) * pre; if (eL) f->acc[0] += eL[i] + eR[i];
        if (DECIM_STEP(D)) {
            f->dn = 0;
            float x = f->acc[0] * (0.5f / D); f->acc[0] = 0;
            for (unsigned c = 0; c < 2; ++c) {
                int16_t *b = f->mem + c * LEN;
                float dl = base * (c ? 1.0f : 0.91f);
                float y = tap(b, LEN - 1u, f->wi[c], dl);
                f->fb[c] += lp * (y - f->fb[c]); y = f->fb[c];                       /* the loop darkens */
                f->lp[c] += 0.002f * (y - f->lp[c]); y -= f->lp[c];                  /* no DC build-up */
                float v = x + fb * y, *z = f->u.sp[c];
                for (unsigned k = 0; k < STAGES; ++k) { float o = -a * v + z[k]; z[k] = v + a * o; v = o; }
                uint32_t wi = (f->wi[c] + 1u) & (LEN - 1u);
                b[wi] = s16(sat(v) * 0.9f);
                f->wi[c] = (uint16_t)wi;
                f->prev[c] = f->cur[c]; f->cur[c] = y;
            }
        }
        float fr = (float)f->dn * (1.0f / D);
        float yl = f->prev[0] + (f->cur[0] - f->prev[0]) * fr, yr = f->prev[1] + (f->cur[1] - f->prev[1]) * fr;
        L[i] += (yl * 0.85f + yr * 0.35f) * mix; R[i] += (yr * 0.85f + yl * 0.35f) * mix;
    }
    for (unsigned c = 0; c < 2; ++c) { for (unsigned k = 0; k < STAGES; ++k) f->u.sp[c][k] = ch_ftz(f->u.sp[c][k]); f->fb[c] = ch_ftz(f->fb[c]); f->lp[c] = ch_ftz(f->lp[c]); }
}

/* ---- REPEAT: a beat repeater. The input goes into a 16384-frame mono ring at half rate. On the sequencer's beat
 * clock (its own when stopped), at the start of every EVERY (a beat, half a bar, one, two or four bars) a WINDOW
 * starts: its first pass is live while it is captured, then it plays REPEATS times, each pass a little quieter and
 * darker by DECAY, and only for GATE of the window's length (100 %: the whole of it, lower: a shorter chop and a rest).
 * CHANCE: how likely each start is taken (100 %: every one). HOLD keeps the current window looping. OFFSET moves the
 * whole grid (the starts and the windows with them) later by sixteenths: 4 at EVERY a bar starts on beat two. ---- */
static const float REP_BEATS[6] = {0.125f, 1.0f / 6, 0.25f, 1.0f / 3, 0.5f, 1.0f};
static const float REP_EVERY[5] = {1, 2, 4, 8, 16};
static void fx_repeat(ch_fx *f, ch_trk *t, unsigned s, float *L, float *R, unsigned n)
{
    if (!f->mem) return;
    enum { D = 2, LEN = 4 * CH_FX_BLOCK };
    unsigned div = (unsigned)(PV(t, s, M_DIV) + 0.5f); if (div > 5) div = 5;
    unsigned ev = (unsigned)(PV(t, s, M_EVERY) + 0.5f); if (ev > 4) ev = 4;
    int per = (int)(REP_EVERY[ev] / REP_BEATS[div] + 0.5f);                                     /* windows in an EVERY */
    float chance = PV(t, s, M_CHANCE), decay = PV(t, s, M_DECAY), gate = ch_clamp(PV(t, s, M_GATE), 0, 1), mix = PV(t, s, M_MIX);
    int reps = 1 + (int)(PV(t, s, M_LENGTH) * 7.999f), hold = PV(t, s, M_HOLD) > 0.5f;
    float bpm = ch_t.bpm > 1 ? ch_t.bpm : 120, bpf = bpm / (60.0f * CH_RATE);
    float slice = ch_min(REP_BEATS[div] * 60.0f / bpm * ((float)CH_RATE / D), (float)LEN - 4);   /* line frames a division */
    float open = gate > 0.995f ? slice + 1 : slice * (0.1f + 0.9f * gate);                      /* line frames of a pass that sound */
    int16_t *b = f->mem;
    if (!f->rng.s) f->rng.s = 0x9E3779B9u;
    /* the beat: the clock's while it runs (ph2 keeps where it left off), else our own */
    float beat = ch_t.playing ? (float)ch_t.beat : f->ph2;
    unsigned ofs = (unsigned)(PV(t, s, M_OFFSET) + 0.5f); if (ofs > 15) ofs = 15;
    float shift = 16.0f - (float)ofs * 0.25f;                /* (16 beats: the longest EVERY, so the grid only moves by the offset) */
    for (unsigned i = 0; i < n; ++i) {
        float x = (L[i] + R[i]) * 0.5f;
        float pos = (beat + shift) / REP_BEATS[div];                   /* in divisions */
        int k = (int)pos;
        if (k != (int)f->t_l) {                              /* a division boundary */
            f->t_l = (float)k;
            if (f->prev[0] > 0) {                            /* repeating: a pass done */
                if (f->cur[0] >= 0 && !hold) { f->fb[0] *= 1 - 0.55f * decay; f->lp[1] *= 1 - 0.3f * decay; }   /* (held: it stays) */
                f->prev[0] -= 1; f->cur[0] = 0;              /* (the read goes back to the slice's start: it stays tight) */
                if (hold && f->prev[0] <= 0) f->prev[0] = 1;
            }
            if (hold ? f->prev[0] <= 0 : ((k % per) + per) % per == 0) {   /* the start of an EVERY (held: whenever nothing loops) */
                if (hold || ch_randf(&f->rng) < chance) {    /* start one: this pass is the capture (one still running is cut) */
                    f->prev[0] = (float)reps + 0.5f; f->cur[1] = (float)f->w; f->cur[0] = -1; f->fb[0] = 1; f->lp[1] = 1;
                } else f->prev[0] = 0;
            }
        }
        /* the ring, at half rate */
        f->acc[0] += x;
        if (DECIM_STEP(D)) { f->dn = 0; b[f->w % LEN] = s16(f->acc[0] * (1.0f / D)); ++f->w; f->acc[0] = 0; }
        float wet = x, g = 1;
        if (f->prev[0] > 0 && f->cur[0] >= 0) {              /* a repeat pass: read the captured slice */
            float rp = f->cur[0]; if (rp >= slice) rp -= slice;
            float p = f->cur[1] + rp; uint32_t ia = (uint32_t)p % LEN, ib = (ia + 1u) % LEN; float fr = p - (float)(int32_t)p;
            float v = (f16(b[ia]) + (f16(b[ib]) - f16(b[ia])) * fr) * f->fb[0];
            f->lp[0] += (v - f->lp[0]) * (0.15f + 0.85f * f->lp[1]);           /* darker each pass */
            wet = f->lp[0];
            g = rp < open ? 1 : 0;                           /* GATE: the pass's first part sounds, the rest rests */
            f->cur[0] = rp + 1.0f / D;
        }
        f->hold[0] += (g - f->hold[0]) * (1.0f / 48); if (f->hold[0] < 0.0005f) f->hold[0] = 0;   /* (its edge, 48 frames) */
        wet *= f->hold[0];
        L[i] += (wet - L[i]) * mix; R[i] += (wet - R[i]) * mix;
        beat += bpf;
    }
    f->ph2 = beat;
    f->lp[0] = ch_ftz(f->lp[0]);
}

/* ---- TRANCE: a step gate. A pattern of up to sixteen steps, each on or off, runs on the sequencer's beat (its own
 * when stopped) at RATE a step; an on step is open for GATE of its length (at 100 %, neighbours run together), and
 * the level falls by DEPTH while it is shut. SMOOTH rounds the edges, from a hard chop (never a click: 0.7 ms) to a
 * soft pump. No memory, a few sums a frame ---- */
enum { TG_RATE, TG_GATE, TG_SMOOTH, TG_DEPTH, TG_S1, TG_S2, TG_S3, TG_S4, TG_LEN };
static void fx_trance(ch_fx *f, ch_trk *t, unsigned s, float *L, float *R, unsigned n)
{
    unsigned div = (unsigned)(PV(t, s, TG_RATE) + 0.5f); if (div > 5) div = 5;
    unsigned len = 1u + (unsigned)(PV(t, s, TG_LEN) + 0.5f); if (len > 16) len = 16;
    unsigned pat = 0;
    for (unsigned k = 0; k < 4; ++k) pat |= ((unsigned)(PV(t, s, TG_S1 + k) + 0.5f) & 15u) << (4 * k);
    float gate = PV(t, s, TG_GATE), duty = gate > 0.995f ? 2 : 0.05f + 0.95f * gate, depth = ch_clamp(PV(t, s, TG_DEPTH), 0, 1);
    float sm = PV(t, s, TG_SMOOTH), kf = 1 - ch_exp2(-1.4427f / ((0.0007f + 0.02f * sm * sm) * (float)CH_RATE));
    float bpm = ch_t.bpm > 1 ? ch_t.bpm : 120, bpf = bpm / (60.0f * CH_RATE), inv = 1 / REP_BEATS[div];
    float beat = ch_t.playing ? (float)ch_t.beat : f->ph2;
    float g1 = f->hold[0], g = f->hold[1];
    for (unsigned i = 0; i < n; ++i) {
        float pos = beat * inv;
        unsigned k = (unsigned)pos; float frac = pos - (float)k;
        float on = (pat >> (k % len) & 1u) && frac < duty ? 1.0f : 0.0f;
        g1 += (on - g1) * kf; g += (g1 - g) * kf;          /* two one-poles: no corner */
        float a = 1 - depth * (1 - g);
        L[i] *= a; R[i] *= a;
        beat += bpf;
    }
    f->ph2 = beat;
    f->hold[0] = ch_ftz(g1); f->hold[1] = ch_ftz(g);
}

/* ---- REVERB: four lines in a feedback delay network (Hadamard), two diffusers before, at half rate ----
 * TYPE is the same network set four ways (the lines, the memory and the work are the same):
 *   HALL   as it always was
 *   ROOM   its lines under half as long and its decay 0.1 to 2.5 s: close walls, early and dense
 *   PLATE  the lines drawn together and the diffusers harder (0.74): no room to hear, a smooth bright sheet; DAMP takes less off
 *   GATED  a HALL whose tail is shut off once the dry sound has been quiet for DECAY (40 ms to 0.8 s): the big snare ---- */
static const uint16_t RV_BASE[4] = {1123, 1381, 1621, 1949};   /* frames at 22.05 kHz, at SIZE 0.4 or so */
static const uint16_t RV_AP[2] = {142, 379};
static void rv_layout(uint16_t cap[6], uint16_t off[6])
{
    uint16_t o = 0;
    for (unsigned k = 0; k < 4; ++k) { cap[k] = (uint16_t)(RV_BASE[k] * 2 + 4); off[k] = o; o = (uint16_t)(o + cap[k]); }
    for (unsigned k = 0; k < 2; ++k) { cap[4 + k] = RV_AP[k]; off[4 + k] = o; o = (uint16_t)(o + cap[4 + k]); }
}
static void fx_reverb(ch_fx *f, ch_trk *t, unsigned s, float *L, float *R, unsigned n)
{
    if (!f->mem) return;
    enum { D = 2 };
    uint16_t cap[6], off[6];
    rv_layout(cap, off);
    const unsigned type = (unsigned)ch_clamp(PV(t, s, 5) + 0.5f, 0, 3);   /* TYPE */
    const float sz = ch_clamp(PV(t, s, R_SIZE), 0, 1);
    float size = type == 1 ? 0.16f + 0.62f * sz : 0.35f + 1.6f * sz;
    float t60 = type == 1 ? ch_exp_map(PV(t, s, R_DECAY), 0.1f, 2.5f) : type == 3 ? 6.0f : ch_exp_map(PV(t, s, R_DECAY), 0.3f, 12);
    float damp = 1 - (type == 2 ? 0.5f : 0.85f) * PV(t, s, R_DAMP), mix = PV(t, s, R_MIX);
    const float dif = type == 2 ? 0.74f : 0.6f, squeeze = type == 2 ? 0.55f : 1.0f;   /* PLATE: denser, and its four lines nearer one length */
    const float gate_hold = ch_exp_map(PV(t, s, R_DECAY), 0.04f, 0.8f) * (float)CH_RATE;   /* GATED: how long after the dry sound the tail stays */
    const float *eL = ch_fx_el, *eR = ch_fx_er; float pre = 1; if (ch_fx_pre) { pre = mix; mix = 1; }   /* (a master effect: MIX is what of the mix goes in, the sends go in whole, and what comes out is heard whole) */
    float len[4], g[4];
    for (unsigned k = 0; k < 4; ++k) {
        len[k] = ch_min(((float)RV_BASE[0] + ((float)RV_BASE[k] - (float)RV_BASE[0]) * squeeze) * size, (float)(cap[k] - 3));
        g[k] = ch_exp2(-9.966f * len[k] / (t60 * ((float)CH_RATE / D)));   /* -60 dB in t60 */
    }
    int16_t *m = f->mem;
    /* DUCK: a follower on the dry sound (attack 5 ms, release 250 ms) takes the reverb down by up to DUCK x 30 dB, the
     * louder the dry the further; f->hold[0] the follower, [1] the gain, smoothed so it never clicks */
    float duck = PV(t, s, R_DUCK), ka = 1 - ch_exp2(-1.4427f / (0.005f * (float)CH_RATE)), kr = 1 - ch_exp2(-1.4427f / (0.25f * (float)CH_RATE));
    /* the reverb's running values in locals for the block (each was a read and a write of the effect's memory every
     * sample, and a write into a line made the chip read the write places again): the same sums, a third less time */
    float h0_ = f->hold[0], h1_ = f->hold[1], ac0 = f->acc[0], ac1 = f->acc[1], pv0 = f->prev[0], pv1 = f->prev[1], cu0 = f->cur[0], cu1 = f->cur[1];
    float lp_[4] = {f->lp[0], f->lp[1], f->lp[2], f->lp[3]};
    uint16_t wi_[6] = {f->wi[0], f->wi[1], f->wi[2], f->wi[3], f->wi[4], f->wi[5]};
    unsigned dn_ = f->dn;
    float gt_ = f->fb[0], gh_ = f->fb[1];                 /* GATED: the gate's gain, and the frames it has left to stay open */
    if (type != 3) gt_ = 1;
    for (unsigned i = 0; i < n; ++i) {
        if (duck > 0.001f || type == 3) {
            float a = ch_max(ch_abs(L[i]), ch_abs(R[i])), e = h0_;
            e += (a - e) * (a > e ? ka : kr); h0_ = e;
            if (type == 3) {                             /* open while the dry sound is up, held, then shut over 12 ms */
                if (a > 0.02f) gh_ = gate_hold; else if (gh_ > 0) gh_ -= 1;
                gt_ += ((gh_ > 0 ? 1.0f : 0.0f) - gt_) * (gh_ > 0 ? 0.05f : 0.002f);
            }
            float q = ch_min(e * 2.5f, 1); q *= q;          /* how loud the dry is: full from about -8 dB, a quiet tail hardly at all */
            float want = duck > 0.001f ? ch_exp2(-duck * 5.0f * q) : 1;   /* up to 30 dB down, in even steps of the knob */
            h1_ += (want - h1_) * ka;
        } else h1_ = 1;
        ac0 += L[i] * pre; ac1 += R[i] * pre;
        if (eL) { ac0 += eL[i]; ac1 += eR[i]; }
        if (++dn_ >= D) {
            dn_ = 0;
            float x = (ac0 + ac1) * (0.5f / D); ac0 = ac1 = 0;
            /* the diffusers: two Schroeder all-passes */
            for (unsigned a = 0; a < 2; ++a) {
                int16_t *ap = m + off[4 + a];
                uint16_t wi = wi_[4 + a];
                float d = f16(ap[wi]), o = d - dif * x;
                ap[wi] = s16(x + dif * o);
                wi_[4 + a] = (uint16_t)(wi + 1u >= cap[4 + a] ? 0 : wi + 1u);
                x = o;
            }
            float y[4];
            for (unsigned k = 0; k < 4; ++k) {
                int16_t *ln = m + off[k];
                float rp = (float)wi_[k] - len[k];
                if (rp < 0) rp += (float)cap[k];
                int32_t ri = (int32_t)rp; float fr = rp - (float)ri;
                uint16_t r1 = (uint16_t)(ri + 1 >= cap[k] ? 0 : ri + 1);
                float v = f16(ln[ri]) + (f16(ln[r1]) - f16(ln[ri])) * fr;
                lp_[k] += damp * (v - lp_[k]);
                y[k] = lp_[k];
            }
            float h0 = 0.5f * (y[0] + y[1] + y[2] + y[3]), h1 = 0.5f * (y[0] - y[1] + y[2] - y[3]);
            float h2 = 0.5f * (y[0] + y[1] - y[2] - y[3]), h3 = 0.5f * (y[0] - y[1] - y[2] + y[3]);
            float hh[4] = {h0, h1, h2, h3}, sg[4] = {1, -1, 1, -1};
            for (unsigned k = 0; k < 4; ++k) {
                int16_t *ln = m + off[k];
                ln[wi_[k]] = s16(x * sg[k] * 0.5f + g[k] * hh[k]);
                wi_[k] = (uint16_t)(wi_[k] + 1u >= cap[k] ? 0 : wi_[k] + 1u);
            }
            pv0 = cu0; pv1 = cu1;
            cu0 = (y[0] + y[2]) * 0.7f; cu1 = (y[1] + y[3]) * 0.7f;
        }
        float fr = (float)dn_ * (1.0f / D);
        float wl = pv0 + (cu0 - pv0) * fr, wr = pv1 + (cu1 - pv1) * fr;
        float dg = mix * h1_ * gt_;
        L[i] += wl * dg; R[i] += wr * dg;                 /* (the dry sound kept whole: the reverb adds, ducked under it) */
    }
    f->hold[0] = h0_; f->hold[1] = h1_; f->acc[0] = ac0; f->acc[1] = ac1; f->prev[0] = pv0; f->prev[1] = pv1; f->cur[0] = cu0; f->cur[1] = cu1;
    for (unsigned k = 0; k < 4; ++k) f->lp[k] = lp_[k];
    for (unsigned k = 0; k < 6; ++k) f->wi[k] = wi_[k];
    f->dn = (uint8_t)dn_;
    f->fb[0] = ch_ftz(gt_); f->fb[1] = gh_;
    for (unsigned k = 0; k < 4; ++k) f->lp[k] = ch_ftz(f->lp[k]);
}

/* ---- CRYSTAL: the crystallizer. The sound is cut into grains a TIME long and each grain played back, REVERSE'd or not,
 * transposed by PITCH, two at a time half a grain apart under smooth windows; what comes out is fed back (FEEDBACK), so
 * each repeat is transposed again: a reversed octave climbing into shimmer. SPREAD sends the two grains apart, left and
 * right. Mono in, at half rate; the block's four lines are one 16384-frame line (a grain at most 0.25 s, so a reversed
 * octave up still reads inside it) ---- */
enum { X_PITCH, X_TIME, X_FB, X_MIX, X_REV, X_SPREAD };
static void fx_crystal(ch_fx *f, ch_trk *t, unsigned s, float *L, float *R, unsigned n)
{
    if (!f->mem) return;
    enum { D = 2, LEN = 16384 };
    float st = (float)(int)(PV(t, s, X_PITCH) + 0.5f) - 12.0f, r = ch_exp2(st * (1.0f / 12));
    int rev = PV(t, s, X_REV) > 0.5f;
    float gmax = rev ? ((float)LEN - 16) / (1 + r) : ((float)LEN - 16) / (r > 1 ? r : 1);
    float G = ch_clamp(dly_frames(t, s, X_TIME, (float)CH_RATE / D, 0.25f), 64, gmax);
    float fb = PV(t, s, X_FB) * 0.92f, mix = PV(t, s, X_MIX), sp = PV(t, s, X_SPREAD);
    const float *eL = ch_fx_el, *eR = ch_fx_er; float pre = 1; if (ch_fx_pre) { pre = mix; mix = 1; }   /* (a master effect: MIX is what of the mix goes in, the sends go in whole, and what comes out is heard whole) */
    int16_t *b = f->mem;
    float *age = f->hold, *start = f->lp, *glen = f->lp + 2;   /* each grain: its age, where it starts in the line, its length */
    if (f->t_l <= 0) { f->t_l = 1; age[0] = 0; age[1] = G * 0.5f; glen[0] = glen[1] = G; start[0] = start[1] = 0; }
    for (unsigned i = 0; i < n; ++i) {
        f->acc[0] += (L[i] + R[i]) * pre; if (eL) f->acc[0] += eL[i] + eR[i];
        if (DECIM_STEP(D)) {
            f->dn = 0;
            float x = f->acc[0] * (0.5f / D); f->acc[0] = 0;
            float g[2];
            for (unsigned k = 0; k < 2; ++k) {
                if (age[k] >= glen[k]) {                     /* a new grain: from the newest sound (reversed), or far enough back to read forward */
                    age[k] -= glen[k]; if (age[k] >= G) age[k] = 0;
                    glen[k] = G;
                    float back = rev ? 0.0f : (r > 1 ? (r - 1) * G : 0.0f) + 4;
                    start[k] = (float)(f->w & (LEN - 1u)) - back;
                }
                float pos = rev ? start[k] - r * age[k] : start[k] + r * age[k];
                float d = (float)(f->w & (LEN - 1u)) - pos;   /* how far back that is now */
                while (d < 1) d += (float)LEN; while (d >= (float)LEN - 2) d -= (float)LEN;
                float wv = 0.5f - 0.5f * ch_sin_turns(age[k] / glen[k] + 0.25f);   /* sin^2 (1 - cos over the grain): the two, half a grain apart, sum to one */
                g[k] = tap(b, LEN - 1u, f->w, d) * wv;
                age[k] += 1;
            }
            float y = g[0] + g[1];
            uint32_t wi = (f->w + 1u) & (LEN - 1u);
            b[wi] = s16(sat(x + fb * y));
            f->w = wi;
            f->prev[0] = f->cur[0]; f->prev[1] = f->cur[1];
            f->cur[0] = g[0] + (1 - sp) * g[1]; f->cur[1] = g[1] + (1 - sp) * g[0];
        }
        float fr = (float)f->dn * (1.0f / D);
        L[i] += (f->prev[0] + (f->cur[0] - f->prev[0]) * fr) * mix;
        R[i] += (f->prev[1] + (f->cur[1] - f->prev[1]) * fr) * mix;
    }
}

/* ch_fx_return: the slot's effect can be a send's return (the ones that add their sound to the dry, whole: the delays
 * and reverbs); *mix its MIX now */
int ch_fx_return(const ch_trk *t, unsigned s, float *mix)
{
    unsigned d = t->device[s];
    if (!fx_is_return(d)) return 0;
    *mix = PV(t, s, (unsigned)mix_param(d));
    return 1;
}
/* For the app's drawings of the effects (the sound uses none of these): an effect's sweep where it is now, a delay's
 * time in seconds, the comb's and the resonator's pitch, DRIVE's curve at a setting */
static const ch_trk *draw_trk(unsigned track) { return track < CH_TRACKS ? &ch_tr[track] : &ch_mst; }
float ch_fx_phase(unsigned track, unsigned slot) { return slot >= CH_FX1 && slot < CH_SLOTS ? draw_trk(track)->fx[slot - CH_FX1].ph : 0; }
float ch_fx_delay_sec(unsigned dev, float v, unsigned mode)
{
    if (dev == CH_DEV_ECHO) return ch_exp_map(v, 0.05f, 1.4f);
    float maxs = dev == CH_DEV_DELAY ? 0.74f : dev == CH_DEV_CRYSTAL ? 0.25f : 1.45f;
    float sec = mode ? ch_exp_map(v, 0.02f, maxs) : DLY_BEATS[dly_div(v)] * 60.0f / (ch_t.bpm > 1 ? ch_t.bpm : 120);
    return ch_min(sec, maxs);
}
float ch_fx_comb_hz(unsigned track, unsigned slot) { return slot >= CH_FX1 && slot < CH_SLOTS ? comb_hz(draw_trk(track), slot) : 440; }
float ch_fx_reso_hz(float v) { return 440 * ch_exp2((reso_note(v) - 69) * (1.0f / 12)); }
float ch_fx_drive_curve(unsigned type, unsigned clip, float drive, float x)
{
    float g = ch_exp2(drive * 30.0f * 0.16609640f);
    float r = (g * ch_sqrt_fast(g) + 1.9953f) * (CH_PI / 10), rn = 1 / ch_sin_turns(ch_clamp(r, 0.01f, CH_PI * 0.5f) * (0.5f / CH_PI));
    return fz_clip(clip > 2 ? 0 : clip, fz_curve(type > 4 ? 0 : type, x, g, r, rn));
}
void ch_fx_process(ch_trk *t, unsigned s, float *L, float *R, unsigned n)
{
    unsigned k = s - CH_FX1;
    ch_fx *f = &t->fx[k];
    if (f->unclean) return;                              /* its lines not yet clean: dry */
    if (!f->rng.s) f->rng.s = 0x1D872B41u ^ (uint32_t)(k * 0x9E3779B1u) ^ (uint32_t)(t->device[s] * 7919u);
    { int mp = mix_param(t->device[s]); if (mp >= 0 && PV(t, s, (unsigned)mp) < 0.001f && !ch_fx_pre) return; }   /* MIX at 0: nothing (a master return: its sends and its tail still) */
    switch (t->device[s]) {
    case CH_DEV_FILTER: fx_filter(f, t, s, L, R, n); break;
    case CH_DEV_DRIVE: fx_drive(f, t, s, L, R, n); break;
    case CH_DEV_CHORUS: fx_sweep(f, t, s, L, R, n, 0); break;
    case CH_DEV_FLANGER: fx_sweep(f, t, s, L, R, n, 1); break;
    case CH_DEV_PHASER: fx_phaser(f, t, s, L, R, n); break;
    case CH_DEV_TREMOLO: fx_tremolo(f, t, s, L, R, n); break;
    case CH_DEV_DELAY: fx_delay(f, t, s, L, R, n); break;
    case CH_DEV_ECHO: fx_echo(f, t, s, L, R, n); break;
    case CH_DEV_DUB: fx_dub(f, t, s, L, R, n); break;
    case CH_DEV_REVERB: fx_reverb(f, t, s, L, R, n); break;
    case CH_DEV_DJ: fx_dj(f, t, s, L, R, n); break;
    case CH_DEV_SIDE: fx_side(f, t, s, L, R, n); break;
    case CH_DEV_RESO: fx_reso(f, t, s, L, R, n); break;
    case CH_DEV_COMB: fx_comb(f, t, s, L, R, n); break;
    case CH_DEV_TAPE_FX: fx_tape(f, t, s, L, R, n); break;
    case CH_DEV_VINYL: fx_vinyl(f, t, s, L, R, n); break;
    case CH_DEV_SPRING: fx_spring(f, t, s, L, R, n); break;
    case CH_DEV_REPEAT: fx_repeat(f, t, s, L, R, n); break;
    case CH_DEV_CRYSTAL: fx_crystal(f, t, s, L, R, n); break;
    case CH_DEV_TRANCE: fx_trance(f, t, s, L, R, n); break;
    case CH_DEV_ERODE: fx_erode(f, t, s, L, R, n); break;
    case CH_DEV_DISPERSE: fx_disperse(f, t, s, L, R, n); break;
    case CH_DEV_UNISON: fx_unison(f, t, s, L, R, n); break;
    default: break;
    }
}

/* the screen's words for a value: 1 = written (out: at least 12 chars) */
static void num(char *o, float v, int dec)
{
    int neg = v < 0; if (neg) v = -v;
    int32_t ip = (int32_t)v, fp = dec ? (int32_t)((v - (float)ip) * 10 + 0.5f) : 0;
    if (fp >= 10) { ++ip; fp = 0; }
    char b[12]; int k = 0;
    do { b[k++] = (char)('0' + ip % 10); ip /= 10; } while (ip && k < 10);
    int j = 0; if (neg) o[j++] = '-';
    while (k) o[j++] = b[--k];
    if (dec) { o[j++] = '.'; o[j++] = (char)('0' + fp); }
    o[j] = 0;
}
static void cat(char *o, const char *s) { while (*o) ++o; while (*s) *o++ = *s++; *o = 0; }
static void hz_text(float hz, char *out)
{
    if (hz >= 1000) { num(out, hz / 1000, hz < 10000); cat(out, "K"); }
    else if (hz >= 10) { num(out, hz, 0); cat(out, "HZ"); }
    else { num(out, hz, 1); cat(out, "HZ"); }
}
int ch_fx_amount_param(unsigned dev) { return dev == CH_DEV_DJ ? (int)J_AMOUNT : mix_param(dev); }
static void ms_text(float sec, char *out) { num(out, sec * 1000, 0); cat(out, "MS"); }
int ch_fx_value_text(unsigned dev, unsigned p, float v, unsigned mode, char *out)
{
    out[0] = 0;
    if ((dev == CH_DEV_DELAY && p == Y_TIME) || (dev == CH_DEV_DUB && p == U_TIME) || (dev == CH_DEV_CRYSTAL && p == 1)) {
        if (mode) ms_text(ch_exp_map(v, 0.02f, dev == CH_DEV_DELAY ? 0.74f : dev == CH_DEV_CRYSTAL ? 0.25f : 1.45f), out); else cat(out, DLY_NAME[dly_div(v)]);
        return 1;
    }
    switch (dev) {
    case CH_DEV_SIDE:
        if (p == S_SRC && v >= (float)CH_TRACKS - 0.5f) {     /* a lane: the track, then its drum's first word (not a KIT: the lane's number) */
            unsigned i = (unsigned)(v + 0.5f) - CH_TRACKS, tr = i / CH_KIT_PARTS % CH_TRACKS, ln = i % CH_KIT_PARTS;
            const ch_track_state *ts = ch_track_get(tr);
            char *o = out; *o++ = 'T'; *o++ = (char)('1' + tr); *o++ = ' ';
            if (ts->device[CH_MATERIAL] == CH_DEV_KIT) {
                unsigned m = (unsigned)(ts->param[CH_KIT_XROW][ln] + 0.5f); const char *nm = ch_drum_names[m < CH_DRUM_MODELS ? m : 0];
                for (unsigned k = 0; nm[k] && nm[k] != ' ' && k < 8; ++k) *o++ = nm[k];
            } else { *o++ = 'L'; *o++ = (char)('1' + ln); }
            *o = 0; return 1;
        }
        if (p == S_ATT) { num(out, ch_exp_map(v, 0.001f, 0.1f) * 1000, 0); cat(out, "MS"); return 1; }
        if (p == S_REL) { num(out, ch_exp_map(v, 0.008f, 1.5f) * 1000, 0); cat(out, "MS"); return 1; }
        break;
    case CH_DEV_FILTER: if (p == F_CUT) { hz_text(ch_exp_map(v, 20, 18000), out); return 1; } break;
    case CH_DEV_DRIVE: if (p == D_DRIVE) { num(out, v * 30, 0); cat(out, "DB"); return 1; } break;
    case CH_DEV_CHORUS: if (p == C_RATE) { hz_text(ch_exp_map(v, 0.05f, 5), out); return 1; } break;
    case CH_DEV_ERODE: if (p == O_FREQ) { hz_text(ch_fx_erode_hz(v), out); return 1; } break;
    case CH_DEV_UNISON: if (p == 0) { num(out, ch_fx_unison_cents(v), 0); cat(out, "CT"); return 1; } break;
    case CH_DEV_DISPERSE:
        if (p == H_FREQ) { hz_text(ch_fx_disp_hz(v), out); return 1; }
        if (p == H_AMT) { num(out, (float)ch_fx_disp_stages(v), 0); return 1; }
        break;
    case CH_DEV_FLANGER: if (p == L_RATE) { hz_text(ch_exp_map(v, 0.03f, 3), out); return 1; } break;
    case CH_DEV_PHASER: if (p == L_RATE) { hz_text(ch_exp_map(v, 0.03f, 4), out); return 1; } break;
    case CH_DEV_TREMOLO: if (p == T_RATE) { hz_text(ch_exp_map(v, 0.2f, 16), out); return 1; } break;
    case CH_DEV_DUB: if (p == U_CUT) { hz_text(ch_exp_map(v, 100, 3000), out); return 1; } break;
    case CH_DEV_ECHO: if (p == E_TIME) { num(out, ch_exp_map(v, 0.05f, 1.4f) * 1000, 0); cat(out, "MS"); return 1; } break;
    case CH_DEV_DJ:
        if (p == J_AMOUNT) {
            if (v > -0.01f && v < 0.01f) { cat(out, "FLAT"); return 1; }
            cat(out, v < 0 ? "LP " : "HP "); char h[12]; hz_text(v < 0 ? ch_exp_map(1 + v, 60, 18000) : ch_exp_map(v, 20, 7000), h); cat(out, h); return 1;
        }
        break;
    case CH_DEV_REPEAT:
        if (p == M_LENGTH) { cat(out, "X"); num(out + 1, (float)(1 + (int)(v * 7.999f)), 0); return 1; }
        break;
    case CH_DEV_COMB:
        if (p == 0) {
            if (!mode) { hz_text(ch_exp_map(v, 30, 8000), out); return 1; }
            static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            int note = (int)(reso_note(v) + 0.5f);
            cat(out, NN[((note % 12) + 12) % 12]); char o[8]; num(o, (float)(note / 12 - 1), 0); cat(out, o);
            if (mode == 2) cat(out, " FOLLOW"); else if (ch_key_mask) cat(out, " KEY");
            return 1;
        }
        break;
    case CH_DEV_TAPE_FX: if (p == P_TONE) { hz_text(ch_exp_map(v, 2500, 16000), out); return 1; } break;
    case CH_DEV_VINYL: if (p == V_TONE) { hz_text(ch_exp_map(v, 300, 5000), out); return 1; } break;
    case CH_DEV_RESO:
        if (p == Z_PITCH) {
            static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            int note = (int)(reso_note(v) + 0.5f);
            cat(out, NN[((note % 12) + 12) % 12]); char o[8]; num(o, (float)(note / 12 - 1), 0); cat(out, o);
            if (ch_key_mask) cat(out, " KEY");
            return 1;
        }
        break;
    case CH_DEV_REVERB: if (p == R_DECAY) { num(out, ch_exp_map(v, 0.3f, 12), 1); cat(out, "S"); return 1; } break;
    default: break;
    }
    return 0;
}
