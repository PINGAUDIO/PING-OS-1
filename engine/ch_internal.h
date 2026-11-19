/* Engine internals shared by the device files. Audio side only. */
#ifndef CH_INTERNAL_H
#define CH_INTERNAL_H
#include "chisel.h"
#include "ch_dsp.h"
#include "ch_adpcm.h"
#include "ch_fe.h"
#include "ch_mi.h"

enum { CH_SCALES = 13, CH_GRID = 9, CH_DIVS = 15 };   /* DIVS: 8 bars .. 1/128 (ch_div_names) */
extern const uint16_t ch_scale_mask[CH_SCALES];
extern const float ch_grid_beats[CH_GRID];
extern const float ch_div_beats[CH_DIVS];
extern const char *const ch_div_names[CH_DIVS];
float ch_division_beats(float v, unsigned mode);
float ch_scale_quantize(float semis, int scale);
extern volatile uint16_t ch_key_mask;      /* the master key, 0 none (ch_key_set) */
extern volatile uint8_t ch_key_root;
float ch_key_quantize(float semis, unsigned root);   /* nearest note in the key, counted from `root` */

typedef struct { float l, r; } ch_lr;

/* Time positions (beats, fractional frames). The FM-1's FPU is single precision and fm1-base refuses soft-double
 * code, so there they are float: the beat clock sums with compensation, DISC holds its own phase between resyncs. */
#if defined(CHISEL_FM1)
typedef float ch_tf;
#else
typedef double ch_tf;
#endif

/* Transport: beats advance while playing; sample counter always runs. */
typedef struct {
    ch_tf beat;            /* at the start of the current sub-block */
    ch_tf beat_c;          /* the sum's lost low bits (Kahan) */
    float bpm, bpm_target;
    float beats_per_frame;
    uint8_t playing, started; /* started: playback began this block */
} ch_time;
extern ch_time ch_t;

/* ---- sample reading ---- */
typedef struct {
    const ch_sample *s;
    int32_t blk[2];
    int16_t pcm[2][CH_ADPCM_BLOCK];
    uint8_t lru;
} ch_reader;
static inline void ch_reader_reset(ch_reader *r, const ch_sample *s) { r->s = s; r->blk[0] = r->blk[1] = -1; r->lru = 0; }
/* Sample i (0 <= i < frames) as float. */
static inline float ch_reader_at(ch_reader *r, int32_t i)
{
    int32_t b = i / (int32_t)CH_ADPCM_BLOCK;
    unsigned o = (unsigned)(i - b * (int32_t)CH_ADPCM_BLOCK);
    if (r->blk[0] == b) return r->pcm[0][o] * (1.0f / 32768);
    if (r->blk[1] == b) return r->pcm[1][o] * (1.0f / 32768);
    unsigned k = r->lru; r->lru ^= 1;
    ch_adpcm_decode_block(r->s->adpcm + (size_t)b * CH_ADPCM_BLOCK_BYTES, r->pcm[k]);
    r->blk[k] = b;
    return r->pcm[k][o] * (1.0f / 32768);
}
/* Wrapped (loop) or clamped (silence outside) read. */
static inline float ch_reader_wrap(ch_reader *r, int32_t i)
{
    int32_t n = (int32_t)r->s->frames;
    i %= n; if (i < 0) i += n;
    return ch_reader_at(r, i);
}
static inline float ch_reader_clip(ch_reader *r, int32_t i)
{
    return i < 0 || i >= (int32_t)r->s->frames ? 0.0f : ch_reader_at(r, i);
}
/* Cubic read at a fractional position; wrap selects the edge rule. */
static inline float ch_reader_cubic(ch_reader *r, float pos, int wrap)
{
    int32_t i = (int32_t)pos; if (pos < 0 && (float)i != pos) --i;
    float t = pos - (float)i;
    float (*get)(ch_reader *, int32_t) = wrap ? ch_reader_wrap : ch_reader_clip;
    return ch_hermite(get(r, i - 1), get(r, i), get(r, i + 1), get(r, i + 2), t);
}

enum { CH_POLY_VOICES = 8 };
typedef struct {
    ch_reader rd;
    uint8_t track, note, active, stage, looping, dir;
    float pos, rate, vel;
    float env, env_lag, flt_env;   /* amp env, shaped filter env */
    ch_svf_t f;
    float fade;              /* a one-shot's end: 0 playing, rising to 1 over its last 3 ms (or from where LENGTH was pulled under it) */
    float envt;              /* the envelope's target (a curved one is worked out every 8th frame) */
    uint32_t n;              /* frames since the note */
    uint8_t kill;            /* the load governor's: fade out over 5 ms */
    float last, dc, dcp;     /* a taken voice's hand-over: its last sound let go over 3 ms on an S curve (dcp 1 .. 0) */
    float rel_from;          /* level when the release began */
    uint32_t age;
} ch_voice;

