/* PING OS-1 1.0: the panel, the screen and the glue to the engine (Chisel).
 * Part of fm1-base's unity build (device/unity.c, host/plat_host.c) or the
 * Windows simulator (sim/), after the platform: gfx.c and announce.c are in scope.
 *
 * The panel:
 *   EDIT                  the engine (tap: open, again: next page; hold: off/on)
 *   FX                    the four FX slots, one after another (hold: that slot's bypass); ALGO: its effect (FILTER
 *                         DRIVE CHORUS FLANGER PHASER TREMOLO DELAY ECHO REVERB, or empty), SELECT: its pages
 *   K1 K2 K3 K4          the four tiles on the screen, left to right (SELECT: the page);  PRESET: the track
 *   SELECT                the page
 *   OCT- / OCT+           the keyboard's octave (lit while it is shifted); in STEP, or SEQ held: the track's lanes an octave
 *   SEQ tap               the STEP view (again: back);  ENV tap: in the sequencer views the keys play the track (again: steps);  SEL tap: the BROWSER
 *   SEL held + ALGO       the track's level, from any view
 *   EDIT held + ALGO      the track's own key (OFF C..B; it overrides the master key); EDIT + SELECT: its scale
 *   EDIT held + OCT-/+    the track's lanes a semitone down / up; in a key, to the next scale note
 *   SEQ held + a knob     that parameter's mode (STEP view: the track's length, rate, direction, swing)
 *   ALGO (the EDIT page)  the engine: DRUM SYNTH SAMPLE GRAIN off (SEQ + SELECT too)
 *   SEQ held + white 1-4  the device's actions;   REC: its main action (DRUM, SYNTH: a hit)
 *   LFO                   the modulators; hold LFO and turn a parameter to map it to the chosen one; LFO + SEL held:
 *                         to the track's TRIG RND modulator (MOD 4 to start: a new random value on every note)
 *   ENV held + key 1-4    that track muted / unmuted (white or black 1-4), from any view; the keys show the muted ones
 *   GLO                   the mix (held: the metronome on / off);  PLAY: transport;  SAVE: the project into the flash (loaded again at power-on)
 *   GLO held + FX         the MASTER chain: DJ FILTER, DELAY, REVERB, SIDECHAIN to start, any effect in any slot; FX
 *                         steps through its slots, ALGO the slot's effect, SELECT the pages; EDIT or a view button leaves
 *   HOME + FX             PERFORM: K1..K4 the master effects' amounts (FILTER, DELAY MIX, REVERB MIX, the fourth's);
 *                         there, HOME held: the amounts turned meanwhile go back to where they were when it is let go
 *   SAVE held + white 1-16  the track's sound saved as that preset; EDIT held + white 1-16: that preset into the track
 *   HOME held + white 1-16  the pattern (all four tracks' sequences): at the next bar while playing, at once stopped;
 *                         with SEQ held too: the pattern playing copied into that one.  GLO's SONG page chains them
 *   HOME                  the TRACKS view (again: back): the four tracks' patterns, a row each, lanes merged;
 *                         white keys: the chosen track's steps (an empty one: its root lane on; a set one: cleared);
 *                         black 1-4: the track; ALGO: the track; SELECT: the step; K1..K4, PRESET: the cell;
 *                         SEQ held + K1..K4: LENGTH RATE DIRECTION SWING, SEQ + ALGO: TEMPO
 *   keys                  the track, played chromatically (C4 = the sound as tuned)
 * The STEP view (after PING Step): the track's 8 note lanes, 16 steps a page
 *   black keys 1-8        the lanes, low to high: press = choose the lane (and hear it)
 *   white keys            the page's 16 steps on the chosen lane: tap on/off; hold and turn to edit;
 *                         hold a step and press black keys: those lanes on that step (chords)
 *   SELECT / ALGO         the cursor: along the steps / up and down the lanes
 *   K1..K4, PRESET        the chosen cell: VELOCITY RATCHET CHANCE MICRO, CYCLE (on an empty step PRESET is the track)
 *   SEQ held + knobs      LENGTH RATE DIRECTION SWING, ALGO SCALE;  SEQ + SELECT: the page;  SEQ + PRESET: the track
 *   a lane's key + SELECT the track's ROOT (the lanes laid out again from it); a lane's key + K1: that lane's note
 *   black keys 9 10 11    the chosen step on / off (either);  the track's mute.  OCT- / OCT+: all lanes an octave
 *   PRESET                the track
 *   REC                   live record (a played note writes its lane where the playhead is); SEQ + REC clears the track
 *                         (in TRACKS: the whole pattern; on the SONG page: the song)
 *   SEQ + ENV             the envelope display on the EDIT page (SYNTH, SAMPLE, GRAIN), in place of the track footer;
 *                         while it shows, K1..K4 are the envelope's own parameters (ATTACK DECAY SUSTAIN/MODE CURVE/LPG)
 *   a step held + a later step   ties the steps between to it: one long note (its bar runs on, unbroken)
 *   REC held + steps      arms them for parameter locks and starts recording (REC blinks): any EDIT-page knob turned
 *                         sets that parameter on those steps only (the sound, MODEL included, can change every step);
 *                         REC held + a step again un-arms it; a REC tap ends the recording
 *   GLO, the AUTOPILOT page  while playing, the sounds varied at random every 1 to 16 bars (and lines redrawn, from VARIETY 50 %);
 *                         off: back as it was (REVERT) or kept (OVERRIDE)
 *   ALGO / PRESET         with the MIXER footer on the screen: ALGO the chosen strip's level, PRESET round the four tracks and the RETURN
 *                         (SEL + ALGO: the engine or the effect, as ALGO without it)
 *   LFO + K1..K4          on MIX LEVEL / PAN / FILTER: the chosen modulator (of the knob's track) onto that track's level, pan or filter
 *   SEQ + FX              on a TRANCE effect's page: the white keys are the gate's sixteen steps, on / off (again: notes)
 *   SEQ + white 8-11      the page (steps 1-16, 17-32, 33-48, 49-64 of a long pattern)
 *   SEQ + white 12-16     generators: a new melody in the track's key, a new beat, the engine's parameters locked to
 *                         random values on every used step, every used step on a random lane, a variation
 *   ARP                   the line generator: TYPE DENSITY ACCENT GATE / OCTAVE OFFSET STEPS SWING on K1-K4 (SELECT: the
 *                         page); the knobs morph the drawn line (nothing is redrawn), ARP again draws new material;
 *                         EDIT + ARP opens it without touching the track, until ARP alone. Leaving the page keeps the line.
 *                         GATE is the engine's decay (the pattern is untouched). On the page: white 1-4 keep or redraw
 *                         NOTE ACCENT GATE OCTAVE when ARP draws again, black 1-8 the lanes the line may use
 * Outside the STEP view the footer shows the four tracks' patterns, lanes merged.
 */
#include "fastmath.h"
#include "chisel.h"
#include "ch_dsp.h"                                  /* (the speed test times the engine's own tanh, exp2 and sin) */
#include "ch_fe.h"
#include "logo.h"
#include "ping_logo.h"

enum { V_DEVICE, V_MOD, V_MIX, V_STEP, V_SONG, V_PERFORM, V_GEN, V_SCOPE, V_HELP };   /* V_HELP: GLO + SAVE, the panel drawn and what each control does */
/* (was)   /* V_SCOPE: GLO + HOME, the output as a figure */
/* V_SONG: HOME, the four tracks' patterns as one grid;
                                                              * V_PERFORM: HOME + FX, the master effects' amounts on K1..K4;
                                                              * V_GEN: ARP, the line generator's settings on K1..K4 (two pages) */
/* the track the device view edits: the master chain (GLO + FX) or the track */
#define ET (ui.mfx ? CH_MASTER : ui.track)
static const uint8_t SLOT_BTN[CH_SLOTS] = {B_EDIT, B_ARP, B_SEL, B_ENV, B_FX};
static const char *const SLOT_NAME[CH_SLOTS] = {"ENGINE", "FX 1", "FX 2", "FX 3", "FX 4"};
static const char *const MSLOT_NAME[CH_SLOTS] = {"MASTER", "MASTER 1", "MASTER 2", "MASTER 3", "MASTER 4"};
static const uint8_t ENC_OF[6] = {EN_K1, EN_K2, EN_K3, EN_K4, EN_ALGO, EN_PRESET};
static const char *const ENC_LBL[6] = {"K1", "K2", "K3", "K4", "ALG", "PRE"};
static const uint8_t WK[16] = PLAT_WHITE_KEYS;
static const uint8_t BK[11] = PLAT_BLACK_KEYS;
enum { PER_PAGE = 4 };                                     /* the four knobs under the screen, K1..K4: one tile each */
/* black and white, and one warm orange: what is on, sounding or touched */
static uint8_t pal808 = 1;        /* the tracks in the warm colours, red orange yellow white; the accent cyan, modulation violet */
void app_ui_pal(int on) { pal808 = (uint8_t)on; }
#define C_ACC (pal808 ? RGB(64, 206, 196) : RGB(255, 132, 36))
#define C_ACC_HI (pal808 ? RGB(170, 236, 230) : RGB(255, 196, 140))
#define C_MOD (pal808 ? RGB(51, 178, 159) : RGB(100, 136, 197))   /* modulation: a soft blue (teal in the 808 colours, as the modulators' panel) */
/* each track its colour, BreakMorph's (its Spectral palette's four, the ramp sampled red to teal, as its hits are
 * coloured in time order): reddish orange, yellow, off white, turquoise; and the same at half, for what is less */
static const uint16_t TRACK_COL_C[4] = {RGB(255, 92, 61), RGB(221, 220, 1), RGB(228, 230, 186), RGB(51, 178, 159)};
static const uint16_t TRACK_DIM_C[4] = {RGB(128, 46, 30), RGB(110, 110, 0), RGB(114, 115, 93), RGB(25, 89, 80)};
static const uint16_t TRACK_REST_C[4] = {RGB(166, 60, 40), RGB(144, 143, 1), RGB(148, 150, 121), RGB(33, 116, 103)};   /* a label at rest (65 %): a hit flashes it white */
static const uint16_t TRACK_HIT_C[4] = {RGB(255, 92, 61), RGB(221, 220, 1), RGB(150, 152, 110), RGB(51, 178, 159)};      /* white text on a hit: T3's cream is near white, so a darker shade */

/* cycles a step can be on: off, then a:b */
static const uint8_t CYC[10][2] = {{0, 0}, {1, 2}, {2, 2}, {1, 3}, {2, 3}, {3, 3}, {1, 4}, {2, 4}, {3, 4}, {4, 4}};
static const char *const RATE_NAME[5] = {"1/32", "1/16", "1/8T", "1/8", "1/4"};
static const char *const DIR_NAME[CH_DIRS] = {"FWD", "REV", "PEND", "RND"};
static const char *const NOTE_NAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

static struct {
    uint8_t view, track, slot, page[CH_SLOTS], mod, mod_page, mix_page, step_page, spd_show;
    uint32_t btn, keys;
    uint32_t down_at[NB];
    uint8_t held_long[NB];
    char msg[2][24];
    uint32_t msg_until;
    uint32_t hint_until;              /* the first power-on's card (GLO + SAVE: HELP): not 0 while it is up; it stays until a button or a key */
    uint8_t count_seen;               /* the count-in's beat last shown */
    uint8_t rec_micro;                /* RECORD, K1: a played note is put down with its MICRO timing, where in its step it fell */
    uint8_t snd_off;                  /* bits 1, 2: SETUP's K4, HOLD FX, the master slot HOME + K3 plays (0..3). (Bit 0 was a switch for the power-on sound: unused) */
    uint8_t count_off;                /* RECORD, K2: the count-in before a recording: 0 a bar (as it starts), 1 none, 2 two bars */
    uint8_t help_told;                /* that card has been shown once (kept in OBJ_SETTINGS) */
    uint8_t redraw, temp, temp_latched, seq_used, live_rec, splash;
    uint32_t home_tap, boot_ms;
    int32_t last_note[NKEYS];
    int16_t sel_step, held_step;      /* the step being edited; the one under a finger, -1 none */
    uint32_t held_at;                 /* when that step went down (a tie needs it held a third of a second) */
    uint8_t step_edited, step_was_on;
    uint8_t sel_lane;                 /* the lane being edited (0 = the lowest) */
    uint8_t scale[CH_TRACKS], root[CH_TRACKS];   /* how the lanes were laid out (the notes are the engine's) */
    uint8_t tkey[CH_TRACKS];          /* the track's own key: 0 none (the master key, if on), 1..12 C..B; it overrides the master key */
    uint8_t audition[11];             /* a lane's note sounding while its black key is held */
    uint32_t init_until;
    int8_t oct;                       /* the keyboard's octave shift, -3..+3 (OCT- / OCT+) */
    int8_t lane_held;
    uint8_t kb_rec;
    uint8_t fxpos;                    /* the effect place (0..3) the FX view is on */
    uint8_t mfx, glo_used;            /* the device view is on the master chain; GLO was used as a shift (no tap, no hold) */
    uint8_t perf_hold;                /* PERFORM: HOME held, the amounts going back to these when it is let go */
    uint8_t lane_edit;                /* the step view's LANE NOTES mode (SEQ + EDIT): SELECT the lane, K1 its note; SEQ ends it */
    float gen[8]; uint8_t gen_page; uint32_t gen_seed;   /* the line generator (ARP): its settings, page, and the seed of the line shown */
    uint32_t gen_nseed;                                  /* the seed of its notes: every TYPE value draws its own list from it (kept while NOTE is locked) */
    uint8_t gen_sting;                                   /* the density draw: 0 GROOVE (the beats first, step 1 always), 1 Sting's, any step */
    uint8_t gen_lock, gen_lanes;                         /* its lane locks (NOTE ACCENT GATE OCTAVE: bit set = kept) and the lanes it may use */
    uint32_t entropy;                                    /* stirred every frame by the clock and the keys: the generators' seed */
    uint8_t gen_pat[CH_STEPS][3];                        /* its drawn material (gen_draw) */
    uint16_t gen_dens[4][16];                            /* Sting's density: for each 16-step page, list k (bits) = k steps picked at random; DENSITY picks the list */
    uint8_t gen_hold;                                    /* EDIT + ARP: the settings change without touching the track, until ARP alone */
    uint8_t env_show;                 /* the envelope display in place of the track footer (SEQ + ENV) */
    uint8_t lock_rec, rec_used;       /* parameter locks being recorded (REC blinks); REC was used as a shift this press */
    uint8_t lock_track;               /* the track whose steps are armed */
    uint32_t lock_steps[2];           /* the armed steps (bit per step, 64) */
    uint8_t undo_fwd;                 /* EDIT + REC: the last swap was a redo (the message) */
    uint8_t scope_init, spec_src, spec_tone;   /* the spectrum (GLO + HOME): set up once; its source, 0 the master, 1..4 a track;
                                                           * the tone MONO draws in, 0..23 round the wheel, 24 white */
    uint8_t mono;                     /* the whole UI in that tone (ui_blit): GLO's DISPLAY page, or the spectrum's MONO */
    uint8_t sync_host;                /* SEND DIRECT: the USB port is a host (plat_usb_host) */
    uint8_t sync;                     /* MIDI SYNC (DISPLAY, K4; kept in OBJ_SETTINGS): 0 OFF, 1 SEND the clock, start and stop, 2 FOLLOW them */
    uint8_t solo_mask;                /* the tracks soloed together (a bit each), 0: none set here */
    uint8_t panel_white;              /* the PANEL's row on the knobs: white buttons, not the track's colour (DISPLAY, K3) */
    uint8_t panel;                    /* the engine and effect pages as a PANEL: every page at once, the one on the knobs lit (the third layout of the EDIT tap) */
    uint8_t mix_ret;                  /* the MIXER footer: the RETURN strip is the chosen one (PRESET goes round to it), not a track */
    uint8_t mixfoot;                  /* the footer as a MIXER, four meters, not the tracks' rows (EDIT + GLO) */
    uint8_t help_sel, help_back;      /* HELP: the control it tells of (0 none, 1.. a button, 15.. a knob, 22 the white keys, 23 the black), the view to go back to */
    uint8_t spec_tilt;                /* the spectrum's TILT (ALGO there): 0 FLAT, the levels as they are; 1..3 the highs lifted 3, 4.5, 6 dB an octave about 1 kHz */
    float spec_view;                  /* its view, 0 front-on .. 1 the angle */
    uint8_t stut, stut_all;           /* the stutter held (HOME + K1 all tracks, K2 this one): the division + 1, 0 off; its scope */
    uint8_t cur_pat, home_used, was_playing, song_edit, save_used;   /* the pattern playing (0..15); HOME used as a shift; the song page's entry */
    int8_t stash_no;                  /* the pattern whose edits the engine's stash holds, -1 none */
    uint8_t env_used;                 /* ENV went down and did something else (a mute, SEQ + ENV): its release is not the tap */
    uint8_t kb_live, sel_used;        /* (an ENV tap): in the sequencer views the keys play the track instead of setting steps;
                                       * sel_used: SEL was held as a shift (ALGO: the level; with LFO: a map), no toggle */
    float perf_was[CH_FX]; uint8_t perf_byp[CH_FX];                   /* REC with a track key held: the whole keyboard plays and records that track */                 /* a lane's black key under a finger (step view), -1 none: K1 sets its note */
    uint8_t pad_note[CH_TRACKS];      /* a track pad's note sounding while its black key is held (main views, REC) */
    uint16_t hits_seen[CH_TRACKS];    /* the engine's hit counts as last seen; a change flashes the track ~80 ms */
    uint32_t hit_until[CH_TRACKS];
    uint8_t mac_arm;                  /* the macro being assigned (OCT- + OCT+ held, white 1-4): 1..4, 0 none; a knob turned maps its parameter */
    uint8_t mac_play;                 /* OCT- + OCT+ tapped together: K1..K4 play the four macros */
    uint8_t gate_keys;                /* SEQ + FX on a TRANCE effect's page: the white keys are its sixteen steps (again: back to notes) */
    uint8_t step_alt;                 /* EDIT + OCT: the step knobs' second page, K2 SEND, K3 CYCLE and K4 GATE */
    uint8_t oct_chord, oct_keyed, oct_edit;   /* the two OCT buttons down together; a white key went with them; EDIT held as each went down */
    uint8_t key_root, key_scale;      /* the master key: root 0 OFF, 1..12 C..B; its scale (SCALE_NAME) */              /* SEQ + SAVE asked "INIT?": SAVE the track, REC everything, until then */
} ui;

/* MONO: the four tracks in one colour (track 1's, which the tone then replaces), so a step is the same shade whichever
 * track it is on (their own colours differ in brightness, and the tone keeps a pixel's brightness) */
static const uint16_t TRACK_COL_M[4] = {RGB(255, 92, 61), RGB(255, 92, 61), RGB(255, 92, 61), RGB(255, 92, 61)};
static const uint16_t TRACK_DIM_M[4] = {RGB(128, 46, 30), RGB(128, 46, 30), RGB(128, 46, 30), RGB(128, 46, 30)};
static const uint16_t TRACK_REST_M[4] = {RGB(166, 60, 40), RGB(166, 60, 40), RGB(166, 60, 40), RGB(166, 60, 40)};
static const uint16_t TRACK_COL_8[4] = {RGB(226, 52, 40), RGB(245, 128, 32), RGB(248, 206, 44), RGB(233, 232, 201)};   /* (the white: halfway back to the cream it was) */
static const uint16_t TRACK_DIM_8[4] = {RGB(113, 26, 20), RGB(122, 64, 16), RGB(124, 103, 22), RGB(116, 116, 100)};
static const uint16_t TRACK_REST_8[4] = {RGB(146, 33, 26), RGB(159, 83, 20), RGB(161, 133, 28), RGB(151, 151, 131)};
#define TRACK_COL (ui.mono ? TRACK_COL_M : pal808 ? TRACK_COL_8 : TRACK_COL_C)
#define TRACK_DIM (ui.mono ? TRACK_DIM_M : pal808 ? TRACK_DIM_8 : TRACK_DIM_C)
#define TRACK_REST (ui.mono ? TRACK_REST_M : pal808 ? TRACK_REST_8 : TRACK_REST_C)
#define TRACK_HIT (ui.mono ? TRACK_COL_M : pal808 ? TRACK_COL_8 : TRACK_HIT_C)
/* MONO (GLO's DISPLAY page, or the spectrum's; K2 / K4 TONE): what is drawn in colour, the steps, the track colours, the
 * accent, the spectrum, in one tone instead. Each band of the canvas, finished, is gone over just before it goes to the
 * screen: a coloured pixel takes the tone at its own brightness (its brightest channel); white, the greys and black, the
 * text and the frames, stay as they are. */
static void tone_rgb(unsigned tone, float *r, float *g, float *b)
{
    if (tone >= 24) { *r = *g = *b = 235; return; }       /* white */
    float hh = (float)tone * 0.25f, x = 1 - fm_fabsf(hh - 2 * fm_floorf(hh * 0.5f) - 1), c[3] = {0, 0, 0};   /* a hue every 15 degrees */
    int sx = (int)hh;
    if (sx == 0) { c[0] = 1; c[1] = x; } else if (sx == 1) { c[0] = x; c[1] = 1; } else if (sx == 2) { c[1] = 1; c[2] = x; }
    else if (sx == 3) { c[1] = x; c[2] = 1; } else if (sx == 4) { c[0] = x; c[2] = 1; } else { c[0] = 1; c[2] = x; }
    *r = 40 + 215 * c[0]; *g = 40 + 215 * c[1]; *b = 40 + 215 * c[2];   /* a little short of full saturation */
    {   /* the dark hues (the blues, violets and reds) lifted toward white until they are as easy to read as the rest:
         * every tone at least a set brightness to the eye, so a step or a bar shows on the panel whatever the hue */
        float y = 0.299f * *r + 0.587f * *g + 0.114f * *b;
        if (y < 150) { float k = (150 - y) / (255 - y); *r += (255 - *r) * k; *g += (255 - *g) * k; *b += (255 - *b) * k; }
    }
}
static void ui_blit(uint32_t x, uint32_t y)
{
    if (ui.mono) {
        float tr, tg, tb; tone_rgb(ui.spec_tone, &tr, &tg, &tb);
        uint32_t R = (uint32_t)tr, G = (uint32_t)tg, B = (uint32_t)tb, n = cv_w * cv_h;
        for (uint32_t i = 0; i < n; ++i) {
            uint32_t p = cv_px[i];
            if (!p) continue;
            uint32_t c = ((p >> 8) | (p << 8)) & 0xFFFFu, r2 = (c >> 11) * 2, g = c >> 5 & 63u, b2 = (c & 31u) * 2;
            uint32_t v = r2 > g ? r2 : g; if (b2 > v) v = b2;   /* 0..63 */
            uint32_t mn = r2 < g ? r2 : g; if (b2 < mn) mn = b2;
            if (v - mn < 8) continue;                    /* near grey: white text, the greys, the frames: left alone */
            cv_px[i] = swap16(RGB(R * v / 63, G * v / 63, B * v / 63));
        }
    }
    cv_blit(x, y);
}
#define cv_blit ui_blit

/* scales for laying out the lanes: lane 1 on the root, then up the scale */
enum { SCALES = 12 };
static const char *const SCALE_NAME[SCALES] = {"MAJOR", "MINOR", "DORIAN", "PHRYG", "LYDIAN", "MIXO",
                                               "HARM MIN", "PENT MAJ", "PENT MIN", "BLUES", "WHOLE", "CHROMA"};
static const uint8_t SCALE_N[SCALES] = {7, 7, 7, 7, 7, 7, 7, 5, 5, 6, 6, 12};
static const uint8_t SCALE_IV[SCALES][12] = {
    {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 9, 10}, {0, 1, 3, 5, 7, 8, 10},
    {0, 2, 4, 6, 7, 9, 11}, {0, 2, 4, 5, 7, 9, 10}, {0, 2, 3, 5, 7, 8, 11}, {0, 2, 4, 7, 9},
    {0, 3, 5, 7, 10}, {0, 3, 5, 6, 7, 10}, {0, 2, 4, 6, 8, 10}, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}};
/* a track's scale and root note: its own, or with the master key on, the key's in the track's octave */
static unsigned keyed(unsigned t) { return ui.tkey[t] ? ui.tkey[t] : ui.key_root; }   /* the key in force: the track's, else the master's; 0 none */
static unsigned eff_scale(unsigned t) { return (ui.tkey[t] ? ui.scale[t] : ui.key_root ? ui.key_scale : ui.scale[t]) % SCALES; }
static int eff_root(unsigned t) { unsigned k = keyed(t); return k ? ui.root[t] - ui.root[t] % 12 + (int)k - 1 : ui.root[t]; }
static void lanes_layout(unsigned t)
{
    unsigned sc = eff_scale(t), n = SCALE_N[sc];
    for (unsigned l = 0; l < CH_LANES; ++l) {
        int note = eff_root(t) + 12 * (int)(l / n) + SCALE_IV[sc][l % n];
        ch_lane_note_set(t, l, (unsigned)(note < 0 ? 0 : note > 127 ? 127 : note));
    }
}
static int in_scale(unsigned t, int note)
{
    unsigned sc = eff_scale(t);
    int d = ((note - eff_root(t)) % 12 + 12) % 12;
    for (unsigned i = 0; i < SCALE_N[sc]; ++i) if (SCALE_IV[sc][i] == d) return 1;
    return 0;
}

/* the master key: on, it lays every track's lanes out in it and tells the engine (RESONATOR, MOSAIC) */
static void key_apply(void)
{
    unsigned mask = 0;
    if (ui.key_root) for (unsigned i = 0; i < SCALE_N[ui.key_scale % SCALES]; ++i) mask |= 1u << SCALE_IV[ui.key_scale % SCALES][i];
    ch_key_set(ui.key_root ? ui.key_root - 1u : 0u, mask);
    for (unsigned t = 0; t < CH_TRACKS; ++t) lanes_layout(t);
}
static const char *const KEY_NAME[13] = {"OFF", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

static int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static int live_step(unsigned t);                      /* (below: the step a live tap belongs to) */
/* the track hit lately (its black key, its name): the engine's count changed within the last 80 ms */
static int hit_on(unsigned t) { return (int32_t)(ui.hit_until[t] - plat_ms()) > 0; }
static void song_sync_lane(void);                      /* (below: the TRACKS view's cell) */
static void smp_bind_all(void);                        /* (samples.h: the bank in the flash) */
static void tempo_turn(int32_t det);                   /* (below: TEMPO on SEQ + ALGO) */
/* AUTOPILOT (GLO's page): while the sequence plays, every so many bars the sounds are varied at random, and now and
 * then a track gets a new line. VARIETY is how many parameters move, how far, and how often a line is redrawn.
 * REVERT (as it starts): each variation is made from the sounds and the pattern as they were when it was turned on,
 * and turning it off puts them back; OVERRIDE: each builds on the last, and off keeps what plays */
enum { AP_MAX = 96 };
static const uint8_t AP_BARS[5] = {1, 2, 4, 8, 16};
static struct {
    uint8_t on, bars_i, revert, phase, n, pat, pat_changed;
    int32_t last_bar, count;
    float variety;
    struct { uint8_t t, slot, p; float orig; } ch[AP_MAX];   /* what it has moved, and where each was */
} ap = {0, 2, 1, 0, 0, 0, 0, -1, 0, 0.3f, {{0, 0, 0, 0}}};
static void ap_set(int on);
static void ap_leave_pattern(void);                    /* (a pattern switch, a save: the pattern as it was, first) */
static void ap_before_save(void);
static void pj_load_begin(void);                       /* (project.h: the saved project) */
static int pj_load_pump(void);
static const char *material_name(unsigned t);          /* (below: a track's sound, for the screen) */
static ch_track_state temp_snap[CH_TRACKS];

/* ------------------------------------------------------------------ audio --- */
static float gain_now;
/* MIDI SYNC, SEND: the clock out, 24 a beat, from the sequencer's own beat while it plays (start: FA, then a clock
 * each 1/24 beat; stop: FC) and at the tempo while it is stopped, so a follower has the tempo before the start.
 * From the render: a block's worth of jitter at most, which a follower's lock smooths. */
enum { SYNC_OFF, SYNC_SEND, SYNC_FOLLOW };
#define SYNC_PKT(st) (0x0Fu | (uint32_t)(st) << 8)
static void sync_send(uint32_t n)
{
    static uint8_t was; static int32_t last; static float acc;
    if (ui.sync != SYNC_SEND) { was = 0; return; }
    const ch_snapshot *sn = ch_snapshot_get();
    if (sn->playing) {
        int32_t k = (int32_t)(sn->beat * 24.0f);
        if (!was) { plat_midi_send(SYNC_PKT(0xFA), TO_USB | TO_BLE); last = k; acc = 0; }
        if (k < last) last = k;
        for (int g = 0; last < k && g < 6; ++g, ++last) plat_midi_send(SYNC_PKT(0xF8), TO_USB | TO_BLE);
        if (last < k) last = k;
    } else {
        if (was) plat_midi_send(SYNC_PKT(0xFC), TO_USB | TO_BLE);
        acc += (float)n * ch_tempo() * (24.0f / (60.0f * (float)CH_RATE));
        if (acc >= 1) { acc -= 1; if (acc > 1) acc = 0; plat_midi_send(SYNC_PKT(0xF8), TO_USB | TO_BLE); }
    }
    was = sn->playing;
}
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    ch_render_far(lr, n, 0);
    sync_send(n);
    logo_sound(lr, n);                                   /* the boot's sound logo, while it runs */
    float gain = (float)master_q12 / 4096.0f;
    for (uint32_t i = 0; i < 2 * n; i += 2) {
        gain_now += (gain - gain_now) * 0.004f;
        lr[i] = (int32_t)((float)lr[i] * gain_now);
        lr[i + 1] = (int32_t)((float)lr[i + 1] * gain_now);
    }
}
void app_panic(void)
{
    ch_transport(0);
    for (unsigned t = 0; t < CH_TRACKS; ++t) for (unsigned k = 0; k < 128; ++k) ch_note(t, k, 0);
}

/* a pattern from text on one lane: . off, x hit, X accent, r ratchet, ? chance, c every other bar */
static void pattern(unsigned t, unsigned lane, const char *p)
{
    unsigned i = 0;
    for (; p[i] && i < CH_STEPS; ++i) {
        ch_step s = {0, 1, 100, 0, 0, 0};
        if (p[i] != '.') s.vel = p[i] == 'X' ? 120 : 90;
        if (p[i] == 'r') s.ratchet = 2;
        if (p[i] == '?') s.chance = 50;
        if (p[i] == 'c') { s.cyc_a = 2; s.cyc_b = 2; }
        ch_step_set(t, lane, i, &s);
    }
    ch_seq_len_set(t, i);
}
/* a synth model's starting values (its knobs, DECAY, LPG, the filter, MODE, ATTACK, GLIDE): set when the model is chosen */
static void synth_presets(unsigned t, unsigned model)
{
    for (unsigned p = 2; p < ch_device_info(CH_DEV_SYNTH)->params; ++p) { float v; if (ch_synth_preset(model, p, &v)) ch_param_set(t, CH_MATERIAL, p, v); }
}
static void material(unsigned t, unsigned dev, unsigned model)
{
    ch_device_set(t, CH_MATERIAL, dev);
    ch_bypass_set(t, CH_MATERIAL, 0);
    ch_param_set(t, CH_MATERIAL, 0, (float)model);
    if (dev == CH_DEV_SYNTH) synth_presets(t, model);
}

/* a SYNTH model's number by its name (the list's order may change) */
static unsigned synth_model(const char *name)
{
    for (unsigned m = 0; ch_synth_names[m]; ++m) {
        const char *a = ch_synth_names[m], *b = name;
        while (*a && *a == *b) { ++a; ++b; }
        if (!*a && !*b) return m;
    }
    return 0;
}
/* a track's starting sound (its pattern starts empty): T1 a KIT (the eight parts of its factory kit, lane n part n),
 * T2 an ANALOG synth, T3 RIPPLE, T4 ACID, each as its model starts; no effects (their line memory stays free); lanes C
 * major from C4. (Each track's DRUM is set up too, should DRUM be chosen: SNARE 909, HAT 808, PING) */
static void track_setup(unsigned t)
{
    static const uint8_t KIT[CH_TRACKS] = {0, 11, 22, 41};
    static const char *const SYN[CH_TRACKS] = {0, "ANALOG", "RIPPLE", "ACID"};
    material(t, CH_DEV_DRUM, KIT[t]);
    if (t == 0) { ch_device_set(0, CH_MATERIAL, CH_DEV_KIT); ch_bypass_set(0, CH_MATERIAL, 0); }
    else material(t, CH_DEV_SYNTH, synth_model(SYN[t]));
    ui.scale[t] = 0; ui.root[t] = 60; ui.tkey[t] = 0; lanes_layout(t);
}
/* INIT: a track, or everything, as it starts */
static void init_track(unsigned t)
{
    ch_track_init(t);
    track_setup(t);
    if (t == ui.track) { ui.sel_step = 0; ui.held_step = -1; ui.sel_lane = 0; ui.step_page = 0; }
}
static void factory_seed(void);                          /* (after project.h: the factory project into the store) */
static void init_all(void)
{
    ch_transport(0);
    for (unsigned t = 0; t < CH_TRACKS; ++t) init_track(t);
    for (unsigned sl = CH_FX1; sl <= CH_FX4; ++sl) ch_device_set(CH_MASTER, sl, CH_DEV_BYPASS);   /* the master chain empty too */
    ch_tempo_set(120); ch_swing_set(0);                  /* a clean start: every track its plain sound, no steps, no effects (the factory project is the first power-on's alone) */
    ui.live_rec = 0; ui.kb_rec = 0;
    ui.key_root = 0; key_apply();
    ui.mac_arm = 0; ui.mac_play = 0;
    for (unsigned m = 0; m < CH_MACROS; ++m) ch_macro_set(m, 0);   /* (init_track: the maps gone) */
}

/* the speed test: what this chip takes for each kind of work, timed once at power-on (LOAD page, K4: its table).
 * Each is a loop of SPD_N steps of one thing; the table shows nanoseconds a step with the empty loop's taken off, so
 * the engine's code can be weighed by what it is made of (a taken branch, a memory read, a divide) and not by a PC's
 * times, which are another shape altogether. The volatile seeds keep the compiler from folding the loops away */
enum { SPD_N = 4096, SPD_TESTS = 12 };
static uint16_t spd_ns[SPD_TESTS];
static const char *const SPD_NAME[SPD_TESTS] = {"LOOP", "FMUL+ADD", "FDIV", "LOAD+STORE", "BRANCH", "TANH", "EXP2", "SIN", "F>INT>F", "CLAMP", "IMUL+ADD", "CALL"};
static uint8_t cpu2_ok, one_core;    /* the second core started; held off from the LOAD page (K1: CORES 1), to tell what it does */
static volatile float spd_f[4] = {0.37f, 1.0009f, 0.0003f, 3.0f};
static volatile uint32_t spd_u[2] = {0x1234567u, 7u};
static float spd_mem[64];
static CH_NOINLINE float spd_leaf(float x) { return x * 0.999f + spd_f[2]; }
static void speed_test(void)
{
    uint32_t base = 0;
    for (unsigned k = 0; k < SPD_TESTS; ++k) {
        float x = spd_f[0], a = spd_f[1], b = spd_f[2], c = spd_f[3], acc = 0;
        uint32_t u = spd_u[0], w = spd_u[1];
        uint32_t t0 = plat_ticks24();
        switch (k) {
        case 0: for (unsigned i = 0; i < SPD_N; ++i) { u += w; } break;
        case 1: for (unsigned i = 0; i < SPD_N; ++i) { x = x * a + b; } break;
        case 2: for (unsigned i = 0; i < SPD_N; ++i) { x = c / (x + a); } break;
        case 3: for (unsigned i = 0; i < SPD_N; ++i) { float v = spd_mem[i & 63u]; spd_mem[(i + 17u) & 63u] = v + b; } break;
        case 4: for (unsigned i = 0; i < SPD_N; ++i) { u ^= u << 13; u ^= u >> 17; u ^= u << 5; if ((u >> 16) & 1u) w += 3; else w ^= u; } break;   /* a branch no one can guess (the xorshift's own steps are in LOOP's class: cheap) */
        case 5: for (unsigned i = 0; i < SPD_N; ++i) { x = ch_tanh(x * c) + b; } break;
        case 6: for (unsigned i = 0; i < SPD_N; ++i) { x = ch_exp2(x - 1.5f) * 0.9f; } break;
        case 7: for (unsigned i = 0; i < SPD_N; ++i) { x = ch_sin_turns(x + 0.31f); } break;
        case 8: for (unsigned i = 0; i < SPD_N; ++i) { x = (float)(int)(x * 100.0f) * 0.011f + b; } break;
        case 9: for (unsigned i = 0; i < SPD_N; ++i) { x = ch_clamp(x * 1.7f - 0.3f, -0.9f, 0.9f); x = ch_max(x, -0.5f); } break;
        case 10: for (unsigned i = 0; i < SPD_N; ++i) { u = u * 1664525u + w; } break;
        default: for (unsigned i = 0; i < SPD_N; ++i) { x = spd_leaf(x); } break;
        }
        uint32_t ticks = (plat_ticks24() - t0) & 0xFFFFFFu;   /* (a 24-bit count) */
        acc = x + (float)(u & 0xFFu) + (float)(w & 0xFFu);
        spd_f[0] = acc * 1e-9f + 0.37f; spd_u[1] = (w & 1u) | 6u;      /* (used, so each loop stays) */
        uint32_t ns = ticks * 16000u / (PLAT_TICKS_PER_US * SPD_N);   /* 1/16 ns a step (32 bits: a test is a millisecond or two, far under where this wraps) */
        if (k == 0) { base = ns; spd_ns[0] = (uint16_t)(ns / 16u); }
        else spd_ns[k] = (uint16_t)((ns > base ? ns - base : 0) / 16u);
    }
}

/* what one voice of each model costs this chip: at power-on, before the audio runs, each drum model and each of our
 * synth models is set on T1 alone, one note struck (a synth's held), and a 64-frame render timed once the note is
 * going, less the same render with no note. Microseconds for a 256-frame block (LOAD page, K4: the pages after the
 * speed test). About half a second in all; the watchdog allows 8 */
enum { COST_SYNTHS = 15, COST_FX0 = CH_DRUM_MODELS + COST_SYNTHS, COST_N = COST_FX0 + CH_FX_TYPES };   /* then every effect: what it adds to a held ANALOG note in the slot after it */
static uint16_t cost_us[COST_N];
static uint32_t cost_render(void)
{
    int32_t buf[128];
    uint32_t t0 = plat_ticks24();
    ch_render_far(buf, 64, 0);
    return (plat_ticks24() - t0) & 0xFFFFFFu;
}
static void model_cost(void)
{
    int32_t buf[128];
    for (unsigned k = 0; k < COST_N; ++k) {
        ch_init();
        for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        if (k >= COST_FX0) {                                /* an effect: as it starts, on a held ANALOG note (one sound both sides, as most tracks are) */
            ch_device_set(0, CH_MATERIAL, CH_DEV_SYNTH); ch_bypass_set(0, CH_MATERIAL, 0);
            ch_param_set(0, CH_MATERIAL, 0, 0);
            for (unsigned q = 2; q < ch_device_info(CH_DEV_SYNTH)->params; ++q) { float v; if (ch_synth_preset(0, q, &v)) ch_param_set(0, CH_MATERIAL, q, v); }
            ch_render_far(buf, 64, 0);                       /* (the settings taken in: the queue holds only so many between renders) */
            ch_device_set(0, CH_FX1, CH_DEV_FX_FIRST + (k - COST_FX0)); ch_bypass_set(0, CH_FX1, 0);
            ch_note(0, 48, 110);
            for (unsigned i = 0; i < 12; ++i) ch_render_far(buf, 64, 0);   /* (its line memory taken and cleared: a block a call, eight at the most) */
            uint32_t on = cost_render(); for (unsigned i = 0; i < 2; ++i) { uint32_t b = cost_render(); if (b < on) on = b; }
            ch_bypass_set(0, CH_FX1, 1);
            for (unsigned i = 0; i < 12; ++i) ch_render_far(buf, 64, 0);   /* (a bypass fades over some blocks: timed once it is done) */
            uint32_t idle = cost_render(); { uint32_t b = cost_render(); if (b < idle) idle = b; }
            uint32_t us = (on > idle ? on - idle : 0) * 4u / PLAT_TICKS_PER_US;
            cost_us[k] = (uint16_t)(us > 65535u ? 65535u : us);
            continue;
        }
        if (k < CH_DRUM_MODELS) {
            ch_device_set(0, CH_MATERIAL, CH_DEV_DRUM); ch_bypass_set(0, CH_MATERIAL, 0);
            ch_param_set(0, CH_MATERIAL, 0, (float)k);
        } else {
            unsigned m = k - CH_DRUM_MODELS;
            ch_device_set(0, CH_MATERIAL, CH_DEV_SYNTH); ch_bypass_set(0, CH_MATERIAL, 0);
            ch_param_set(0, CH_MATERIAL, 0, (float)m);
            for (unsigned q = 2; q < ch_device_info(CH_DEV_SYNTH)->params; ++q) { float v; if (ch_synth_preset(m, q, &v)) ch_param_set(0, CH_MATERIAL, q, v); }
            ch_param_set(0, CH_MATERIAL, 7, 1);           /* MODE GATE: the note held, the voice at its full cost */
        }
        for (unsigned i = 0; i < 4; ++i) ch_render_far(buf, 64, 0);
        uint32_t idle = cost_render(); { uint32_t b = cost_render(); if (b < idle) idle = b; }
        ch_note(0, 60, 110);
        ch_render_far(buf, 64, 0);                           /* (the note's first block: its start is not what is asked) */
        uint32_t on = cost_render(); { uint32_t b = cost_render(); if (b < on) on = b; }
        uint32_t us = (on > idle ? on - idle : 0) * 4u / PLAT_TICKS_PER_US;
        cost_us[k] = (uint16_t)(us > 65535u ? 65535u : us);
    }
    ch_init();
}

/* this build, as a byte (its date and time folded): kept with the settings, so the first power-on after any install
 * shows the HELP card again, once */
static uint8_t build_id(void) { const char *c = __DATE__ __TIME__; unsigned h = 0; while (*c) h = h * 31u + (unsigned char)*c++; return (uint8_t)h; }
void app_init(void)
{
    ch_init();
    model_cost();                                        /* (before the profile clock and the second core: a plain render) */
    ch_profile_clock(plat_ticks24, PLAT_TICKS_PER_US);
    { uint8_t b[5]; int sn = plat_store_load(OBJ_SETTINGS, b, 5); if (sn >= 4 && b[0] == 'S' && b[1] == 'Y' && b[2] <= 2) { ui.sync = b[2]; ui.help_told = b[3] & 1u; ui.rec_micro = b[3] >> 1 & 1u; ui.count_off = (uint8_t)((b[3] >> 2 & 3u) % 3u); ui.snd_off = (uint8_t)((b[3] >> 6 & 3u) << 1); ch_limiter_set(!(b[3] >> 5 & 1u)); if (sn < 5 || b[4] != build_id()) ui.help_told = 0; } }   /* (a new build since: the card once more) */   /* MIDI SYNC, as it was left */
#if defined(FM1B_CPU2) && FM1B_CPU2
    if (plat_cpu2_set(1) == 0) { ch_worker(plat_cpu2_run, plat_cpu2_wait); cpu2_ok = 1; }   /* the second core renders some tracks */
#endif
    speed_test();
    for (unsigned t = 0; t < CH_TRACKS; ++t) track_setup(t);    /* the factory sounds */
    ui.view = V_SONG; ui.slot = CH_MATERIAL;       /* the TRACKS view first */
    ui.sel_step = 0; ui.held_step = -1; ui.lane_held = -1; ui.stash_no = -1;
    ui.spec_tilt = 1;                                    /* TILT 3 dB: music falls about that much an octave, so the picture sits level */
    ui.spec_tone = 12;                                   /* the tone MONO starts on: 180 degrees, cyan (the project may bring another) */
    smp_bind_all();                                /* the sample bank in the flash */
    if (plat_store_size(OBJ_PROJECT) < 8) factory_seed();              /* nothing saved yet: the factory project */
    pj_load_begin(); pj_load_pump();               /* the saved project, the rest of it as the frames go by */
}

/* ------------------------------------------------------------------- text --- */
static void text_cat(char *d, const char *s) { while (*d) d++; while (*s) *d++ = *s++; *d = 0; }
static void int_text(int32_t v, char *b)
{
    char t[12]; int n = 0; uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do t[n++] = (char)('0' + u % 10u); while ((u /= 10u) && n < 11);
    if (v < 0) *b++ = '-';
    while (n) *b++ = t[--n];
    *b = 0;
}
/* a ratchet's name: 1X..4X, then the rolls X IN, X OUT (speeding up, slowing down), F IN, F OUT (the velocity fading too) */
static void ratchet_text(unsigned r, char *v)
{
    static const char *const ROLL[4] = {"X IN", "X OUT", "F IN", "F OUT"};   /* (XF OUT would not fit the tile) */
    v[0] = 0;
    if (r >= CH_RATCHET_X_IN && r <= CH_RATCHET_LAST) text_cat(v, ROLL[r - CH_RATCHET_X_IN]); else { int_text((int)r, v); text_cat(v, "X"); }
}
/* The ratchet on a step's bar: black lines across it, cutting it into as many parts as it has hits: halves for 2X,
 * thirds for 3X, quarters for 4X. A roll's four parts are uneven, the first hit at the foot: IN (the hits closing up)
 * long to short going up, OUT short to long; the fading rolls' parts dim toward their quiet end.
 * x, w: the bar; yb: the row under it; h: its height (ratchet_min at the least); col: its colour; bg: the cell's */
static uint16_t mix565(uint16_t a, uint16_t b, float t);
/* a tied step: the note before it carried on, drawn as a chequer of the track's colour over the cell, from its foot up:
 * three pixels a square, four across the cell (two a square ran together into a plain dim bar on the FM-1's screen) */
static void tie_fill(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t col)
{
    for (int32_t r = 0, py = h; py > 0; ++r, py -= 3) {
        int32_t sh = py < 3 ? py : 3;
        for (int32_t px = (r & 1) ? 3 : 0; px < w; px += 6) cv_rect(x + px, y + py - sh, px + 3 > w ? w - px : 3, sh, col);
    }
}
static int32_t ratchet_min(unsigned r) { return r < 2 ? 0 : (int32_t)(r >= CH_RATCHET_X_IN ? 4u : r) * 2 - 1; }   /* (a part a pixel, a line between) */
static void ratchet_cuts(int32_t x, int32_t yb, int32_t w, int32_t h, unsigned r, uint16_t col, uint16_t bg)
{
    static const int8_t EVEN[4] = {1, 1, 1, 1}, ROLL_IN[4] = {4, 2, 2, 1}, ROLL_OUT[4] = {1, 2, 2, 4};
    if (r < 2) return;
    int roll = r >= CH_RATCHET_X_IN, in = r == CH_RATCHET_X_IN || r == CH_RATCHET_XF_IN, fade = r >= CH_RATCHET_XF_IN;
    const int8_t *wt = roll ? (in ? ROLL_IN : ROLL_OUT) : EVEN;
    int32_t n = roll ? 4 : (int32_t)r, sum = 0, avail = h - (n - 1), sz[4], used = 0;
    if (avail < n) return;
    for (int32_t k = 0; k < n; ++k) sum += wt[k];
    for (int32_t k = 0; k < n; ++k) { sz[k] = avail * wt[k] / sum; if (sz[k] < 1) sz[k] = 1; used += sz[k]; }
    for (int32_t k = 0; used != avail; k = (k + 1) % n) {   /* the rounding: a pixel at a time, to the heavy parts first */
        int32_t i = !roll ? k : in ? k : n - 1 - k;
        if (used < avail) { sz[i]++; used++; } else if (sz[i] > 1) { sz[i]--; used--; }
    }
    int32_t y = yb;
    for (int32_t k = 0; k < n; ++k) {
        float quiet = !fade ? 0 : in ? (float)(3 - k) * 0.25f : (float)k * 0.25f;   /* (F IN: louder as it goes; F OUT: quieter) */
        if (quiet > 0) cv_rect(x, y - sz[k], w, sz[k], mix565(col, bg, quiet * 0.8f));
        y -= sz[k];
        if (k < n - 1) { y -= 1; cv_rect(x, y, w, 1, C_BLACK); }
    }
}
static void note_text(unsigned n, char *b) { b[0] = 0; text_cat(b, NOTE_NAME[n % 12]); char o[8]; int_text((int32_t)n / 12 - 1, o); text_cat(b, o); }
void app_say(const char *a, const char *b)
{
    uint32_t i;
    for (i = 0; i < 23u && a && a[i]; i++) ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++) ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1200u;
    ui.redraw = 1;
    if (ann_on()) {
        char t[48]; t[0] = 0; text_cat(t, ui.msg[0]); text_cat(t, " "); text_cat(t, ui.msg[1]);
        ann_say(t, 0, ANN_NOW);
    }
}

/* the parameter under page tile i of the current view, -1 none */
#include "project.h"
static void header(void);
static void edit_scope(unsigned i, int32_t det);
static void sp_reset(void);
static const char *const SP_PAL[2];
#include "samples.h"
/* the factory project (factory.h, made by tools/make_factory.py from a project saved on an FM-1): its objects unpacked
 * into the store, as if it had been saved there; the load that follows brings it in. At the first power-on ever (nothing
 * saved); INIT does not bring it back. The packing is described in tools/make_factory.py */
#include "factory.h"
static void factory_seed(void)
{
    const unsigned char *p = FACTORY;
    while (*p != 0xFF) {
        const unsigned obj = p[0], n = (unsigned)p[1] | (unsigned)p[2] << 8;
        const unsigned char *e = p + 3 + n; unsigned o = 0;
        p += 3; memset(&pj_buf, 0, sizeof pj_buf);
        while (p < e) {
            unsigned t = *p++;
            if (t < 0x80) { memcpy(pj_buf.raw + o, p, t + 1); p += t + 1; o += t + 1; }
            else { unsigned d = *p++ + 1u; for (unsigned m = (t & 0x7Fu) + 3; m; --m, ++o) pj_buf.raw[o] = pj_buf.raw[o - d]; }
        }
        plat_store_save(obj, &pj_buf, ((const pj_hdr *)pj_buf.raw)->size);
        pj_crc[obj] = 0;
    }
}
static const ch_device_desc *cur_dev(void) { return ui.slot < CH_SLOTS ? ch_device_info(ch_track_get(ET)->device[ui.slot]) : 0; }
/* an FX slot's effect as the screen names it */
static const char *fx_name(const ch_track_state *ts, unsigned slot)
{
    return ts->device[slot] == CH_DEV_BYPASS ? "EMPTY" : ch_device_info(ts->device[slot])->name;
}
/* the effects in the order the lists give them, by kind: the filters; the sweeps; the delays; the reverbs; the textures;
 * the two that work in time; the sidechain. (Their numbers, which the saves hold, stay as they were) */
static const uint8_t FX_ORDER[] = {
    CH_DEV_FILTER, CH_DEV_DJ, CH_DEV_DISPERSE, CH_DEV_COMB,
    CH_DEV_PHASER, CH_DEV_FLANGER, CH_DEV_TREMOLO, CH_DEV_CHORUS, CH_DEV_UNISON,
    CH_DEV_DELAY, CH_DEV_DUB, CH_DEV_ECHO, CH_DEV_CRYSTAL,
    CH_DEV_REVERB, CH_DEV_SPRING,
    CH_DEV_ERODE, CH_DEV_VINYL, CH_DEV_TAPE_FX, CH_DEV_RESO, CH_DEV_DRIVE,
    CH_DEV_TRANCE, CH_DEV_REPEAT,
    CH_DEV_SIDE};
typedef char fx_order_complete[sizeof FX_ORDER == CH_FX_TYPES ? 1 : -1];
/* the FX view on slot k (0..3) */
static void fx_open(unsigned k)
{
    const ch_track_state *ts = ch_track_get(ET);
    ui.fxpos = (uint8_t)(k % CH_FX); ui.view = V_DEVICE;
    ui.slot = (uint8_t)(CH_FX1 + ui.fxpos);
    app_say(ui.mfx ? MSLOT_NAME[ui.slot] : SLOT_NAME[ui.slot], fx_name(ts, ui.slot));
    ui.redraw = 1;
}
/* ALGO on an FX slot: its effect, the next or the last of the list (FX_ORDER; empty between its ends). Only this
 * slot changes: it starts at the effect's own settings */
static void fx_choose(int32_t det)
{
    const ch_track_state *ts = ch_track_get(ET);
    unsigned cur = ts->device[ui.slot], n = CH_FX_TYPES + 1;           /* 0 empty, then the effects */
    unsigned at = 0, dev;
    for (unsigned k = 0; k < CH_FX_TYPES; ++k) if (FX_ORDER[k] == cur) at = k + 1;
    at = (at + (det > 0 ? 1 : n - 1)) % n;
    dev = at ? FX_ORDER[at - 1] : CH_DEV_BYPASS;
    ch_device_set(ET, ui.slot, dev);
    ch_bypass_set(ET, ui.slot, 0);
    ui.page[ui.slot] = 0;
    app_say(ui.mfx ? MSLOT_NAME[ui.slot] : SLOT_NAME[ui.slot], fx_name(ts, ui.slot));
    ui.redraw = 1;
}
static int env_applies(void);
static int panel_on(void);
/* A grey with no colour in it on this screen: the same step of red, green and blue (0..31). The dark greys written as
 * RGB(13, 14, 16) and the like round to more blue than red, and on the device they are blue */
#define GREY5(n) ((uint16_t)(((n) << 11) | ((n) << 6) | (n)))
static uint8_t ui_knobs, fxviz_off;   /* (the knob look and the effect drawings: set further down) */
static int mac_shown(void);
/* the MIXER footer is on the screen: ALGO is then the chosen strip's level, and PRESET goes round to the RETURN too */
static int mixer_on(void);
static int mixpage_on(void);
static void body_mixpage(void);
/* the track that sounds alone (every other muted), or -1 */
static int solo_track(void)
{
    int one = -1;
    for (unsigned t = 0; t < CH_TRACKS; ++t) if (!ch_track_get(t)->mute) { if (one >= 0) return -1; one = (int)t; }
    return one;
}
static const char *const MIXMOD_NAME[3] = {"LEVEL", "PAN", "FILTER"};
/* the envelope page: while the display is shown, the knobs are the envelope's own parameters */
static const int8_t ENV_P_SYNTH[4] = {8, 5, 7, 6}, ENV_P_POLY[4] = {7, 8, 9, 10}, ENV_P_GRAIN[4] = {7, -1, -1, -1};
static const int8_t ENV_P_FILTER[4] = {15, 16, 14, 12};   /* the filter envelope's page: F ATTACK F DECAY ENV>FILT CUTOFF */
static unsigned material_visible(unsigned t, const ch_device_desc *d, uint8_t *out);
static int tile_param_at(unsigned page, unsigned i);
static int tile_param(unsigned i)
{
    const ch_device_desc *d = cur_dev();
    if (!d) return -1;
    if (env_applies()) {
        unsigned dev = ch_track_get(ui.track)->device[CH_MATERIAL];
        if (ui.env_show == 2) return ENV_P_FILTER[i & 3u];
        return (dev == CH_DEV_SYNTH ? ENV_P_SYNTH : dev == CH_DEV_POLY ? ENV_P_POLY : ENV_P_GRAIN)[i & 3u];
    }
    return tile_param_at(ui.page[ui.slot], i);
}
static int tile_param_at(unsigned page, unsigned i)   /* the parameter at place i of a page, -1 none */
{
    const ch_device_desc *d = cur_dev();
    if (!d) return -1;
    int p = (int)(page * PER_PAGE + i);
    if (ui.slot == CH_MATERIAL && !ui.mfx) { uint8_t vis[32]; unsigned n = material_visible(ui.track, d, vis); return p < (int)n ? vis[p] : -1; }
    if (p >= (int)d->params) return -1;
    return d->order ? (int)d->order[p] : p;             /* the device's page order */
}
static unsigned pages_of(unsigned n) { return n ? (n + PER_PAGE - 1) / PER_PAGE : 1; }
/* the material slot's visible parameters, in page order: a SYNTH model hides what it has no use for (the oscillator tuning off ANALOG) */
static unsigned material_visible(unsigned t, const ch_device_desc *d, uint8_t *out)
{
    unsigned dev = ch_track_get(t)->device[CH_MATERIAL], n = 0;
    unsigned model = dev == CH_DEV_SYNTH ? (unsigned)(ch_track_get(t)->param[ch_row(CH_MATERIAL, dev)][0] + 0.5f) : 0;
    const uint8_t *ord = dev == CH_DEV_SYNTH && ch_synth_order(model) ? ch_synth_order(model) : d->order;   /* (a model with pages of its own: WOBBLE) */
    for (unsigned k = 0; k < d->params; ++k) {
        unsigned p = ord ? ord[k] : k;
        if (dev == CH_DEV_SYNTH && !ch_synth_param_shown(model, p)) continue;
        out[n++] = (uint8_t)p;
    }
    return n;
}
static unsigned pages_now(void)
{
    const ch_device_desc *d = cur_dev();
    if (!d) return 1;
    if (ui.slot == CH_MATERIAL && !ui.mfx) { uint8_t vis[32]; return pages_of(material_visible(ui.track, d, vis)); }
    return pages_of(d->params);
}
static unsigned model_of(unsigned t, unsigned dev) { return (unsigned)(ch_track_get(t)->param[ch_row(CH_MATERIAL, dev)][0] + 0.5f); }
/* a parameter's name as shown: DRUM's and SYNTH's timbre controls take the model's names */
static const char *param_name(unsigned t, unsigned slot, unsigned p)
{
    unsigned dev = ch_track_get(t)->device[slot];
    const char *l = 0;
    if (dev == CH_DEV_DRUM) l = ch_drum_label(model_of(t, dev), p);
    else if (dev == CH_DEV_SYNTH) l = ch_synth_label(model_of(t, dev), p);
    else if (dev == CH_DEV_DRIVE) l = ch_fx_label(dev, (unsigned)(ch_track_get(t)->param[ch_row(slot, dev)][4] + 0.5f), p);   /* CHARACTER as the TYPE names it */
    return l ? l : ch_device_info(dev)->param[p].name;
}

/* DRIVE in CRUSH: its RATE as the share of the sample rate it is held at, its BITS as bits (the others as the effect says) */
static void crush_text(unsigned t, unsigned slot, unsigned dev, int p, float v, char *val)
{
    if (dev != CH_DEV_DRIVE || (unsigned)(ch_track_get(t)->param[ch_row(slot, dev)][4] + 0.5f) != 5) return;
    char n[8];
    if (p == 0) { val[0] = 0; text_cat(val, "1/"); int_text((int32_t)(1.0f * fm_powf(32.0f, fm_clampf(v, 0, 1)) + 0.5f), n); text_cat(val, n); }
    else if (p == 5) { val[0] = 0; int_text((int32_t)(16.0f - 13.0f * fm_clampf(v, 0, 1) + 0.5f), n); text_cat(val, n); text_cat(val, " BIT"); }
}
static void value_text(const ch_param_desc *pd, float v, unsigned mode, char *out)
{
    out[0] = 0;
    if (pd->kind == CH_SELECT) { text_cat(out, pd->choices ? pd->choices[(int)(v + 0.5f)] : "?"); return; }
    int32_t pct = (int32_t)(v * 100 + (v < 0 ? -0.5f : 0.5f));
    if (pd->kind == CH_BI && pct > 0) text_cat(out, "+");
    char n[12]; int_text(pct, n); text_cat(out, n);
    (void)mode;
}

/* ---------------------------------------------------------------- editing --- */
/* PITCH, an engine's parameter 1: -1..1 is +-24 semitones (SAMPLE +-48). Shown in semitones; CHROMATIC a semitone a detent */
static int pitch_range(unsigned slot, unsigned dev, int p)
{
    if (slot != CH_MATERIAL || ui.mfx) return 0;
    if (dev == CH_DEV_KIT) return p >= 0 && p < (int)CH_PARAMS && !(p & 1) ? 24 : 0;   /* KIT: each part's PITCH (the even ones of its 16) */
    if (p != 1) return 0;
    return dev == CH_DEV_POLY ? 48 : dev == CH_DEV_SYNTH || dev == CH_DEV_DRUM || dev == CH_DEV_GRAIN ? 24 : 0;
}
static void pitch_text(float v, int range, unsigned mode, char *out)
{
    float s = v * (float)range; char n[12];
    out[0] = 0;
    if (mode == 0) {
        int d = (int)(s + (s < 0 ? -0.5f : 0.5f));
        if (d > 0) text_cat(out, "+");
        int_text(d, n); text_cat(out, n);
    } else {
        int t = (int)(s * 10 + (s < 0 ? -0.5f : 0.5f)), a = t < 0 ? -t : t;   /* tenths of a semitone */
        text_cat(out, t > 0 ? "+" : t < 0 ? "-" : "");
        int_text(a / 10, n); text_cat(out, n); text_cat(out, "."); int_text(a % 10, n); text_cat(out, n);
    }
    text_cat(out, " ST");
}
/* What a click of a knob is worth, from how fast that knob is being turned (every knob alike, wherever its value goes):
 * slowly, a hundredth of the range, so a value is set to the unit; faster, more, up to eight hundredths, so a brisk half
 * turn crosses the range. The speed is clicks a second, smoothed over the last few, forgotten after a fifth of a second
 * at rest (the first click after a pause is always the fine one) */
static float enc_s = 0.01f;
static void enc_speed(unsigned k, int32_t e)
{
    static uint32_t last[7]; static float rate[7];
    uint32_t now = plat_ms(), dt = now - last[k];
    last[k] = now;
    if (dt > 200u) rate[k] = 0;
    else rate[k] = 0.5f * rate[k] + 0.5f * (float)(e < 0 ? -e : e) * 1000.0f / (float)(dt < 4u ? 4u : dt);
    float g = rate[k] * (1.0f / 12.0f);                  /* (to 12 clicks a second: fine) */
    enc_s = g <= 1 ? 0.01f : fm_clampf(0.01f * fm_powf(g, 1.4f), 0.01f, 0.08f);
}
static float pitch_step(float v, int range, unsigned mode, int32_t det)   /* the next value a detent away */
{
    if (mode != 0) return v + (float)det * enc_s * 0.25f;   /* FREE: a quarter of a percent, a tenth of a semitone or so */
    int d = (int)(v * (float)range + (v < 0 ? -0.5f : 0.5f)) + (det > 0 ? 1 : -1);
    d = d < -range ? -range : d > range ? range : d;
    return (float)d / (float)range;
}
static float step_of(const ch_param_desc *pd, int32_t det)
{
    float s = enc_s;                                    /* (by the knob's speed: enc_speed) */
    if (pd->kind == CH_SELECT) s = 1;
    return (float)det * s;
}
/* A knob's click on an effect's value that is shown in steps (a synced TIME, REPEATS, a note, a rate): the usual small
 * step, and if what is shown has not changed, on until it has. So no value ever takes more than a click to move */
static float click_to(const ch_param_desc *pd, unsigned dev, unsigned p, unsigned mode, float cur, int32_t det)
{
    float lo = pd->kind == CH_BI ? -1.0f : 0.0f, nv = fm_clampf(cur + step_of(pd, det), lo, 1);
    char a[16], b[16];
    if (pd->kind == CH_SELECT || dev < CH_DEV_FX_FIRST || !ch_fx_value_text(dev, p, cur, mode, a)) return cur + step_of(pd, det);
    for (int k = 0; k < 150; ++k) {
        if (!ch_fx_value_text(dev, p, nv, mode, b)) break;
        const char *x = a, *y = b;
        while (*x && *x == *y) { ++x; ++y; }
        if (*x || *y) break;                             /* it reads differently: there */
        if (nv <= lo || nv >= 1) break;
        nv = fm_clampf(nv + (det > 0 ? 0.01f : -0.01f), lo, 1);
    }
    return nv;
}
static void lock_turn(unsigned i, int32_t det);
static void edit_param(unsigned i, int32_t det)
{
    int p = tile_param(i);
    if (p < 0) return;
    if (ui.lock_rec && !ui.mfx && ui.lock_track == ui.track && !(ui.btn >> B_SEQ & 1u) && !(ui.btn >> B_LFO & 1u)) { lock_turn(i, det); return; }
    const ch_track_state *ts = ch_track_get(ET);
    unsigned dev = ts->device[ui.slot], row = ch_row(ui.slot, dev);
    const ch_param_desc *pd = &ch_device_info(dev)->param[p];
    if (ui.btn >> B_SEQ & 1u) {                         /* SEQ held: the parameter's mode */
        ui.seq_used = 1;
        const char *const *mo = dev == CH_DEV_SYNTH && ui.slot == CH_MATERIAL && ch_synth_modes(model_of(ET, dev), (unsigned)p) ? ch_synth_modes(model_of(ET, dev), (unsigned)p) : pd->modes;   /* (a model's own: WOBBLE's DEPTH) */
        if (!mo || p >= (int)CH_PARAMS) return;
        int nm = 0; while (mo[nm]) ++nm;
        int m = ((int)ts->mode[row][p] + (det > 0 ? 1 : -1) + nm) % nm;
        ch_mode_set(ET, ui.slot, (unsigned)p, (unsigned)m);
        app_say(param_name(ET, ui.slot, (unsigned)p), mo[m]);
        return;
    }
    if (ui.mac_arm && !(ui.btn >> B_LFO & 1u)) {        /* a macro being assigned: the turn sets how far the macro moves this parameter */
        char t[20] = "MACRO "; char n[12]; int_text(ui.mac_arm, n); text_cat(t, n);
        if (ui.mfx) { app_say(t, "NOT ON THE MASTER"); return; }
        if (p >= (int)CH_PARAMS) { app_say(param_name(ET, ui.slot, (unsigned)p), "CANNOT BE MAPPED"); return; }
        unsigned mm = CH_MODS + ui.mac_arm - 1u;
        float depth = 0;
        for (unsigned k = 0; k < CH_MAPS; ++k)
            if (ts->map[k].depth != 0 && ts->map[k].mod == mm && ts->map[k].slot == ui.slot && ts->map[k].param == (unsigned)p) depth = ts->map[k].depth;
        depth = fm_clampf(depth + (float)det * enc_s, -1, 1);
        if (depth > -0.005f && depth < 0.005f) depth = 0;
        if (ch_map_set(ET, mm, ui.slot, (unsigned)p, depth, 0) < 0) { app_say(t, "NO FREE MAPS"); return; }
        text_cat(t, " "); if (depth > 0) text_cat(t, "+"); int_text((int32_t)(depth * 100 + (depth < 0 ? -0.5f : 0.5f)), n); text_cat(t, n);
        app_say(param_name(ET, ui.slot, (unsigned)p), t); ui.redraw = 1;
        return;
    }
    if (ui.btn >> B_LFO & 1u) {                         /* LFO held: map the selected modulator; with SEL too: the TRIG RND one */
        if (ui.mfx) { app_say(param_name(ET, ui.slot, (unsigned)p), "NOT ON THE MASTER"); return; }   /* (the modulators are the tracks': it said MOD 1 and mapped nothing) */
        unsigned mm = ui.mod;
        if (ui.btn >> B_SEL & 1u) {
            ui.sel_used = 1;
            mm = CH_MODS;
            for (unsigned k = 0; k < CH_MODS; ++k) if (ts->mod_type[k] == CH_MOD_TRIG) { mm = k; break; }
            if (mm == CH_MODS) { app_say("TRIG RND", "NO MOD SET TO IT"); return; }
        }
        float depth = 0;
        for (unsigned k = 0; k < CH_MAPS; ++k)
            if (ts->map[k].depth != 0 && ts->map[k].mod == mm && ts->map[k].slot == ui.slot && ts->map[k].param == p)
                depth = ts->map[k].depth;
        depth = fm_clampf(depth + (float)det * enc_s, -1, 1);
        if (depth > -0.005f && depth < 0.005f) depth = 0;
        ch_map_set(ET, mm, ui.slot, (unsigned)p, depth, 1);
        char t[20] = ""; char n[12];
        if (mm != ui.mod || ts->mod_type[mm] == CH_MOD_TRIG) text_cat(t, "TRIG "); else text_cat(t, "MOD ");
        int_text((int32_t)mm + 1, n); text_cat(t, n);
        text_cat(t, " "); int_text((int32_t)(depth * 100), n); text_cat(t, n);
        app_say(param_name(ET, ui.slot, (unsigned)p), t);
        return;
    }
    if (p >= (int)CH_PARAMS && (ui.btn >> B_LFO & 1u)) { app_say(param_name(ET, ui.slot, (unsigned)p), "NOT MODULATED"); return; }
    if (ui.slot == CH_MATERIAL && p == 1 && keyed(ui.track) && ts->mode[row][p] == 0 && (dev == CH_DEV_DRUM || dev == CH_DEV_SYNTH)) {
        /* the key locked, PITCH chromatic: a detent moves to the next tuning that keeps the track's root note in key */
        int base = (int)ch_seq_get(ui.track)->note[0], d = (int)(ts->param[row][p] * 24 + (ts->param[row][p] < 0 ? -0.5f : 0.5f));
        int dir = det > 0 ? 1 : -1, nd = d;
        for (int k = 0; k < 12; ++k) { nd += dir; if (nd < -24 || nd > 24) { nd = d; break; } if (in_scale(ui.track, base + nd)) break; }
        ch_param_set(ET, ui.slot, (unsigned)p, (float)nd / 24.0f);
        char v[12] = ""; if (nd > 0) text_cat(v, "+"); { char n[8]; int_text(nd, n); text_cat(v, n); } text_cat(v, " IN KEY");
        app_say("PITCH", v);
        return;
    }
    float cur = ch_param_of(ts, ui.slot, dev, (unsigned)p);
    {   /* PITCH: semitones */
        int pr = pitch_range(ui.slot, dev, p);
        if (pr) {
            float nv = fm_clampf(pitch_step(cur, pr, ts->mode[row][p], det), -1, 1);
            ch_param_set(ET, ui.slot, (unsigned)p, nv);
            char v[16]; pitch_text(nv, pr, ts->mode[row][p], v); app_say("PITCH", v);
            return;
        }
    }
    if (dev == CH_DEV_SIDE && p == 0) {                  /* SIDECHAIN's SOURCE: the four tracks, then the lanes of the tracks that are KITs (the rest passed over) */
        int was = (int)(cur + 0.5f), k = was, dir = det > 0 ? 1 : -1;
        for (;;) {
            k += dir;
            if (k < 0 || k >= (int)(CH_TRACKS + CH_TRACKS * CH_KIT_PARTS)) { k = was; break; }
            if (k < (int)CH_TRACKS || ch_track_get((unsigned)(k - (int)CH_TRACKS) / CH_KIT_PARTS)->device[CH_MATERIAL] == CH_DEV_KIT) break;
        }
        ch_param_set(ET, ui.slot, 0, (float)k);
        char v[16]; if (!ch_fx_value_text(dev, 0, (float)k, 0, v)) value_text(pd, (float)k, 0, v);
        app_say("SOURCE", v);
        return;
    }
    char sv[16]; int nch = dev == CH_DEV_SYNTH && ui.slot == CH_MATERIAL ? ch_synth_value_text(model_of(ET, dev), (unsigned)p, cur, sv) : 0;
    if (nch > 1) {                                       /* a named value: one choice a detent; a long list (WOBBLE's DEPTH, two hundred values) by the knob's speed, as a plain value */
        int k = (int)(enc_s * (float)(nch - 1) + 0.5f); if (k < 1) k = 1;
        int i = (int)(fm_clampf(cur, 0, 1) * (float)(nch - 1) + 0.5f) + (det > 0 ? k : -k);
        i = i < 0 ? 0 : i >= nch ? nch - 1 : i;
        float nv = (float)i / (float)(nch - 1);
        ch_param_set(ET, ui.slot, (unsigned)p, nv);
        ch_synth_value_text(model_of(ET, dev), (unsigned)p, nv, sv); app_say(param_name(ET, ui.slot, (unsigned)p), sv);
        return;
    }
    ch_param_set(ET, ui.slot, (unsigned)p, p < (int)CH_PARAMS ? click_to(pd, dev, (unsigned)p, ts->mode[row][p], cur, det) : cur + step_of(pd, det));
    if (p >= (int)CH_PARAMS) { char val[16]; value_text(pd, ch_param_of(ch_track_get(ET), ui.slot, dev, (unsigned)p), 0, val); app_say(param_name(ET, ui.slot, (unsigned)p), val); }
    if (p == 0 && (dev == CH_DEV_DRUM || dev == CH_DEV_SYNTH))
        app_say("MODEL", (dev == CH_DEV_DRUM ? ch_drum_names : ch_synth_names)[model_of(ET, dev)]);
    if (p == 0 && dev == CH_DEV_SYNTH && ui.slot == CH_MATERIAL) synth_presets(ET, model_of(ET, dev));
}
/* the master effects' amounts and bypasses as they are now: what a held HOME puts back when it is let go */
static void perf_snap(void)
{
    const ch_track_state *ms = ch_track_get(CH_MASTER);
    for (unsigned i = 0; i < CH_FX; ++i) {
        unsigned slot = CH_FX1 + i, dev = ms->device[slot];
        int p = dev == CH_DEV_BYPASS ? -1 : ch_fx_amount_param(dev);
        ui.perf_was[i] = p < 0 ? -2.0f : ms->param[ch_row(slot, dev)][p]; ui.perf_byp[i] = ms->bypass[slot];
    }
}
/* PERFORM: K1..K4 the master effects' amounts (the FILTER, the MIXes, SIDECHAIN's AMOUNT); a bypassed slot comes on */
static void edit_perform(unsigned i, int32_t det)
{
    const ch_track_state *ts = ch_track_get(CH_MASTER);
    unsigned slot = CH_FX1 + i, dev = ts->device[slot];
    int p = dev == CH_DEV_BYPASS ? -1 : ch_fx_amount_param(dev);
    if (p < 0) { app_say(MSLOT_NAME[slot], dev == CH_DEV_BYPASS ? "EMPTY" : "NO AMOUNT"); return; }
    const ch_param_desc *pd = &ch_device_info(dev)->param[p];
    unsigned row = ch_row(slot, dev);
    ch_param_set(CH_MASTER, slot, (unsigned)p, click_to(pd, dev, (unsigned)p, ts->mode[row][p], ts->param[row][p], det));
    if (ts->bypass[slot]) ch_bypass_set(CH_MASTER, slot, 0);
}
static void edit_mod(unsigned i, int32_t det)
{
    unsigned p = (ui.mod_page & 1) * PER_PAGE + i;
    if (p >= CH_MOD_PARAMS) return;
    const ch_track_state *ts = ch_track_get(ui.track);
    unsigned m = ui.mod_page / 2;
    const ch_param_desc *pd = ch_mod_param_info(ts->mod_type[m], p);
    if (ui.btn >> B_SEQ & 1u) {
        ui.seq_used = 1;
        if (!pd->modes) return;
        int nm = 0; while (pd->modes[nm]) ++nm;
        int md = ((int)ts->mod_mode[m][p] + (det > 0 ? 1 : -1) + nm) % nm;
        ch_mod_mode_set(ui.track, m, p, (unsigned)md);
        app_say(pd->name, pd->modes[md]);
        return;
    }
    int synced = pd->modes && ts->mod_mode[m][p] && p != 1 && !(ts->mod_type[m] == CH_MOD_FOLLOW && p == 6 && ts->mod_mode[m][p] == 1);   /* (p 1: the LFO's AMOUNT, whose mode is its polarity, not a division) */
    if (synced) {                                        /* a division a detent */
        int i = (int)ch_div_index(ts->mod_param[m][p]) + (det > 0 ? 1 : -1), n = (int)ch_div_count();
        i = i < 0 ? 0 : i >= n ? n - 1 : i;
        ch_mod_param_set(ui.track, m, p, ch_div_value((unsigned)i));
        return;
    }
    ch_mod_param_set(ui.track, m, p, ts->mod_param[m][p] + step_of(pd, det));
}
/* the SENDS page: K1 is FX SEND (into the master's delays and reverbs), K2..K4 the three other tracks in order (a track has no send to itself) */
static unsigned send_other(unsigned t, unsigned k) { unsigned o = k; if (o >= t) ++o; return o < CH_TRACKS ? o : 0; }
static const char *ss_name(unsigned k)
{
    static const char *const N[4] = {"MASTER ALL", "MASTER 2 3 4", "MASTER 3 4", "MASTER 4"};
    return N[k >= CH_SS_FX && k <= CH_SS_LAST ? k - CH_SS_FX : 0];
}
enum { MIX_PAGES = 11 };                             /* LEVEL PAN FILTER SENDS KEY LOAD SONG DISPLAY, SEND TO (8), AUTOPILOT (9) and SETUP (10) */
/* the pages in the order SELECT turns them: SEND TO comes after SENDS (its number stays 8: the others' are used all over) */
/* (LOAD, page 5, is not among them: GLO + EDIT opens it, a screen of its own) */
enum { MIX_NAV = 10 };
static const uint8_t MIX_ORDER[MIX_NAV] = {4, 0, 1, 2, 3, 8, 6, 9, 10, 7};   /* (KEY / TEMPO first: GLO opens on it) */
static unsigned mix_pos(unsigned page) { for (unsigned k = 0; k < MIX_NAV; ++k) if (MIX_ORDER[k] == page) return k; return 0; }
/* the next of where a track's step SEND joins the master chain: ALL (its first effect), or its 2nd, 3rd, 4th */
static unsigned ss_next(unsigned t, unsigned cur, int dir)
{
    (void)t;
    if (cur < CH_SS_FX || cur > CH_SS_LAST) cur = CH_SS_FX;
    if (dir > 0) return cur < CH_SS_LAST ? cur + 1u : cur;
    return cur > CH_SS_FX ? cur - 1u : cur;
}
/* the settings kept over a power-off (OBJ_SETTINGS, five bytes): MIDI SYNC, and in the last one bit 0 the first
 * power-on's card shown (for the build in the fifth byte), bit 1 RECORD's MICRO, bits 2 and 3 its COUNT IN, bit 5 the limiter off, bits 6 and 7 HOLD FX's slot */
static uint8_t build_id(void);
static void settings_save(void)
{
    uint8_t b[5] = {'S', 'Y', ui.sync, (uint8_t)((ui.help_told ? 1u : 0u) | (ui.rec_micro ? 2u : 0u) | (unsigned)ui.count_off << 2 | (ch_limiter() ? 0u : 32u) | (unsigned)(ui.snd_off >> 1) << 6), build_id()};
    plat_store_save(OBJ_SETTINGS, b, 5);
}
static void edit_mix(unsigned i, int32_t det)
{
    const ch_track_state *ts;
    float d = (float)det * enc_s;
    if (ui.mix_page == 5) {                              /* LOAD: K1 the cores, 2 or 1 (the second core's tracks back on the first) */
        if (i == 3) {                                    /* K4: the load table, the speed test, then the models' costs twelve a page */
            unsigned pages = 2 + (COST_N + 17) / 18;
            ui.spd_show = (uint8_t)((ui.spd_show + (det > 0 ? 1u : pages - 1u)) % pages);
            return;
        }
        if (i == 2) { ch_kit_core2_set(det > 0); app_say("KIT ON CORE 2", ch_kit_core2() ? "ALLOWED" : "NO"); return; }   /* K3: a KIT may go to the second core */
        if (i == 1) { ch_limiter_set(det > 0); app_say("LIMITER", det > 0 ? "ON" : "OFF"); settings_save(); return; }   /* K2: the master limiter */
        if (i != 0) return;
        one_core = det < 0;
#if defined(FM1B_CPU2) && FM1B_CPU2
        if (one_core) ch_worker(0, 0); else if (cpu2_ok) ch_worker(plat_cpu2_run, plat_cpu2_wait);
#endif
        app_say("CORES", one_core || !cpu2_ok ? "1: ALL ON THE FIRST" : "2");
        return;
    }
    if (ui.mix_page == 7) {                              /* DISPLAY: K1 the colours (COLOUR, or the whole UI in one tone), K2 the tone */
        char v[12] = "";
        if (i == 0) { ui.mono = det > 0; app_say("COLOURS", SP_PAL[ui.mono]); }
        else if (i == 1) {
            ui.spec_tone = (uint8_t)(((int)ui.spec_tone + (det > 0 ? 1 : -1) + 25) % 25);
            if (ui.spec_tone == 24) text_cat(v, "WHITE"); else { int_text((int32_t)ui.spec_tone * 15, v); text_cat(v, " DEG"); }
            app_say("TONE", v);
        }
        else if (i == 3) {
            static const char *const SY[4] = {"OFF", "SEND", "SEND DIRECT", "FOLLOW"};   /* (DIRECT: the USB port a host, an FM-1 on it through a powered hub; not kept: the next power-on is SEND, so a computer can always reach it) */
            int was = ui.sync == SYNC_FOLLOW ? 3 : ui.sync == SYNC_SEND ? 1 + ui.sync_host : 0;
            int k = was + (det > 0 ? 1 : -1); k = k < 0 ? 0 : k > 3 ? 3 : k;
            if (k != was) {
                ui.sync = (uint8_t)(k == 3 ? SYNC_FOLLOW : k ? SYNC_SEND : SYNC_OFF);
                if ((k == 2) != ui.sync_host) { ui.sync_host = k == 2; plat_usb_host(ui.sync_host); }
                settings_save();
            }
            app_say("MIDI SYNC", SY[k]);
        }
        else if (i == 2) { ui.panel_white = det > 0; app_say("PANEL ROW", ui.panel_white ? "WHITE" : "TRACK"); }
        return;
    }
    if (ui.mix_page == 10) {                             /* RECORD: K1 MICRO (a played note keeps where in its step it fell), K2 the count-in */
        static const char *const CI[3] = {"1 BAR", "OFF", "2 BARS"};
        if (i == 0) { ui.rec_micro = det > 0; app_say("REC MICRO", ui.rec_micro ? "ON: AS PLAYED" : "OFF: ON THE STEP"); settings_save(); }
        else if (i == 1) {                               /* (OFF, 1 BAR, 2 BARS, left to right) */
            static const uint8_t ORD[3] = {1, 0, 2};
            int k = ui.count_off == 1 ? 0 : ui.count_off == 0 ? 1 : 2; k = clampi(k + (det > 0 ? 1 : -1), 0, 2);
            ui.count_off = ORD[k]; app_say("COUNT IN", CI[ui.count_off]); settings_save();
        }
        else if (i == 2) { ch_limiter_set(det > 0); app_say("LIMITER", det > 0 ? "ON" : "OFF"); settings_save(); }   /* (the same switch as LOAD's K2; kept here) */
        else {                                           /* K4 HOLD FX: the master slot that HOME + K3 plays */
            unsigned q = (unsigned)clampi((int)(ui.snd_off >> 1) + (det > 0 ? 1 : -1), 0, 3);
            ui.snd_off = (uint8_t)(q << 1); app_say(MSLOT_NAME[CH_FX1 + q], fx_name(ch_track_get(CH_MASTER), CH_FX1 + q)); settings_save();
        }
        return;
    }
    if (ui.mix_page == 9) {                              /* AUTOPILOT: K1 on / off, K2 the bars between variations, K3 VARIETY, K4 REVERT or OVERRIDE */
        char v[16] = "";
        if (i == 0) { ap_set(det > 0); app_say("AUTOPILOT", ap.on ? (ch_snapshot_get()->playing ? "ON" : "ON: PRESS PLAY") : ap.revert ? "OFF: AS IT WAS" : "OFF: KEPT"); }
        else if (i == 1) { ap.bars_i = (uint8_t)clampi((int)ap.bars_i + (det > 0 ? 1 : -1), 0, 4); int_text(AP_BARS[ap.bars_i], v); text_cat(v, AP_BARS[ap.bars_i] == 1 ? " BAR" : " BARS"); app_say("EVERY", v); }
        else if (i == 2) { ap.variety = fm_clampf(ap.variety + (float)det * enc_s, 0.02f, 1); int_text((int32_t)(ap.variety * 100 + 0.5f), v); text_cat(v, "%"); app_say("VARIETY", v); }
        else if (i == 3) { ap.revert = det < 0; app_say("WHEN TURNED OFF", ap.revert ? "REVERT: AS IT WAS" : "OVERRIDE: KEEP IT"); }
        return;
    }
    if (ui.mix_page == 6) {                              /* SONG: K1 the entry, K2 its pattern (END closes the song), K3 bars, K4 on/off */
        unsigned e = ui.song_edit;
        if (i == 0) { int v = clampi((int)e + det, 0, PJ_SONG_MAX - 1); ui.song_edit = (uint8_t)v; char n[8]; int_text(v + 1, n); app_say("ENTRY", n); }
        else if (i == 1) {
            int v = clampi((int)song.e[e].pat + det, 0, PJ_PATTERNS); song.e[e].pat = (uint8_t)v;
            song.len = 0; while (song.len < PJ_SONG_MAX && song.e[song.len].pat) ++song.len;   /* up to the first END */
            if (v) { char n[8] = "P"; char d[4]; int_text(v, d); text_cat(n, d); app_say("PATTERN", n); } else app_say("PATTERN", "END");
        }
        else if (i == 2) { int v = clampi((int)(song.e[e].bars ? song.e[e].bars : 4) + det, 1, 64); song.e[e].bars = (uint8_t)v; char n[8]; int_text(v, n); app_say("BARS", n); }
        else if (i == 3) { song.on = !song.on; app_say("SONG", song.on ? "ON" : "OFF"); }
        return;
    }
    if (ui.mix_page == 4) {                              /* the master key: K1 its root (or OFF), K2 its scale; K3 TEMPO, K4 SWING */
        if (i == 2) { tempo_turn(det); return; }
        if (i == 3) { float sw = fm_clampf(ch_swing() + (float)det * enc_s, 0, 1); ch_swing_set(sw); char v[12]; int_text((int32_t)(50 + sw * 25 + 0.5f), v); text_cat(v, "%"); app_say("SWING", v); return; }
        if (i == 0) { ui.key_root = (uint8_t)((ui.key_root + (det > 0 ? 1u : 12u)) % 13u); key_apply(); app_say("KEY", KEY_NAME[ui.key_root]); }
        else if (i == 1) {
            ui.key_scale = (uint8_t)((ui.key_scale + (det > 0 ? 1u : SCALES - 1u)) % SCALES);
            if (ui.key_root) key_apply();
            app_say("KEY SCALE", SCALE_NAME[ui.key_scale]);
        }
        return;
    }
    if (ui.mix_page == 8) {                              /* SEND TO: where each track joins the master chain with its sends, a knob a track */
        if (i < CH_TRACKS) {
            unsigned k = ss_next(i, ch_track_get(i)->ss_dest, det);
            ch_mix_set(i, CH_MIX_SSDEST, (float)k);
            char n[12] = "T"; n[1] = (char)('1' + i); n[2] = 0; text_cat(n, " SEND TO");
            app_say(n, ss_name(k));
        }
        return;
    }
    if (ui.mix_page == 3) {                              /* the selected track's sends */
        if (i == 4) {                                    /* ALGO: where the steps' SEND joins the master chain */
            unsigned k = ss_next(ui.track, ch_track_get(ui.track)->ss_dest, det);
            ch_mix_set(ui.track, CH_MIX_SSDEST, (float)k);
            app_say("SEND TO", ss_name(k));
            return;
        }
        ts = ch_track_get(ui.track);
        if (i == 0) ch_mix_set(ui.track, CH_MIX_TOFX, fm_clampf(ts->tofx + d, 0, 1));   /* K1: FX SEND, into the master's returns */
        else if (i < 4) { unsigned o = send_other(ui.track, i - 1u); ch_send_set(ui.track, o, ts->send[o] + d); }   /* K2..K4: the other tracks */
        return;
    }
    if (i == 4 && ui.mix_page == 0) {                    /* MIX LEVEL, ALGO: the RETURN, all that the tracks send into the master effects */
        ch_return_set(fm_clampf(ch_return() + d, 0, 1));
        char v[8]; int_text((int32_t)(ch_return() * 100 + 0.5f), v); app_say("RETURN LEVEL", v);
        return;
    }
    if (i >= CH_TRACKS) return;
    ts = ch_track_get(i);
    if (ui.mix_page <= 2 && (ui.mac_arm || (ui.btn >> B_LFO & 1u))) {   /* LFO held (the chosen modulator, of the knob's track) or a macro being assigned: onto the track's LEVEL, PAN or FILTER */
        int mac = !(ui.btn >> B_LFO & 1u);
        unsigned mm = mac ? CH_MODS + ui.mac_arm - 1u : ui.mod;
        float depth = 0;
        for (unsigned k = 0; k < CH_MAPS; ++k)
            if (ts->map[k].depth != 0 && ts->map[k].mod == mm && ts->map[k].slot == CH_MIXSLOT && ts->map[k].param == ui.mix_page) depth = ts->map[k].depth;
        depth = fm_clampf(depth + (float)det * enc_s, -1, 1);
        if (depth > -0.005f && depth < 0.005f) depth = 0;
        if (!mac) ui.held_long[B_LFO] = 1;
        char a[16] = {'T', (char)('1' + i), ' ', 0}; text_cat(a, MIXMOD_NAME[ui.mix_page]);
        char t[20] = ""; char n[12];
        text_cat(t, mac ? "MACRO " : "MOD "); int_text((int32_t)(mac ? ui.mac_arm : mm + 1u), n); text_cat(t, n);
        if (ch_map_set(i, mm, CH_MIXSLOT, ui.mix_page, depth, !mac) < 0) { app_say(t, "NO FREE MAPS"); return; }
        text_cat(t, " "); if (depth > 0) text_cat(t, "+"); int_text((int32_t)(depth * 100 + (depth < 0 ? -0.5f : 0.5f)), n); text_cat(t, n);
        app_say(a, t);
        return;
    }
    if (ui.mix_page == 0) ch_mix_set(i, CH_MIX_LEVEL, fm_clampf(ts->level + d * 4, 0, 1));   /* (4 a detent: the whole way in about a turn) */
    else if (ui.mix_page == 1) ch_mix_set(i, CH_MIX_PAN, fm_clampf(ts->pan + d, -1, 1));
    else ch_mix_set(i, CH_MIX_FILTER, fm_clampf(ts->filter + d, -1, 1));
}

static int cyc_index(const ch_step *s)
{
    for (int i = 0; i < 10; ++i) if (CYC[i][0] == s->cyc_a && CYC[i][1] == s->cyc_b) return i;
    return 0;
}
/* the next note up or down a lane can take: by the scale's notes, or by semitones in CHROMA */
static int lane_note_step(unsigned t, int note, int dir)
{
    for (int k = 0; k < 12; ++k) { note += dir; if (note < 12 || note > 115) break; if (in_scale(t, note)) return note; }
    return note < 12 ? 12 : note > 115 ? 115 : note;
}
/* LANE NOTES: a lane's note heard for a moment as it is chosen or changed (let go by input(), a third of a second on) */
static uint8_t lane_heard, lane_heard_t; static uint32_t lane_heard_until;
static void lane_hear(unsigned n)
{
    if (lane_heard) ch_note(lane_heard_t, lane_heard, 0);
    ch_note(ui.track, n, 100);
    lane_heard = (uint8_t)n; lane_heard_t = ui.track; lane_heard_until = plat_ms() + 320u;
}
/* STEP view knobs: the chosen cell (and PRESET the lane's note), or with SEQ held the track's settings */
static void edit_step(unsigned i, int32_t det)
{
    const ch_seq_state *sq = ch_seq_get(ui.track);
    char v[16];
    if (ui.btn >> B_SEQ & 1u) {
        ui.seq_used = 1;
        if (i == 0) { int32_t l = clampi(sq->len + det, 1, CH_STEPS); ch_seq_len_set(ui.track, (unsigned)l); int_text(l, v); app_say("LENGTH", v); }
        else if (i == 1) { int32_t r = clampi(sq->rate + (det > 0 ? 1 : -1), 0, 4); ch_seq_rate_set(ui.track, (unsigned)r); app_say("RATE", RATE_NAME[r]); }
        else if (i == 2) { int32_t d = (sq->dir + (det > 0 ? 1 : CH_DIRS - 1)) % CH_DIRS; ch_seq_dir_set(ui.track, (unsigned)d); app_say("DIRECTION", DIR_NAME[d]); }
        else if (i == 3) { float s = fm_clampf(ch_swing() + (float)det * enc_s, 0, 1); ch_swing_set(s); int_text((int32_t)(50 + s * 25 + 0.5f), v); text_cat(v, "%"); app_say("SWING", v); }
        else if (i == 4 && ui.view == V_SONG) tempo_turn(det);
        else if (i == 4) {
            if (ui.key_root && !ui.tkey[ui.track]) { app_say("SCALE", "MASTER KEY (EDIT + ALGO: OWN)"); return; }   /* the key decides; a track key overrides it */
            ui.scale[ui.track] = (uint8_t)((ui.scale[ui.track] + (det > 0 ? 1 : SCALES - 1)) % SCALES); lanes_layout(ui.track); app_say("SCALE", SCALE_NAME[ui.scale[ui.track]]);
        } else {                                        /* with the key on: the track's octave */
            ui.root[ui.track] = (uint8_t)clampi(ui.root[ui.track] + (keyed(ui.track) ? (det > 0 ? 12 : -12) : det), 24, 96);
            lanes_layout(ui.track); note_text((unsigned)eff_root(ui.track), v); app_say("ROOT", v);
        }
        return;
    }
    if (ui.lane_edit && ui.view == V_STEP) {             /* LANE NOTES: K1 the chosen lane's note, a scale step a click (a semitone with no scale); the other knobs rest */
        if (i != 0) return;
        int n = lane_note_step(ui.track, sq->note[ui.sel_lane], det > 0 ? 1 : -1);
        ch_lane_note_set(ui.track, ui.sel_lane, (unsigned)n);
        lane_hear((unsigned)n);
        note_text((unsigned)n, v);
        char l[12] = "LANE "; char d[4]; int_text(ui.sel_lane + 1, d); text_cat(l, d);
        app_say(l, v);
        return;
    }
    if (i == 0 && ui.lane_held >= 0) {                  /* a lane's key held + K1: its note, along the scale */
        ui.sel_lane = (uint8_t)ui.lane_held;
        int n = lane_note_step(ui.track, sq->note[ui.sel_lane], det > 0 ? 1 : -1);
        ch_lane_note_set(ui.track, ui.sel_lane, (unsigned)n);
        note_text((unsigned)n, v);
        char l[12] = "LANE "; char d[4]; int_text(ui.sel_lane + 1, d); text_cat(l, d);
        app_say(l, v);
        return;
    }
    if (ui.sel_step < 0) return;
    if (ui.held_step >= 0) ui.step_edited = 1;
    ch_step s = sq->step[ui.sel_lane][ui.sel_step];
    if (ui.step_alt && (i == 2 || i == 3)) i = i == 2 ? 4u : 5u;   /* the second page: K3 CYCLE, K4 GATE */
    else if (ui.step_alt && i == 1) i = 6u;                          /* and K2 SEND */
    else if (ui.step_alt && i == 0) {                                /* and K1 OCTAVE: the step's note up or down, to three octaves either way (a KIT: that drum's hit) */
        if (!s.vel) { app_say("OCTAVE", "STEP IS OFF"); return; }
        int o = clampi(ch_step_oct(&s) + (det > 0 ? 1 : -1), -3, 3);
        ch_step_oct_set(&s, o);
        v[0] = 0; if (o > 0) text_cat(v, "+"); { char n[4]; int_text(o, n); text_cat(v, n); } app_say("OCTAVE", v);
        ch_step_set(ui.track, ui.sel_lane, (unsigned)ui.sel_step, &s);
        return;
    }
    if (i == 0) { s.vel = (uint8_t)clampi(s.vel + det * 4, 0, 127); int_text(s.vel, v); app_say("VELOCITY", v); }
    else {
        if (!s.vel) {                                    /* editing an empty cell sets it (not CYCLE: that only edits) */
            if (i == 4) { app_say("CYCLE", "STEP IS OFF"); return; }
            s.vel = 100;
        }
        if (i == 1) { s.ratchet = (uint8_t)clampi(s.ratchet + (det > 0 ? 1 : -1), 1, CH_RATCHET_LAST); ratchet_text(s.ratchet, v); app_say("RATCHET", v); }
        else if (i == 2) { s.chance = (uint8_t)clampi(s.chance + det * 5, 0, 100); int_text(s.chance, v); text_cat(v, "%"); app_say("CHANCE", v); }
        else if (i == 3) { s.micro = (int8_t)clampi(s.micro + det, -50, 50); int_text(s.micro, v); text_cat(v, "%"); app_say("MICRO", v); }   /* 1 % of a step a detent */
        else if (i == 6) {                               /* SEND: what this hit adds to the track's send, a fifteenth a detent */
            int f = clampi((int)ch_step_send(&s) + (det > 0 ? 1 : -1), 0, 15);
            s.send = (uint8_t)f; int_text((f * 100 + 7) / 15, v); text_cat(v, "%"); app_say("SEND", v);
        }
        else if (i == 5) {                               /* GATE: how much of the step the note is held, a fifteenth a detent (as SEND) */
            s.ungate = (uint8_t)clampi((int)s.ungate - (det > 0 ? 1 : -1), 0, 15);
            int_text((int32_t)ch_step_gate(&s), v); text_cat(v, "%"); app_say("GATE", v);
        }
        else {
            int c = clampi(cyc_index(&s) + (det > 0 ? 1 : -1), 0, 9);
            s.cyc_a = CYC[c][0]; s.cyc_b = CYC[c][1];
            if (!c) app_say("CYCLE", "OFF");
            else { int_text(s.cyc_a, v); text_cat(v, ":"); char b[4]; int_text(s.cyc_b, b); text_cat(v, b); app_say("CYCLE", v); }
        }
    }
    ch_step_set(ui.track, ui.sel_lane, (unsigned)ui.sel_step, &s);
}

/* ---- parameter locks: REC held + steps arms them and starts recording; a knob on an EDIT page then writes that
 * value as a lock on every armed step (the track's own setting stays); a REC tap ends it ---- */
static int lock_armed(unsigned idx) { return (ui.lock_steps[idx >> 5] >> (idx & 31u)) & 1u; }
static unsigned lock_count(void) { unsigned n = 0; for (unsigned i = 0; i < CH_STEPS; ++i) n += lock_armed(i); return n; }
static int lock_first(void) { for (unsigned i = 0; i < CH_STEPS; ++i) if (lock_armed(i)) return (int)i; return -1; }
static void lock_arm(unsigned idx)
{
    if (ui.lock_rec && ui.lock_track != ui.track) { ui.lock_steps[0] = ui.lock_steps[1] = 0; }   /* another track: start over */
    ui.lock_track = ui.track;
    ui.lock_steps[idx >> 5] ^= 1u << (idx & 31u);
    ui.lock_rec = lock_count() != 0;
    ui.sel_step = (int16_t)idx; song_sync_lane();
    char v[16] = ""; int_text((int32_t)lock_count(), v); text_cat(v, " STEPS ARMED");
    app_say(ui.lock_rec ? "LOCK REC" : "LOCK REC OFF", ui.lock_rec ? v : "");
    ui.redraw = 1;
}
static void lock_end(void)
{
    ui.lock_rec = 0; ui.lock_steps[0] = ui.lock_steps[1] = 0;
    app_say("LOCK REC", "DONE"); ui.redraw = 1;
}
/* a knob while recording: the lock's value on the armed steps, from the first armed step's lock (or the knob) */
static void lock_turn(unsigned i, int32_t det)
{
    int p = tile_param(i);
    if (p < 0) return;
    const ch_track_state *ts = ch_track_get(ui.track);
    unsigned dev = ts->device[ui.slot], row = ch_row(ui.slot, dev);
    const ch_param_desc *pd = &ch_device_info(dev)->param[p];
    int first = lock_first();
    if (p >= (int)CH_PARAMS) { app_say(param_name(ui.track, ui.slot, (unsigned)p), "NO LOCKS ON THIS"); return; }
    float v = ts->param[row][p];
    if (first >= 0) ch_lock_get(ui.track, (unsigned)first, ui.slot, (unsigned)p, &v);
    int pr = pitch_range(ui.slot, dev, p);
    v = pr ? pitch_step(v, pr, ts->mode[row][p], det) : v + step_of(pd, det);
    float lo = pd->kind == CH_BI ? -1.0f : 0.0f, hi = pd->kind == CH_SELECT ? (float)(pd->steps - 1) : 1.0f;
    v = fm_clampf(v, lo, hi);
    if (pd->kind == CH_SELECT) v = (float)(int)(v + 0.5f);
    int full = 0;
    for (unsigned k = 0; k < CH_STEPS; ++k) if (lock_armed(k) && ch_lock_set(ui.track, k, ui.slot, (unsigned)p, v) != 0) full = 1;
    char val[16]; value_text(pd, v, ts->mode[row][p], val);
    if (pr) pitch_text(v, pr, ts->mode[row][p], val);
    if (dev >= CH_DEV_FX_FIRST) { char fv[16]; if (ch_fx_value_text(dev, (unsigned)p, v, ts->mode[row][p], fv)) { val[0] = 0; text_cat(val, fv); } }
    crush_text(ui.track, ui.slot, dev, p, v, val);
    if (dev == CH_DEV_SYNTH && ui.slot == CH_MATERIAL) { char sv[16]; if (ch_synth_value_text(model_of(ui.track, dev), (unsigned)p, v, sv)) { val[0] = 0; text_cat(val, sv); } }
    if (p == 0 && (dev == CH_DEV_DRUM || dev == CH_DEV_SYNTH)) { val[0] = 0; text_cat(val, (dev == CH_DEV_DRUM ? ch_drum_names : ch_synth_names)[(int)v]); }
    char nm[20] = "LOCK "; text_cat(nm, param_name(ui.track, ui.slot, (unsigned)p));
    app_say(full ? "LOCKS FULL (32)" : nm, val);
    ui.redraw = 1;
}

static void change_device(int32_t det)
{
    const ch_track_state *ts = ch_track_get(ui.track);
    if (ui.slot == CH_MATERIAL) {
        static const uint8_t order[6] = {CH_DEV_DRUM, CH_DEV_KIT, CH_DEV_SYNTH, CH_DEV_POLY, CH_DEV_GRAIN, CH_DEV_BYPASS};
        int at = 0;
        for (int k = 0; k < 6; ++k) if (order[k] == ts->device[0]) at = k;   /* (all six: OFF was passed over, and a turn right from it went past DRUM) */
        at = (at + (det > 0 ? 1 : 5)) % 6;
        ch_device_set(ui.track, 0, order[at]);
        ch_bypass_set(ui.track, 0, 0);
        if (order[at] == CH_DEV_SYNTH) synth_presets(ui.track, model_of(ui.track, CH_DEV_SYNTH));
        ui.page[0] = 0;
        app_say("ENGINE", order[at] == CH_DEV_BYPASS ? "OFF" : ch_device_info(order[at])->name);
    } else {
        ch_bypass_set(ui.track, ui.slot, !ts->bypass[ui.slot]);
        app_say(SLOT_NAME[ui.slot], ts->bypass[ui.slot] ? "BYPASS" : ch_device_info(ts->device[ui.slot])->name);
    }
}

static void main_action(void)
{
    if (ui.slot >= CH_SLOTS) return;
    const ch_track_state *ts = ch_track_get(ui.track);
    unsigned dev = ts->device[ui.slot];
    switch (dev) {
    case CH_DEV_DRUM: case CH_DEV_KIT: case CH_DEV_SYNTH: case CH_DEV_POLY: case CH_DEV_GRAIN: ch_note(ui.track, 60, 110); ch_note(ui.track, 60, 0); break;
    default: break;
    }
}
static void device_action(unsigned a)
{
    if (ui.slot >= CH_SLOTS) return;
    const ch_track_state *ts = ch_track_get(ui.track);
    const ch_device_desc *d = ch_device_info(ts->device[ui.slot]);
    unsigned n = 0; while (d->actions[n]) ++n;
    if (a >= n) return;
    unsigned v = 1;
    ch_action(ui.track, ui.slot, a, v);
    app_say(d->name, d->actions[a]);
}

/* ------------------------------------------------------------------ input --- */
/* MIDI SYNC, FOLLOW: the clock in. The tempo is the clocks' own rate, measured over the last four beats; while
 * playing, the sequencer's beat is held on the count of clocks since the start (24 a beat) by leaning the tempo a
 * little, never by a jump: two seconds to close a gap, 12 % at most. Start, stop and clock go on to the other links
 * (a chain). No clock for two seconds: the tempo stays where it was and the FM-1 is on its own again. */
static struct { uint32_t n, t_last, mark[5], q; float bpm, set; uint8_t run, have; } syn;
static void sync_clock(uint32_t st)
{
    uint32_t now = plat_ms();
    if (st == 0xF8u) {
        syn.t_last = now; syn.have = 1; ++syn.n;
        if (syn.q++ % 24u == 0) {                        /* a beat's mark: the tempo from the marks kept (up to four beats back) */
            for (int k = 4; k > 0; --k) syn.mark[k] = syn.mark[k - 1];
            syn.mark[0] = now;
            unsigned beats = syn.q / 24u; if (beats > 4) beats = 4;
            if (beats) {
                uint32_t dt = now - syn.mark[beats];
                if (dt > 150u * beats && dt < 2600u * beats) {
                    float inst = 60000.0f * (float)beats / (float)dt;
                    float d = inst - syn.bpm; if (d < 0) d = -d;
                    syn.bpm = syn.bpm > 0 && d < syn.bpm * 0.08f ? syn.bpm + (inst - syn.bpm) * 0.2f : inst;   /* (a real change of tempo: taken at once) */
                }
            }
        }
    } else if (st == 0xFAu || st == 0xFBu) {
        if (ch_snapshot_get()->playing) ch_transport(0);     /* (already playing: from the top) */
        ch_transport(1); syn.n = 0; syn.run = 1; syn.t_last = now;
    } else if (st == 0xFCu) { ch_transport(0); syn.run = 0; }
}
static void sync_follow(void)
{
    if (ui.sync != SYNC_FOLLOW) { syn.have = 0; syn.q = 0; syn.bpm = 0; syn.run = 0; return; }
    if (!syn.have) return;
    uint32_t now = plat_ms();
    if (now - syn.t_last > 2000u) { syn.have = 0; syn.q = 0; syn.bpm = 0; syn.run = 0; return; }
    if (syn.bpm <= 0) return;
    float bpm = syn.bpm;
    const ch_snapshot *sn = ch_snapshot_get();
    if (sn->playing && syn.run) {
        float ext = (float)(now - syn.t_last) * bpm / 60000.0f; if (ext > 1.0f / 24) ext = 1.0f / 24;
        float err = (float)syn.n / 24.0f + ext - sn->beat;
        if (err < 8 && err > -8) {                       /* (further apart than two bars: not this start's count) */
            float c = err * 30.0f, lim = bpm * 0.12f;
            bpm += c > lim ? lim : c < -lim ? -lim : c;
        }
    }
    float d = bpm - ch_tempo(); if (d < 0) d = -d;       /* (against the tempo as it is: a turn of TEMPO here is put back) */
    if (d > 0.02f) { syn.set = bpm; ch_tempo_set(bpm); ui.redraw = 1; }
}
static void midi_poll(void)
{
    uint32_t pkt; int port;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = (pkt >> 8) & 0xFFu, d1 = (pkt >> 16) & 0x7Fu, d2 = pkt >> 24;
        unsigned ch = st & 15u;
        if (ui.sync == SYNC_FOLLOW && (st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu)) {
            sync_clock(st);
            plat_midi_send(pkt, (TO_USB | TO_BLE) & ~plat_midi_in_from());
            continue;
        }
        if (ui.sync == SYNC_SEND && st >= 0xF8u) continue;   /* (the master takes no clock, start or stop from outside) */
        if (st == 0xFAu) ch_transport(1);
        else if (st == 0xFCu) ch_transport(0);
        else if (ch < CH_TRACKS && ((st & 0xF0u) == 0x90u || (st & 0xF0u) == 0x80u))
            ch_note(ch, d1, (st & 0xF0u) == 0x90u ? d2 : 0);
    }
}

/* live record: a played note lands on its lane, on the step the playhead is on */
static int live_micro(unsigned t);
static void live_write(unsigned note, unsigned vel)
{
    int st = live_step(ui.track);
    if (st < 0) return;
    const ch_seq_state *sq = ch_seq_get(ui.track);
    unsigned l = 0; int best = 1000;                     /* the lane nearest the note played (a tie: the lower) */
    for (unsigned k = 0; k < CH_LANES; ++k) {
        int d = (int)sq->note[k] - (int)note; if (d < 0) d = -d;
        if (d < best || (d == best && sq->note[k] < sq->note[l])) { best = d; l = k; }
    }
    ch_step s = sq->step[l][st];
    s.vel = (uint8_t)vel;
    if (!s.ratchet) s.ratchet = 1;
    if (!s.chance) s.chance = 100;
    if (ui.rec_micro) s.micro = (int8_t)live_micro(ui.track);   /* RECORD, MICRO: where in the step it was played */
    ch_step_set(ui.track, l, (unsigned)st, &s);
}

/* the step a live tap belongs to: the playhead's, or the next one when the tap is in the step's later half
 * (going forward), so a tap a hair early lands on the beat */
static int live_step(unsigned t)
{
    const ch_snapshot *sn = ch_snapshot_get();
    const ch_seq_state *sq = ch_seq_get(t);
    int st = sn->step[t], len = sq->len ? sq->len : 16;
    if (st < 0) return -1;
    if (sn->step_frac[t] > 0.5f && sq->dir == CH_DIR_FWD) st = (st + 1) % len;
    return st;
}
/* how far from that step's start the tap fell, per cent of a step, -50 .. +50 (a step's MICRO): past the half it
 * belongs to the next step and is early for it. Forward only: another direction has no next step to be early for */
static int live_micro(unsigned t)
{
    const ch_snapshot *sn = ch_snapshot_get();
    if (sn->step[t] < 0 || ch_seq_get(t)->dir != CH_DIR_FWD) return 0;
    float f = sn->step_frac[t];
    return clampi((int)((f > 0.5f ? f - 1 : f) * 100 + (f > 0.5f ? -0.5f : 0.5f)), -50, 50);
}
/* a track pad (main views, REC on, playing): the track's base note (lane 1) on the nearest step, added */
static void pad_hit(unsigned t)
{
    unsigned n = ch_seq_get(t)->note[0];
    ch_note(t, n, 100); ui.pad_note[t] = (uint8_t)n;
    int st = live_step(t);
    if (st < 0) return;
    ch_step s = ch_seq_get(t)->step[0][st];
    if (!s.vel) { s.vel = 100; if (!s.ratchet) s.ratchet = 1; if (!s.chance) s.chance = 100; if (ui.rec_micro) s.micro = (int8_t)live_micro(t); ch_step_set(t, 0, (unsigned)st, &s); }
}

/* a cell on or off: on with the defaults, or cleared */
/* SEL held on a set step in the step view: SELECT moves it along its lane, ALGO to another lane; the step it lands on
 * takes its place (a swap), its locks go with it along the lane; letting go of SEL leaves it there */
static int step_sel_set(void)
{
    return ui.view == V_STEP && ui.sel_step >= 0 && ch_seq_get(ui.track)->step[ui.sel_lane][ui.sel_step].vel != 0;
}
static void step_move(int vertical, int det)
{
    const ch_seq_state *q = ch_seq_get(ui.track);
    unsigned t = ui.track, len = q->len ? q->len : 16;
    unsigned l0 = ui.sel_lane, s0 = (unsigned)ui.sel_step, l1 = l0, s1 = s0;
    if (vertical) { int l = (int)l0 - det; l1 = (unsigned)clampi(l, 0, CH_LANES - 1); }   /* (up the screen is up) */
    else s1 = (unsigned)(((int)s0 + det) % (int)len + (int)len) % len;
    if (l1 == l0 && s1 == s0) return;
    ch_step a = q->step[l0][s0], b = q->step[l1][s1];
    ch_step_set(t, l1, s1, &a); ch_step_set(t, l0, s0, &b);
    if (!vertical) {                                     /* the locks of the two steps change places too */
        ch_lock la[CH_LOCKS], lb[CH_LOCKS]; unsigned na = 0, nb = 0;
        for (unsigned k = 0; k < q->nlock && k < CH_LOCKS; ++k) {
            if (q->lock[k].step == s0) la[na++] = q->lock[k];
            else if (q->lock[k].step == s1) lb[nb++] = q->lock[k];
        }
        if (na || nb) {
            ch_lock_clear(t, s0); ch_lock_clear(t, s1);
            for (unsigned k = 0; k < na; ++k) ch_lock_set(t, s1, la[k].slot, la[k].param, (float)la[k].v / 256.0f);
            for (unsigned k = 0; k < nb; ++k) ch_lock_set(t, s0, lb[k].slot, lb[k].param, (float)lb[k].v / 256.0f);
        }
    }
    ui.sel_lane = (uint8_t)l1; ui.sel_step = (int16_t)s1; ui.step_page = (uint8_t)(s1 / 16); song_sync_lane();
    char v[16] = ""; char n[8]; note_text(q->note[l1], v); text_cat(v, " "); int_text((int32_t)s1 + 1, n); text_cat(v, n);
    app_say("MOVED TO", v); ui.redraw = 1;
}
static void cell_toggle(unsigned lane, unsigned idx)
{
    ch_step s = ch_seq_get(ui.track)->step[lane][idx];
    if (s.vel) s.vel = 0;
    else { s.vel = 100; if (!s.ratchet) s.ratchet = 1; if (!s.chance) s.chance = 100; }
    s.tie = 0;
    ch_step_set(ui.track, lane, idx, &s);
    if (!s.vel) {                                        /* the step empty on every lane: its locks go too */
        const ch_seq_state *q = ch_seq_get(ui.track); int any = 0;
        for (unsigned l = 0; l < CH_LANES; ++l) any |= q->step[l][idx].vel != 0;
        if (!any) ch_lock_clear(ui.track, idx);
    }
}
/* a tie: a step held, a later step tapped: the steps after the held one up to it are tied to it (on, with the held
 * step's velocity, no retrigger: one long note). Tapping a tied step on its own clears it as any step. */
static void tie_to_lane(unsigned t, unsigned lane, unsigned from, unsigned to);
static void tie_to(unsigned lane, unsigned from, unsigned to)
{
    const ch_seq_state *sq = ch_seq_get(ui.track);
    ch_step head = sq->step[lane][from];
    if (!head.vel) { head.vel = 100; head.ratchet = 1; head.chance = 100; head.tie = 0; ch_step_set(ui.track, lane, from, &head); }
    for (unsigned i = from + 1; i <= to && i < CH_STEPS; ++i) {
        ch_step s = sq->step[lane][i];
        s.vel = head.vel; s.ratchet = 1; s.chance = 100; s.tie = 1; s.micro = 0;
        ch_step_set(ui.track, lane, i, &s);
    }
    char v[12] = ""; int_text((int32_t)(to - from + 1), v); text_cat(v, " STEPS"); app_say("TIE", v);
}
static void step_key(int w, int down)
{
    unsigned idx = ui.step_page * 16u + (unsigned)w;
    ch_step s = ch_seq_get(ui.track)->step[ui.sel_lane][idx];
    /* a tie only from a step held on purpose (a third of a second): two steps pressed together in passing, as fingers roll
     * over a beat, are two steps, not a tie that would hold through (silence) the ones between */
    if (down && ui.held_step >= 0 && (int)idx > ui.held_step && plat_ms() - ui.held_at >= 330u) { tie_to(ui.sel_lane, (unsigned)ui.held_step, idx); ui.step_edited = 1; ui.redraw = 1; return; }
    if (down && ui.held_step >= 0 && (int)idx != ui.held_step) ui.held_step = -1;   /* (pressed in passing: the first is let be)*/
    if (down) {
        ui.sel_step = (int16_t)idx; ui.held_step = (int16_t)idx; ui.step_edited = 0; ui.held_at = plat_ms();
        ui.step_was_on = s.vel > 0;
        if (!s.vel) cell_toggle(ui.sel_lane, idx);          /* an empty cell turns on at once */
    } else if (ui.held_step == (int16_t)idx) {
        if (ui.step_was_on && !ui.step_edited) cell_toggle(ui.sel_lane, idx);   /* a tap on a set cell clears it */
        ui.held_step = -1;
    }
    ui.redraw = 1;
}
/* the tempo: a BPM a detent, four on a fast turn */
static void tempo_turn(int32_t det)
{
    int32_t a = det < 0 ? -det : det;
    ch_tempo_set(ch_tempo() + (float)det * (a >= 3 ? 4.0f : 1.0f));
    char v[12]; int_text((int32_t)(ch_tempo() + 0.5f), v); text_cat(v, " BPM"); app_say("TEMPO", v);
}
/* the TRACKS view (HOME): a track's lanes merged into one row. A cell is the lowest lane with a hit on that step,
 * or the root lane (lane 1) when none has one: the knobs edit that lane's cell */
static unsigned song_lane(unsigned t, unsigned idx)
{
    const ch_seq_state *q = ch_seq_get(t);
    for (unsigned l = 0; l < CH_LANES; ++l) if (q->step[l][idx].vel) return l;
    return 0;
}
static void song_sync_lane(void) { if (ui.view == V_SONG && ui.sel_step >= 0) ui.sel_lane = (uint8_t)song_lane(ui.track, (unsigned)ui.sel_step); }
/* a step of the chosen track: with any lane on, every lane off; empty, the root lane on */
static void song_toggle(unsigned idx)
{
    const ch_seq_state *q = ch_seq_get(ui.track);
    int any = 0;
    for (unsigned l = 0; l < CH_LANES; ++l) any |= q->step[l][idx].vel != 0;
    if (any) { for (unsigned l = 0; l < CH_LANES; ++l) if (q->step[l][idx].vel) cell_toggle(l, idx); }
    else cell_toggle(0, idx);
    song_sync_lane();
}
static void song_step_key(int w, int down)
{
    unsigned idx = ui.step_page * 16u + (unsigned)w;
    const ch_seq_state *q = ch_seq_get(ui.track);
    int any = 0;
    for (unsigned l = 0; l < CH_LANES; ++l) any |= q->step[l][idx].vel != 0;
    if (down && ui.held_step >= 0 && (int)idx > ui.held_step && plat_ms() - ui.held_at >= 330u) {   /* held on purpose + a later step: tied */
        tie_to(song_lane(ui.track, (unsigned)ui.held_step), (unsigned)ui.held_step, idx); ui.step_edited = 1; ui.redraw = 1; return;
    }
    if (down && ui.held_step >= 0 && (int)idx != ui.held_step) ui.held_step = -1;   /* (pressed in passing: the first is let be) */
    if (down) {
        ui.sel_step = (int16_t)idx; ui.held_step = (int16_t)idx; ui.step_edited = 0; ui.held_at = plat_ms();
        ui.step_was_on = any != 0;
        if (!any) song_toggle(idx);                          /* an empty step turns on at once */
        song_sync_lane();
    } else if (ui.held_step == (int16_t)idx) {
        if (ui.step_was_on && !ui.step_edited) song_toggle(idx);   /* a tap on a set step clears it */
        ui.held_step = -1;
    }
    ui.redraw = 1;
}
/* the black keys in the TRACKS view: 1-4 the track (heard: its root note; REC on and playing: a hit written),
 * 9/10 the chosen step on/off */
static void song_black(int b, int down)
{
    if (!down) {
        if (b < CH_TRACKS && ui.audition[b]) { ch_note((unsigned)b, ui.audition[b], 0); ui.audition[b] = 0; }
        return;
    }
    if (b < CH_TRACKS) {
        ui.track = (uint8_t)b; ui.mix_ret = 0; song_sync_lane();
        if (ui.live_rec && ch_snapshot_get()->playing) pad_hit((unsigned)b);
        else { unsigned n = ch_seq_get((unsigned)b)->note[0]; ch_note((unsigned)b, n, 100); ui.audition[b] = (uint8_t)n; }
        char n[4] = {'T', (char)('1' + b), 0}; app_say(n, material_name((unsigned)b));
    } else if (b == 8 || b == 9) {
        if (ui.sel_step >= 0) song_toggle((unsigned)ui.sel_step);
    }                                                    /* (11: nothing; the mute is ENV held + a track key) */
    ui.redraw = 1;
}

/* ---- generators (SEQ held + white keys 12-16 in the sequencer views): a line in key, a beat, random parameters,
 * random lanes, a variation ----
 * The lanes are laid out in the track's key (lanes_layout), so a melody is a walk over lane numbers: it stays in
 * key by construction. A new line each press: the generator is stirred with the moment of the press (the microsecond
 * clock, the battery reading), so the sequence depends on the player's timing, not on how many times it has run. */
static uint32_t gen_seed = 0x2545F491u;
static uint32_t gen_rnd(void) { uint32_t x = gen_seed; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return gen_seed = x; }
static uint32_t gen_mix(uint32_t h) { h ^= h >> 16; h *= 0x7FEB352Du; h ^= h >> 15; h *= 0x846CA68Bu; h ^= h >> 16; return h; }   /* a hash: the same seed, the same list */
/* Sting's NOTES: sixteen lists of sixteen random notes, list k having k - 1 of its positions replaced by the root; TYPE k
 * plays list k. Here each list is drawn from the note seed, so a TYPE is a melody of its own and comes back as it was */
static void gen_type_list(uint32_t nseed, unsigned type, unsigned page, uint8_t *deg, uint8_t *oct, uint8_t *root)
{
    uint32_t x = gen_mix(nseed ^ type * 0x9E3779B9u ^ page * 0x85EBCA6Bu) | 1u;
    uint8_t perm[16];
    for (unsigned k = 0; k < 16; ++k) {
        x ^= x << 13; x ^= x >> 17; x ^= x << 5; deg[k] = (uint8_t)(x % 7u);
        x ^= x << 13; x ^= x >> 17; x ^= x << 5; oct[k] = (uint8_t)(x % 100u < 40u);
        perm[k] = (uint8_t)k; root[k] = 0;
    }
    for (unsigned k = 15; k > 0; --k) { x ^= x << 13; x ^= x >> 17; x ^= x << 5; unsigned j = x % (k + 1); uint8_t t = perm[k]; perm[k] = perm[j]; perm[j] = t; }
    for (unsigned k = 0; k + 1 < type && k < 16; ++k) root[perm[k]] = 1;   /* k - 1 of them: the root */
}
static void gen_stir(void)
{
    uint32_t e = plat_ticks24() * 2654435761u ^ plat_ms() * 0x9E3779B9u ^ (uint32_t)plat_battery_raw() << 20 ^ ui.entropy;
    gen_seed ^= e; if (!gen_seed) gen_seed = 0x2545F491u;
    for (int i = 0; i < 4; ++i) gen_rnd();                 /* a few turns to mix it in */
}
static int gen_chance(unsigned pct) { return gen_rnd() % 100u < pct; }
static void gen_clear(unsigned t)
{
    ch_step off = {0, 1, 100, 0, 0, 0};
    for (unsigned l = 0; l < CH_LANES; ++l) for (unsigned i = 0; i < CH_STEPS; ++i) ch_step_set(t, l, i, &off);
    ch_lock_clear(t, CH_STEPS);
}
static void gen_put(unsigned t, unsigned lane, unsigned i, unsigned vel, unsigned ratchet)
{
    ch_step st = {(uint8_t)vel, (uint8_t)ratchet, 100, 0, 0, 0};
    ch_step_set(t, lane, i, &st);
}
/* MELODY: an 8-step motif (a rhythm with the beats favoured, a walk in small steps with the odd leap), repeated over
 * the length with variations, each bar's end a little different; accents on the beats */
static void gen_melody(unsigned t)
{
    unsigned len = ch_seq_get(t)->len ? ch_seq_get(t)->len : 16;
    gen_clear(t);
    int motif[8]; int lane = 2 + (int)(gen_rnd() % 3u), same = 0, dir = gen_chance(50) ? 1 : -1;
    unsigned dens = 35 + gen_rnd() % 30u;                 /* how busy: 35..65 % of the steps */
    for (unsigned i = 0; i < 8; ++i) {
        unsigned w = (i & 3u) == 0 ? dens + 40 : (i & 1u) == 0 ? dens + 5 : dens - 20;   /* beats, off-beats, 16ths */
        if (!gen_chance(w > 95 ? 95 : w)) { motif[i] = -1; continue; }
        unsigned r = gen_rnd() % 100u;
        int d = r < 12 ? 0 : r < 55 ? 1 : r < 80 ? 2 : r < 92 ? 3 : 5;  /* a step, a third, the odd leap */
        if (d == 0 && same >= 1) d = 1;                    /* a note twice at most */
        if (gen_chance(30)) dir = -dir;                    /* mostly on in one direction: a line, not a dither */
        d *= dir;
        if (lane + d < 0 || lane + d > CH_LANES - 1) { dir = -dir; d = -d; }
        same = d == 0 ? same + 1 : 0;
        lane += d; if (lane < 0) lane = 0; if (lane > CH_LANES - 1) lane = CH_LANES - 1;
        motif[i] = lane;
    }
    if (motif[0] < 0) motif[0] = lane;                     /* it starts on a note */
    for (unsigned i = 0; i < len; ++i) {
        int m = motif[i & 7u];
        if (i >= 8) {                                      /* later bars: a few notes moved, dropped or added */
            if (m >= 0 && gen_chance(20)) m += gen_chance(50) ? 1 : -1;
            else if (m >= 0 && gen_chance(10)) m = -1;
            else if (m < 0 && gen_chance(12)) m = lane;
            if ((i & 15u) == 15 && m >= 0 && gen_chance(60)) m = motif[0] + (gen_chance(50) ? 1 : -1);   /* a turn at the bar's end */
        }
        if (m < 0) continue;
        if (m > CH_LANES - 1) m = CH_LANES - 1; if (m < 0) m = 0;
        unsigned vel = ((i & 3u) == 0 ? 104 : 84) + gen_rnd() % 12u;
        gen_put(t, (unsigned)m, i, vel, 1);
    }
}
/* BEAT: hits on the root lane from one of a few classic bars, lightly changed: a hit or two added or taken away,
 * the odd ratchet; accents where the template has them, the later bars varied a little more */
static void gen_beat(unsigned t)
{
    static const char *const BAR[10] = {
        "X...X...X...X...", "X.......X.......", "X..x..X...x.X...", "X..x....X..x....", "X...X..x..X.x...",
        "X......x..X.....", "x.x.x.x.x.x.x.x.", "X.xxX.xxX.xxX.xx", "x.X.x.X.x.X.x.X.", "X..X..X.X..X..X."};
    unsigned len = ch_seq_get(t)->len ? ch_seq_get(t)->len : 16;
    gen_clear(t);
    const char *b = BAR[gen_rnd() % 10u];
    for (unsigned i = 0; i < len; ++i) {
        unsigned p = i % 16u; char c = b[p];
        int on = c != '.';
        unsigned flip = i < 16 ? 7 : 14;                   /* a change here and there; more in later bars */
        if (gen_chance(flip)) on = !on;
        if (p == 0) on = 1;
        if (!on) continue;
        unsigned vel = (c == 'X' ? 112 : (p & 3u) == 0 ? 96 : 78) + gen_rnd() % 10u;
        gen_put(t, 0, i, vel > 127 ? 127 : vel, gen_chance(5) ? 2 : 1);
    }
}
/* VARY: the pattern as it is, with a few notes moved to a neighbouring lane, a few dropped, a few added */
static void gen_vary(unsigned t)
{
    const ch_seq_state *q = ch_seq_get(t);
    unsigned len = q->len ? q->len : 16;
    int melodic = ch_track_get(t)->device[CH_MATERIAL] == CH_DEV_SYNTH;
    for (unsigned i = 0; i < len; ++i) {
        int on = -1;
        for (unsigned l = 0; l < CH_LANES; ++l) if (q->step[l][i].vel) { on = (int)l; break; }
        if (on >= 0) {
            if (i != 0 && gen_chance(12)) { ch_step s = q->step[on][i]; ch_step off = {0, 1, 100, 0, 0, 0}; ch_step_set(t, (unsigned)on, i, &off); (void)s; }
            else if (melodic && gen_chance(18)) {
                int to = on + (gen_chance(50) ? 1 : -1); if (to < 0) to = 1; if (to > CH_LANES - 1) to = CH_LANES - 2;
                ch_step s = q->step[on][i]; ch_step off = {0, 1, 100, 0, 0, 0};
                ch_step_set(t, (unsigned)on, i, &off); ch_step_set(t, (unsigned)to, i, &s);
            }
        } else if (gen_chance(melodic ? 10 : 7)) {
            int nb = 0;                                    /* near the last note, or the root ("near" is Windows') */
            for (int k = (int)i - 1; k >= 0 && nb == 0; --k) for (unsigned l = 0; l < CH_LANES; ++l) if (q->step[l][k].vel) { nb = (int)l; break; }
            int to = melodic ? nb + (int)(gen_rnd() % 3u) - 1 : 0; if (to < 0) to = 0; if (to > CH_LANES - 1) to = CH_LANES - 1;
            gen_put(t, (unsigned)to, i, 76 + gen_rnd() % 16u, 1);
        }
    }
}
/* RANDOM PARAMS: the engine's sound, locked to a random value on every used step (the steps themselves untouched):
 * two of the model's four main parameters a step, within a third of the knob either side of where it sits, until the
 * track's locks are used up */
static void gen_params(unsigned t)
{
    const ch_seq_state *q = ch_seq_get(t);
    const ch_track_state *ts = ch_track_get(t);
    unsigned len = q->len ? q->len : 16, dev = ts->device[CH_MATERIAL];
    if (dev == CH_DEV_BYPASS) return;
    const ch_device_desc *d = ch_device_info(dev);
    unsigned row = ch_row(CH_MATERIAL, dev);
    static const uint8_t SYN[4] = {2, 3, 4, 12}, OTH[4] = {2, 3, 4, 5};   /* SYNTH: its three and CUTOFF; the rest: the four after PITCH */
    const uint8_t *pp = dev == CH_DEV_SYNTH ? SYN : OTH;
    ch_lock_clear(t, CH_STEPS);
    int full = 0;
    for (unsigned i = 0; i < len && !full; ++i) {
        int used = 0;
        for (unsigned l = 0; l < CH_LANES; ++l) if (q->step[l][i].vel && !q->step[l][i].tie) used = 1;
        if (!used) continue;
        unsigned a = gen_rnd() % 4u, b = (a + 1 + gen_rnd() % 3u) % 4u;
        for (unsigned k = 0; k < 2 && !full; ++k) {
            unsigned p = pp[k ? b : a];
            if (p >= CH_PARAMS || !d->param[p].name || d->param[p].kind == CH_SELECT) continue;
            float cur = ts->param[row][p], lo = d->param[p].kind == CH_BI ? -1.0f : 0.0f;
            float v = cur + ((float)(gen_rnd() % 1000u) / 1000.0f - 0.5f) * 0.66f;
            v = v < lo ? lo : v > 1 ? 1 : v;
            if (ch_lock_set(t, i, CH_MATERIAL, p, v) != 0) full = 1;
        }
    }
}
/* the LINE generator (ARP), after the acid-line generators: a press draws four related 16-step patterns for the
 * track, a note (two choices a step: a free one and an acid one), an accent layer, a gate layer and an octave, each
 * step with ranks for density, style and octave. The settings do not redraw: they choose how much of the drawn
 * material plays (DENSITY reveals steps rank by rank, STYLE moves each step from its free note to its acid note,
 * OCTAVE lets the jumps through), or scale it (ACCENT the three velocities, GATE the three lengths), so a turn
 * morphs the line and ARP again draws new material. The lanes are the track's key: lane 0 the root, 4 the fifth,
 * 7 the octave. Packed 3 bytes a step: deg_free | deg_acid << 3 | oct_acid << 6 | oct_free << 7;
 * rank | style_rank << 4; accent | gate << 2 | oct_rank << 4. */
enum { G_TYPE, G_DENSITY, G_ACCENT, G_GATE, G_OCTAVE, G_OFFSET, G_STEPS, G_SWING };
static const char *const GEN_NAME[8] = {"TYPE", "DENSITY", "ACCENT", "GATE", "OCTAVE", "OFFSET", "STEPS", "SWING"};
static const float GEN_DEF[8] = {10.0f / 15, 0.55f, 1.0f, 0.6f, 0.5f, 0, 0, 0};   /* (ACCENT at 100: accents at 127) */   /* TYPE 11, as the acid-line generators default */
/* TYPE 1..16, as Sting counts it (read from the device): the line starts as sixteen random notes, and type k puts the root in
 * k - 1 of them at random positions, so 1 is all random and 16 all root but one. Here the random note is a random lane (the
 * track's key), the root lane 0; the octave jumps are a lane of their own, let through by OCTAVE */
static unsigned gen_type(void) { return 1 + (unsigned)(ui.gen[G_TYPE] * 15.0f + 0.5f); }
enum { GL_NOTE = 1, GL_ACCENT = 2, GL_GATE = 4, GL_OCTAVE = 8 };   /* the lane locks */
static void gen_draw(void)
{
    uint8_t rank[16], srank[16], orank[16];
    /* density as Sting draws it: sixteen lists a page, list k being k of the 16 steps picked at random (urn), each list its
     * own draw, so a sparse line is not the beats and a denser one is not the sparse one plus a step */
    for (unsigned pg = 0; pg < 4; ++pg)
        for (unsigned k = 1; k <= 16; ++k) {
            uint8_t urn[16]; for (unsigned j = 0; j < 16; ++j) urn[j] = (uint8_t)j;
            uint16_t bits = 0;
            if (ui.gen_sting) {                          /* Sting: k of the 16, any of them */
                for (unsigned j = 0; j < k; ++j) { unsigned r = j + gen_rnd() % (16u - j); uint8_t t = urn[j]; urn[j] = urn[r]; urn[r] = t; bits |= (uint16_t)(1u << urn[j]); }
            } else {                                     /* GROOVE: step 1 first, then by weight: the beats, the eighths, the sixteenths, each still a chance */
                static const uint8_t W[16] = {0, 1, 3, 1, 6, 1, 3, 1, 6, 1, 3, 1, 6, 1, 3, 1};
                bits = 1;
                for (unsigned j = 1; j < k; ++j) {
                    unsigned total = 0; for (unsigned p = 1; p < 16; ++p) if (!(bits >> p & 1u)) total += W[p];
                    unsigned r = gen_rnd() % total, p = 1;
                    for (; p < 16; ++p) { if (bits >> p & 1u) continue; if (r < W[p]) break; r -= W[p]; }
                    bits |= (uint16_t)(1u << p);
                }
            }
            ui.gen_dens[pg][k - 1] = bits;
        }
    for (unsigned k = 0; k < 16; ++k) { rank[k] = (uint8_t)k; srank[k] = (uint8_t)k; orank[k] = (uint8_t)k; }
    for (unsigned k = 15; k > 0; --k) { unsigned j = gen_rnd() % (k + 1); uint8_t t = srank[k]; srank[k] = srank[j]; srank[j] = t; }
    for (unsigned k = 15; k > 0; --k) { unsigned j = gen_rnd() % (k + 1); uint8_t t = orank[k]; orank[k] = orank[j]; orank[j] = t; }
    {   /* the class order: 0 | 4 8 12 | 2 6 10 14 | the odd ones, each class shuffled */
        static const uint8_t CLASS[16] = {0, 4, 8, 12, 2, 6, 10, 14, 1, 3, 5, 7, 9, 11, 13, 15};
        uint8_t order[16]; for (unsigned k = 0; k < 16; ++k) order[k] = CLASS[k];
        for (unsigned k = 3; k > 1; --k) { unsigned j = 1 + gen_rnd() % k; uint8_t t = order[k]; order[k] = order[j]; order[j] = t; }
        for (unsigned k = 7; k > 4; --k) { unsigned j = 4 + gen_rnd() % (k - 3); uint8_t t = order[k]; order[k] = order[j]; order[j] = t; }
        for (unsigned k = 15; k > 8; --k) { unsigned j = 8 + gen_rnd() % (k - 7); uint8_t t = order[k]; order[k] = order[j]; order[j] = t; }
        for (unsigned k = 0; k < 16; ++k) rank[order[k]] = (uint8_t)k;
    }
    for (unsigned i = 0; i < CH_STEPS; ++i) {
        unsigned p = i % 16u;
        unsigned deg_free = gen_rnd() % 7u, oct_free = gen_chance(40);
        unsigned r = gen_rnd() % 100u, deg_acid, oct_acid = 0;   /* the acid note: the root most of the time, its octave, the fifth, the seventh, a neighbour */
        if (r < 48) deg_acid = 0; else if (r < 66) { deg_acid = 0; oct_acid = 1; } else if (r < 78) deg_acid = 4; else if (r < 86) deg_acid = 6;
        else deg_acid = gen_chance(50) ? 1 : 2;
        oct_acid = gen_chance(40);                          /* the jump, let through by OCTAVE */
        unsigned acc = gen_chance(33) ? 2 : 0;              /* Sting's two velocities, a third of them the accent (random 3 -> 113, else 70) */
        (void)p;
        unsigned g = gen_rnd() % 100u, gate = acc == 2 ? (g < 45 ? 2 : g < 80 ? 1 : 0) : (g < 20 ? 2 : g < 55 ? 1 : 0);
        uint8_t *gp = ui.gen_pat[i], b0 = (uint8_t)(deg_free | deg_acid << 3 | oct_acid << 6 | oct_free << 7), b1 = (uint8_t)(rank[p] | srank[p] << 4), b2 = (uint8_t)(acc | gate << 2 | orank[p] << 4);
        if (ui.gen_lock & GL_NOTE) { b0 = (uint8_t)((b0 & 0xC0) | (gp[0] & 0x3F)); b1 = gp[1]; }   /* a kept lane keeps what it had */
        if (ui.gen_lock & GL_OCTAVE) { b0 = (uint8_t)((b0 & 0x3F) | (gp[0] & 0xC0)); b2 = (uint8_t)((b2 & 0x0F) | (gp[2] & 0xF0)); }
        if (ui.gen_lock & GL_ACCENT) b2 = (uint8_t)((b2 & ~3u) | (gp[2] & 3u));
        if (ui.gen_lock & GL_GATE) b2 = (uint8_t)((b2 & ~12u) | (gp[2] & 12u));
        gp[0] = b0; gp[1] = b1; gp[2] = b2;
    }
}
/* the line as the settings make it, a step at a time: its lane, whether DENSITY lets it sound, its octave jump, its
 * gate layer. gen_apply writes it into the track; the page draws it */
static unsigned gen_plan(unsigned t, uint8_t *lane, uint8_t *on, int8_t *oc, uint8_t *gl)
{
    const ch_seq_state *q = ch_seq_get(t);
    unsigned len = q->len ? q->len : 16;
    float dens = ui.gen[G_DENSITY], octv = ui.gen[G_OCTAVE];
    unsigned off = (unsigned)(ui.gen[G_OFFSET] * (float)(len - 1) + 0.5f) % len;
    unsigned dlim = (unsigned)(dens * 16.99f), slim = gen_type() - 1, olim = (unsigned)(octv * 16.99f);
    uint8_t tdeg[4][16], toct[4][16], troot[4][16], have[4] = {0}; unsigned type = gen_type();
    unsigned count = 0;
    for (unsigned i = 0; i < len; ++i) {
        const uint8_t *g = ui.gen_pat[(i + off) % len];
        unsigned rank = g[1] & 15u, srank = g[1] >> 4, orank = g[2] >> 4;
        unsigned pos = (i + off) % len;
        on[i] = dlim ? (ui.gen_dens[(pos / 16u) & 3u][dlim - 1] >> (pos % 16u)) & 1u : 0;   /* the density list for this page, at the offset */
        (void)rank;
        unsigned pg = (pos / 16u) & 3u;
        if (!have[pg]) { gen_type_list(ui.gen_nseed, type, pg, tdeg[pg], toct[pg], troot[pg]); have[pg] = 1; }
        int acid = troot[pg][pos % 16u]; (void)srank; (void)slim;
        unsigned deg = acid ? 0 : tdeg[pg][pos % 16u], oct = toct[pg][pos % 16u];   /* the root where the type puts it, else the list's note */
        if (orank >= olim) oct = 0;
        oc[i] = (int8_t)(oct ? (orank & 1u ? -1 : 1) : 0);   /* the jump: the note an octave up or down, a PITCH lock on the step */
        {   /* onto the lanes the line may use (black keys 1-8 on the page) */
            uint8_t ok[CH_LANES]; unsigned n = 0, mask = ui.gen_lanes ? ui.gen_lanes : 0xFFu;
            for (unsigned k = 0; k < CH_LANES; ++k) if (mask >> k & 1u) ok[n++] = (uint8_t)k;
            lane[i] = ok[deg % n];
        }
        gl[i] = (uint8_t)(g[2] >> 2 & 3u);
        count += on[i];
    }
    return count;
}
/* GATE on the line page: on an engine that holds its notes (SYNTH, SAMPLE, GRAIN) it is each step's own GATE, the
 * drawn gate layer's three lengths scaled by it (at 100 every note the whole step). A DRUM has nothing to hold: there
 * it is still the drum's DECAY */
static int gen_gate_steps(unsigned t)
{
    unsigned dev = ch_track_get(t)->device[CH_MATERIAL];
    return dev == CH_DEV_SYNTH || dev == CH_DEV_POLY || dev == CH_DEV_GRAIN;
}
static unsigned gen_apply(unsigned t)
{
    const ch_seq_state *q = ch_seq_get(t);
    unsigned len = q->len ? q->len : 16;
    float acc = ui.gen[G_ACCENT];
    unsigned off = (unsigned)(ui.gen[G_OFFSET] * (float)(len - 1) + 0.5f) % len;
    uint8_t lane[CH_STEPS], on[CH_STEPS], gl[CH_STEPS], slide_same[CH_STEPS] = {0}; int8_t oc[CH_STEPS];
    unsigned count = gen_plan(t, lane, on, oc, gl);
    gen_clear(t);
    const ch_track_state *ts = ch_track_get(t); unsigned dev = ts->device[CH_MATERIAL];
    int pitched = dev == CH_DEV_SYNTH || dev == CH_DEV_DRUM || dev == CH_DEV_POLY || dev == CH_DEV_GRAIN;   /* PITCH is parameter 1 on each */
    float pitch = pitched ? ch_param_of(ts, CH_MATERIAL, dev, 1) : 0;
    for (unsigned i = 0; i < len; ++i) {
        if (!on[i]) continue;
        unsigned al = ui.gen_pat[(i + off) % len][2] & 3u;
        float vel = al == 2 ? 100 + 27 * acc : 100;       /* a note 100, an accent up to 127 (ACCENT: how far up; the plain notes went down to 40 as it rose, Sting's way) */
        if (slide_same[i]) { ch_step st = ch_seq_get(t)->step[lane[i]][i]; st.vel = (uint8_t)(vel > 127 ? 127 : vel < 20 ? 20 : vel); ch_step_set(t, lane[i], i, &st); }   /* the step before slides into this one: it stays a tie */
        else gen_put(t, lane[i], i, (unsigned)(vel > 127 ? 127 : vel < 20 ? 20 : vel), 1);
        if (dev == CH_DEV_SYNTH && (gl[i] & 3u) == 3u && on[(i + 1) % len]) {   /* a slide: the note held into the next step (ACID slides to it, the others hold) */
            unsigned j = (i + 1) % len;
            if (lane[j] == lane[i]) { ch_step st = ch_seq_get(t)->step[lane[j]][j]; if (!st.tie) { st.tie = 1; ch_step_set(t, lane[j], j, &st); } slide_same[j] = 1; }
            else { ch_step st = {(uint8_t)(vel > 127 ? 127 : vel < 20 ? 20 : vel), 1, 100, 0, 0, 0, 1}; ch_step_set(t, lane[i], j, &st); }
        }
        if (oc[i] && pitched) {                          /* the jump: the step's own octave (it was a PITCH lock, of which a track has 32) */
            ch_step st = ch_seq_get(t)->step[lane[i]][i]; ch_step_oct_set(&st, oc[i]); ch_step_set(t, lane[i], i, &st);
        }
        (void)pitch;
        if (gen_gate_steps(t) && !slide_same[i]) {          /* the step's GATE: short, middling or long by its layer, all of them by the knob; a note that slides on: the whole step */
            static const float LAYER[3] = {0.55f, 0.8f, 1.0f};
            ch_step st = ch_seq_get(t)->step[lane[i]][i];
            float g = ui.gen[G_GATE], L = (gl[i] & 3u) == 3u ? 1.0f : LAYER[gl[i] & 3u], pc = 100.0f * g * (L + (1 - L) * g);
            if ((gl[i] & 3u) == 3u && on[(i + 1) % len]) pc = 100;
            ch_step_gate_set(&st, (unsigned)(pc < 7 ? 7 : pc > 100 ? 100 : pc + 0.5f));
            ch_step_set(t, lane[i], i, &st);
        }
    }
    return count;
}
/* the engine parameter GATE drives: the decay (GRAIN: its release); -1 with no engine */
static int gen_gate_param(unsigned t)
{
    unsigned dev = ch_track_get(t)->device[CH_MATERIAL];
    return dev == CH_DEV_SYNTH ? 5 : dev == CH_DEV_DRUM ? 2 : dev == CH_DEV_POLY ? 8 : dev == CH_DEV_GRAIN ? 7 : -1;
}
/* the page's keys: white 1-4 keep or redraw a lane (NOTE ACCENT GATE OCTAVE), black 1-8 the lanes the line may use */
static void gen_key(int white, int black)
{
    static const char *const LN[4] = {"NOTE", "ACCENT", "GATE", "OCTAVE"};
    if (white >= 0 && white < 4) { ui.gen_lock ^= (uint8_t)(1u << white); app_say(LN[white], ui.gen_lock >> white & 1u ? "KEPT WHEN ARP DRAWS" : "REDRAWN"); }
    else if (white == 4) { ui.gen_sting ^= 1; app_say("DRAW", ui.gen_sting ? "STING: ANY STEP" : "GROOVE: BEATS FIRST"); }   /* (takes with the next line) */
    else if (black >= 0 && black < CH_LANES) {
        uint8_t m = (uint8_t)((ui.gen_lanes ? ui.gen_lanes : 0xFFu) ^ (1u << black));
        if (m) { ui.gen_lanes = m; if (ui.gen_seed && !ui.gen_hold) gen_apply(ui.track); }
        char v[16] = "LANE "; char n[4]; int_text(black + 1, n); text_cat(v, n); app_say(v, (ui.gen_lanes ? ui.gen_lanes : 0xFFu) >> black & 1u ? "IN" : "OUT");
    } else return;
    song_sync_lane(); ui.redraw = 1;
}
/* GATE with no line of the generator's on the track (the page just opened, nothing drawn yet): the notes the track
 * has, each as long as the knob says. A note the next step ties on from stays whole, or the tie would have nothing to hold */
static void gen_gate_all(unsigned t)
{
    unsigned len = ch_seq_get(t)->len ? ch_seq_get(t)->len : 16, pc = (unsigned)(ui.gen[G_GATE] * 100 + 0.5f);
    for (unsigned l = 0; l < CH_LANES; ++l) for (unsigned i = 0; i < len; ++i) {
        ch_step st = ch_seq_get(t)->step[l][i];
        if (!st.vel || ch_seq_get(t)->step[l][(i + 1) % len].tie) continue;
        ch_step_gate_set(&st, pc < 7 ? 7 : pc); ch_step_set(t, l, i, &st);
    }
}
static void gen_open(void);
static void gen_again_here(void)                       /* HOME + ARP in TRACKS, SEQ + ARP in the step view: a new line at the generator's values, the view kept */
{
    if (!ui.gen_seed) for (unsigned k = 0; k < 8; ++k) ui.gen[k] = GEN_DEF[k];
    ui.gen_hold = 0;
    ch_undo_mark(); ui.undo_fwd = 0;
    gen_stir(); ui.gen_seed = gen_rnd();
    if (!(ui.gen_lock & GL_NOTE) || !ui.gen_nseed) ui.gen_nseed = gen_rnd();
    gen_draw();
    unsigned n = gen_apply(ui.track); song_sync_lane(); ui.redraw = 1;
    char v[24] = ""; int_text((int32_t)n, v); text_cat(v, " NOTES");
    app_say("NEW LINE", v);
}
static void gen_open(void)
{
    if (ui.view != V_GEN) { ui.view = V_GEN; ui.mfx = 0; }
    if (!ui.gen_seed) for (unsigned k = 0; k < 8; ++k) ui.gen[k] = GEN_DEF[k];
    ui.gen_hold = 0;
    ch_undo_mark(); ui.undo_fwd = 0;
    gen_stir(); ui.gen_seed = gen_rnd();
    if (!(ui.gen_lock & GL_NOTE) || !ui.gen_nseed) ui.gen_nseed = gen_rnd();
    gen_draw();
    unsigned n = gen_apply(ui.track); song_sync_lane(); ui.redraw = 1;
    char v[24] = ""; int_text((int32_t)n, v); text_cat(v, " NOTES. ARP: NEW");
    app_say("LINE", v);
}
static void edit_gen(unsigned i, int32_t det)
{
    unsigned k = ui.gen_page * 4 + i;
    char v[12] = "";
    if (k == G_STEPS) {                                  /* the track's length, as SEQ + K1 */
        int32_t l = clampi((int32_t)ch_seq_get(ui.track)->len + det, 1, CH_STEPS);
        ch_seq_len_set(ui.track, (unsigned)l); int_text(l, v); app_say("STEPS", v);
    } else if (k == G_SWING) {
        float sw = fm_clampf(ch_swing() + (float)det * enc_s, 0, 1); ch_swing_set(sw);
        int_text((int32_t)(50 + sw * 25 + 0.5f), v); text_cat(v, "%"); app_say("SWING", v);
        ui.redraw = 1; return;
    } else if (k == G_GATE && !gen_gate_steps(ui.track)) {   /* a DRUM: the note length is its decay, nothing in the pattern changes (the others: the steps' own GATE, below) */
        int p = gen_gate_param(ui.track);
        if (p < 0) { app_say("GATE", "NO ENGINE"); return; }
        const ch_track_state *ts = ch_track_get(ui.track);
        float d = fm_clampf(ts->param[ch_row(CH_MATERIAL, ts->device[CH_MATERIAL])][p] + (float)det * enc_s, 0, 1);
        ch_param_set(ui.track, CH_MATERIAL, (unsigned)p, d);
        int_text((int32_t)(d * 100 + 0.5f), v); app_say("GATE", v); ui.redraw = 1; return;
    } else if (k == G_OFFSET) {                          /* a step a detent, round either way: the whole line moved */
        int len = ch_seq_get(ui.track)->len ? ch_seq_get(ui.track)->len : 16;
        int st = (int)(ui.gen[k] * (float)(len - 1) + 0.5f);
        st = ((st + (det > 0 ? 1 : -1)) % len + len) % len;
        ui.gen[k] = (float)st / (float)(len - 1);
        if (st > len / 2) { v[0] = '-'; int_text(len - st, v + 1); } else int_text(st, v);
        app_say("OFFSET", v);
    } else if (k == G_TYPE) {                            /* a type a detent, 1..16 */
        int t = clampi((int)gen_type() + (det > 0 ? 1 : -1), 1, 16);
        ui.gen[k] = (float)(t - 1) / 15.0f; int_text(t, v); app_say("TYPE", v);
    } else {
        ui.gen[k] = fm_clampf(ui.gen[k] + (float)det * enc_s, 0, 1);
        int_text((int32_t)(ui.gen[k] * 100 + 0.5f), v); app_say(GEN_NAME[k], v);
    }
    if (ui.gen_seed && !ui.gen_hold) { gen_apply(ui.track); song_sync_lane(); }   /* (held: the track waits for ARP) */
    else if (k == G_GATE) gen_gate_all(ui.track);        /* (but GATE: the notes already there take it) */
    ui.redraw = 1;
}
/* EDIT + ARP: the page, to set the line up; nothing is drawn into the track until ARP alone */
static void gen_open_held(void)
{
    ui.view = V_GEN; ui.mfx = 0; ui.gen_hold = 1; ui.redraw = 1;
    ch_undo_mark(); ui.undo_fwd = 0;                     /* (GATE may change the notes that are there: EDIT + REC puts them back) */
    if (!ui.gen_seed) for (unsigned k = 0; k < 8; ++k) ui.gen[k] = GEN_DEF[k];
    app_say("LINE", "ARP AGAIN: DRAW");
}
/* the generator's tile text and bar for setting k */
static float gen_tile(unsigned k, char *val)
{
    val[0] = 0;
    if (k == G_STEPS) { unsigned l = ch_seq_get(ui.track)->len; int_text((int32_t)l, val); return (float)l / (float)CH_STEPS; }
    if (k == G_SWING) { int_text((int32_t)(50 + ch_swing() * 25 + 0.5f), val); text_cat(val, "%"); return ch_swing(); }
    if (k == G_GATE && !gen_gate_steps(ui.track)) {
        int p = gen_gate_param(ui.track); if (p < 0) { text_cat(val, "-"); return 0; }
        const ch_track_state *ts = ch_track_get(ui.track); float d = ts->param[ch_row(CH_MATERIAL, ts->device[CH_MATERIAL])][p];
        int_text((int32_t)(d * 100 + 0.5f), val); return d;
    }
    if (k == G_TYPE) { int_text((int32_t)gen_type(), val); return ui.gen[k]; }
    if (k == G_OFFSET) {
        int len = ch_seq_get(ui.track)->len ? ch_seq_get(ui.track)->len : 16, st = (int)(ui.gen[k] * (float)(len - 1) + 0.5f);
        if (st > len / 2) { val[0] = '-'; int_text(len - st, val + 1); } else int_text(st, val);
        return ui.gen[k];
    }
    int_text((int32_t)(ui.gen[k] * 100 + 0.5f), val); return ui.gen[k];
}
/* RANDOM LANES: every used step moved to a random lane (its other settings kept); a step with two notes keeps both apart */
static void gen_lanes(unsigned t)
{
    const ch_seq_state *q = ch_seq_get(t);
    unsigned len = q->len ? q->len : 16;
    ch_step off = {0, 1, 100, 0, 0, 0};
    for (unsigned i = 0; i < len; ++i) {
        ch_step held[CH_LANES]; unsigned n = 0;
        for (unsigned l = 0; l < CH_LANES; ++l) if (q->step[l][i].vel) { held[n++] = q->step[l][i]; ch_step_set(t, l, i, &off); }
        if (!n) continue;
        unsigned used = 0;
        for (unsigned k = 0; k < n; ++k) {
            unsigned to = gen_rnd() % CH_LANES;
            while (used >> to & 1u) to = (to + 1) % CH_LANES;
            used |= 1u << to;
            ch_step_set(t, to, i, &held[k]);
        }
    }
}
static void tie_to_lane(unsigned t, unsigned lane, unsigned from, unsigned to)
{
    const ch_seq_state *q = ch_seq_get(t);
    ch_step head = q->step[lane][from];
    for (unsigned i = from + 1; i <= to && i < CH_STEPS; ++i) {
        ch_step s = head; s.ratchet = 1; s.chance = 100; s.tie = 1; s.micro = 0;
        ch_step_set(t, lane, i, &s);
    }
}
static void generate(unsigned which)
{
    unsigned t = ui.track;
    ch_undo_mark(); ui.undo_fwd = 0;
    gen_stir();
    if (which == 0) { gen_melody(t); app_say("NEW MELODY", "IN KEY"); }
    else if (which == 1) { gen_beat(t); app_say("NEW BEAT", ""); }
    else if (which == 2) { gen_params(t); app_say("RANDOM", "STEP PARAMETERS"); }
    else if (which == 3) { gen_lanes(t); app_say("RANDOM", "LANES"); }
    else { gen_vary(t); app_say("VARIED", ""); }
    song_sync_lane(); ui.redraw = 1;
}

/* ---- AUTOPILOT's work ---- */
static void ap_params_back(void)                       /* every parameter it moved, where it was */
{
    for (unsigned k = 0; k < ap.n; ++k) ch_param_set(ap.ch[k].t, ap.ch[k].slot, ap.ch[k].p, ap.ch[k].orig);
    ap.n = 0;
}
static int ap_name_is(const char *n, const char *w) { while (*w) if (*n++ != *w++) return 0; return 1; }   /* (n starts with w) */
/* one track's variation: of its engine's and effects' parameters, VARIETY's share (one at least) moved by up to half
 * VARIETY of their range. Left alone: a melodic engine's tuning (it stays in key), what would stop or re-route the
 * sound (HOLD, the sources, VOICES, MODE, a gate's steps). Levels and mixes move a third as far and never under 0.6 of
 * where they were. The stepped choices (a MODEL, a shape, a division) change only from VARIETY 60 % */
static void ap_vary_track(unsigned t)
{
    const ch_track_state *ts = ch_track_get(t);
    if (ts->mute) return;
    for (unsigned slot = 0; slot < CH_SLOTS; ++slot) {
        unsigned dev = ts->device[slot];
        const ch_device_desc *dd = ch_device_info(dev);
        if (dev == CH_DEV_BYPASS || !dd || !dd->params || ts->bypass[slot]) continue;
        int melodic = slot == CH_MATERIAL && (dev == CH_DEV_SYNTH || dev == CH_DEV_POLY || dev == CH_DEV_GRAIN);
        unsigned want = 1u + (unsigned)(ap.variety * (float)dd->params * 0.5f), tries = want * 4u;
        while (want && tries-- && ap.n < AP_MAX) {
            unsigned p = gen_rnd() % dd->params;
            const ch_param_desc *pd = &dd->param[p];
            const char *nm = pd->name;
            if (!nm || nm[0] == '-') continue;
            if (ap_name_is(nm, "HOLD") || ap_name_is(nm, "SOURCE") || ap_name_is(nm, "INPUT") || ap_name_is(nm, "FOLLOW") || ap_name_is(nm, "VOICES")
                || ap_name_is(nm, "MODE") || ap_name_is(nm, "STEPS") || ap_name_is(nm, "SAMPLE") || ap_name_is(nm, "LOOP") || ap_name_is(nm, "LENGTH")) continue;
            if (melodic && (ap_name_is(nm, "PITCH") || ap_name_is(nm, "A ") || ap_name_is(nm, "B "))) continue;
            int dup = 0; for (unsigned k = 0; k < ap.n; ++k) dup |= ap.ch[k].t == t && ap.ch[k].slot == slot && ap.ch[k].p == p;
            if (dup) continue;
            float cur = ch_param_of(ts, slot, dev, p), nv;
            if (pd->kind == CH_SELECT) {
                if (ap.variety < 0.6f || pd->steps < 2) continue;
                nv = (float)(gen_rnd() % pd->steps);
                if ((int)(nv + 0.5f) == (int)(cur + 0.5f)) continue;
            } else {
                float lo = pd->kind == CH_BI ? -1.0f : 0.0f, span = 1 - lo;
                float d = ((float)(gen_rnd() % 2001u) * 0.001f - 1.0f) * ap.variety * 0.5f * span;
                int level = ap_name_is(nm, "LEVEL") || ap_name_is(nm, "MIX") || ap_name_is(nm, "DEPTH");
                if (level || ap_name_is(nm, "PITCH")) d *= 0.33f;
                nv = fm_clampf(cur + d, lo, 1);
                if (level && nv < cur * 0.6f) nv = cur * 0.6f;
                if (nv == cur) continue;
            }
            ap.ch[ap.n].t = (uint8_t)t; ap.ch[ap.n].slot = (uint8_t)slot; ap.ch[ap.n].p = (uint8_t)p; ap.ch[ap.n].orig = cur;
            if (ap.revert) ++ap.n;                           /* (OVERRIDE: nothing to put back) */
            ch_param_set(t, slot, p, nv);
            --want;
        }
    }
}
/* now and then, from VARIETY 50 %, a track's line anew: a melodic track from the line generator (ARP's, at its
 * settings), a drum track varied (from 85 %, sometimes a whole new beat) */
static void ap_line(void)
{
    if (ap.variety < 0.5f || gen_rnd() % 100u >= (unsigned)((ap.variety - 0.5f) * 200.0f)) return;
    unsigned t = gen_rnd() % CH_TRACKS, dev = 0;
    for (unsigned k = 0; k < CH_TRACKS; ++k, t = (t + 1u) % CH_TRACKS) {
        dev = ch_track_get(t)->device[CH_MATERIAL];
        int any = 0;                                        /* (only a track that has a line: an empty one stays empty) */
        { const ch_seq_state *q = ch_seq_get(t); for (unsigned l = 0; l < CH_LANES && !any; ++l) for (unsigned i = 0; i < q->len && i < CH_STEPS; ++i) if (q->step[l][i].vel) { any = 1; break; } }
        if (any && !ch_track_get(t)->mute && dev != CH_DEV_BYPASS) break;
        dev = 0;
    }
    if (!dev) return;
    if (dev == CH_DEV_SYNTH || dev == CH_DEV_POLY || dev == CH_DEV_GRAIN) {
        uint8_t was = ui.track; ui.track = (uint8_t)t;
        if (!ui.gen_seed) for (unsigned k = 0; k < 8; ++k) ui.gen[k] = GEN_DEF[k];
        ui.gen_hold = 0; ui.gen_seed = gen_rnd();
        if (!(ui.gen_lock & GL_NOTE) || !ui.gen_nseed) ui.gen_nseed = gen_rnd();
        gen_draw(); gen_apply(t);
        ui.track = was;
    } else if (ap.variety > 0.85f && gen_rnd() % 100u < 40u) gen_beat(t);
    else gen_vary(t);
    ap.pat_changed = 1; song_sync_lane(); ui.redraw = 1;
}
static void ap_set(int on)
{
    on = on != 0;
    if (on == ap.on) return;
    if (on) { gen_stir(); ap.n = 0; ap.count = 0; ap.last_bar = -1; ap.phase = 0; ap.pat_changed = 0; ap.pat = ui.cur_pat; ch_keep_mark(); }
    else if (ap.revert) { ap_params_back(); if (ap.pat_changed) { ch_keep_back(); song_sync_lane(); } ap.pat_changed = 0; }
    else ap.n = 0;
    ap.on = (uint8_t)on; ui.redraw = 1;
}
static void ap_leave_pattern(void)
{
    if (ap.on && ap.revert && ap.pat_changed) { ch_keep_back(); ap.pat_changed = 0; }
}
static void ap_before_save(void)                       /* REVERT: what is saved is yours, not the variation playing */
{
    if (!ap.on || !ap.revert) return;
    ap_params_back(); ap_leave_pattern(); ap.count = 0;
}
/* each pass of the main loop: a bar line crossed is counted; at the count, the variation, a track a pass (the
 * commands spread out) */
static void ap_tick(void)
{
    if (!ap.on) return;
    const ch_snapshot *sn = ch_snapshot_get();
    if (!sn->playing) { ap.last_bar = -1; ap.phase = 0; return; }
    if (sn->pattern_pending || pj_loading()) return;
    if (ap.pat != ui.cur_pat) { ap.pat = ui.cur_pat; ap.pat_changed = 0; ch_keep_mark(); }   /* another pattern: that one is what REVERT goes back to */
    if (ap.phase) {
        if (ap.phase <= CH_TRACKS) ap_vary_track(ap.phase - 1u);
        else { ap_line(); ap.phase = 0; ui.redraw = 1; return; }
        ++ap.phase;
        return;
    }
    int32_t bar = (int32_t)(sn->beat * 0.25f);
    if (ap.last_bar < 0) { ap.last_bar = bar; return; }
    if (bar == ap.last_bar) return;
    ap.last_bar = bar;
    if (++ap.count < (int32_t)AP_BARS[ap.bars_i]) return;
    ap.count = 0;
    if (ap.revert) { ap_params_back(); if (ap.pat_changed) { ch_keep_back(); ap.pat_changed = 0; } }   /* each variation from how it was */
    else ap.n = 0;
    gen_stir();
    ap.phase = 1;
}

/* the track's lanes (its note range) an octave down (d -12) or up (+12): black keys 9/10, or SEQ held + OCT- / OCT+ */
static void lanes_shift(int d)
{
    ch_undo_mark(); ui.undo_fwd = 0;
    const ch_seq_state *sq = ch_seq_get(ui.track);
    int lo = 127, hi = 0;
    for (unsigned l = 0; l < CH_LANES; ++l) { if (sq->note[l] < lo) lo = sq->note[l]; if (sq->note[l] > hi) hi = sq->note[l]; }
    if (lo + d < 12 || hi + d > 115) { app_say(d < 0 ? "OCTAVE DOWN" : "OCTAVE UP", "AT THE END"); return; }
    for (unsigned l = 0; l < CH_LANES; ++l) ch_lane_note_set(ui.track, l, (unsigned)(sq->note[l] + d));
    ui.root[ui.track] = (uint8_t)(ui.root[ui.track] + d);
    char v[8]; note_text(sq->note[0], v); app_say(d < 0 ? "OCTAVE DOWN" : "OCTAVE UP", v);
    ui.redraw = 1;
}

/* EDIT + PRESET: every lane a semitone up or down; in a key, to its next scale note (each lane keeps its own place) */
static void lanes_step(int dir)
{
    const ch_seq_state *sq = ch_seq_get(ui.track);
    int k = keyed(ui.track) != 0;
    unsigned nn[CH_LANES];
    for (unsigned l = 0; l < CH_LANES; ++l) {
        int n = k ? lane_note_step(ui.track, sq->note[l], dir) : sq->note[l] + dir;
        if (n < 12 || n > 115 || n == (int)sq->note[l]) { app_say(dir < 0 ? "STEP DOWN" : "STEP UP", "AT THE END"); return; }
        nn[l] = (unsigned)n;
    }
    for (unsigned l = 0; l < CH_LANES; ++l) ch_lane_note_set(ui.track, l, nn[l]);
    ui.root[ui.track] = (uint8_t)clampi(k ? lane_note_step(ui.track, ui.root[ui.track], dir) : ui.root[ui.track] + dir, 12, 115);
    char v[8]; note_text(sq->note[0], v); app_say(k ? (dir < 0 ? "SCALE STEP DOWN" : "SCALE STEP UP") : (dir < 0 ? "SEMITONE DOWN" : "SEMITONE UP"), v);
    ui.redraw = 1;
}
/* EDIT + ALGO: the track's own key (OFF: the master key, if any); EDIT + SELECT: its scale */
static void track_key_turn(int32_t det)
{
    ui.tkey[ui.track] = (uint8_t)((ui.tkey[ui.track] + (det > 0 ? 1u : 12u)) % 13u);
    lanes_layout(ui.track);
    app_say("TRACK KEY", ui.tkey[ui.track] ? KEY_NAME[ui.tkey[ui.track]] : ui.key_root ? "OFF: MASTER KEY" : "OFF");
    ui.redraw = 1;
}
static void track_scale_turn(int32_t det)
{
    ui.scale[ui.track] = (uint8_t)((ui.scale[ui.track] + (det > 0 ? 1 : SCALES - 1)) % SCALES);
    lanes_layout(ui.track);
    app_say("TRACK SCALE", ui.key_root && !ui.tkey[ui.track] ? "(MASTER KEY RULES)" : SCALE_NAME[ui.scale[ui.track]]);
    ui.redraw = 1;
}

/* the black keys in the STEP view: 1-8 the lanes, 9/10 the chosen step on/off */
static void lane_key(int b, int down)
{
    if (!down) {
        if (ui.audition[b]) { ch_note(ui.track, ui.audition[b], 0); ui.audition[b] = 0; }
        if (ui.lane_held == b) ui.lane_held = -1;
        return;
    }
    if (b < CH_LANES) {
        ui.sel_lane = (uint8_t)b; ui.lane_held = (int8_t)b;
        if (ui.held_step >= 0) {                         /* a chord: this lane on the held step too */
            cell_toggle((unsigned)b, (unsigned)ui.held_step);
            ui.step_edited = 1;
        } else if (ui.live_rec) {                        /* REC on: this lane's step, toggled, and heard */
            const ch_snapshot *sn = ch_snapshot_get();
            int st = sn->playing && sn->step[ui.track] >= 0 ? live_step(ui.track) : ui.sel_step;   /* playing: the nearest step */
            unsigned n = ch_seq_get(ui.track)->note[b];
            if (st >= 0) {
                cell_toggle((unsigned)b, (unsigned)st);
                int now_on = ch_seq_get(ui.track)->step[b][st].vel != 0;
                if (now_on) { ch_note(ui.track, n, 100); ui.audition[b] = (uint8_t)n; }
                char l[16] = "STEP "; char d[4]; int_text(st + 1, d); text_cat(l, d);
                app_say(l, now_on ? "ON" : "OFF");
            }
        } else {
            unsigned n = ch_seq_get(ui.track)->note[b];
            ch_note(ui.track, n, 100); ui.audition[b] = (uint8_t)n;
            char l[12] = "LANE "; char d[4], v[8]; int_text(b + 1, d); text_cat(l, d); note_text(n, v);
            app_say(l, v);
        }
    } else if (b < 10) {                                 /* the chosen step on / off */
        if (ui.sel_step >= 0) {
            cell_toggle(ui.sel_lane, (unsigned)ui.sel_step);
            if (ui.held_step >= 0) ui.step_edited = 1;
            int on = ch_seq_get(ui.track)->step[ui.sel_lane][ui.sel_step].vel != 0;
            char l[16] = "STEP "; char d[4]; int_text(ui.sel_step + 1, d); text_cat(l, d);
            app_say(l, on ? "ON" : "OFF");
        }
    }                                                    /* (11: nothing; the mute is ENV held + a track key) */
    ui.redraw = 1;
}

#include "library.h"
#include "browser.h"
/* the mixer's keys: white 1-4 a track's mute, black 1-4 its solo (the MIX pages, the MIXER footer, and with ENV held from any view) */
static void mix_keys(int white, int black)
{
    char v[16] = "T";
    if (white >= 0) {
        unsigned m = !ch_track_get((unsigned)white)->mute;
        for (unsigned t = 0; t < CH_TRACKS; ++t)   /* another mute key held: this track goes the way that one went */
            if ((int)t != white && (ui.keys >> WK[t] & 1u)) { m = ch_track_get(t)->mute; break; }
        ui.solo_mask = 0;
        ch_mix_set((unsigned)white, CH_MIX_MUTE, m ? 1.0f : 0.0f);
        { char n[4]; int_text(white + 1, n); text_cat(v, n); } text_cat(v, m ? " MUTED" : " ON");
        app_say("MUTE", v);
    } else {                                  /* solo: the others muted; again: all on (it is kept as the mutes are, nothing more) */
        unsigned held = 0;                    /* the other solo keys held: this track joins them (or, one of them already, leaves) */
        for (unsigned t = 0; t < CH_TRACKS; ++t) if ((int)t != black && (ui.keys >> BK[t] & 1u)) held |= 1u << t;
        if (held) {
            unsigned grp = (ui.solo_mask ? ui.solo_mask : 0u) | held;
            grp ^= 1u << black; grp |= held;
            for (unsigned t = 0; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, grp >> t & 1u ? 0.0f : 1.0f);
            ui.solo_mask = (uint8_t)grp;
            v[0] = 0;
            for (unsigned t = 0; t < CH_TRACKS; ++t) if (grp >> t & 1u) { char n[4] = {'T', (char)('1' + t), ' ', 0}; text_cat(v, n); }
            app_say("SOLO", v);
        } else {
            int was = ui.solo_mask ? (ui.solo_mask >> black & 1u) : solo_track() == black;
            for (unsigned t = 0; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, !was && (int)t != black ? 1.0f : 0.0f);
            ui.solo_mask = was ? 0 : (uint8_t)(1u << black);
            if (was) app_say("SOLO", "OFF");
            else { char n[4]; int_text(black + 1, n); text_cat(v, n); app_say("SOLO", v); }
        }
    }
}
/* the logo over: the first time this has ever run, a card saying where HELP is */
static void splash_over(uint32_t now)
{
    if (ui.help_told) return;
    ui.help_told = 1; ui.hint_until = 1u;
    settings_save();
}
static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), now = plat_ms();
    uint32_t pressed = btn & ~ui.btn, released = ui.btn & ~btn;
    ui.btn = btn;
    midi_poll();
    sync_follow();
    {   /* the tracks' hits, for the lights (every pass of the main loop) and the screen */
        const ch_snapshot *sn = ch_snapshot_get();
        for (unsigned t = 0; t < CH_TRACKS; ++t)
            if (sn->hits[t] != ui.hits_seen[t]) { ui.hits_seen[t] = sn->hits[t]; ui.hit_until[t] = now + 80u; ui.redraw = 1; }
        if (sn->count != ui.count_seen) {                /* the count-in: its beat on the top line */
            ui.count_seen = sn->count;
            if (sn->count) { char v[4] = {(char)('0' + sn->count), 0}; app_say("COUNT IN", v); } else { ui.msg_until = 0; ui.redraw = 1; }
        }
    }
    if (ui.splash) {                                     /* any touch skips the splash */
        if (pressed || (keys & ~ui.keys)) {           /* the touch that ends it is spent on that: no tap, no long hold from it */
            ui.splash = 0; ui.redraw = 1; logo_sound_stop(); lcd_fill(0, 0, 240, 240, C_BLACK);
            for (unsigned k = 0; k < NB; ++k) if (btn >> k & 1u) { ui.down_at[k] = now; ui.held_long[k] = 1; }
            splash_over(now);
        }
        ui.keys = keys;
        plat_enc(EN_SELECT);
        for (unsigned i = 0; i < 6; ++i) plat_enc(ENC_OF[i]);
        return;
    }
    if (ui.hint_until) {                                 /* the first power-on's card: any touch ends it, and is spent on that */
        if (pressed || (keys & ~ui.keys)) {
            ui.hint_until = 0; ui.redraw = 1; lcd_fill(0, 0, 240, 240, C_BLACK);
            for (unsigned k = 0; k < NB; ++k) if (btn >> k & 1u) { ui.down_at[k] = now; ui.held_long[k] = 1; }
            ui.glo_used = 1; ui.save_used = 1; ui.home_used = 1;
        }
        ui.keys = keys;
        plat_enc(EN_SELECT);
        for (unsigned i = 0; i < 6; ++i) plat_enc(ENC_OF[i]);
        return;
    }
    for (unsigned k = 0; k < NB; ++k) if (pressed >> k & 1u) { ui.down_at[k] = now; ui.held_long[k] = 0; }
    if (!lib.open && (((pressed >> B_SAVE & 1u) && (btn >> B_EDIT & 1u)) || ((pressed >> B_EDIT & 1u) && (btn >> B_SAVE & 1u)))) {   /* SAVE + EDIT: the LIBRARY */
        lib_open(); pressed &= ~((1u << B_SAVE) | (1u << B_EDIT));
    }
    if (!lib.open && !brw.open && (((pressed >> B_SAVE & 1u) && (btn >> B_GLO & 1u)) || ((pressed >> B_GLO & 1u) && (btn >> B_SAVE & 1u)))) {
        /* GLO + SAVE, either first: HELP; again: back */
        if (ui.view == V_HELP) ui.view = ui.help_back; else { ui.help_back = ui.view; ui.view = V_HELP; ui.help_sel = 0; }
        for (unsigned k = 0; k < NB; ++k) if (btn >> k & 1u) ui.held_long[k] = 1;   /* (what is held has done its work) */
        ui.glo_used = 1; ui.save_used = 1; ui.mfx = 0; ui.keys = keys; ui.redraw = 1;
        lcd_fill(0, 40, 240, 200, C_BLACK);
        return;
    }
    if (ui.view == V_HELP) {                             /* HELP: a control touched is told of, and does nothing */
        unsigned was = ui.help_sel;
        for (unsigned k = 0; k < NB; ++k) if (pressed >> k & 1u) { ui.help_sel = (uint8_t)(1u + k); ui.held_long[k] = 1; }
        for (unsigned e = 0; e < NE; ++e) if (plat_enc((int)e)) ui.help_sel = (uint8_t)(15u + e);
        { uint32_t kp = keys & ~ui.keys; if (kp) ui.help_sel = (kp & 0x2B5A52Au) ? 23 : 22; }   /* (the black keys' bits) */
        ui.glo_used = 1; ui.save_used = 1; ui.home_used = 1; ui.keys = keys;
        if (ui.help_sel != was) ui.redraw = 1;
        return;
    }
    if (lib.open) { lib_input(pressed, btn, keys, now); return; }   /* (library.h: every key and knob its own while open) */
    if (brw.open) { brw_input(pressed, btn, keys, now); return; }   /* (browser.h: the same) */
    if (pressed >> B_SEQ & 1u) ui.seq_used = 0;
    /* EDIT: the engine (tap: open, again: its next page; held: off/on). FX: the FX slots one after another (held:
     * the slot's bypass). (ARP: nothing here; ENV: with the keys, the mutes) */
    if ((pressed >> B_FX & 1u) && (btn >> B_GLO & 1u)) {      /* GLO + FX: the master chain's view */
        ui.glo_used = 1; ui.held_long[B_FX] = 1;                /* (this FX press: nothing more) */
        if (!(ui.view == V_DEVICE && ui.mfx)) { ui.mfx = 1; fx_open(0); }
        else fx_open(ui.fxpos + 1u);
    }
    /* ARP: a tap opens the line generator and touches nothing; on that page a tap draws a new line. Held (half a second)
     * it draws one into the chosen track from whatever view this is. So a line is never drawn by a slip of the finger */
    if ((pressed >> B_ARP & 1u) && (btn >> B_EDIT & 1u)) { ui.held_long[B_EDIT] = 1; ui.held_long[B_ARP] = 1; gen_open_held(); }   /* EDIT + ARP: the page (as the tap) */
    else if ((pressed >> B_ARP & 1u) && ui.view == V_SONG && (btn >> B_HOME & 1u)) { ui.home_used = 1; ui.held_long[B_ARP] = 1; gen_again_here(); }   /* HOME + ARP in TRACKS: a new line at once, staying here */
    else if ((pressed >> B_ARP & 1u) && ui.view == V_STEP && (btn >> B_SEQ & 1u)) { ui.seq_used = 1; ui.held_long[B_ARP] = 1; gen_again_here(); }     /* SEQ + ARP in the step view: the same */
    if ((btn >> B_ARP & 1u) && !ui.held_long[B_ARP] && now - ui.down_at[B_ARP] > 500u) {
        ui.held_long[B_ARP] = 1;
        if (ui.view == V_GEN) gen_open(); else gen_again_here();
    }
    if ((released >> B_ARP & 1u) && !ui.held_long[B_ARP]) {
        if (ui.view == V_GEN) gen_open();              /* on its page a tap draws too: there it is meant */
        else gen_open_held();
    }
    {   /* a TRANCE effect's page: the white keys set its steps, from the moment it is opened; SEQ + FX: the keys play notes (again: the steps). Leaving the page ends it */
        int tg = ui.view == V_DEVICE && ui.slot >= CH_FX1 && ch_track_get(ET)->device[ui.slot] == CH_DEV_TRANCE;
        if (ui.gate_keys && !tg) { ui.gate_keys = 0; ui.redraw = 1; }
        {   /* opening a TRANCE effect's page puts its steps on the white keys at once (SEQ + FX: the keys play notes again, for this visit) */
            static uint8_t tg_was;
            if (tg && !tg_was) { ui.gate_keys = 1; ui.redraw = 1; app_say("GATE STEPS", "WHITE KEYS: STEPS ON / OFF"); }
            tg_was = (uint8_t)tg;
        }
        if ((pressed >> B_FX & 1u) && (btn >> B_SEQ & 1u)) {
            ui.held_long[B_FX] = 1; ui.seq_used = 1; ui.redraw = 1;
            if (!tg) app_say("GATE STEPS", "OPEN A TRANCE EFFECT FIRST");
            else {
                ui.gate_keys = !ui.gate_keys;
                if (ui.gate_keys) ui.page[ui.slot] = 1;        /* (its page of steps) */
                app_say("GATE STEPS", ui.gate_keys ? "WHITE KEYS: STEPS ON / OFF" : "OFF: KEYS PLAY NOTES");
            }
        }
    }
    if ((pressed >> B_FX & 1u) && (btn >> B_HOME & 1u)) {     /* HOME + FX: PERFORM */
        ui.held_long[B_FX] = 1; ui.home_used = 1; ui.view = V_PERFORM; ui.mfx = 0; ui.redraw = 1; app_say("PERFORM", "MASTER AMOUNTS");
    }
    if (lane_heard && (int32_t)(now - lane_heard_until) > 0) { ch_note(lane_heard_t, lane_heard, 0); lane_heard = 0; }
    if (ui.view != V_STEP) ui.lane_edit = 0;              /* (the mode is the step view's: leaving it ends it) */
    if ((pressed >> B_EDIT & 1u) && (btn >> B_SEQ & 1u) && ui.view == V_STEP) {   /* SEQ + EDIT in the step view: LANE NOTES, each lane's note (SELECT the lane, K1 the note); again, or a SEQ tap: done */
        ui.seq_used = 1; ui.held_long[B_EDIT] = 1; ui.redraw = 1;
        if (ch_track_get(ui.track)->device[CH_MATERIAL] == CH_DEV_KIT) app_say("LANE NOTES", "A KIT: ITS LANES ARE DRUMS");
        else { ui.lane_edit ^= 1u; app_say("LANE NOTES", ui.lane_edit ? "SELECT: LANE  K1: NOTE" : "DONE"); }
    }
    {
        const ch_track_state *ts = ch_track_get(ET);
        unsigned fxs = ui.view == V_DEVICE && ui.slot >= CH_FX1 ? ui.slot : CH_FX1 + ui.fxpos;
        static const uint8_t SB[2] = {B_EDIT, B_FX};
        for (unsigned k = 0; k < 2; ++k) {
            unsigned b = SB[k], target = k ? fxs : CH_MATERIAL;
            if (!k && ui.mfx) { ts = ch_track_get(ui.track); }   /* EDIT: the track's engine, leaving the master */
            if ((btn >> b & 1u) && !ui.held_long[b] && now - ui.down_at[b] > 500u && !(k && mixpage_on())) {   /* (the MIXER page: FX held is K1-K4 on the master effects) */
                ui.held_long[b] = 1;
                unsigned by = !ts->bypass[target];
                ch_bypass_set(k ? ET : ui.track, target, by);
                app_say(k ? (ui.mfx ? MSLOT_NAME[target] : SLOT_NAME[target]) : SLOT_NAME[0], by ? "BYPASS" : "ON");
                ui.redraw = 1;
            }
            if ((released >> b & 1u) && !ui.held_long[b]) {
                if (k) {
                    if (mixpage_on()) { ui.mixfoot = 0; ui.mfx = 0; fx_open(ui.fxpos); }   /* an FX tap on the MIXER page: out to the effect's pages */
                    else if (ui.view == V_DEVICE && ui.slot >= CH_FX1) fx_open(ui.fxpos + 1u);   /* the next slot */
                    else { ui.mfx = 0; fx_open(ui.fxpos); }
                } else if (ui.view == V_DEVICE && !ui.mfx && ui.slot == CH_MATERIAL) {   /* EDIT again: the layout, round (SELECT turns the pages) */
                    unsigned lay = ui.panel ? 2u : ui.mixfoot ? 1u : 0u;
                    lay = (lay + 1u) % 3u;
                    ui.mixfoot = lay == 1; ui.panel = lay == 2;
                    app_say("LAYOUT", lay == 0 ? "PAGES + TRACKS" : lay == 1 ? "MIXER" : "PANEL");
                }
                else { ui.mfx = 0; ui.view = V_DEVICE; ui.slot = CH_MATERIAL; ui.page[0] = 0; }
                ui.redraw = 1;
            }
        }
    }
    if ((pressed >> B_ENV & 1u) && (btn >> B_SEQ & 1u)) {   /* SEQ + ENV: the envelope display (and its page of knobs) */
        ui.seq_used = 1; ui.redraw = 1;
        if (ui_knobs >= 2 && ui.view == V_DEVICE && ui.slot != CH_MATERIAL) {   /* on an effect's page: its drawing, or the tracks */
            fxviz_off = !fxviz_off; app_say("FX VIEW", fxviz_off ? "TRACKS" : "PICTURE");
        } else
        {   /* the amplitude envelope, then (a SYNTH) the filter's, then off */
            unsigned dev = ch_track_get(ui.track)->device[CH_MATERIAL];
            ui.env_show = (uint8_t)(ui.env_show == 0 ? 1 : ui.env_show == 1 && dev == CH_DEV_SYNTH ? 2 : 0);
            if (ui.env_show) { ui.mfx = 0; ui.view = V_DEVICE; ui.slot = CH_MATERIAL; }   /* to the track's EDIT page, where it draws */
            app_say("ENVELOPE", !ui.env_show ? "OFF" : ui.env_show == 2 ? "FILTER" : dev == CH_DEV_SYNTH || dev == CH_DEV_POLY || dev == CH_DEV_GRAIN ? "AMP (AGAIN: FILTER)" : "NOT FOR DRUMS");
        }
    }
    if (pressed >> B_SEL & 1u) ui.sel_used = 0;
    if ((released >> B_SEL & 1u) && !ui.sel_used && !(btn >> B_LFO & 1u) && now - ui.down_at[B_SEL] < 500u) { brw_open(); return; }   /* a SEL tap: the BROWSER (browser.h) */
    if (pressed >> B_ENV & 1u) ui.env_used = (btn >> B_SEQ & 1u) != 0;
    if ((released >> B_ENV & 1u) && !ui.env_used && now - ui.down_at[B_ENV] < 500u) {   /* an ENV tap: the keys as a keyboard in the sequencer views, or the steps again */
        ui.kb_live = !ui.kb_live;
        app_say("KEYS", ui.kb_live ? "PLAY THE TRACK" : "THE STEPS"); ui.redraw = 1;
    }
    {   /* OCT- and OCT+: each on its release, unless the two went down together: then the macros. Held together with
         * white 1-4: that macro is assigned (the knobs turned map their parameters to it) until both are pressed again;
         * pressed together alone: K1..K4 play the macros, again: the knobs as they were */
        const uint32_t OCTS = (1u << B_OCTDN) | (1u << B_OCTUP);
        for (unsigned b = B_OCTDN; b <= B_OCTUP; ++b)
            if (pressed >> b & 1u) { if (btn >> B_EDIT & 1u) { ui.oct_edit |= (uint8_t)(1u << (b - B_OCTDN)); ui.held_long[B_EDIT] = 1; } else ui.oct_edit &= (uint8_t)~(1u << (b - B_OCTDN)); }
        if ((pressed & OCTS) && (btn & OCTS) == OCTS && !ui.oct_chord) { ui.oct_chord = 1; ui.oct_keyed = 0; }
        if (ui.oct_chord) {
            if (!(btn & OCTS)) {                           /* both let go */
                ui.oct_chord = 0;
                if (!ui.oct_keyed) {
                    if (ui.mac_arm) { char t[12] = "MACRO "; char n[4]; int_text(ui.mac_arm, n); text_cat(t, n); ui.mac_arm = 0; app_say(t, "ASSIGNED"); }
                    else { ui.mac_play = !ui.mac_play; app_say("MACROS", ui.mac_play ? "ON K1-K4" : "OFF: THE KNOBS BACK"); }
                }
                ui.redraw = 1;
            }
        } else {
            unsigned up = released >> B_OCTUP & 1u, dn = released >> B_OCTDN & 1u;
            if (up || dn) {
                int edit = (ui.oct_edit >> (up ? 1 : 0)) & 1u;
                if (edit) {                                /* EDIT + OCT: the step knobs' other page (OCT+: SEND, CYCLE, GATE; OCT-: RATCHET, PROB, MICRO) */
                    ui.step_alt = up ? 1 : 0; ui.redraw = 1;
                    app_say("STEP KNOBS", ui.step_alt ? "OCT SEND CYCLE GATE" : "VEL RATCH PROB MICRO");
                }
                else if (((ui.view == V_STEP || ui.view == V_SONG) && !ui.kb_live) || (ui.btn >> B_SEQ & 1u)) {
                    lanes_shift(up ? 12 : -12);           /* the STEP view, or SEQ held: the track's lanes */
                    if (ui.btn >> B_SEQ & 1u) ui.seq_used = 1;
                } else {                                   /* the keyboard's octave */
                    int o = ui.oct + (up ? 1 : -1);
                    ui.oct = (int8_t)(o < -3 ? -3 : o > 3 ? 3 : o);
                    char v[8] = ""; if (ui.oct > 0) text_cat(v, "+"); { char n[6]; int_text(ui.oct, n); text_cat(v, n); }
                    app_say("OCTAVE", v); ui.redraw = 1;
                }
            }
        }
    }
    if ((released >> B_LFO & 1u) && now - ui.down_at[B_LFO] < 300u && !ui.held_long[B_LFO]) { ui.view = V_MOD; ui.mfx = 0; ui.redraw = 1; }
    /* GLO: a tap the mix; used as a shift meanwhile (FX, PLAY: the tapped tempo), or held long: nothing (the metronome is REC held) */
    if (pressed >> B_GLO & 1u) ui.glo_used = 0;
    if ((released >> B_GLO & 1u) && !ui.glo_used && now - ui.down_at[B_GLO] <= 500u) { if (ui.view != V_MIX) ui.mix_page = 4; ui.view = V_MIX; ui.mfx = 0; ui.redraw = 1; }   /* (from another view: its first page, KEY / TEMPO) */
    if (pressed >> B_HOME & 1u) { ui.home_used = 0; pat_scan(); }
    if (((pressed >> B_GLO & 1u) && (btn >> B_EDIT & 1u)) || ((pressed >> B_EDIT & 1u) && (btn >> B_GLO & 1u))) {
        /* GLO + EDIT, either first: the LOAD screen (the audio's load, the cores, the limiter); again: the GLO pages */
        ui.glo_used = 1; ui.held_long[B_EDIT] = 1; ui.mfx = 0;
        if (ui.view == V_MIX && ui.mix_page == 5) ui.mix_page = 4; else { ui.view = V_MIX; ui.mix_page = 5; }
        ui.redraw = 1;
    }
    if ((pressed >> B_HOME & 1u) && (btn >> B_GLO & 1u)) {   /* GLO + HOME: the scope, again: back */
        ui.home_used = 1; ui.glo_used = 1;
        if (ui.view == V_SCOPE) ui.view = V_DEVICE;
        else {
            ui.view = V_SCOPE;
            if (!ui.scope_init) { ui.scope_init = 1; ui.spec_src = 0; ui.spec_view = 1; }
            sp_reset();
        }
        ui.mfx = 0; ui.redraw = 1; app_say("SPECTRUM", ui.view == V_SCOPE ? "K1 SRC K2 VIEW K3 COLOUR K4 TONE" : "OFF");
    }
    if ((released >> B_HOME & 1u) && ui.stut) { ui.stut = 0; ch_stutter(0, 0, 0); app_say("STUTTER", "OFF"); ui.redraw = 1; }
    if ((released >> B_HOME & 1u) && !ui.home_used && ui.view != V_PERFORM && !ui.perf_hold) {   /* a HOME tap: the TRACKS view */
        ui.view = ui.view == V_SONG ? V_DEVICE : V_SONG; ui.mfx = 0; song_sync_lane(); ui.redraw = 1;
    }
    if ((pressed >> B_HOME & 1u) && ui.view == V_PERFORM && !(btn >> B_FX & 1u)) {   /* PERFORM: HOME held, a passing tweak */
        perf_snap();
        ui.perf_hold = 1; app_say("PERFORM", "HOLD: BACK ON RELEASE"); ui.redraw = 1;
    }
    if ((released >> B_HOME & 1u) && ui.perf_hold) {       /* let go: the amounts as they were */
        const ch_track_state *ms = ch_track_get(CH_MASTER);
        for (unsigned i = 0; i < CH_FX; ++i) {
            unsigned slot = CH_FX1 + i, dev = ms->device[slot];
            int p = dev == CH_DEV_BYPASS ? -1 : ch_fx_amount_param(dev);
            if (p >= 0 && ui.perf_was[i] > -1.5f) ch_param_set(CH_MASTER, slot, (unsigned)p, ui.perf_was[i]);
            if (ms->bypass[slot] != ui.perf_byp[i]) ch_bypass_set(CH_MASTER, slot, ui.perf_byp[i]);
        }
        ui.perf_hold = 0; app_say("PERFORM", "BACK"); ui.redraw = 1;
    }
    if (ui.perf_hold == 1 && ui.view != V_PERFORM) ui.perf_hold = 0;   /* (2: HOME + K3's, from any view) */
    if ((pressed >> B_HOME & 1u) && (btn >> B_FX & 1u)) { ui.home_used = 1; ui.view = V_PERFORM; ui.mfx = 0; ui.redraw = 1; app_say("PERFORM", "MASTER AMOUNTS"); }
    if ((released >> B_SEQ & 1u) && !ui.seq_used && now - ui.down_at[B_SEQ] < 400u && ui.lane_edit) {   /* a SEQ tap in LANE NOTES: done, the step view stays */
        ui.lane_edit = 0; app_say("LANE NOTES", "DONE"); ui.redraw = 1;
    } else if ((released >> B_SEQ & 1u) && !ui.seq_used && now - ui.down_at[B_SEQ] < 400u) {
        ui.view = ui.view == V_STEP ? V_DEVICE : V_STEP; ui.mfx = 0; ui.redraw = 1;
    }
    if ((pressed >> B_PLAY & 1u) && (btn >> B_GLO & 1u)) {   /* GLO held + PLAY: tap the tempo (the mean of the last taps, up to four gaps; a pause of two seconds starts over) */
        static uint32_t tap_at, gap[4]; static unsigned gaps;
        uint32_t dt = now - tap_at; tap_at = now; ui.glo_used = 1;
        if (dt < 150u || dt > 2000u) { gaps = 0; app_say("TAP TEMPO", "TAP AGAIN"); }
        else {
            if (gaps < 4) gap[gaps++] = dt; else { gap[0] = gap[1]; gap[1] = gap[2]; gap[2] = gap[3]; gap[3] = dt; }
            uint32_t sum = 0; for (unsigned k = 0; k < gaps; ++k) sum += gap[k];
            ch_tempo_set((float)(int32_t)(60000.0f * (float)gaps / (float)sum + 0.5f));
            char v[8]; int_text((int32_t)(ch_tempo() + 0.5f), v); app_say("TAP TEMPO", v);
        }
        ui.redraw = 1;
    }
    else if (pressed >> B_PLAY & 1u) {                    /* PLAY; with REC armed, from stopped: a bar counted in first (PLAY again: no) */
        const ch_snapshot *ps = ch_snapshot_get();
        if (ps->count) ch_transport(0);
        else if (!ps->playing && ui.live_rec && ui.count_off != 1) ch_count_in(ui.count_off == 2 ? 8u : 4u);
        else ch_transport(!ps->playing);
    }
    /* INIT: SEQ + SAVE asks; then SAVE initialises the track, REC everything; any other button cancels */
    if (ui.init_until && (int32_t)(now - ui.init_until) > 0) { ui.init_until = 0; app_say("INIT", "CANCELLED"); }
    if (ui.init_until && pressed) {
        uint32_t pr = pressed & ~(1u << B_SEQ);
        if (pr >> B_SAVE & 1u) { init_track(ui.track); char n[4] = {'T', (char)('1' + ui.track), 0}; app_say("INIT", n); }
        else if (pr >> B_REC & 1u) { init_all(); ui.rec_used = 1; app_say("INIT", "ALL TRACKS"); }
        else if (pr) app_say("INIT", "CANCELLED");
        if (pr) { ui.init_until = 0; pressed = 0; ui.redraw = 1; }
    } else if ((pressed >> B_SAVE & 1u) && (btn >> B_SEQ & 1u)) {
        ui.seq_used = 1; ui.save_used = 1; ui.init_until = now + 4000u;   /* (SAVE's release then saves nothing: it was asking) */
        app_say("INIT?", "SAVE:TRACK REC:ALL"); ui.msg_until = ui.init_until;
        pressed &= ~(1u << B_SAVE);
    }
    if (pressed >> B_REC & 1u) ui.rec_used = 0;
    if ((pressed >> B_REC & 1u) && (btn >> B_EDIT & 1u)) {  /* EDIT + REC: the pattern as it was before the last generator, clear or lane shift; again: forward again */
        ui.rec_used = 1; ui.held_long[B_EDIT] = 1;
        if (ch_undo_swap() == 0) { ui.undo_fwd ^= 1; song_sync_lane(); app_say("UNDO", ui.undo_fwd ? "THE PATTERN BACK" : "REDONE"); }
        else app_say("UNDO", "NOTHING TO UNDO");
        ui.redraw = 1;
    }
    if ((pressed >> B_REC & 1u) && ui.mac_arm) {          /* REC while a macro is assigned: all its maps undone, every track */
        ui.rec_used = 1;
        unsigned n = 0, mm = CH_MODS + ui.mac_arm - 1u;
        for (unsigned t = 0; t < CH_TRACKS; ++t) {
            const ch_track_state *ms = ch_track_get(t);
            for (unsigned k = 0; k < CH_MAPS; ++k) if (ms->map[k].depth != 0 && ms->map[k].mod == mm) { ch_map_set(t, mm, ms->map[k].slot, ms->map[k].param, 0, 0); ++n; }
        }
        char l[12] = "MACRO "; char d[4]; int_text(ui.mac_arm, d); text_cat(l, d);
        char v[16] = ""; if (!n) text_cat(v, "NO MAPS"); else { int_text((int32_t)n, v); text_cat(v, n == 1 ? " MAP CLEARED" : " MAPS CLEARED"); }
        app_say(l, v); ui.redraw = 1;
    } else if ((pressed >> B_REC & 1u) && (btn >> B_LFO & 1u)) {   /* LFO + REC: every mapping of the chosen modulator undone */
        ui.rec_used = 1; ui.held_long[B_LFO] = 1;
        const ch_track_state *ms = ch_track_get(ui.track);
        unsigned n = 0;
        for (unsigned k = 0; k < CH_MAPS; ++k)
            if (ms->map[k].depth != 0 && ms->map[k].mod == ui.mod) { ch_map_set(ui.track, ui.mod, ms->map[k].slot, ms->map[k].param, 0, 1); ++n; }
        char l[8] = "MOD "; char d[4]; int_text((int32_t)ui.mod + 1, d); text_cat(l, d);
        char v[16] = ""; if (!n) text_cat(v, "NO MAPS"); else { int_text((int32_t)n, v); text_cat(v, n == 1 ? " MAP CLEARED" : " MAPS CLEARED"); }
        app_say(l, v); ui.redraw = 1;
    }
    if ((pressed >> B_REC & 1u) && ui.view == V_STEP && (btn >> B_SEQ & 1u)) {   /* SEQ + REC: the track cleared */
        ui.seq_used = 1; ui.rec_used = 1;
        ch_undo_mark(); ui.undo_fwd = 0; gen_clear(ui.track);
        app_say("CLEARED", "THE TRACK");
    }
    if ((pressed >> B_REC & 1u) && ui.view == V_SONG && (btn >> B_SEQ & 1u)) {   /* in TRACKS: the whole pattern */
        ui.seq_used = 1; ui.rec_used = 1;
        ch_undo_mark(); ui.undo_fwd = 0; for (unsigned t = 0; t < CH_TRACKS; ++t) gen_clear(t);
        app_say("CLEARED", "THE PATTERN"); ui.redraw = 1;
    }
    if ((pressed >> B_REC & 1u) && ui.view == V_MIX && ui.mix_page == 6 && (btn >> B_SEQ & 1u)) {   /* on the SONG page: the song */
        ui.seq_used = 1; ui.rec_used = 1;
        for (unsigned e = 0; e < PJ_SONG_MAX; ++e) { song.e[e].pat = 0; song.e[e].bars = 0; }
        song.len = 0; song.on = 0; song.pos = 0; ui.song_edit = 0;
        app_say("CLEARED", "THE SONG"); ui.redraw = 1;
    }
    if ((released >> B_REC & 1u) && !ui.rec_used && now - ui.down_at[B_REC] > 500u && !ui.lock_rec) {   /* REC held and let go, nothing pressed with it: the metronome on / off */
        unsigned on = !ch_snapshot_get()->metronome;
        ch_metronome_set(on); app_say("METRONOME", on ? "ON" : "OFF"); ui.redraw = 1;
    }
    else if ((released >> B_REC & 1u) && !ui.rec_used) {     /* a REC tap: lock recording ends, else live rec on / off */
        if (ui.lock_rec) lock_end();
        else {
            int held = -1;                               /* a track's key (black 1-4) held: the whole keyboard for it */
            if (ui.view != V_STEP && ui.view != V_SONG) for (unsigned t = 0; t < CH_TRACKS; ++t) if (keys >> BK[t] & 1u) held = (int)t;
            if (!ui.live_rec && held >= 0) {
                ui.live_rec = 1; ui.kb_rec = 1; ui.track = (uint8_t)held;
                char n[16] = "ON: KEYS T"; char d[4] = {(char)('1' + held), 0}; text_cat(n, d);
                app_say("LIVE REC", n);
            } else {
                ui.live_rec = !ui.live_rec; ui.kb_rec = 0;
                app_say("LIVE REC", ui.live_rec ? (ui.view == V_STEP ? "ON" : "ON: PADS 1-4") : "OFF");
            }
        }
    }
    /* SAVE: a tap opens the LIBRARY; held 0.7 s with nothing else, it saves the project (a white key pressed sooner
     * stores a preset instead, as it did). save_used: the press has been spent, its release does nothing */
    { static uint32_t save_at PLAT_POOL;                   /* (in the pool: the RAM proper is full; set at every press, so it needs no start value) */
      if (pressed >> B_SAVE & 1u) { ui.save_used = 0; save_at = now; pre_scan(); }
      if ((btn >> B_SAVE & 1u) && !ui.save_used && now - save_at >= 700u) { ui.save_used = 1; pj_save(); ui.redraw = 1; }
      if ((released >> B_SAVE & 1u) && !ui.save_used) lib_open(); }
    if ((pressed >> B_EDIT & 1u)) pre_scan();
    /* keys */
    for (unsigned k = 0; k < NKEYS; ++k) {
        uint32_t bit = 1u << k;
        int white = -1, black = -1;
        for (int w = 0; w < 16; ++w) if (WK[w] == k) white = w;
        for (int b = 0; b < 11; ++b) if (BK[b] == k) black = b;
        if ((keys & bit) && !(ui.keys & bit)) {
            if (ui.oct_chord) {                           /* OCT- + OCT+ held: white 1-4 picks the macro to assign */
                if (white >= 0 && white < CH_MACROS) {
                    ui.oct_keyed = 1; ui.mac_arm = (uint8_t)(white + 1);
                    char t[12] = "MACRO "; char n[4]; int_text(white + 1, n); text_cat(t, n);
                    app_say(t, "TURN KNOBS"); ui.redraw = 1;
                }
                continue;
            }
            if (ui.gate_keys && !(btn >> B_ENV & 1u)) {    /* the TRANCE gate's steps: a white key turns its step on or off (the black keys rest) */
                if (white >= 0) {
                    const ch_track_state *gs = ch_track_get(ET);
                    unsigned p = 4u + (unsigned)white / 4u, cur = (unsigned)(gs->param[ch_row(ui.slot, CH_DEV_TRANCE)][p] + 0.5f) & 15u;
                    cur ^= 1u << ((unsigned)white & 3u);
                    ch_param_set(ET, ui.slot, p, (float)cur);
                    char n[12] = "STEP "; char d[4]; int_text(white + 1, d); text_cat(n, d);
                    app_say(n, cur >> ((unsigned)white & 3u) & 1u ? "ON" : "OFF"); ui.redraw = 1;
                }
                continue;
            }
            if (btn >> B_ENV & 1u) {                      /* ENV held, from any view: the mixer's keys, white 1-4 a track's mute, black 1-4 its solo */
                ui.env_used = 1;
                if ((white >= 0 && white < CH_TRACKS) || (black >= 0 && black < CH_TRACKS)) { mix_keys(white, black); ui.redraw = 1; }
                continue;
            }
            if (ui.view == V_GEN && !(btn >> B_SEQ & 1u) && !(btn >> B_HOME & 1u) && !(btn >> B_SAVE & 1u) && !(btn >> B_EDIT & 1u) && !(btn >> B_ENV & 1u)) { gen_key(white, black); continue; }   /* the line page's keys */
            if ((btn >> B_SEQ & 1u) && (ui.view == V_STEP || ui.view == V_SONG) && white >= 11 && white < 16) { ui.seq_used = 1; generate((unsigned)white - 11u); continue; }   /* SEQ + white 12-16: the generators */
            if ((btn >> B_SEQ & 1u) && (ui.view == V_STEP || ui.view == V_SONG) && white >= 7 && white < 11) {   /* SEQ + white 8-11: the page */
                ui.seq_used = 1;
                unsigned pg = (unsigned)white - 7u, len = ch_seq_get(ui.track)->len;
                if (pg * 16u >= len) { char v[8]; int_text((int32_t)len, v); app_say("PAGE", "PAST THE END"); (void)v; continue; }
                ui.step_page = (uint8_t)pg;
                if (ui.sel_step >= 0) { ui.sel_step = (int16_t)(pg * 16u + (unsigned)ui.sel_step % 16u); song_sync_lane(); }
                char v[8] = ""; int_text((int32_t)pg + 1, v); text_cat(v, "/4"); app_say("PAGE", v); ui.redraw = 1;
                continue;
            }
            if ((btn >> B_LFO & 1u) && white >= 0 && white < 4) { ui.mod = (uint8_t)white; ui.mod_page = (uint8_t)(white * 2); app_say("MOD", ch_mod_name(ch_track_get(ui.track)->mod_type[white])); continue; }
            if ((btn >> B_SAVE & 1u) && white >= 0 && !(btn >> B_SEQ & 1u)) {   /* SAVE held + a white key: the sound as that preset */
                ui.save_used = 1; preset_save((unsigned)white); continue;
            }
            if ((btn >> B_EDIT & 1u) && white >= 0) {   /* EDIT held + a white key: that preset into the track */
                ui.held_long[B_EDIT] = 1; preset_load((unsigned)white); continue;
            }
            if ((btn >> B_HOME & 1u) && white >= 0) {   /* HOME held + a white key: the pattern (SEQ too: a copy into it) */
                ui.home_used = 1;
                if (btn >> B_SEQ & 1u) { ui.seq_used = 1; pattern_copy((unsigned)white); }
                else pattern_select((unsigned)white, 1);
                continue;
            }
            int seqv = (ui.view == V_STEP || ui.view == V_SONG) && !ui.kb_live;   /* (an ENV tap: a keyboard instead) */
            if (seqv && (btn >> B_REC & 1u) && white >= 0) { ui.rec_used = 1; lock_arm(ui.step_page * 16u + (unsigned)white); continue; }   /* REC + a step: armed for locks */
            if (seqv && ui.view == V_STEP && white >= 0) { step_key(white, 1); continue; }
            if (seqv && ui.view == V_STEP && black >= 0) { lane_key(black, 1); continue; }
            if (seqv && ui.view == V_SONG && white >= 0) { song_step_key(white, 1); continue; }
            if (seqv && ui.view == V_SONG && black >= 0) { song_black(black, 1); continue; }
            if (ui.live_rec && !ui.kb_rec && ui.view != V_STEP && ui.view != V_SONG && black >= 0 && black < CH_TRACKS) {   /* the track pads: play, and record while playing */
                if (ch_snapshot_get()->playing) pad_hit((unsigned)black);
                else { unsigned n = ch_seq_get((unsigned)black)->note[0]; ch_note((unsigned)black, n, 100); ui.pad_note[black] = (uint8_t)n; }
                continue;
            }
            int keypg = ui.view == V_MIX && ui.mix_page == 4 && !btn;   /* the KEY page: a key played is the key note, and is heard (below) */
            if (keypg) { ui.key_root = (uint8_t)((53u + k) % 12u + 1u); key_apply(); app_say("KEY", KEY_NAME[ui.key_root]); ui.redraw = 1; }
            else if ((ui.view == V_MIX || mixer_on()) && !(btn & ((1u << B_SEQ) | (1u << B_LFO) | (1u << B_SAVE) | (1u << B_EDIT) | (1u << B_HOME) | (1u << B_REC)))
                && ((white >= 0 && white < CH_TRACKS) || (black >= 0 && black < CH_TRACKS))) {   /* the MIX pages, and wherever the MIXER footer is up: white 1-4 a track's mute, black 1-4 its solo */
                mix_keys(white, black);
                ui.redraw = 1; continue;
            }
            if (ui.view == V_DEVICE && !ui.kb_live) {      /* EDIT and FX: the keys as in TRACKS (the steps, lanes merged; black 1-4 the track); an ENV tap: a keyboard */
                if (mixer_on()) continue;                  /* (the MIXER footer up: the keys are its mutes and solos, the rest dark and at rest) */
                if (white >= 0 && (btn >> B_REC & 1u)) { ui.rec_used = 1; lock_arm(ui.step_page * 16u + (unsigned)white); continue; }
                if (white >= 0) { if (ui.step_page * 16u + (unsigned)white < ch_seq_get(ui.track)->len) song_step_key(white, 1); continue; }
                if (black >= 0) { song_black(black, 1); continue; }
            }
            ui.last_note[k] = 53 + (int32_t)k + 12 * ui.oct;
            if (ui.last_note[k] < 1) ui.last_note[k] = 1; else if (ui.last_note[k] > 127) ui.last_note[k] = 127;
            ch_note(ui.track, (unsigned)ui.last_note[k], 100);
            if (ui.live_rec && ch_snapshot_get()->playing) live_write((unsigned)ui.last_note[k], 100);
        } else if (!(keys & bit) && (ui.keys & bit)) {
            if (ui.view == V_STEP && white >= 0) step_key(white, 0);
            if ((ui.view == V_SONG || ui.view == V_DEVICE) && white >= 0) song_step_key(white, 0);
            if ((ui.view == V_SONG || ui.view == V_DEVICE) && black >= 0) song_black(black, 0);
            else if (black >= 0) lane_key(black, 0);          /* (releases are harmless whichever mode set the key) */
            if (black >= 0 && black < CH_TRACKS && ui.pad_note[black]) { ch_note((unsigned)black, ui.pad_note[black], 0); ui.pad_note[black] = 0; }
            if (ui.last_note[k]) { ch_note(ui.track, (unsigned)ui.last_note[k], 0); ui.last_note[k] = 0; }
        }
    }
    ui.keys = keys;
    ui.entropy = ui.entropy * 1664525u + 1013904223u + plat_ticks24() + keys;   /* the frame's timing and keys, for the generators' seeds */
    /* encoders */
    int32_t sel = plat_enc(EN_SELECT);
    if (sel) {
        enc_speed(6, sel);
        if (btn >> B_EDIT & 1u) { ui.held_long[B_EDIT] = 1; track_scale_turn(sel); }   /* EDIT + SELECT: the track's scale */
        else if (mixpage_on()) {                         /* the MIXER page: SELECT is the RETURN's level */
            char v[8]; ch_return_set(fm_clampf(ch_return() + (float)sel * enc_s, 0, 1));
            int_text((int32_t)(ch_return() * 100 + 0.5f), v); app_say("RETURN LEVEL", v);
        }
        else if (ui.lane_edit && ui.view == V_STEP && !(btn >> B_SEQ & 1u)) {   /* LANE NOTES: SELECT up and down the lanes, each heard */
            int l = (int)ui.sel_lane + (sel > 0 ? 1 : -1); l = l < 0 ? 0 : l > CH_LANES - 1 ? CH_LANES - 1 : l;
            ui.sel_lane = (uint8_t)l;
            lane_hear(ch_seq_get(ui.track)->note[l]);
            char lb[12] = "LANE "; char d[4], nv[8]; int_text(l + 1, d); text_cat(lb, d); note_text(ch_seq_get(ui.track)->note[l], nv);
            app_say(lb, nv);
        }
        else if (btn >> B_SEQ & 1u) {
            ui.seq_used = 1;
            if (ui.view == V_DEVICE && ui.slot == CH_MATERIAL) change_device(sel);
            else if (ui.view == V_DEVICE) {               /* (SEQ held) an effect's pages */
                const ch_device_desc *d = cur_dev();
                if (d) { unsigned n = pages_now(); ui.page[ui.slot] = (uint8_t)((ui.page[ui.slot] + (sel > 0 ? 1u : n - 1u)) % n); }
            }
            else if (ui.view == V_STEP || ui.view == V_SONG) {   /* (SEQ held) the page, through the pattern's length */
                unsigned len = ch_seq_get(ui.track)->len, pages = (len + 15u) / 16u; if (!pages) pages = 1;
                unsigned pg = (ui.step_page + (sel > 0 ? 1u : pages - 1u)) % pages;
                ui.step_page = (uint8_t)pg;
                if (ui.sel_step >= 0) { ui.sel_step = (int16_t)(pg * 16u + (unsigned)ui.sel_step % 16u); song_sync_lane(); }
                char v[8] = ""; int_text((int32_t)pg + 1, v); text_cat(v, "/"); { char q[4]; int_text((int32_t)pages, q); text_cat(v, q); } app_say("PAGE", v);
            }
            else if (ui.view == V_MOD) {
                unsigned m = ui.mod_page / 2;
                unsigned ty = ch_track_get(ui.track)->mod_type[m] == CH_MOD_TRIG ? CH_MOD_WAVE : CH_MOD_TRIG;   /* LFO or TRIG RND */
                ch_mod_type_set(ui.track, m, ty);
                app_say("MOD", ch_mod_name(ty));
            }
        } else if (ui.view == V_DEVICE && ui.slot >= CH_SLOTS) {
            /* an empty place: no pages (FX steps along the chain) */
        } else if (ui.view == V_DEVICE) {
            unsigned n = pages_now();
            ui.page[ui.slot] = (uint8_t)((ui.page[ui.slot] + (sel > 0 ? 1u : n - 1u)) % n);
        } else if (ui.view == V_GEN) {
            ui.gen_page ^= 1; ui.redraw = 1;
        } else if (ui.view == V_MOD) {
            ui.mod_page = (uint8_t)((ui.mod_page + (sel > 0 ? 1u : 7u)) % 8u);
            ui.mod = (uint8_t)(ui.mod_page / 2);
        } else if ((ui.btn >> B_SEL & 1u) && step_sel_set()) { ui.sel_used = 1; step_move(0, (int)sel); }   /* SEL held on a set step + SELECT: the step along its lane */
        else if (ui.view == V_STEP && ui.lane_held >= 0) edit_step(5, sel);   /* a lane's key held + SELECT: the ROOT */
        else if (ui.view == V_STEP || ui.view == V_SONG) {
            /* the selected step, left and right through the pattern (round at the ends); the page follows it */
            int len = ch_seq_get(ui.track)->len ? ch_seq_get(ui.track)->len : 16;
            int st = (ui.sel_step >= 0 ? ui.sel_step : 0) + (int)sel;   /* one step a detent */
            st = (st % len + len) % len;
            ui.sel_step = (int16_t)st; ui.step_page = (uint8_t)(st / 16); song_sync_lane();
            char v[8]; int_text(st + 1, v); app_say("STEP", v);
        } else if (ui.mix_page == 5) ui.mix_page = 4;   /* (the LOAD screen: SELECT back to the pages) */
        else ui.mix_page = MIX_ORDER[(mix_pos(ui.mix_page) + (sel > 0 ? 1u : MIX_NAV - 1u)) % MIX_NAV];
        ui.redraw = 1;
    }
    for (unsigned i = 0; i < 6; ++i) {
        int32_t e = plat_enc(ENC_OF[i]);
        if (!e) continue;
        enc_speed(i, e);
        if (i == 4 && ui.view == V_MIX && ui.mix_page == 5) {   /* LOAD, ALGORITHM: the engine's sub-block, 32 frames or 64 (a trial: fewer trips between the tracks' code, a coarser control rate) */
            ch_sub_set(e > 0 ? 64u : 32u);
            app_say("SLICE", ch_sub_get() == 64 ? "64" : "32"); continue;
        }
        int seqh = (int)(ui.btn >> B_SEQ & 1u);
        if (ui.mac_play && i < 4 && !ui.mac_arm && !seqh && !(ui.btn >> B_HOME & 1u)) {   /* the macros on K1..K4 */
            float v = fm_clampf(ch_macro(i) + (float)e * enc_s, 0, 1);
            ch_macro_set(i, v);
            char l[12] = "MACRO "; char n[8]; int_text((int32_t)i + 1, n); text_cat(l, n); int_text((int32_t)(v * 100 + 0.5f), n);
            app_say(l, n); ui.redraw = 1; continue;
        }
        if (i == 4 && (ui.btn >> B_EDIT & 1u)) { ui.held_long[B_EDIT] = 1; track_key_turn(e); continue; }   /* EDIT + ALGO: the track's key, from any view */
        if (i == 5 && (ui.btn >> B_EDIT & 1u)) {         /* EDIT + PRESET: the lanes a semitone a detent, or a scale step */
            ui.held_long[B_EDIT] = 1; lanes_step(e > 0 ? 1 : -1);
            continue;
        }
        if (i == 4 && (ui.btn >> B_SEL & 1u) && step_sel_set()) { ui.sel_used = 1; step_move(1, (int)e); continue; }   /* SEL held on a set step + ALGO: the step to another lane */
        if (ui.view == V_SONG && !seqh && i == 4) {    /* the TRACKS view: ALGO up and down the tracks */
            int t = (int)ui.track + (int)e; t = t < 0 ? 0 : t > CH_TRACKS - 1 ? CH_TRACKS - 1 : t;
            ui.track = (uint8_t)t; ui.mix_ret = 0; song_sync_lane();
            char n[4] = {'T', (char)('1' + t), 0}; app_say(n, material_name((unsigned)t));
            ui.redraw = 1; continue;
        }
        if (ui.view == V_STEP && !seqh && i == 4) {    /* the step view: ALGO up and down the lanes (SELECT: the steps) */
            int l = (int)ui.sel_lane - (int)e;   /* (reversed: the way the cursor moves on the screen) */ l = l < 0 ? 0 : l > CH_LANES - 1 ? CH_LANES - 1 : l;
            ui.sel_lane = (uint8_t)l;
            char lb[12] = "LANE "; char d[4], nv[8]; int_text(l + 1, d); text_cat(lb, d); note_text(ch_seq_get(ui.track)->note[l], nv);
            app_say(lb, nv); ui.redraw = 1; continue;
        }
        if (i == 4 && mixer_on() && !seqh && !(ui.view == V_MIX && ui.mix_page == 3)) {   /* the MIXER footer on the screen: ALGO is the chosen strip's level (a track's, or the RETURN's); SEL + ALGO does what ALGO does without it */
            if (!(ui.btn >> B_SEL & 1u)) {
                char v[8];
                if (ui.mix_ret) {
                    ch_return_set(fm_clampf(ch_return() - (float)e * enc_s, 0, 1));   /* (the other way round from ALGO's lists: up is up) */
                    int_text((int32_t)(ch_return() * 100 + 0.5f), v); app_say("RETURN LEVEL", v);
                } else {
                    float lv = fm_clampf(ch_track_get(ui.track)->level - (float)e * enc_s, 0, 1);
                    ch_mix_set(ui.track, CH_MIX_LEVEL, lv);
                    char l[12] = {'T', (char)('1' + ui.track), ' ', 'L', 'E', 'V', 'E', 'L', 0}; int_text((int32_t)(lv * 100 + 0.5f), v);
                    app_say(l, v);
                }
                ui.redraw = 1; continue;
            }
            if (ui.view == V_DEVICE) {
                ui.sel_used = 1;
                if (ui.slot == CH_MATERIAL) change_device(e); else fx_choose(e);
                ui.redraw = 1; continue;
            }
        }
        if (i == 4 && (ui.btn >> B_SEL & 1u)) {          /* SEL held + ALGO: the track's level, from any view */
            ui.sel_used = 1;
            float lv = fm_clampf(ch_track_get(ui.track)->level + (float)e * enc_s, 0, 1);
            ch_mix_set(ui.track, CH_MIX_LEVEL, lv);
            char l[12] = {'T', (char)('1' + ui.track), ' ', 'L', 'E', 'V', 'E', 'L', 0}; char v[8]; int_text((int32_t)(lv * 100 + 0.5f), v);
            app_say(l, v); ui.redraw = 1; continue;
        }
        if (i < 2 && (ui.btn >> B_HOME & 1u) && ui.view != V_PERFORM) {   /* HOME + K1 (all tracks) or K2 (this track): the stutter; the first turn starts it at 1/16, then either knob sets the rate */
            static const char *const STUT_NAME[CH_STUTTER_DIVS] = {"1/8", "1/16", "1/32", "1/64"};
            ui.home_used = 1;
            if (!ch_snapshot_get()->playing) { app_say("STUTTER", "PLAY FIRST"); continue; }
            int d = ui.stut ? clampi((int)ui.stut - 1 + (e > 0 ? 1 : -1), 0, CH_STUTTER_DIVS - 1) : 1;
            if (!ui.stut) ui.stut_all = i == 0;
            ui.stut = (uint8_t)(d + 1); ch_stutter(1, (unsigned)d, ui.stut_all ? 15u : 1u << ui.track);
            char l[12] = "STUTTER"; if (!ui.stut_all) { l[7] = ' '; l[8] = 'T'; l[9] = (char)('1' + ui.track); l[10] = 0; }
            app_say(l, STUT_NAME[d]); ui.redraw = 1; continue;
        }
        if (i == 2 && (ui.btn >> B_HOME & 1u) && ui.view != V_PERFORM) {   /* HOME + K3: the chosen master effect's amount (GLO, SETUP: HOLD FX), for as long as HOME is held */
            unsigned q = ui.snd_off >> 1;
            ui.home_used = 1;
            if (!ui.perf_hold) { perf_snap(); ui.perf_hold = 2; }
            edit_perform(q, e * 4); ui.redraw = 1; continue;
        }
        if (ui.view == V_PERFORM) { if (i < 4) edit_perform(i, e); continue; }
        if (ui.view == V_SCOPE) { if (i < 5) edit_scope(i, e); continue; }   /* (ALGO: its TILT) */
        if (ui.view == V_GEN) { if (i < 4) edit_gen(i, e); continue; }
        if (i < 4 && mixpage_on()) {                     /* the MIXER page: K1-K4 the tracks' levels; FX held: the master effects' amounts */
            if (ui.btn >> B_FX & 1u) { ui.held_long[B_FX] = 1; edit_perform(i, e); }
            else {
                float lv = fm_clampf(ch_track_get(i)->level + (float)e * enc_s, 0, 1);
                ch_mix_set(i, CH_MIX_LEVEL, lv);
                char l[12] = {'T', (char)('1' + i), ' ', 'L', 'E', 'V', 'E', 'L', 0}; char v[8]; int_text((int32_t)(lv * 100 + 0.5f), v);
                app_say(l, v);
            }
            ui.redraw = 1; continue;
        }
        if (i == 5) {                                    /* PRESET: the track, one a detent, round (the step view: SEQ + PRESET) */
            ui.mfx = 0;
            if (seqh) ui.seq_used = 1;
            if (mixer_on()) {                            /* the MIXER footer: five strips round, the RETURN after track 4 */
                int q = ((ui.mix_ret ? (int)CH_TRACKS : (int)ui.track) + (int)e) % ((int)CH_TRACKS + 1); if (q < 0) q += CH_TRACKS + 1;
                ui.mix_ret = q == CH_TRACKS;
                if (ui.mix_ret) { app_say("RETURN", "ALGO: ITS LEVEL"); ui.redraw = 1; continue; }
                e = q - (int)ui.track;
            }
            ui.mix_ret = 0;
            int t = ((int)ui.track + (int)e) % (int)CH_TRACKS; if (t < 0) t += CH_TRACKS;
            ui.track = (uint8_t)t;
            char n[4] = {'T', (char)('1' + t), 0}; app_say(n, material_name((unsigned)t));
            ui.redraw = 1; continue;
        }
        if (i == 4 && ui.view == V_DEVICE && ui.slot == CH_MATERIAL) { change_device(e); ui.redraw = 1; continue; }   /* ALGO: the engine */
        if (i == 4 && ui.view == V_DEVICE) { fx_choose(e); continue; }   /* ALGO: the place's effect */
        if (i == 4 && ui.view != V_STEP && ui.view != V_SONG && !(ui.view == V_MIX && (ui.mix_page == 3 || ui.mix_page == 0))) continue;     /* ALGO elsewhere: nothing (the SENDS page: the steps' SEND's send) */
        if (ui.view == V_DEVICE) edit_param(i, e);
        else if (ui.view == V_MOD) edit_mod(i, e);
        else if (ui.view == V_STEP || ui.view == V_SONG) edit_step(i, e);
        else edit_mix(i, e);
        ui.redraw = 1;
    }
    /* lights (the FM-1's are one colour: full, dim, off; PLAY has a green and a red LED):
     * every labelled button mid (readable), full when it is the one in use: the open slot; PLAY green while playing, red while it records; REC blinks while
     * live record waits for PLAY, full while it records (or LOCK); in the STEP view the keys below */
    const ch_snapshot *sn = ch_snapshot_get();
    uint32_t lit = 0, dim = 0, klit = 0, kdim = 0;
    for (unsigned s = 0; s < CH_SLOTS; ++s) {
        if (ui.view == V_DEVICE && ui.slot == s) lit |= 1u << (s ? B_FX : B_EDIT);   /* EDIT: the engine; FX: a slot */
    }
    if (ui.view == V_MOD) lit |= 1u << B_LFO;
    if (ui.view == V_MIX) lit |= 1u << B_GLO;
    if (ap.on && ui.view != V_MIX && (now / 400u) & 1u) lit |= 1u << B_GLO;   /* AUTOPILOT on: GLO blinks slowly */
    else if (sn->metronome) dim |= 1u << B_REC;   /* the metronome on: REC dim (its hold) */
    if (ui.kb_live) lit |= 1u << B_ENV;
    if (ui.view == V_DEVICE && ui.mfx) lit |= 1u << B_GLO;   /* the master chain: GLO and FX */
    if (ui.view == V_PERFORM) lit |= (1u << B_HOME) | (1u << B_FX);
    if (ui.view == V_SCOPE) lit |= (1u << B_HOME) | (1u << B_GLO);
    if (ui.stut) lit |= 1u << B_HOME;
    if (ui.view == V_GEN) lit |= 1u << B_ARP;
    if (ui.view == V_SONG && !ui.kb_live) {                  /* HOME lit; the chosen track's steps on the white keys, lanes merged */
        lit |= 1u << B_HOME;
        const ch_seq_state *sq = ch_seq_get(ui.track);
        for (unsigned w = 0; w < 16; ++w) {
            unsigned idx = ui.step_page * 16u + w;
            if (idx >= sq->len) continue;
            int on = 0, here = sn->step[ui.track] == (int)idx;
            for (unsigned l = 0; l < CH_LANES; ++l) on |= sq->step[l][idx].vel != 0;
            if (on != here) klit |= 1u << WK[w];
            else if ((w & 3u) == 0) kdim |= 1u << WK[w];
        }
        for (unsigned t = 0; t < CH_TRACKS; ++t) { if (t == ui.track || hit_on(t)) klit |= 1u << BK[t]; else kdim |= 1u << BK[t]; }
    }
    if (ui.view == V_SONG) lit |= 1u << B_HOME;
    if (ui.view == V_DEVICE && !ui.kb_live) {            /* EDIT and FX: the keys as TRACKS has them (the track's steps, lanes merged; black 1-4 the tracks) */
        for (unsigned t = 0; t < CH_TRACKS; ++t) { if (t == ui.track || hit_on(t)) klit |= 1u << BK[t]; else kdim |= 1u << BK[t]; }
        const ch_seq_state *sq = ch_seq_get(ui.track);
        for (unsigned w = 0; w < 16; ++w) {
            unsigned idx = ui.step_page * 16u + w;
            if (idx >= sq->len) continue;
            int on = 0, here = sn->step[ui.track] == (int)idx;
            for (unsigned l = 0; l < CH_LANES; ++l) on |= sq->step[l][idx].vel != 0;
            if (on != here) klit |= 1u << WK[w];
            else if ((w & 3u) == 0) kdim |= 1u << WK[w];
        }
    }
    if (ui.view == V_STEP) lit |= 1u << B_SEQ;
    if (ui.view == V_STEP && !ui.kb_live) {
        const ch_seq_state *sq = ch_seq_get(ui.track);
        /* the step keys: an on step full; the beats (1 5 9 13) dim, for the grid; past the length dark.
         * The playhead lights an empty step and darkens an on one, so it shows over both */
        int blink = (now / 200u) & 1u;                     /* 2.5 Hz */
        for (unsigned w = 0; w < 16; ++w) {
            unsigned idx = ui.step_page * 16u + w;
            if (idx >= sq->len) continue;
            const ch_step *st = &sq->step[ui.sel_lane][idx];
            int on = st->vel != 0, here = sn->step[ui.track] == (int)idx;
            if (on && !here && (st->chance < 100 || st->cyc_b) && blink) on = 0;   /* chance / cycle: it blinks */
            if (on != here) klit |= 1u << WK[w];
            else if ((w & 3u) == 0) kdim |= 1u << WK[w];
        }
        /* black keys 1-8, the lanes: the chosen one full, a sounding one flashes full, lanes with notes dim
         * (holding a step: the lanes on that step) */
        klit |= 1u << BK[ui.sel_lane];
        for (unsigned l = 0; l < CH_LANES; ++l) {
            if (sn->lanes[ui.track] >> l & 1u) klit |= 1u << BK[l];
            int used = 0;
            if (ui.held_step >= 0) used = sq->step[l][ui.held_step].vel != 0;
            else for (unsigned i = 0; i < sq->len && !used; ++i) used = sq->step[l][i].vel != 0;
            if (used) kdim |= 1u << BK[l];
        }
    }
    if (ui.view == V_GEN) {                              /* the line page: white 1-4 the lanes redrawn (full) or kept (dim), black 1-8 the lanes in use */
        klit = 0; kdim = 0;
        for (unsigned w = 0; w < 4; ++w) { if (ui.gen_lock >> w & 1u) kdim |= 1u << WK[w]; else klit |= 1u << WK[w]; }
        if (ui.gen_sting) klit |= 1u << WK[4]; else kdim |= 1u << WK[4];   /* white 5: the draw, lit for Sting's */
        unsigned mask = ui.gen_lanes ? ui.gen_lanes : 0xFFu;
        for (unsigned k = 0; k < CH_LANES; ++k) { if (mask >> k & 1u) klit |= 1u << BK[k]; else kdim |= 1u << BK[k]; }
    }
    uint32_t kdark = 0;                                  /* keys held dark, whatever else says */
    if (ui.kb_live && (ui.view == V_STEP || ui.view == V_SONG || ui.view == V_DEVICE)) {   /* the keys play the track: every key full, so it cannot be missed; a held one goes dark */
        klit = 0; kdim = 0;
        if (keyed(ui.track) && ch_track_get(ui.track)->device[CH_MATERIAL] != CH_DEV_KIT) {   /* in a key (the track's own, else the master's): its notes full, the rest dim (a KIT has no notes to be in key) */
            for (unsigned k = 0; k < NKEYS; ++k) { if (in_scale(ui.track, 53 + (int)k)) klit |= 1u << k; else kdim |= 1u << k; }   /* (the rest dim) */
        } else {
            for (unsigned w = 0; w < 16; ++w) klit |= 1u << WK[w];
            for (unsigned k = 0; k < 11; ++k) klit |= 1u << BK[k];
        }
        kdark = keys;
    }
    if ((ui.view == V_MIX || mixer_on()) && ui.view != V_GEN) {   /* the MIX pages, and the MIXER footer: white 1-4 the mutes (a muted track full), black 1-4 the solos (the one alone full) */
        int so = solo_track(), pads = ui.live_rec && !ui.kb_rec;   /* (recording live: the black keys are the track pads) */
        klit = 0; kdim = 0; kdark = 0;                       /* nothing else lit (not the steps, not a keyboard that is on), so the eight read at a glance */
        for (unsigned t = 0; t < CH_TRACKS; ++t) {
            uint32_t w = 1u << WK[t], b = 1u << BK[t];
            klit &= ~w; kdim &= ~w; kdark &= ~w;
            if (ch_track_get(t)->mute) klit |= w; else kdim |= w;
            if (!pads) { klit &= ~b; kdim &= ~b; kdark &= ~b; if (ui.solo_mask ? (ui.solo_mask >> t & 1u) : so == (int)t) klit |= b; else kdim |= b; }
        }
        if (ui.view == V_MIX && ui.mix_page == 4) {          /* the KEY page: the keyboard, every key dim and the key note full in each octave */
            klit = 0; kdim = (1u << NKEYS) - 1u;
            if (ui.key_root) for (unsigned k = 0; k < NKEYS; ++k) if ((53u + k) % 12u + 1u == ui.key_root) klit |= 1u << k;
            kdim &= ~klit;
        }
    }
    if ((btn >> B_SAVE & 1u) || (btn >> B_EDIT & 1u)) {     /* SAVE or EDIT held: the presets on the white keys */
        klit = 0; kdim = 0; kdark = 0;
        for (unsigned w = 0; w < 16; ++w) if (pre_exist >> w & 1u) kdim |= 1u << WK[w];
    }
    if (btn >> B_ENV & 1u) {                                /* ENV held: as the mixer's keys, white 1-4 the mutes (a muted track full), black 1-4 the solos (the ones alone full) */
        int so = solo_track();
        klit = 0; kdim = 0; kdark = 0;
        for (unsigned t = 0; t < CH_TRACKS; ++t) {
            if (ch_track_get(t)->mute) klit |= 1u << WK[t]; else kdim |= 1u << WK[t];
            if (ui.solo_mask ? (ui.solo_mask >> t & 1u) : so == (int)t) klit |= 1u << BK[t]; else kdim |= 1u << BK[t];
        }
    }
    if (btn >> B_HOME & 1u) {                               /* HOME held: the patterns on the white keys */
        klit = 0; kdim = 0; kdark = 0;
        for (unsigned w = 0; w < 16; ++w) { if (w == ui.cur_pat) klit |= 1u << WK[w]; else if (pat_exist >> w & 1u) kdim |= 1u << WK[w]; }
    }
    if (ui.live_rec && !ui.kb_rec && ui.view != V_STEP && ui.view != V_SONG)
        for (unsigned t = 0; t < CH_TRACKS; ++t) { if (hit_on(t)) klit |= 1u << BK[t]; else kdim |= 1u << BK[t]; }
    if (sn->playing) lit |= ui.live_rec ? 1u << B_PLAY : LED_PLAY_GREEN;   /* red: recording */
    else if (sn->count && now < ui.msg_until - 1000u) lit |= 1u << B_PLAY;   /* counting in: a flash a beat */
    if (sn->locked[ui.track] || (ui.live_rec && sn->playing))
        lit |= 1u << B_REC;
    else if (ui.live_rec && (now / 250u) & 1u) lit |= 1u << B_REC;      /* armed: blinks until PLAY */
    if (ui.lock_rec) { if ((now / 120u) & 1u) lit |= 1u << B_REC; else lit &= ~(1u << B_REC); }   /* lock recording: a fast blink */
    if (ui.oct > 0) lit |= 1u << B_OCTUP; else if (ui.oct < 0) lit |= 1u << B_OCTDN;
    if (ui.mac_play) lit |= (1u << B_OCTUP) | (1u << B_OCTDN);   /* the macros: both OCT lit; assigning: both blinking, its key lit */
    if (ui.mac_arm) { if ((now / 250u) & 1u) lit |= (1u << B_OCTUP) | (1u << B_OCTDN); else lit &= ~((1u << B_OCTUP) | (1u << B_OCTDN)); }
    if (ui.gate_keys) {                                  /* the TRANCE gate's steps on the white keys: on full, off dim, past its LENGTH dark; the step sounding now turned over */
        static const float SB[6] = {0.125f, 1.0f / 6, 0.25f, 1.0f / 3, 0.5f, 1.0f};
        const float *gp = ch_track_get(ET)->param[ch_row(ui.slot, CH_DEV_TRANCE)];
        unsigned div = (unsigned)(gp[0] + 0.5f), len = 1u + (unsigned)(gp[8] + 0.5f), pat = 0;
        if (div > 5) div = 5;
        if (len > 16) len = 16;
        for (unsigned q = 0; q < 4; ++q) pat |= ((unsigned)(gp[4 + q] + 0.5f) & 15u) << (4 * q);
        int here = sn->playing ? (int)((unsigned)(sn->beat / SB[div]) % len) : -1;
        klit = 0; kdim = 0; kdark = 0;
        for (unsigned w = 0; w < len; ++w) { int on = pat >> w & 1u; if (on != ((int)w == here)) klit |= 1u << WK[w]; else kdim |= 1u << WK[w]; }
        if ((now / 250u) & 1u) lit |= 1u << B_FX; else lit &= ~(1u << B_FX);   /* FX blinks: the keys are its steps */
    }
    if (ui.oct_chord) { klit = 0; kdim = 0; kdark = 0; for (unsigned m = 0; m < CH_MACROS; ++m) { if (ui.mac_arm == m + 1) klit |= 1u << WK[m]; else kdim |= 1u << WK[m]; } }
    plat_lights(lit | btn, (klit | keys) & ~kdark, dim, kdim & ~kdark, 1);
    plat_lights_mid((1u << NB) - 1u, 0);                  /* every labelled button at mid, so its label reads; lit wins */
}
void app_input_only(void) { input(); }

/* ------------------------------------------------------------------- draw --- */
static void blit_logo(int32_t x, int32_t y, const uint16_t *px, int32_t n)
{
    for (int32_t j = 0; j < n; ++j)
        for (int32_t i = 0; i < n; ++i) cv_pset(x + i, y + j, px[j * n + i]);
}

static void tile(int32_t x, int32_t y, const char *enc, const char *name, const char *val, const char *mode,
                 float v, float live, int bipolar, int active)
{
    cv_rect(x, y, 78, 38, C_LINE);
    cv_rect(x + 1, y + 1, 76, 36, C_BLACK);
    if (!name) return;
    cv_text(x + 3, y + 2, &FONT_XS, name, active ? C_GRAY : C_DIM);
    if (text_w(&FONT_XS, name) + text_w(&FONT_XS, enc) < 68) cv_text(x + 75 - text_w(&FONT_XS, enc), y + 2, &FONT_XS, enc, C_DIM);
    cv_text(x + 3, y + 12, &FONT_B, val, active ? C_HI : C_DIM);
    if (mode && text_w(&FONT_B, val) + text_w(&FONT_XS, mode) < 68) cv_text(x + 75 - text_w(&FONT_XS, mode), y + 14, &FONT_XS, mode, C_WHITE);
    else if (mode) cv_text(x + 75 - text_w(&FONT_XS, mode), y + 22, &FONT_XS, mode, C_WHITE);
    /* the knob's value, and a tick where modulation has it now */
    int32_t bx = x + 3, bw = 72, by = y + 31;
    cv_rect(bx, by, bw, 4, C_LINE);
    float a = bipolar ? (v + 1) * 0.5f : v, b = bipolar ? (live + 1) * 0.5f : live;
    a = fm_clampf(a, 0, 1); b = fm_clampf(b, 0, 1);
    if (bipolar) {
        int32_t c = bx + bw / 2, e = bx + (int32_t)(a * (float)bw);
        cv_rect(c < e ? c : e, by, c < e ? e - c : c - e, 4, C_HI);
    } else cv_rect(bx, by, (int32_t)(a * (float)bw), 4, C_HI);
    cv_rect(bx + (int32_t)(b * (float)(bw - 2)), by - 1, 2, 6, C_ACC_HI);
}

/* ---- A trial: the tiles as knobs (app_ui_knobs; while it is off nothing below is reached). A dark cap in a black well,
 * a short white pointer near its rim, the value an arc round it (from the left, or from the top for a parameter with
 * a centre), the modulators' reach a thin teal arc outside; the knob's name bright in the corner, the parameter and
 * its value centred under it. Drawn a pixel at a time with its edges softened; no arctangent: a point is on an arc by
 * the side of the arc's two ends it lies on ---- */
static uint8_t ui_knobs = 2;      /* (on, on the ui-knobs branch: the old layout is main) */
static float knob_mod = -1;       /* a modulated tile: where its modulators have it now, 0..1 (set by the caller, for the one tile) */
static uint16_t knob_frame;       /* a tile's box in this colour (the PANEL's row on the knobs); 0: the usual line */
static uint16_t knob_fill;        /* (mock-up, ui_knobs 2) the tile filled with this colour, its text black, its arc white; 0: black as usual */
void app_ui_knobs(int on) { ui_knobs = (uint8_t)on; }
static void mock_row_engine(const ch_track_state *ts);
static void mock_row_fx(const ch_track_state *ts);
static int fxviz_on(void);
static int genviz_on(void);
static uint8_t fxviz_off;        /* SEQ + ENV on an effect's page: the tracks back under its knobs, its drawing put away */
static uint16_t dev_col(unsigned dev);
static uint16_t mix565(uint16_t a, uint16_t b, float t);
/* a colour at a fraction, for a fill: every channel cut to the same steps (green's extra bit dropped), so a dark tint
 * stays the hue it came from and does not turn green */
static uint16_t tint565(uint16_t c, float t)
{
    int32_t r = (int32_t)((float)(c >> 11) * t + 0.5f), g = (int32_t)((float)((c >> 5) & 63) * t * 0.5f + 0.5f), b = (int32_t)((float)(c & 31) * t + 0.5f);
    return (uint16_t)((r << 11) | (g << 6) | b);
}
/* a step's bar by its CHANCE: the track's colour at 100, fading into the cell under it as the chance falls (a trace left at 0) */
static uint16_t chance_col(uint16_t col, uint16_t bg, unsigned chance)
{
    return chance >= 100u ? col : mix565(col, bg, 0.85f * (float)(100u - chance) * 0.01f);
}
static uint16_t mix565(uint16_t a, uint16_t b, float t)
{
    if (t >= 1) return b;
    if (t <= 0) return a;
    int32_t q = (int32_t)(t * 256);
    int32_t r = (a >> 11) + ((((int32_t)(b >> 11) - (a >> 11)) * q) >> 8);
    int32_t g = ((a >> 5) & 63) + ((((int32_t)((b >> 5) & 63) - ((a >> 5) & 63)) * q) >> 8);
    int32_t bl = (a & 31) + ((((int32_t)(b & 31) - (a & 31)) * q) >> 8);
    return (uint16_t)((r << 11) | (g << 5) | bl);
}
typedef struct { float x0, y0, x1, y1; int wide; } arc_t;
static arc_t arc_of(float a0, float a1) { arc_t a = {fm_cosf(a0), fm_sinf(a0), fm_cosf(a1), fm_sinf(a1), a1 - a0 > 3.14159265f}; return a; }
static int arc_has(const arc_t *a, float px, float py)
{
    float c0 = a->x0 * py - a->y0 * px, c1 = px * a->y1 - py * a->x1;   /* past the start, before the end */
    return a->wide ? !(c0 < 0 && c1 < 0) : (c0 >= 0 && c1 >= 0);
}
static void knob_draw(int32_t cx, int32_t cy, float R, float frac, int bip, float mod, uint16_t col, int active)
{
    const float PI_ = 3.14159265f, W = 2.0f, RM = R - W * 0.5f, RC = R - W - 2.2f, A0 = 0.75f * PI_, SW = 1.5f * PI_;
    frac = fm_clampf(frac, 0, 1);
    float ang = A0 + SW * frac, as = bip ? 1.5f * PI_ : A0, ae = ang;
    if (ae < as) { float t = as; as = ae; ae = t; }
    if (bip && ae - as < 0.12f) { as = 1.5f * PI_ - 0.06f; ae = 1.5f * PI_ + 0.06f; }   /* at its centre: a sliver, so it reads as there */
    const float ca = fm_cosf(ang), sa = fm_sinf(ang);
    const arc_t val = arc_of(as, ae), trk = arc_of(A0, A0 + SW);
    int hasmod = mod >= 0; arc_t ma = trk;
    if (hasmod) { float ms = ang, me = A0 + SW * fm_clampf(mod, 0, 1); if (me < ms) { float t = ms; ms = me; me = t; } if (me - ms < 0.12f) me = ms + 0.12f; ma = arc_of(ms, me); }
    const uint16_t TRK = GREY5(5), WELL = RGB(6, 7, 9);
    if (!active) col = C_DIM;
    /* (25 frames a second, four knobs, 1500 pixels each: a pixel's distance from the centre is read from a table made
     * once, in eighths, and the two other roots are taken only within a pixel or two of what they draw) */
    static uint8_t DQ[24][24], dq_ok;                     /* (to 23: the knob at 17 and its modulation ring) */
    if (!dq_ok) { dq_ok = 1; for (int32_t j = 0; j < 24; ++j) for (int32_t i = 0; i < 24; ++i) DQ[j][i] = (uint8_t)(fm_sqrtf((float)(i * i + j * j)) * 8 + 0.5f); }
    int32_t r = (int32_t)R + 5;
    if (r > 23) r = 23;
    const float dmax = hasmod ? R + 4.5f : R + 1.1f;
    for (int32_t j = -r; j <= r; ++j)
        for (int32_t i = -r; i <= r; ++i) {
            const float px = (float)i, py = (float)j, d = (float)DQ[j < 0 ? -j : j][i < 0 ? -i : i] * 0.125f;
            if (d > dmax) continue;
            uint16_t c = knob_fill ? knob_fill : C_BLACK; int hit = 0;
            float k = R + 1.1f - d;                                      /* the well */
            if (k > 0) { c = mix565(c, WELL, k); hit = 1; }
            k = W * 0.5f + 0.5f - (d > RM ? d - RM : RM - d);            /* the ring: its track, the value on it */
            if (k > 0) {
                if (arc_has(&trk, px, py)) { c = mix565(c, TRK, k); hit = 1; }
                if (arc_has(&val, px, py)) { c = mix565(c, col, k); hit = 1; }
            }
            {   /* the value's end rounded, and where a knob at nothing still shows */
                float ex = px - RM * ca, ey = py - RM * sa;
                if (ex > -2 && ex < 2 && ey > -2 && ey < 2) {
                    float q = W * 0.5f + 0.5f - fm_sqrtf(ex * ex + ey * ey);
                    if (q > 0) { c = mix565(c, col, q); hit = 1; }
                }
            }
            if (hasmod) {
                float rm2 = R + 2.6f; k = 1.15f - (d > rm2 ? d - rm2 : rm2 - d);
                if (k > 0 && arc_has(&ma, px, py)) { c = mix565(c, C_MOD, k); hit = 1; }
            }
            k = RC + 0.5f - d;                                           /* the cap, lit from above; its pointer */
            if (k > 0) {
                int32_t g = (int32_t)(60 + 22 * (1 - d / RC) - 16 * py / RC);   /* (a touch flatter than a glossy cap: about three quarters of the shading) */
                if (!active) g = g * 5 / 8;
                if (g < 8) g = 8;
                if (g > 230) g = 230;
                c = mix565(c, GREY5(g >> 3), k); hit = 1;   /* (neutral, in whole steps: a bluish cap in finer steps of green came out blue and speckled) */
                float t = px * ca + py * sa, lo = RC * 0.42f, hi = RC * 0.92f;
                if (t < lo) t = lo;
                if (t > hi) t = hi;
                float dx = px - t * ca, dy = py - t * sa;
                if (dx > -1.4f && dx < 1.4f && dy > -1.4f && dy < 1.4f) {
                    float q = 1.35f - fm_sqrtf(dx * dx + dy * dy);
                    if (q > 0) c = mix565(c, active ? C_WHITE : C_GRAY, q);
                }
            }
            if (hit) cv_pset(cx + i, cy + j, c);
        }
}
static void text_mid(int32_t x, int32_t w, int32_t y, const felucca_font_t *f, const felucca_font_t *f2, const char *s, uint16_t c)
{
    char t[24]; unsigned k = 0;
    while (s[k] && k < 23) { t[k] = s[k]; ++k; }
    t[k] = 0;
    if (text_w(f, t) > w && f2) f = f2;
    if (text_w(f, t) > w) f = &FONT_T;                     /* (a long one: the smallest face before it is cut) */
    while (k > 1 && text_w(f, t) > w) t[--k] = 0;
    cv_text(x + (w - text_w(f, t)) / 2, y, f, t, c);
}
/* The panel the four knobs sit on, wherever there are four: a dark grey, its edge a colour (the track's on its own
 * pages, white on the line generator's, teal on the modulators', grey where the page is no track's) a line and a half
 * wide (a full row and one at two fifths), its corners rounded. 78 rows from y0 */
#define PANEL_GREY GREY5(1)
#define C_GLO RGB(176, 124, 208)                         /* the GLO pages: their panel, their arcs and their place in the list, the violet */
static void knob_panel(int32_t y0, uint16_t tc)
{
    static const uint8_t CUT[3] = {3, 1, 1};              /* (its corners a little rounder than square: they were cut four deep) */
    const int32_t TH = 78;
    const uint16_t gr = PANEL_GREY, tc2 = mix565(gr, tc, 0.4f);
    cv_rect(1, y0, 238, TH, tc); cv_rect(2, y0 + 1, 236, TH - 2, tc2); cv_rect(3, y0 + 2, 234, TH - 4, gr);
    for (int32_t q = 0; q < 3; ++q) {
        cv_rect(1, y0 + q, CUT[q], 1, C_BLACK); cv_rect(239 - CUT[q], y0 + q, CUT[q], 1, C_BLACK);
        cv_rect(1, y0 + TH - 1 - q, CUT[q], 1, C_BLACK); cv_rect(239 - CUT[q], y0 + TH - 1 - q, CUT[q], 1, C_BLACK);
    }
    for (int32_t e = 0; e < 2; ++e) {                      /* the line round each corner, and the half line inside it */
        int32_t ya = e ? y0 + TH - 2 : y0 + 1, yb = e ? y0 + TH - 3 : y0 + 2;
        cv_rect(2, ya, 2, 1, tc); cv_rect(236, ya, 2, 1, tc); cv_rect(2, yb, 1, 1, tc); cv_rect(237, yb, 1, 1, tc);
        cv_rect(3, yb, 1, 1, tc2); cv_rect(236, yb, 1, 1, tc2);
    }
}
static void tile_k(int32_t x, int32_t y, const char *enc, const char *name, const char *val, const char *mode, float v, int bipolar, int active)
{
    const int32_t w = 58, h = 78;
    if (knob_fill) {                                     /* (mock-up) the PANEL's row on the knobs: the track's colour as the outline, a quarter of it inside, the text white as on any tile */
        /* (the four share one panel, drawn by the PANEL before them: no box of its own) */
        cv_text(x + 2, y + 2, &FONT_T, enc, C_WHITE);   /* (the smallest face: a size up it ran into the knob) */
        if (!name) return;
        knob_draw(x + 29, y + 24, 17, bipolar ? (v + 1) * 0.5f : v, bipolar, knob_mod, knob_frame ? knob_frame : C_ACC, active);   /* (17, and three rows lower: the knob and its words in the panel's middle, the mode a smaller line under them) */
        text_mid(x, w - 1, y + 43, &FONT_XS, &FONT_T, name, active ? RGB(168, 170, 180) : C_DIM);
        text_mid(x, w - 1, y + 54, &FONT_XS, &FONT_T, val, active ? C_WHITE : C_DIM);   /* (the value in the name's face, white: it was the bold one, a size up) */
        if (mode) text_mid(x, w - 1, y + 66, &FONT_T, 0, mode, C_ACC);
        return;
    }
    cv_rect(x, y, w, h, knob_frame ? knob_frame : C_LINE);
    cv_rect(x + 1, y + 1, w - 2, h - 2, C_BLACK);
    cv_text(x + 2, y + 2, &FONT_T, enc, C_WHITE);   /* (the smallest face: a size up it ran into the knob) */
    if (!name) return;
    /* the arc in the track's colour on the pages that are a track's (its engine and effects, its line, its modulators);
     * orange, as it was, on those that are no track's (GLO, the master's, PERFORM) */
    int own = (ui.view == V_DEVICE && !ui.mfx) || ui.view == V_MOD || ui.view == V_GEN;
    knob_draw(x + 29, y + 24, 17, bipolar ? (v + 1) * 0.5f : v, bipolar, knob_mod, own ? TRACK_COL[ui.track] : C_ACC, active);
    text_mid(x, w - 1, y + 43, &FONT_XS, &FONT_T, name, active ? RGB(168, 170, 180) : C_DIM);
    text_mid(x, w - 1, y + 54, &FONT_XS, &FONT_T, val, active ? C_WHITE : C_DIM);
    if (mode) text_mid(x, w - 1, y + 66, &FONT_T, 0, mode, C_ACC);
}

/* a tile for one of K1..K4: 58 x 78, the knob's name, the parameter's (two lines if it must), the value (smaller
 * if it must), its mode in orange, the level bar with a tick where modulation has it now */
static void tile4(int32_t x, const char *enc, const char *name, const char *val, const char *mode,
                  float v, float live, int bipolar, int active)
{
    const int32_t w = 58, h = 78, iw = w - 6;
    if (ui_knobs) { tile_k(x, 0, enc, name, val, mode, v, bipolar, active); return; }
    cv_rect(x, 0, w, h, C_LINE);
    cv_rect(x + 1, 1, w - 2, h - 2, C_BLACK);
    cv_text(x + 3, 2, &FONT_XS, enc, C_DIM);
    if (!name) return;
    {   /* the name: one line, or broken at the last space that fits */
        char a[24], *b = 0; unsigned k = 0;
        while (name[k] && k < 23) { a[k] = name[k]; ++k; } a[k] = 0;
        if (text_w(&FONT_XS, a) > iw)
            for (int j = (int)k - 1; j > 0; --j) if (a[j] == ' ') { a[j] = 0; if (text_w(&FONT_XS, a) <= iw) { b = a + j + 1; break; } a[j] = ' '; }
        cv_text(x + 3, 13, &FONT_XS, a, active ? C_GRAY : C_DIM);
        if (b) cv_text(x + 3, 25, &FONT_XS, b, active ? C_GRAY : C_DIM);
    }
    const felucca_font_t *vf = text_w(&FONT_B, val) <= iw ? &FONT_B : &FONT_XS;
    cv_text(x + 3, 38, vf, val, active ? C_HI : C_DIM);   /* (the text clear of the two sliders at 67 and 72) */
    if (mode) cv_text(x + 3, 52, &FONT_XS, mode, C_ACC);
    int32_t bx = x + 3, bw = iw, by = h - 7;          /* the level bar: 5 high (thicker: it reads at a glance), a row clear of the modulation slider above it */
    cv_rect(bx, by, bw, 5, C_LINE);
    float a = bipolar ? (v + 1) * 0.5f : v, b = bipolar ? (live + 1) * 0.5f : live;
    a = fm_clampf(a, 0, 1); b = fm_clampf(b, 0, 1);
    if (bipolar) {
        int32_t c = bx + bw / 2, e = bx + (int32_t)(a * (float)bw);
        cv_rect(c < e ? c : e, by, c < e ? e - c : c - e, 5, C_HI);
    } else cv_rect(bx, by, (int32_t)(a * (float)bw), 5, C_HI);
    cv_rect(bx + (int32_t)(b * (float)(bw - 2)), by - 1, 2, 7, C_ACC_HI);
}

static const char *material_name(unsigned t)
{
    const ch_track_state *ts = ch_track_get(t);
    if (ts->bypass[0] || ts->device[0] == CH_DEV_BYPASS) return "--";
    if (ts->device[0] == CH_DEV_DRUM) return ch_drum_names[model_of(t, CH_DEV_DRUM)];
    if (ts->device[0] == CH_DEV_SYNTH) return ch_synth_names[model_of(t, CH_DEV_SYNTH)];
    return ch_device_info(ts->device[0])->name;
}

/* the battery, top left on the text's rows (6..16): an outline, its terminal, and 0 to 3 cells (empty, a third, two
 * thirds, full: the base's own bands, which say what they know; a percentage sat at 100 long after it was not). Empty:
 * the outline in orange. On USB power the word. Returns where the next thing may start */
static int32_t battery_w(void) { int b = plat_battery(); return b == 4 ? text_w(&FONT_B, "USB") : b < 0 ? 0 : 23; }
#ifndef HDR_LOGO_LEFT
#define HDR_LOGO_LEFT 1                                  /* 1: the logo top left, the battery top right */
#endif
/* The output's level beside the battery: twelve slices two wide, the last two red (within 3 dB of the
 * ceiling), the rest green, the unlit ones a dark grey; the peak held a moment and let fall. meter 1: the level as it
 * is; 2 and up: that many slices less two lit, for a picture of it */
static uint8_t ui_meter = 1;
void app_ui_meter(int on) { ui_meter = (uint8_t)on; }
static void header_meter(int32_t x, float pk)
{
    static float held;
    float db = pk > 1e-5f ? 6.0206f * fm_log2f(pk) : -100.0f, u = fm_clampf((db + 42.0f) / 42.0f, 0, 1);
    held = u > held ? u : held - 0.03f;
    if (held < 0) held = 0;
    int32_t lit = ui_meter >= 2 ? (int32_t)ui_meter - 2 : (int32_t)(held * 12.0f + 0.5f);
    for (int32_t k = 0; k < 12; ++k)
        cv_rect(x + k * 3, 6, 2, 11, k < lit ? (k >= 10 ? RGB(240, 60, 48) : RGB(70, 214, 110)) : GREY5(4));
}
static int32_t header_battery(int32_t x)
{
    int b = plat_battery();
    if (b == 4) return cv_text(x, 4, &FONT_B, "USB", C_GRAY) + 8;
    if (b < 0) return x;
    const int32_t bw = 21, bh = 11, y = 6;
    uint16_t oc = b == 0 ? C_ACC : C_GRAY;
    cv_rect(x, y, bw, 1, oc); cv_rect(x, y + bh - 1, bw, 1, oc);
    cv_rect(x, y, 1, bh, oc); cv_rect(x + bw - 1, y, 1, bh, oc);
    cv_rect(x + bw, y + 3, 2, bh - 6, oc);
    for (int c = 0; c < b && c < 3; ++c) cv_rect(x + 2 + c * 6, y + 2, 5, bh - 4, C_GRAY);
    return x + bw + 2 + 7;
}
static int mixpage_on(void);
static void header(void)
{
    const ch_track_state *ts = ch_track_get(ui.track);
    const ch_snapshot *sn = ch_snapshot_get();
    /* (the effects' row starts at 38, two rows up: the header stops short of it there. Drawn to 40 it blacked the row's
     * top two lines every frame before the row was drawn over them, and they flickered) */
    const uint32_t HH = ui_knobs >= 2 && ui.view == V_DEVICE && (ui.mfx || !ui.lock_rec) && !mixpage_on() ? 38 : 40;
    cv_begin(240, HH, C_BLACK);
    uint32_t now = plat_ms();
    int mst = ui.view == V_DEVICE && ui.mfx;             /* the master chain's view: its boxes */
    if (mst) ts = ch_track_get(CH_MASTER);
    char t[24] = "T"; char n[12]; int_text(ui.track + 1, n); text_cat(t, n);
    if (mst) { t[0] = 0; text_cat(t, "MST"); }
    int32_t brx = 240 - 5 - battery_w();                /* (the battery's x, top right) */
    int32_t bx = HDR_LOGO_LEFT ? 3 + LOGO_SMALL + 6 : header_battery(6);
    int32_t rlim = HDR_LOGO_LEFT ? brx : 240 - LOGO_SMALL - 3;   /* where the right-hand mark starts */
    if (ui_meter && HDR_LOGO_LEFT) rlim = brx - 6 - 35;   /* (the meter, left of the battery: drawn with it below) */
    int32_t hx = cv_text(bx, 4, &FONT_B, t, !mst && hit_on(ui.track) ? TRACK_HIT[ui.track] : C_WHITE);   /* a hit: the track's colour */
    {   /* the pattern playing (and the one cued), the song's place */
        char pt[16] = "P"; int_text(ui.cur_pat + 1, n); text_cat(pt, n);
        if (sn->pattern_pending) text_cat(pt, "*");
        if (song.on && song.len) { text_cat(pt, " S"); int_text(song.pos + 1, n); text_cat(pt, n); }
        hx = cv_text(hx + 6, 4, &FONT_B, pt, sn->pattern_pending && (now / 250u) & 1u ? C_DIM : C_GRAY);
    }
    if (now < ui.msg_until) {
        cv_begin(240, HH, C_BLACK);                        /* the message takes the line, cut short of the logo */
        if (!HDR_LOGO_LEFT) header_battery(6);
        int32_t x = cv_text(cv_text(bx, 4, &FONT_B, t, C_WHITE) + 8, 4, &FONT_B, ui.msg[0], C_WHITE);
        char m1[32] = ""; text_cat(m1, ui.msg[1]);
        int32_t lim = rlim - 3;
        while (m1[0] && x + 8 + text_w(&FONT_B, m1) > lim) { unsigned n = 0; while (m1[n]) ++n; m1[n - 1] = 0; }
        cv_text(x + 8, 4, &FONT_B, m1, C_HI);
    } else {
        int_text((int32_t)(ch_tempo() + 0.5f), n);
        t[0] = 0; text_cat(t, n); text_cat(t, " BPM");
        hx = cv_text(hx + 8, 4, &FONT_B, t, C_GRAY);
        /* the transport and its flags flow on after the tempo, on the same line, each only where it fits */
        int32_t right = rlim - 5;
        /* the transport: a green triangle playing, a grey square stopped */
        if (hx + 10 + 9 <= right) {
            int32_t ix = hx + 10;
            if (sn->playing) for (int32_t c = 0; c < 9; ++c) { int32_t top = (c * 5 + 4) / 9; cv_rect(ix + c, 6 + top, 1, 11 - 2 * top, RGB(70, 210, 110)); }   /* rows 6..16, the text's */
            else cv_rect(ix, 7, 9, 9, C_DIM);
            hx = ix + 9;
        }
        const char *fl[2] = {ui.live_rec ? "REC" : 0, sn->metronome ? "MET" : 0};
        uint16_t fc[2] = {C_ACC_HI, C_GRAY};
        for (unsigned k = 0; k < 2; ++k) {
            if (!fl[k]) continue;
            int32_t w = text_w(&FONT_B, fl[k]);
            if (hx + 10 + w > right) break;
            hx = cv_text(hx + 10, 4, &FONT_B, fl[k], fc[k]);
        }
    }
    if (HDR_LOGO_LEFT) { blit_logo(3, 2, logo_small, LOGO_SMALL); header_battery(brx); if (ui_meter) header_meter(brx - 6 - 35, sn->master[0] * gain_now);   /* (what leaves the jack: the mix's peak after the MASTER knob, which is applied here in the app, past where the engine measures) */ }
    else blit_logo(240 - LOGO_SMALL - 3, 2, logo_small, LOGO_SMALL);
    /* the chain: the engine, then the four FX slots in order; the open one lit, empty or bypassed ones dim */
    if (ui_knobs >= 2 && mst) cv_text(6, 21, &FONT_B, "MASTER", mix565(C_ACC, C_WHITE, 0.25f));   /* the master chain: the EDIT page's header, its name where an engine's is, in cyan as its four places are */
    else if (ui_knobs >= 2) mock_row_engine(ts);
    else for (unsigned s = 0; s < CH_SLOTS; ++s) {
        int32_t x = 4 + (int32_t)s * 47;
        int open = ui.view == V_DEVICE && ui.slot == s;
        int empty = s && ts->device[s] == CH_DEV_BYPASS;
        uint16_t c = empty || (s && ts->bypass[s]) ? C_DIM : open ? C_WHITE : C_HI;
        cv_rect(x, 24, 45, 14, open ? C_LINE : C_BLACK);
        const char *nm = empty ? "+" : s && ts->bypass[s] ? "--" : !s && mst ? "MASTER" : !s && ts->device[0] == CH_DEV_BYPASS ? "OFF" : ch_device_info(ts->device[s])->name;
        char sh[8];
        if (text_w(&FONT_XS, nm) > 43) { unsigned k = 0; while (nm[k] && k < 4) { sh[k] = nm[k]; ++k; } sh[k] = 0; nm = sh; }   /* BITCRUSH: BITC */
        cv_text(x + (45 - text_w(&FONT_XS, nm)) / 2, 24, &FONT_XS, nm, c);   /* (inside its box, 24 to 37: at 27 the name hung out below it) */
    }
    cv_blit(0, 0);
}

/* (Mock-up, ui_knobs 2.) A colour for each kind of engine and each family of effect, none of them a track's: the
 * engines warm to cool (DRUM apricot, KIT sand, SYNTH sky, SAMPLE lilac, GRAIN mint); the effects by what they do:
 * filters green, drive and wear rose, movement violet, time and space blue, rhythm and ducking amber */
static uint16_t dev_col(unsigned dev)
{
    switch (dev) {
    case CH_DEV_DRUM: return TRACK_COL[0];              /* the five engines: the tracks' four colours and teal; OFF white */
    case CH_DEV_KIT: return TRACK_COL[1];
    case CH_DEV_SYNTH: return TRACK_COL[2];
    case CH_DEV_POLY: return TRACK_COL[3];
    case CH_DEV_GRAIN: return RGB(51, 178, 159);
    case CH_DEV_BYPASS: return C_WHITE;
    case CH_DEV_FILTER: case CH_DEV_DJ: case CH_DEV_COMB: case CH_DEV_DISPERSE: return RGB(130, 205, 130);
    case CH_DEV_DRIVE: case CH_DEV_TAPE_FX: case CH_DEV_VINYL: case CH_DEV_ERODE: case CH_DEV_RESO: return RGB(238, 120, 140);
    case CH_DEV_CHORUS: case CH_DEV_FLANGER: case CH_DEV_PHASER: case CH_DEV_TREMOLO: case CH_DEV_UNISON: return RGB(176, 128, 220);
    case CH_DEV_DELAY: case CH_DEV_ECHO: case CH_DEV_DUB: case CH_DEV_REVERB: case CH_DEV_SPRING: case CH_DEV_CRYSTAL: return RGB(100, 150, 230);
    case CH_DEV_SIDE: case CH_DEV_REPEAT: case CH_DEV_TRANCE: return RGB(245, 185, 70);
    default: return C_GRAY;
    }
}
/* the engine and its model on one line, the engine's name in its colour; a box behind them when the engine is what is open */
static void mock_row_engine(const ch_track_state *ts)
{
    unsigned dev = ts->device[0]; int open = ui.view == V_DEVICE && ui.slot == 0;
    const char *en = dev == CH_DEV_BYPASS ? "OFF" : ch_device_info(dev)->name, *mn = material_name(ui.track);
    const char *a = en, *b = mn;
    while (*a && *a == *b) { ++a; ++b; }
    int same = !*a && !*b;
    /* On black, no box: the engine's name in the bold face, its colour lifted a quarter toward white (a deep red or
     * orange alone is the hardest thing on the screen to read), the model beside it bold and white. The engine being
     * what is open: a white bar under its name, as an open effect's pill has */
    const felucca_font_t *f = text_w(&FONT_B, en) + (same ? 0 : 7 + text_w(&FONT_B, mn)) <= 200 ? &FONT_B : &FONT_XS;
    int32_t x = cv_text(6, f == &FONT_B ? 21 : 23, f, en, mix565(dev_col(dev), C_WHITE, 0.25f));
    (void)open;                                          /* (no bar under it: with no effect's bar lit, it is the engine that is open) */
    if (!same) cv_text(x + 7, f == &FONT_B ? 21 : 23, f, mn, C_WHITE);
}
/* the four effects, a row of their own under it: each its family's colour, the open one a filled chip, an empty place a dim plus */
static void mock_row_fx(const ch_track_state *ts)
{
    for (unsigned sl = CH_FX1; sl < CH_SLOTS; ++sl) {
        int32_t x = 4 + (int32_t)(sl - CH_FX1) * 50;
        unsigned dev = ts->device[sl]; int open = ui.slot == sl, empty = dev == CH_DEV_BYPASS, byp = ts->bypass[sl];
        const char *nm = empty ? "+" : ch_device_info(dev)->name;
        char sh[8];
        if (text_w(&FONT_XS, nm) > 44) { unsigned k = 0; while (nm[k] && k < 4) { sh[k] = nm[k]; ++k; } sh[k] = 0; nm = sh; }
        /* a pill for each place, the row's whole 16: solid in its place's colour, its name black;
         * the open one at full with a white bar under it, the others a little under full (never faded: they are read
         * too); a bypassed one grey, an empty place dark with a plus */
        uint16_t col = ui.mfx ? C_ACC : TRACK_COL[(sl - CH_FX1) & 3u];   /* (the master's four: cyan) */
        uint16_t body = empty ? GREY5(3) : byp ? GREY5(9) : col;   /* (every place at full, open or not, on its page or off it: they are there to be read. The open one: the white bar) */
        cv_rect(x + 2, 0, 43, 13, body); cv_rect(x + 1, 1, 45, 11, body); cv_rect(x, 2, 47, 9, body);
        if (open) cv_rect(x + 3, 14, 41, 2, C_WHITE);
        cv_text(x + (47 - text_w(&FONT_XS, nm)) / 2, -1, &FONT_XS, nm, empty ? C_GRAY : C_BLACK);   /* (one face for all four: the bold one fitted only the short names) */
    }
}

/* the STEP view: an info line, the 8 lanes (two bands of four), the chosen cell's values */
#define C_CELL GREY5(3)                                  /* (the grid's greys: neutral, and a step or two darker. On the device its gamma lifts a dark grey, and the bluish ones they were read as bright blue cells) */
#define C_BEAT GREY5(5)                                  /* steps 5-8 and 13-16: the lighter quarters */
#define C_CELL_SEL GREY5(4)                              /* the chosen lane / track: the cells a quarter brighter */
#define C_BEAT_SEL GREY5(6)
#define C_PLAY GREY5(11)
#define C_SOFT RGB(122, 62, 18)
#define C_PAST GREY5(1)                                  /* steps past the pattern's length: a ghost of a cell, half what it was (it read as an empty step on the panel) */
static uint16_t cell_col(int sel, int play, unsigned c) { return play ? C_PLAY : (c & 4) ? (sel ? C_BEAT_SEL : C_BEAT) : sel ? C_CELL_SEL : C_CELL; }
static void lane_band(unsigned band)
{
    const ch_seq_state *sq = ch_seq_get(ui.track);
    const ch_snapshot *sn = ch_snapshot_get();
    char v[8];
    cv_begin(240, 72, C_BLACK);
    for (unsigned r = 0; r < 4; ++r) {
        unsigned lane = (band ? 3u : 7u) - r;
        int32_t y = (int32_t)r * 18;
        int sel = lane == ui.sel_lane, sounding = (sn->lanes[ui.track] >> lane & 1u) != 0;
        note_text(sq->note[lane], v);
        if (ch_track_get(ui.track)->device[CH_MATERIAL] == CH_DEV_KIT) {   /* KIT: the lane's part, by its drum's first word */
            unsigned m = (unsigned)(ch_track_get(ui.track)->param[CH_KIT_XROW][lane] + 0.5f); const char *nm = ch_drum_names[m < CH_DRUM_MODELS ? m : 0];
            unsigned k = 0; while (nm[k] && nm[k] != ' ' && k < 7) { v[k] = nm[k]; ++k; } v[k] = 0;
            while (k > 1 && text_w(&FONT_XS, v) > 24) v[--k] = 0;
        }
        if (ui.lane_edit && sel && (plat_ms() / 250u & 1u)) { cv_rect(0, y + 1, 27, 16, C_WHITE); cv_text(2, y + 3, &FONT_XS, v, C_BLACK); }   /* LANE NOTES: the lane being set blinks */
        else cv_text(2, y + 3, &FONT_XS, v, sounding ? TRACK_HIT[ui.track] : sel ? C_WHITE : C_GRAY);
        for (unsigned c = 0; c < 16; ++c) {
            unsigned idx = ui.step_page * 16u + c;
            int32_t x = 29 + (int32_t)c * 13;   /* (every gap alike; steps 5-8 and 13-16 sit on a lighter grey instead) */
            const ch_step *s = &sq->step[lane][idx];
            int play = sn->step[ui.track] == (int)idx;
            if (idx >= sq->len) { cv_rect(x, y + 1, 12, 16, C_PAST); continue; }
            cv_rect(x, y + 1, 12, 16, cell_col(sel, play, c));
            if (s->vel) {
                int32_t h = 4 + (int32_t)s->vel * 12 / 127;
                uint16_t col = chance_col(TRACK_COL[ui.track], cell_col(sel, play, c), s->chance);   /* chance: fading with it (the ratchet's parts are the TRACKS view's: these bars are too small for them) */
                int32_t mx = s->micro / 25;
                if (s->tie) tie_fill(x, y + 17 - h, 12, h, col);                 /* tied: the whole cell across, chequered */
                else {
                    int32_t bw = 2 + ((int32_t)ch_step_gate(s) * 8 + 50) / 100;
                    cv_rect(x + 1 + mx, y + 17 - h, bw, h, col);   /* as wide as its GATE */
                    unsigned sv = ch_step_send(s);
                    if (ch_track_get(ui.track)->device[CH_MATERIAL] == CH_DEV_KIT)   /* a KIT's send is the whole kit's at that step: every drum on it is shaded, as high as the most any of them sends */
                        for (unsigned l2 = 0; l2 < CH_LANES; ++l2) { const ch_step *o = &sq->step[l2][idx]; if (o->vel && ch_step_send(o) > sv) sv = ch_step_send(o); }
                    if (sv) {                                   /* SEND: fine stripes from the bottom, as high as what this hit adds to the send (none: a plain bar) */
                        int32_t sh = (h * (int32_t)sv + 7) / 15;
                        uint16_t bg = cell_col(sel, play, c);
                        for (int32_t py = 1; py < sh; py += 2) cv_rect(x + 1 + mx, y + 16 - py, bw, 1, bg);
                    }
                }
                {   /* its octave: a white pip an octave down the cell's right edge, from the top (up) or from the bottom (down), each in a black box */
                    int o = ch_step_oct(s), n = o < 0 ? -o : o;
                    for (int q = 0; q < n; ++q) { int32_t py = o > 0 ? y + 2 + q * 3 : y + 14 - q * 3; cv_rect(x + 8, py - 1, 4, 4, C_BLACK); cv_rect(x + 9, py, 2, 2, C_WHITE); }   /* (each on black: read on any track's colour, the near white one too) */
                }
                if (s->cyc_b) cv_rect(x + 1, y + 1, 10, 1, C_WHITE);
            }
            if (ch_lock_any(ui.track, idx)) {               /* a parameter lock on the step: a black dot on its bars; on a step with no hit, one white dot on the chosen lane */
                int any = 0; for (unsigned k = 0; k < CH_LANES; ++k) any |= sq->step[k][idx].vel != 0;
                if (s->vel) cv_rect(x + 8, y + 13, 3, 3, C_BLACK); else if (!any && sel) cv_rect(x + 8, y + 13, 3, 3, C_WHITE);
            }
            if (ui.lock_rec && ui.lock_track == ui.track && lock_armed(idx) && r == 0) cv_rect(x, y + 1, 12, 2, C_ACC);   /* armed: a mark over the lane */
            if (sel && ui.sel_step == (int)idx) {            /* the chosen cell */
                cv_rect(x, y + 1, 12, 1, C_WHITE); cv_rect(x, y + 16, 12, 1, C_WHITE);
                cv_rect(x, y + 1, 1, 16, C_WHITE); cv_rect(x + 11, y + 1, 1, 16, C_WHITE);
            }
        }
    }
    cv_blit(0, band ? 126 : 54);
}
/* the values under the knobs: the chosen cell's (K1..K4, PRESET), or the track's while SEQ is held (the TRACKS
 * view: TEMPO on ALGO) */
static void macro_strip(int32_t y0, int32_t h);
static int mac_shown(void);
static void step_tiles(const ch_seq_state *sq)
{
    if (mac_shown()) { macro_strip(198, 42); return; }
    char v[16];
    cv_begin(240, 42, C_BLACK);
    static const char *const SL[5] = {"VEL", "RATCH", "PROB", "MICRO", "CYCLE"};
    static const char *const SEND_NAME[4] = {"SEND", "SEND>M2", "SEND>M3", "SEND>M4"};   /* the steps' SEND, and the one send it acts on */
    static const char *const TL[5] = {"LEN", "RATE", "DIR", "SWING", "SCALE"};
    static const char *const KL[5] = {"K1", "K2", "K3", "K4", "PRE"};   /* (SEQ held: SCALE / TEMPO is ALG's) */
    int seq = (int)(ui.btn >> B_SEQ & 1u);
    const ch_step *s = ui.sel_step >= 0 ? &sq->step[ui.sel_lane][ui.sel_step] : 0;
    for (unsigned i = 0; i < 5; ++i) {
        int32_t x = 1 + (int32_t)(i == 4 ? 0 : i + 1) * 48;   /* (the fifth, PRESET's and ALGORITHM's, at the left: those knobs are on the panel's left, K1 to K4 on its right) */
        cv_rect(x, 0, 46, 42, C_LINE);
        cv_rect(x + 1, 1, 44, 40, C_BLACK);
        if (i == 4 && !seq && !(ui.lane_edit && ui.view == V_STEP)) {   /* the fifth: no setting, a note of where the other four are (it named the track, which the top line does) */
            /* a note, not a setting: no tile round it, grey, the two buttons drawn as the keys they are */
            const char *oc = ui.step_alt ? "OCT-" : "OCT+";
            cv_rect(x, 0, 46, 42, C_BLACK);
            cv_text(x + 1, 0, &FONT_T, "MORE:", C_GRAY);
            cv_rect(x + 1, 13, 25, 13, C_GRAY); cv_rect(x + 2, 14, 23, 11, C_BLACK); cv_text(x + 1 + (25 - text_w(&FONT_T, "EDIT")) / 2, 14, &FONT_T, "EDIT", C_GRAY);
            cv_text(x + 29, 14, &FONT_T, "+", C_GRAY);
            cv_rect(x + 1, 28, 25, 13, C_GRAY); cv_rect(x + 2, 29, 23, 11, C_BLACK); cv_text(x + 1 + (25 - text_w(&FONT_T, oc)) / 2, 29, &FONT_T, oc, C_GRAY);
            continue;
        }
        cv_text(x + 3, 1, &FONT_T, seq && i == 4 ? "ALG" : ui.lane_edit && ui.view == V_STEP && !seq && i == 1 ? "SEL" : KL[i], C_DIM);   /* its knob, small, above the name */
        v[0] = 0;
        int lit = 1;
        if (ui.lane_edit && ui.view == V_STEP && !seq) {   /* LANE NOTES: K1 the lane's note, the others at rest */
            if (i == 0) { cv_text(x + 3, 11, &FONT_XS, "NOTE", C_GRAY); note_text(sq->note[ui.sel_lane], v); }
            else if (i == 1) { cv_text(x + 3, 11, &FONT_XS, "LANE", C_GRAY); int_text(ui.sel_lane + 1, v); }
            else continue;
        } else if (seq) {
            cv_text(x + 3, 11, &FONT_XS, i == 4 && ui.view == V_SONG ? "TEMPO" : TL[i], C_GRAY);
            if (i == 4 && ui.view == V_SONG) int_text((int32_t)(ch_tempo() + 0.5f), v);
            else if (i == 0) int_text(sq->len, v);
            else if (i == 1) text_cat(v, RATE_NAME[sq->rate]);
            else if (i == 2) text_cat(v, DIR_NAME[sq->dir]);
            else if (i == 3) { int_text((int32_t)(50 + ch_swing() * 25 + 0.5f), v); text_cat(v, "%"); }
            else { const char *n = ui.key_root && !ui.tkey[ui.track] ? "KEY" : SCALE_NAME[ui.scale[ui.track]]; unsigned k = 0; while (n[k] && k < 5) { v[k] = n[k]; ++k; } v[k] = 0; }
        } else {
            unsigned k = ui.step_alt && (i == 2 || i == 3) ? (i == 2 ? 4u : 5u) : ui.step_alt && i == 1 ? 6u : ui.step_alt && i == 0 ? 7u : i;   /* the second page: OCTAVE, SEND, CYCLE, GATE */
            cv_text(x + 3, 11, &FONT_XS, i == 4 ? "TRACK" : k == 7 ? "OCT" : k == 6 ? SEND_NAME[ch_track_get(ui.track)->ss_dest > CH_SS_FX && ch_track_get(ui.track)->ss_dest <= CH_SS_LAST ? ch_track_get(ui.track)->ss_dest - CH_SS_FX : 0] : k == 5 ? "GATE" : SL[k], k != i ? C_MOD : C_GRAY);   /* (the second page's names in teal) */
            if (i == 4) { lit = 1; v[0] = 'T'; v[1] = (char)('1' + ui.track); v[2] = 0; }
            else if (s) {
                lit = s->vel != 0;
                if (k == 7) { int o = ch_step_oct(s); if (o > 0) text_cat(v, "+"); char n[4]; int_text(o, n); text_cat(v, n); }
                else if (i == 0) { if (s->vel) int_text(s->vel, v); else text_cat(v, "OFF"); }
                else if (k == 6) { unsigned f = ch_step_send(s); int_text((int32_t)((f * 100 + 7) / 15), v); text_cat(v, "%"); if (s->vel) cv_rect(x + 3, 38, (int32_t)f * 40 / 15, 2, C_MOD); }   /* SEND: its bar */
                else if (i == 1) { if (s->tie) text_cat(v, "TIE"); else ratchet_text(s->ratchet, v); }
                else if (k == 5) { int_text((int32_t)ch_step_gate(s), v); text_cat(v, "%"); if (s->vel) cv_rect(x + 3, 38, (int32_t)ch_step_gate(s) * 40 / 100, 2, C_MOD); }   /* GATE: its bar sideways */
                else if (k == 2) { int_text(s->chance, v); text_cat(v, "%"); }
                else if (k == 3) int_text(s->micro, v);
                else { if (s->cyc_b) { int_text(s->cyc_a, v); text_cat(v, ":"); char b[4]; int_text(s->cyc_b, b); text_cat(v, b); } else text_cat(v, "-"); }
            }
        }
        cv_text(x + 3, 25, text_w(&FONT_B, v) <= 40 ? &FONT_B : &FONT_XS, v, lit ? C_HI : C_DIM);
    }
    cv_blit(0, 198);
}

static void body_steps(void)
{
    const ch_seq_state *sq = ch_seq_get(ui.track);
    if (ui.step_page * 16u >= sq->len) ui.step_page = 0;  /* the length shrank under the page */
    char t[48], v[16];
    cv_begin(240, 14, C_BLACK);
    t[0] = 0; text_cat(t, material_name(ui.track)); text_cat(t, "  ");
    if (ui.kb_live) text_cat(t, "KEYS: PLAY  ");
    if (ui.lane_edit) { t[0] = 0; text_cat(t, "LANE NOTES  "); }
    if (keyed(ui.track)) { text_cat(t, ui.tkey[ui.track] ? "T-KEY " : "KEY "); text_cat(t, KEY_NAME[keyed(ui.track)]); text_cat(t, " "); }
    text_cat(t, SCALE_NAME[eff_scale(ui.track)]);
    cv_text(4, 1, &FONT_XS, t, C_WHITE);
    t[0] = 0; int_text(ui.step_page + 1, v); text_cat(t, v); text_cat(t, "/4  ");
    int_text(sq->len, v); text_cat(t, v); text_cat(t, " "); text_cat(t, RATE_NAME[sq->rate]); text_cat(t, " "); text_cat(t, DIR_NAME[sq->dir]);
    cv_text(236 - text_w(&FONT_XS, t), 1, &FONT_XS, t, C_GRAY);
    cv_blit(0, 40);
    lane_band(0);
    lane_band(1);
    step_tiles(sq);
}
/* the TRACKS view: the four tracks' patterns, a row each (lanes merged), the chosen track's row lit */
static void body_song(void)
{
    const ch_snapshot *sn = ch_snapshot_get();
    const ch_seq_state *sq = ch_seq_get(ui.track);
    if (ui.step_page * 16u >= sq->len) ui.step_page = 0;
    char t[48], v[16];
    cv_begin(240, 14, C_BLACK);
    t[0] = 0; text_cat(t, "TRACKS  T"); { char d[4] = {(char)('1' + ui.track), 0}; text_cat(t, d); } text_cat(t, " "); text_cat(t, material_name(ui.track));
    if (ui.kb_live) text_cat(t, "  KEYS: PLAY");
    cv_text(4, 1, &FONT_XS, t, C_WHITE);
    t[0] = 0; int_text(ui.step_page + 1, v); text_cat(t, v); text_cat(t, "/4  ");
    int_text(sq->len, v); text_cat(t, v); text_cat(t, " "); text_cat(t, RATE_NAME[sq->rate]); text_cat(t, " "); text_cat(t, DIR_NAME[sq->dir]);
    cv_text(236 - text_w(&FONT_XS, t), 1, &FONT_XS, t, C_GRAY);
    cv_blit(0, 40);
    for (unsigned band = 0; band < 2; ++band) {
        cv_begin(240, 72, C_BLACK);
        for (unsigned r = 0; r < 2; ++r) {
            unsigned tr = band * 2 + r;
            const ch_seq_state *q = ch_seq_get(tr);
            int32_t y = (int32_t)r * 36;
            int sel = tr == ui.track;
            char n[4] = {'T', (char)('1' + tr), 0};
            if (sel) {                                   /* the track being edited: its number boxed in white, at full colour */
                cv_rect(1, y + 7, 24, 1, C_WHITE); cv_rect(1, y + 27, 24, 1, C_WHITE);
                cv_rect(1, y + 7, 1, 21, C_WHITE); cv_rect(24, y + 7, 1, 21, C_WHITE);
            }
            cv_text(4, y + 10, &FONT_B, n, hit_on(tr) ? C_WHITE : TRACK_COL[tr]);   /* (every track's name at full, chosen or not: the chosen one has the box) */
            if (ch_track_get(tr)->mute) cv_text(6, y + 26, &FONT_T, "MUTE", C_GRAY);
            for (unsigned c = 0; c < 16; ++c) {
                unsigned idx = ui.step_page * 16u + c;
                int32_t x = 29 + (int32_t)c * 13;   /* (every gap alike; steps 5-8 and 13-16 sit on a lighter grey instead) */
                int play = sn->step[tr] == (int)idx;
                if (idx >= q->len) { cv_rect(x, y + 2, 12, 32, C_PAST); continue; }
                cv_rect(x, y + 2, 12, 32, cell_col(sel, play, c));
                unsigned top = 0, rat = 1, gate = 0, snd = 0, chn = 0; int cyc = 0, tie = 0;
                for (unsigned l = 0; l < CH_LANES; ++l) {
                    const ch_step *st = &q->step[l][idx];
                    if (!st->vel) continue;
                    if (st->vel > top) top = st->vel;
                    if (st->ratchet > rat) rat = st->ratchet;
                    if (ch_step_gate(st) > gate) gate = ch_step_gate(st);
                    if (ch_step_send(st) > snd) snd = ch_step_send(st);
                    if (st->chance > chn) chn = st->chance; cyc |= st->cyc_b != 0; tie |= st->tie != 0;
                }
                if (top) {
                    int32_t h = 6 + (int32_t)top * 24 / 127;
                    if (!tie && h < ratchet_min(rat)) h = ratchet_min(rat);
                    const uint16_t bar = chance_col(TRACK_COL[tr], cell_col(sel, play, c), chn);   /* the likeliest hit's chance: fading with it */
                    if (tie) tie_fill(x, y + 33 - h, 12, h, bar);   /* tied: the whole cell across, chequered */
                    else cv_rect(x + 1, y + 33 - h, 2 + ((int32_t)gate * 8 + 50) / 100, h, bar);   /* as wide as the longest GATE */
                    if (snd && !tie) {                          /* SEND: fine stripes from the bottom, as high as the most a hit here sends */
                        int32_t sh = (h * (int32_t)snd + 7) / 15; uint16_t bg = cell_col(sel, play, c);
                        for (int32_t py = 1; py < sh; py += 2) cv_rect(x + 1, y + 32 - py, 2 + ((int32_t)gate * 8 + 50) / 100, 1, bg);
                    }
                    if (!tie) ratchet_cuts(x + 1, y + 33, 2 + ((int32_t)gate * 8 + 50) / 100, h, rat, bar, cell_col(sel, play, c));
                    if (cyc) cv_rect(x + 1, y + 2, 10, 1, C_WHITE);
                }
                if (ch_lock_any(tr, idx)) cv_rect(x + 8, y + 29, 3, 3, top ? C_BLACK : C_WHITE);   /* a parameter lock on the step: black on the bar, white on an empty cell */
                if (sel && ui.lock_rec && ui.lock_track == tr && lock_armed(idx)) { cv_rect(x, y + 2, 12, 2, C_ACC); cv_rect(x, y + 32, 12, 2, C_ACC); }
                if (sel && ui.sel_step == (int)idx) {
                    cv_rect(x, y + 2, 12, 1, C_WHITE); cv_rect(x, y + 33, 12, 1, C_WHITE);
                    cv_rect(x, y + 2, 1, 32, C_WHITE); cv_rect(x + 11, y + 2, 1, 32, C_WHITE);
                }
            }
        }
        cv_blit(0, band ? 126 : 54);
    }
    step_tiles(sq);
}


/* ---- the spectrum (GLO + HOME): a 3D spectrogram drawn as vector lines ----
 * Frequency runs across, 60 Hz to 9 kHz; time runs back: a new line at the front every 40 ms, the oldest leaving at the
 * back, each line the spectrum of its moment, as high as it is loud. Solid: each strip hides what runs behind it. Each
 * stretch takes the colour of its height on the ramp, cream low through teal, yellow and orange to red, in lines a
 * pixel wide; the back a little dimmer, the newest line brightest. K1 SOURCE: the master as heard,
 * or one track. K2 VIEW: from front-on, ridgelines stacked up the screen, turned round to 40 degrees.
 * The bands: a Goertzel filter each on the engine's ring (ch_scope_data), its window shorter as the band is higher. */
enum { SP_LINES = 30, SP_BINS = 96, SP_EVERY = 3 };   /* 96 points across; a line back in time at every third: 32 frequency lines */
static uint8_t sp_hist[SP_LINES][SP_BINS] PLAT_POOL;    /* the levels, 0..255; [sp_at] the newest */
static float sp_coef[SP_BINS], sp_ref = 0.05f;
static uint16_t sp_len[SP_BINS];
static uint8_t sp_at;
static uint32_t sp_last;
static const char *const SP_SRC[CH_TRACKS + 1] = {"MASTER", "T1", "T2", "T3", "T4"};
static const uint8_t SP_RAMP[5][3] = {{255, 92, 61}, {255, 132, 36}, {221, 220, 1}, {51, 178, 159}, {228, 230, 186}};   /* THEME, low to high: (a warm charcoal
                                                   * at ground level), T1 red, the accent orange, T2 yellow, T4 teal, T3 cream on the peaks */
static const char *const SP_PAL[2] = {"THEME", "MONO"};
static const char *const SP_TILT[4] = {"FLAT", "3 DB / OCT", "4.5 DB / OCT", "6 DB / OCT"};
static void sp_reset(void)
{
    for (unsigned i = 0; i < SP_BINS; ++i) {
        float fc = 60.0f * fm_powf(150.0f, (float)i / (SP_BINS - 1)), n = 8.0f * 22050.0f / fc;   /* narrow: a quarter of the band's frequency, so harmonics stand apart */
        sp_len[i] = (uint16_t)fm_clampf(n, 16, CH_SCOPE_N);
        sp_coef[i] = 2 * fm_cosf(6.2831853f * fc / 22050.0f);
    }
    for (unsigned k = 0; k < SP_LINES; ++k) for (unsigned i = 0; i < SP_BINS; ++i) sp_hist[k][i] = 0;
    sp_at = 0; sp_ref = 0.05f;
    ch_scope_source(ui.spec_src);
}
/* this moment's spectrum, as the newest line: each band against the recent loudest, over 36 dB */
static void sp_push(void)
{
    unsigned w; const int8_t *buf = ch_scope_data(&w);
    float amp[SP_BINS], top = 0;
    for (unsigned b = 0; b < SP_BINS; ++b) {
        unsigned n = sp_len[b], k = (w + CH_SCOPE_N - n) & (CH_SCOPE_N - 1);
        float s1 = 0, s2 = 0, cf = sp_coef[b], half = (float)(n - 1) * 0.5f;
        for (unsigned i = 0; i < n; ++i) {
            float d = (float)i - half, tw = 1 - (d < 0 ? -d : d) / (half + 1), win = tw * tw * (3 - 2 * tw);   /* a smoothstep window: Hann-like, low side lobes */
            float s0 = (float)buf[k] * (1.0f / 127) * win + cf * s1 - s2;
            s2 = s1; s1 = s0; k = (k + 1) & (CH_SCOPE_N - 1);
        }
        float p = s1 * s1 + s2 * s2 - cf * s1 * s2;
        amp[b] = 4 * fm_sqrtf(p > 0 ? p : 0) / (float)n;
        if (ui.spec_tilt) {                              /* TILT: the band's level times (its frequency / 1 kHz) ^ (dB an octave / 6.02); the bands are 60 Hz x 150 ^ (b / 95) */
            float sl = (float)(ui.spec_tilt + 1u) * 0.249f;   /* 3, 4.5, 6 dB an octave */
            amp[b] *= fm_powf(0.06f * fm_powf(150.0f, (float)b / (SP_BINS - 1)), sl);
        }
        if (amp[b] > top) top = amp[b];
    }
    sp_ref = top > sp_ref ? top : fm_maxf(0.004f, sp_ref + (top - sp_ref) * 0.01f);
    sp_at = (uint8_t)((sp_at + 1) % SP_LINES);
    for (unsigned b = 0; b < SP_BINS; ++b)
        sp_hist[sp_at][b] = amp[b] > 0 ? (uint8_t)(fm_clampf(1 + 6.0206f * fm_log2f(amp[b] / sp_ref) / 36.0f, 0, 1) * 255) : 0;
}
static uint16_t sp_col(float h, float f)              /* the ramp at height h (0..1), at f of its brightness */
{
    h = fm_clampf(h, 0, 1);
    if (ui.mono) {                                       /* MONO: one tone at every height (K4 TONE); only the depth dims it */
        float r, g, b; tone_rgb(ui.spec_tone, &r, &g, &b);
        return RGB((uint32_t)(r * f), (uint32_t)(g * f), (uint32_t)(b * f));
    }
    h *= 4; int i = (int)h; if (i > 3) i = 3; float t = h - (float)i;
    const uint8_t (*R)[3] = SP_RAMP;
    float r = (float)R[i][0] + ((float)R[i + 1][0] - R[i][0]) * t;
    float g = (float)R[i][1] + ((float)R[i + 1][1] - R[i][1]) * t;
    float b = (float)R[i][2] + ((float)R[i + 1][2] - R[i][2]) * t;
    {                                                    /* ground level a warm charcoal, the bottom 5 % of the height rising into the red (h is 0..4 here) */
        float u = fm_clampf(h * (1.0f / 0.2f), 0, 1);
        r = 70 + (r - 70) * u; g = 62 + (g - 62) * u; b = 56 + (b - 56) * u;   /* a warm charcoal */
    }
    return RGB((uint32_t)(r * f), (uint32_t)(g * f), (uint32_t)(b * f));
}
/* a line, as vectors do it, a pixel a step (halo: a 3 x 3 dot instead, kept for a glow should it come back) */
static void sp_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c, int halo)
{
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1, sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx - dy;
    for (unsigned n = 0; n < 600; ++n) {
        if (halo) cv_rect(x0 - 1, y0 - 1, 3, 3, c); else cv_pset(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int32_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}
static void body_scope(void)
{
    uint32_t now = plat_ms();
    if (now - sp_last >= 40u) { sp_last = now; sp_push(); }
    cv_begin(240, 14, C_BLACK);
    cv_text(6, 1, &FONT_XS, "SPECTRUM", C_WHITE);
    cv_text(234 - text_w(&FONT_XS, SP_SRC[ui.spec_src]), 1, &FONT_XS, SP_SRC[ui.spec_src], C_GRAY);
    { char tl[24] = "TILT "; text_cat(tl, SP_TILT[ui.spec_tilt & 3u]); cv_text(120 - text_w(&FONT_T, tl) / 2, 3, &FONT_T, tl, C_DIM); }   /* (ALGO turns it) */
    cv_blit(0, 40);
    /* the view: a point at frequency u, age v (0 now) and height h is O + A u + B v - C h; K2 turns it from front-on to the angle */
    /* the block turned on a turntable by K2, 0 to 40 degrees, and looked down on from a little above as it turns: frequency
     * runs along (cos, sin x drop), time back along (sin, -cos x drop); the two lengths fitted to the screen's width, the
     * front-right corner kept off the bottom */
    float yaw = ui.spec_view * 0.6981317f, k = fm_minf(1, ui.spec_view * (40.0f / 23.0f));   /* 40 degrees at full; the tilt settles by 23 */
    float cy = fm_cosf(yaw), sy = fm_sinf(yaw), drop = 1 - 0.48f * k, ratio = 0.53f + 0.15f * k;
    float lu = 228 / (cy + ratio * sy), lv = lu * ratio, cc = 48;
    float ax = lu * cy, ay = lu * sy * drop, bx = lv * sy, by = -lv * cy * drop;
    float ox = 6, oy = 234 - ay;
    static const int32_t BAND_Y[3] = {54, 134, 214}, BAND_H[3] = {80, 80, 26};
    for (unsigned bd = 0; bd < 3; ++bd) {
        cv_begin(240, (uint32_t)BAND_H[bd], C_BLACK);
        int32_t oyb = BAND_Y[bd], lo = oyb, hi = oyb + BAND_H[bd];
        int16_t px[SP_BINS], py[SP_BINS], qx[SP_BINS], qy[SP_BINS];   /* this line's points, and the line behind it */
        const uint8_t *behind = 0;
        for (int k = SP_LINES - 1; k >= 0; --k) {     /* the oldest first: each strip hides what is behind it */
            const uint8_t *lv = sp_hist[(sp_at + SP_LINES - (unsigned)k) % SP_LINES];
            float v = (float)k / (SP_LINES - 1), nr = 1 - v, f = 0.45f + 0.55f * fm_powf(nr, 1.2f);   /* the back dimmed, between the two mockups V and W */
            int32_t ymin = 9999, ymax = -9999;
            for (unsigned i = 0; i < SP_BINS; ++i) {
                float u = (float)i / (SP_BINS - 1), h = (float)lv[i] * (1.0f / 255);
                px[i] = (int16_t)(ox + ax * u + bx * v); py[i] = (int16_t)(oy + ay * u + by * v - cc * h * (1 - 0.15f * v));
                if (py[i] < ymin) ymin = py[i];
            }
            ymax = (int32_t)(oy + by * v + (ay > 0 ? ay : 0)) + 2;
            if (ymax >= lo && ymin < hi + 3) {        /* (a strip wholly outside this band: nothing to draw) */
                if (behind)                            /* the lines along time, to the strip behind: before this strip's face, which hides what runs behind it */
                    for (unsigned i = 0; i < SP_BINS; i += SP_EVERY) {
                        float h = ((float)lv[i] + (float)behind[i]) * (0.5f / 255);
                        sp_line(px[i], py[i] - oyb, qx[i], qy[i] - oyb, sp_col(h, f * 0.8f), 0);
                    }
                for (unsigned i = 0; i + 1 < SP_BINS; ++i)   /* the face: from the line down to the strip's floor, black (the front: filled) */
                    for (int32_t x = px[i]; x < px[i + 1]; ++x) {
                        float a = (float)(x - px[i]) / (float)(px[i + 1] - px[i]);
                        int32_t yt = (int32_t)((float)py[i] + ((float)py[i + 1] - (float)py[i]) * a);
                        int32_t yf = (int32_t)(oy + by * v + ay * ((float)x - ox - bx * v) / ax);
                        if (yf < yt) continue;
                        cv_rect(x, yt + 1 - oyb, 1, yf - yt, C_BLACK);
                    }
                if (!k)                                /* the front face: the lines along time carried straight down to the floor */
                    for (unsigned i = 0; i < SP_BINS; i += SP_EVERY) {
                        float u = (float)i / (SP_BINS - 1);
                        sp_line(px[i], py[i] - oyb, px[i], (int32_t)(oy + ay * u) - oyb, sp_col((float)lv[i] * (1.0f / 255), f * 0.8f), 0);
                    }
                for (unsigned i = 0; i + 1 < SP_BINS; ++i) {   /* the line, a pixel wide */
                    float h = ((float)lv[i] + (float)lv[i + 1]) * (0.5f / 255);
                    sp_line(px[i], py[i] - oyb, px[i + 1], py[i + 1] - oyb, sp_col(h, f), 0);
                }
            }
            for (unsigned i = 0; i < SP_BINS; ++i) { qx[i] = px[i]; qy[i] = py[i]; }
            behind = lv;
        }
        cv_blit(0, (uint32_t)oyb);
    }
}
static void edit_scope(unsigned i, int32_t det)
{
    char v[12] = "";
    if (i == 0) { ui.spec_src = (uint8_t)clampi((int)ui.spec_src + (det > 0 ? 1 : -1), 0, CH_TRACKS); ch_scope_source(ui.spec_src); sp_ref = 0.05f; app_say("SOURCE", SP_SRC[ui.spec_src]); }
    else if (i == 1) {
        ui.spec_view = fm_clampf(ui.spec_view + (float)det * 0.05f, 0, 1);
        if (ui.spec_view < 0.01f) text_cat(v, "FRONT"); else if (ui.spec_view > 0.99f) text_cat(v, "ANGLE"); else int_text((int32_t)(ui.spec_view * 100 + 0.5f), v);
        app_say("VIEW", v);
    }
    else if (i == 2) { ui.mono = det > 0; app_say("COLOURS", SP_PAL[ui.mono]); }
    else if (i == 3) {                                   /* MONO's tone: a hue a detent, round, white after the last */
        ui.spec_tone = (uint8_t)(((int)ui.spec_tone + (det > 0 ? 1 : -1) + 25) % 25);
        if (ui.spec_tone == 24) text_cat(v, "WHITE"); else { int_text((int32_t)ui.spec_tone * 15, v); text_cat(v, " DEG"); }
        app_say("TONE", v);
    }
    else if (i == 4) {                                   /* ALGO: the TILT, FLAT or the highs lifted */
        ui.spec_tilt = (uint8_t)clampi((int)ui.spec_tilt + (det > 0 ? 1 : -1), 0, 3);
        sp_ref = 0.05f; app_say("TILT", SP_TILT[ui.spec_tilt]);
    }
    ui.redraw = 1;
}
/* one engine or effect parameter as the screen shows it: its name, value in words, mode, the knob and the value now */
typedef struct { const char *name, *md; char val[16]; float bv, lv; int bip, nmap; float mlo, mhi, mnow; } dev_cell;   /* nmap: modulators mapped; mlo..mhi their reach, mnow the value now (0..1 of the bar) */
static float side_src_frac(float v)
{
    int k = (int)(v + 0.5f), n = 0, at = 0;
    for (int j = 0; j < (int)(CH_TRACKS + CH_TRACKS * CH_KIT_PARTS); ++j) {
        if (j >= (int)CH_TRACKS && ch_track_get((unsigned)(j - (int)CH_TRACKS) / CH_KIT_PARTS)->device[CH_MATERIAL] != CH_DEV_KIT) continue;
        if (j <= k) at = n;
        ++n;
    }
    return n > 1 ? (float)at / (float)(n - 1) : 0;
}
static int dev_cell_at(int p, dev_cell *c)
{
    if (p < 0) return 0;
    const ch_track_state *ts = ch_track_get(ET);
    const ch_snapshot *sn = ch_snapshot_get();
    unsigned dev = ts->device[ui.slot];
    const ch_param_desc *pd = &ch_device_info(dev)->param[p];
    float v = ch_param_of(ts, ui.slot, dev, (unsigned)p), live = p >= (int)CH_PARAMS ? v : ui.mfx ? sn->mvalue[ui.slot][p] : sn->value[ui.track][ui.slot][p];
    int locked = 0;
    char *val = c->val;
    if (ui.lock_rec && !ui.mfx && ui.lock_track == ui.track) { int fs = lock_first(); if (fs >= 0 && ch_lock_get(ui.track, (unsigned)fs, ui.slot, (unsigned)p, &v)) locked = 1; }
    unsigned pmode = ch_mode_of(ts, ui.slot, dev, (unsigned)p);
    value_text(pd, v, pmode, val);
    if (pitch_range(ui.slot, dev, p)) pitch_text(v, pitch_range(ui.slot, dev, p), pmode, val);
    if (dev >= CH_DEV_FX_FIRST) { char fv[16]; if (ch_fx_value_text(dev, (unsigned)p, v, pmode, fv)) { val[0] = 0; text_cat(val, fv); } }
    crush_text(ET, ui.slot, dev, p, v, val);
    if (dev == CH_DEV_SYNTH && ui.slot == CH_MATERIAL) { char sv[16]; if (ch_synth_value_text(model_of(ui.track, dev), (unsigned)p, v, sv)) { val[0] = 0; text_cat(val, sv); } }
    if (locked) { char lv[16] = "L "; text_cat(lv, val); val[0] = 0; text_cat(val, lv); }
    if (dev == CH_DEV_SYNTH && p >= 2 && p <= 4 && model_of(ui.track, CH_DEV_SYNTH) >= CH_SYNTH_MODELS - CH_FE_MODELS) {
        const char *nm = ch_fe_value(model_of(ui.track, CH_DEV_SYNTH) - (CH_SYNTH_MODELS - CH_FE_MODELS), (unsigned)p - 2, v);
        if (nm) { val[0] = 0; text_cat(val, nm); }   /* a Felucca engine's stepped choice by name */
    }
    c->md = pd->modes ? pd->modes[pmode] : 0;
    if (dev == CH_DEV_SYNTH && ui.slot == CH_MATERIAL && p < (int)CH_PARAMS) { const char *const *mo = ch_synth_modes(model_of(ui.track, dev), (unsigned)p); if (mo) c->md = mo[pmode & 1u]; }
    c->lv = pd->kind == CH_SELECT ? live / (float)(pd->steps - 1) : live;
    c->bv = pd->kind == CH_SELECT ? v / (float)(pd->steps - 1) : v;
    if (dev == CH_DEV_SIDE && p == 0) { c->bv = side_src_frac(v); c->lv = side_src_frac(live); }   /* (the knob's whole turn over the sources there are) */
    c->bip = pd->kind == CH_BI;
    c->name = param_name(ET, ui.slot, (unsigned)p);
    {   /* the modulators mapped here: how far they can take it, on the bar (a map moves it depth x its output, the bar's whole width a
         * depth of 1, bipolar both ways, unipolar one way) */
        float base = c->bip ? (c->bv + 1) * 0.5f : c->bv, lo = 0, hi = 0;
        c->nmap = 0;
        if (p < (int)CH_PARAMS)
            for (unsigned k = 0; k < CH_MAPS; ++k) {
                float d = ts->map[k].depth;
                if (d == 0 || ts->map[k].slot != ui.slot || ts->map[k].param != (unsigned)p) continue;
                ++c->nmap;
                if (ts->map[k].bipolar) { float a = d < 0 ? -d : d; lo -= a; hi += a; } else if (d > 0) hi += d; else lo += d;
            }
        c->mlo = fm_clampf(base + lo, 0, 1); c->mhi = fm_clampf(base + hi, 0, 1);
        c->mnow = fm_clampf(c->bip ? (c->lv + 1) * 0.5f : c->lv, 0, 1);
    }
    return 1;
}
/* the modulation slider, just above a level bar and its size: teal from the knob to where all the modulators together have
 * it now (their sum, as the engine adds them), on the same track */

static void mod_bar(int32_t bx, int32_t bw, int32_t y, int32_t h, const dev_cell *c)
{
    float base = c->bip ? (c->bv + 1) * 0.5f : c->bv;
    base = fm_clampf(base, 0, 1);
    int32_t a = bx + (int32_t)(base * (float)bw), b = bx + (int32_t)(c->mnow * (float)bw);
    cv_rect(bx, y, bw, h, C_LINE);
    if (b == a) b = a + (c->mnow >= base ? 1 : -1);          /* (at rest: a sliver, so it reads as there) */
    cv_rect(a < b ? a : b, y, a < b ? b - a : a - b, h, C_MOD);
}
/* (Mock-up, ui_knobs 2 and up.) An effect's page: in place of the tracks under its knobs, a drawing of what it does, made
 * from its settings alone (no audio is read, no memory kept): a filter's response, a drive's curve, a sweep's wave, a
 * delay's echoes, a reverb's tail, a duck, a repeat's slices, a gate's steps. One curve, its family's colour, a quarter
 * of that under it, in a framed well with a faint grid. SEQ + ENV on the page brings the tracks back, and again the drawing */
static int fxviz_on(void)
{
    if (ui_knobs < 2 || fxviz_off || ui.view != V_DEVICE || ui.slot == CH_MATERIAL || mac_shown()) return 0;
    return ch_track_get(ET)->device[ui.slot] != CH_DEV_BYPASS;
}
static float fxv_wave(float t, unsigned shape)
{
    float fl = (float)(int)t; if (fl > t) fl -= 1;
    t -= fl;
    switch (shape) {
    case 1: return t < 0.5f ? 4 * t - 1 : 3 - 4 * t;
    case 2: return 2 * t - 1;
    case 3: return 1 - 2 * t;
    case 4: return t < 0.5f ? 1.0f : -1.0f;
    case 5: { uint32_t h = (uint32_t)(int32_t)fl * 2654435761u + 0x9E3779B9u; h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; return (float)(h & 0xFFFFu) * (2.0f / 65535.0f) - 1; }   /* (a value a cycle) */
    default: return fm_sinf(t * 6.2831853f);
    }
}
static float fxv_filter(float u, float fc, float k, unsigned type, int steep)
{
    float f = 20 * fm_exp2f(u * 9.97f), r = f / fc, r2 = r * r;
    float lp = 1 / fm_sqrtf((1 - r2) * (1 - r2) + k * k * r2 + 1e-9f);
    float m = type == 1 ? r2 * lp : type == 2 ? k * r * lp : type == 3 ? (r2 > 1 ? r2 - 1 : 1 - r2) * lp : lp;
    if (steep) m *= m;
    return fm_clampf((6.02f * fm_log2f(m + 1e-6f) + 36) * (1.0f / 52), 0, 1);
}
/* the ones that are one value at each point across: the filters, the two step effects, TAPE and VINYL */
static float fxv(unsigned dev, const ch_track_state *ts, unsigned sl, float u)
{
#define FP(k) ch_param_of(ts, sl, dev, k)
    switch (dev) {
    case CH_DEV_FILTER: return fxv_filter(u, 20 * fm_exp2f(FP(0) * 9.8f), 2 - 1.75f * FP(1), (unsigned)(FP(2) + 0.5f), FP(3) > 0.5f);
    case CH_DEV_DJ: { float a = FP(0); if (a > -0.02f && a < 0.02f) return 0.69f; return fxv_filter(u, 20 * fm_exp2f((a < 0 ? 1 + a : a) * 9.8f), 2 - 1.75f * FP(1), a < 0 ? 0 : 1, 0); }
    case CH_DEV_REPEAT: {
        static const float WN[6] = {0.5f, 0.667f, 1, 1.333f, 2, 4};
        float c = u * 16, w = WN[(unsigned)(FP(0) + 0.5f) % 6], st = FP(8);
        unsigned n = 1 + (unsigned)(FP(2) * 7.99f);
        if (c - (float)(int)c > 0.9f) return 0;
        if (c < st || c >= st + w * (float)(n + 1)) return 0.14f;
        unsigned k = (unsigned)((c - st) / w);
        if (((c - st) / w - (float)k) > FP(3) && k) return 0.14f;
        return k ? 0.8f * (1 - FP(5) * 0.11f * (float)k) : 1.0f;
    }
    case CH_DEV_TRANCE: {
        float c = u * 16; unsigned len = 1u + ((unsigned)(FP(8) + 0.5f) & 15u), i = ((unsigned)c & 15u) % len, m = (unsigned)(FP(4 + i / 4) + 0.5f);   /* (past LENGTH: round again) */
        float fr = c - (float)(int)c;
        if (fr > 0.92f) return 0;
        return (m >> (i & 3u) & 1u) && fr < 0.15f + 0.77f * FP(1) ? 0.9f : 0.9f * (1 - FP(3)) + 0.1f;
    }
    case CH_DEV_TAPE_FX: return 0.5f + 0.28f * FP(0) * fm_sinf(u * 9) + 0.06f * FP(0) * fm_sinf(u * 61);
    case CH_DEV_VINYL: { uint32_t h = (uint32_t)(u * 224) * 2246822519u; h ^= h >> 15; return 0.5f + ((h & 255u) < (uint32_t)(FP(0) * 40) ? ((h >> 8 & 1u) ? 0.35f : -0.35f) : 0.02f * fm_sinf(u * 300)); }
    default: return 0.5f;
    }
#undef FP
}
/* The plot: 224 columns from x 8, its top at row 6 of the 98 under the knobs; 70 rows where a ruler runs under it, 85
 * where none does. A drawing is made once a frame into three rows of numbers (one curve's top and bottom in each
 * column, so a curve that turns many times inside a column is a band there and not a moire; a second, fainter curve)
 * and drawn from them in the canvas's two bands */
enum { FX0 = 8, FXW = 224, FXT = 6 };
enum { FXK_FLOOR, FXK_MID, FXK_TAIL, FXK_BARS, FXK_VOICES };       /* filled from the floor; from the middle line; a tail, mirrored; echoes */
enum { FXA_NONE, FXA_HZ, FXA_SEC, FXA_BEAT };          /* the ruler: 100 1K 10K; seconds; beats */
typedef struct { uint8_t kind, axis; float span, mark; } fxv_plan;   /* span: seconds or beats across; mark: a pitch marked on the ruler (Hz) */
static uint8_t fxa_hi[FXW], fxa_lo[FXW], fxb[FXW];
static int32_t fx_ph;                                  /* the plot's rows */
static uint16_t fx_hits;                               /* SIDECHAIN: its source's hits across the bar */
static float fxv_map(float v, float lo, float hi) { return lo * fm_exp2f(fm_clampf(v, 0, 1) * fm_log2f(hi / lo)); }
static float fxv_hz(float u) { return 20 * fm_exp2f(u * 9.97f); }
static float fxv_db(float m) { return fm_clampf((6.02f * fm_log2f(m + 1e-6f) + 21) * (1.0f / 42), 0, 1); }   /* -21 .. +21 dB */
static uint32_t fx_grit;                               /* ERODE's drawing: the grain's seed (it changes eight times a second: a shimmer) */
static float fx_gritf(uint32_t a, uint32_t b)          /* 0 .. 1, the same for the same place and seed */
{
    uint32_t h = (a * 0x9E3779B1u) ^ (b * 0x85EBCA6Bu) ^ (fx_grit * 0xC2B2AE35u);
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 13;
    return (float)(h & 0xFFFFu) * (1.0f / 65536.0f);
}
static void fxa_put(int x, float v)
{
    int y = (int)((1 - fm_clampf(v, 0, 1)) * (float)(fx_ph - 1) + 0.5f);
    if (y < fxa_hi[x]) fxa_hi[x] = (uint8_t)y;
    if (y > fxa_lo[x]) fxa_lo[x] = (uint8_t)y;
}
static void fxb_put(int x, float v) { fxb[x] = (uint8_t)(1 + (int)((1 - fm_clampf(v, 0, 1)) * (float)(fx_ph - 1) + 0.5f)); }
/* a delay in a loop, as it answers at w (a turn of w from one peak to the next): fed back (kind 0), fed forward (1),
 * fed back and heard after the delay (2: the flanger); dry and wet its two parts in what is heard */
static float fxv_cmag(float w, float G, float dry, float wet, int kind)
{
    float c = fm_cosf(w), s = fm_sinf(w), re, im;
    if (kind == 1) { re = 1 + G * c; im = -G * s; }
    else {
        float dr = 1 - G * c, di = G * s, D = 1 / (dr * dr + di * di + 1e-9f);
        re = dr * D; im = -di * D;
        if (kind == 2) { float a = re * c + im * s, b = im * c - re * s; re = a; im = b; }
    }
    re = dry + wet * re; im = wet * im;
    return fm_sqrtf(re * re + im * im);
}
/* its peaks `per` Hz apart, across the plot: four points a column, and where a column holds half a turn or more, the
 * peak and the dip themselves. fc: the loop's low pass (0: none) */
static void fxv_combcols(float per, float G0, float fc, float dry, float wet, int kind)
{
    for (int x = 0; x < FXW; ++x) {
        float f0 = fxv_hz((float)x * (1.0f / FXW)), f1 = fxv_hz((float)(x + 1) * (1.0f / FXW)), fm = 0.5f * (f0 + f1);
        float G = fc > 0 ? G0 / fm_sqrtf(1 + (fm / fc) * (fm / fc)) : G0;
        if (f1 - f0 > per * 0.5f) { fxb[x] = 255;      /* (a band: drawn as one, not as a line) */
            fxa_put(x, fxv_db(fxv_cmag(0, G, dry, wet, kind))); fxa_put(x, fxv_db(fxv_cmag(3.14159265f, G, dry, wet, kind))); }
        else for (int q = 0; q < 4; ++q) fxa_put(x, fxv_db(fxv_cmag(6.2831853f * (f0 + (f1 - f0) * ((float)q + 0.5f) * 0.25f) / per, G, dry, wet, kind)));
    }
}
static int fxv_ruled(unsigned dev) { return dev != CH_DEV_DRIVE && dev != CH_DEV_TAPE_FX && dev != CH_DEV_VINYL && dev != CH_DEV_UNISON; }
static fxv_plan fxv_prepare(unsigned dev, const ch_track_state *ts, unsigned sl)
{
#define FP(k) ch_param_of(ts, sl, dev, k)
    fxv_plan pl = {FXK_FLOOR, FXA_NONE, 0, 0};
    const float SR = (float)CH_RATE;
    float ph = ch_fx_phase(ET, sl), bpm = ch_tempo(); if (bpm < 24) bpm = 120;
    for (int x = 0; x < FXW; ++x) { fxa_hi[x] = 255; fxa_lo[x] = 0; fxb[x] = 0; }
    switch (dev) {
    case CH_DEV_COMB: {                                  /* its real answer: a peak at the pitch and at every harmonic of it (on this ruler, closer and closer) */
        float hz = ch_fx_comb_hz(ET, sl), g = fm_clampf(FP(1), 0, 1), mix = FP(3), pol = FP(4) > 0.5f ? -1.0f : 1.0f; int ff = FP(5) > 0.5f;
        float gk = ff ? g * pol : 0.985f * g * pol, comp = ff ? 1 / (1 + 0.5f * g) : fm_sqrtf(1 - gk * gk);
        fxv_combcols(hz, gk, fxv_map(1 - FP(2), 700, 20000), 1 - mix, mix * comp, ff);
        pl.axis = FXA_HZ; pl.mark = hz; break;
    }
    case CH_DEV_RESO: {
        static const float HARM[4] = {1, 2, 3, 4}, STR[4] = {1, 2.41f, 4.07f, 6.3f}, GN[4] = {1, 0.6f, 0.42f, 0.3f};
        float hz = ch_fx_reso_hz(FP(0)), k = 2 - 1.96f * fm_clampf(FP(1), 0, 1), make = k * (1 + (1 - k) * 0.6f), sp = FP(2), mix = FP(3), fb[4];
        for (unsigned b = 0; b < 4; ++b) fb[b] = hz * (HARM[b] + (STR[b] - HARM[b]) * sp);
        for (int i = 0; i < FXW * 4; ++i) {
            float f = fxv_hz(((float)i + 0.5f) * (1.0f / (FXW * 4))), sum = 0;
            for (unsigned b = 0; b < 4; ++b) { float r = f / fb[b]; sum += GN[b] * r / fm_sqrtf((1 - r * r) * (1 - r * r) + k * k * r * r + 1e-9f); }
            fxa_put(i >> 2, fxv_db((1 - mix) + mix * make * sum));
        }
        pl.axis = FXA_HZ; pl.mark = hz; break;
    }
    case CH_DEV_ERODE: {                                 /* what shakes the delay: the band of noise at FREQ, as wide as WIDTH, as tall as AMOUNT; SINE: a line at FREQ.
                                                          * And what it does: the band's edge worn rough, the more the more AMOUNT (its fill eaten into: body_fxviz) */
        float hz = ch_fx_erode_hz(FP(0)), q = 1 / fxv_map(1 - FP(1), 0.5f, 12.0f), amt = FP(2); int sine = (int)(FP(3) + 0.5f) >= 2;
        const float sp = sine ? fm_exp2f(0.5f * fm_clampf(FP(1), 0, 1)) : 1;   /* SINE: WIDTH parts the line in two, the left side's below FREQ, the right's above */
        if (sine) q = 0.02f;
        for (int x = 0; x < FXW; ++x) {
            float top = 0, mm = 0;
            for (int i = 0; i < 4; ++i) {
                float r = fxv_hz(((float)(x * 4 + i) + 0.5f) * (1.0f / (FXW * 4))) / hz * sp, m = q * r / fm_sqrtf((1 - r * r) * (1 - r * r) + q * q * r * r + 1e-9f);
                if (sp > 1.0001f) { float r2 = r / (sp * sp), m2 = q * r2 / fm_sqrtf((1 - r2 * r2) * (1 - r2 * r2) + q * q * r2 * r2 + 1e-9f); if (m2 > m) m = m2; }
                float v = fxv_db(0.09f + 11.0f * amt * m);
                if (v > top) top = v;
                if (m > mm) mm = m;
            }
            fxa_put(x, top + (fx_gritf((uint32_t)x, 977u) - 0.5f) * 0.5f * amt * (0.15f + 0.85f * mm));
        }
        pl.axis = FXA_HZ; pl.mark = hz; pl.span = amt; break;   /* (span: on this one, how worn) */
    }
    case CH_DEV_DISPERSE: {                              /* an all-pass: the level a flat line (the middle one), and round it the turn it gives the sound, the phase of one
                                                          * stage: down to half a turn back at FREQ, where it wraps to half a turn on and comes home. PINCH: how sudden; AMOUNT
                                                          * (how many stages): how tall it is drawn */
        float hz = ch_fx_disp_hz(FP(0)), q = ch_fx_disp_q(FP(1)), ns = (float)ch_fx_disp_stages(FP(2));
        float tall = ns > 0 ? 0.25f + 0.75f * ns * (1.0f / 16) : 0;
        for (int i = 0; i < FXW * 4; ++i) {
            float r = fxv_hz(((float)i + 0.5f) * (1.0f / (FXW * 4))) / hz, den = 1 - r * r, sg = den < 0 ? 1.0f : -1.0f;
            float x = (r / q) / ((den < 0 ? -den : den) + 1e-6f), at = x > 1 ? 1.5707963f - (1 / x) * (0.7853982f + 0.273f * (1 - 1 / x)) : x * (0.7853982f + 0.273f * (1 - x));
            fxa_put(i >> 2, 0.5f + 0.47f * tall * sg * at * (2.0f / 3.14159265f));
        }
        pl.kind = FXK_MID; pl.axis = FXA_HZ; pl.mark = hz; break;
    }
    case CH_DEV_FLANGER: {                               /* the comb it makes, where its sweep is now: RATE moves it, DEPTH how far, FEEDBACK sharpens the teeth */
        float lfo = fxv_wave(ph, (unsigned)(FP(4) + 0.5f)), d = 13.0f + 265.0f * FP(1) * (0.5f + 0.5f * lfo), mix = FP(3);
        fxv_combcols(SR / d, FP(2) * 0.9f, 0, 1 - mix, mix, 2);
        pl.axis = FXA_HZ; break;
    }
    case CH_DEV_PHASER: {                                /* its all-passes (POLES: 2, 4, 6 or 8): a notch a pair, swept */
        int np = 2 + 2 * (int)(FP(5) + 0.5f); np = np < 2 ? 2 : np > 8 ? 8 : np;
        float lfo = fxv_wave(ph, (unsigned)(FP(4) + 0.5f)), fc = fxv_map(0.5f + 0.5f * FP(1) * lfo, 150, 5000), fb = FP(2) * 0.85f, mix = FP(3);
        for (int i = 0; i < FXW * 2; ++i) {
            float r = fxv_hz(((float)i + 0.5f) * (1.0f / (FXW * 2))) / fc, q = 1 / (1 + r * r), ar = (1 - r * r) * q, ai = -2 * r * q;
            float er = 1, ei = 0;
            for (int k = 0; k < np; ++k) { float tr = er * ar - ei * ai; ei = er * ai + ei * ar; er = tr; }   /* the all-passes, one after another */
            float dr = 1 - fb * er, di = -fb * ei, D = 1 / (dr * dr + di * di + 1e-9f);
            float re = (1 - 0.5f * mix) + 0.5f * mix * (er * dr + ei * di) * D, im = 0.5f * mix * (ei * dr - er * di) * D;
            fxa_put(i >> 1, fxv_db(fm_sqrtf(re * re + im * im)));
        }
        pl.axis = FXA_HZ; break;
    }
    case CH_DEV_UNISON: pl.kind = FXK_VOICES; break;      /* (its own drawing: fxv_unison) */
    case CH_DEV_CHORUS: {                                /* its two voices' delays, the last four seconds of them, now at the right */
        float rate = fxv_map(FP(0), 0.05f, 5), dp = 0.42f * (0.1f + 0.9f * FP(1)); unsigned sh = (unsigned)(FP(3) + 0.5f);
        for (int x = 0; x < FXW; ++x) {
            float t = ph - rate * 4.0f * (1 - ((float)x + 0.5f) * (1.0f / FXW));
            fxa_put(x, 0.5f + dp * fxv_wave(t, sh)); fxb_put(x, 0.5f + dp * fxv_wave(t + 0.25f, sh));
        }
        pl.kind = FXK_MID; pl.axis = FXA_SEC; pl.span = 4; break;
    }
    case CH_DEV_TREMOLO: {                               /* the level, the last two seconds; the right side fainter where STEREO parts them */
        float rate = fxv_map(FP(0), 0.2f, 16), dp = FP(1), st = FP(3); unsigned sh = (unsigned)(FP(2) + 0.5f);
        for (int i = 0; i < FXW * 2; ++i) {
            float t = ph - rate * 2.0f * (1 - ((float)i + 0.5f) * (1.0f / (FXW * 2)));
            float s = sh == 1 ? fxv_wave(t, 1) : fm_sinf((t - (float)(int)t) * 6.2831853f);
            if (sh == 2) s = fm_clampf(s * 6, -1, 1);
            fxa_put(i >> 1, 0.06f + 0.88f * (1 - dp * (0.5f - 0.5f * s)));
            if (st > 0.02f && (i & 1)) fxb_put(i >> 1, 0.06f + 0.88f * (1 - dp * (0.5f - 0.5f * s * (1 - 2 * st))));
        }
        pl.axis = FXA_SEC; pl.span = 2; break;
    }
    case CH_DEV_DRIVE: {                                 /* one cycle of a sine at 0.866 through the TYPE at this DRIVE and then the CLIP, a heavy line that whitens as DRIVE rises; behind it, faint, the sine that went in */
        unsigned type = (unsigned)(FP(4) + 0.5f), clip = (unsigned)(FP(6) + 0.5f); float dr = FP(0), mix = FP(3);
        float hold = fxv_map(dr, 1, 32), q = fm_exp2f(15 - 13 * fm_clampf(FP(5), 0, 1));
        for (int x = 0; x < FXW; ++x) {
            float in = 0.866f * fm_sinf(((float)x + 0.5f) * (6.2831853f / FXW)), y;
            if (type == 5) { float h = 0.866f * fm_sinf(((float)(int)((float)(int)((float)x / hold) * hold) + 0.5f) * (6.2831853f / FXW)); y = (float)(int)(h * q + (h < 0 ? -0.5f : 0.5f)) / q; }
            else y = ch_fx_drive_curve(type, clip, dr, in);
            fxa_put(x, 0.5f + 0.46f * fm_clampf(in + (y - in) * mix, -1, 1)); fxb_put(x, 0.5f + 0.46f * in);
        }
        pl.kind = FXK_MID; pl.mark = 1 + dr; break;      /* (mark: on this one, the line's weight and how white) */
    }
    case CH_DEV_SIDE: {                                  /* the duck across a bar, at the tempo, under its source's own hits (the marks along the top) */
        unsigned sel = (unsigned)(FP(0) + 0.5f) % (CH_TRACKS + CH_TRACKS * CH_KIT_PARTS), src = sel < CH_TRACKS ? sel : (sel - CH_TRACKS) / CH_KIT_PARTS;
        int lane = sel < CH_TRACKS ? -1 : (int)((sel - CH_TRACKS) % CH_KIT_PARTS);
        const ch_seq_state *sq = ch_seq_get(src); unsigned len = sq && sq->len ? sq->len : 16;
        fx_hits = 0;
        if (sq) for (unsigned i = 0; i < 16; ++i) for (unsigned l = 0; l < CH_LANES; ++l) if ((lane < 0 || (int)l == lane) && sq->step[l][i % len].vel) fx_hits |= (uint16_t)(1u << i);
        float amt = FP(1), dt = (240.0f / bpm) / (float)(FXW * 4);
        float ka = 1 - fm_exp2f(-1.4427f * dt / fxv_map(FP(2), 0.001f, 0.1f)), kr = 1 - fm_exp2f(-1.4427f * dt / fxv_map(FP(3), 0.008f, 1.5f));
        float env = 0, g = 1;
        for (int pass = 0; pass < 2; ++pass)             /* (the bar once round first: its left edge as the loop leaves it) */
            for (int i = 0; i < FXW * 4; ++i) {
                if (i % 56 == 0 && (fx_hits >> (i / 56) & 1u)) env = 1;
                float target = 1 - amt * env;
                g += (target - g) * (target < g ? ka : kr);
                env *= 1 - kr;
                if (pass) fxa_put(i >> 2, 0.06f + 0.88f * g);
            }
        pl.axis = FXA_BEAT; pl.span = 4; break;
    }
    case CH_DEV_REVERB: case CH_DEV_SPRING: {            /* the tail on a ruler of seconds: it ends where DECAY says (60 dB down); the core its highs, gone sooner */
        float t60, t60h, rise, mix = FP(3), ten = FP(0);
        if (dev == CH_DEV_REVERB) { t60 = fxv_map(FP(1), 0.3f, 12); t60h = t60 * (1 - 0.85f * FP(2)); rise = 0.03f + 0.2f * FP(0); }
        else { float lp = (1540.0f - 770.0f * ten) / (SR * 0.5f); t60 = lp * 3 / (-0.30103f * fm_log2f(0.5f + 0.46f * FP(1))); t60h = t60 * (0.35f + 0.65f * FP(2)); rise = 0.02f; }
        float half = (float)(fx_ph / 2 - 1), amp = 0.3f + 0.7f * fm_sqrtf(fm_clampf(mix, 0, 1));
        for (int x = 0; x < FXW; ++x) {
            float t = 6.0f * (float)x * (1.0f / FXW) - 0.08f, a = 0, b = 0;
            if (t > 0) {
                float on = t < rise ? t / rise : 1, e = 1 - t / t60, eh = 1 - t / t60h, tex;
                if (dev == CH_DEV_REVERB) { uint32_t h = (uint32_t)((float)x / (1 + ten * 3)) * 2246822519u; h ^= h >> 13; h *= 2654435761u; tex = 0.62f + 0.38f * (float)(h >> 24) * (1.0f / 255); }
                else { float sn = fm_sinf(t * (18 + 50 * ten)); tex = 0.55f + 0.45f * (sn < 0 ? -sn : sn); }
                if (e > 0) a = amp * on * e * tex * half;
                if (eh > 0) b = 0.55f * amp * on * eh * tex * half;
            }
            fxa_lo[x] = (uint8_t)a; fxb[x] = (uint8_t)b;
        }
        pl.kind = FXK_TAIL; pl.axis = FXA_SEC; pl.span = 6; break;
    }
    case CH_DEV_DELAY: case CH_DEV_ECHO: case CH_DEV_DUB: case CH_DEV_CRYSTAL: {
        unsigned tp = dev == CH_DEV_CRYSTAL ? 1u : 0u, free_ = dev == CH_DEV_ECHO || ts->mode[ch_row(sl, dev)][tp];
        pl.kind = FXK_BARS;
        if (free_) { pl.axis = FXA_SEC; pl.span = dev == CH_DEV_DELAY ? 3.0f : dev == CH_DEV_CRYSTAL ? 1.0f : 6.0f; }
        else { pl.axis = FXA_BEAT; pl.span = 8; }
        break;
    }
    case CH_DEV_TRANCE: {                                /* the gate as it sounds: its steps as long as GATE has them, down to DEPTH's floor, their
                                                          * edges rounded by SMOOTH as the effect rounds them (two one-poles, at the tempo) */
        static const float DB[6] = {0.125f, 1.0f / 6, 0.25f, 1.0f / 3, 0.5f, 1};
        float span = 16 * DB[(unsigned)(FP(0) + 0.5f) % 6], gate = FP(1), duty = gate > 0.995f ? 2 : 0.05f + 0.95f * gate, sm = FP(2), low = 0.9f * (1 - FP(3)) + 0.1f;
        float bpm = ch_tempo() > 1 ? ch_tempo() : 120, dt = span * 60.0f / bpm / (float)(FXW * 4), kf = 1 - fm_exp2f(-1.4427f * dt / (0.0007f + 0.02f * sm * sm));
        unsigned len = 1u + ((unsigned)(FP(8) + 0.5f) & 15u);
        float g1 = low, g = low;
        for (int pass = 0; pass < 2; ++pass)                 /* (once round unseen, so the left edge is as the loop comes into it) */
            for (int x = 0; x < FXW; ++x)
                for (int q = 0; q < 4; ++q) {
                    float c = ((float)(x * 4 + q) + 0.5f) * (16.0f / (FXW * 4)), fr = c - (float)(int)c;
                    unsigned i = ((unsigned)c & 15u) % len, m = (unsigned)(FP(4 + i / 4) + 0.5f);
                    float on = (m >> (i & 3u) & 1u) && fr < duty ? 0.9f : low;
                    g1 += (on - g1) * kf; g += (g1 - g) * kf;
                    if (pass) fxa_put(x, g);
                }
        pl.axis = FXA_BEAT; pl.span = span; break;
    }
    default:
        for (int x = 0; x < FXW; ++x) fxa_put(x, fxv(dev, ts, sl, ((float)x + 0.5f) * (1.0f / FXW)));
        if (dev == CH_DEV_FILTER || dev == CH_DEV_DJ) pl.axis = FXA_HZ;
        else if (dev == CH_DEV_REPEAT) { pl.axis = FXA_BEAT; pl.span = 4; }
        else if (dev == CH_DEV_TRANCE) { static const float DB[6] = {0.125f, 1.0f / 6, 0.25f, 1.0f / 3, 0.5f, 1}; pl.axis = FXA_BEAT; pl.span = 16 * DB[(unsigned)(FP(0) + 0.5f) % 6]; }
        else pl.kind = FXK_MID;
        break;
    }
    return pl;
#undef FP
}
static char *fxv_int(char *o, unsigned v) { char t[6]; int n = 0; do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 6); while (n) *o++ = t[--n]; *o = 0; return o; }
/* the ruler: pass 0 its faint lines up the plot (before the drawing), pass 1 its line, ticks and words (after) */
static void fxv_axis(fxv_plan pl, int pass)
{
    const int32_t B = FXT + fx_ph, ay = B + 1, ly = B + 6;
    char t[10];
    if (pl.axis == FXA_NONE) return;
    if (pass) cv_rect(FX0, ay, FXW, 1, GREY5(6));
    if (pl.axis == FXA_HZ) {
        static const float F[8] = {50, 100, 200, 500, 1000, 2000, 5000, 10000};
        for (unsigned i = 0; i < 8; ++i) {
            int32_t x = FX0 + (int32_t)(fm_log2f(F[i] * 0.05f) * (1.0f / 9.97f) * FXW); int big = i == 1 || i == 4 || i == 7;
            if (!pass) { if (big) cv_rect(x, FXT, 1, fx_ph, GREY5(1)); continue; }
            cv_rect(x, ay, 1, big ? 4 : 2, big ? C_GRAY : GREY5(8));
            if (big) { const char *s = i == 1 ? "100" : i == 4 ? "1K" : "10K"; cv_text(x - text_w(&FONT_T, s) / 2, ly, &FONT_T, s, C_GRAY); }
        }
        if (pass && pl.mark > 20 && pl.mark < 20000) {    /* the pitch: a mark of its own */
            int32_t x = FX0 + (int32_t)(fm_log2f(pl.mark * 0.05f) * (1.0f / 9.97f) * FXW);
            cv_rect(x - 1, ay - 1, 3, 3, C_WHITE);
        }
    } else if (pl.axis == FXA_SEC) {
        float step = pl.span <= 1 ? 0.25f : pl.span <= 2 ? 0.5f : 1.0f;
        for (int k = 0; (float)k * step * 0.5f < pl.span - 0.001f; ++k) {
            float sec = (float)k * step * 0.5f; int32_t x = FX0 + (int32_t)(sec / pl.span * FXW); int big = !(k & 1);
            if (!pass) { if (big && k) cv_rect(x, FXT, 1, fx_ph, GREY5(1)); continue; }
            cv_rect(x, ay, 1, big ? 4 : 2, big ? C_GRAY : GREY5(8));
            if (!big || !k) continue;
            char *o = t; unsigned ms = (unsigned)(sec * 1000 + 0.5f);
            if (pl.span <= 1) { o = fxv_int(o, ms); *o++ = 'M'; }
            else { o = fxv_int(o, ms / 1000); if (ms % 1000) { *o++ = '.'; o = fxv_int(o, ms % 1000 / 100); } }
            *o++ = 'S'; *o = 0;
            cv_text(x - text_w(&FONT_T, t) / 2, ly, &FONT_T, t, C_GRAY);
        }
    } else {
        float ppb = (float)FXW / pl.span;
        for (int b = 0; (float)b < pl.span - 0.001f; ++b) {
            int32_t x = FX0 + (int32_t)((float)b * ppb); int bar = !(b & 3);
            if (!pass) { if (b) cv_rect(x, FXT, 1, fx_ph, GREY5(1)); continue; }
            cv_rect(x, ay, 1, bar ? 5 : 3, bar ? C_WHITE : C_GRAY);
            if (ppb >= 24) for (int q = 1; q < 4; ++q) cv_rect(x + (int32_t)((float)q * ppb * 0.25f), ay, 1, 2, GREY5(8));
            if (pl.span > 8.01f && !bar) continue;
            char *o = t;
            if (pl.span <= 4.01f) o = fxv_int(o, (unsigned)b + 1u);
            else { o = fxv_int(o, (unsigned)b / 4u + 1u); *o++ = '.'; o = fxv_int(o, ((unsigned)b & 3u) + 1u); }
            cv_text(x + 3, ly, &FONT_T, t, bar ? C_WHITE : C_GRAY);
        }
    }
}
/* the delays: the hit (white, at the left) and each echo after it where it falls on the ruler, as tall as it is loud
 * (MIX the first, FEEDBACK each one after), fading as the loop wears it. Up and down from the middle: left and right,
 * so PING PONG is echoes on alternate sides. ECHO's WOW pushes them off the grid; CRYSTAL's climb or fall by PITCH */
static void fxv_bars(unsigned dev, const ch_track_state *ts, unsigned sl, fxv_plan pl, uint16_t col, uint16_t tint)
{
#define FP(k) ch_param_of(ts, sl, dev, k)
    unsigned tp = dev == CH_DEV_CRYSTAL ? 1u : 0u;
    float sec = ch_fx_delay_sec(dev, FP(tp), ts->mode[ch_row(sl, dev)][tp]), bpm = ch_tempo(); if (bpm < 24) bpm = 120;
    float sp = (pl.axis == FXA_BEAT ? sec * bpm * (1.0f / 60) : sec) / pl.span * (float)FXW;        /* columns between echoes */
    float fb = dev == CH_DEV_DELAY ? FP(1) * 0.95f : dev == CH_DEV_ECHO ? FP(1) * 1.05f : dev == CH_DEV_DUB ? FP(1) * 1.15f : FP(2) * 0.92f;
    float amp = fm_sqrtf(fm_clampf(FP(tp + 2), 0, 1));
    float wear = dev == CH_DEV_DELAY ? (FP(3) < 0 ? -FP(3) : FP(3)) * 0.3f : dev == CH_DEV_ECHO ? 0.06f + FP(4) * 0.25f : dev == CH_DEV_DUB ? 0.12f : 0.05f;
    float wow = dev == CH_DEV_ECHO ? FP(3) * 3.0f : 0, st = dev == CH_DEV_CRYSTAL ? (float)(int)(FP(0) + 0.5f) - 12.0f : 0;
    int ping = dev == CH_DEV_DELAY && FP(4) > 0.5f;
    const int32_t mid = FXT + fx_ph / 2, half = fx_ph / 2 - 2, bw = sp >= 7 ? 3 : sp >= 3.5f ? 2 : 1;
    if (fb > 0.98f) fb = 0.98f;
    if (sp < 1) sp = 1;
    cv_rect(FX0, mid, FXW, 1, GREY5(3));
    cv_rect(FX0, mid - half, 2, 2 * half + 1, C_WHITE);                                           /* the hit */
    for (int k = 1; k < 200; ++k) {
        float xf = (float)k * sp + (wow ? wow * fm_sinf((float)k * 2.4f) : 0);
        if (xf > (float)(FXW - bw) || amp < 0.03f) break;
        int32_t h = (int32_t)(amp * (float)half + 0.5f), x = FX0 + (int32_t)xf, cy = mid;
        if (h < 1) h = 1;
        if (st != 0) { float lift = (float)(half - 6) * (1 - fm_exp2f(-(float)k * (st < 0 ? -st : st) * (1.0f / 14))); cy -= (int32_t)(st > 0 ? lift : -lift); if (h > half - (cy < mid ? mid - cy : cy - mid)) h = half - (cy < mid ? mid - cy : cy - mid); if (h < 1) h = 1; }
        float fade = (float)(k - 1) * wear; uint16_t c = mix565(col, tint, fade > 0.8f ? 0.8f : fade);
        if (ping) { if (k & 1) cv_rect(x, cy - h, bw, h, c); else cv_rect(x, cy + 1, bw, h, c); }
        else cv_rect(x, cy - h, bw, 2 * h + 1, c);
        amp *= fb;
    }
#undef FP
}
/* UNISON's picture. Left, the last four seconds: the sound itself a straight line through the middle, and each copy a
 * wave of its own, swaying at its own speed, stacked above and below it; VOICES is how many, DETUNE how far apart they
 * stand (their pitch from the sound's), MIX how bright the copies are against the line. (SPREAD is not drawn: a panel of
 * dots for the two sides was there, and was not wanted) */
static void fxv_unison(const ch_track_state *ts, unsigned sl, uint16_t col, uint16_t tint)
{
    static const uint8_t M[6] = {43, 57, 31, 71, 89, 103};
    static const float OFF[6] = {0, 0.37f, 0.71f, 0.13f, 0.55f, 0.89f};
    const float det = fm_clampf(ch_param_of(ts, sl, CH_DEV_UNISON, 0), 0, 1);
    const float mix = fm_clampf(ch_param_of(ts, sl, CH_DEV_UNISON, 3), 0, 1);
    int vs = (int)(ch_param_of(ts, sl, CH_DEV_UNISON, 1) + 0.5f); vs = vs < 0 ? 0 : vs > 2 ? 2 : vs;
    const int nv = 2 + 2 * vs;
    enum { WX = FXW };
    const int32_t T = FXT, H = fx_ph, mid = T + H / 2;
    const float now = (float)(plat_ms() % 100000u) * 0.001f, reach = (0.08f + 0.92f * det) * (float)(H / 2 - 7);
    const uint16_t wet = mix565(tint, col, 0.2f + 0.8f * mix), dry = mix565(GREY5(3), C_WHITE, 1 - mix);
    cv_rect(FX0, mid, WX, 1, dry);
    for (int k = 0; k < nv; ++k) {
        const float side = (k & 1) ? 1.0f : -1.0f, rank = (float)(k / 2 + 1) / (float)(nv / 2);
        const float y0 = (float)mid + side * rank * reach;
        int prev = -1, y;
        for (int x = 0; x < WX; ++x) {
            const float t = now - 4.0f * (1 - ((float)x + 0.5f) * (1.0f / WX)), a = t * (float)M[k] * 0.01f + OFF[k];
            y = (int)(y0 - 3.0f * fm_sinf((a - (float)(int)a) * 6.2831853f) + 0.5f);
            const int lo = prev < 0 || prev > y ? y : prev, hi = prev < 0 || prev < y ? y : prev;
            cv_rect(FX0 + x, lo, 1, hi - lo + 1, wet);
            prev = y;
        }
    }
}
static void body_fxviz(void)
{
    const ch_track_state *ts = ch_track_get(ET);
    unsigned dev = ts->device[ui.slot];
    enum { BAND = 80 };
    uint16_t col = ui.mfx ? C_ACC : TRACK_COL[(ui.slot - CH_FX1) & 3u], tint = tint565(col, 0.25f);   /* (its place's colour, as its pill) */
    if (ts->bypass[ui.slot]) { col = C_GRAY; tint = RGB(14, 14, 18); }
    fx_ph = fxv_ruled(dev) ? 70 : 85;
    fx_grit = plat_ms() >> 7;
    fxv_plan pl = fxv_prepare(dev, ts, ui.slot);
    const int32_t T = FXT, H = fx_ph, B = T + H, mid = T + H / 2;
    const uint16_t dim = mix565(tint, col, 0.45f);
    const int heavy = dev == CH_DEV_DRIVE;
    const uint16_t lc = heavy && !ts->bypass[ui.slot] ? mix565(col, C_WHITE, 0.6f * (pl.mark - 1)) : col;
    for (int band = 0; band < 2; ++band) {
        cv_begin(240, band ? 98 - BAND : BAND, C_BLACK);
        cv_oy = band ? -BAND : 0;
        cv_rect(FX0 - 4, 2, FXW + 8, 1, C_LINE); cv_rect(FX0 - 4, 95, FXW + 8, 1, C_LINE);
        cv_rect(FX0 - 4, 2, 1, 94, C_LINE); cv_rect(FX0 + FXW + 3, 2, 1, 94, C_LINE);
        for (int q = 1; q < 4; ++q) cv_rect(FX0, T + H * q / 4, FXW, 1, GREY5(1));
        fxv_axis(pl, 0);
        if (pl.kind == FXK_BARS) fxv_bars(dev, ts, ui.slot, pl, col, tint);
        else if (pl.kind == FXK_VOICES) fxv_unison(ts, ui.slot, col, tint);
        else if (pl.kind == FXK_TAIL) {
            const int32_t half = H / 2 - 1;
            cv_rect(FX0, mid, FXW, 1, GREY5(3));
            for (int x = 0; x < FXW; ++x) {
                int a = fxa_lo[x], b = fxb[x];
                if (!a) continue;
                cv_rect(FX0 + x, mid - a, 1, 2 * a + 1, tint);
                if (b) cv_rect(FX0 + x, mid - b, 1, 2 * b + 1, dim);
                cv_pset(FX0 + x, mid - a, col); cv_pset(FX0 + x, mid + a, col);
            }
            cv_rect(FX0, mid - half, 2, 2 * half + 1, C_WHITE);                                   /* the hit */
        } else {
            int pb = -1, plo = -1, phi = -1;
            if (pl.kind == FXK_MID) cv_rect(FX0, mid, FXW, 1, GREY5(3));
            for (int x = 0; x < FXW; ++x) {               /* the fill, then the fainter curve, then the curve over both */
                int hi = T + fxa_hi[x], lo = T + fxa_lo[x];
                if (pl.kind == FXK_FLOOR) { if (lo < B - 1) cv_rect(FX0 + x, lo + 1, 1, B - lo - 1, tint); }
                else if (hi > mid) cv_rect(FX0 + x, mid + 1, 1, hi - mid - 1, tint);
                else if (lo < mid) cv_rect(FX0 + x, lo + 1, 1, mid - lo - 1, tint);
            }
            if (dev == CH_DEV_ERODE && pl.span > 0.01f && !ts->bypass[ui.slot])   /* ERODE: holes eaten into the fill, most of them near the top of a tall column */
                for (int x = 0; x < FXW; ++x) {
                    int lo = T + fxa_lo[x], hgt = B - 1 - lo; if (hgt < 2) continue;
                    float dens = pl.span * 0.8f * (0.25f + 0.75f * (float)hgt / (float)H), fall = 1.0f / (float)hgt;
                    for (int y = lo + 1; y < B; ++y) if (fx_gritf((uint32_t)x, (uint32_t)y) < dens * (1 - (float)(y - lo) * fall)) cv_pset(FX0 + x, y, C_BLACK);
                }
            for (int x = 0; x < FXW; ++x) {
                if (!fxb[x] || fxb[x] == 255) { pb = -1; continue; }
                int y = T + fxb[x] - 1, a = pb < 0 ? y : pb < y ? pb : y, b = pb < 0 ? y : pb < y ? y : pb;
                cv_rect(FX0 + x, a, 1, b - a + 1, dim);
                pb = y;
            }
            for (int x = 0; x < FXW; ++x) {
                int a = T + fxa_hi[x], b = T + fxa_lo[x], hi = a, lo = b;
                if (plo >= 0) { if (plo < a) a = plo; if (phi > b) b = phi; }
                if (fxb[x] == 255 && lo - hi > 2) { cv_rect(FX0 + x, hi, 1, lo - hi + 1, mix565(tint, col, 0.25f)); cv_pset(FX0 + x, hi, col); cv_pset(FX0 + x, lo, col); }   /* the teeth too close to draw: the band they fill, its edges bright */
                else cv_rect(FX0 + x, a, 1 + heavy, b - a + 1 + heavy, lc);
                plo = lo; phi = hi;
            }
            if (dev == CH_DEV_SIDE) for (unsigned i = 0; i < 16; ++i) if (fx_hits >> i & 1u) cv_rect(FX0 + (int32_t)i * 14, T, 2, 4, C_WHITE);
        }
        fxv_axis(pl, 1);
        cv_blit(0, (uint32_t)(134 + (band ? BAND : 0)));
    }
    cv_oy = 0;
}

/* the PANEL (EDIT again, its third layout): on an engine or effect page, every page at once, a row of four each, in place of the tiles and
 * the footer; the row on K1..K4 framed and bright, SELECT moving it as it turns the pages. Six rows show; more scroll. */
static int panel_on(void)
{
    if (ui_knobs >= 2 && ui.view == V_DEVICE && ui.slot != CH_MATERIAL && ui.slot < CH_SLOTS && ch_track_get(ET)->device[ui.slot] != CH_DEV_BYPASS) return 0;   /* an effect: always its knobs, its pages and its drawing, whatever the layout */
    return ui.panel && ui.view == V_DEVICE && ui.slot < CH_SLOTS && !env_applies() && cur_dev() && cur_dev()->params;
}
static unsigned pages_now(void);
/* the PANEL with the knobs: the row on K1..K4 is four knob tiles at their full size, framed in the track's colour, where
 * its page is in the list; every other page a short row, the parameter's name and its value and no knob, so six pages
 * show at once (more scroll) and the knobs stay as large as on the page view */
static void body_panel_k(void)
{
    const ch_track_state *ts = ch_track_get(ET);
    int mac = mac_shown();
    const uint16_t pcol = ui.mfx ? C_ACC : TRACK_COL[ui.track];   /* (the master's pages: cyan) */
    const int32_t TH = 78, SH = ui_knobs >= 2 ? 14 : 20, room = mac ? 142 : 178;   /* (mock-up: names alone, a size larger, seven rows) */
    /* the knobs' row always first; under it the pages that follow, in order and round again past the last: SELECT
     * turns the list up into the knobs, the knobs stay where they are */
    unsigned n = pages_now(), cur = ui.page[ui.slot], maxs = 1u + (unsigned)((room - TH) / SH), shown = n > maxs ? maxs : n;
    /* With the knobs: under them the whole list in its own order, every page a row, the one on the knobs among them in
     * the track's colour (it was the pages after it, turning up into the knobs: where one was in the list was lost).
     * More pages than rows: the list moves only as far as keeps the one on the knobs in sight */
    unsigned rows = maxs - 1u, first = 0;
    if (ui_knobs >= 2) {
        static uint8_t top;
        if (n <= rows) top = 0;
        else { if (cur < top) top = (uint8_t)cur; if (cur >= top + rows) top = (uint8_t)(cur - rows + 1u); if (top + rows > n) top = (uint8_t)(n - rows); }
        first = top; shown = 1u + (n < rows ? n : rows);
    }
    int on = !ts->bypass[ui.slot];
    const int32_t BY[3] = {54, 134, 214}, BH[3] = {80, mac ? 64 : 80, 26};
    if (mac) macro_strip(198, 42);
    for (unsigned bd = 0; bd < (mac ? 2u : 3u); ++bd) {
        cv_begin(240, (uint32_t)BH[bd], C_BLACK);
        int32_t yy = 55;
        for (unsigned r = 0; r < shown; ++r) {
            unsigned pg = ui_knobs >= 2 ? (r ? first + r - 1u : cur) : (cur + r) % n; int lit = r == 0;
            int32_t rh = lit ? TH : SH, y0 = yy - BY[bd];
            yy += rh;
            if (y0 >= BH[bd] || y0 + rh <= 0) continue;
            for (unsigned i = 0; i < 4; ++i) {
                int32_t x = 1 + (int32_t)i * 60;
                dev_cell c; int ok = dev_cell_at(tile_param_at(pg, i), &c);
                if (lit) {
                    knob_frame = pcol; knob_mod = ok && c.nmap ? c.mnow : -1;
                    if (ui_knobs >= 2 && i == 0) knob_panel(y0, pcol);
                    if (ui_knobs >= 2) knob_fill = PANEL_GREY;   /* (an eighth: darker than that and the screen has no steps left to hold the hue) */
                    {   /* (the four drawn a little in from the panel's sides: a long value had its first letter on the edge) */
                        int32_t xk = ui_knobs >= 2 ? 4 + (int32_t)i * 58 : x;
                        if (ok) tile_k(xk, y0, ENC_LBL[i], c.name, c.val, c.md, c.bv, c.bip, on); else tile_k(xk, y0, ENC_LBL[i], 0, 0, 0, 0, 0, 0);
                    }
                    knob_frame = 0; knob_mod = -1; knob_fill = 0;
                    continue;
                }
                if (ui_knobs >= 2 && pg == cur) { if (i == 0) { cv_rect(2, y0, 236, rh - 1, pcol); cv_rect(1, y0 + 1, 238, rh - 3, pcol); } }   /* the row on the knobs: a bar of the track's colour, its names black */
                else cv_rect(x, y0 + rh - 1, 58, 1, C_LINE);
                if (!ok) continue;
                char t[16]; unsigned k = 0;
                if (ui_knobs >= 2) {                       /* (mock-up) the name alone, in the tiles' own face */
                    while (c.name[k] && k < 15) { t[k] = c.name[k]; ++k; }
                    t[k] = 0;
                    const felucca_font_t *nf = text_w(&FONT_XS, t) > 57 ? &FONT_T : &FONT_XS;
                    while (k > 1 && text_w(nf, t) > 57) t[--k] = 0;
                    cv_text(x + (58 - text_w(nf, t)) / 2, y0, nf, t, pg == cur ? C_BLACK : c.nmap ? C_MOD : C_HI);
                    continue;
                }
                while (c.name[k] && k < 15) { t[k] = c.name[k]; ++k; }
                t[k] = 0;
                while (k > 1 && text_w(&FONT_T, t) > 54) t[--k] = 0;
                cv_text(x + 3, y0, &FONT_T, t, pg < cur ? C_DIM : C_GRAY);   /* (a page come round from the top: its names dimmer) */
                k = 0;
                while (c.val[k] && k < 15) { t[k] = c.val[k]; ++k; }
                t[k] = 0;
                const felucca_font_t *vf = text_w(&FONT_XS, t) > 54 ? &FONT_T : &FONT_XS;
                while (k > 1 && text_w(vf, t) > 54) t[--k] = 0;
                cv_text(x + 3, y0 + 8, vf, t, c.nmap ? C_MOD : on ? C_WHITE : C_DIM);
            }
        }
        cv_blit(0, (uint32_t)BY[bd]);
    }
}
static void body_panel(void)
{
    if (ui_knobs) { body_panel_k(); return; }
    const ch_track_state *ts = ch_track_get(ET);
    unsigned n = pages_now(), cur = ui.page[ui.slot], first = 0, shown = n > 4 ? 4 : n;   /* four rows at most, each full height: a 4 x 4 grid that reads at a glance; more pages scroll with the one on the knobs */
    if (n > 4) {                                         /* more than four pages: the lit row runs to the top or the bottom row before the rows scroll under it */
        static uint8_t top;                                /* (the first row shown, kept from draw to draw) */
        if (cur < top) top = (uint8_t)cur;
        if (cur >= top + 4u) top = (uint8_t)(cur - 3u);
        if (top + 4u > n) top = (uint8_t)(n - 4u);
        first = top;
    }
    int mac = mac_shown();                               /* the macros: the pages above them, the strip at the bottom */
    int32_t rh = (mac ? 142 : 178) / (int32_t)shown; if (rh > 44) rh = 44;
    int on = !ts->bypass[ui.slot];
    const int32_t BY[3] = {54, 134, 214}, BH[3] = {80, mac ? 64 : 80, 26};
    if (mac) macro_strip(198, 42);
    for (unsigned bd = 0; bd < (mac ? 2u : 3u); ++bd) {
        cv_begin(240, (uint32_t)BH[bd], C_BLACK);
        for (unsigned r = 0; r < shown; ++r) {
            unsigned pg = first + r; int lit = pg == cur;
            int32_t y0 = 55 + (int32_t)r * rh - BY[bd];   /* (a row clear of the title above) */
            if (y0 >= BH[bd] || y0 + rh <= 0) continue;
            for (unsigned i = 0; i < 4; ++i) {
                int32_t x = 1 + (int32_t)i * 60, w = 58;
                dev_cell c;
                if (lit) {                                 /* the row on the knobs: four white buttons (or orange, DISPLAY's PANEL ROW), their corners gently rounded, their text black */
                    static const uint8_t CUT[3] = {3, 1, 1};
                    cv_rect(x, y0, w, rh - 2, ui.panel_white ? C_WHITE : TRACK_COL[ui.track]);
                    for (int32_t q = 0; q < 3; ++q) {
                        cv_rect(x, y0 + q, CUT[q], 1, C_BLACK); cv_rect(x + w - CUT[q], y0 + q, CUT[q], 1, C_BLACK);
                        cv_rect(x, y0 + rh - 3 - q, CUT[q], 1, C_BLACK); cv_rect(x + w - CUT[q], y0 + rh - 3 - q, CUT[q], 1, C_BLACK);
                    }
                }
                else { cv_rect(x, y0 + rh - 3, w, 1, C_LINE); }
                if (!dev_cell_at(tile_param_at(pg, i), &c)) continue;
                /* the text white and as large as the row allows: tall rows (four pages or fewer) the name and value both in
                 * the tiles' small face, short rows (a synth's six) the name a size down; a value too wide drops a size */
                int tall = rh >= 36;
                const felucca_font_t *nf = tall ? &FONT_XS : &FONT_T, *vf = &FONT_XS;
                char nm[16]; unsigned k = 0; while (c.name[k] && k < 15) { nm[k] = c.name[k]; ++k; } nm[k] = 0;
                if (text_w(nf, nm) > w - 6) nf = &FONT_T;
                while (k > 1 && text_w(nf, nm) > w - 6) nm[--k] = 0;      /* (a name too long: cut) */
                cv_text(x + 3, y0 + (tall ? 2 : 0), nf, nm, lit ? C_BLACK : C_HI);
                char vv[16]; k = 0; while (c.val[k] && k < 15) { vv[k] = c.val[k]; ++k; } vv[k] = 0;
                if (text_w(vf, vv) > w - 6) vf = &FONT_T;
                while (k > 1 && text_w(vf, vv) > w - 6) vv[--k] = 0;
                cv_text(x + 3, y0 + (tall ? 17 : 8), vf, vv, lit ? C_BLACK : on ? C_WHITE : C_DIM);   /* (clear of the modulation slider, 3 above the bar) */
                int32_t bh = tall ? 4 : 3, mh = tall ? 3 : 2;       /* the level bar and the modulation slider: as thick as the row allows */
                int32_t bx = x + 3, bw = w - 6, by = y0 + rh - (tall ? 8 : 6);
                float a = fm_clampf(c.bip ? (c.bv + 1) * 0.5f : c.bv, 0, 1), b = fm_clampf(c.bip ? (c.lv + 1) * 0.5f : c.lv, 0, 1);
                int wh = lit && ui.panel_white;                   /* a white button: the bar's track light grey, its fill orange, the value's mark black */
                uint16_t tk = TRACK_COL[ui.track];                  /* a track-coloured button: the bar's track that colour at 5/8 */
                tk = (uint16_t)((((tk >> 11) * 5 / 8) << 11) | ((((tk >> 5) & 63) * 5 / 8) << 5) | ((tk & 31) * 5 / 8));
                cv_rect(bx, by, bw, bh, wh ? RGB(120, 122, 132) : lit ? tk : C_LINE);
                uint16_t fc = wh ? C_ACC : lit ? C_BLACK : C_GRAY;
                if (c.bip) { int32_t m = bx + bw / 2, e = bx + (int32_t)(a * (float)bw); cv_rect(m < e ? m : e, by, m < e ? e - m : m - e, bh, fc); }
                else cv_rect(bx, by, (int32_t)(a * (float)bw), bh, fc);
                if (c.nmap) mod_bar(bx, bw, by - mh - 1, mh, &c);          /* modulated: the modulators' sum, above the bar */
                else cv_rect(bx + (int32_t)(b * (float)(bw - 2)), by - 1, 2, bh + 2, wh ? C_BLACK : lit ? C_WHITE : C_ACC_HI);   /* the value now */
            }
        }
        cv_blit(0, (uint32_t)BY[bd]);
    }
}
/* ---- the GLO pages' list: under the panel, where the tracks or the MIXER's faders are on other views, every GLO
 * page a row of its four settings' names, as the PANEL layout lists an engine's pages; the one on the knobs a bar of
 * the GLO blue, its names black. SELECT turns the pages; more than fit: the list moves to keep that one in sight. ---- */
static int glolist_on(void) { return ui.view == V_MIX && ui.mix_page != 5 && ui_knobs >= 2 && !mac_shown(); }   /* (not the LOAD screen: its own) */
static const char *glo_name(unsigned page, unsigned i)
{
    static const char *const N[MIX_PAGES][4] = {
        {"LEVEL 1", "LEVEL 2", "LEVEL 3", "LEVEL 4"}, {"PAN 1", "PAN 2", "PAN 3", "PAN 4"}, {"FILTER 1", "FILTER 2", "FILTER 3", "FILTER 4"},
        {"FX SEND", 0, 0, 0}, {"KEY", "KEY SCALE", "TEMPO", "SWING"}, {"LOAD", "", "", ""}, {"ENTRY", "PATTERN", "BARS", "SONG"},
        {"COLOURS", "TONE", "PANEL ROW", "MIDI SYNC"}, {"SEND TO 1", "SEND TO 2", "SEND TO 3", "SEND TO 4"}, {"AUTOPILOT", "EVERY", "VARIETY", "WHEN OFF"},
        {"MICRO", "COUNT IN", "LIMITER", "HOLD FX"}};
    static const char *const SN[4] = {"TO T1", "TO T2", "TO T3", "TO T4"};
    static const char *const TS[4] = {"T1 SEND", "T2 SEND", "T3 SEND", "T4 SEND"};   /* K1 on SENDS: the chosen track's send into the master effects */
    if (page == 3) return i ? SN[send_other(ui.track, i - 1u)] : TS[ui.track % 4u];
    return N[page][i];
}
static void body_glolist(void)
{
    enum { SH = 14 };
    const int32_t top = ui.mix_page == 5 ? 152 : 134;      /* (the LOAD screen's table reaches further down; none of the rows is it) */
    const unsigned rows = (unsigned)(232 - top) / SH, cur = mix_pos(ui.mix_page);
    static uint8_t first;
    if (ui.mix_page == 5) {} else if (cur < first) first = (uint8_t)cur;
    if (ui.mix_page != 5 && cur >= first + rows) first = (uint8_t)(cur - rows + 1u);
    if (first + rows > MIX_NAV) first = (uint8_t)(MIX_NAV - rows);
    for (unsigned r = 0; r < rows; ++r) {
        unsigned k = first + r, page = MIX_ORDER[k]; int lit = k == cur && ui.mix_page != 5;
        cv_begin(240, SH, C_BLACK);
        if (lit) { cv_rect(2, 0, 236, SH - 1, C_GLO); cv_rect(1, 1, 238, SH - 3, C_GLO); }
        for (unsigned i = 0; i < 4; ++i) {
            int32_t x = 1 + (int32_t)i * 60;
            const char *nm = glo_name(page, i);
            if (!lit) cv_rect(x, SH - 1, 58, 1, C_LINE);
            char t[16]; unsigned n = 0;
            while (nm[n] && n < 15) { t[n] = nm[n]; ++n; }
            t[n] = 0;
            const felucca_font_t *nf = text_w(&FONT_XS, t) > 57 ? &FONT_T : &FONT_XS;
            while (n > 1 && text_w(nf, t) > 57) t[--n] = 0;
            cv_text(x + (58 - text_w(nf, t)) / 2, 0, nf, t, lit ? C_BLACK : C_HI);
        }
        cv_blit(0, (uint32_t)(top + (int32_t)r * SH));
    }
    if (top + (int32_t)rows * SH < 232) { cv_begin(240, (uint32_t)(232 - top - (int32_t)rows * SH), C_BLACK); cv_blit(0, (uint32_t)(top + (int32_t)rows * SH)); }
}
/* ---- HELP (GLO + SAVE): the panel as a line drawing, white on black, every control with its name; the one last
 * touched filled in, and under the drawing what it does. Nothing acts while it is open. ---- */
static void help_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    for (int32_t dy = -r; dy <= r; ++dy) { int32_t dx = 0; while ((dx + 1) * (dx + 1) + dy * dy <= r * r + r / 2) ++dx; cv_rect(cx - dx, cy + dy, 2 * dx + 1, 1, c); }
}
static void help_knob(int32_t cx, int32_t cy, int32_t r, const char *name, int on)
{
    help_disc(cx, cy, r, C_WHITE);
    if (!on) help_disc(cx, cy, r - 1, C_BLACK);
    cv_rect(cx, cy - r + 1, 1, r - 1, on ? C_BLACK : C_WHITE);
    cv_text(cx - text_w(&FONT_T, name) / 2, cy + r, &FONT_T, name, C_WHITE);
}
static void help_box(int32_t x, int32_t y, int32_t w, int32_t h, const char *name, int on)
{
    cv_rect(x, y, w, h, C_WHITE);
    if (!on) cv_rect(x + 1, y + 1, w - 2, h - 2, C_BLACK);
    if (name) cv_text(x + (w - text_w(&FONT_T, name) + 1) / 2, y + (h - 11) / 2, &FONT_T, name, on ? C_BLACK : C_WHITE);
}
static void body_help(void)
{
    static const char *const NAME[24] = {"HELP", "FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC", "OCT-", "OCT+",
        "SELECT", "ALGORITHM", "PRESETS", "KNOB 1", "KNOB 2", "KNOB 3", "KNOB 4", "WHITE KEYS", "BLACK KEYS"};
    static const char *const KN[4] = {"THE FOUR SETTINGS ON THE SCREEN, LEFT TO RIGHT", "LFO + A KNOB: MAP THE MODULATOR TO IT", "SEQ + A KNOB: THAT SETTING'S MODE", "REC + STEPS, THEN A KNOB: A LOCK ON THEM"};
    static const char *const T[24][4] = {
        {"PRESS A BUTTON OR A KEY, OR TURN A KNOB,", "TO SEE WHAT IT DOES.", "NOTHING IS CHANGED WHILE HELP IS OPEN.", "GLO + SAVE: BACK"},
        {"THE TRACK'S EFFECTS, FOUR SLOTS", "TAP AGAIN: THE NEXT SLOT. HOLD: ON OR OFF", "ALGORITHM PICKS THE SLOT'S EFFECT", "GLO + FX: MASTER EFFECTS. HOME + FX: PERFORM"},
        {"THE BROWSER", "SEL + ALGORITHM: THE TRACK'S LEVEL", "SEL ON A SET STEP + SELECT: MOVE THE STEP", ""},
        {"THE KEYS PLAY THE TRACK, OR SET STEPS AGAIN", "ENV + WHITE 1 TO 4: MUTE A TRACK", "ENV + BLACK 1 TO 4: SOLO A TRACK", "SEQ + ENV: THE ENVELOPE"},
        {"THE MODULATORS", "LFO + A KNOB: MAP THE MODULATOR TO IT", "LFO + WHITE 1 TO 4: CHOOSE THE MODULATOR", "LFO + REC: CLEAR ITS MAPS"},
        {"THE TRACK'S ENGINE AND ITS PAGES", "TAP AGAIN: THE LAYOUT. HOLD: ON OR OFF", "EDIT + A WHITE KEY: LOAD A PRESET", "EDIT + REC: UNDO"},
        {"GLOBAL: TEMPO, KEY, MIX, SENDS, SONG", "SELECT TURNS ITS PAGES", "GLO + PLAY: TAP THE TEMPO. GLO + EDIT: LOAD", "GLO + HOME: SPECTRUM. GLO + SAVE: HELP"},
        {"THE TRACKS VIEW: THE FOUR PATTERNS", "HOME + A WHITE KEY: CHOOSE A PATTERN", "HOME + SEQ + A WHITE KEY: COPY IT THERE", "HOME + KNOB 1, 2: STUTTER. KNOB 3: HOLD FX"},
        {"TAP: THE LIBRARY. HOLD: SAVE THE PROJECT", "SAVE + A WHITE KEY: STORE A PRESET", "IN THE BROWSER: KEEP THE CHOICE", "SEQ + SAVE: INIT"},
        {"THE LINE GENERATOR", "ON ITS PAGE, TAP AGAIN: A NEW LINE", "HOLD: A NEW LINE ON THE CHOSEN TRACK", ""},
        {"THE TRACK'S STEP SEQUENCER", "THE KEYS SET STEPS. HOLD A STEP TO EDIT IT", "SEQ + A KNOB: THAT SETTING'S MODE", "SEQ + WHITE 12 TO 16: GENERATORS. SEQ + REC: CLEAR"},
        {"PLAY AND STOP", "GLO + PLAY: TAP THE TEMPO", "", ""},
        {"RECORD WHAT IS PLAYED", "HELD, THEN LET GO: THE METRONOME", "REC + STEPS: ARM THEM FOR KNOB LOCKS", "EDIT + REC: UNDO"},
        {"THE KEYBOARD AN OCTAVE DOWN", "IN THE SEQUENCER: THE LANES AN OCTAVE DOWN", "OCT- AND OCT+ TOGETHER: THE MACROS ON THE KNOBS", ""},
        {"THE KEYBOARD AN OCTAVE UP", "IN THE SEQUENCER: THE LANES AN OCTAVE UP", "OCT- AND OCT+ TOGETHER: THE MACROS ON THE KNOBS", ""},
        {"TURNS THE PAGES", "IN THE SEQUENCER: THE CHOSEN STEP", "EDIT + SELECT: THE TRACK'S SCALE", ""},
        {"PICKS THE ENGINE, OR THE SLOT'S EFFECT", "SEL + ALGORITHM: THE TRACK'S LEVEL", "EDIT + ALGORITHM: THE TRACK'S KEY", "SEQ + ALGORITHM: THE SCALE"},
        {"CHOOSES THE TRACK, 1 TO 4", "EDIT + PRESETS: THE LANES A SEMITONE", "", ""},
        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        {"PLAY NOTES, OR SET STEPS IN THE SEQUENCER", "WITH HOME: PATTERNS. WITH SAVE, EDIT: PRESETS", "ON THE GLO PAGES, 1 TO 4: MUTE A TRACK", ""},
        {"PLAY NOTES", "A STEP HELD + BLACK KEYS: A CHORD", "ON THE GLO PAGES, 1 TO 4: SOLO A TRACK", ""}};
    const unsigned sel = ui.help_sel < 24 ? ui.help_sel : 0;
#define HB(b) (sel == 1u + (b))
#define HE(e) (sel == 15u + (e))
    cv_begin(240, 14, C_BLACK);
    cv_text(6, 1, &FONT_XS, "HELP", C_WHITE);
    cv_text(234 - text_w(&FONT_T, "GLO + SAVE: BACK"), 3, &FONT_T, "GLO + SAVE: BACK", C_GRAY);
    cv_blit(0, 40);
    /* the panel's top: the knobs and OCT at the left, the screen, the four knobs and the two rows of buttons */
    cv_begin(240, 80, C_BLACK);
    cv_rect(2, 0, 236, 1, C_WHITE); cv_rect(0, 2, 1, 78, C_WHITE); cv_rect(239, 2, 1, 78, C_WHITE); cv_rect(1, 1, 1, 1, C_WHITE); cv_rect(238, 1, 1, 1, C_WHITE);
    help_knob(18, 12, 6, "MASTER", 0); help_knob(52, 12, 6, "SELECT", HE(EN_SELECT));
    help_knob(18, 40, 6, "PRESET", HE(EN_PRESET)); help_knob(52, 40, 6, "ALGO", HE(EN_ALGO));
    help_box(4, 62, 29, 13, "OCT-", HB(B_OCTDN)); help_box(37, 62, 29, 13, "OCT+", HB(B_OCTUP));
    help_box(72, 4, 40, 36, 0, 0);
    cv_text(92 - text_w(&FONT_T, "SCREEN") / 2, 17, &FONT_T, "SCREEN", C_GRAY);
    for (unsigned i = 0; i < 4; ++i) { static const char *const K[4] = {"K1", "K2", "K3", "K4"}; help_knob(134 + (int32_t)i * 30, 12, 6, K[i], HE(EN_K1 + i)); }
    {
        static const uint8_t R1[6] = {B_FX, B_SEL, B_ENV, B_LFO, B_EDIT, B_GLO}, R2[6] = {B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC};
        for (unsigned i = 0; i < 6; ++i) {
            help_box(98 + (int32_t)i * 23, 45, 24, 14, NAME[1 + R1[i]], HB(R1[i]));
            help_box(98 + (int32_t)i * 23, 61, 24, 14, NAME[1 + R2[i]], HB(R2[i]));
        }
    }
    cv_blit(0, 54);
    /* the keys: eleven black above, sixteen white below */
    cv_begin(240, 34, C_BLACK);
    cv_rect(0, 0, 1, 32, C_WHITE); cv_rect(239, 0, 1, 32, C_WHITE); cv_rect(2, 33, 236, 1, C_WHITE); cv_rect(1, 32, 1, 1, C_WHITE); cv_rect(238, 32, 1, 1, C_WHITE);
    cv_rect(4, 0, 232, 1, C_GRAY);
    for (unsigned i = 0; i < 16; ++i) help_box(6 + (int32_t)i * 14 + (int32_t)i / 2, 17, 12, 13, 0, sel == 22);
    { static const uint8_t AFTER[11] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};
      for (unsigned k = 0; k < 11; ++k) { unsigned a = AFTER[k] + 1u; help_box(6 + (int32_t)a * 14 + (int32_t)a / 2 - 7, 4, 12, 11, 0, sel == 23); } }
    cv_blit(0, 134);
    /* what the one touched does */
    cv_begin(240, 64, C_BLACK);
    cv_text(6, 2, &FONT_XS, NAME[sel], C_WHITE);
    for (unsigned k = 0; k < 4; ++k) {
        const char *l = sel >= 18 && sel <= 21 ? KN[k] : T[sel][k];
        if (!l || !l[0]) continue;
        const char *q = l; char t[60]; unsigned n = 0;
        while (q[n] && n < 59) { t[n] = q[n]; ++n; }
        t[n] = 0;
        while (n > 1 && text_w(&FONT_T, t) > 230) t[--n] = 0;
        cv_text(6, 18 + (int32_t)k * 11, &FONT_T, t, k ? C_GRAY : C_HI);
    }
    cv_blit(0, 168);
#undef HB
#undef HE
}
static void body(void)
{
    if (ui.view == V_HELP) { body_help(); return; }
    if (ui.view == V_SCOPE) { body_scope(); return; }
    if (ui.view == V_STEP) { body_steps(); return; }
    if (ui.view == V_SONG) { body_song(); return; }
    if (mixpage_on()) { body_mixpage(); return; }
    const ch_track_state *ts = ch_track_get(ui.view == V_DEVICE ? ET : ui.view == V_PERFORM ? CH_MASTER : ui.track);
    const ch_snapshot *sn = ch_snapshot_get();
    char title[32], pg[12], val[16];
    /* the title line */
    const int fxrow = ui_knobs >= 2 && ui.view == V_DEVICE && (ui.mfx || !ui.lock_rec);   /* the effects' row: two rows taller, from 38 */
    cv_begin(240, fxrow ? 16 : 14, C_BLACK);
    title[0] = 0; pg[0] = 0;
    if (ui.view == V_GEN) {
        text_cat(title, ui.gen_hold ? "LINE: SET UP, ARP DRAWS IT" : "LINE: ARP DRAWS, KNOBS SHAPE"); int_text(ui.gen_page + 1, pg); text_cat(pg, "/2");
    } else if (ui.view == V_PERFORM) {
        text_cat(title, ui.perf_hold ? "PERFORM: HOLDING (HOME)" : "PERFORM: THE MASTER EFFECTS");
    } else if (ui.view == V_DEVICE && ui.slot != CH_MATERIAL) {
        const ch_device_desc *d = cur_dev();
        text_cat(title, ui.mfx ? MSLOT_NAME[ui.slot] : SLOT_NAME[ui.slot]); text_cat(title, " "); text_cat(title, fx_name(ts, ui.slot));
        if (ts->device[ui.slot] == CH_DEV_BYPASS) text_cat(title, ": ALGO ADDS");
        else if ((ui.mfx ? sn->mfx_nomem : sn->fx_nomem[ui.track]) >> (ui.slot - CH_FX1) & 1u) text_cat(title, " NO MEMORY");
        else if (ts->bypass[ui.slot]) text_cat(title, " BYPASS");
        int_text(ui.page[ui.slot] + 1, pg); text_cat(pg, "/"); { char q[8]; int_text((int32_t)pages_of(d->params), q); text_cat(pg, q); }
    } else if (ui.view == V_DEVICE) {
        const ch_device_desc *d = cur_dev();
        text_cat(title, SLOT_NAME[ui.slot]);
        {   /* the slot, then its device (once, when they share a name) or BYPASS */
            const char *dn = ts->bypass[ui.slot] && ui.slot ? "BYPASS" : !ui.slot && ts->device[0] == CH_DEV_BYPASS ? "OFF" : d->name;
            const char *a = SLOT_NAME[ui.slot], *b = dn;
            while (*a && *a == *b) { ++a; ++b; }
            if (*a || *b) { text_cat(title, " "); text_cat(title, dn); }
        }
        if (env_applies()) text_cat(pg, "ENV");
        else { int_text(ui.page[ui.slot] + 1, pg); text_cat(pg, "/"); { char q[8]; int_text((int32_t)pages_now(), q);   /* (the pages this model shows) */ text_cat(pg, q); } }
    } else if (ui.view == V_MOD) {
        unsigned m = ui.mod_page / 2;
        text_cat(title, "MOD "); int_text((int32_t)m + 1, pg); text_cat(title, pg); text_cat(title, " ");
        text_cat(title, ch_mod_name(ts->mod_type[m]));
        pg[0] = 0; int_text(ui.mod_page % 2 + 1, pg); text_cat(pg, "/2");
    } else {
        static const char *const MIXP[MIX_PAGES] = {"MIX LEVEL", "MIX PAN", "MIX FILTER", "SENDS", "KEY / TEMPO", "LOAD (US)", "SONG", "DISPLAY", "SEND TO", "AUTOPILOT", "SETUP"};
        text_cat(title, MIXP[ui.mix_page]);
        if (ui.mix_page == 0) { char v[8]; int_text((int32_t)(ch_return() * 100 + 0.5f), v); text_cat(title, "   RETURN "); text_cat(title, v); }   /* (ALGO turns it) */
        if (ui.mix_page == 3) {                            /* SENDS: the send the steps' SEND acts on (ALGO turns it) */
            unsigned k = ch_track_get(ui.track)->ss_dest; char n[4] = {'M', (char)('2' + k - CH_SS_FX - 1u), 0};
            text_cat(title, "   SEND TO: "); text_cat(title, k > CH_SS_FX && k <= CH_SS_LAST ? n : "ALL");
        }
        if (ui.mix_page == 5) pg[0] = 0;                   /* (LOAD: not one of the pages) */
        else { int_text((int32_t)mix_pos(ui.mix_page) + 1, pg); char q[4]; int_text(MIX_NAV, q); text_cat(pg, "/"); text_cat(pg, q); }
        {   /* the audio's load: now, its peak over the last few seconds, and dropouts since power-on */
            static uint32_t peak, peak_at;
            uint32_t now = plat_ms(), c = plat_cpu_pct(), xr = plat_xruns();
            if (c >= peak || now - peak_at > 3000u) { peak = c; peak_at = now; }
            char l[40] = "CPU "; char n[12];
            int_text((int32_t)c, n); text_cat(l, n); text_cat(l, "% PK "); int_text((int32_t)peak, n); text_cat(l, n); text_cat(l, "%");
            uint16_t col = peak >= 90u || xr ? C_ACC : C_GRAY;
            if (xr) { text_cat(l, " XR "); int_text((int32_t)xr, n); text_cat(l, n); }
            if (ui.mix_page != 3) {                            /* (the SENDS page: its title has the room) */
                /* as much of it as clears the title: all, then without "CPU", then the load alone, then none */
                const char *form[3] = {l, l + 4, 0}; char now_only[8]; int_text((int32_t)c, now_only); text_cat(now_only, "%"); form[2] = now_only;
                int32_t right = 234 - text_w(&FONT_XS, pg) - 8, left = 6 + text_w(&FONT_XS, title) + 6;
                for (unsigned k = 0; k < 3; ++k) if (right - text_w(&FONT_XS, form[k]) >= left && !(k == 2 && xr)) { cv_text(right - text_w(&FONT_XS, form[k]), 1, &FONT_XS, form[k], col); break; }
            }
        }
    }
    if (ui.lock_rec && ui.view == V_DEVICE && !ui.mfx) { title[0] = 0; text_cat(title, "LOCK REC  "); char n[8]; int_text((int32_t)lock_count(), n); text_cat(title, n); text_cat(title, " STEPS"); }
    if (fxrow) mock_row_fx(ch_track_get(ET));
    else {
        int32_t tx = cv_text(6, 1, &FONT_XS, title, ui.lock_rec && ui.view == V_DEVICE && !ui.mfx ? C_ACC : C_WHITE);
        if (ui.view == V_DEVICE && ui.gate_keys) cv_text(tx + 5, 2, &FONT_T, "KEYS: STEPS", C_WHITE);
        else if (ui.view == V_DEVICE) cv_text(tx + 5, 2, &FONT_T, "< ALG", C_DIM);   /* ALGO: the engine, or the place's effect */
    }
    cv_text(234 - text_w(&FONT_XS, pg), fxrow ? 2 : 1, &FONT_XS, pg, C_GRAY);
    cv_blit(0, fxrow ? 38 : 40);
    if (panel_on()) { body_panel(); return; }
    /* the four tiles, K1..K4 left to right (the LOAD page: its table) */
    cv_begin(240, 80, C_BLACK);
    if (ui.view == V_MIX && ui.mix_page == 5) {
        static const char *const COL[5] = {"ENG", "FX1", "FX2", "FX3", "FX4"};
        char v[12];
        if (ui.spd_show >= 2) {                            /* a voice of each model: microseconds a 256-frame block */
            /* eighteen a page, the effects first (they are what is asked about most), then the drums, then the synths */
            unsigned first = (ui.spd_show - 2u) * 18u;
            for (unsigned q = 0; q < 18 && first + q < COST_N; ++q) {
                unsigned k = (first + q + COST_FX0) % COST_N;
                const char *nm = k >= COST_FX0 ? ch_device_info(CH_DEV_FX_FIRST + (k - COST_FX0))->name : k < CH_DRUM_MODELS ? ch_drum_names[k] : ch_device_info(CH_DEV_SYNTH)->param[0].choices[k - CH_DRUM_MODELS];
                char sh[10]; unsigned c = 0; while (nm[c] && c < 9) { sh[c] = nm[c]; ++c; } sh[c] = 0;
                int32_t x = 4 + (int32_t)(q % 3) * 79, y = (int32_t)(q / 3) * 13 + 1;
                x = cv_text(x, y, &FONT_T, sh, C_GRAY);
                int_text(cost_us[k], v); cv_text(x + 3, y, &FONT_T, v, C_HI);
            }
        } else if (ui.spd_show) {                          /* the speed test: nanoseconds a step, the empty loop's taken off (LOOP: its own) */
            for (unsigned k = 0; k < SPD_TESTS; ++k) {
                int32_t x = 6 + (int32_t)(k % 3) * 78, y = (int32_t)(k / 3) * 14 + 2;
                x = cv_text(x, y, &FONT_T, SPD_NAME[k], C_GRAY);
                int_text(spd_ns[k], v); cv_text(x + 4, y, &FONT_T, v, C_HI);
            }
        } else {
        for (unsigned c = 0; c < 5; ++c) cv_text(40 + (int32_t)c * 40, 0, &FONT_T, COL[c], C_GRAY);
        for (unsigned t = 0; t < CH_TRACKS; ++t) {
            int32_t y = 12 + (int32_t)t * 12;
            char tn[4] = {'T', (char)('1' + t), 0};
            cv_text(6, y, &FONT_T, tn, TRACK_COL[t]);
            for (unsigned c = 0; c < 5; ++c) { int_text(sn->load_us[t][c], v); cv_text(40 + (int32_t)c * 40, y, &FONT_T, v, sn->load_us[t][c] ? C_HI : C_DIM); }
        }
        }
        char l[48] = "RENDER "; char n[12];
        static uint32_t blk, blk_at;                       /* the platform's longest whole block (USB audio and all), held a second */
        static uint32_t gap, tkr;                          /* the longest wait between two blocks' starts, the timer interrupt's longest run */
        if (plat_ms() - blk_at > 1000u) { blk = plat_audio_block_max(); gap = plat_audio_gap_max(); tkr = plat_tick_run_max(); blk_at = plat_ms(); }
        int_text(sn->render_us, n); text_cat(l, n); text_cat(l, " BLK "); int_text((int32_t)blk, n); text_cat(l, n); text_cat(l, " OF "); int_text(sn->budget_us, n); text_cat(l, n);
        if (ui.spd_show < 2) cv_text(6, 60, &FONT_T, l, sn->budget_us && sn->render_us * 10 > sn->budget_us * 9 ? C_ACC : C_HI);
        char d[64] = "C"; int_text(sn->cores ? sn->cores : 1, n); text_cat(d, n);
        if (sn->core2_tracks) { text_cat(d, ":"); for (unsigned t = 0; t < CH_TRACKS; ++t) if (sn->core2_tracks >> t & 1u) { char tn[4] = {' ', 'T', (char)('1' + t), 0}; text_cat(d, tn); } }
        text_cat(d, " LINES "); int_text((int32_t)ch_fx_blocks_used(), n); text_cat(d, n); text_cat(d, "/"); int_text(CH_FX_BLOCKS, n); text_cat(d, n);
        text_cat(d, " ("); for (unsigned t = 0; t <= CH_MASTER; ++t) { int_text((int32_t)ch_fx_blocks_held(t), n); text_cat(d, n); text_cat(d, t < CH_MASTER ? " " : ")"); }   /* each track's, the master's */
        if (ui.spd_show < 2) cv_text(126, 60, &FONT_T, d, C_GRAY);
        {   /* the load governor: its voice limit now, and the voices it has faded to stay in time */
            char g[48] = "VOICES "; char m[12];
            int_text(sn->voice_cap, m); text_cat(g, m); text_cat(g, " FADED "); int_text(sn->shed, m); text_cat(g, m);
            text_cat(g, " MFX "); int_text(sn->master_us, m); text_cat(g, m);
            char g2[56] = "GAP "; int_text((int32_t)gap, m); text_cat(g2, m);         /* the longest wait between two blocks' starts: over the block's length (OF), one began late */
            text_cat(g2, " TICK "); int_text((int32_t)tkr, m); text_cat(g2, m);         /* the timer interrupt's longest run */
            text_cat(g2, " SUB "); int_text(sn->sub0_us, m); text_cat(g2, m); text_cat(g2, "/"); int_text(sn->subn_us, m); text_cat(g2, m);   /* a render's first sub-block / its others: far apart, the flash cache refilling */
            text_cat(g2, ch_limiter() ? " LIM ON" : " LIM OFF"); if (!ch_kit_core2()) text_cat(g2, " KC1"); text_cat(g2, ch_sub_get() == 64 ? " S64" : " S32");
#if defined(FM1B_CPU2) && FM1B_CPU2
            { text_cat(g2, " K"); int_text((int32_t)plat_cpu2_stack(), m); text_cat(g2, m); }   /* (K: how deep the second core's stack has gone, bytes of 4864) */
#endif
            if (ui.spd_show < 2) cv_text(6, 70, &FONT_T, g, C_GRAY);
            /* the guard: broken values caught, the last one's track, stage and kind */
            unsigned nb = sn->master_bad;
            for (unsigned t = 0; t < CH_TRACKS; ++t) for (unsigned k = 0; k < CH_SLOTS; ++k) nb += sn->bad[t][k];
            static const char *const ST[CH_SLOTS] = {"ENG", "FX1", "FX2", "FX3", "FX4"};
            static const char *const KIND[4] = {"", "NAN", "INF", "HUGE"};
            char b2[40] = "BAD "; int_text((int32_t)nb, m); text_cat(b2, m);
            if (nb && sn->bad_kind) {
                char tn[4] = {' ', 'T', (char)('1' + sn->bad_track), 0}; text_cat(b2, tn);
                text_cat(b2, " "); text_cat(b2, ST[sn->bad_slot < CH_SLOTS ? sn->bad_slot : 0]);
                text_cat(b2, " "); text_cat(b2, KIND[sn->bad_kind & 3]);
            }
            if (ui.spd_show < 2) cv_text(150, 70, &FONT_T, b2, nb ? C_ACC : C_GRAY);
            cv_blit(0, 54);
            cv_begin(240, 18, C_BLACK);
            cv_text(6, 2, &FONT_T, g2, C_GRAY);
        }
        cv_blit(0, 134);
        cv_begin(240, 8, C_BLACK); cv_blit(0, 232);
        return;
    }
    if (ui_knobs >= 2) {   /* the page's knobs on the panel the PANEL's are on: the track's colour, white (the line), teal (a modulator), grey (no track's) */
        int own = ui.view == V_DEVICE && !ui.mfx;
        uint16_t pc = own ? TRACK_COL[ui.track] : ui.view == V_DEVICE && ui.mfx ? C_ACC : ui.view == V_GEN ? RGB(176, 124, 208) : ui.view == V_MOD ? RGB(51, 178, 159) : ui.view == V_MIX ? C_GLO : C_GRAY;   /* (the line generator violet, the modulators teal: their curves and marks the same teal, C_MOD) */
        knob_panel(0, pc); knob_fill = PANEL_GREY; knob_frame = own || (ui.view == V_DEVICE && ui.mfx) || ui.view == V_GEN || ui.view == V_MOD || ui.view == V_MIX ? pc : 0;
    }
    /* an effect's page with no effect on in the whole chain (the track's four places, or the master's: each empty or
     * turned off): the panel empty, and on it what brings one on */
    int fxhint = ui.view == V_DEVICE && ui.slot != CH_MATERIAL && ui.slot < CH_SLOTS;
    if (fxhint) for (unsigned k = CH_FX1; k < CH_SLOTS; ++k) if (ts->device[k] != CH_DEV_BYPASS && !ts->bypass[k]) fxhint = 0;
    for (unsigned i = 0; i < 4; ++i) {
        int32_t x = ui_knobs >= 2 ? 4 + (int32_t)i * 58 : 1 + (int32_t)i * 60;
        if (fxhint) { tile4(x, ENC_LBL[i], 0, 0, 0, 0, 0, 0, 0); continue; }
        if (ui.view == V_DEVICE) {
            dev_cell c;
            if (!dev_cell_at(tile_param(i), &c)) { tile4(x, ENC_LBL[i], 0, 0, 0, 0, 0, 0, 0); continue; }
            knob_mod = ui_knobs && c.nmap ? c.mnow : -1;
            tile4(x, ENC_LBL[i], c.name, c.val, c.md, c.bv, c.nmap ? c.bv : c.lv, c.bip, !ts->bypass[ui.slot]);
            knob_mod = -1;
            if (c.nmap && !ui_knobs) mod_bar(x + 3, 52, 78 - 6 - 5, 3, &c);   /* (the tile's bar: x + 3, 52 wide, 3 high, at 72; this at 67) */
        } else if (ui.view == V_GEN) {                     /* the line generator's settings */
            unsigned k = ui.gen_page * 4 + i;
            float bv = gen_tile(k, val);
            tile4(x, ENC_LBL[i], GEN_NAME[k], val, 0, bv, bv, 0, 1);
        } else if (ui.view == V_PERFORM) {                 /* the master effects' amounts */
            unsigned slot = CH_FX1 + i, dev = ts->device[slot];
            int p = dev == CH_DEV_BYPASS ? -1 : ch_fx_amount_param(dev);
            if (p < 0) { tile4(x, ENC_LBL[i], dev == CH_DEV_BYPASS ? "EMPTY" : ch_device_info(dev)->name, "-", 0, 0, 0, 0, 0); continue; }
            const ch_param_desc *pd = &ch_device_info(dev)->param[p];
            unsigned row = ch_row(slot, dev);
            float v = ts->param[row][p], live = sn->mvalue[slot][p];
            value_text(pd, v, 0, val);
            { char fv[16]; if (ch_fx_value_text(dev, (unsigned)p, v, ts->mode[row][p], fv)) { val[0] = 0; text_cat(val, fv); } }
            tile4(x, ENC_LBL[i], ch_device_info(dev)->name, val, pd->name, v, live, pd->kind == CH_BI, !ts->bypass[slot]);
        } else if (ui.view == V_MOD) {
            unsigned m = ui.mod_page / 2, p = (ui.mod_page & 1) * PER_PAGE + i;
            if (p >= CH_MOD_PARAMS) { tile4(x, ENC_LBL[i], 0, 0, 0, 0, 0, 0, 0); continue; }
            const ch_param_desc *pd = ch_mod_param_info(ts->mod_type[m], p);
            float v = ts->mod_param[m][p];
            value_text(pd, v, ts->mod_mode[m][p], val);
            { char mv[16]; if (ch_mod_value_text(ts->mod_type[m], p, v, ts->mod_mode[m][p], mv)) { val[0] = 0; text_cat(val, mv); } }
            const char *md = pd->modes ? pd->modes[ts->mod_mode[m][p]] : 0;
            const char *lb = ch_mod_label(ts->mod_type[m], (unsigned)(ts->mod_param[m][2] + 0.5f), p);
            tile4(x, ENC_LBL[i], lb ? lb : pd->name, val, md, v, i == 0 ? sn->mod[ui.track][m] * 0.5f + 0.5f : v, pd->kind == CH_BI, 1);
        } else if (ui.mix_page == 9) {                       /* AUTOPILOT */
            if (i == 0) tile4(x, ENC_LBL[i], "AUTOPILOT", ap.on ? "ON" : "OFF", ap.on && !sn->playing ? "PRESS PLAY" : 0, ap.on ? 1.0f : 0.0f, ap.on ? 1.0f : 0.0f, 0, 1);
            else if (i == 1) { int_text(AP_BARS[ap.bars_i], val); tile4(x, ENC_LBL[i], "EVERY", val, AP_BARS[ap.bars_i] == 1 ? "BAR" : "BARS", (float)ap.bars_i * 0.25f, (float)ap.bars_i * 0.25f, 0, 1); }
            else if (i == 2) { int_text((int32_t)(ap.variety * 100 + 0.5f), val); tile4(x, ENC_LBL[i], "VARIETY", val, ap.variety >= 0.5f ? "NEW LINES" : 0, ap.variety, ap.variety, 0, 1); }
            else tile4(x, ENC_LBL[i], "WHEN OFF", ap.revert ? "REVERT" : "OVERRIDE", ap.revert ? "AS IT WAS" : "KEEP IT", ap.revert ? 0.0f : 1.0f, ap.revert ? 0.0f : 1.0f, 0, 1);
        } else if (ui.mix_page == 6) {                       /* SONG: the entry under edit */
            unsigned e = ui.song_edit; char v[12];
            if (i == 0) { int_text(e + 1, v); text_cat(v, "/"); { char q[4]; int_text(song.len, q); text_cat(v, q); } tile4(x, ENC_LBL[i], "ENTRY", v, song.on && song.pos == e && sn->playing ? "PLAYING" : 0, (float)e / (PJ_SONG_MAX - 1), (float)e / (PJ_SONG_MAX - 1), 0, 1); }
            else if (i == 1) { if (song.e[e].pat) { v[0] = 'P'; v[1] = 0; char q[4]; int_text(song.e[e].pat, q); text_cat(v, q); } else { v[0] = 0; text_cat(v, "END"); } tile4(x, ENC_LBL[i], "PATTERN", v, 0, (float)song.e[e].pat / PJ_PATTERNS, (float)song.e[e].pat / PJ_PATTERNS, 0, song.e[e].pat != 0); }
            else if (i == 2) { int_text(song.e[e].bars ? song.e[e].bars : 4, v); tile4(x, ENC_LBL[i], "BARS", v, 0, (float)song.e[e].bars / 64, (float)song.e[e].bars / 64, 0, song.e[e].pat != 0); }
            else tile4(x, ENC_LBL[i], "SONG", song.on ? "ON" : "OFF", 0, song.on ? 1.0f : 0.0f, song.on ? 1.0f : 0.0f, 0, 1);
        } else if (ui.mix_page == 7) {                       /* DISPLAY */
            char v[12] = "";
            if (i == 0) tile4(x, ENC_LBL[i], "COLOURS", SP_PAL[ui.mono], 0, ui.mono ? 1.0f : 0.0f, ui.mono ? 1.0f : 0.0f, 0, 1);
            else if (i == 1) { if (ui.spec_tone == 24) text_cat(v, "WHITE"); else { int_text((int32_t)ui.spec_tone * 15, v); } tile4(x, ENC_LBL[i], "TONE", v, ui.spec_tone == 24 ? 0 : "DEGREES", (float)ui.spec_tone / 24, (float)ui.spec_tone / 24, 0, ui.mono); }
            else if (i == 2) tile4(x, ENC_LBL[i], "PANEL ROW", ui.panel_white ? "WHITE" : "TRACK", 0, ui.panel_white ? 1.0f : 0.0f, ui.panel_white ? 1.0f : 0.0f, 0, 1);
            else tile4(x, ENC_LBL[i], "MIDI SYNC", ui.sync == SYNC_SEND ? (ui.sync_host ? "DIRECT" : "SEND") : ui.sync == SYNC_FOLLOW ? "FOLLOW" : "OFF", ui.sync == SYNC_FOLLOW ? (syn.have ? "LOCKED" : "NO CLOCK") : ui.sync_host ? (plat_usb_host_state() == USBH_CONNECTED ? "LINKED" : "LOOKING") : 0, (float)ui.sync / 2, (float)ui.sync / 2, 0, 1);
        } else if (ui.mix_page == 10) {                      /* RECORD */
            static const char *const CI[3] = {"1 BAR", "OFF", "2 BARS"};
            if (i == 0) tile4(x, ENC_LBL[i], "MICRO", ui.rec_micro ? "ON" : "OFF", ui.rec_micro ? "AS PLAYED" : "ON THE STEP", ui.rec_micro ? 1.0f : 0.0f, ui.rec_micro ? 1.0f : 0.0f, 0, 1);
            else if (i == 1) { float cv = ui.count_off == 1 ? 0.0f : ui.count_off == 0 ? 0.5f : 1.0f; tile4(x, ENC_LBL[i], "COUNT IN", CI[ui.count_off], 0, cv, cv, 0, 1); }
            else if (i == 2) { int on = ch_limiter(); tile4(x, ENC_LBL[i], "LIMITER", on ? "ON" : "OFF", 0, (float)on, (float)on, 0, 1); }
            else { static const char *const MN[4] = {"M1", "M2", "M3", "M4"}; unsigned q = ui.snd_off >> 1;   /* HOLD FX: the slot, and what is in it */
                   tile4(x, ENC_LBL[i], "HOLD FX", MN[q], fx_name(ch_track_get(CH_MASTER), CH_FX1 + q), (float)q / 3.0f, (float)q / 3.0f, 0, 1); }
        } else if (ui.mix_page == 8) {                       /* SEND TO: each track, ALL or M2..M4 */
            static const char *const TN8[4] = {"TRACK 1", "TRACK 2", "TRACK 3", "TRACK 4"};
            static const char *const SV[4] = {"ALL", "M2", "M3", "M4"};
            static const char *const SW[4] = {"FX 1 2 3 4", "FX 2 3 4", "FX 3 4", "FX 4"};   /* the master effects the hit goes through */
            unsigned k = ch_track_get(i)->ss_dest; k = k > CH_SS_FX && k <= CH_SS_LAST ? k - CH_SS_FX : 0;
            tile4(x, ENC_LBL[i], TN8[i], SV[k], SW[k], 0, 0, 0, 1);
        } else if (ui.mix_page == 4) {
            if (i == 0) tile4(x, ENC_LBL[i], "KEY", KEY_NAME[ui.key_root], "OR PLAY IT", (float)ui.key_root / 12.0f, (float)ui.key_root / 12.0f, 0, 1);
            else if (i == 1) tile4(x, ENC_LBL[i], "KEY SCALE", SCALE_NAME[ui.key_scale], 0, (float)ui.key_scale / (SCALES - 1), (float)ui.key_scale / (SCALES - 1), 0, ui.key_root != 0);
            else if (i == 2) { char v[12]; int_text((int32_t)(ch_tempo() + 0.5f), v); tile4(x, ENC_LBL[i], "TEMPO", v, "BPM", (ch_tempo() - 24) / 260.0f, (ch_tempo() - 24) / 260.0f, 0, 1); }
            else { char v[12]; int_text((int32_t)(50 + ch_swing() * 25 + 0.5f), v); text_cat(v, "%"); tile4(x, ENC_LBL[i], "SWING", v, 0, ch_swing(), ch_swing(), 0, 1); }
        } else {
            static const char *const TN[4] = {"TRACK 1", "TRACK 2", "TRACK 3", "TRACK 4"};
            const ch_track_state *tt = ch_track_get(ui.mix_page == 3 ? ui.track : i);
            unsigned so = ui.mix_page == 3 && i ? send_other(ui.track, i - 1u) : 0;   /* the SENDS page: K1 FX SEND, K2..K4 the other tracks */
            float v = ui.mix_page == 0 ? tt->level : ui.mix_page == 1 ? tt->pan : ui.mix_page == 2 ? tt->filter : i == 0 ? tt->tofx : tt->send[so];
            int bip = ui.mix_page == 1 || ui.mix_page == 2;
            float live = v;                              /* (where a modulator or a macro has it now) */
            if (ui.mix_page < 3) live = fm_clampf(v + ch_snapshot_get()->mixmod[i][ui.mix_page], bip ? -1.0f : 0.0f, 1);
            int_text((int32_t)(v * 100 + (v < 0 ? -0.5f : 0.5f)), val);
            tile4(x, ENC_LBL[i], ui.mix_page == 3 ? glo_name(3, i) : TN[i], val, 0, v, live, bip, 1);
        }
    }
    if (fxhint) {                                        /* how the effects come on: grey, on the empty panel */
        static const char *const H[3] = {"PRESS FX TO PICK FX SLOT", "TURN ALGORITHM TO PICK FX", "HOLD FX TO DEACTIVATE/ACTIVATE SLOT"};
        for (unsigned k = 0; k < 3; ++k) {
            const felucca_font_t *f = text_w(&FONT_XS, H[k]) > 220 ? &FONT_T : &FONT_XS;
            cv_text((240 - text_w(f, H[k])) / 2, 18 + (int32_t)k * 15, f, H[k], RGB(150, 150, 156));   /* (a light-to-mid grey: there to be read, not to shout) */
        }
    }
    if (ui.view == V_DEVICE && ui.slot >= CH_SLOTS) {     /* an empty place: how to fill it */
        const char *h1 = "TURN ALGO", *h2 = "TO ADD AN EFFECT";
        cv_rect(20, 22, 200, 36, C_BLACK);
        cv_text((240 - text_w(&FONT_B, h1)) / 2, 24, &FONT_B, h1, C_HI);
        cv_text((240 - text_w(&FONT_XS, h2)) / 2, 42, &FONT_XS, h2, C_GRAY);
    }
    knob_fill = 0; knob_frame = 0;
    cv_blit(0, 54);
    if (!env_applies() && ui.view != V_MOD && !fxviz_on() && !genviz_on() && !glolist_on()) { cv_begin(240, 18, C_BLACK); cv_blit(0, 134); }   /* (not on the LFO page: its drawing starts on these rows, and blacked first each frame its MAPPED TO line flickered) */   /* down to the footer (the step view's lanes reach here;
                                                                             * the envelope display draws these rows itself) */
    cv_begin(240, 8, C_BLACK); cv_blit(0, 232);           /* and under it (the step view's tiles reach the bottom) */
}

/* ---- the envelope display (SEQ + ENV): the engine's amplitude envelope drawn where the track footer is,
 * on the EDIT page of a SYNTH, SAMPLE or GRAIN track; a white bar at the right is the level now ---- */
static int env_applies(void)
{
    if (!ui.env_show || ui.view != V_DEVICE || ui.mfx || ui.slot != CH_MATERIAL) return 0;
    unsigned dev = ch_track_get(ui.track)->device[CH_MATERIAL];
    return dev == CH_DEV_SYNTH || dev == CH_DEV_POLY || dev == CH_DEV_GRAIN;
}
static void sec_text(char *o, float sec)
{
    o[0] = 0;
    if (sec < 1) { int_text((int32_t)(sec * 1000 + 0.5f), o); text_cat(o, "MS"); }
    else { int_text((int32_t)sec, o); text_cat(o, "."); char d[4]; int_text((int32_t)(sec * 10 + 0.5f) % 10, d); text_cat(o, d); text_cat(o, "S"); }
}
static float env_shape(float x, float c) { return c == 0 ? x : fm_powf(x < 1e-6f ? 1e-6f : x, fm_powf(2.0f, c * 2.5f)); }   /* SAMPLE's CURVE */
static float fexp(float v, float lo, float hi) { return lo * fm_powf(hi / lo, fm_clampf(v, 0, 1)); }
static void body_env(void)
{
    const ch_track_state *ts = ch_track_get(ui.track);
    const ch_snapshot *sn = ch_snapshot_get();
    unsigned dev = ts->device[CH_MATERIAL], row = ch_row(CH_MATERIAL, dev);
    const float *p = ts->param[row];
    /* the segments: attack, decay (to the sustain level), a hold while the note lasts, release; seconds and a level */
    float att, dec, sus, rel, crv = 0; int hold, expo;
    if (dev == CH_DEV_SYNTH && ui.env_show == 2) {        /* the filter envelope: F ATTACK, F DECAY, to nothing */
        att = p[15] > 0.005f ? fexp(p[15], 0.001f, 2.0f) : 0.001f; dec = fexp(ts->param[CH_SYNTH_XROW][0], 0.02f, 8.0f);
        sus = 0; rel = 0; hold = 0; expo = 1;
    } else if (dev == CH_DEV_SYNTH) {
        unsigned mode = (unsigned)(p[7] + 0.5f);          /* PING, GATE, DRONE */
        att = p[8] > 0.005f ? fexp(p[8], 0.001f, 2.0f) : 0.001f; dec = fexp(p[5], 0.02f, 8.0f);
        sus = mode == 0 ? 0 : 1; rel = mode == 1 ? dec : 0; if (mode == 0) rel = 0; else dec = 0;
        hold = mode != 0; expo = 1;
        if (mode == 2) rel = 0;
    } else if (dev == CH_DEV_POLY) {
        att = p[7] * p[7] * 4.0f; dec = p[8] * p[8] * 4.0f; sus = fm_clampf(p[9], 0, 1); crv = p[10]; rel = dec; hold = 1; expo = 0;
        if (att < 0.001f) att = 0.001f; if (dec < 0.001f) dec = 0.001f;
    } else {
        att = p[10] * p[10] * 6.0f; if (att < 0.003f) att = 0.003f; dec = 0; sus = 1; rel = fexp(p[7], 0.02f, 6.0f); hold = 1; expo = 1;   /* (the cloud's ATTACK and RELEASE) */
    }
    enum { X0 = 6, W = 224, Y0 = 16, H = 72, BAND = 80 };
    /* drawn twice: rows 0..79, then 80..97 (the canvas holds 80 rows); each band blitted whole, so nothing blinks */
    for (int band = 0; band < 2; ++band) {
    cv_begin(240, band ? 98 - BAND : BAND, C_BLACK);
    cv_oy = band ? -BAND : 0;
    char l[48] = "", v[12];
    text_cat(l, "A "); sec_text(v, att); text_cat(l, v);
    if (dec > 0) { text_cat(l, "  D "); sec_text(v, dec); text_cat(l, v); }
    if (hold && dev != CH_DEV_GRAIN) { text_cat(l, "  S "); int_text((int32_t)(sus * 100 + 0.5f), v); text_cat(l, v); text_cat(l, "%"); }
    if (rel > 0) { text_cat(l, "  R "); sec_text(v, rel); text_cat(l, v); }
    if (!band) { cv_text(X0, 2, &FONT_T, ui.env_show == 2 ? "FILTER ENVELOPE" : "ENVELOPE", C_GRAY); cv_text(234 - text_w(&FONT_T, l), 2, &FONT_T, l, C_HI); }
    /* the widths: the timed parts share what the hold leaves, each at least a sliver */
    int hw = hold ? 44 : 0; float total = att + dec + rel; if (total <= 0) total = 1;
    int aw = (int)((float)(W - hw) * att / total), dw = (int)((float)(W - hw) * dec / total);
    if (aw < 2) aw = 2; if (dec > 0 && dw < 2) dw = 2;
    int rw = W - hw - aw - dw; if (rw < 0) rw = 0;
    cv_rect(X0, Y0 + H, W, 1, C_LINE);
    if (hold) { cv_rect(X0 + aw + dw, Y0, 1, H, C_LINE); cv_rect(X0 + aw + dw + hw, Y0, 1, H, C_LINE); }   /* the held part */
    int32_t py = -1;
    for (int x = 0; x < W; ++x) {
        float lv;
        if (x < aw) lv = expo ? (float)(x + 1) / (float)aw : env_shape((float)(x + 1) / (float)aw, crv);
        else if (x < aw + dw) {
            float u = (float)(x - aw + 1) / (float)dw;
            lv = expo ? sus + (1 - sus) * fm_powf(0.001f, u) : sus + (1 - sus) * (1 - env_shape(u, -crv));
        } else if (x < aw + dw + hw) lv = sus;
        else if (rw > 0 && rel > 0) {
            float u = (float)(x - aw - dw - hw + 1) / (float)rw;
            lv = expo ? sus * fm_powf(0.001f, u) : sus * (1 - env_shape(u, -crv));
        } else lv = sus;
        int32_t y = Y0 + H - 1 - (int32_t)(lv * (float)(H - 1) + 0.5f);
        cv_rect(X0 + x, y + 1, 1, Y0 + H - y - 1, tint565(TRACK_COL[ui.track], 0.25f));   /* (a quarter of the line's colour, the track's: it was a grey) */                      /* under the curve */
        if (py < 0) py = y;
        int32_t a = y < py ? y : py, b = y < py ? py : y;
        cv_rect(X0 + x, a, 1, b - a + 1, TRACK_COL[ui.track]);
        py = y;
    }
    if (ui.env_show != 2) {   /* the level now */
        int32_t h = (int32_t)(sn->env[ui.track] * (float)H + 0.5f);
        cv_rect(234, Y0, 4, H, RGB(20, 22, 30));
        if (h > 0) cv_rect(234, Y0 + H - h, 4, h, C_WHITE);
    }
    cv_blit(0, 134 + band * BAND);
    }
    cv_oy = 0;
}

/* ---- the MOD view's lower half: the modulator drawn, and where it is mapped ----
 * Left, an LFO with a wave (SINE .. SQUARE): one cycle as its knobs make it (SKEW, AMOUNT, OFFSET), teal, a white dot riding it
 * where the engine's phase is now. Anything else (STEPS, DRIFT, TRIG RND) has no fixed shape: its output of the last few seconds
 * scrolls by instead, the dot the value now. Right, every parameter it is mapped to, its slot and depth. */
static int8_t mv_hist[96];                              /* the trace, -127..127 (bytes: the RAM is short) */
static uint8_t mv_hist_at, mv_hist_who = 0xFF;
static float mv_wave(const float *p, unsigned shape, float ph)   /* the engine's wave_at and finish, for the drawing */
{
    ph -= (float)(int)ph; if (ph < 0) ph += 1;
    float k = 0.5f + 0.49f * p[5];
    float q = ph < k ? 0.5f * ph / k : 0.5f + 0.5f * (ph - k) / (1 - k), w;
    switch (shape) {
    case CH_LFO_TRI: w = q < 0.5f ? 4 * q - 1 : 3 - 4 * q; break;
    case CH_LFO_SAWUP: w = 2 * q - 1; break;
    case CH_LFO_SAWDN: w = 1 - 2 * q; break;
    case CH_LFO_SQUARE: w = q < 0.5f ? 1.0f : -1.0f; break;
    default: w = fm_sinf(6.2831853f * (q - 0.25f)); break;
    }
    return fm_clampf(w * p[1] + p[3], -1, 1);
}
static void body_modviz(void)
{
    const ch_track_state *ts = ch_track_get(ui.track);
    const ch_snapshot *sn = ch_snapshot_get();
    unsigned m = ui.mod_page / 2, type = ts->mod_type[m];
    const float *p = ts->mod_param[m];
    unsigned shape = (unsigned)(p[2] + 0.5f);
    int wave = (type == CH_MOD_WAVE || type == CH_MOD_RANDOM) && shape < CH_LFO_STEPS;
    float now = sn->mod[ui.track][m];
    float ph = (float)sn->mod_ph[ui.track][m] * (1.0f / 256);   /* read once a frame: both strips draw the same moment */
    /* as many cycles as about a second of it holds (one at least, sixteen at most), drawn from a cycle's start at the left: the
     * faster, the more cycles, so the dot, crossing them in the engine's own count, keeps an even pace however fast it runs */
    float hz = wave ? ch_mod_rate_hz(ui.track, m) : 0;
    int ncyc = (int)(hz + 0.5f); ncyc = ncyc < 1 ? 1 : ncyc > 16 ? 16 : ncyc;
    float at = (float)(sn->mod_cyc[ui.track][m] % (unsigned)ncyc) + ph;   /* the dot: which drawn cycle, and how far into it */
    int fast = wave && 103.0f * hz / (float)ncyc * 0.04f > 20.0f;   /* past 20 pixels a frame even so (over ~75 Hz, beyond the knob): no dot */
    uint8_t who = (uint8_t)(ui.track * CH_MODS + m);
    int8_t nq = (int8_t)(fm_clampf(now, -1, 1) * 127);
    if (who != mv_hist_who) { mv_hist_who = who; for (unsigned i = 0; i < 96; ++i) mv_hist[i] = nq; mv_hist_at = 0; }
    mv_hist[mv_hist_at] = nq; mv_hist_at = (uint8_t)((mv_hist_at + 1) % 96);
    enum { X0 = 6, W = 112, Y0 = 4, H = 90 };              /* the drawing's box, rows from 134 */
    static const int32_t BY[2] = {134, 214}, BH[2] = {80, 18};
    for (unsigned bd = 0; bd < 2; ++bd) {
        cv_begin(240, (uint32_t)BH[bd], C_BLACK);
        int32_t o = BY[bd] - 134;                          /* (this band's first row, in the view's rows) */
        cv_rect(X0, Y0 - o, W, 1, C_LINE); cv_rect(X0, Y0 + H - 1 - o, W, 1, C_LINE);
        cv_rect(X0, Y0 - o, 1, H, C_LINE); cv_rect(X0 + W - 1, Y0 - o, 1, H, C_LINE);
        int32_t cy = Y0 + H / 2 - o, amp = H / 2 - 5;
        for (int32_t x = X0 + 2; x < X0 + W - 2; x += 4) cv_rect(x, cy, 2, 1, C_LINE);   /* zero, dashed */
        int32_t px = -1, py = 0, dx, dy;
        if (wave) {
            float xn = at / (float)ncyc * (float)(W - 9);    /* where the dot is, pixels into the curve */
            for (int32_t i = 0; i < W - 8; ++i) {
                float v = mv_wave(p, shape, (float)i * (float)ncyc / (float)(W - 9));
                int32_t x = X0 + 4 + i, y = cy - (int32_t)(v * (float)amp);
                if (px >= 0) { int32_t a = py < y ? py : y, b = py < y ? y : py; cv_rect(x, a, 1, b - a + 1, C_MOD); }
                px = x; py = y;
            }
            dx = X0 + 4 + (int32_t)xn;
        } else {
            for (int32_t i = 0; i < 96; ++i) {
                float v = (float)mv_hist[(mv_hist_at + (unsigned)i) % 96] * (1.0f / 127);
                int32_t x = X0 + 4 + i * (W - 9) / 95, y = cy - (int32_t)(v * (float)amp);
                if (px >= 0) { int32_t a = py < y ? py : y, b = py < y ? y : py; cv_rect(px, a, x - px > 0 ? x - px : 1, 1, C_MOD); cv_rect(x, a, 1, b - a + 1, C_MOD); }
                px = x; py = y;
            }
            dx = X0 + W - 5;
        }
        dy = cy - (int32_t)((wave ? mv_wave(p, shape, ph) : now) * (float)amp);   /* on the curve, at the phase */
        if (!fast) cv_rect(dx - 2, dy - 2, 5, 5, C_WHITE);
        /* where it is mapped */
        cv_text(126, 3 - o, &FONT_T, "MAPPED TO", C_GRAY);
        unsigned shown = 0, total = 0;
        static const char *const SL[CH_SLOTS] = {"ENG", "FX1", "FX2", "FX3", "FX4"};
        for (unsigned k = 0; k < CH_MAPS; ++k) {
            float d = ts->map[k].depth;
            if (d == 0 || ts->map[k].mod != m || ts->map[k].slot > CH_MIXSLOT) continue;
            ++total;
            if (shown >= 6) continue;
            int32_t y = 17 + (int32_t)shown * 12 - o;
            unsigned sl = ts->map[k].slot;
            char l[24] = "";
            if (sl == CH_MIXSLOT) { text_cat(l, "MIX "); text_cat(l, MIXMOD_NAME[ts->map[k].param % 3u]); }   /* the track's mixer */
            else {
                if (ts->device[sl] == CH_DEV_BYPASS || ts->map[k].param >= ch_device_info(ts->device[sl])->params) continue;
                text_cat(l, SL[sl]); text_cat(l, " "); text_cat(l, param_name(ui.track, sl, ts->map[k].param));
            }
            unsigned n = 0; while (l[n]) ++n;
            while (n > 4 && text_w(&FONT_T, l) > 82) l[--n] = 0;
            cv_text(126, y, &FONT_T, l, C_HI);
            char v[12] = ""; int32_t pc = (int32_t)(d * 100 + (d < 0 ? -0.5f : 0.5f));
            if (pc > 0) text_cat(v, "+"); { char q[8]; int_text(pc, q); text_cat(v, q); }
            cv_text(234 - text_w(&FONT_T, v), y, &FONT_T, v, C_MOD);
            ++shown;
        }
        if (!total) cv_text(126, 17 - o, &FONT_T, "NOTHING YET", C_DIM), cv_text(126, 29 - o, &FONT_T, "HOLD LFO + TURN", C_DIM), cv_text(126, 41 - o, &FONT_T, "A KNOB TO MAP", C_DIM);
        else if (total > shown) { char v[16] = "+"; char q[6]; int_text((int32_t)(total - shown), q); text_cat(v, q); text_cat(v, " MORE"); cv_text(126, 17 + 6 * 12 - o, &FONT_T, v, C_DIM); }
        cv_blit(0, (uint32_t)BY[bd]);
    }
}
/* the footer as a MIXER (EDIT + GLO toggles it): each track a pair of vertical meters, left and right, in LED segments from
 * -48 dB to +6 (the top segment white: over 0 dB), a white mark at its fader (unity a little below the top); under them its number in its colour (white
 * on a hit) and its sound, M when muted */
static void footer_mixer(void)
{
    const ch_snapshot *sn = ch_snapshot_get();
    cv_begin(240, 80, C_BLACK);
    enum { MT = 3, MH = 50, SEG = 3, SW = 48 };          /* the meters' top and height; a segment and its gap; a strip's width (five: the tracks and the RETURN) */
    for (unsigned t = 0; t <= CH_TRACKS; ++t) {
        int ret = t == CH_TRACKS;                          /* the RETURN: what the tracks send into the master effects */
        int32_t cx = SW / 2 + (int32_t)t * SW;
        const ch_track_state *ts = ch_track_get(ret ? 0 : t);
        uint16_t scol = ret ? RGB(163, 109, 174) : TRACK_COL[t];   /* (the RETURN: a violet no track has) */
        for (unsigned c = 0; c < 2; ++c) {
            float lv = ret ? sn->ret_level[c] : sn->level[t][c], db = lv > 1e-6f ? 6.0206f * fm_log2f(lv) : -96.0f;
            float u = fm_clampf((db + 48.0f) / 54.0f, 0, 1);
            int32_t x = cx - 9 + (int32_t)c * 11, lit = (int32_t)(u * (float)(MH / SEG) + 0.5f);
            for (int32_t k = 0; k < MH / SEG; ++k) {
                int32_t y = MT + MH - (k + 1) * SEG;
                uint16_t col = k < lit ? (k == MH / SEG - 1 ? C_WHITE : scol) : C_LINE;
                cv_rect(x, y, 7, SEG - 1, col);
            }
        }
        {   /* the fader: the level knob (gain (v / 0.8)^2, 0.8 unity), on the meters' scale */
            float g = ret ? ch_return() : ts->level, db = g > 1e-4f ? 12.0412f * fm_log2f(g * 1.25f) : -96.0f;
            int32_t y = MT + MH - (int32_t)(fm_clampf((db + 48.0f) / 54.0f, 0, 1) * (float)MH);
            cv_rect(cx - 12, y, 24, 1, C_WHITE);
        }
        char n[4] = {'T', (char)('1' + t), 0};
        if (ret) { n[0] = 'R'; n[1] = 'E'; n[2] = 'T'; n[3] = 0; }
        int chosen = ui.mix_ret ? ret : t == ui.track;      /* the strip ALGO turns */
        uint16_t tc = ret ? (chosen ? C_WHITE : C_HI) : hit_on(t) ? TRACK_HIT[t] : chosen ? C_WHITE : TRACK_COL[t];
        cv_text(cx - text_w(&FONT_XS, n) / 2, MT + MH + 2, &FONT_XS, n, tc);
        if (!ret && ts->mute) cv_text(cx + 12, MT + MH + 2, &FONT_XS, "M", C_WHITE);
        char nm[16]; const char *src = ret ? "MASTER FX" : material_name(t); unsigned k = 0;
        while (src[k] && k < 15) { nm[k] = src[k]; ++k; } nm[k] = 0;
        while (k > 1 && text_w(&FONT_T, nm) > SW - 4) nm[--k] = 0;
        cv_text(cx - text_w(&FONT_T, nm) / 2, MT + MH + 15, &FONT_T, nm, chosen ? C_HI : C_GRAY);
        if (chosen) {                       /* the chosen track: a box around its strip (white text alone is easy to miss) */
            int32_t bx = (int32_t)t * SW + 1;
            cv_rect(bx, 0, SW - 2, 1, C_WHITE); cv_rect(bx, 79, SW - 2, 1, C_WHITE);
            cv_rect(bx, 0, 1, 80, C_WHITE); cv_rect(bx + SW - 3, 0, 1, 80, C_WHITE);
        }
    }
    cv_blit(0, 152);
}
/* the macros (OCT- + OCT+), in place of the bottom of the screen while they are on K1..K4 or one is being assigned:
 * a box each, its value as a bar, how many parameters it moves; tall (80): also the first of them */
static int mac_shown(void) { return ui.mac_play || ui.mac_arm; }
static int mixer_on(void)
{
    return ui.mixfoot && !mac_shown() && !panel_on() && !env_applies()
        && (ui.view == V_DEVICE || (ui.view == V_MIX && ui_knobs < 2) || ui.view == V_PERFORM);   /* (not the LINE page: its line is drawn there; nor the GLO pages: their list is there) */
}
static int mixpage_on(void) { return ui.view == V_DEVICE && mixer_on(); }
/* the MIXER page (EDIT's second layout): the whole screen under the header. Above, two thirds: the four tracks and the RETURN,
 * tall meters with the fader's mark, the RETURN on the left (SELECT its level), K1-K4 the tracks' levels. Below, a third: the four master effects' amounts, K1-K4 while FX is
 * held. Drawn in four bands of 50 rows (the canvas holds 80). */
static void body_mixpage(void)
{
    const ch_snapshot *sn = ch_snapshot_get();
    const ch_track_state *ms = ch_track_get(CH_MASTER);
    int fxh = (int)(ui.btn >> B_FX & 1u);
    enum { MT = 71, MH = 78, SEG = 3, SW = 48, FY = 180 };   /* the meters' top and height (absolute rows); a strip's width; the effects' top */
    for (int32_t oy = 40; oy < 240; oy += 50) {
        cv_begin(240, 50, C_BLACK);
#define Y(v) ((int32_t)(v) - oy)
        cv_text(6, Y(41), &FONT_XS, "MIXER", C_WHITE);
        for (unsigned t = 0; t <= CH_TRACKS; ++t) {
            int ret = t == CH_TRACKS;
            int32_t col = ret ? 0 : (int32_t)t + 1;         /* the RETURN on the left, then the tracks */
            int32_t cx = SW / 2 + col * SW;
            const ch_track_state *ts = ch_track_get(ret ? 0 : t);
            uint16_t scol = ret ? RGB(163, 109, 174) : TRACK_COL[t];   /* (the RETURN: a violet no track has) */
            int chosen = ui.mix_ret ? ret : t == ui.track;
            { char kl[4] = {'K', (char)('1' + t), 0}; cv_text(cx - text_w(&FONT_T, ret ? "SEL" : kl) / 2, Y(54), &FONT_T, ret ? "SEL" : kl, !ret && fxh ? C_DIM : C_GRAY); }   /* (clear of the meters and the fader's mark at full: the meters start 3 lower, the names sit 4 higher) */
            for (unsigned c = 0; c < 2; ++c) {
                float lv = ret ? sn->ret_level[c] : sn->level[t][c], db = lv > 1e-6f ? 6.0206f * fm_log2f(lv) : -96.0f;
                float u = fm_clampf((db + 48.0f) / 54.0f, 0, 1);
                int32_t x = cx - 11 + (int32_t)c * 12, lit = (int32_t)(u * (float)(MH / SEG) + 0.5f);
                for (int32_t k = 0; k < MH / SEG; ++k) {
                    int32_t y = MT + MH - (k + 1) * SEG;
                    cv_rect(x, Y(y), 10, SEG - 1, k < lit ? (k == MH / SEG - 1 ? C_WHITE : scol) : C_LINE);
                }
            }
            {   /* the fader: the level knob, on the meters' scale */
                float g = ret ? ch_return() : ts->level, db = g > 1e-4f ? 12.0412f * fm_log2f(g * 1.25f) : -96.0f;
                int32_t y = MT + MH - (int32_t)(fm_clampf((db + 48.0f) / 54.0f, 0, 1) * (float)MH);
                cv_rect(cx - 15, Y(y - 1), 30, 2, C_WHITE);
            }
            char n[4] = {'T', (char)('1' + t), 0};
            if (ret) { n[0] = 'R'; n[1] = 'E'; n[2] = 'T'; n[3] = 0; }
            uint16_t tc = ret ? (chosen ? C_WHITE : C_HI) : hit_on(t) ? TRACK_HIT[t] : chosen ? C_WHITE : TRACK_COL[t];
            cv_text(cx - text_w(&FONT_XS, n) / 2, Y(MT + MH + 2), &FONT_XS, n, tc);
            if (!ret && ts->mute) cv_text(cx + 12, Y(MT + MH + 2), &FONT_XS, "M", C_WHITE);
            char nm[16]; const char *src = ret ? "MASTER FX" : material_name(t); unsigned k = 0;
            while (src[k] && k < 15) { nm[k] = src[k]; ++k; } nm[k] = 0;
            while (k > 1 && text_w(&FONT_T, nm) > SW - 4) nm[--k] = 0;
            cv_text(cx - text_w(&FONT_T, nm) / 2, Y(MT + MH + 15), &FONT_T, nm, chosen ? C_HI : C_GRAY);
        }
        /* the master effects: a box each under its track's strip, the amount as a bar */
        for (unsigned i = 0; i < 4; ++i) {
            int32_t x = ((int32_t)i + 1) * SW + 1, w = SW - 2;
            unsigned slot = CH_FX1 + i, dev = ms->device[slot];
            int p = dev == CH_DEV_BYPASS ? -1 : ch_fx_amount_param(dev);
            int on = p >= 0 && !ms->bypass[slot];
            cv_rect(x, Y(FY), w, 58, fxh ? C_WHITE : C_LINE); cv_rect(x + 1, Y(FY + 1), w - 2, 56, C_BLACK);
            { char l[4] = {'M', (char)('1' + i), 0}; cv_text(x + 4, Y(FY + 3), &FONT_T, l, fxh ? C_WHITE : C_GRAY); }
            char nm[16]; const char *src = dev == CH_DEV_BYPASS ? "EMPTY" : ch_device_info(dev)->name; unsigned k = 0;
            while (src[k] && k < 15) { nm[k] = src[k]; ++k; } nm[k] = 0;
            while (k > 1 && text_w(&FONT_XS, nm) > w - 6) nm[--k] = 0;
            cv_text(x + 4, Y(FY + 14), &FONT_XS, nm, on ? C_WHITE : C_DIM);
            char val[16] = "-"; float v = 0;
            if (p >= 0) {
                const ch_param_desc *pd = &ch_device_info(dev)->param[p];
                unsigned row = ch_row(slot, dev);
                v = ms->param[row][p];
                value_text(pd, v, 0, val);
                { char fv[16]; if (ch_fx_value_text(dev, (unsigned)p, v, ms->mode[row][p], fv)) { val[0] = 0; text_cat(val, fv); } }
                k = 0; while (val[k]) ++k;
                while (k > 1 && text_w(&FONT_XS, val) > w - 6) val[--k] = 0;
            }
            cv_text(x + 4, Y(FY + 30), &FONT_XS, val, on ? C_HI : C_DIM);
            cv_rect(x + 4, Y(FY + 48), w - 8, 4, C_LINE);
            if (p >= 0) cv_rect(x + 4, Y(FY + 48), (int32_t)(fm_clampf(v, 0, 1) * (float)(w - 8)), 4, on ? C_ACC : C_DIM);
        }
        cv_text(4, Y(FY + 3), &FONT_T, "K1-K4:", C_GRAY);   /* what the knobs are on, under the RETURN */
        cv_text(4, Y(FY + 14), &FONT_T, "LEVELS", fxh ? C_DIM : C_WHITE);
        cv_text(4, Y(FY + 30), &FONT_T, "FX HELD:", C_GRAY);
        cv_text(4, Y(FY + 41), &FONT_T, "MASTER FX", fxh ? C_WHITE : C_DIM);
#undef Y
        cv_blit(0, (uint32_t)oy);
    }
}
static void macro_strip(int32_t y0, int32_t h)
{
    cv_begin(240, (uint32_t)h, C_BLACK);
    for (unsigned m = 0; m < CH_MACROS; ++m) {
        int32_t x = 1 + (int32_t)m * 60, w = 58;
        int arm = ui.mac_arm == m + 1;
        cv_rect(x, 0, w, h, arm ? C_WHITE : C_LINE);
        cv_rect(x + 1, 1, w - 2, h - 2, C_BLACK);
        unsigned n = 0; const char *first = 0; unsigned ft = 0, fs = 0, fp = 0;
        for (unsigned t = 0; t < CH_TRACKS; ++t) {
            const ch_track_state *ts = ch_track_get(t);
            for (unsigned k = 0; k < CH_MAPS; ++k)
                if (ts->map[k].depth != 0 && ts->map[k].mod == CH_MODS + m && ts->map[k].slot <= CH_MIXSLOT) {
                    if (!n) { first = "x"; ft = t; fs = ts->map[k].slot; fp = ts->map[k].param; }
                    ++n;
                }
        }
        char l[8] = "M"; char d[8]; int_text((int32_t)m + 1, d); text_cat(l, d);
        cv_text(x + 4, 2, &FONT_XS, l, arm ? C_WHITE : C_MOD);
        int_text((int32_t)(ch_macro(m) * 100 + 0.5f), d);
        cv_text(x + w - 4 - text_w(&FONT_B, d), h >= 60 ? 16 : 14, &FONT_B, d, n ? C_WHITE : C_DIM);
        if (h >= 60) {
            char c[16] = ""; if (arm && !n) text_cat(c, "TURN A KNOB"); else { int_text((int32_t)n, c); text_cat(c, n == 1 ? " MAP" : " MAPS"); }
            cv_text(x + 4, 36, &FONT_T, c, arm ? C_WHITE : C_GRAY);
            if (first) {
                char nm[24] = {'T', (char)('1' + ft), ' ', 0};
                text_cat(nm, fs == CH_MIXSLOT ? MIXMOD_NAME[fp % 3u] : param_name(ft, fs, fp));
                unsigned k = 0; while (nm[k]) ++k;
                while (k > 3 && text_w(&FONT_T, nm) > w - 8) nm[--k] = 0;
                cv_text(x + 4, 50, &FONT_T, nm, C_DIM);
            }
        }
        int32_t bw = w - 8, by = h - 7;
        cv_rect(x + 4, by, bw, 3, C_LINE);
        cv_rect(x + 4, by, (int32_t)(ch_macro(m) * (float)bw), 3, C_MOD);
    }
    cv_blit(0, (uint32_t)y0);
}
/* ---- the LINE page's lower half: the line itself, in the page's violet. The track's steps as a roll, lane 1 (the
 * root) at the bottom, the whole length across; an accent nearer white, a soft note fainter, a slide joined to the note
 * it goes to, an octave jump a cap over or under its note. Only what plays is drawn (the steps DENSITY holds back were
 * there as faint marks, and read as notes). The playhead crosses it, the note under it white ---- */
static int genviz_on(void) { return ui.view == V_GEN && !mac_shown(); }   /* (whatever EDIT's layout: with the MIXER footer chosen the line page showed the mixer where its line is) */
static void body_genviz(void)
{
    const ch_snapshot *sn = ch_snapshot_get();
    const ch_seq_state *sq = ch_seq_get(ui.track);
    const unsigned len = sq->len ? sq->len : 16;
    const uint16_t col = RGB(176, 124, 208), tint = tint565(col, 0.25f), soft = mix565(tint, col, 0.5f);
    enum { BAND = 80, T = 6, LH = 9, B = T + CH_LANES * LH };
    const unsigned mask = ui.gen_lanes ? ui.gen_lanes : 0xFFu;
    const int stepg = gen_gate_steps(ui.track);            /* (each note as long as its own step's GATE) */
    float gate = 1;                                        /* GATE on a DRUM: the note's length, as it sounds (its decay): a fifth of its step to all of it */
    { int gp = gen_gate_param(ui.track); const ch_track_state *ts = ch_track_get(ui.track);
      if (gp >= 0) gate = 0.2f + 0.8f * fm_clampf(ts->param[ch_row(CH_MATERIAL, ts->device[CH_MATERIAL])][gp], 0, 1); }
    const int ps = sn->playing ? sn->step[ui.track] : -1;
#define GX(i) (FX0 + (int32_t)((i) * (unsigned)FXW / len))
    for (int band = 0; band < 2; ++band) {
        cv_begin(240, band ? 98 - BAND : BAND, C_BLACK);
        cv_oy = band ? -BAND : 0;
        cv_rect(FX0 - 4, 2, FXW + 8, 1, C_LINE); cv_rect(FX0 - 4, 95, FXW + 8, 1, C_LINE);
        cv_rect(FX0 - 4, 2, 1, 94, C_LINE); cv_rect(FX0 + FXW + 3, 2, 1, 94, C_LINE);
        for (unsigned l = 0; l < CH_LANES; ++l)             /* the lanes the line may use: a rule each, the root's a step brighter */
            if (mask >> l & 1u) cv_rect(FX0, T + (int32_t)(CH_LANES - 1 - l) * LH + 4, FXW, 1, l ? GREY5(1) : GREY5(3));
        for (unsigned i = 4; i < len; i += 4) cv_rect(GX(i), T, 1, B - T, i & 15u ? GREY5(1) : GREY5(4));
        if (ps >= 0 && (unsigned)ps < len) cv_rect(GX((unsigned)ps), T, GX((unsigned)ps + 1) - GX((unsigned)ps), B - T, tint);   /* the playhead */
        for (unsigned l = 0; l < CH_LANES; ++l) {
            int32_t y = T + (int32_t)(CH_LANES - 1 - l) * LH;
            for (unsigned i = 0; i < len; ++i) {
                const ch_step *st = &sq->step[l][i];
                if (!st->vel) continue;
                int32_t x = GX(i), w = GX(i + 1) - x - 1; if (w < 1) w = 1;
                uint16_t c = (int)i == ps ? C_WHITE : st->vel >= 100 ? mix565(col, C_WHITE, 0.7f * (float)(st->vel - 100) * (1.0f / 27)) : mix565(soft, col, fm_clampf((float)(st->vel - 40) * (1.0f / 60), 0, 1));   /* (by its velocity, smoothly: the page's colour at 100, toward white up to 127, fainter under 100) */
                if (st->tie && i) { cv_rect(x - 1, y + 3, w + 1, 3, c); continue; }   /* held from the step before: joined to it, a little thinner */
                if (!(i + 1 < len && sq->step[l][i + 1].vel && sq->step[l][i + 1].tie)) { w = (int32_t)((float)w * (stepg ? (float)ch_step_gate(st) * 0.01f : gate) + 0.5f); if (w < 2) w = 2; }   /* (held into the next step: its whole length) */
                cv_rect(x, y + 2, w, 5, c);
                if (ch_step_oct(st)) cv_rect(x, ch_step_oct(st) > 0 ? y : y + 8, w, 1, c);   /* the jump: a cap over (up) or under */
            }
        }
        cv_rect(FX0, B + 2, FXW, 1, GREY5(6));               /* the ruler: beats, the bars white; bars numbered (one bar: its beats) */
        for (unsigned i = 0; i < len; i += 4) {
            int32_t x = GX(i); int bar = !(i & 15u);
            cv_rect(x, B + 2, 1, bar ? 5 : 3, bar ? C_WHITE : C_GRAY);
            if (len > 16 && !bar) continue;
            char n[4]; int_text((int32_t)(len > 16 ? i / 16u : i / 4u) + 1, n);
            cv_text(x + 3, B + 7, &FONT_T, n, bar ? C_HI : C_GRAY);
        }
        if (ps >= 0 && (unsigned)ps < len) cv_rect(GX((unsigned)ps), B + 1, GX((unsigned)ps + 1) - GX((unsigned)ps), 3, C_WHITE);
        cv_blit(0, 134 + band * BAND);
    }
#undef GX
    cv_oy = 0;
}
static void footer(void)
{
    if (mac_shown()) { macro_strip(152, 80); return; }
    if (ui.mixfoot) { footer_mixer(); return; }
    const ch_snapshot *sn = ch_snapshot_get();
    cv_begin(240, 80, C_BLACK);
    /* the four tracks: hit, sound, level, and the pattern lane with its playhead */
    for (unsigned t = 0; t < CH_TRACKS; ++t) {
        int32_t y = 2 + (int32_t)t * 19;
        char n[4] = {'T', (char)('1' + t), 0};
        uint16_t tc = hit_on(t) ? TRACK_HIT[t] : C_WHITE;   /* (no track's name dimmed for not being the chosen one: that one has a white mark at the row's left) */
        if (t == ui.track) cv_rect(0, y + 4, 2, 10, C_WHITE);
        cv_text(4, y + 2, &FONT_XS, n, tc);
        {   /* the sound's name, cut to its room (the steps take the width the level meter had: the MIXER view has the meters) */
            char nm[16]; const char *src = material_name(t); unsigned k = 0;
            while (src[k] && k < 15) { nm[k] = src[k]; ++k; } nm[k] = 0;
            while (k > 1 && text_w(&FONT_XS, nm) > 58) nm[--k] = 0;
            cv_text(20, y + 2, &FONT_XS, nm, C_HI);
        }
        const ch_seq_state *sq = ch_seq_get(t);
        int ps = sn->step[t];
        unsigned page = ps >= 0 ? (unsigned)ps / 16u : (t == ui.track ? ui.step_page : 0);
        if (page * 16u >= sq->len) page = 0;                 /* never a page past the pattern's end */
        for (unsigned w = 0; w < 16; ++w) {
            unsigned idx = page * 16u + w;
            int32_t x = 93 + (int32_t)w * 9;   /* (every gap alike; steps 5-8 and 13-16 on the lighter grey, as in TRACKS) */
            unsigned top = 0, snd = 0;                       /* the lanes merged: a hit if any lane has one */
            for (unsigned l = 0; l < CH_LANES; ++l) if (sq->step[l][idx].vel) { if (sq->step[l][idx].vel > top) top = sq->step[l][idx].vel; if (ch_step_send(&sq->step[l][idx]) > snd) snd = ch_step_send(&sq->step[l][idx]); }
            uint16_t c = idx >= sq->len ? C_PAST : top ? TRACK_COL[t] : (w & 4u) ? C_BEAT : C_CELL;
            if ((int)idx == ps) c = C_WHITE;
            cv_rect(x, y + 3, 8, 12, c);
            if (top && snd && idx < sq->len) { int32_t sh = (12 * (int32_t)snd + 7) / 15; for (int32_t py = 1; py < sh; py += 2) cv_rect(x, y + 14 - py, 8, 1, C_BLACK); }   /* SEND: stripes from the bottom */
        }
        if (ch_track_get(t)->mute) cv_text(81, y + 2, &FONT_XS, "M", C_WHITE);
    }
    cv_blit(0, 152);
}


/* ---- the pattern out to the computer, for a MIDI file (the Transfer page's command MX_REQ) ----
 * The page asks; this sends what it needs and the page writes the file: a short head (the tempo, the swing, each track's
 * engine and name), then the pattern as it is saved (ch_pattern_pack: the four tracks' lanes, notes and steps). It goes
 * in the update's framing, MX_CHUNK bytes a frame: F0 pack7(00 59 51, offset16, total16, count, the bytes, ~sum) F7 */
enum { MX_CHUNK = 96, MX_HEAD = 4 + CH_TRACKS * 14 };
static void mx_frame(unsigned off, const uint8_t *d, unsigned k, unsigned total)
{
    uint8_t body[9 + MX_CHUNK], f[2 + ((9 + MX_CHUNK) * 8 + 6) / 7];
    unsigned n = 0, s = 0, m = 0, nb = 0; uint32_t acc = 0;
    body[n++] = 0; body[n++] = 0x59; body[n++] = MX_REQ;
    body[n++] = (uint8_t)off; body[n++] = (uint8_t)(off >> 8); body[n++] = (uint8_t)total; body[n++] = (uint8_t)(total >> 8); body[n++] = (uint8_t)k;
    for (unsigned i = 0; i < k; ++i) body[n++] = d[i];
    for (unsigned i = 3; i < n; ++i) s += body[i];
    body[n++] = (uint8_t)~s;
    f[m++] = 0xF0;
    for (unsigned i = 0; i < n; ++i) {                    /* 8 bits to 7, low bits first (the update's pack7) */
        acc |= (uint32_t)body[i] << nb; nb += 8;
        while (nb >= 7) { f[m++] = (uint8_t)(acc & 0x7Fu); acc >>= 7; nb -= 7; }
    }
    if (nb) f[m++] = (uint8_t)(acc & 0x7Fu);
    f[m++] = 0xF7;
    plat_sysex_send(f, m);
}
static void midi_export(void)
{
    if (pj_loading()) return;                            /* (the buffer is the loader's meanwhile) */
    uint8_t *b = pj_buf.raw; unsigned n = 0;
    unsigned bpm = (unsigned)(ch_tempo() * 10.0f + 0.5f);
    b[n++] = 1; b[n++] = (uint8_t)bpm; b[n++] = (uint8_t)(bpm >> 8); b[n++] = (uint8_t)(ch_swing() * 255.0f + 0.5f);
    for (unsigned t = 0; t < CH_TRACKS; ++t) {
        const ch_track_state *ts = ch_track_get(t); const char *nm = material_name(t);
        b[n++] = (uint8_t)ts->device[CH_MATERIAL]; b[n++] = (uint8_t)(ts->mute ? 1u : 0u) | (ts->device[CH_MATERIAL] == CH_DEV_KIT ? 2u : 0u) | (ts->device[CH_MATERIAL] == CH_DEV_DRUM ? 4u : 0u);
        unsigned k = 0; for (; k < 12 && nm[k]; ++k) b[n++] = (uint8_t)nm[k];
        for (; k < 12; ++k) b[n++] = 0;
    }
    unsigned p = ch_pattern_pack(b + n, PLAT_STORE_MAX - n);
    if (!p) { app_say("MIDI", "TOO LARGE TO SEND"); mx_frame(0, b, 0, 0); return; }
    unsigned total = n + p;
    for (unsigned off = 0; off < total; off += MX_CHUNK) mx_frame(off, b + off, total - off > MX_CHUNK ? MX_CHUNK : total - off, total);
    app_say("MIDI", "SENT TO THE COMPUTER"); ui.redraw = 1;
}

/* ---- the saved project out to the computer, for the plugin (the Transfer page's command PX_REQ) ----
 * The page asks for a piece at an offset and gets one frame back, so nothing queues up: kind 1, a store object by
 * its number (the project, the tracks, the presets, the patterns: as SAVE left them); kind 2, the sample bank as the
 * flash holds it (its record's sector, then its payload); kind 3, the page is done (the screen says so). The frame:
 * F0 pack7(00 59 52, kind, number, offset24, total24, count, the bytes, ~sum) F7; total 0: there is no such piece.
 * An object is read into the loader's buffer (there is no RAM for another): while a project is coming in or a pattern
 * waits there for its bar, no answer, and the page asks again. */
enum { PX_CHUNK = 96 };
static void project_export(const uint8_t *d, uint32_t n)
{
    if (n < 12) return;
    unsigned kind = d[6], id = d[7], s = 0, m = 0, nb = 0, q = 0; uint32_t off = (uint32_t)d[8] | (uint32_t)d[9] << 8 | (uint32_t)d[10] << 16, total = 0, acc = 0;
    const uint8_t *src = 0;
    if (kind == 3) { app_say("PROJECT", "SENT TO THE COMPUTER"); ui.redraw = 1; return; }
    if (pj_loading() || ch_snapshot_get()->pattern_pending) return;
    if (kind == 1 && id < OBJ_NOBJ) {
        int have = plat_store_load(id, pj_buf.raw, sizeof pj_buf.raw);
        if (have > 0) { total = (uint32_t)have; src = pj_buf.raw; }
    } else if (kind == 2) {
        const uint8_t *r = plat_data(SMP_BASE); smp_rec h;
        if (r) {
            memcpy(&h, r, sizeof h);
            if (h.magic == SMP_MAGIC && h.len >= SMP_HEAD && h.len <= SMP_MAX && h.hcrc == smp_crc32(0, (const uint8_t *)&h, 12)) { total = (SMP_DATA - SMP_BASE) + h.len; src = r; }
        }
    }
    unsigned k = off < total ? (total - off > PX_CHUNK ? PX_CHUNK : (unsigned)(total - off)) : 0;
    uint8_t body[13 + PX_CHUNK], f[2 + ((13 + PX_CHUNK) * 8 + 6) / 7];
    body[q++] = 0; body[q++] = 0x59; body[q++] = PX_REQ; body[q++] = (uint8_t)kind; body[q++] = (uint8_t)id;
    body[q++] = (uint8_t)off; body[q++] = (uint8_t)(off >> 8); body[q++] = (uint8_t)(off >> 16);
    body[q++] = (uint8_t)total; body[q++] = (uint8_t)(total >> 8); body[q++] = (uint8_t)(total >> 16); body[q++] = (uint8_t)k;
    for (unsigned i = 0; i < k; ++i) body[q++] = src[off + i];
    for (unsigned i = 3; i < q; ++i) s += body[i];
    body[q++] = (uint8_t)~s;
    f[m++] = 0xF0;
    for (unsigned i = 0; i < q; ++i) {                    /* 8 bits to 7, low bits first (the update's pack7) */
        acc |= (uint32_t)body[i] << nb; nb += 8;
        while (nb >= 7) { f[m++] = (uint8_t)(acc & 0x7Fu); acc >>= 7; nb -= 7; }
    }
    if (nb) f[m++] = (uint8_t)(acc & 0x7Fu);
    f[m++] = 0xF7;
    plat_sysex_send(f, m);
}

void app_ui_init(void)
{
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.splash = 1; ui.boot_ms = plat_ms();
    logo_sound_start();                                  /* the sound logo, in step with the picture */
    logo_frame(0);
    ui.redraw = 1;
}
void app_splash_replay(void) { app_ui_init(); }
void app_splash(void) { lcd_fill(0, 0, 240, 240, C_BLACK); }   /* (APP_SPLASH, before app_init: black, as the logo starts) */
void app_frame(void)
{
    static uint32_t last_meter;
    uint32_t now = plat_ms();
    if (pj_loading()) { pj_load_pump(); ui.redraw = 1; }
    else {
        const ch_snapshot *sn = ch_snapshot_get();
        if (ui.was_playing && !sn->playing) pat_flush(1);  /* stopped: the pattern's edits to the flash */
        ui.was_playing = sn->playing;
        song_frame();
    }

    input();
    ap_tick();
    ann_tick(now);
    if (ui.splash) {
        if (now - ui.boot_ms < LOGO_MS) {
#if !defined(PLAT_HOST)
            static uint32_t swept_at;                      /* felucca-bsp's power-on sweep, once, as the name comes */
            if (now - ui.boot_ms >= (uint32_t)(LOGO_HIT * 1000.0f) - 60u && swept_at != ui.boot_ms) {
                swept_at = ui.boot_ms; fm1_led_anim_start(1);
            }
#endif
            logo_frame(now - ui.boot_ms); return;
        }
        ui.splash = 0; ui.redraw = 1;
        lcd_fill(0, 0, 240, 240, C_BLACK);
        splash_over(now);
    }
    if (ui.hint_until) {                                 /* the first power-on: where HELP is */
        {                                                /* (no time limit: it waits for a button) */
            if (ui.redraw) {
                static const char *const L[5] = {"HOLD GLO AND PRESS SAVE", "FOR HELP, AT ANY TIME", "FOR A NEW, EMPTY PROJECT:", "HOLD SEQ, PRESS SAVE, THEN REC", "PRESS ANY BUTTON TO CONTINUE"};
                cv_begin(240, 64, C_BLACK);
                help_box(70, 2, 42, 20, "GLO", 0); help_box(128, 2, 42, 20, "SAVE", 0);
                cv_rect(116, 11, 8, 2, C_WHITE); cv_rect(119, 8, 2, 8, C_WHITE);
                cv_text(120 - text_w(&FONT_XS, L[0]) / 2, 32, &FONT_XS, L[0], C_WHITE);
                cv_text(120 - text_w(&FONT_XS, L[1]) / 2, 46, &FONT_XS, L[1], C_WHITE);
                cv_blit(0, 62);
                cv_begin(240, 64, C_BLACK);              /* then how to start from nothing (the factory project is what is there) */
                cv_text(120 - text_w(&FONT_XS, L[2]) / 2, 8, &FONT_XS, L[2], C_GRAY);
                cv_text(120 - text_w(&FONT_XS, L[3]) / 2, 22, &FONT_XS, L[3], C_WHITE);
                cv_text(120 - text_w(&FONT_T, L[4]) / 2, 50, &FONT_T, L[4], C_GRAY);
                cv_blit(0, 126);
                ui.redraw = 0;
            }
            return;
        }
    }
    if (ui.msg_until && now >= ui.msg_until) { ui.msg_until = 0; ui.redraw = 1; }
    if (lib.open) {                                     /* the LIBRARY: the whole screen, ~10 times a second */
        if (ui.redraw || now - last_meter > 100u) { lib_draw(now); ui.redraw = 0; last_meter = now; }
    } else if (brw.open) {                              /* the BROWSER: the same */
        if (ui.redraw || now - last_meter > 100u) { brw_draw(now); ui.redraw = 0; last_meter = now; }
    } else if (ui.redraw || now - last_meter > 40u) {     /* meters, playheads and live values ~25 times a second */
        header();
        body();
        if (ui.view != V_STEP && ui.view != V_SONG && ui.view != V_SCOPE && ui.view != V_HELP && !panel_on() && !mixpage_on()) { if (mac_shown()) footer(); else if (genviz_on()) body_genviz(); else if (ui.view == V_MOD) body_modviz(); else if (glolist_on()) body_glolist(); else if (env_applies()) body_env(); else if (fxviz_on() && !panel_on()) body_fxviz(); else footer(); }
        ui.redraw = 0;
        last_meter = now;
    }
}
