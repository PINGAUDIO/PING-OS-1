/* Device and modulator parameter tables, scales and note divisions.
 * A device's table is its knobs in order: U (0..1), B (-1..1), UM/BM (with a SEQ + knob mode list), or a
 * CH_SELECT with its names. The UI pages them four to a row. To add a knob: add it here, read it in the device's
 * code with PV(), and the screens, locks, modulation and saving follow by themselves. */
#include "chisel.h"
#include "ch_internal.h"

static const char *const scales[] = {
    "CHROMATIC", "MAJOR", "MINOR", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYD",
    "HARM MIN", "PENTA MAJ", "PENTA MIN", "WHOLE", "FIFTHS", "OCTAVES", 0};
/* Pitch-class masks, bit n = semitone n above C. */
const uint16_t ch_scale_mask[CH_SCALES] = {
    0xfff, 0xab5, 0x5ad, 0x6ad, 0x5ab, 0xad5, 0x6b5, 0x9ad, 0x295, 0x4a9, 0x555, 0x081, 0x001};
const char *const *ch_scale_names(void) { return scales; }

static const char *const sync_modes[] = {"FREE", "STRAIGHT", "TRIPLET", "DOTTED", 0};
static const char *const pitch_modes[] = {"CHROMATIC", "FREE", 0};

#define U(n, d) {n, CH_UNI, 0, d, 0, 0}
#define B(n, d) {n, CH_BI, 0, d, 0, 0}
#define UM(n, d, m) {n, CH_UNI, 0, d, m, 0}
#define BM(n, d, m) {n, CH_BI, 0, d, m, 0}

/* KIT: each part's PITCH and DECAY (lockable), then its MODEL and LEVEL (extras); the factory kit, boom bap's eight:
 * a deep kick, a lo-fi snare, closed and open hats, a rim, a clap, a shaker, a tom */
#define KP(n) BM("PITCH " n, 0, pitch_modes)
#define KD(n, d) U("DECAY " n, d)
#define KM(n, m) {"HIT " n, CH_SELECT, CH_DRUM_MODELS, m, 0, ch_drum_names}
#define KL(n, l) U("LEVEL " n, l)
static const ch_param_desc kit[] = {
    KP("1"), KD("1", 0.5f), KP("2"), KD("2", 0.5f), KP("3"), KD("3", 0.4f), KP("4"), KD("4", 0.72f),
    KP("5"), KD("5", 0.45f), KP("6"), KD("6", 0.5f), KP("7"), KD("7", 0.4f), KP("8"), KD("8", 0.55f),
    KM("1", 5), KM("2", 15), KM("3", 22), KM("4", 24), KM("5", 19), KM("6", 17), KM("7", 38), KM("8", 35),   /* (places in the list, by kind) */
    KL("1", 0.85f), KL("2", 0.8f), KL("3", 0.55f), KL("4", 0.5f), KL("5", 0.6f), KL("6", 0.7f), KL("7", 0.62f), KL("8", 0.7f)};
/* its pages: a part a page, MODEL PITCH DECAY LEVEL */
static const uint8_t kit_order[] = {16, 0, 1, 24, 17, 2, 3, 25, 18, 4, 5, 26, 19, 6, 7, 27, 20, 8, 9, 28, 21, 10, 11, 29, 22, 12, 13, 30, 23, 14, 15, 31};
static const char *ch_sample_names_v[CH_SAMPLES + 1] = {"-", "-", "-", "-", "-", "-", "-", "-", 0};
void ch_sample_name_set(unsigned slot, const char *name) { if (slot < CH_SAMPLES) ch_sample_names_v[slot] = name ? name : "-"; }
static const ch_param_desc poly[] = {
    {"SAMPLE", CH_SELECT, CH_SAMPLES, 0, 0, (const char *const *)ch_sample_names_v}, BM("PITCH", 0, pitch_modes), U("START", 0), U("LENGTH", 1), U("LEVEL", 0.8f),
    U("LOOP", 0), U("XFADE", 0), U("ATTACK", 0), U("DECAY", 0.5f),
    U("SUSTAIN", 1), B("CURVE", 0), U("VELOCITY", 1.0f), B("F AMOUNT", 0), B("F SHAPE", 0)};