enum { CH_DRUM_MODES = 8, CH_KS_LEN = 1024 };
typedef struct {
    uint8_t on, model, note, kill, quiet;          /* quiet: sub-blocks its sound has been under CH_TAIL_END */
    uint8_t part;                  /* KIT: the part it plays (its settings), 0..7 */
    uint32_t n;                    /* samples since the hit */
    float vel, fade, dc, last, dcp;   /* dcp: a re-struck voice's hand-over, its last sound let go over 3 ms on an S curve */
    float amp, aux, aux2, pit, snap;
    float ph[6], fb, hold, chaos, sh, lp;
    ch_svf_t f[4];
    float ry1[CH_DRUM_MODES], ry2[CH_DRUM_MODES], rb1[CH_DRUM_MODES], rb2[CH_DRUM_MODES], rg[CH_DRUM_MODES];
    float var_p, var_d, var_s;
    int16_t *ks; uint16_t ks_w, lfsr; float ks_prev;
    ch_rng_t rng;
} ch_dvoice;
enum { CH_DVOICES = 4, CH_SVOICES = 4 };
/* how much a drum voice still matters when one must go: its level, a low drum (a kick, a tom, an 808) twice that, and the
 * hit's body kept whole (150 ms, a low one 300) (ch_drum.c) */
float ch_dvoice_worth(const ch_dvoice *v);   /* per track: chords from the lanes */
/* all tracks together: drum and synth voices sounding at once. More would ask the FM-1 for more than it can
 * render in time (the fuzz that ignores MASTER); a note over the budget fades the oldest voice out (~5 ms). */
enum { CH_VOICE_BUDGET = 8 };
typedef struct { ch_dvoice v[CH_DVOICES]; uint8_t cur; int8_t kit_oct[8]; float flash; } ch_drum;   /* (kit_oct: a KIT's part struck this many semitones up or down, its step's octaves, as its lane's step last said) */

enum { CH_BUBBLES = 6 };
typedef struct {
    uint8_t quiet;                                 /* sub-blocks its sound has been under CH_TAIL_END */
    uint8_t on, model, note, gate, stage, kill;   /* kill: being faded out (stolen by the voice budget) */
    uint8_t fe_dirty;                  /* a Mutable model used the Felucca voice's room: start it afresh before a Felucca note */
    uint8_t pend_note, pend_vel;       /* a note waiting for this voice: it was taken while sounding, and fades out first (3 ms) */
    uint32_t n;
    float fade;
    float vel, env, hz, hz_target, hz_jit, dc, last, fb;
    float dcp;                     /* a taken voice's hand-over: the last sound's level (dc) let go over 3 ms on an S curve, 1 .. 0 */
    float ph[8];
    ch_svf_t f[4];
    float ry1[6], ry2[6], rb1[6], rb2[6], rg[6];
    struct { float s, c, e, de, dk; } bub[CH_BUBBLES];   /* BUBBLE: each bubble (its sine and cosine, its angle a frame and that angle's rise, its decay a frame; e 0: none) */
    int16_t *ks; uint16_t ks_w;     /* the string's line (the drum voice's buffer: a track is one or the other) */
    ch_rng_t rng;
    float kc[8], kg[3];            /* the model's constants for this block (model_block), so the samples don't redo them */
    float lpg_g;                   /* the low-pass gate's coefficient, renewed every 8 samples */
    float fenv, flt_g;             /* the filter's envelope and its coefficient */
    uint8_t fstage;                /* 0: the filter envelope rising (F ATTACK), 1: falling (F DECAY) */
    union {                        /* one model sounds on a voice at a time: the Felucca voice, or a Mutable model's state */
        ch_fe_voice fe;            /* the Felucca models' voice (SYNTH models FE_BASE..) */
        mi_fold mfold;             /* FOLD (ch_mi.c) */
        mi_string mstr;            /* STRING (ch_mi.c; its line is ks) */
        mi_modal mmod;             /* MODAL */
        mi_grain mgrain;           /* FORMANT */
        mi_chord mchord;           /* CHORD */
        struct { float jph, jf[4], jt[4], win; } rip;   /* RIPPLE: its jitter (a random line each for SYNC FOLD BEND NOISE), the sync window held for the cycle */
    };
} ch_svoice;
typedef struct { ch_svoice v[CH_SVOICES]; uint8_t cur; float last_hz; float c13; } ch_synth;   /* c13: ACID's accent sweep charge, building over repeated accents */

