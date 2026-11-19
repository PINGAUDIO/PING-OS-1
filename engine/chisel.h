/* Chisel: the sound engine of PING OS-1, a four-track rhythm synthesis engine for the M-VAVE FM-1.
 *
 * This header is the whole public API: an app (the firmware in app/, the simulator in sim/, the plugin, the
 * benches) includes only this. A fork that wants a different front panel keeps engine/ and writes a new app.
 *
 * Where things are:
 *   ch_core.c      the render loop, the two-core split, the master chain, the command ring, the sequencer clock
 *   ch_params.c    every device's parameter table (names, ranges, defaults): add a knob here first
 *   ch_synth.c     the SYNTH engine's models (ANALOG, ACID, WOBBLE and the rest)
 *   ch_drum.c      the DRUM engine's models
 *   ch_material.c  the SAMPLE engine; ch_kit*.c the KIT; ch_grain*.c GRAIN
 *   ch_fx.c        every effect, one function each, and the table that names them
 *   ch_mods.c      the modulators (LFO, envelope, random, follower)
 *   ch_mi*.c, ch_fe*.c   third-party DSP under their own licences: docs/THIRD_PARTY.md
 * On the FM-1 code runs from flash through a small cache: a per-sample loop must carry CH_AUDIO_CODE (it is then
 * kept in RAM) and anything it calls per sample must be CH_SI, or it costs about four times as much.
 *
 * Each track runs its engine (MATERIAL), then four FX slots in order, each holding
 * any one of the plain effects (ch_fx.c) or nothing, with four modulators of its own and four sends into any
 * track's input.
 *
 * Threading: the audio interrupt calls ch_render() once per 128-frame block.
 * Everything else is called from the control task and only queues commands
 * for the audio side; the engine never shares mutable state across the two
 * except through the command ring and the read-only snapshot. */
#ifndef CHISEL_H
#define CHISEL_H
#include <stdint.h>
#include <stddef.h>
#include "ch_platform.h"

enum { CH_TRACKS = 4, CH_SLOTS = 5, CH_PARAMS = 16, CH_MODS = 4, CH_MAPS = 24, CH_MACROS = 4,
       CH_SAMPLES = 8, CH_SENDS = 4 };
enum { CH_FX = 4 };                     /* FX slots a track has, after its engine */
/* the master: four FX slots after the tracks' mix, DJ FILTER, DELAY, REVERB, then any effect. In the API it is
 * track CH_MASTER (ch_device_set, ch_bypass_set, ch_param_set, ch_mode_set, ch_track_get); its MATERIAL slot is empty */
enum { CH_MASTER = CH_TRACKS };
enum ch_slot { CH_MATERIAL, CH_FX1, CH_FX2, CH_FX3, CH_FX4 };
enum ch_device {
    CH_DEV_BYPASS,                      /* the engine off; in an FX slot: empty */
    CH_DEV_KIT, CH_DEV_POLY, CH_DEV_DRUM, CH_DEV_SYNTH, CH_DEV_GRAIN,   /* (KIT took the retired DISC's number; TAPE, the looper, is gone) */
    CH_DEV_FILTER, CH_DEV_DRIVE, CH_DEV_COMB, CH_DEV_CHORUS, CH_DEV_FLANGER, CH_DEV_PHASER, CH_DEV_TREMOLO,
    CH_DEV_DELAY, CH_DEV_ECHO, CH_DEV_DUB, CH_DEV_REVERB, CH_DEV_DJ, CH_DEV_SIDE, CH_DEV_RESO, CH_DEV_TAPE_FX, CH_DEV_VINYL, CH_DEV_SPRING, CH_DEV_REPEAT, CH_DEV_CRYSTAL, CH_DEV_TRANCE, CH_DEV_ERODE, CH_DEV_DISPERSE, CH_DEV_UNISON, CH_DEVICES
};
enum { CH_DEV_FX_FIRST = CH_DEV_FILTER, CH_FX_TYPES = CH_DEVICES - CH_DEV_FILTER };
enum { CH_DELAY_DIVS = 10 };            /* DELAY's TIME: 1/32 .. 1/4D */
/* settings rows: the engines keep theirs per engine (a track's DRUM and SYNTH each remember), the FX slots theirs
 * per slot (two slots with the same effect, each its own) */