static const char *const tails[] = {"CHOKE", "RING", 0};
static const ch_param_desc drum[] = {
    {"MODEL", CH_SELECT, CH_DRUM_MODELS, 0, 0, ch_drum_names}, BM("PITCH", 0, pitch_modes), U("DECAY", 0.5f),
    U("SHAPE", 0.5f), U("COLOR", 0.5f), B("TONE", 0), U("SNAP", 0), B("BEND", 0), U("NOISE", 0),
    U("DRIVE", 0), U("VARIATION", 0), U("LEVEL", 0.8f), U("VELOCITY", 1.0f), {"TAILS", CH_SELECT, 2, 1, 0, tails}};
static const char *const synth_modes[] = {"PING", "GATE", "DRONE", 0};
static const char *const octs[] = {"-2", "-1", "0", "+1", "+2", 0};
static const char *const semis[] = {"-12", "-11", "-10", "-9", "-8", "-7", "-6", "-5", "-4", "-3", "-2", "-1", "0", "+1", "+2", "+3", "+4", "+5", "+6", "+7", "+8", "+9", "+10", "+11", "+12", 0};
static const ch_param_desc grain_p[] = {
    {"SAMPLE", CH_SELECT, CH_SAMPLES, 0, 0, (const char *const *)ch_sample_names_v}, BM("PITCH", 0, pitch_modes), U("POSITION", 0.2f), U("SIZE", 0.5f),
    U("DENSITY", 0.8f), U("SPRAY", 0.2f), U("SPREAD", 0.5f), U("RELEASE", 0.5f),
    U("LEVEL", 0.8f), U("VELOCITY", 1.0f), U("ATTACK", 0)};
static const char *const synth_voices[] = {"4", "2", "1", "LEGATO", 0};
static const char *const wob_trig[] = {"FREE", "RETRIG", 0};
static const ch_param_desc synth[] = {
    {"MODEL", CH_SELECT, CH_SYNTH_MODELS, 0, 0, ch_synth_names}, BM("PITCH", 0, pitch_modes), U("HARMONICS", 0.5f),
    U("TIMBRE", 0.5f), U("MORPH", 0.5f), U("DECAY", 0.45f), U("LPG", 0.5f), {"MODE", CH_SELECT, 3, 1, 0, synth_modes},
    U("ATTACK", 0), U("GLIDE", 0), U("LEVEL", 0.8f), U("VELOCITY", 1.0f),
    U("CUTOFF", 1), U("RES", 0), B("ENV>FILT", 0),   /* the filter after the model, before the LPG: open by default. ENV>FILT both ways: below 0 the envelope shuts the filter from CUTOFF, and it opens back as the envelope falls */
    U("F ATTACK", 0),
    /* from here the SYNTH's extras (CH_SYNTH_XROW): the filter envelope's decay, then the oscillators' tuning (ANALOG) */
    U("F DECAY", 0.5f), {"A OCT", CH_SELECT, 5, 2, 0, octs}, {"A SEMI", CH_SELECT, 25, 12, 0, semis}, B("A FINE", 0),
    {"B OCT", CH_SELECT, 5, 2, 0, octs}, {"B SEMI", CH_SELECT, 25, 12, 0, semis}, B("B FINE", 0),
    /* RIPPLE's: its noise, the sync window, the AM (a ratio of the note, its depth and feedback), the jitter */
    U("NOISE", 0), U("WINDOW", 0), U("AM RATIO", 0.25f), U("AM DEPTH", 0), U("AM FDBK", 0), U("JITTER", 0), U("J RATE", 0.4f),
    {"VOICES", CH_SELECT, 4, 0, 0, synth_voices},    /* the notes at once: 4, 2, 1; LEGATO: one, and a note played over a held one takes its voice on, nothing starting again */
    {"TRIG", CH_SELECT, 2, 0, 0, wob_trig}};          /* WOBBLE's: its sweep running on (FREE), or from the top at each note */