/* GRAIN: a cloud of grains from a sample (ch_grain.c); each grain decodes the block it reads into its own cache */
enum { CH_GRAINS1 = 6 };
typedef struct {
    float pos, rate, al, ar;       /* where it reads, its step, its level left and right */
    float wc, ws, we;              /* its window's quadrature pair and step (0.5 - 0.5 wc) */
    int32_t left, blk;             /* frames it has left (0: free); the block in its cache */
    uint16_t skip;                 /* frames into the block it starts at */
    int16_t pcm[CH_ADPCM_BLOCK + 1];   /* (and the frame after the block's last) */
} ch_grain1;
typedef struct { ch_grain1 g[CH_GRAINS1]; float env, env_out, vel, next; const ch_sample *smp; uint8_t on, gate, note; ch_rng_t rng; } ch_graineng;
extern uint8_t ch_grain_cap;

typedef struct {                   /* one lane's notes in flight */
    ch_tf due, rat_next, rat_gap, gate_off;
    float rat_mul;                 /* a roll: each gap is the last times this (IN < 1, OUT > 1) */
    uint8_t due_vel, due_rat, rat_left, rat_vel, gate_note, due_long, rat_kind, due_oct;   /* (due_oct: the step's octave, as stored) */   /* due_long: a tie follows, the gate reaches it; rat_kind 0 even, 1 X IN, 2 X OUT, 3 XF IN, 4 XF OUT */
    uint8_t due_gate, gate;        /* the step's GATE (0..100 %): the one due, the one sounding (its ratchets' hits too) */
    uint8_t due_snd;               /* the step's SEND, as stored (0: all) */
} ch_lane_run;
typedef struct {
    uint8_t len, dir, rate;        /* the pattern itself is the control side's (ch_seq_get) */
    int16_t pos;                   /* step scheduled (half a step ahead), -1 none yet */
    int16_t shown, ahead;          /* the step sounding now (the playhead), the one scheduled next */
    ch_tf ahead_at;                /* its time on the grid */
    ch_tf shown_at;                /* the sounding step's time on the grid */
    int8_t pend_dir;
    uint32_t pass;                 /* completed passes, for cycles */
    ch_tf next;                    /* beat of the next step */
    uint32_t count;                /* steps played since start (swing parity) */
    ch_lane_run ln[CH_LANES];
    ch_rng_t rng;
} ch_seq;

/* an FX slot's effect state (ch_fx.c): its line memory, then the parts each effect uses */
typedef struct {
    int16_t *mem;                  /* line memory (CH_FX_BLOCK-frame blocks), NULL: none needed, or none free */
    uint8_t blocks, nomem, dn;     /* blocks held; 1 = it needs some and found none; the decimation's count */
    uint8_t unclean;               /* blocks of its memory still to clear (dry until 0) */
    uint8_t mono;                  /* this block's left and right are the same sound (track_sub looked): FILTER and DRIVE work one side and copy it */
    uint8_t wide;                  /* it has been given two different sides since it was last reset: its two memories may differ, so it works both from then on
                                    * (going back to one side when the sides fell silent together put the left's ringing in place of the right's) */
    uint32_t w;                    /* the line's write place */
    float ph, ph2;                 /* LFO phases, turns */
    float t_l;                     /* the delay time, gliding (line frames) */
    union {
        struct { float z[2][8]; ch_svf_t f[2][2]; } a;   /* per side: filter and all-pass memories; FILTER's two stages */
        float sp[2][20];           /* SPRING: two springs' dispersion chains */
        struct { float z[2][5]; float xo[2][8]; float gl, gs, gp, ck, cv; } d;   /* (ck, cv: COMP's gain and the settings it was worked out for) */   /* DRIVE: CRUSH's tilt (z); per side the crossover's states; the GATE's level and gain */
                                   /* DRIVE: per side its filters (tilt, DC, pre-emphasis, gate), then the oversampling's half-bands up and
                                    * down (a branch each: three allpasses' states) and the odd sample held: 2x, and a second stage for 4x */
    } u;
    float fb[2], acc[2], prev[2], cur[2], hold[2];
    uint16_t wi[6];                /* REVERB: its lines' write places */
    ch_rng_t rng;                  /* noise (TAPE, VINYL); seeded on first use */
    float lp[4];
} ch_fx;