/* (the material rows stop after COMB's number: the engines, and the two spare rows below. An effect's number never
 * names a row: it would be a row a track, never used) */
enum { CH_MROWS = CH_DEV_COMB + 1, CH_ROWS = CH_MROWS + CH_FX };
static inline unsigned ch_row(unsigned slot, unsigned device) { return slot == 0 ? (device < CH_MROWS ? device : 0u) : CH_MROWS + slot - 1; }
/* the SYNTH's parameters past CH_PARAMS (the filter's decay, the oscillators' tuning) live in the material row of the
 * retired BITCRUSH, which nothing else uses: ch_param_set takes them as parameters 16.., this reads them back */
enum { CH_SYNTH_XROW = CH_DEV_COMB };   /* (COMB took the retired BITCRUSH's number; as an effect it uses the slot rows, never this one) */
/* KIT: eight drum parts on one track, lane n playing part n. Its 16 parameters are each part's PITCH and DECAY (2k, 2k + 1:
 * lockable, mappable); its extras, MODEL and LEVEL of each (16.. 23, 24.. 31), sit in FILTER's material row, as the SYNTH's do */
enum { CH_KIT_PARTS = 8, CH_KIT_XROW = CH_DEV_FILTER };
/* the spare row an engine keeps its parameters past CH_PARAMS in, 0 none */
static inline unsigned ch_xrow(unsigned dev) { return dev == CH_DEV_SYNTH ? (unsigned)CH_SYNTH_XROW : dev == CH_DEV_KIT ? (unsigned)CH_KIT_XROW : 0u; }
/* LFO (WAVE; RANDOM is the same since the shapes became its SHAPE parameter) and TRIG RND are the ones offered; FOLLOW and
 * ADSR stay compiled for stored projects. TRIG: a new random value on each note. */
enum ch_mod_type { CH_MOD_WAVE, CH_MOD_RANDOM, CH_MOD_FOLLOW, CH_MOD_ADSR, CH_MOD_TRIG, CH_MOD_TYPES };
enum { CH_LFO_SINE, CH_LFO_TRI, CH_LFO_SAWUP, CH_LFO_SAWDN, CH_LFO_SQUARE, CH_LFO_STEPS, CH_LFO_DRIFT, CH_LFO_SHAPES };
enum { CH_MOD_PARAMS = 8 };

/* Parameter kinds: UNI 0..1, BI -1..1, SELECT an index 0..steps-1. */
enum ch_kind { CH_UNI, CH_BI, CH_SELECT };
typedef struct {
    const char *name;
    uint8_t kind, steps;          /* steps: SELECT size */
    float def;                    /* normalised default */
    const char *const *modes;     /* NULL-terminated mode names, or NULL */
    const char *const *choices;   /* names for SELECT values, or NULL */
} ch_param_desc;
typedef struct {
    const char *name;
    uint8_t slot, params;          /* slot it belongs to, live parameter count */
    const ch_param_desc *param;
    const char *const *actions;    /* NULL-terminated action names */
    const uint8_t *order;          /* the parameters' order on the pages (NULL: as numbered): the ones to reach first lead */
} ch_device_desc;
const ch_device_desc *ch_device_info(unsigned device);
const ch_param_desc *ch_mod_param_info(unsigned type, unsigned param);
const char *ch_mod_name(unsigned type);
const char *ch_mod_label(unsigned type, unsigned shape, unsigned param);   /* a parameter's name for the LFO's shape, or NULL */
int ch_mod_value_text(unsigned type, unsigned param, float v, unsigned mode, char *out);   /* 1: a division or Hz was written */
unsigned ch_div_count(void);                   /* the synced divisions (8 bars .. 1/128) */
unsigned ch_div_index(float v);
float ch_div_value(unsigned i);
const char *const *ch_scale_names(void);