/* the effects (ch_fx.c): each a page of four (DELAY and ECHO a second page) */
static const char *const filter_types[] = {"LOW PASS", "HIGH PASS", "BAND PASS", "NOTCH", 0};
static const char *const filter_slopes[] = {"12 DB", "24 DB", 0};
static const char *const lfo_shapes[] = {"SINE", "TRIANGLE", "SQUARE", 0};
static const char *const off_on[] = {"OFF", "ON", 0};
static const ch_param_desc fx_filter[] = {
    U("CUTOFF", 0.6f), U("RES", 0.2f), {"TYPE", CH_SELECT, 4, 0, 0, filter_types}, {"SLOPE", CH_SELECT, 2, 0, 0, filter_slopes}};
static const char *const drive_types[] = {"TUBE", "DIODE", "NONLINEAR", "OVERDRIVE", "SINE", "CRUSH", 0};
static const char *const drive_clips[] = {"CUBIC", "SIMPLE", "HARD", 0};
static const char *const drive_comp[] = {"OFF", "ON", 0};
static const ch_param_desc fx_drive[] = {U("DRIVE", 0.3f), B("TONE", 0), U("LEVEL", 0.8f), U("MIX", 1),
    {"TYPE", CH_SELECT, 6, 0, 0, drive_types}, U("CHARACTER", 0), {"CLIP", CH_SELECT, 3, 0, 0, drive_clips}, {"COMP", CH_SELECT, 2, 1, 0, drive_comp}};   /* (CHARACTER: GATE, CRUSH's BITS) */   /* (the name CHARACTER shows follows the TYPE: ch_fx_label) */
static const uint8_t drive_order[] = {4, 0, 1, 3, 6, 5, 2, 7};   /* TYPE DRIVE TONE MIX, then CLIP GATE LEVEL COMP */
static const char *const sweep_shapes[] = {"SINE", "TRIANGLE", "SAW UP", "SAW DOWN", "SQUARE", "RANDOM", 0};   /* the sweep's LFO: six shapes */
#define SWEEP_SHAPE {"SHAPE", CH_SELECT, 6, 0, 0, sweep_shapes}
static const ch_param_desc fx_chorus[] = {U("RATE", 0.35f), U("DEPTH", 0.5f), U("MIX", 0.5f), SWEEP_SHAPE};
static const ch_param_desc fx_flanger[] = {U("RATE", 0.3f), U("DEPTH", 0.6f), B("FEEDBACK", 0.5f), U("MIX", 0.5f), SWEEP_SHAPE};
static const char *const phaser_poles[] = {"2", "4", "6", "8", 0};
static const ch_param_desc fx_phaser[] = {U("RATE", 0.35f), U("DEPTH", 0.7f), U("FEEDBACK", 0.4f), U("MIX", 1), SWEEP_SHAPE,
    {"POLES", CH_SELECT, 4, 2, 0, phaser_poles}};   /* its all-passes: two a notch (six, three notches, as it always was) */
static const ch_param_desc fx_tremolo[] = {U("RATE", 0.5f), U("DEPTH", 0.6f), {"SHAPE", CH_SELECT, 3, 0, 0, lfo_shapes}, U("STEREO", 0)};
static const char *const time_modes[] = {"SYNC", "FREE", 0};
static const ch_param_desc fx_delay[] = {
    UM("TIME", 0.55f, time_modes), U("FEEDBACK", 0.35f), U("MIX", 0.35f), B("TONE", -0.2f),
    {"PING PONG", CH_SELECT, 2, 0, 0, off_on}};
static const ch_param_desc fx_echo[] = {U("TIME", 0.6f), U("FEEDBACK", 0.45f), U("MIX", 0.35f), U("WOW", 0.3f), U("TONE", 0.45f)};
static const char *const dub_filters[] = {"BAND", "LOW PASS", "HIGH PASS", "NOTCH", 0};
static const ch_param_desc fx_dub[] = {
    UM("TIME", 0.55f, time_modes), U("FEEDBACK", 0.6f), U("MIX", 0.4f), U("CUTOFF", 0.5f), U("SWEEP", 0.4f),
    {"FILTER", CH_SELECT, 4, 0, 0, dub_filters}, U("RES", 0.68f)};   /* the filter in its feedback: BAND as it always was (RESONANCE 68) */