/* ---- modulators ---- */
typedef struct {
    uint8_t type, shape;
    uint8_t wraps, cyc;      /* an LFO's whole cycles counted (free: as it turns over; synced: from the beat), and that count at the
                              * phase shown (PHASE added): the MOD view's place among the cycles it draws */
    float p[CH_MOD_PARAMS];
    uint8_t m[CH_MOD_PARAMS];
    float phase;             /* free-running phase */
    float out_l, out_r;      /* bipolar -1..1 */
    float uni_l, uni_r;      /* unipolar 0..1 */
    /* RANDOM */
    float steps[16], rnd_l, rnd_r; int8_t last_step;
    /* FOLLOW */
    float env_l, env_r, hold_l, hold_r, hold_phase, dly[64]; uint8_t dly_w, dly_div;
    /* ADSR */
    uint8_t gate, stage; float level, from;
    float sl_l, sl_r; uint8_t sl_on;  /* the output's minimum slew (~2 ms): a square's edge, a step, a new random value never jump */
    ch_rng_t rng;
} ch_mod;

typedef struct {
    uint8_t used, mod, slot, param, bipolar;
    float depth;
} ch_map;

typedef struct {
    uint8_t device[CH_SLOTS], bypass[CH_SLOTS];
    float base[CH_ROWS][CH_PARAMS];         /* targets from control (rows: ch_row) */
    float smooth[CH_ROWS][CH_PARAMS];       /* smoothed bases */
    uint8_t mode[CH_ROWS][CH_PARAMS];
    ch_lr v[CH_SLOTS][CH_PARAMS];           /* modulated, this sub-block */
    ch_mod mod[CH_MODS];
    ch_map map[CH_MAPS];
    float mac_s[CH_MACROS];                 /* the macros, smoothed (this track's sub-blocks) */
    float send[CH_SENDS], level, filter, pan;
    float ssend, ss_new, ssg, ssg1; uint8_t ss_set;          /* (ssg: the share as heard, gliding to ssend through ssg1: two one-poles, no corner, so it never clicks) */
    /* the step's SEND: the share of the sends now; a hit's this sub-block (the largest of them) */
    float send_s[CH_SENDS], send_s1[CH_SENDS], filter_s;      /* the sends and the mix filter as heard: gliding to the knobs (no step) */
    float mixmod[3];                        /* the maps onto the mixer (CH_MIXSLOT): added to LEVEL, PAN, FILTER */
    float g_l, g_r, g1_l, g1_r;             /* smoothed mix gains: two one-poles in a row (g1 the first), so a change starts and ends without a corner */
    uint8_t mute, run, sample[2];
    uint8_t ss_dest;                         /* what the steps' SEND adds to (0: all sends, 1..4, CH_SS_FX) */
    float tofx, fx_s, fx_s1;                  /* FX SEND, and as heard (gliding as the sends do) */
    ch_svf_t dj[2];
    ch_fx fx[CH_FX];
    ch_drum drum;
    ch_synth synth;
    ch_graineng grain;
    ch_seq seq;
    uint16_t fx_quiet[CH_SLOTS];                    /* sub-blocks an effect has had silence in and given silence out: past CH_FX_SLEEP it is not run until sound comes */
    uint16_t pwake; uint8_t pbeat, mod_used;       /* pwake: sub-blocks the values are still gliding (a setting, a lock or a device moved); asleep, only mapped values are made again. mod_used: the modulators a map reads (a bit each) */
    float lk_v[CH_SLOTS][CH_PARAMS]; uint16_t lk_mask[CH_SLOTS];   /* the parameter locks in force (the current step's) */
    float lvl_in, lvl_mat, lvl_out;         /* follower sources, this sub-block (peak) */
    float peak_l, peak_r;
    uint8_t notes_held, poly_loop;
    /* the declick dips (ch_core.c): how far down each stage is, 0 (as zeroed: full) .. 1 (out: silent, or for an FX slot
     * its dry sound), and where it is going (+1 down, -1 back up); [CH_SLOTS] the whole track (INIT) */
    float dk[CH_SLOTS + 1]; int8_t dd[CH_SLOTS + 1];
} ch_trk;
/* a stopped voice's fade: its progress 1 .. 0 over CH_KILL_S, its gain on an S curve (no corner where it starts or ends) */
#ifndef CH_TAIL_END                                     /* (the bench's null tests build with 0: never) */
#define CH_TAIL_END 5e-4f                               /* -66 dB: a tail under this ... */
#endif
#define CH_TAIL_SUBS 32                                 /* ... for this many sub-blocks (23 ms), its envelope under 2 %, is let go */
#define CH_KILL_S 0.008f
#define CH_KILL_STEP (1.0f / (CH_KILL_S * (float)CH_RATE))
CH_SI float ch_fade_gain(float f) { f = f < 0 ? 0 : f > 1 ? 1 : f; return f * f * (3 - 2 * f); }