/* Device actions (index into ch_device_desc.actions). */
enum { CH_POLY_LOAD, CH_POLY_LOOPMODE, CH_POLY_TRIG };
enum { CH_DRUM_TRIG };

/* DRUM: 40 models; SHAPE (param 3) and COLOR (4) are named per model. */
enum { CH_DRUM_MODELS = 49 };
extern const char *const ch_drum_names[CH_DRUM_MODELS + 1];
extern const uint8_t ch_drum_order[CH_DRUM_MODELS];   /* the model (as the engine numbers them) at each place in the list */
const char *ch_drum_label(unsigned model, unsigned param);   /* NULL: use the table's name */
/* SYNTH: a macro oscillator, 15 models; HARMONICS (2), TIMBRE (3), MORPH (4) named per model. */
enum { CH_SYNTH_MODELS = 20 };      /* 15 of ours, then 5 from Felucca (ch_fe.h) */
extern const char *const ch_synth_names[CH_SYNTH_MODELS + 1];
/* an effect's value in the screen's words ("1.2K", "250MS", "8 BIT"); 0 = none (show it as a number) */
int ch_fx_value_text(unsigned device, unsigned param, float value, unsigned mode, char *out);
const char *ch_delay_div_name(unsigned i);       /* DELAY / DUB DELAY's synced TIME, by its step */
/* the effect's amount: its MIX (AMOUNT, DJ FILTER's FILTER) parameter, -1 none: the PERFORM page's knob */
int ch_fx_amount_param(unsigned device);
/* for the app's drawings of the effects */
float ch_fx_phase(unsigned track, unsigned slot);                  /* its sweep, in turns, now */
float ch_fx_delay_sec(unsigned device, float value, unsigned mode);   /* a delay's TIME in seconds, at the tempo */
float ch_fx_comb_hz(unsigned track, unsigned slot);
float ch_fx_reso_hz(float value);
float ch_fx_erode_hz(float value);            /* ERODE's FREQ, in Hz */
float ch_fx_disp_hz(float value);             /* DISPERSE's FREQ, in Hz; its PINCH as a Q; its AMOUNT in stages */
float ch_fx_disp_q(float value);
unsigned ch_fx_disp_stages(float value);
float ch_fx_drive_curve(unsigned type, unsigned clip, float drive, float x);
/* the effects' line memory: blocks held by track ti (CH_MASTER too), and in all (CH_FX_BLOCKS of them) */
unsigned ch_fx_blocks_held(unsigned ti);
unsigned ch_fx_blocks_used(void);
const char *ch_synth_label(unsigned model, unsigned param);
const char *ch_fx_label(unsigned dev, unsigned type, unsigned param);   /* an effect knob named for its TYPE (DRIVE), or NULL */
int ch_synth_value_text(unsigned model, unsigned param, float v, char *out);   /* a named value (a wave, a chord, a ratio): the number of choices, 0 if plain */
int ch_synth_preset(unsigned model, unsigned param, float *v);               /* the model's starting value for a parameter: 1 when it has one */
const uint8_t *ch_synth_order(unsigned model);   /* a model's own page order (every parameter), or 0: the SYNTH's */
const char *const *ch_synth_modes(unsigned model, unsigned param);          /* a model's own modes for a parameter (SEQ + its knob), 0 none: WOBBLE's DEPTH, UNIPOLAR or BIPOLAR */
int ch_synth_param_shown(unsigned model, unsigned param);                   /* 0: a parameter the model has no use for (the tuning, off ANALOG) */

/* Samples: blocked IMA ADPCM (ch_adpcm.h), mono, at `rate`. The data must
 * stay valid and unchanged while bound (it is read from mapped Flash). */