static const uint8_t dub_order[] = {0, 1, 2, 3, 5, 6, 4};   /* TIME FEEDBACK MIX CUTOFF, then FILTER RESONANCE SWEEP */
static const ch_param_desc fx_dj[] = {B("FILTER", 0), U("RES", 0.15f)};
/* SIDECHAIN's SOURCE: a track (any of its hits), or one lane of a track (a KIT: that drum alone; ch_fx_value_text names it) */
static const char *const side_src[] = {"T1", "T2", "T3", "T4", "T1 L1", "T1 L2", "T1 L3", "T1 L4", "T1 L5", "T1 L6", "T1 L7", "T1 L8", "T2 L1", "T2 L2", "T2 L3", "T2 L4", "T2 L5", "T2 L6", "T2 L7", "T2 L8", "T3 L1", "T3 L2", "T3 L3", "T3 L4", "T3 L5", "T3 L6", "T3 L7", "T3 L8", "T4 L1", "T4 L2", "T4 L3", "T4 L4", "T4 L5", "T4 L6", "T4 L7", "T4 L8", 0};
static const ch_param_desc fx_side[] = {{"SOURCE", CH_SELECT, 4 + CH_TRACKS * CH_KIT_PARTS, 0, 0, side_src}, U("AMOUNT", 0), U("ATTACK", 0.15f), U("RELEASE", 0.55f),   /* (RELEASE 8 ms .. 1.5 s; it started at 30 ms. The default the same 143 ms) */
    {"+SOURCE", CH_SELECT, 2, 0, 0, off_on}};
static const ch_param_desc fx_reso[] = {U("PITCH", 0.5f), U("RES", 0.6f), U("SPREAD", 0), U("MIX", 0.5f)};
static const ch_param_desc fx_tapefx[] = {U("WOW", 0.3f), U("SATURATE", 0.3f), U("TONE", 0.6f), U("HISS", 0.15f), U("MIX", 1)};
static const ch_param_desc fx_vinyl[] = {U("CRACKLE", 0.4f), U("POPS", 0.2f), U("DUST", 0.2f), U("TONE", 0.5f), U("MIX", 1)};
static const ch_param_desc fx_spring[] = {U("TENSION", 0.5f), U("DECAY", 0.5f), U("TONE", 0.5f), U("MIX", 0.35f)};
static const char *const rep_divs[] = {"1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4", 0};
static const char *const rep_every[] = {"1/4", "1/2", "1 BAR", "2 BARS", "4 BARS", 0};
static const char *const rep_offs[] = {"0", "1/16", "2/16", "3/16", "4/16", "5/16", "6/16", "7/16", "8/16", "9/16", "10/16", "11/16", "12/16", "13/16", "14/16", "15/16", 0};
static const ch_param_desc fx_repeat[] = {
    {"WINDOW", CH_SELECT, 6, 2, 0, rep_divs}, {"EVERY", CH_SELECT, 5, 2, 0, rep_every}, U("REPEATS", 0.4f), U("GATE", 1),
    U("CHANCE", 1), U("DECAY", 0.3f), U("MIX", 1), {"HOLD", CH_SELECT, 2, 0, 0, off_on}, {"OFFSET", CH_SELECT, 16, 0, 0, rep_offs}};