extern ch_trk ch_tr[CH_TRACKS];
extern ch_trk ch_mst;                  /* the master chain (its FX slots and settings only) */
extern uint16_t ch_hits[CH_TRACKS];
extern uint16_t ch_lane_hits[CH_TRACKS][CH_KIT_PARTS];
extern uint8_t ch_last_note[CH_TRACKS];   /* each track's latest note-on (COMB's FOLLOW), 0 none yet */    /* each track's note-ons, counted (SIDECHAIN's trigger) */
extern unsigned ch_sub;                    /* a sub-block's frames now, 32 or 64 (ch_sub_set) */
extern unsigned ch_sub_off, ch_frames;    /* this sub-block's place in the render call, and the call's frames */
extern const float *ch_fx_el_g, *ch_fx_er_g; extern uint8_t ch_fx_pre_g, ch_fx_sent, ch_fx_retmask;   /* (ch_fx.c: a master effect as a send's return) */
int ch_fx_return(const ch_trk *t, unsigned s, float *mix);
int ch_fx_mono_ok(const ch_trk *t, unsigned s);   /* the effect treats the two sides alike: one sound on both can be worked once */
extern float ch_tmix[CH_TRACKS][2][CH_SUB];   /* each track's output this sub-block, as mixed (the master SIDECHAIN spares its source) */
extern const ch_sample *ch_smp[CH_SAMPLES];
extern ch_voice ch_voices[CH_POLY_VOICES];
extern float ch_mix_level;

/* device processors: in-place on n frames of L/R (n <= CH_SUB) */
void ch_poly_process(ch_trk *t, unsigned ti, float *L, float *R, unsigned n);
void ch_poly_note(ch_trk *t, unsigned ti, unsigned note, unsigned vel);
void ch_poly_action(ch_trk *t, unsigned ti, unsigned a, unsigned v);
void ch_fx_process(ch_trk *t, unsigned slot, float *L, float *R, unsigned n);
void ch_fx_prepare(ch_trk *t, unsigned ti, unsigned k);   /* slot CH_FX1 + k's memory (the first core) */
void ch_fx_release(ch_trk *t, unsigned ti, unsigned k);
void ch_fx_reset(ch_trk *t, unsigned k);                  /* its state cleared, its memory kept and silenced */
void ch_drum_init(ch_trk *t, unsigned ti);
void ch_synth_init(ch_trk *t, unsigned ti);
void ch_synth_note(ch_trk *t, unsigned ti, unsigned note, unsigned vel);
CH_AUDIO_CODE void ch_synth_process(ch_trk *t, unsigned ti, float *L, float *R, unsigned n);
void ch_drum_note(ch_trk *t, unsigned ti, unsigned note, unsigned vel);
void ch_kit_note(ch_trk *t, unsigned ti, unsigned part, unsigned vel);   /* KIT: a part struck (ch_drum.c) */
void ch_grain_note(ch_trk *t, unsigned ti, unsigned note, unsigned vel);
void ch_grain_process(ch_trk *t, unsigned ti, float *L, float *R, unsigned n);
CH_AUDIO_CODE void ch_drum_process(ch_trk *t, unsigned ti, float *L, float *R, unsigned n);
void ch_mods_init(ch_mod *m, unsigned type, uint32_t seed);
CH_AUDIO_CODE void ch_mods_tick(ch_trk *t, unsigned ti, unsigned n);
CH_AUDIO_CODE void ch_mod_note(ch_trk *t, int on);
void ch_mem_init(void);

/* Parameter access for the device code: the slot's modulated value. */
#define PV(t, slot, i) ((t)->v[slot][i].l)
#define PVR(t, slot, i) ((t)->v[slot][i].r)
#define PMODE(t, dev, i) ((t)->mode[dev][i])

/* Steep low-end curves, after the "sweet spot in the first 5%" reports:
 * x^2 and x^3 keep fine resolution near zero. */
static inline float ch_sq(float x) { return x * x; }
static inline float ch_cube(float x) { return x * x * x; }
static inline float ch_exp_map(float x, float lo, float hi) { return lo * ch_exp2(ch_log2(hi / lo) * ch_clamp(x, 0, 1)); }
static inline float ch_level_gain(float v) { float g = v * (1.0f / 0.8f); return g * g; } /* 0.8 = unity */
#endif