typedef struct {
    const uint8_t *adpcm;
    uint32_t frames, rate;
    float bpm;                    /* 0 = unknown */
} ch_sample;

/* Snapshot the control side can read at any time (written by audio). */
typedef struct {
    float level[CH_TRACKS][2];             /* track peak, decays */
    float mixmod[CH_TRACKS][3];            /* what the modulators and macros add to a track's LEVEL, PAN and FILTER now */
    float ret_level[2];                    /* the RETURN's: what the tracks send into the master effects, after its level */
    float master[2];
    float value[CH_TRACKS][CH_SLOTS][CH_PARAMS]; /* modulated, normalised (L) */
    float mod[CH_TRACKS][CH_MODS];         /* modulator outputs (L) */
    uint8_t mod_ph[CH_TRACKS][CH_MODS];    /* an LFO's phase now, 0..255 a cycle (the MOD view's dot) */
    uint8_t mod_cyc[CH_TRACKS][CH_MODS];   /* and its whole cycles, counted (which of the drawn cycles the dot is in) */
    uint8_t locked[CH_TRACKS];
    float lim_gr;                          /* the limiter's gain this block at its least (1: nothing taken off) */
    uint8_t voices, playing;
    float beat;                            /* transport position in beats */
    int8_t step[CH_TRACKS];                /* sequencer playhead, -1 stopped */
    float hit[CH_TRACKS];                  /* 1 on a hit, decays (audio-rate: a screen loop can miss it) */
    uint16_t hits[CH_TRACKS];              /* counts the track's note-ons: the UI flashes on any change, never missing one */
    uint8_t lanes[CH_TRACKS];              /* bit per lane whose note is sounding now */
    float env[CH_TRACKS];                  /* the engine's amplitude envelope now, 0..1 (SYNTH, SAMPLE, GRAIN: the loudest voice) */
    float step_frac[CH_TRACKS];            /* how far into the playhead's step, 0..1 (a live tap: the nearest step) */
    uint16_t load_us[CH_TRACKS][CH_SLOTS]; /* the time each stage took (engine, then the effects by slot), microseconds
                                            * per render call, the peak of the last second (ch_profile_clock) */
    uint16_t render_us, budget_us;         /* the whole render's peak, and the time the call's audio lasts */
    uint8_t cores, core2_tracks;           /* the cores rendering (1, 2), and the second's tracks (a bit each) */
    uint8_t voice_cap;                     /* the load governor's voice limit now (2 .. CH_VOICE_BUDGET) */
    uint16_t shed;                         /* voices it has faded out to keep the render in time (since start) */
    uint16_t bad[CH_TRACKS][CH_SLOTS];     /* blocks a stage put out a broken value (NaN, infinity, past 16384):
                                            * muted and that stage reset (since start) */
    uint8_t bad_track, bad_slot, bad_kind; /* the last: where, and 1 NaN, 2 infinity, 3 too large */
    uint16_t master_bad;                   /* the master's state found broken and reset */
    uint8_t metronome;                     /* the click is on */
    uint8_t count;                         /* a count-in is running: the beat it is on, 1 up (0: none) */
    float mvalue[CH_SLOTS][CH_PARAMS];     /* the master chain's modulated values (as value[] for a track) */
    uint16_t master_us;                    /* the master chain's time, peak of the last second */
    uint8_t mfx_nomem;                     /* a bit per master FX slot whose effect found no line memory */
    uint8_t fx_nomem[CH_TRACKS];           /* a bit per FX slot whose effect found no line memory (passes dry) */
    uint8_t pattern_pending;               /* a pattern switch waits for the bar */
    uint16_t bar;                          /* the bar the transport is in (0 from the start) */
    uint16_t sub0_us, subn_us;             /* a render's first 32-frame sub-block and the mean of its others, us, averaged (the first carries the flash cache's refill) */
    uint32_t blocks, overloads;
} ch_snapshot;