/* TRANCE: a gate on a pattern of up to sixteen steps, four to a knob (the first step on the left) */
static const char *const tg_steps[] = {"----", "X---", "-X--", "XX--", "--X-", "X-X-", "-XX-", "XXX-", "---X", "X--X", "-X-X", "XX-X", "--XX", "X-XX", "-XXX", "XXXX", 0};
static const char *const tg_len[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", 0};
static const ch_param_desc fx_trance[] = {
    {"RATE", CH_SELECT, 6, 2, 0, rep_divs}, U("GATE", 0.5f), U("SMOOTH", 0.15f), U("DEPTH", 1),
    {"STEP 1-4", CH_SELECT, 16, 13, 0, tg_steps}, {"STEP 5-8", CH_SELECT, 16, 13, 0, tg_steps},
    {"STEP 9-12", CH_SELECT, 16, 13, 0, tg_steps}, {"STEP 13-16", CH_SELECT, 16, 5, 0, tg_steps},
    {"LENGTH", CH_SELECT, 16, 15, 0, tg_len}};
static const char *const comb_modes[] = {"FREE", "NOTE", "FOLLOW", 0};
static const char *const comb_pol[] = {"+", "-", 0};
static const char *const comb_types[] = {"FEEDBACK", "FEED FWD", 0};
static const char *const comb_src[] = {"OWN", "T1", "T2", "T3", "T4", 0};
static const ch_param_desc fx_comb[] = {
    UM("FREQ", 0.45f, comb_modes), U("FEEDBACK", 0.85f), U("DAMP", 0.25f), U("MIX", 1),   /* (it started at FEEDBACK 70, MIX 50: a ripple of a few dB, hardly a comb) */
    {"POLARITY", CH_SELECT, 2, 0, 0, comb_pol}, {"TYPE", CH_SELECT, 2, 0, 0, comb_types}, U("SPREAD", 0),
    {"FOLLOW", CH_SELECT, 5, 0, 0, comb_src}};
static const char *const crystal_pitch[] = {"-12 ST", "-11 ST", "-10 ST", "-9 ST", "-8 ST", "-7 ST", "-6 ST", "-5 ST", "-4 ST", "-3 ST", "-2 ST", "-1 ST",
    "0 ST", "+1 ST", "+2 ST", "+3 ST", "+4 ST", "+5 ST", "+6 ST", "+7 ST", "+8 ST", "+9 ST", "+10 ST", "+11 ST", "+12 ST", 0};
static const ch_param_desc fx_crystal[] = {   /* CRYSTAL: an octave up, reversed, at a sixteenth */
    {"PITCH", CH_SELECT, 25, 24, 0, crystal_pitch}, UM("TIME", 0.3f, time_modes), U("FEEDBACK", 0.5f), U("MIX", 0.35f),
    {"REVERSE", CH_SELECT, 2, 1, 0, off_on}, U("SPREAD", 0.6f)};
/* ERODE: a short delay shaken by a band of noise, or by a sine: grit and air on whatever goes through. All of it is the
 * shaken sound (AMOUNT at 0: nothing) */
static const char *const erode_modes[] = {"NOISE", "WIDE", "SINE", 0};
static const ch_param_desc fx_erode[] = {U("FREQ", 0.6f), U("WIDTH", 0.5f), U("AMOUNT", 0.35f), {"MODE", CH_SELECT, 3, 0, 0, erode_modes}};
/* DISPERSE: a chain of all-passes round FREQ: nothing is louder or quieter, the sound round FREQ comes late (a hit
 * becomes a chirp). AMOUNT: how many, PINCH: how narrow */
static const ch_param_desc fx_disperse[] = {U("FREQ", 0.4f), U("PINCH", 0.35f), U("AMOUNT", 0.5f), U("MIX", 1)};
static const char *const unison_voices[] = {"2", "4", "6", 0};
static const ch_param_desc fx_unison[] = {U("DETUNE", 0.4f), {"VOICES", CH_SELECT, 3, 1, 0, unison_voices}, U("SPREAD", 0.8f), U("MIX", 0.6f)};
static const char *const reverb_types[] = {"HALL", "ROOM", "PLATE", "GATED", 0};
static const ch_param_desc fx_reverb[] = {U("SIZE", 0.5f), U("DECAY", 0.45f), U("DAMP", 0.4f), U("MIX", 0.3f),
    U("DUCK", 0), {"TYPE", CH_SELECT, 4, 0, 0, reverb_types}};   /* DUCK: the reverb dips while the dry sound plays, and swells back in the gaps */

static const char *const poly_actions[] = {"LOAD", "LOOP MODE", "TRIG", 0};
static const char *const drum_actions[] = {"TRIG", 0};
static const char *const none[] = {0};

/* the pages' order: SYNTH leads with the model and its three knobs, then PITCH DECAY LPG MODE, then ATTACK GLIDE LEVEL VELOCITY;
 * SAMPLE with SAMPLE PITCH START LENGTH, then the envelope, then LOOP XFADE and the filter, LEVEL and VELOCITY last */
static const uint8_t synth_order[] = {0, 2, 3, 4, 23, 25, 26, 27, 12, 13, 14, 5, 15, 16, 1, 6, 7, 30, 8, 9, 10, 11, 24, 28, 29, 31, 17, 18, 19, 20, 21, 22};
/* MODEL + its three; (RIPPLE: NOISE and the AM); the filter + DECAY; its envelope, PITCH, LPG; MODE ATTACK GLIDE LEVEL; VELOCITY + OSC A
 * (RIPPLE: + WINDOW, JITTER, J RATE); OSC B. A model shows only what it uses (ch_synth_param_shown) */
typedef char synth_order_complete[sizeof synth_order == 32 ? 1 : -1];   /* every SYNTH parameter in the page order */
static const uint8_t poly_order[] = {0, 1, 2, 3, 7, 8, 9, 10, 5, 6, 12, 13, 4, 11};
#define N(a) (uint8_t)(sizeof a / sizeof a[0])
static const ch_device_desc devices[CH_DEVICES] = {
    {"BYPASS", CH_MATERIAL, 0, 0, none},
    {"KIT", CH_MATERIAL, N(kit), kit, drum_actions, kit_order},   /* (the retired DISC's number) */
    {"SAMPLE", CH_MATERIAL, N(poly), poly, poly_actions, poly_order},
    {"DRUM", CH_MATERIAL, N(drum), drum, drum_actions},
    {"SYNTH", CH_MATERIAL, N(synth), synth, drum_actions, synth_order},
    {"GRAIN", CH_MATERIAL, N(grain_p), grain_p, drum_actions},
    {"FILTER", CH_FX1, N(fx_filter), fx_filter, none},
    {"DRIVE", CH_FX1, N(fx_drive), fx_drive, none, drive_order},
    {"COMB", CH_FX1, N(fx_comb), fx_comb, none},        /* (the retired BITCRUSH's number) */
    {"CHORUS", CH_FX1, N(fx_chorus), fx_chorus, none},
    {"FLANGER", CH_FX1, N(fx_flanger), fx_flanger, none},
    {"PHASER", CH_FX1, N(fx_phaser), fx_phaser, none},
    {"TREMOLO", CH_FX1, N(fx_tremolo), fx_tremolo, none},
    {"DELAY", CH_FX1, N(fx_delay), fx_delay, none},
    {"ECHO", CH_FX1, N(fx_echo), fx_echo, none},
    {"DUB DELAY", CH_FX1, N(fx_dub), fx_dub, none, dub_order},
    {"REVERB", CH_FX1, N(fx_reverb), fx_reverb, none},
    {"DJ FILTER", CH_FX1, N(fx_dj), fx_dj, none},
    {"SIDECHAIN", CH_FX1, N(fx_side), fx_side, none},
    {"RESONATOR", CH_FX1, N(fx_reso), fx_reso, none},
    {"TAPE", CH_FX1, N(fx_tapefx), fx_tapefx, none},
    {"VINYL", CH_FX1, N(fx_vinyl), fx_vinyl, none},
    {"SPRING", CH_FX1, N(fx_spring), fx_spring, none},
    {"REPEAT", CH_FX1, N(fx_repeat), fx_repeat, none},
    {"CRYSTAL", CH_FX1, N(fx_crystal), fx_crystal, none},
    {"TRANCE", CH_FX1, N(fx_trance), fx_trance, none},
    {"ERODE", CH_FX1, N(fx_erode), fx_erode, none},
    {"DISPERSE", CH_FX1, N(fx_disperse), fx_disperse, none},
    {"UNISON", CH_FX1, N(fx_unison), fx_unison, none}};
const ch_device_desc *ch_device_info(unsigned d) { return d < CH_DEVICES ? &devices[d] : 0; }

static const char *const mod_shapes[] = {"SINE", "TRIANGLE", "SAW UP", "SAW DOWN", "SQUARE", "STEPS", "DRIFT", 0};
/* AMOUNT's mode (SEQ + its knob): BIPOLAR, the wave either side of where the knob it moves is set; UNIPOLAR, from
 * there upward only, nothing to AMOUNT (a map's depth under 0 turns it downward) */
static const char *const polar_modes[] = {"BIPOLAR", "UNIPOLAR", 0};
/* the LFO: SHAPE picks the wave, STEPS (sample and hold) or DRIFT (a smooth random walk); on those SKEW is VARIATION */
static const ch_param_desc lfo_p[CH_MOD_PARAMS] = {
    UM("RATE", 0.5f, sync_modes), UM("AMOUNT", 1, polar_modes), {"SHAPE", CH_SELECT, CH_LFO_SHAPES, 0, 0, mod_shapes}, B("OFFSET", 0),
    U("PHASE", 0), B("SKEW", 0), U("SMOOTH", 0), U("SPREAD", 0)};
static const char *const follow_src[] = {"TRACK IN", "MATERIAL", "TRACK 1", "TRACK 2", "TRACK 3", "TRACK 4", "MIX", 0};
static const char *const hold_modes[] = {"OFF", "FREE", "STRAIGHT", "TRIPLET", "DOTTED", 0};
static const ch_param_desc follow_p[CH_MOD_PARAMS] = {
    U("DELAY", 0), U("AMOUNT", 1), U("RISE", 0.1f), U("FALL", 0.3f),
    UM("INPUT", 0.5f, follow_src), B("FILTER", 0), UM("HOLD", 0, hold_modes), U("SPREAD", 0)};
static const ch_param_desc adsr_p[CH_MOD_PARAMS] = {
    U("ATTACK", 0.05f), U("DECAY", 0.3f), U("SUSTAIN", 0.6f), U("RELEASE", 0.3f),
    B("ATT CURVE", 0), B("DEC CURVE", 0), B("REL CURVE", 0), U("SPREAD", 0)};
static const ch_param_desc trig_p[CH_MOD_PARAMS] = {        /* TRIG RND: a value drawn on each of the track's notes */
    U("CHANCE", 1), UM("AMOUNT", 1, polar_modes), U("SMOOTH", 0), B("OFFSET", 0),   /* (AMOUNT's mode as an LFO's: both ways from the knob, or upward only) */
    U("SPREAD", 0), U("-", 0), U("-", 0), U("-", 0)};
static const ch_param_desc *const mod_tables[CH_MOD_TYPES] = {lfo_p, lfo_p, follow_p, adsr_p, trig_p};
static const char *const mod_names[CH_MOD_TYPES] = {"LFO", "LFO", "FOLLOW", "ADSR", "TRIG RND"};
const ch_param_desc *ch_mod_param_info(unsigned t, unsigned p)
{
    return t < CH_MOD_TYPES && p < CH_MOD_PARAMS ? &mod_tables[t][p] : 0;
}
const char *ch_mod_name(unsigned t) { return t < CH_MOD_TYPES ? mod_names[t] : ""; }
static void hz_cat(char *o, float hz)
{
    unsigned k = 0; while (o[k]) ++k;
    if (hz < 10) { unsigned t = (unsigned)(hz * 10 + 0.5f); o[k++] = (char)('0' + t / 10); o[k++] = '.'; o[k++] = (char)('0' + t % 10); }
    else { unsigned n = (unsigned)(hz + 0.5f), d = n >= 100 ? 100 : n >= 10 ? 10 : 1; for (; d; d /= 10) o[k++] = (char)('0' + n / d % 10); }
    o[k++] = 'H'; o[k++] = 'Z'; o[k] = 0;
}
int ch_mod_value_text(unsigned t, unsigned p, float v, unsigned mode, char *out)
{
    out[0] = 0;
    if ((t == CH_MOD_WAVE || t == CH_MOD_RANDOM) && p == 0) {
        if (mode) { const char *n = ch_div_names[ch_div_index(v)]; unsigned k = 0; while (n[k]) { out[k] = n[k]; ++k; } out[k] = 0; }
        else hz_cat(out, ch_exp_map(v, 0.01f, 50.0f));
        return 1;
    }
    if (t == CH_MOD_FOLLOW && p == 6 && mode >= 2) { const char *n = ch_div_names[ch_div_index(v)]; unsigned k = 0; while (n[k]) { out[k] = n[k]; ++k; } out[k] = 0; return 1; }
    return 0;
}
const char *ch_mod_label(unsigned t, unsigned shape, unsigned p)
{
    return (t == CH_MOD_WAVE || t == CH_MOD_RANDOM) && p == 5 && shape >= CH_LFO_STEPS ? "VARIATION" : 0;
}

/* Grid divisions as whole notes: 4/1 .. 1/64. */
const float ch_grid_beats[CH_GRID] = {16, 8, 4, 2, 1, 0.5f, 0.25f, 0.125f, 0.0625f};
/* Modulator / rate divisions, slowest first: 16 bars .. 1/64. */
/* the synced divisions, long to short so the knob turns the same way as in FREE (up: faster): whole notes 8 .. 1, then
 * 1/2 .. 1/128, in beats */
const float ch_div_beats[CH_DIVS] = {32, 28, 24, 20, 16, 12, 8, 4, 2, 1, 0.5f, 0.25f, 0.125f, 0.0625f, 0.03125f};
const char *const ch_div_names[CH_DIVS] = {"8", "7", "6", "5", "4", "3", "2", "1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/128"};
unsigned ch_div_index(float v) { return (unsigned)(ch_clamp(v, 0, 1) * (CH_DIVS - 1) + 0.5f); }
float ch_div_value(unsigned i) { return (float)(i < CH_DIVS ? i : CH_DIVS - 1) / (float)(CH_DIVS - 1); }
unsigned ch_div_count(void) { return CH_DIVS; }

float ch_division_beats(float v, unsigned mode)
{
    /* mode 1 STRAIGHT, 2 TRIPLET, 3 DOTTED. */
    float b = ch_div_beats[ch_div_index(v)];
    return mode == 2 ? b * (2.0f / 3) : mode == 3 ? b * 1.5f : b;
}
/* Nearest in-scale semitone. Scale 0 (CHROMATIC) rounds to semitones; a
 * negative scale leaves the value continuous. */
volatile uint16_t ch_key_mask;
volatile uint8_t ch_key_root;
void ch_key_set(unsigned root, unsigned mask) { ch_key_root = (uint8_t)(root % 12u); ch_key_mask = (uint16_t)(mask & 0xfffu); }
float ch_key_quantize(float semis, unsigned root)
{
    uint16_t mask = ch_key_mask;
    if (!mask) return semis;
    int n = (int)(semis + (semis < 0 ? -0.5f : 0.5f));
    for (int d = 0; d < 12; ++d) {
        int up = n + d, dn = n - d;
        if (mask >> ((((up - (int)root) % 12) + 12) % 12) & 1) return (float)up;
        if (mask >> ((((dn - (int)root) % 12) + 12) % 12) & 1) return (float)dn;
    }
    return (float)n;
}
float ch_scale_quantize(float semis, int scale)
{
    if (scale < 0 || scale >= CH_SCALES) return semis;
    int n = (int)(semis + (semis < 0 ? -0.5f : 0.5f));
    uint16_t mask = ch_scale_mask[scale];
    for (int d = 0; d < 12; ++d) {
        int up = n + d, dn = n - d;
        if (mask >> (((up % 12) + 12) % 12) & 1) return (float)up;
        if (mask >> (((dn % 12) + 12) % 12) & 1) return (float)dn;
    }
    return (float)n;
}