/* ---- audio side ---- */
void ch_init(void);
/* out: `frames` interleaved stereo Q23 (frames <= CH_MAX_BLOCK). in: the same
 * shape of live input (USB) or NULL. */
CH_AUDIO_CODE void ch_render(int32_t *out, unsigned frames, const int32_t *in);
void ch_render_far(int32_t *out, unsigned frames, const int32_t *in);   /* ch_render for a caller in the flash (the FM-1: ch_render is RAM code, past a plain call's reach; this unit's calls are far) */
/* a clock for the load page: a free-running counter and its ticks a microsecond (the FM-1: plat_ticks24, 24) */
typedef uint32_t (*ch_clock_fn)(void);
void ch_profile_clock(ch_clock_fn fn, unsigned ticks_per_us);
/* a second core for the render (the FM-1: plat_cpu2_run, plat_cpu2_wait): some of the tracks go to it, the
 * split by the time each took last. run: 1 = it has fn(arg), 0 = not (done here); wait: 0 = done, else not run */
typedef int (*ch_job_run_fn)(void (*fn)(uint32_t), uint32_t arg);
typedef int (*ch_job_wait_fn)(void);
void ch_worker(ch_job_run_fn run, ch_job_wait_fn wait);
const ch_snapshot *ch_snapshot_get(void);

/* ---- control side: everything below only queues a command ---- */
int ch_sample_bind(unsigned slot, const ch_sample *sample);
void ch_sample_name_set(unsigned slot, const char *name);   /* the SAMPLE parameter's choice names (the app's) */
int ch_device_set(unsigned track, unsigned slot, unsigned device);
int ch_bypass_set(unsigned track, unsigned slot, unsigned on);
/* an FX slot (CH_FX1..CH_FX4): any effect, or CH_DEV_BYPASS for none. Putting one in sets that slot alone: its
 * settings to the effect's own, its modulation mappings cleared, the other slots untouched. */
int ch_param_set(unsigned track, unsigned slot, unsigned param, float value);
int ch_mode_set(unsigned track, unsigned slot, unsigned param, unsigned mode);
int ch_action(unsigned track, unsigned slot, unsigned action, unsigned value);
int ch_mod_type_set(unsigned track, unsigned mod, unsigned type);
int ch_mod_param_set(unsigned track, unsigned mod, unsigned param, float value);
int ch_mod_mode_set(unsigned track, unsigned mod, unsigned param, unsigned mode);
float ch_mod_rate_hz(unsigned track, unsigned mod);   /* an LFO's speed now, cycles a second (synced: at the tempo) */
int ch_mod_shape_set(unsigned track, unsigned mod, unsigned shape); /* WAVE: SIN/TRI/SQUARE */
/* depth -1..1; 0 removes the mapping. bipolar 0 = unipolar. */
enum { CH_MIXSLOT = 5 };   /* a map's slot for the track's mixer: its param 0 LEVEL, 1 PAN, 2 FILTER (= CH_SLOTS) */
int ch_map_set(unsigned track, unsigned mod, unsigned slot, unsigned param, float depth, unsigned bipolar);
/* the macros: four knobs shared by every track. A map whose mod is CH_MODS + m (0..3) follows macro m, unipolar:
 * the parameter moves by depth x the macro's value (0..1) */
int ch_macro_set(unsigned m, float v);
/* voices cut while still clearly sounding (level over 0.1): by a KIT taking a voice for a new hit, indexed by the part cut;
 * by the voice budget or the load governor, indexed by track (diagnostics: the bench reads them) */
extern uint32_t ch_cut_kit[8], ch_cut_budget[4], ch_cut_bpart[8];
extern uint32_t ch_guard_trips[CH_SLOTS];
/* what became of the steps the sequencer meant to play (diagnostics): hits sent; skipped by CHANCE, by CYCLE; held by a tie;
 * a note's voice cut while under 100 ms old by the voice budget or the load governor, by being taken (stolen), by a choke;
 * a note waiting for its voice let go before it began */
typedef struct { uint32_t hits, chance, cycle, tie, cut_budget, cut_steal, cut_choke, pend_lost; } ch_dbg_t;
extern ch_dbg_t ch_dbg;   /* the guard silencing a stage (a value past 16384, or not a number), by slot, all tracks */   /* bpart: the budget's drum cuts by part */
float ch_macro(unsigned m);
int ch_note(unsigned track, unsigned note, unsigned velocity); /* velocity 0 = off */
int ch_send_set(unsigned track, unsigned target, float amount);
enum { CH_MIX_LEVEL, CH_MIX_FILTER, CH_MIX_PAN, CH_MIX_MUTE, CH_MIX_RUN, CH_MIX_SSDEST, CH_MIX_TOFX };   /* SSDEST: what the steps' SEND adds to (0 all sends, 1..4 one, CH_SS_FX); TOFX: 0..1 */
/* the RETURN: the level of all the tracks send into the master effects (as a track's LEVEL: 0.8 unity, as it starts) */
void ch_return_set(float level);
float ch_return(void);
enum { CH_SS_FX = 5, CH_SS_LAST = 8 };   /* CH_SS_FX: the whole master chain (in at its first effect); 6, 7, 8: in at the master's 2nd, 3rd, 4th effect, past the ones before */
int ch_mix_set(unsigned track, unsigned what, float value);
int ch_master_set(float compress, float main);
void ch_sub_set(unsigned frames);          /* the sub-block: 32 frames (as ever) or 64, taken at the next render call */
unsigned ch_sub_get(void);
void ch_kit_core2_set(int on);                /* a KIT may render on the second core (on at the start) */
int ch_kit_core2(void);
void ch_limiter_set(int on);                 /* the master limiter (on at the start); off, only the knee and the clamp hold full scale */
int ch_limiter(void);
int ch_transport(unsigned playing);
int ch_tempo_set(float bpm);
enum { CH_STUTTER_DIVS = 4 };               /* 1/8, 1/16, 1/32, 1/64 */
int ch_stutter(unsigned on, unsigned div, unsigned tracks);   /* a hold: the step sounding repeats at the division on the tracks given (bits; 0 = all); off resumes where the pattern would be */
int ch_count_in(unsigned beats);       /* stopped: that many clicks a beat apart (the first higher), then the transport starts; a stop ends it */
int ch_metronome_set(unsigned on);      /* a soft click on each beat while playing, the bar's first a little higher */

/* ---- sequencer (after PING Step): per track, 8 note lanes of up to 64 steps ----
 * Each lane plays one note (its own, set by the UI from a scale or by hand);
 * lanes sounding on the same step make a chord. A cell: velocity (0 = off),
 * ratchet 1..4 even hits, 5..8 the rolls (CH_RATCHET_*), chance 0..100 %, cycle (play on pass `cyc_a` of every `cyc_b`,
 * 0 = always), micro timing -50..+50 % of a step. The track plays its steps at
 * its own length, rate and direction; swing delays the off-beat sixteenths.
 * The pattern lives on the control side and the audio side reads it as it
 * plays (a cell is a few bytes: a torn read costs one step at most). */
enum { CH_STEPS = 64, CH_LANES = 8 };
enum { CH_DIR_FWD, CH_DIR_REV, CH_DIR_PEND, CH_DIR_RND, CH_DIRS };
enum { CH_RATCHET_X_IN = 5, CH_RATCHET_X_OUT = 6, CH_RATCHET_XF_IN = 7, CH_RATCHET_XF_OUT = 8, CH_RATCHET_LAST = 8 };
/* ratchet values past the even 1..4: rolls of four hits. X IN speeds up into the step's end, X OUT slows out of its start;
 * the XF pair also fades the velocity in or out along a curve */
/* a step, six bytes (2048 of them are the pattern): tie, the lane's note before holds through this step (no retrigger);
 * ungate, how much shorter than the whole step its note is held, in fifteenths (0, as a zeroed step has it: the full step) */
/* send (in the ratchet's byte, so the step stays six bytes and `{vel, ratchet, ...}` still means what it did): what this
 * step's hit adds to the track's send, in fifteenths: 0 nothing (the send knob alone), 15 all of it */
typedef struct { uint8_t vel; union { uint8_t ratchet_send; struct { uint8_t ratchet : 4, send : 4; }; }; uint8_t chance; uint8_t cyc_a : 4, cyc_b : 4; int8_t micro; uint8_t tie : 1, ungate : 4, oct : 3; } ch_step;
/* (ungate in fifteenths, as SEND is, which leaves the byte three bits: oct, the step's note up or down by octaves, -3..+3 as a
 * 3-bit two's complement, so a zeroed step has none. A KIT's lanes are drums: no octaves there) */
typedef char ch_step_is_six_bytes[sizeof(ch_step) == 6 ? 1 : -1];
static inline int ch_step_oct(const ch_step *s) { return (int)(s->oct ^ 4u) - 4; }
static inline void ch_step_oct_set(ch_step *s, int o) { s->oct = (uint8_t)((o < -3 ? -3 : o > 3 ? 3 : o) & 7); }
static inline void ch_step_gate_set(ch_step *s, unsigned g) { s->ungate = (uint8_t)(((100u - (g > 100u ? 100u : g)) * 15u + 50u) / 100u); }
static inline unsigned ch_step_send(const ch_step *s) { return s->send; }   /* SEND, 0..15 fifteenths */
static inline unsigned ch_step_gate(const ch_step *s) { return (100u * (15u - s->ungate) + 7u) / 15u; }   /* GATE, 0..100 % (sixteen lengths) */
int ch_step_set(unsigned track, unsigned lane, unsigned step, const ch_step *s);
int ch_lane_note_set(unsigned track, unsigned lane, unsigned note);
int ch_track_init(unsigned track);
/* The master key (optional): `mask` bit n = the pitch class n semitones above `root` (0..11) is in key; 0 = no key.
 * (Kept for the UI's lanes.) */
void ch_key_set(unsigned root, unsigned mask);   /* the track as it starts: empty pattern, its sound, effects and mix reset */   /* MIDI note; 60 = the sound as tuned */
int ch_seq_len_set(unsigned track, unsigned len);          /* 1..64 */
int ch_seq_dir_set(unsigned track, unsigned dir);
int ch_seq_rate_set(unsigned track, unsigned rate);        /* 0 1/32, 1 1/16, 2 1/8T, 3 1/8, 4 1/4 */
int ch_swing_set(float amount);                            /* 0..1 = 50..75 % */
/* A parameter lock: on that step the track's parameter (slot, param) takes `v` (the value as the knob would set it,
 * times 256) instead of its knob setting, until a step without a lock on it. Any lane; CH_LOCKS a track. */
enum { CH_LOCKS = 32 };
typedef struct { uint8_t step, slot, param; int16_t v; } ch_lock;
typedef struct { ch_step step[CH_LANES][CH_STEPS]; uint8_t note[CH_LANES]; uint8_t len, dir, rate; ch_lock lock[CH_LOCKS]; uint8_t nlock; } ch_seq_state;
int ch_lock_set(unsigned track, unsigned step, unsigned slot, unsigned param, float value);   /* -1: no room */
int ch_lock_get(unsigned track, unsigned step, unsigned slot, unsigned param, float *value);  /* 1 when locked */
void ch_lock_clear(unsigned track, unsigned step);                                           /* every lock on the step; step >= CH_STEPS: all */
int ch_lock_any(unsigned track, unsigned step);
const ch_seq_state *ch_seq_get(unsigned track);
/* Patterns: the four tracks' sequences together. The one playing is in RAM; the app keeps the others packed (in
 * the flash). A switch replaces all four, at once or at the next bar while playing (snapshot pattern_pending until
 * then); the one replaced may be kept in the engine's stash (one), to come back without a save. The packed form
 * (ch_pattern_pack: a mask a lane, then only the hits that differ from a plain one) is what the app stores. */
enum { CH_PAT_MAX = 3600 };                     /* a packed pattern's most bytes (one the app can store) */
unsigned ch_pattern_pack(uint8_t *out, unsigned cap);     /* the current: bytes, 0 = it does not fit */
unsigned ch_stash_pack(uint8_t *out, unsigned cap);
int ch_pattern_switch(const uint8_t *packed, int at_bar, int keep);   /* packed NULL: an empty one; keep: the
                                                                        * current into the stash. The packed bytes
                                                                        * stay the app's until pattern_pending clears */
int ch_pattern_restore(int at_bar, int keep);  /* the stash in; keep: the current into the stash instead (a swap) */
int ch_stash_valid(void);
enum { CH_SCOPE_N = 512 };                      /* the spectrum's ring: one signal at half rate (22.05 kHz), the last 512 frames (23 ms), signed bytes */
const int8_t *ch_scope_data(unsigned *wpos);    /* the ring; *wpos the frame about to be written (the oldest). Read live: a torn frame is a little noise */
void ch_scope_source(unsigned src);             /* what it holds: 0 the master as heard, 1..4 a track as mixed */
int ch_keep_mark(void);                         /* the pattern packed aside for AUTOPILOT's revert (its own copy, apart from the undo's; 0 ok) */
int ch_keep_back(void);                         /* that pattern back, as it was (-1: nothing kept) */
int ch_undo_mark(void);                         /* the pattern packed aside before a sweeping change (0 ok) */
int ch_undo_swap(void);                         /* that pattern back, the current kept in its place: again redoes (-1: nothing) */
void ch_stash_drop(void);
int ch_pattern_dirty(void);                     /* edited since it was loaded or marked saved */
void ch_pattern_mark_saved(void);
float ch_swing(void);

/* Current base values held on the control side (what the UI edits). */
typedef struct {
    uint8_t device[CH_SLOTS], bypass[CH_SLOTS];
    float param[CH_ROWS][CH_PARAMS];      /* settings: ch_row(slot, device) */
    uint8_t mode[CH_ROWS][CH_PARAMS];
    uint8_t mod_type[CH_MODS], mod_shape[CH_MODS];
    float mod_param[CH_MODS][CH_MOD_PARAMS];
    uint8_t mod_mode[CH_MODS][CH_MOD_PARAMS];
    struct { uint8_t mod, slot, param, bipolar; float depth; } map[CH_MAPS];
    float send[CH_SENDS], level, filter, pan;
    uint8_t mute, sample[2];             /* sample slot for DISC / POLY */
    uint8_t ss_dest;                     /* what the steps' SEND adds to: CH_SS_FX the track's TO FX (as it starts), 1..4 that track's send (though its knob be at 0), 0 each send that is turned up */
    float tofx;                          /* FX SEND: how much of the track is sent into the master's delays and reverbs, whole, whatever their MIX (0, as it starts; the track is in the mix all the same) */
} ch_track_state;
static inline float ch_param_of(const ch_track_state *ts, unsigned slot, unsigned device, unsigned p)
{ return p < CH_PARAMS ? ts->param[ch_row(slot, device)][p] : ts->param[ch_xrow(device)][p - CH_PARAMS]; }
static inline unsigned ch_mode_of(const ch_track_state *ts, unsigned slot, unsigned device, unsigned p)
{ return p < CH_PARAMS ? ts->mode[ch_row(slot, device)][p] : 0; }
const ch_track_state *ch_track_get(unsigned track);
float ch_tempo(void);

#endif
