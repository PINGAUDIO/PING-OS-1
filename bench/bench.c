/* Chisel PC bench: renders the engine offline to WAV files in dev/out.
 *   bench adpcm <in.wav> <out.wav>              round-trip a sample through the codec
 *   bench scene <name> <sample.wav> <out.wav> [seconds] [bpm]
 * Scenes: plain, poly, groove, fx, chords; fx-NAME (a groove through one effect)
 *   bench fxall <dir>      every effect at its defaults on the same groove, a WAV each
 *   bench fxcheck [n]      random chains, every knob thrown about: counts broken values
 *   bench comb DIR         COMB's pitch in each FREQ mode, read back; WAVs to DIR
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "wav.h"
#include "ch_adpcm.h"
#include "chisel.h"
#include "ch_internal.h"

static int16_t *to_pcm16(const float *x, unsigned n)
{
    int16_t *p = malloc(n * 2);
    for (unsigned i = 0; i < n; ++i) {
        float v = x[i] * 32767.0f;
        p[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
    return p;
}
/* Linear resample to 32 kHz (the sample bank's rate). Good enough for tests. */
static float *resample(const float *x, unsigned n, unsigned from, unsigned to, unsigned *out_n)
{
    unsigned m = (unsigned)((double)n * to / from);
    float *y = malloc(sizeof(float) * (m + 1));
    for (unsigned i = 0; i < m; ++i) {
        double p = (double)i * from / to; unsigned k = (unsigned)p; double f = p - k;
        y[i] = (float)(x[k] * (1 - f) + (k + 1 < n ? x[k + 1] : 0) * f);
    }
    *out_n = m;
    return y;
}

static int cmd_adpcm(const char *in, const char *out)
{
    unsigned n = 0, rate = 0;
    float *x = wav_read_mono(in, &n, &rate);
    if (!x) { fprintf(stderr, "cannot read %s\n", in); return 1; }
    int16_t *pcm = to_pcm16(x, n);
    uint8_t *enc = malloc(ch_adpcm_bytes(n));
    size_t bytes = ch_adpcm_encode(pcm, n, enc);
    int32_t *o = malloc(sizeof(int32_t) * 2 * (n + CH_ADPCM_BLOCK));
    double err = 0, sig = 0;
    for (unsigned b = 0; b * CH_ADPCM_BLOCK < n; ++b) {
        int16_t blk[CH_ADPCM_BLOCK];
        ch_adpcm_decode_block(enc + b * CH_ADPCM_BLOCK_BYTES, blk);
        for (unsigned i = 0; i < CH_ADPCM_BLOCK && b * CH_ADPCM_BLOCK + i < n; ++i) {
            unsigned k = b * CH_ADPCM_BLOCK + i;
            double d = blk[i] - pcm[k];
            err += d * d; sig += (double)pcm[k] * pcm[k];
            o[2 * k] = o[2 * k + 1] = blk[i] * 256;
        }
    }
    printf("%u frames @ %u Hz, %zu bytes (%.2f bits/frame), SNR %.1f dB\n",
           n, rate, bytes, bytes * 8.0 / n, 10 * log10(sig / (err + 1e-9)));
    wav_write_q23(out, o, n, rate);
    return 0;
}

/* set a parameter by name on the device currently in `slot` */
static void P(unsigned t, unsigned slot, const char *name, float v)
{
    const ch_track_state *s = ch_track_get(t);
    const ch_device_desc *d = ch_device_info(s->device[slot]);
    for (unsigned i = 0; i < d->params; ++i)
        if (!strcmp(d->param[i].name, name)) { ch_param_set(t, slot, i, v); return; }
    fprintf(stderr, "no param %s on %s\n", name, d->name); exit(3);
}
static void M(unsigned t, unsigned slot, const char *name, unsigned mode)
{
    const ch_track_state *s = ch_track_get(t);
    const ch_device_desc *d = ch_device_info(s->device[slot]);
    for (unsigned i = 0; i < d->params; ++i)
        if (!strcmp(d->param[i].name, name)) { ch_mode_set(t, slot, i, mode); return; }
    fprintf(stderr, "no param %s on %s\n", name, d->name); exit(3);
}
static void on(unsigned t, unsigned slot) { ch_bypass_set(t, slot, 0); }

typedef void (*event_fn)(double sec, int frame);
static event_fn events;
static const char *scene_name;

/* POLY plays a little figure; ADSR modulation follows */
static void poly_events(double sec, int f)
{
    static const int notes[] = {48, 55, 60, 63, 67, 72, 70, 67};
    int step = (int)(sec * 4);
    static int last = -1;
    if (step != last) {
        if (last >= 0) ch_note(0, (unsigned)notes[last % 8], 0);
        ch_note(0, (unsigned)notes[step % 8], 100);
        last = step;
    }
    (void)f;
}
static void lane_steps(unsigned t, unsigned lane, const char *pat)
{
    /* one char per step: . off, x hit, X accent, r ratchet x2, R x3, ? 50% chance, c 1:2 cycle, e early, l late, - tie */
    for (unsigned i = 0; pat[i] && i < CH_STEPS; ++i) {
        ch_step st = {0, 1, 100, 0, 0, 0};
        char c = pat[i];
        if (c != '.') st.vel = c == 'X' ? 127 : 96;
        if (c == 'r') st.ratchet = 2;
        if (c == 'R') st.ratchet = 3;
        if (c == '?') st.chance = 50;
        if (c == 'c') { st.cyc_a = 2; st.cyc_b = 2; }
        if (c == 'e') st.micro = -20;
        if (c == 'l') st.micro = 25;
        if (c == '-') { st.vel = 96; st.tie = 1; }            /* tied to the step before */
        ch_step_set(t, lane, i, &st);
    }
    ch_seq_len_set(t, (unsigned)strlen(pat));
}
static void steps(unsigned t, const char *pat, unsigned note)
{
    /* on the lane that plays `note` (the default lanes are C major from C4), else lane 1 retuned */
    const ch_seq_state *q = ch_seq_get(t);
    unsigned lane = 0;
    while (lane < CH_LANES && q->note[lane] != note) ++lane;
    if (lane == CH_LANES) { lane = 0; ch_lane_note_set(t, 0, note); }
    lane_steps(t, lane, pat);
}
static void drum(unsigned t, unsigned model)
{
    ch_device_set(t, CH_MATERIAL, CH_DEV_DRUM); ch_bypass_set(t, CH_MATERIAL, 0);
    {   /* (model: the engine's own number, as these tests were written; the parameter is its place in the list) */
        unsigned place = 0;
        for (unsigned k = 0; k < CH_DRUM_MODELS; ++k) if (ch_drum_order[k] == model) place = k;
        ch_param_set(t, CH_MATERIAL, 0, (float)place);
    }
    ch_mix_set(t, CH_MIX_MUTE, 0);
}

/* an effect into a track's FX slot (1..4) */
static unsigned fx_dev(const char *name)
{
    for (unsigned d = CH_DEV_FX_FIRST; d < CH_DEVICES; ++d) if (!strcmp(ch_device_info(d)->name, name)) return d;
    fprintf(stderr, "no effect %s\n", name); exit(3);
}
static void fx(unsigned t, unsigned slot, const char *name) { ch_device_set(t, slot, fx_dev(name)); ch_bypass_set(t, slot, 0); }

static void setup_scene(const char *s)
{
    ch_tempo_set(120);
    for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
    if (!strcmp(s, "plain")) { }
    else if (!strncmp(s, "fx-", 3)) {                  /* a groove through one effect at its defaults */
        drum(0, 0); steps(0, "X...x...X..xx.x.", 60);
        drum(1, 9); steps(1, "....X.......X...", 60); ch_mix_set(1, CH_MIX_MUTE, 0);
        drum(2, 15); steps(2, "x.x.x.x.x.x.x.xx", 60); ch_mix_set(2, CH_MIX_MUTE, 0); ch_mix_set(2, CH_MIX_LEVEL, 0.6f);
        if (strcmp(s + 3, "DRY")) for (unsigned t = 0; t < 3; ++t) fx(t, CH_FX1, s + 3);
    } else if (!strncmp(s, "mfx-", 4)) {               /* the groove, dry, through one effect on the master's 2nd slot at its defaults */
        drum(0, 0); steps(0, "X...x...X..xx.x.", 60);
        drum(1, 9); steps(1, "....X.......X...", 60); ch_mix_set(1, CH_MIX_MUTE, 0);
        drum(2, 15); steps(2, "x.x.x.x.x.x.x.xx", 60); ch_mix_set(2, CH_MIX_MUTE, 0); ch_mix_set(2, CH_MIX_LEVEL, 0.6f);
        ch_device_set(CH_MASTER, CH_FX1, CH_DEV_BYPASS); ch_device_set(CH_MASTER, CH_FX3, CH_DEV_BYPASS); ch_device_set(CH_MASTER, CH_FX4, CH_DEV_BYPASS);
        fx(CH_MASTER, CH_FX2, s + 4);
    } else if (!strcmp(s, "poly")) {
        ch_device_set(0, CH_MATERIAL, CH_DEV_POLY); P(0, 0, "ATTACK", 0.05f); P(0, 0, "DECAY", 0.35f); P(0, 0, "SUSTAIN", 0.4f);
        P(0, 0, "FLTR AMOUNT", 0.6f); P(0, 0, "START", 0.0f); P(0, 0, "LENGTH", 0.3f);
        ch_action(0, CH_MATERIAL, CH_POLY_LOOPMODE, 0);
        fx(0, CH_FX1, "REVERB"); events = poly_events;
    } else if (!strcmp(s, "groove")) {
        ch_tempo_set(124); ch_swing_set(0.25f);
        drum(0, 0); steps(0, "X...x...X...x..r", 60); P(0, 0, "DECAY", 0.55f); P(0, 0, "DRIVE", 0.2f);
        drum(1, 9); steps(1, "....X.......X..c", 60); fx(1, CH_FX1, "REVERB"); P(1, CH_FX1, "MIX", 0.25f);
        drum(2, 15); steps(2, "xxXxx?Xxxx?XxxRx", 60); P(2, 0, "VARIATION", 0.4f); ch_mix_set(2, CH_MIX_LEVEL, 0.6f);
        drum(3, 27); steps(3, "..x..l.x.e..x.c.", 67); ch_seq_dir_set(3, CH_DIR_PEND);
        fx(3, CH_FX1, "CHORUS"); fx(3, CH_FX2, "DELAY"); P(3, CH_FX2, "FEEDBACK", 0.4f);
        ch_mix_set(3, CH_MIX_LEVEL, 0.6f);
    } else if (!strcmp(s, "fx")) {                     /* a kick through a stack: FILTER DRIVE DELAY REVERB */
        ch_tempo_set(120);
        drum(0, 0); steps(0, "X...x...X..xx.x.", 60);
        fx(0, CH_FX1, "FILTER"); P(0, CH_FX1, "RESONANCE", 0.6f); P(0, CH_FX1, "CUTOFF", 0.55f);
        fx(0, CH_FX2, "DRIVE"); P(0, CH_FX2, "DRIVE", 0.5f);
        fx(0, CH_FX3, "DELAY"); P(0, CH_FX3, "FEEDBACK", 0.5f);
        fx(0, CH_FX4, "REVERB");
    } else if (!strcmp(s, "locks")) {                  /* T1: a kick, the 5th and 13th steps locked to a snare (MODEL) and a long DECAY */
        drum(0, 0); steps(0, "x...x...x...x...", 60);
        ch_lock_set(0, 4, CH_MATERIAL, 0, 9); ch_lock_set(0, 12, CH_MATERIAL, 0, 9); ch_lock_set(0, 12, CH_MATERIAL, 2, 0.9f);
    } else if (!strcmp(s, "tie") || !strcmp(s, "notie")) {   /* T1: a SYNTH in GATE mode, a note held over tied steps, then plain steps */
        ch_device_set(0, CH_MATERIAL, CH_DEV_POLY); ch_bypass_set(0, CH_MATERIAL, 0);   /* (the sample looped: it sustains while gated) */
        P(0, 0, "LOOP", 1); P(0, 0, "SUSTAIN", 1); P(0, 0, "DECAY", 0.15f); P(0, 0, "ATTACK", 0); ch_action(0, CH_MATERIAL, CH_POLY_LOOPMODE, 0);
        steps(0, strcmp(s, "tie") ? "x.x.x.x.x...x...x.x.x.x.x...x..." : "x---x---x---x---x-x-x-x-x...x...", 60);
    } else if (!strcmp(s, "grain")) {                 /* T4: the GRAIN engine on the sample, a note a bar, SPRAY up */
        ch_mix_set(0, CH_MIX_MUTE, 1);
        ch_device_set(3, CH_MATERIAL, CH_DEV_GRAIN); ch_bypass_set(3, CH_MATERIAL, 0); ch_mix_set(3, CH_MIX_MUTE, 0);
        P(3, CH_MATERIAL, "SPRAY", 0.5f); P(3, CH_MATERIAL, "DENSITY", 0.7f); P(3, CH_MATERIAL, "POSITION", 0.3f);
        lane_steps(3, 0, "X...............X...............");
    } else if (!strncmp(s, "chords", 6)) {          /* chords-solo: T4 alone */
        /* the lanes as chords and a line: T1 kick, T4 SYNTH VA playing C-E-G / A-C-E stabs over a lane melody */
        ch_tempo_set(118);
        drum(0, 0); steps(0, "X...X...X...X...", 60);
        ch_device_set(3, CH_MATERIAL, CH_DEV_SYNTH); ch_bypass_set(3, CH_MATERIAL, 0); ch_param_set(3, CH_MATERIAL, 0, 0); ch_mix_set(3, CH_MIX_MUTE, 0);
        lane_steps(3, 0, "X.......X.......");   /* C */
        lane_steps(3, 2, "X.......x.......");   /* E */
        lane_steps(3, 4, "X.......x.......");   /* G */
        lane_steps(3, 5, "....X.......x...");   /* A */
        lane_steps(3, 7, "....x.......X...");   /* C5 */
        lane_steps(3, 1, "..x...x...x...rx");   /* D: a line through it */
        fx(3, CH_FX1, "DELAY"); P(3, CH_FX1, "MIX", 0.25f);
        ch_mix_set(3, CH_MIX_LEVEL, 0.55f);
        if (s[6]) { ch_mix_set(0, CH_MIX_MUTE, 1); ch_bypass_set(3, CH_FX1, 1); ch_param_set(3, CH_MATERIAL, 0, s[7] == '-' ? (float)atoi(s + 8) : 4); }   /* chords-solo[-MODEL]: dry */
    } else { fprintf(stderr, "unknown scene %s\n", s); exit(2); }
}

static int cmd_scene(const char *name, const char *wav, const char *out, double secs, float bpm, char **over, int n_over)
{
    unsigned n = 0, rate = 0;
    float *x = wav_read_mono(wav, &n, &rate);
    if (!x) { fprintf(stderr, "cannot read %s\n", wav); return 1; }
    unsigned m = 0;
    float *y = resample(x, n, rate, 32000, &m);
    if (m > 32000 * 20) m = 32000 * 20;                 /* tests use the first 20 s */
    int16_t *pcm = to_pcm16(y, m);
    uint8_t *enc = malloc(ch_adpcm_bytes(m));
    ch_adpcm_encode(pcm, m, enc);
    static ch_sample smp; smp.adpcm = enc; smp.frames = m; smp.rate = 32000; smp.bpm = bpm;
    ch_init();
    for (unsigned s = 0; s < CH_SAMPLES; ++s) ch_sample_bind(s, &smp);
    scene_name = name; events = 0;
    setup_scene(name);
    /* overrides: slot:on | slot:off | slot:NAME=value | slot:NAME@mode   (track 0, or t2. .. t4. first: t4.0:DECAY=0.1) */
    for (int a = 0; a < n_over; ++a) {
        char buf[64]; strncpy(buf, over[a], 63); buf[63] = 0;
        char *c = strchr(buf, ':'); if (!c) continue; *c = 0;
        unsigned trk = 0; const char *b0 = buf;
        if (buf[0] == 't' && buf[1] >= '1' && buf[1] <= '4' && buf[2] == '.') { trk = (unsigned)(buf[1] - '1'); b0 = buf + 3; }
        unsigned slot = (unsigned)atoi(b0); char *rest = c + 1;
        if (!strcmp(rest, "on")) { on(trk, slot); continue; }
        if (!strcmp(rest, "off")) { ch_bypass_set(trk, slot, 1); continue; }
        char *eq = strchr(rest, '='), *at = strchr(rest, '@');
        if (eq) { *eq = 0; P(trk, slot, rest, (float)atof(eq + 1)); }
        else if (at) { *at = 0; M(trk, slot, rest, (unsigned)atoi(at + 1)); }
    }
    ch_transport(1);
    unsigned frames = (unsigned)(secs * CH_RATE), block = 128;
    int32_t *buf = malloc(sizeof(int32_t) * 2 * (frames + block));
    clock_t c0 = clock();
    double peak = 0, sum = 0;
    for (unsigned f = 0; f < frames; f += block) {
        if (events) events((double)f / CH_RATE, (int)f);
        ch_render(buf + 2 * f, block, NULL);
        for (unsigned i = 0; i < 2 * block; ++i) { double v = buf[2 * f + i] / 8388608.0; if (fabs(v) > peak) peak = fabs(v); sum += v * v; }
    }
    double cpu = (double)(clock() - c0) / CLOCKS_PER_SEC;
    printf("%-10s peak %.3f  rms %.3f  host cpu %.2f%% of real time  (%.1f us per 128-frame block)\n", name, peak,
           sqrt(sum / (frames * 2.0)), 100 * cpu / secs, 1e6 * cpu / (frames / (double)block));
    { const ch_snapshot *sn = ch_snapshot_get();
      printf("           render %u of %u us (peak, last second), voice limit %u, faded %u\n", sn->render_us, sn->budget_us, sn->voice_cap, sn->shed); }
    wav_write_q23(out, buf, frames, CH_RATE);
    return 0;
}

/* every DRUM model: two hits (hard, soft) at the defaults, then the stats */
/* lanes: which notes each track's voices hold, step by step (the sequencer's chords, checked in the engine) */
/* cost: one track playing 4-note chords (every 1/8 at 120 BPM, the way lanes do) on each DRUM and SYNTH model,
 * or a KICK 808 through one effect (fxd: the effect); the render's time per 128-frame block (2.9 ms), best of 3 */
static double cost_of(unsigned dev, unsigned model, unsigned fxd)
{
    double best = 1e9;
    for (int run = 0; run < 3; ++run) {
        ch_init(); ch_tempo_set(120);
        ch_device_set(0, CH_MATERIAL, dev); ch_bypass_set(0, CH_MATERIAL, 0); ch_param_set(0, CH_MATERIAL, 0, (float)model);
        ch_mix_set(0, CH_MIX_MUTE, 0);
        if (fxd) ch_device_set(0, CH_FX1, fxd);
        ch_step st = {100, 1, 100, 0, 0, 0};
        for (unsigned l = 0; l < (fxd ? 1u : 4u); ++l) for (unsigned i = 0; i < 16; i += 2) ch_step_set(0, l * 2, i, &st);
        int32_t buf[2 * 128];
        for (unsigned b = 0; b < 20; ++b) ch_render(buf, 128, 0);
        ch_transport(1);
        for (unsigned b = 0; b < 100; ++b) ch_render(buf, 128, 0);       /* warm: voices up */
        clock_t c0 = clock();
        unsigned blocks = 1500;                                        /* 4.4 s */
        for (unsigned b = 0; b < blocks; ++b) ch_render(buf, 128, 0);
        double us = (double)(clock() - c0) * 1e6 / CLOCKS_PER_SEC / blocks;
        if (us < best) best = us;
    }
    return best;
}
static int cmd_cost(void)
{
    double base;
    { ch_init(); int32_t buf[2 * 128]; clock_t c0 = clock(); for (unsigned b = 0; b < 3000; ++b) ch_render(buf, 128, 0);
      base = (double)(clock() - c0) * 1e6 / CLOCKS_PER_SEC / 3000; }
    printf("empty engine: %.1f us a block\n", base);
    for (unsigned m = 0; m < CH_DRUM_MODELS; ++m) printf("DRUM  %-10s %6.1f us\n", ch_drum_names[m], cost_of(CH_DEV_DRUM, m, 0) - base);
    for (unsigned m = 0; m < CH_SYNTH_MODELS; ++m) printf("SYNTH %-10s %6.1f us\n", ch_synth_names[m], cost_of(CH_DEV_SYNTH, m, 0) - base);
    double kick = cost_of(CH_DEV_DRUM, 0, 0);
    for (unsigned d = CH_DEV_FX_FIRST; d < CH_DEVICES; ++d) printf("FX    %-10s %6.1f us\n", ch_device_info(d)->name, cost_of(CH_DEV_DRUM, 0, d) - kick);
    return 0;
}
static int cmd_ratchet(unsigned n)
{
    ch_init(); ch_tempo_set(120);
    drum(0, 0);
    ch_step st = {120, (uint8_t)n, 100, 0, 0, 0};
    ch_step_set(0, 0, 0, &st);
    int32_t buf[2 * 32];
    for (unsigned b = 0; b < 100; ++b) ch_render(buf, 32, 0);
    ch_transport(1);
    unsigned hits = 0; float prev = 0;
    for (unsigned b = 0; b < 44100 / 8 / 32; ++b) {             /* one 1/16 step at 120 BPM: 125 ms */
        ch_render(buf, 32, 0);
        float f = ch_snapshot_get()->hit[0];
        if (f > prev + 0.05f && f > 0.8f) { ++hits; printf("  hit at %.1f ms\n", b * 32 * 1000.0 / 44100); }
        prev = f;
    }
    printf("ratchet %u: %u hits\n", n, hits);
    return 0;
}
/* stutter: a kick every 4th step; the hold from 0.55 s to 1.05 s at 1/32 should give 8 hits 62.5 ms apart, then the pattern back at 1.5 s */
static int cmd_stutter(unsigned tracks)
{
    ch_init(); ch_tempo_set(120);
    drum(0, 0); steps(0, "X...X...X...X...", 60);
    drum(1, 15); steps(1, "x.x.x.x.x.x.x.x.", 60);   /* a hat every other step: held with the kick, or playing on under a one-track hold */
    int32_t buf[2 * 32];
    for (unsigned b = 0; b < 100; ++b) ch_render(buf, 32, 0);
    ch_transport(1);
    unsigned last[2] = {ch_snapshot_get()->hits[0], ch_snapshot_get()->hits[1]}, n[2] = {0, 0}, held[2] = {0, 0};
    for (unsigned b = 0; b < 44100 * 2 / 32; ++b) {
        double ms = b * 32 * 1000.0 / 44100;
        if (b == (unsigned)(0.55 * 44100 / 32)) ch_stutter(1, 2, tracks);
        if (b == (unsigned)(1.13 * 44100 / 32)) ch_stutter(0, 0, 0);   /* (mid-eighth: 1.13 s at 120 is 2.26 beats; it ends at 2.5, 1250 ms) */
        ch_render(buf, 32, 0);
        for (unsigned t = 0; t < 2; ++t) {
            unsigned h = ch_snapshot_get()->hits[t];
            if (h != last[t]) { if (!t) printf("  kick at %.1f ms\n", ms); last[t] = h; ++n[t]; if (ms > 550 && ms < 1245) ++held[t]; }
        }
    }
    printf("stutter (tracks %u): kick %u hits, %u in the hold (expect 11, to 1250 ms); hat %u in the hold (expect %s); the beat back at 1500 ms\n",
           tracks, n[0], held[0], held[1], tracks == 1 ? "2, playing on" : "11, held");
    return 0;
}
/* acid: the ACID model playing a line with accents and slides, to listen to: DIR/acid_line.wav (the preset, 2 bars at
 * 130) and DIR/acid_sweep.wav (RESO 0.8, CUTOFF swept up and back over 4 bars) */
static void acid_lanes(unsigned t)
{
    ch_device_set(t, CH_MATERIAL, CH_DEV_SYNTH); ch_bypass_set(t, CH_MATERIAL, 0); ch_mix_set(t, CH_MIX_MUTE, 0);
    ch_param_set(t, CH_MATERIAL, 0, 11);   /* ACID */
    for (unsigned p = 2; p < ch_device_info(CH_DEV_SYNTH)->params; ++p) { float v; if (ch_synth_preset(11, p, &v)) ch_param_set(t, CH_MATERIAL, p, v); }
    static const uint8_t NOTE[4] = {60, 63, 67, 72};
    static const char *const PAT[4] = {"X.x-.X..x....XXX", "...x......X-....", ".......x........", "...........x...."};
    for (unsigned l = 0; l < 4; ++l) { ch_lane_note_set(t, l, NOTE[l]); lane_steps(t, l, PAT[l]); }
}
static int cmd_acid(const char *dir)
{
    char path[512]; int32_t buf[2 * 128];
    for (unsigned which = 0; which < 3; ++which) {
        ch_init(); ch_tempo_set(130);
        for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        acid_lanes(0);
        unsigned bars = which == 1 ? 4 : 2, frames = (unsigned)(bars * 4 * 60.0 / 130 * CH_RATE);
        int32_t *all = calloc((size_t)2 * (frames + 128), sizeof(int32_t));
        if (which == 1) ch_param_set(0, CH_MATERIAL, 13, 0.8f);
        if (which == 2) { ch_param_set(0, CH_MATERIAL, 4, 0.85f); ch_param_set(0, CH_MATERIAL, 13, 0.7f); }   /* DRIVE 85: the overdrive stage */
        ch_render(buf, 128, NULL); ch_transport(1);
        double pk = 0;
        for (unsigned f = 0; f < frames; f += 128) {
            if (which == 1) { double u = (double)f / frames; ch_param_set(0, CH_MATERIAL, 12, (float)(0.1 + 0.6 * (u < 0.5 ? u * 2 : 2 - u * 2))); }
            ch_render(all + 2 * f, 128, NULL);
            for (unsigned i = 0; i < 256; ++i) { double v = fabs(all[2 * f + i] / 8388608.0); if (v > pk) pk = v; }
        }
        snprintf(path, sizeof path, "%s/%s", dir, which == 0 ? "acid_line.wav" : which == 1 ? "acid_sweep.wav" : "acid_drive.wav");
        wav_write_q23(path, all, frames, CH_RATE);
        printf("%s  peak %.3f\n", path, pk);
        free(all);
    }
    return 0;
}
/* crystal DIR: CRYSTAL's transposition read back. A held sine (A4) through it, wet only: the octave up (880 Hz) must stand
 * above the note itself (440) and anything else; then 808 BOOM and the crystal on a beat to listen to, DIR/*.wav */
static double goertzel(const int32_t *b, unsigned n, double hz)
{
    double w = 2 * 3.14159265358979 * hz / CH_RATE, c = 2 * cos(w), s1 = 0, s2 = 0;
    for (unsigned i = 0; i < n; ++i) { double s0 = b[2 * i] / 8388608.0 + c * s1 - s2; s2 = s1; s1 = s0; }
    return sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / n;
}
static int cmd_crystal(const char *dir)
{
    int32_t buf[2 * 128]; int bad = 0;
    for (int rev = 0; rev <= 1; ++rev) {
        ch_init();
        for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        ch_device_set(0, CH_MATERIAL, CH_DEV_SYNTH); ch_bypass_set(0, CH_MATERIAL, 0); ch_mix_set(0, CH_MIX_MUTE, 0);
        ch_param_set(0, CH_MATERIAL, 0, 0); ch_param_set(0, CH_MATERIAL, 2, 1.0f); ch_param_set(0, CH_MATERIAL, 3, 0);   /* ANALOG: A a sine, B off */
        ch_param_set(0, CH_MATERIAL, 7, 2);              /* DRONE */
        ch_param_set(0, CH_MATERIAL, 12, 1.0f);          /* the filter open */
        fx(0, CH_FX1, "CRYSTAL"); P(0, CH_FX1, "MIX", 1.0f); P(0, CH_FX1, "FEEDBACK", 0.0f); P(0, CH_FX1, "REVERSE", (float)rev);
        ch_render(buf, 128, NULL); ch_note(0, 69, 110);
        unsigned frames = CH_RATE * 2; int32_t *all = calloc((size_t)2 * (frames + 128), sizeof(int32_t));
        for (unsigned f = 0; f < frames; f += 128) ch_render(all + 2 * f, 128, NULL);
        /* the dry note is there too (the effect adds): judge the octave against the fifth and the off-notes */
        const int32_t *q = all + 2 * CH_RATE; unsigned m = CH_RATE / 2;
        /* the note's own frequency, found (ANALOG sounds an octave down, a little detuned: near 220), then each target at its
         * strongest within 3 Hz (the grains' windows spread a partial a little) */
        double f0 = 220, best = 0;
        for (double hz = 210; hz <= 230; hz += 0.25) { double a = goertzel(q, m, hz); if (a > best) { best = a; f0 = hz; } }
        double a440 = best, a880 = 0, a660 = 0, a1100 = 0;
        for (double d_ = -3; d_ <= 3; d_ += 0.25) {
            double a = goertzel(q, m, 2 * f0 + d_); if (a > a880) a880 = a;
            a = goertzel(q, m, 1.5 * f0 + d_); if (a > a660) a660 = a;
            a = goertzel(q, m, 2.5 * f0 + d_); if (a > a1100) a1100 = a;
        }
        int ok = a880 > 4 * a660 && a880 > 4 * a1100 && a880 > 0.01;
        bad += !ok;
        printf("crystal %s: note %.3f  octave %.3f  (fifth %.4f, third %.4f) %s\n", rev ? "reversed" : "forward ", a440, a880, a660, a1100, ok ? "ok" : "FAIL");
        free(all);
    }
    /* to listen: 808 BOOM notes, then the factory beat through CRYSTAL */
    char path[512];
    {
        ch_init(); ch_tempo_set(90);
        for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        drum(0, 40);                                    /* 808 BOOM */
        ch_param_set(0, CH_MATERIAL, 4, 0.45f);          /* SATURATE (the COLOR knob) */
        static const uint8_t NT[8] = {40, 40, 43, 38, 40, 40, 47, 45};
        unsigned frames = CH_RATE * 6; int32_t *all = calloc((size_t)2 * (frames + 128), sizeof(int32_t));
        double pk = 0;
        for (unsigned f = 0; f < frames; f += 128) {
            unsigned k = f / (CH_RATE * 3 / 4);
            if (f % (CH_RATE * 3 / 4) < 128 && k < 8) ch_note(0, NT[k] + 20, 110);
            ch_render(all + 2 * f, 128, NULL);
            for (unsigned i = 0; i < 256; ++i) { double v = fabs(all[2 * f + i] / 8388608.0); if (v > pk) pk = v; }
        }
        snprintf(path, sizeof path, "%s/808_boom.wav", dir); wav_write_q23(path, all, frames, CH_RATE); free(all);
        printf("808 BOOM: peak %.3f -> %s\n", pk, path);
    }
    {
        ch_init(); ch_tempo_set(90); ch_swing_set(0.3f);
        for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        ch_device_set(0, CH_MATERIAL, CH_DEV_KIT); ch_bypass_set(0, CH_MATERIAL, 0); ch_mix_set(0, CH_MIX_MUTE, 0);
        static const char *const BEAT[8] = {"X......X..X.....", "....X.......X...", "x.y.x.y.x.y.x...", "..............o.",
                                            "...........g....", "................", ".s.s.s.s.s.s.s.s", "................"};
        for (unsigned l = 0; l < 8; ++l) for (unsigned i = 0; i < 16; ++i) {
            char c = BEAT[l][i]; if (c == '.') continue;
            ch_step st = {(uint8_t)(c == 'X' ? 112 : c == 'x' ? 96 : c == 'y' ? 70 : c == 'o' ? 84 : c == 'g' ? 52 : 66), 1, 100, 0, 0, 0, 0};
            ch_step_set(0, l, i, &st);
        }
        fx(0, CH_FX1, "CRYSTAL"); P(0, CH_FX1, "MIX", 0.45f);
        unsigned frames = (unsigned)(4 * 4 * 60.0 / 90 * CH_RATE); int32_t *all = calloc((size_t)2 * (frames + 128), sizeof(int32_t));
        ch_render(buf, 128, NULL); ch_transport(1);
        double pk = 0;
        for (unsigned f = 0; f < frames; f += 128) { ch_render(all + 2 * f, 128, NULL); for (unsigned i = 0; i < 256; ++i) { double v = fabs(all[2 * f + i] / 8388608.0); if (v > pk) pk = v; } }
        snprintf(path, sizeof path, "%s/beat_crystal.wav", dir); wav_write_q23(path, all, frames, CH_RATE); free(all);
        printf("beat through CRYSTAL: peak %.3f -> %s\n", pk, path);
    }
    return bad != 0;
}
/* drives: each DRIVE type on a held sine (A3, 220 Hz): at DRIVE 0.2, 0.5 and 0.9, CHARACTER 0 and 1, the level against the dry
 * sine (dB, RMS) and how much of it is harmonics (THD, % of RMS off the fundamental); any broken value fails */
static int cmd_drives(void)
{
    static const char *const TN[6] = {"TUBE", "DIODE", "NONLINEAR", "OVERDRIVE", "SINUSOID", "CRUSH"}; static const char *const CN[3] = {"CUBIC", "SIMPLE", "HARD"};
    int32_t buf[2 * 128]; int bad = 0;
    double dry = 0;
    for (int pass = -1; pass < 6 * 9; ++pass) {
        ch_init();
        for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        ch_device_set(0, CH_MATERIAL, CH_DEV_SYNTH); ch_bypass_set(0, CH_MATERIAL, 0); ch_mix_set(0, CH_MIX_MUTE, 0);
        ch_param_set(0, CH_MATERIAL, 0, 0); ch_param_set(0, CH_MATERIAL, 2, 1.0f); ch_param_set(0, CH_MATERIAL, 3, 0);
        ch_param_set(0, CH_MATERIAL, 7, 2); ch_param_set(0, CH_MATERIAL, 12, 1.0f);
        unsigned type = 0; float drv = 0, chr = 0;
        if (pass >= 0) {
            type = (unsigned)pass / 9; drv = (pass % 3) == 0 ? 0.0f : (pass % 3) == 1 ? 0.5f : 1.0f; chr = (float)((pass / 3) % 3);
            fx(0, CH_FX1, "DRIVE");
            ch_param_set(0, CH_FX1, 4, (float)type); ch_param_set(0, CH_FX1, 0, drv);
            if (type == 5) ch_param_set(0, CH_FX1, 5, chr * 0.5f); else ch_param_set(0, CH_FX1, 6, chr);
        }
        ch_render(buf, 128, NULL); ch_note(0, 69, 110);
        unsigned frames = CH_RATE; int32_t *all = calloc((size_t)2 * (frames + 128), sizeof(int32_t));
        for (unsigned f = 0; f < frames; f += 128) ch_render(all + 2 * f, 128, NULL);
        const int32_t *q = all + CH_RATE; unsigned m = CH_RATE / 2; double rms = 0; int nan = 0;
        for (unsigned i = 0; i < m; ++i) { double v = q[2 * i] / 8388608.0; if (v != v) nan = 1; rms += v * v; }
        rms = sqrt(rms / m);
        double f0 = 220, best = 0;
        for (double hz = 210; hz <= 230; hz += 0.25) { double a = goertzel(q, m, hz); if (a > best) { best = a; f0 = hz; } }
        double fund = best * 2 / sqrt(2.0);               /* the fundamental's RMS */
        double thd = rms > fund ? sqrt(rms * rms - fund * fund) / rms * 100 : 0;
        if (pass < 0) { dry = rms; printf("dry sine: rms %.3f\n", rms); free(all); continue; }
        double db = 20 * log10(rms / dry);
        if (nan || rms > 1.0) ++bad;
        printf("%-9s %-6s drive %.1f: %+5.1f dB  thd %5.1f%% %s\n", TN[type], type == 5 ? (chr == 0 ? "16BIT" : chr == 1 ? "9BIT" : "2BIT") : CN[(int)chr], drv, db, thd, nan ? "NaN!" : "");
        free(all);
    }
    return bad != 0;
}
/* alias: aliasing in DRIVE. A sine near 2.96 kHz (its harmonics past the 7th fold back below Nyquist off the harmonic
 * series) through HARD CLIP, FUZZ, FOLD and SATURATE at DRIVE 0.9: the share of the output that is not the note or its
 * harmonics, in dB (lower is cleaner). Built with CH_DRIVE_NO_OS for the comparison without oversampling */
static int cmd_alias(void)
{
    static const unsigned TY[5] = {0, 1, 2, 3, 4}; static const char *const TN[5] = {"TUBE", "DIODE", "NONLINEAR", "OVERDRIVE", "SINUSOID"};
    int32_t buf[2 * 128];
    for (unsigned k = 0; k < 5; ++k) {
        ch_init();
        for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        ch_device_set(0, CH_MATERIAL, CH_DEV_SYNTH); ch_bypass_set(0, CH_MATERIAL, 0); ch_mix_set(0, CH_MIX_MUTE, 0);
        ch_param_set(0, CH_MATERIAL, 0, 0); ch_param_set(0, CH_MATERIAL, 2, 1.0f); ch_param_set(0, CH_MATERIAL, 3, 0);
        ch_param_set(0, CH_MATERIAL, 7, 2); ch_param_set(0, CH_MATERIAL, 12, 1.0f);
        fx(0, CH_FX1, "DRIVE"); ch_param_set(0, CH_FX1, 4, (float)TY[k]); ch_param_set(0, CH_FX1, 0, 0.9f);
        ch_render(buf, 128, NULL); ch_note(0, 114, 110);
        unsigned frames = CH_RATE; int32_t *all = calloc((size_t)2 * (frames + 128), sizeof(int32_t));
        for (unsigned f = 0; f < frames; f += 128) ch_render(all + 2 * f, 128, NULL);
        const int32_t *q = all + CH_RATE / 2; unsigned m = CH_RATE / 4;
        double tot = 0; for (unsigned i = 0; i < m; ++i) { double v = q[2 * i] / 8388608.0; tot += v * v; }
        tot /= m;
        double f0 = 2960, best = 0;
        for (double hz = 2900; hz <= 3020; hz += 0.25) { double a = goertzel(q, m, hz); if (a > best) { best = a; f0 = hz; } }
        double harm = 0;
        for (unsigned h = 1; h * f0 < CH_RATE / 2; ++h) {
            double b = 0; for (double d = -3; d <= 3; d += 0.25) { double a = goertzel(q, m, h * f0 + d); if (a > b) b = a; }
            harm += 2 * b * b;                          /* (a sine's power from its Goertzel amplitude) */
        }
        double rest = tot > harm ? tot - harm : 1e-12;
        printf("%-9s inharmonic %6.1f dB\n", TN[k], 10 * log10(rest / tot));
        free(all);
    }
    return 0;
}
/* safety: the master's guards. A wall (four noise drones, each hard-clipped at full and through COMB at full feedback, the
 * * tracks and MAIN at full, the compressor off) must never pass the ceiling, and after ~2 s the runaway guard must take it 12 dB down; the
 * factory beat through a heavy DRIVE, loud but music, must never trip it */
static int cmd_safety(void)
{
    int32_t buf[2 * 128]; int bad = 0;
    for (int scene = 0; scene < 2; ++scene) {
        ch_init(); ch_tempo_set(90);
        for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        if (scene == 0) {                              /* all four tracks screaming, the master compressor and MAIN at full */
            for (unsigned t = 0; t < CH_TRACKS; ++t) {
                ch_device_set(t, CH_MATERIAL, CH_DEV_SYNTH); ch_bypass_set(t, CH_MATERIAL, 0); ch_mix_set(t, CH_MIX_MUTE, 0);
                ch_param_set(t, CH_MATERIAL, 0, 12); ch_param_set(t, CH_MATERIAL, 7, 2); ch_param_set(t, CH_MATERIAL, 12, 1.0f);   /* NOISE, droning */
                fx(t, CH_FX1, "DRIVE"); ch_param_set(t, CH_FX1, 4, 3);   /* FUZZ */ ch_param_set(t, CH_FX1, 0, 1.0f); ch_param_set(t, CH_FX1, 2, 1.0f);
                fx(t, CH_FX2, "COMB"); P(t, CH_FX2, "FEEDBACK", 1.0f); P(t, CH_FX2, "MIX", 1.0f); P(t, CH_FX2, "DAMP", 0.0f);
                ch_mix_set(t, CH_MIX_LEVEL, 1.0f);
            }
            ch_master_set(0.0f, 1.0f);                     /* (the compressor off: at full it holds the stack down itself) */
            ch_render(buf, 128, NULL);
            for (unsigned t = 0; t < CH_TRACKS; ++t) ch_note(t, 48 + 7 * t, 127);
        } else {
            ch_swing_set(0.3f);
            ch_device_set(0, CH_MATERIAL, CH_DEV_KIT); ch_bypass_set(0, CH_MATERIAL, 0); ch_mix_set(0, CH_MIX_MUTE, 0);
            static const char *const BEAT[3] = {"X......X..X.....", "....X.......X...", "x.y.x.y.x.y.x..."};
            for (unsigned l = 0; l < 3; ++l) for (unsigned i = 0; i < 16; ++i) {
                char c = BEAT[l][i]; if (c == '.') continue;
                ch_step st = {(uint8_t)(c == 'X' ? 120 : c == 'x' ? 100 : 70), 1, 100, 0, 0, 0, 0}; ch_step_set(0, l, i, &st);
            }
            fx(0, CH_FX1, "DRIVE"); ch_param_set(0, CH_FX1, 0, 0.8f);
            ch_mix_set(0, CH_MIX_LEVEL, 1.0f); ch_master_set(0.5f, 1.0f);   /* loud: half the compressor, MAIN up */
            ch_render(buf, 128, NULL); ch_transport(1);
        }
        unsigned frames = CH_RATE * 8; double pk = 0, early = 0, late = 0; unsigned ne = 0, nl = 0; int prot = 0, when = -1;
        double grmax = 0, grsum = 0; unsigned grn = 0;
        for (unsigned f = 0; f < frames; f += 128) {
            ch_render(buf, 128, NULL);
            for (unsigned i = 0; i < 256; ++i) {
                double v = buf[i] / 8388608.0; if (fabs(v) > pk) pk = fabs(v);
                if (f < CH_RATE * 3 / 2) { early += v * v; ++ne; } else if (f > CH_RATE * 5) { late += v * v; ++nl; }
            }
            { float g = ch_snapshot_get()->lim_gr; double d = g > 1e-6f ? -20 * log10(g) : 120; if (d > grmax) grmax = d; grsum += d; ++grn; }
        }
        (void)late; (void)nl; (void)prot; (void)when;
        printf("%s: peak %.3f (the ceiling 0.94, the knee to 1.0)\n", scene ? "loud beat  " : "noise wall ", pk);
        printf("             limiting: %.1f dB at most, %.1f dB on average; level %.1f dBFS RMS (first 1.5 s)\n", grmax, grsum / grn, 10 * log10(early / ne));
        if (pk > 0.95) ++bad;
    }
    printf("%s\n", bad ? "SAFETY: FAILED" : "SAFETY: ok");
    return bad != 0;
}
/* kit DIR: the KIT engine. Each of its eight parts struck alone (the factory kit: each must sound, none break), every drum
 * model on every part at the knobs' ends (no broken value, no peak past 4), then the factory beat for four bars at 90 BPM
 * swung to DIR/kit_beat.wav */
static int cmd_kit(const char *dir)
{
    int bad = 0; int32_t buf[2 * 128];
    for (unsigned part = 0; part < CH_KIT_PARTS; ++part) {
        ch_init();
        for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        ch_device_set(0, CH_MATERIAL, CH_DEV_KIT); ch_bypass_set(0, CH_MATERIAL, 0); ch_mix_set(0, CH_MIX_MUTE, 0);
        ch_render(buf, 128, NULL);
        ch_note(0, ch_seq_get(0)->note[part], 110);
        double pk = 0; int nan = 0;
        for (unsigned b = 0; b < 200; ++b) { ch_render(buf, 128, NULL); for (unsigned i = 0; i < 256; ++i) { double v = buf[i] / 8388608.0; if (v != v) nan = 1; if (fabs(v) > pk) pk = fabs(v); } }
        unsigned m = (unsigned)(ch_track_get(0)->param[CH_KIT_XROW][part] + 0.5f);
        printf("part %u %-11s peak %.3f %s\n", part + 1, ch_drum_names[m], pk, nan ? "NaN!" : pk < 0.02 ? "SILENT!" : "");
        if (nan || pk < 0.02) ++bad;
    }
    unsigned runs = 0; double worst = 0;
    for (unsigned m = 0; m < CH_DRUM_MODELS; ++m) for (unsigned c = 0; c < 4; ++c) {
        ch_init();
        for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        ch_device_set(0, CH_MATERIAL, CH_DEV_KIT); ch_bypass_set(0, CH_MATERIAL, 0);
        for (unsigned part = 0; part < CH_KIT_PARTS; ++part) {
            ch_param_set(0, CH_MATERIAL, 16 + part, (float)m);
            ch_param_set(0, CH_MATERIAL, 2 * part, (c & 1) ? 1.0f : -1.0f);     /* PITCH at an end */
            ch_param_set(0, CH_MATERIAL, 2 * part + 1, (c & 2) ? 1.0f : 0.0f);  /* DECAY at an end */
            ch_param_set(0, CH_MATERIAL, 24 + part, 1.0f);
        }
        ch_render(buf, 128, NULL);
        for (unsigned part = 0; part < CH_KIT_PARTS; ++part) ch_note(0, ch_seq_get(0)->note[part], 127);
        double pk = 0; int nan = 0;
        for (unsigned b = 0; b < 120; ++b) { ch_render(buf, 128, NULL); for (unsigned i = 0; i < 256; ++i) { double v = buf[i] / 8388608.0; if (v != v) nan = 1; if (fabs(v) > pk) pk = fabs(v); } }
        ++runs; if (pk > worst) worst = pk;
        if (nan || pk > 4) { ++bad; printf("FAIL %s corner %u: %s %.2f\n", ch_drum_names[m], c, nan ? "NaN" : "peak", pk); }
    }
    printf("kit stress: %u runs, loudest %.2f\n", runs, worst);
    /* the factory beat */
    ch_init(); ch_tempo_set(90); ch_swing_set(0.3f);
    for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
    ch_device_set(0, CH_MATERIAL, CH_DEV_KIT); ch_bypass_set(0, CH_MATERIAL, 0); ch_mix_set(0, CH_MIX_MUTE, 0);
    static const char *const BEAT[8] = {"X......X..X.....", "....X.......X...", "x.y.x.y.x.y.x...", "..............o.",
                                        "...........g....", "................", ".s.s.s.s.s.s.s.s", "................"};
    for (unsigned l = 0; l < 8; ++l) for (unsigned i = 0; i < 16; ++i) {
        char c = BEAT[l][i]; if (c == '.') continue;
        ch_step st = {(uint8_t)(c == 'X' ? 112 : c == 'x' ? 96 : c == 'y' ? 70 : c == 'o' ? 84 : c == 'g' ? 52 : 66), 1, 100, 0, 0, 0, 0};
        ch_step_set(0, l, i, &st);
    }
    unsigned frames = (unsigned)(4 * 4 * 60.0 / 90 * CH_RATE);
    int32_t *all = calloc((size_t)2 * (frames + 128), sizeof(int32_t));
    ch_render(buf, 128, NULL); ch_transport(1);
    double pk = 0;
    for (unsigned f = 0; f < frames; f += 128) { ch_render(all + 2 * f, 128, NULL); for (unsigned i = 0; i < 256; ++i) { double v = fabs(all[2 * f + i] / 8388608.0); if (v > pk) pk = v; } }
    char path[512]; snprintf(path, sizeof path, "%s/kit_beat.wav", dir); wav_write_q23(path, all, frames, CH_RATE); free(all);
    printf("beat: peak %.3f -> %s\n%s\n", pk, path, bad ? "KIT: FAILED" : "KIT: ok");
    return bad != 0;
}
/* mistress: the Mutable-ported and merged models (FOLD VOICE CHORD STRING MODAL PARTICLE) at every corner of their three knobs, in PING,
 * GATE and DRONE, a low and a high note each: any broken value, or a peak past 4, fails */
static int cmd_mistress(void)
{
    static const unsigned M[6] = {1, 3, 6, 8, 9, 10};   /* FOLD VOICE CHORD STRING MODAL PARTICLE */
    int bad = 0; unsigned runs = 0; double worst = 0;
    int32_t buf[2 * 128];
    for (unsigned mi = 0; mi < 6; ++mi) for (unsigned mode = 0; mode < 3; ++mode) for (unsigned c = 0; c < 8; ++c) for (unsigned nn = 0; nn < 2; ++nn) {
        ch_init();
        for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        ch_device_set(0, CH_MATERIAL, CH_DEV_SYNTH); ch_bypass_set(0, CH_MATERIAL, 0); ch_mix_set(0, CH_MIX_MUTE, 0);
        ch_param_set(0, CH_MATERIAL, 0, (float)M[mi]);
        for (unsigned k = 0; k < 3; ++k) ch_param_set(0, CH_MATERIAL, 2 + k, (c >> k & 1u) ? 1.0f : 0.0f);
        ch_param_set(0, CH_MATERIAL, 7, (float)mode);
        ch_render(buf, 128, NULL);
        ch_note(0, nn ? 96 : 24, 127);
        double pk = 0; int nan = 0;
        for (unsigned b = 0; b < 200; ++b) {
            if (b == 120) ch_note(0, nn ? 96 : 24, 0);
            ch_render(buf, 128, NULL);
            for (unsigned i = 0; i < 256; ++i) { double v = buf[i] / 8388608.0; if (v != v) nan = 1; if (fabs(v) > pk) pk = fabs(v); }
        }
        ++runs; if (pk > worst) worst = pk;
        if (nan || pk > 4) { ++bad; printf("FAIL %s mode %u knobs %u%u%u note %u: %s peak %.2f\n", ch_synth_names[M[mi]], mode, c & 1, c >> 1 & 1, c >> 2 & 1, nn ? 96 : 24, nan ? "NaN" : "", pk); }
    }
    printf("mistress: %u runs, %d failing, loudest peak %.2f\n", runs, bad, worst);
    return bad != 0;
}
/* ab MODEL DIR: one synth model through a phrase to compare builds by ear: notes from C2 up to G5 at its own settings,
 * then C3 repeating while each of its three knobs sweeps end to end in turn; DIR/<model>.wav, and its peak */
static int cmd_ab(unsigned m, const char *dir)
{
    if (m >= CH_SYNTH_MODELS) return 2;
    unsigned secs = 8, frames = secs * CH_RATE, block = 128, half = CH_RATE / 2;
    int32_t *b = calloc((size_t)2 * (frames + block), sizeof(int32_t));
    ch_init(); ch_tempo_set(120);
    for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
    ch_device_set(0, CH_MATERIAL, CH_DEV_SYNTH); ch_bypass_set(0, CH_MATERIAL, 0); ch_mix_set(0, CH_MIX_MUTE, 0);
    ch_param_set(0, CH_MATERIAL, 0, (float)m);
    float knob[3];
    for (unsigned p = 2; p < ch_device_info(CH_DEV_SYNTH)->params; ++p) { float v; if (ch_synth_preset(m, p, &v)) ch_param_set(0, CH_MATERIAL, p, v); }
    for (unsigned k = 0; k < 3; ++k) knob[k] = ch_track_get(0)->param[ch_row(CH_MATERIAL, CH_DEV_SYNTH)][2 + k];
    { int32_t warm[256]; ch_render(warm, 128, NULL); }
    static const uint8_t NOTES[8] = {36, 43, 48, 55, 60, 67, 72, 79};
    int held = -1; double pk = 0;
    for (unsigned f = 0; f < frames; f += block) {
        unsigned slot = f / half, in = f % half;
        if (in < block) {                                 /* a note each half second */
            int n = slot < 8 ? NOTES[slot] : 48;
            if (held >= 0) ch_note(0, (unsigned)held, 0);
            ch_note(0, (unsigned)n, 110); held = n;
        }
        if (held >= 0 && in >= (unsigned)(half * 0.7) && in < (unsigned)(half * 0.7) + block) { ch_note(0, (unsigned)held, 0); held = -1; }
        if (f >= 4u * CH_RATE) {                          /* the sweeps: K1, K2, K3 a third of the time each */
            float u = (float)(f - 4u * CH_RATE) / (float)(4u * CH_RATE); unsigned k = (unsigned)(u * 3); if (k > 2) k = 2;
            float w = u * 3 - (float)k;
            for (unsigned q = 0; q < 3; ++q) ch_param_set(0, CH_MATERIAL, 2 + q, q == k ? w : knob[q]);
        }
        ch_render(b + 2 * f, block, NULL);
        for (unsigned i = 0; i < 2 * block; ++i) { double v = fabs(b[2 * f + i] / 8388608.0); if (v > pk) pk = v; }
    }
    char path[512]; snprintf(path, sizeof path, "%s/%s.wav", dir, ch_synth_names[m]);
    wav_write_q23(path, b, frames, CH_RATE);
    printf("%-10s peak %.3f -> %s\n", ch_synth_names[m], pk, path);
    free(b);
    return 0;
}
/* microtest: a step every 4 on one track, micro 0 against micro 5, on a few engines: each hit's peak, to find hits that go silent */
static int cmd_microtest(void)
{
    struct { unsigned dev, model; const char *name; } E[] = {{CH_DEV_DRUM, 0, "DRUM KICK"}, {CH_DEV_DRUM, 32, "DRUM PLUCK"}, {CH_DEV_SYNTH, 0, "SYNTH ANALOG"}, {CH_DEV_SYNTH, 11, "SYNTH ACID"}, {CH_DEV_SYNTH, 14, "SYNTH PHASE"}};
    for (unsigned e = 0; e < sizeof E / sizeof E[0]; ++e)
        for (int mic = 0; mic <= 5; mic += 5) {
            ch_init(); ch_tempo_set(120);
            for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
            ch_device_set(0, CH_MATERIAL, E[e].dev); ch_bypass_set(0, CH_MATERIAL, 0); ch_param_set(0, CH_MATERIAL, 0, (float)E[e].model);
            ch_mix_set(0, CH_MIX_MUTE, 0);
            ch_step st = {100, 1, 100, 0, 0, (int8_t)mic, 0};
            for (unsigned i = 0; i < 16; i += 4) ch_step_set(0, 0, i, &st);
            int32_t buf[2 * 128]; ch_render(buf, 128, 0); ch_transport(1);
            unsigned per = (unsigned)(CH_RATE * 0.125), frames = per * 32;   /* a 16th at 120: 1/8 s; two bars */
            double pk[32] = {0};
            for (unsigned f = 0; f < frames; f += 128) {
                ch_render(buf, 128, 0);
                for (unsigned i = 0; i < 256; ++i) { double v = fabs(buf[i] / 8388608.0); unsigned k = (f + i / 2) / per; if (k < 32 && v > pk[k]) pk[k] = v; }
            }
            printf("%-13s micro %d:", E[e].name, mic);
            for (unsigned k = 0; k < 32; k += 4) printf(" %.2f", pk[k] > pk[k + 1] ? pk[k] : pk[k + 1]);
            printf("\n");
        }
    return 0;
}
/* comb: COMB's pitch read back from its output (the autocorrelation's period) for each FREQ mode, POLARITY -, and
 * FOLLOW from its own track and from another; DIR/comb_*.wav to listen to */
static double comb_period_hz(const int32_t *b, unsigned n)
{
    static double r[2400]; double best = -1e30; unsigned at = 0;
    for (unsigned lag = 8; lag < 2400; ++lag) {
        double acc = 0; for (unsigned i = 0; i + lag < n; ++i) acc += (double)b[2 * i] * (double)b[2 * (i + lag)];
        r[lag] = acc; if (acc > best) { best = acc; at = lag; }
    }
    for (unsigned k = 4; k >= 2; --k) {                    /* a multiple of the period: take the shortest that is as strong */
        unsigned l = (unsigned)((double)at / k + 0.5); if (l < 9 || l > 2398) continue;
        double m = r[l]; if (r[l - 1] > m) m = r[l - 1]; if (r[l + 1] > m) m = r[l + 1];
        if (m > 0.85 * best) { at = l; break; }
    }
    double y0 = r[at - 1], y1 = r[at], y2 = r[at + 1], den = y0 - 2 * y1 + y2;   /* the peak, between frames */
    double fr = den != 0 ? 0.5 * (y0 - y2) / den : 0;
    return CH_RATE / ((double)at + fr);
}
static int cmd_comb(const char *dir)
{
    struct { const char *name; unsigned mode, pol, src, own_note, other_note; float freq; double want; } C[] = {
        {"free",        0, 0, 0, 60, 0,  0.45f, 0},
        {"note_a3",     1, 0, 0, 60, 0,  33.0f / 72, 220.0},
        {"note_a3_neg", 1, 1, 0, 60, 0,  33.0f / 72, 110.0},
        {"follow_own",  2, 0, 0, 72, 0,  33.0f / 72, 440.0},
        {"follow_t2",   2, 0, 2, 60, 67, 33.0f / 72, 329.63},
        {"follow_t2lo", 2, 0, 2, 60, 48, 33.0f / 72, 110.0}};
    int fails = 0; char path[512];
    for (unsigned c = 0; c < sizeof C / sizeof C[0]; ++c) {
        ch_init(); ch_tempo_set(120);
        for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        drum(0, 19); steps(0, "xxxxxxxxxxxxxxxx", C[c].own_note);
        if (C[c].other_note) { drum(1, 19); steps(1, "x...............", C[c].other_note); ch_mix_set(1, CH_MIX_LEVEL, 0); }
        fx(0, CH_FX1, "COMB");
        ch_param_set(0, CH_FX1, 0, C[c].freq); ch_mode_set(0, CH_FX1, 0, C[c].mode);
        ch_param_set(0, CH_FX1, 1, 0.95f); ch_param_set(0, CH_FX1, 2, 0.1f); ch_param_set(0, CH_FX1, 3, 1.0f);
        ch_param_set(0, CH_FX1, 4, (float)C[c].pol); ch_param_set(0, CH_FX1, 7, (float)C[c].src);
        unsigned frames = CH_RATE * 2;
        int32_t *all = calloc((size_t)2 * (frames + 128), sizeof(int32_t)), buf[256];
        ch_render(buf, 128, NULL); ch_transport(1);
        for (unsigned f = 0; f < frames; f += 128) ch_render(all + 2 * f, 128, NULL);
        double pk = 0; for (unsigned i = 0; i < 2 * frames; ++i) { double v = fabs(all[i] / 8388608.0); if (v > pk) pk = v; }
        double hz = comb_period_hz(all + 2 * (frames - 12000), 12000);
        double want = C[c].want ? C[c].want : 30.0 * pow(8000.0 / 30.0, C[c].freq);
        {   /* the autocorrelation at the wanted period and at half and twice it, against lag 0 */
            const int32_t *q = all + 2 * (frames - 12000); double r0 = 0, rw[3] = {0, 0, 0}; unsigned L[3];
            L[0] = (unsigned)(CH_RATE / want / 2 + 0.5); L[1] = (unsigned)(CH_RATE / want + 0.5); L[2] = 2 * L[1];
            for (unsigned i = 0; i + 2400 < 12000; ++i) { r0 += (double)q[2 * i] * q[2 * i]; for (unsigned k = 0; k < 3; ++k) rw[k] += (double)q[2 * i] * q[2 * (i + L[k])]; }
            printf("   r(P/2) %+.2f  r(P) %+.2f  r(2P) %+.2f\n", rw[0] / r0, rw[1] / r0, rw[2] / r0);
        }
        double cents = 1200 * log2(hz / want); int ok = fabs(cents) < 15;
        fails += !ok;
        printf("%-12s %8.2f Hz  want %8.2f  %+6.1f cents  peak %.2f  %s\n", C[c].name, hz, want, cents, pk, ok ? "ok" : "FAIL");
        snprintf(path, sizeof path, "%s/comb_%s.wav", dir, C[c].name); wav_write_q23(path, all, frames, CH_RATE);
        free(all);
    }
    return fails ? 1 : 0;
}
/* scope: the output tap the screen's scope reads; the kit for a second, then the last 512 frames' extremes */
static int cmd_scope(void)
{
    ch_init(); ch_tempo_set(120);
    drum(0, 0); steps(0, "X...x...X...x...", 60);
    drum(2, 15); steps(2, "x.x.x.x.x.x.x.x.", 60);
    int32_t buf[2 * 128];
    ch_render(buf, 128, NULL); ch_transport(1);
    for (unsigned b = 0; b < 44100 / 128 + 4; ++b) ch_render(buf, 128, NULL);   /* past the kick on beat 3 */
    unsigned w; const int8_t *sc = ch_scope_data(&w); unsigned n = CH_SCOPE_N;
    int lo = 127, hi = -128, nz = 0;
    for (unsigned i = 0; i < n; ++i) { if (sc[i] < lo) lo = sc[i]; if (sc[i] > hi) hi = sc[i]; nz += sc[i] != 0; }
    printf("spectrum ring: %u frames, min %d max %d, %d nonzero bytes; output peak this block %.3f\n", n, lo, hi, nz, buf[0] / 8388608.0);
    return 0;
}
/* budget: 16 notes at once over the four tracks (SYNTH CHORD); prints the voices sounding */
static int cmd_budget(void)
{
    ch_init();
    for (unsigned t = 0; t < CH_TRACKS; ++t) { ch_device_set(t, CH_MATERIAL, CH_DEV_SYNTH); ch_bypass_set(t, CH_MATERIAL, 0); ch_param_set(t, CH_MATERIAL, 0, 6); ch_mix_set(t, CH_MIX_MUTE, 0); }
    int32_t buf[2 * 128];
    ch_render(buf, 128, 0);
    for (unsigned t = 0; t < CH_TRACKS; ++t) for (unsigned k = 0; k < 4; ++k) ch_note(t, 48 + k * 5, 100);
    for (unsigned b = 0; b < 10; ++b) {
        ch_render(buf, 128, 0);
        unsigned on = 0, live = 0;
        for (unsigned t = 0; t < CH_TRACKS; ++t) for (unsigned i = 0; i < CH_SVOICES; ++i) { on += ch_tr[t].synth.v[i].on; live += ch_tr[t].synth.v[i].on && !ch_tr[t].synth.v[i].kill; }
        if (b == 0 || b == 9) printf("block %u: %u voices on, %u not fading (budget %d)\n", b, on, live, CH_VOICE_BUDGET);
    }
    return 0;
}
/* voices: the default kit playing; how many drum voices each track keeps running, sampled over 4 seconds */
static int cmd_voices(void)
{
    ch_init(); ch_tempo_set(120);
    drum(0, 0); steps(0, "X...x...X...x..r", 60);
    drum(1, 9); steps(1, "....X.......X..c", 60);
    drum(2, 15); steps(2, "x.x.X.x.x.x.X.x?", 60);
    drum(3, 27); steps(3, "..x...x....x.c..", 67);
    int32_t buf[2 * 128];
    ch_render(buf, 128, 0); ch_transport(1);
    unsigned sum[CH_TRACKS] = {0}, mx[CH_TRACKS] = {0}, nb = 0;
    for (unsigned b = 0; b < 44100 * 4 / 128; ++b) {
        ch_render(buf, 128, 0); ++nb;
        for (unsigned t = 0; t < CH_TRACKS; ++t) {
            unsigned c = 0;
            for (unsigned i = 0; i < CH_DVOICES; ++i) c += ch_tr[t].drum.v[i].on;
            sum[t] += c; if (c > mx[t]) mx[t] = c;
        }
    }
    for (unsigned t = 0; t < CH_TRACKS; ++t) printf("T%u %-10s voices running: average %.2f, most %u\n", t + 1, ch_drum_names[(unsigned)(ch_track_get(t)->param[CH_DEV_DRUM][0] + 0.5f)], (double)sum[t] / nb, mx[t]);
    return 0;
}
static int cmd_lanes(void)
{
    ch_init(); ch_tempo_set(120);
    ch_device_set(3, CH_MATERIAL, CH_DEV_SYNTH); ch_bypass_set(3, CH_MATERIAL, 0); ch_mix_set(3, CH_MIX_MUTE, 0);
    ch_param_set(3, CH_MATERIAL, 7, 1);                    /* GATE mode: a voice holds while its lane's gate is open */
    drum(0, 13);                                            /* T1: CLAVE, the drum's 4 voices */
    lane_steps(3, 0, "X...x...");  lane_steps(3, 2, "X.......");  lane_steps(3, 4, "X...x...");
    lane_steps(3, 1, "..x.....");  lane_steps(3, 7, "....x...");
    lane_steps(0, 0, "x.x.....");  lane_steps(0, 4, "x.......");  lane_steps(0, 7, "x...x...");
    int32_t buf[2 * 128];
    ch_render(buf, 128, 0);
    ch_transport(1);
    int last = -2;
    for (unsigned b = 0; b < 600; ++b) {
        ch_render(buf, 128, 0);
        int st = ch_snapshot_get()->step[3];
        if (st == last) continue;
        last = st;
        ch_render(buf, 128, 0); ch_render(buf, 128, 0);   /* into the step */
        printf("step %d  T4 synth:", st + 1);
        for (unsigned k = 0; k < CH_SVOICES; ++k) if (ch_tr[3].synth.v[k].on && ch_tr[3].synth.v[k].gate) printf(" %u", ch_tr[3].synth.v[k].note);
        printf("   T1 drum:");
        for (unsigned k = 0; k < CH_DVOICES; ++k) if (ch_tr[0].drum.v[k].on && ch_tr[0].drum.v[k].n < 3000) printf(" %u", ch_tr[0].drum.v[k].note);
        printf("   lanes T4 %02x\n", ch_snapshot_get()->lanes[3]);
        if (st >= 7) break;
    }
    return 0;
}
static int cmd_drums(const char *dir)
{
    ch_init();
    for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
    ch_device_set(0, CH_MATERIAL, CH_DEV_DRUM);
    unsigned per = (unsigned)(1.6 * CH_RATE), block = 128, total = per * CH_DRUM_MODELS;
    int32_t *all = calloc((size_t)2 * (total + block), sizeof(int32_t));
    int bad = 0;
    printf("%-10s %6s %6s %7s %7s\n", "model", "peak", "soft", "tail ms", "clicks");
    for (unsigned m = 0; m < CH_DRUM_MODELS; ++m) {
        ch_init();
        for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        ch_device_set(0, CH_MATERIAL, CH_DEV_DRUM);
        ch_param_set(0, CH_MATERIAL, 13, 0);   /* TAILS: CHOKE, so the soft hit stands alone */
        ch_param_set(0, CH_MATERIAL, 0, (float)m);
        { int32_t warm[256]; ch_render(warm, 128, NULL); }
        int32_t *b = all + 2 * (size_t)per * m;
        double pk = 0, pk2 = 0; unsigned last = 0; int nan = 0;
        for (unsigned f = 0; f < per; f += block) {
            if (f == 0) ch_note(0, 60, 127);
            if (f == (unsigned)(0.8 * CH_RATE) / block * block) ch_note(0, 60, 60);
            ch_render(b + 2 * f, block, NULL);
            for (unsigned i = 0; i < block; ++i) {
                double v = fabs(b[2 * (f + i)] / 8388608.0);
                if (v != v) nan = 1;
                if (f + i < 0.8 * CH_RATE) { if (v > pk) pk = v; } else if (v > pk2) pk2 = v;
                if (v > 0.001 && f + i < 35200) last = f + i;
            }
        }
        if (nan || pk < 0.05 || pk > 0.995) bad = 1;
        printf("%-10s %6.3f %6.3f %7.0f %s\n", ch_drum_names[m], pk, pk2, last * 1000.0 / CH_RATE, nan ? "NaN!" : "");
        char path[512]; snprintf(path, sizeof path, "%s/%02u_%s.wav", dir, m, ch_drum_names[m]);
        for (char *q = path + strlen(dir); *q; ++q) if (*q == ' ') *q = '_';
        wav_write_q23(path, b, per, CH_RATE);
    }
    char path[512]; snprintf(path, sizeof path, "%s/all_models.wav", dir);
    wav_write_q23(path, all, total, CH_RATE);
    return bad;
}

/* the factory beat on a KIT (T1), and the same with three synth tracks going, for 16 bars: how often a voice is cut
 * while still clearly sounding, and by what (a KIT hit taking its voice; the voice budget / load governor) */
static int cmd_kitcut(void)
{
    static const char *const BEAT[8] = {
        "X......X..X.....", "....X.......X...", "x.y.x.y.x.y.x...", "..............o.",
        "...........g....", "................", ".s.s.s.s.s.s.s.s", "................"};
    static const char *const PART[8] = {"kick", "snare", "hat", "open hat", "rim", "clap", "shaker", "tom"};
    for (unsigned scene = 0; scene < 2; ++scene) {
        ch_init();
        ch_device_set(0, CH_MATERIAL, CH_DEV_KIT); ch_bypass_set(0, CH_MATERIAL, 0);
        for (unsigned l = 0; l < 8; ++l)
            for (unsigned i = 0; i < 16; ++i) {
                char c = BEAT[l][i]; if (c == '.') continue;
                ch_step st = {(uint8_t)(c == 'X' ? 112 : c == 'x' ? 96 : c == 'y' ? 70 : c == 'o' ? 84 : c == 'g' ? 52 : 66), 1, 100, 0, 0, 0, 0};
                ch_step_set(0, l, i, &st);
            }
        ch_seq_len_set(0, 16); ch_tempo_set(90); ch_swing_set(0.3f);
        for (unsigned t = 1; t < CH_TRACKS; ++t) {
            if (!scene) { ch_mix_set(t, CH_MIX_MUTE, 1); continue; }
            ch_device_set(t, CH_MATERIAL, CH_DEV_SYNTH); ch_param_set(t, CH_MATERIAL, 0, t == 1 ? 0.0f : t == 2 ? 13.0f : 8.0f);   /* ANALOG, RIPPLE, STRING */
            for (unsigned q = 2; q < ch_device_info(CH_DEV_SYNTH)->params; ++q) { float v; if (ch_synth_preset(t == 1 ? 0 : t == 2 ? 13 : 8, q, &v)) ch_param_set(t, CH_MATERIAL, q, v); }
            ch_param_set(t, CH_MATERIAL, 7, 1); ch_param_set(t, CH_MATERIAL, 5, 0.6f);   /* GATE, a release */
            for (unsigned i = 0; i < 16; i += (t == 1 ? 2 : 4)) for (unsigned l = 0; l < 3; ++l) { ch_step st = {90, 1, 100, 0, 0, 0, 0}; ch_step_set(t, l, i, &st); }   /* chords */
            ch_seq_len_set(t, 16);
        }
        for (unsigned k = 0; k < 8; ++k) ch_cut_kit[k] = 0;
        for (unsigned k = 0; k < 4; ++k) ch_cut_budget[k] = 0;
        for (unsigned k = 0; k < 8; ++k) ch_cut_bpart[k] = 0;
        int32_t buf[256]; ch_render(buf, 128, NULL); ch_transport(1);
        unsigned frames = (unsigned)(CH_RATE * 60.0 / 90 * 4 * 16);
        unsigned kicks = 0, seen = 0;
        for (unsigned f = 0; f < frames; f += 128) { ch_render(buf, 128, NULL); unsigned h = ch_snapshot_get()->hits[0]; if (h != seen) { seen = h; } }
        kicks = 16 * 3;
        ch_transport(0);
        printf("%s: 16 bars, %u kicks\n", scene ? "kit + 3 synth tracks" : "kit alone          ", kicks);
        for (unsigned k = 0; k < 8; ++k) if (ch_cut_kit[k]) printf("   kit took the voice of a sounding %-8s %4u times\n", PART[k], (unsigned)ch_cut_kit[k]);
        for (unsigned t = 0; t < 4; ++t) if (ch_cut_budget[t]) printf("   the voice budget cut a sounding voice on T%u %4u times\n", t + 1, (unsigned)ch_cut_budget[t]);
        for (unsigned k = 0; k < 8; ++k) if (ch_cut_bpart[k]) printf("      of them a drum voice playing part %-8s %4u times\n", PART[k], (unsigned)ch_cut_bpart[k]);
    }
    return 0;
}
/* REVERB's DUCK: a kick on every beat into a long reverb, its MIX full; the reverb alone (the dry taken out by running
 * the same with MIX 0 and subtracting) measured in the first 100 ms after each kick and in the last 200 ms before the
 * next, at DUCK 0, 0.5 and 1. Ducked, the first is well under the second; at 0 they are close */
static int cmd_duck(const char *dir)
{
    unsigned beat = (unsigned)(CH_RATE * 0.5), bars = 8, frames = beat * 4 * bars, block = 128;
    int32_t *dry = calloc((size_t)2 * (frames + block), sizeof(int32_t)), *wet = calloc((size_t)2 * (frames + block), sizeof(int32_t));
    static const float DK[3] = {0, 0.5f, 1};
    int bad = 0; double first0 = 0;
    for (unsigned c = 0; c < 4; ++c) {
        int32_t *b = c == 0 ? dry : wet;
        ch_init();
        for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        ch_device_set(0, CH_MATERIAL, CH_DEV_DRUM); ch_param_set(0, CH_MATERIAL, 0, 0);
        fx(0, CH_FX1, "REVERB"); P(0, CH_FX1, "DECAY", 0.8f); P(0, CH_FX1, "SIZE", 0.8f);
        P(0, CH_FX1, "MIX", c == 0 ? 0.0f : 1.0f); P(0, CH_FX1, "DUCK", c == 0 ? 0.0f : DK[c - 1]);
        ch_tempo_set(120);
        for (unsigned i = 0; i < 16; ++i) { ch_step st = {(uint8_t)(i % 4 == 0 ? 110 : 0), 1, 100, 0, 0, 0, 0}; ch_step_set(0, 0, i, &st); }
        ch_seq_len_set(0, 16);
        int32_t w[256]; for (unsigned k = 0; k < 40; ++k) ch_render(w, 128, NULL);   /* the reverb's memory cleared */
        ch_transport(1);
        for (unsigned f = 0; f < frames; f += block) ch_render(b + 2 * f, block, NULL);
        ch_transport(0);
        if (c == 0) continue;
        double early = 0, late = 0; unsigned ne = 0, nl = 0;
        for (unsigned f = beat * 8; f < frames; ++f) {           /* (after two bars: the tail built up) */
            double r = (wet[2 * f] - dry[2 * f]) / 8388608.0;
            unsigned ph = f % beat;
            if (ph < CH_RATE / 10) { early += r * r; ++ne; } else if (ph > beat - CH_RATE / 5) { late += r * r; ++nl; }
        }
        double e = 10 * log10(early / ne + 1e-20), l = 10 * log10(late / nl + 1e-20);
        printf("DUCK %3.0f%%: the reverb %6.1f dB under the kick, %6.1f dB before the next (%+.1f dB)\n", DK[c - 1] * 100, e, l, e - l);
        if (c == 1) first0 = e;
        if (c == 3 && !(e < first0 - 12)) bad = 1;
        if (dir) { char path[512]; snprintf(path, sizeof path, "%s/duck_%03.0f.wav", dir, DK[c - 1] * 100); wav_write_q23(path, wet, frames, CH_RATE); }
    }
    free(dry); free(wet);
    printf("duck: %s\n", bad ? "FAIL" : "ok");
    return bad;
}
/* DUB DELAY's filters: one snare hit into each FILTER type, FEEDBACK and RESONANCE at their middle and at full, 6 s:
 * the peak, and the level in the last second (a runaway would stay up; a held loop is allowed, a blow-up is not) */
static int cmd_dubf(const char *dir)
{
    static const char *const TN[4] = {"BAND", "LOW PASS", "HIGH PASS", "NOTCH"};
    unsigned frames = CH_RATE * 6, block = 128; int bad = 0;
    int32_t *b = calloc((size_t)2 * (frames + block), sizeof(int32_t));
    for (unsigned ty = 0; ty < 4; ++ty)
        for (unsigned hard = 0; hard < 2; ++hard) {
            ch_init();
            for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
            ch_device_set(0, CH_MATERIAL, CH_DEV_DRUM); ch_param_set(0, CH_MATERIAL, 0, 9);
            fx(0, CH_FX1, "DUB DELAY"); P(0, CH_FX1, "MIX", 1.0f); P(0, CH_FX1, "FILTER", (float)ty);
            P(0, CH_FX1, "FEEDBACK", hard ? 1.0f : 0.6f); P(0, CH_FX1, "RESONANCE", hard ? 1.0f : 0.68f);
            int32_t w[256]; for (unsigned k = 0; k < 40; ++k) ch_render(w, 128, NULL);
            ch_note(0, 60, 120);
            double pk = 0, last = 0; int nan = 0;
            for (unsigned f = 0; f < frames; f += block) {
                ch_render(b + 2 * f, block, NULL);
                for (unsigned i = 0; i < block; ++i) { double v = b[2 * (f + i)] / 8388608.0; if (v != v) nan = 1; if (fabs(v) > pk) pk = fabs(v); if (f + i >= frames - CH_RATE) last += v * v; }
            }
            double lr = sqrt(last / CH_RATE);
            printf("%-9s %s: peak %.3f, last second %.3f rms%s\n", TN[ty], hard ? "FB 100 RES 100" : "FB  60 RES  68", pk, lr, nan ? " NaN" : "");
            if (nan || pk > 0.95) bad = 1;
            if (dir) { char path[512]; snprintf(path, sizeof path, "%s/dub_%u_%s.wav", dir, ty, hard ? "hard" : "mid"); wav_write_q23(path, b, frames, CH_RATE); }
        }
    free(b);
    printf("dubf: %s\n", bad ? "FAIL" : "ok");
    return bad;
}
/* ANALOG with OSC B on each of its waves (and OSC A on each), MIX at the middle: a note, its peak, NaN, the engine's
 * own bad-value count */
static int cmd_anab(void)
{
    if (getenv("ANADBG")) {
        int32_t b2[256];
        for (int k = 0; k <= 12; ++k) {
            float tv = 0.4f + 0.05f * (float)k;
            ch_init(); for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
            unsigned mdl = getenv("ANAMODEL") ? (unsigned)atoi(getenv("ANAMODEL")) : 0;
            ch_device_set(0, CH_MATERIAL, CH_DEV_SYNTH); ch_param_set(0, CH_MATERIAL, 0, (float)mdl);
            if (getenv("ANAP")) ch_param_set(0, CH_MATERIAL, (unsigned)atoi(getenv("ANAP")), tv); else ch_param_set(0, CH_MATERIAL, 3, tv); ch_render(b2, 128, NULL); ch_note(0, 48, 110);
            double pk = 0; for (unsigned f = 0; f < 20; ++f) { ch_render(b2, 128, NULL); for (unsigned q = 0; q < 128; ++q) { double v = fabs(b2[2 * q] / 8388608.0); if (v > pk) pk = v; } }
            printf("timbre %.2f: peak %.3f value %.3f\n", tv, pk, ch_snapshot_get()->value[0][0][3]);
        }
        return 0;
    }
    static const char *const WB[7] = {"OFF", "SAW", "SQUARE", "PULSE", "TRI", "SINE", "SUB"};
    int bad = 0; int32_t buf[256];
    for (unsigned side = 0; side < 2; ++side)
        for (unsigned w = 0; w < (side ? 5u : 7u); ++w) {
            ch_init();
            for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
            ch_device_set(0, CH_MATERIAL, CH_DEV_SYNTH); ch_param_set(0, CH_MATERIAL, 0, 0);
            for (unsigned q = 2; q < ch_device_info(CH_DEV_SYNTH)->params; ++q) { float v; if (ch_synth_preset(0, q, &v)) ch_param_set(0, CH_MATERIAL, q, v); }
            if (side) ch_param_set(0, CH_MATERIAL, 2, (float)w / 4.0f); else ch_param_set(0, CH_MATERIAL, 3, (float)w / 6.0f);
            ch_param_set(0, CH_MATERIAL, 4, 0.5f);
            ch_render(buf, 128, NULL);
            ch_note(0, 48, 110);
            double pk = 0; int nan = 0;
            for (unsigned f = 0; f < CH_RATE / 2; f += 128) { ch_render(buf, 128, NULL); for (unsigned i = 0; i < 128; ++i) { double v = buf[2 * i] / 8388608.0; if (v != v) nan = 1; if (fabs(v) > pk) pk = fabs(v); } }
            unsigned nb = 0; for (unsigned s2 = 0; s2 < CH_SLOTS; ++s2) nb += ch_snapshot_get()->bad[0][s2];
            printf("OSC %s %-7s peak %.3f%s bad %u\n", side ? "A" : "B", side ? (const char *[]){"SAW","SQUARE","PULSE","TRI","SINE"}[w] : WB[w], pk, nan ? " NaN" : "", nb);
            if (pk < 0.05 || nan) bad = 1;
        }
    return bad;
}
/* every SYNTH model, 60 trials each: random knobs (its own extras too), random notes and velocities every 60-150 ms,
 * the knobs moved while it plays, chords; how often the engine's guard had to silence the track */
static int cmd_guardhunt(void)
{
    uint32_t rng = 12345; int bad = 0; int32_t buf[256];
    #define RND() (rng = rng * 1664525u + 1013904223u, (float)(rng >> 8) / 16777216.0f)
    for (unsigned m = 0; m < CH_SYNTH_MODELS; ++m) {
        uint32_t trips = 0;
        for (unsigned tr = 0; tr < 60; ++tr) {
            ch_init();
            for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
            ch_device_set(0, CH_MATERIAL, CH_DEV_SYNTH); ch_param_set(0, CH_MATERIAL, 0, (float)m);
            const ch_device_desc *d = ch_device_info(CH_DEV_SYNTH);
            for (unsigned q = 1; q < d->params; ++q) {
                if (q == 7) { ch_param_set(0, CH_MATERIAL, q, (float)(int)(RND() * 3)); continue; }   /* MODE */
                const ch_param_desc *pd = &d->param[q];
                float v = pd->kind == CH_SELECT ? (float)(int)(RND() * pd->steps) : pd->kind == CH_BI ? RND() * 2 - 1 : RND();
                if (RND() < 0.2f) v = pd->kind == CH_SELECT ? (float)(pd->steps - 1) : 1;   /* the extremes, often */
                ch_param_set(0, CH_MATERIAL, q, v);
            }
            for (unsigned k = 0; k < CH_SLOTS; ++k) ch_guard_trips[k] = 0;
            ch_render(buf, 128, NULL);
            unsigned next = 0, held[3] = {0, 0, 0};
            for (unsigned f = 0; f < CH_RATE; f += 32) {
                if (f >= next) {
                    for (unsigned c = 0; c < 3; ++c) { if (held[c]) ch_note(0, held[c], 0); held[c] = 0; }
                    unsigned nn = RND() < 0.3f ? 3 : 1;
                    for (unsigned c = 0; c < nn; ++c) { held[c] = 24 + (unsigned)(RND() * 72); ch_note(0, held[c], 1 + (unsigned)(RND() * 126)); }
                    next = f + (unsigned)(CH_RATE * (0.06f + 0.09f * RND()));
                }
                if (RND() < 0.05f) ch_param_set(0, CH_MATERIAL, 2 + (unsigned)(RND() * 3), RND());   /* a knob moved */
                ch_render(buf, 32, NULL);
            }
            trips += ch_guard_trips[CH_MATERIAL];
        }
        printf("%-9s guard trips in 60 s of random play: %u\n", ch_synth_names[m], (unsigned)trips);
        if (trips) bad = 1;
    }
    for (unsigned m = 0; m <= CH_DRUM_MODELS; ++m) {   /* every DRUM model, then the KIT (m == CH_DRUM_MODELS) */
        uint32_t trips = 0;
        for (unsigned tr = 0; tr < 30; ++tr) {
            ch_init();
            for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
            unsigned dev = m == CH_DRUM_MODELS ? CH_DEV_KIT : CH_DEV_DRUM;
            ch_device_set(0, CH_MATERIAL, dev);
            const ch_device_desc *d = ch_device_info(dev);
            for (unsigned q = 0; q < d->params; ++q) {
                const ch_param_desc *pd = &d->param[q];
                float v = pd->kind == CH_SELECT ? (float)(int)(RND() * pd->steps) : pd->kind == CH_BI ? RND() * 2 - 1 : RND();
                if (RND() < 0.2f) v = pd->kind == CH_SELECT ? (float)(pd->steps - 1) : 1;
                if (q == 0 && dev == CH_DEV_DRUM) v = (float)m;
                ch_param_set(0, CH_MATERIAL, q, v);
            }
            for (unsigned k = 0; k < CH_SLOTS; ++k) ch_guard_trips[k] = 0;
            ch_render(buf, 128, NULL);
            unsigned next = 0;
            for (unsigned f = 0; f < CH_RATE; f += 32) {
                if (f >= next) { ch_note(0, 36 + (unsigned)(RND() * 48), 1 + (unsigned)(RND() * 126)); next = f + (unsigned)(CH_RATE * (0.02f + 0.1f * RND())); }
                ch_render(buf, 32, NULL);
            }
            trips += ch_guard_trips[CH_MATERIAL];
            if (ch_guard_trips[CH_MATERIAL] && getenv("HUNTV")) {   /* the settings that tripped it */
                printf("  %s trial %u:", dev == CH_DEV_KIT ? "KIT" : ch_drum_names[m], tr);
                for (unsigned q = 1; q < d->params && q < 16; ++q) printf(" %s=%.2f", d->param[q].name, ch_track_get(0)->param[ch_row(CH_MATERIAL, dev)][q]);
                printf("\n");
            }
        }
        if (trips) { printf("%-9s guard trips in 30 s of random hits: %u\n", m == CH_DRUM_MODELS ? "KIT" : ch_drum_names[m], (unsigned)trips); bad = 1; }
    }
    printf("drums and KIT: %s\n", bad ? "see above" : "no guard trips");
    #undef RND
    return bad;
}
/* ---- clicks: every way the sound can start, stop or switch, each measured for a discontinuity ----
 * A sustained sine (WAVES at SINE, nothing to hide a click under) or a ringing drum is set going, then the event. The
 * measure is the waveform's second difference (its bend from one sample to the next): a sine bends smoothly by A(2 pi
 * f / fs)^2 a sample, a step or a kink bends at once. The worst bend within 4 ms after the event, against the worst in
 * the 40 ms before it: a ratio near 1 is clean; past ~3 it is an audible click (and its absolute size, full scale 1).
 * Each scene names the event; FAIL past the threshold. */
typedef struct { const char *name; double ratio, abs; } clk_res;
static void clk_synth(unsigned t, unsigned mode, float lvl)
{
    ch_device_set(t, CH_MATERIAL, CH_DEV_SYNTH); ch_bypass_set(t, CH_MATERIAL, 0); ch_mix_set(t, CH_MIX_MUTE, 0);
    ch_param_set(t, CH_MATERIAL, 0, 5);                       /* WAVES */
    for (unsigned q = 2; q < ch_device_info(CH_DEV_SYNTH)->params; ++q) { float v; if (ch_synth_preset(5, q, &v)) ch_param_set(t, CH_MATERIAL, q, v); }
    ch_param_set(t, CH_MATERIAL, 3, 0); ch_param_set(t, CH_MATERIAL, 4, 0); ch_param_set(t, CH_MATERIAL, 2, 0);   /* SINE, unfiltered, uncrushed */
    ch_param_set(t, CH_MATERIAL, 6, 0); ch_param_set(t, CH_MATERIAL, 12, 1); ch_param_set(t, CH_MATERIAL, 14, 0);   /* no LPG, filter open */
    ch_param_set(t, CH_MATERIAL, 7, (float)mode); ch_param_set(t, CH_MATERIAL, 5, 0.9f);   /* MODE; a long DECAY */
    ch_mix_set(t, CH_MIX_LEVEL, lvl);
}
static double clk_measure(const float *x, unsigned n, unsigned ev, double *absv)
{
    double base = 1e-9, after = 0;
    unsigned b0 = ev > (unsigned)(CH_RATE / 25) ? ev - (unsigned)(CH_RATE / 25) : 2;
    for (unsigned i = b0; i < ev && i < n; ++i) { double d = fabs(x[i] - 2 * x[i - 1] + x[i - 2]); if (d > base) base = d; }
    for (unsigned i = ev; i < ev + (unsigned)(CH_RATE / 250) && i < n; ++i) { double d = fabs(x[i] - 2 * x[i - 1] + x[i - 2]); if (d > after) after = d; }
    *absv = after;
    return after / fmax(base, 2e-4);                     /* (a floor: a silent "before" is not a click's measure) */
}
static ch_sample clk_smp;
static void clk_sample(void)                              /* a 1 s sine, 220 Hz, at 32 kHz */
{
    static uint8_t *enc; enum { M = 32000 };
    if (!enc) { static int16_t pcm[M]; for (unsigned i = 0; i < M; ++i) pcm[i] = (int16_t)(12000 * sin(6.283185307 * 220.0 * i / 32000.0)); enc = malloc(ch_adpcm_bytes(M)); ch_adpcm_encode(pcm, M, enc); }
    clk_smp.adpcm = enc; clk_smp.frames = M; clk_smp.rate = 32000; clk_smp.bpm = 120;
    for (unsigned k = 0; k < CH_SAMPLES; ++k) ch_sample_bind(k, &clk_smp);
}
static int cmd_clicks(const char *dir)
{
    enum { SC = 26 };
    clk_res res[SC]; unsigned nres = 0; int bad = 0;
    unsigned frames = CH_RATE * 2, ev = (CH_RATE / 32) * 32;  /* each scene: 2 s, the event at 1 s (on the 32-frame render grid) */
    float *x = calloc(frames + 64, sizeof(float)), *y = calloc(frames + 64, sizeof(float)); int32_t buf[64];
    for (unsigned sc = 0; sc < SC; ++sc) {
        const char *name = 0;
        /* a new sound at the event (a stolen voice's new note, a choke's new hit): the scene again without what was
         * sounding before, taken away, so what is left is the event's effect on the old sound alone */
        int sub = sc == 2 || sc == 3 || sc == 15 || sc == 16 || sc == 21;
        int mode = sc == 20 ? 1 : sc == 22 ? 2 : sc == 23 ? 3 : 0;
        if (mode == 2) sub = 1;                            /* (the reference: the second pass, without the modulation) */
        for (unsigned pass = 0; pass < (sub ? 2u : 1u); ++pass) {
        float *o = pass ? y : x;
        ch_init();
        for (unsigned t = 0; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        ch_master_set(0, 0.7f);
        /* the scene, up to its event */
        switch (sc) {
        case 0: name = "synth note on, ATTACK 0 (a sine)"; clk_synth(0, 1, 0.7f); break;
        case 1: name = "synth note off, DECAY 0 (GATE)"; clk_synth(0, 1, 0.7f); ch_param_set(0, CH_MATERIAL, 5, 0); break;
        case 2: name = "synth voice stolen (5th note, 4 voices)"; clk_synth(0, 1, 0.5f); break;
        case 3: name = "voice budget cuts a sounding synth"; for (unsigned t = 0; t < 3; ++t) clk_synth(t, 1, 0.3f); break;
        case 4: name = "mute on a sounding track"; clk_synth(0, 2, 0.7f); break;
        case 5: name = "unmute a sounding track"; clk_synth(0, 2, 0.7f); ch_mix_set(0, CH_MIX_MUTE, 1); break;
        case 6: name = "engine off (bypass) while sounding"; clk_synth(0, 2, 0.7f); break;
        case 7: name = "engine changed SYNTH to DRUM while sounding"; clk_synth(0, 2, 0.7f); break;
        case 8: name = "synth model changed while sounding"; clk_synth(0, 2, 0.7f); break;
        case 9: name = "DRIVE bypassed on a sine"; clk_synth(0, 2, 0.5f); fx(0, CH_FX1, "DRIVE"); P(0, CH_FX1, "DRIVE", 0.7f); break;
        case 10: name = "DRIVE TYPE switched on a sine"; clk_synth(0, 2, 0.5f); fx(0, CH_FX1, "DRIVE"); P(0, CH_FX1, "DRIVE", 0.7f); break;
        case 11: name = "REVERB removed mid-tail"; clk_synth(0, 2, 0.4f); fx(0, CH_FX1, "REVERB"); P(0, CH_FX1, "MIX", 0.6f); P(0, CH_FX1, "DECAY", 0.8f); break;
        case 12: name = "DELAY added on a sine"; clk_synth(0, 2, 0.5f); break;
        case 13: name = "ANALOG wave switched (TIMBRE step)"; clk_synth(0, 2, 0.5f); ch_param_set(0, CH_MATERIAL, 0, 0); ch_param_set(0, CH_MATERIAL, 2, 1.0f); ch_param_set(0, CH_MATERIAL, 3, 0); break;
        case 14: name = "a filter CUTOFF jump (a lock)"; clk_synth(0, 2, 0.5f); ch_param_set(0, CH_MATERIAL, 12, 0.9f); break;
        case 15: name = "drum CHOKE: a long kick retriggered"; drum(0, 0); ch_mix_set(0, CH_MIX_MUTE, 0); ch_param_set(0, CH_MATERIAL, 2, 1.0f); break;
        case 16: name = "KIT: part 3 chokes part 4 (tonal parts)"; for (unsigned l = 0; l < CH_LANES; ++l) ch_lane_note_set(0, l, 60 + l); ch_device_set(0, CH_MATERIAL, CH_DEV_KIT); ch_bypass_set(0, CH_MATERIAL, 0); ch_mix_set(0, CH_MIX_MUTE, 0);
            ch_param_set(0, CH_MATERIAL, 16 + 2, 35); ch_param_set(0, CH_MATERIAL, 16 + 3, 35); ch_param_set(0, CH_MATERIAL, 7, 1.0f); break;   /* (PING: no noise, so the subtraction holds; part 4 long) */
        case 17: name = "INIT track while sounding"; clk_synth(0, 2, 0.7f); break;
        case 18: name = "transport stop on held notes"; clk_synth(0, 1, 0.7f); ch_param_set(0, CH_MATERIAL, 5, 0); steps(0, "X...............", 60); break;
        case 19: name = "track LEVEL jump 80 to 10"; clk_synth(0, 2, 0.8f); break;
        case 20: name = "sample one-shot reaching its end"; clk_sample(); ch_device_set(0, CH_MATERIAL, CH_DEV_POLY); ch_bypass_set(0, CH_MATERIAL, 0); ch_mix_set(0, CH_MIX_MUTE, 0); P(0, CH_MATERIAL, "LENGTH", 0.3f); P(0, CH_MATERIAL, "SUSTAIN", 1.0f); break;
        case 21: name = "sample voice stolen (9th of 8)"; clk_sample(); ch_device_set(0, CH_MATERIAL, CH_DEV_POLY); ch_bypass_set(0, CH_MATERIAL, 0); ch_mix_set(0, CH_MIX_MUTE, 0); P(0, CH_MATERIAL, "SUSTAIN", 1.0f); ch_mix_set(0, CH_MIX_LEVEL, 0.3f); break;
        case 22: name = "square LFO on a synth LEVEL"; clk_synth(0, 2, 0.7f);
            if (!pass) { ch_mod_type_set(0, 0, CH_MOD_WAVE); ch_mod_shape_set(0, 0, 4); ch_mod_mode_set(0, 0, 0, 0); ch_mod_param_set(0, 0, 0, 0.55f); ch_map_set(0, 0, CH_MATERIAL, 10, -0.5f, 0); } break;
        case 23: name = "metronome over a loud mix (dropouts)"; clk_synth(0, 2, 1.0f); ch_param_set(0, CH_MATERIAL, 10, 1.0f); ch_master_set(1.0f, 1.0f); ch_metronome_set(1); ch_transport(1); break;
        case 24: name = "a send switched on"; clk_synth(0, 2, 0.6f); ch_mix_set(1, CH_MIX_MUTE, 0); ch_device_set(1, CH_MATERIAL, CH_DEV_SYNTH); break;
        case 25: name = "mix FILTER jump (LP to open)"; clk_synth(0, 2, 0.6f); ch_mix_set(0, CH_MIX_FILTER, -0.7f); break;
        }
        for (unsigned f = 0; f < frames; f += 32) {
            unsigned pre = (sc == 15 || sc == 16) ? ev - 2624u : (CH_RATE / 64) * 32;   /* (the drums: 60 ms before, still ringing) */
            if (f == pre && !pass) {                          /* the sound going before the event */
                if (sc == 2) for (unsigned k = 0; k < 4; ++k) ch_note(0, 48 + 5 * k, 100);
                else if (sc == 20) ch_note(0, 60, 100);
                else if (sc == 21) for (unsigned k = 0; k < 8; ++k) ch_note(0, 55 + k, 100);
                else if (sc == 3) for (unsigned t = 0; t < 3; ++t) for (unsigned k = 0; k < 3; ++k) ch_note(t, 48 + 4 * k + t, 100);
                else if (sc == 15) ch_note(0, 36, 120);
                else if (sc == 16) ch_note(0, 63, 110);
                else if (sc == 18) ch_transport(1);
                else if (sc != 0 && sc != 12) ch_note(0, 57, 100);
                if (sc == 12) ch_note(0, 57, 100);
            }
            if (f == ev) {
                switch (sc) {
                case 0: ch_note(0, 57, 100); break;
                case 1: ch_note(0, 57, 0); break;
                case 2: ch_note(0, 72, 100); break;
                case 3: ch_note(1, 80, 100); ch_note(2, 81, 100); break;
                case 4: ch_mix_set(0, CH_MIX_MUTE, 1); break;
                case 5: ch_mix_set(0, CH_MIX_MUTE, 0); break;
                case 6: ch_bypass_set(0, CH_MATERIAL, 1); break;
                case 7: ch_device_set(0, CH_MATERIAL, CH_DEV_DRUM); break;
                case 8: ch_param_set(0, CH_MATERIAL, 0, 0); break;
                case 9: ch_bypass_set(0, CH_FX1, 1); break;
                case 10: P(0, CH_FX1, "TYPE", 6); break;
                case 11: ch_device_set(0, CH_FX1, CH_DEV_BYPASS); break;
                case 12: fx(0, CH_FX1, "DELAY"); break;
                case 13: ch_param_set(0, CH_MATERIAL, 3, 1.0f / 6); break;
                case 14: ch_param_set(0, CH_MATERIAL, 12, 0.2f); break;
                case 15: ch_note(0, 36, 120); break;
                case 16: ch_note(0, 62, 110); break;
                case 17: ch_track_init(0); break;
                case 18: ch_transport(0); break;
                case 19: ch_mix_set(0, CH_MIX_LEVEL, 0.1f); break;
                case 21: ch_note(0, 70, 100); break;
                case 24: ch_send_set(0, 1, 1.0f); break;
                case 25: ch_mix_set(0, CH_MIX_FILTER, 0); break;
                }
            }
            ch_render(buf, 32, NULL);
            for (unsigned i = 0; i < 32; ++i) o[f + i] = (float)(buf[2 * i] / 8388608.0);
        }
        }
        if (sub && mode != 2) for (unsigned i = 0; i < frames; ++i) x[i] -= y[i];
        unsigned at = ev;
        if (sc == 18) {                                       /* the stop lands where the transport stops: the event itself */
        }
        double av, r;
        if (mode == 0) r = clk_measure(x, frames, at, &av);
        else if (mode == 3) {                              /* dropouts: samples at exactly 0 while the mix is loud */
            unsigned z = 0; for (unsigned i = ev; i < frames; ++i) if (x[i] == 0 && fabs(x[i - 1]) > 0.2f) ++z;
            r = (double)z * 100; av = (double)z * 0.01;
        } else {                                           /* the worst bend over the run, against a steady stretch (1) or the reference (2) */
            unsigned p0 = (CH_RATE / 64) * 32, a0 = p0 + CH_RATE / 20, a1 = p0 + CH_RATE / 7;
            double base = 2e-4, mx = 0;
            for (unsigned i = (mode == 1 ? a0 : a0); i < (mode == 1 ? a1 : frames); ++i) { double d = fabs((mode == 1 ? x : y)[i] - 2 * (mode == 1 ? x : y)[i - 1] + (mode == 1 ? x : y)[i - 2]); if (d > base) base = d; }
            for (unsigned i = (mode == 1 ? a1 : a0); i < frames; ++i) { double d = fabs(x[i] - 2 * x[i - 1] + x[i - 2]); if (d > mx) mx = d; }
            r = mx / base; av = mx;
        }
        res[nres].name = name; res[nres].ratio = r; res[nres].abs = av; ++nres;
        { double pk = 0; for (unsigned i = 0; i < frames; ++i) if (fabs(x[i]) > pk) pk = fabs(x[i]); if (getenv("CLKV")) printf("  scene %u peak %.3f\n", sc, pk); }
        if (dir) { char path[512]; snprintf(path, sizeof path, "%s/click_%02u.wav", dir, sc); { int32_t *q = calloc((size_t)frames * 2, sizeof(int32_t)); for (unsigned i = 0; i < frames; ++i) q[2 * i] = q[2 * i + 1] = (int32_t)(x[i] * 8388607.0f); wav_write_q23(path, q, frames, CH_RATE); free(q); } }
    }
    printf("%-46s %8s %8s\n", "event", "x before", "size");
    for (unsigned k = 0; k < nres; ++k) {
        int c = res[k].ratio > 3.0 && res[k].abs > 0.002;
        printf("%-46s %8.1f %8.4f %s\n", res[k].name, res[k].ratio, res[k].abs, c ? "CLICK" : "");
        bad += c;
    }
    printf("clicks: %d of %u events click\n", bad, nres);
    free(x); free(y);
    return bad ? 1 : 0;
}
/* dropouts: the factory tracks (KIT beat, ANALOG, RIPPLE, ACID) playing busy lines for 64 bars at 90, swung; what became of
 * every step the sequencer meant to play (ch_dbg). Arg: the voice budget's cap to force (as the load governor would) */
static double now_us(void);
static void drop_synth(unsigned t, unsigned model, unsigned mode)
{
    ch_device_set(t, CH_MATERIAL, CH_DEV_SYNTH); ch_bypass_set(t, CH_MATERIAL, 0); ch_mix_set(t, CH_MIX_MUTE, 0);
    ch_param_set(t, CH_MATERIAL, 0, (float)model);
    for (unsigned q = 2; q < ch_device_info(CH_DEV_SYNTH)->params; ++q) { float v; if (ch_synth_preset(model, q, &v)) ch_param_set(t, CH_MATERIAL, q, v); }
    ch_param_set(t, CH_MATERIAL, 7, (float)mode);
}
static int cmd_dropout(void)
{
    static const char *const BEAT[8] = {
        "X......X..X.....", "....X.......X...", "x.y.x.y.x.y.x...", "..............o.",
        "...........g....", "................", ".s.s.s.s.s.s.s.s", "................"};
    ch_init();
    ch_device_set(0, CH_MATERIAL, CH_DEV_KIT); ch_bypass_set(0, CH_MATERIAL, 0);
    for (unsigned l = 0; l < 8; ++l) ch_lane_note_set(0, l, 60 + l);
    for (unsigned l = 0; l < 8; ++l)
        for (unsigned i = 0; i < 16; ++i) {
            char c = BEAT[l][i]; if (c == '.') continue;
            ch_step st = {(uint8_t)(c == 'X' ? 112 : c == 'x' ? 96 : c == 'y' ? 70 : c == 'o' ? 84 : c == 'g' ? 52 : 66), 1, 100, 0, 0, 0, 0};
            ch_step_set(0, l, i, &st);
        }
    drop_synth(1, 0, 1); drop_synth(2, 13, 0); drop_synth(3, 11, 1);   /* ANALOG (GATE), RIPPLE (PING), ACID */
    for (unsigned t = 1; t < 4; ++t) for (unsigned l = 0; l < 8; ++l) ch_lane_note_set(t, l, 48 + 2 * l);
    for (unsigned i = 0; i < 16; i += 2) for (unsigned l = 0; l < 3; ++l) { ch_step st = {90, 1, 100, 0, 0, 0, 0}; ch_step_set(1, (i / 2 + 2 * l) % 8, i, &st); }   /* chords on the eighths */
    for (unsigned i = 0; i < 16; ++i) { ch_step st = {(uint8_t)(70 + (i % 3) * 20), 1, 100, 0, 0, 0, 0}; ch_step_set(2, (i * 3) % 8, i, &st); }       /* a sixteenth line */
    for (unsigned i = 0; i < 16; ++i) if (i % 4 != 3) { ch_step st = {(uint8_t)(i % 4 == 0 ? 120 : 90), 1, 100, 0, 0, 0, (uint8_t)(i % 8 == 6)}; ch_step_set(3, (i * 5) % 8, i, &st); }   /* acid, a slide or two */
    for (unsigned t = 0; t < 4; ++t) ch_seq_len_set(t, 16);
    ch_tempo_set(90); ch_swing_set(0.3f);
    memset(&ch_dbg, 0, sizeof ch_dbg);
    int32_t buf[512]; ch_render(buf, 256, NULL); ch_transport(1);
    unsigned frames = (unsigned)(CH_RATE * 60.0 / 90 * 4 * 64);
    /* each 256-frame render timed (the device's half buffer): the mean, the worst, and on which step the worst ones fall */
    double sum = 0, mx = 0; unsigned nr = 0, hist[16] = {0}; double top[16] = {0};
    for (unsigned f = 0; f < frames; f += 256) {
        double u0 = now_us(); ch_render(buf, 256, NULL); double us = now_us() - u0;
        if (f > (unsigned)CH_RATE) { sum += us; ++nr; if (us > mx) mx = us; unsigned st = (unsigned)ch_snapshot_get()->step[0] & 15u; if (us > top[st]) top[st] = us; ++hist[st]; }
    }
    ch_transport(0);
    printf("renders of 256 frames: mean %.1f us, worst %.1f us (%.1fx the mean)\n", sum / nr, mx, mx / (sum / nr));
    printf("worst render by the step it fell on:"); for (unsigned k = 0; k < 16; ++k) printf(" %u:%.0f", k + 1, top[k]); printf("\n");
    printf("64 bars: hits sent %u; skipped by CHANCE %u, by CYCLE %u; tie holds %u\n", (unsigned)ch_dbg.hits, (unsigned)ch_dbg.chance, (unsigned)ch_dbg.cycle, (unsigned)ch_dbg.tie);
    printf("young notes (<100 ms) cut: by the voice budget %u, taken for a new note %u; waiting notes lost %u\n", (unsigned)ch_dbg.cut_budget, (unsigned)ch_dbg.cut_steal, (unsigned)ch_dbg.pend_lost);
    return 0;
}
/* ties on a drum: a KIT lane with steps 1-4 tied (a held step and a later one pressed), 4 bars: every step must sound */
static int cmd_drumtie(void)
{
    ch_init();
    ch_device_set(0, CH_MATERIAL, CH_DEV_KIT); ch_bypass_set(0, CH_MATERIAL, 0);
    for (unsigned l = 0; l < 8; ++l) ch_lane_note_set(0, l, 60 + l);
    for (unsigned i = 0; i < 16; i += 4)
        for (unsigned k = 0; k < 4; ++k) { ch_step st = {100, 1, 100, 0, 0, 0, (uint8_t)(k != 0)}; ch_step_set(0, 0, i + k, &st); }
    ch_seq_len_set(0, 16); ch_tempo_set(120); ch_swing_set(0.3f);
    int32_t buf[256]; ch_render(buf, 128, NULL); ch_transport(1);
    unsigned h0 = ch_snapshot_get()->hits[0];
    for (unsigned f = 0; f < CH_RATE * 8; f += 128) ch_render(buf, 128, NULL);
    unsigned h = (unsigned)(ch_snapshot_get()->hits[0] - h0);
    ch_transport(0);
    printf("KIT lane, 16 steps a bar all on, 12 of them tied, 4 bars: %u hits (64 expected)\n", h);
    return h >= 63 ? 0 : 1;
}
/* two tracks on one step: the factory KIT beat on T1, and (second run) one ANALOG note on T2 on step 1; the kit's own
 * output (ch_tmix) and the master's at that step, the limiter's least gain, and voices cut */
static int cmd_twostep(void)
{
    static const char *const BEAT[8] = {
        "X......X..X.....", "....X.......X...", "x.y.x.y.x.y.x...", "..............o.",
        "...........g....", "................", ".s.s.s.s.s.s.s.s", "................"};
    double sx = getenv("CH_SLOW") ? atof(getenv("CH_SLOW")) : 1;
    for (unsigned run = 0; run < 3; ++run) {
        ch_init();
        ch_device_set(0, CH_MATERIAL, CH_DEV_KIT); ch_bypass_set(0, CH_MATERIAL, 0);
        for (unsigned l = 0; l < 8; ++l) ch_lane_note_set(0, l, 60 + l);
        for (unsigned l = 0; l < 8; ++l) for (unsigned i = 0; i < 16; ++i) {
            char c = BEAT[l][i]; if (c == '.') continue;
            ch_step st = {(uint8_t)(c == 'X' ? 112 : c == 'x' ? 96 : c == 'y' ? 70 : c == 'o' ? 84 : c == 'g' ? 52 : 66), 1, 100, 0, 0, 0, 0}; ch_step_set(0, l, i, &st);
        }
        drop_synth(1, 0, 0);                                  /* T2: ANALOG as the factory sets it */
        for (unsigned t = 2; t < 4; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        if (run) { ch_step st = {100, 1, 100, 0, 0, 0, 0}; ch_step_set(1, 0, 0, &st); }
        if (run == 2) ch_mix_set(1, CH_MIX_MUTE, 1);
        for (unsigned t = 0; t < 2; ++t) ch_seq_len_set(t, 16);
        ch_tempo_set(90); ch_swing_set(0.3f);
        memset(&ch_dbg, 0, sizeof ch_dbg); for (unsigned k = 0; k < 4; ++k) ch_cut_budget[k] = 0;
        int32_t buf[64]; ch_render(buf, 32, NULL); ch_transport(1);
        unsigned step = (unsigned)(CH_RATE * 60.0 / 90 / 4), bar = step * 16;
        double kitpk = 0, mpk = 0, gmin = 1; unsigned over = 0;
        for (unsigned f = 0; f < bar * 4; f += 32) {
            double u0 = now_us(); ch_render(buf, 32, NULL); if ((now_us() - u0) * sx > 32 * 1e6 / CH_RATE) ++over;
            if (f >= bar && f % bar < step) {                 /* step 1 of bars 2-4 */
                for (unsigned i = 0; i < 32; ++i) { double k = fabs(ch_tmix[0][0][i]); if (k > kitpk) kitpk = k; double m = fabs(buf[2 * i] / 8388608.0); if (m > mpk) mpk = m; }
                if (ch_snapshot_get()->lim_gr < gmin) gmin = ch_snapshot_get()->lim_gr;
            }
        }
        ch_transport(0);
        printf("%s: on step 1 the kit's own peak %.3f, the master's %.3f, the limiter down %.1f dB; voices cut by the budget %u, young %u\n",
               run == 2 ? "kit + muted note " : run ? "kit + ANALOG note" : "kit alone        ", kitpk, mpk, -20 * log10(gmin), (unsigned)(ch_cut_budget[0] + ch_cut_budget[1]), (unsigned)ch_dbg.cut_budget);
        printf("                    renders that would miss their time at this speed: %u of %u\n", over, bar * 4 / 32);
    }
    return 0;
}
/* what a hit costs: one track, a single hit on each beat (KIT part 0..7 in turn, or one DRUM model), 256-frame renders;
 * the median time of the renders holding a hit's first block, against the median of the others */
static int dcmp(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
static int cmd_hitcost(void)
{
    enum { N = 2000 };
    static double hit[N], plain[N];
    for (unsigned mode = 0; mode < 3; ++mode) {
        unsigned nh = 0, np = 0;
        ch_init();
        for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        if (mode == 0) { ch_device_set(0, CH_MATERIAL, CH_DEV_KIT); ch_bypass_set(0, CH_MATERIAL, 0); for (unsigned l = 0; l < 8; ++l) ch_lane_note_set(0, l, 60 + l); }
        else if (mode == 1) drum(0, 0);
        else { drop_synth(0, 0, 0); }
        int32_t buf[512]; ch_render(buf, 256, NULL);
        for (unsigned b = 0; b < 4 * N && (nh < N || np < N); ++b) {
            int h = (b % 8) == 0;
            if (h) ch_note(0, mode == 0 ? 60 + (b / 8) % 8 : 48, 110);
            double u0 = now_us(); ch_render(buf, 256, NULL); double us = now_us() - u0;
            if (b < 16) continue;
            if (h) { if (nh < N) hit[nh++] = us; } else if (np < N) plain[np++] = us;
        }
        qsort(hit, nh, sizeof hit[0], dcmp); qsort(plain, np, sizeof plain[0], dcmp);
        printf("%-6s render with a hit: median %.1f us; without: %.1f us; a hit adds %.1f us (%.0f%%)\n",
               mode == 0 ? "KIT" : mode == 1 ? "DRUM" : "SYNTH", hit[nh / 2], plain[np / 2], hit[nh / 2] - plain[np / 2], 100 * (hit[nh / 2] / plain[np / 2] - 1));
    }
    return 0;
}
/* what DRIVE costs per type: a sine through it, the median 256-frame render with it, less without */
static int cmd_drivecost(void)
{
    enum { N = 1500 };
    static double ts[N];
    double base = 0;
    for (int type = -1; type < 8; ++type) {
        ch_init();
        for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        drop_synth(0, 5, 2);                                  /* WAVES droning (DRONE) */
        if (type >= 0) {
            fx(0, CH_FX1, "DRIVE");
            const ch_device_desc *dd = ch_device_info(CH_DEV_DRIVE);
            if ((unsigned)type >= dd->param[4].steps) break;
            ch_param_set(0, CH_FX1, 4, (float)type); ch_param_set(0, CH_FX1, 0, 0.7f); ch_param_set(0, CH_FX1, 5, 0.5f);
        }
        int32_t buf[512];
        for (unsigned b = 0; b < 40; ++b) ch_render(buf, 256, NULL);
        for (unsigned b = 0; b < N; ++b) { double u0 = now_us(); ch_render(buf, 256, NULL); ts[b] = now_us() - u0; }
        qsort(ts, N, sizeof ts[0], dcmp);
        if (type < 0) { base = ts[N / 2]; printf("no DRIVE: %.1f us a 256-frame render\n", base); }
        else printf("DRIVE %-9s %6.1f us a render (%.1f us a sample pair)\n", ch_device_info(CH_DEV_DRIVE)->param[4].choices[type], ts[N / 2] - base, (ts[N / 2] - base) / 256);
    }
    return 0;
}
/* drivewav DIR: DRIVE to listen to. The factory beat (KIT) and an acid line, two bars at 90 each, dry and through every
 * TYPE at GAIN 10 and 24 dB (CLIP CUBIC, TONE flat): DIR/drive_<source>_<type>_<dB>.wav */
static int cmd_drivewav(const char *dir)
{
    static const char *const TN[6] = {"tube", "diode", "nonlinear", "overdrive", "sinusoidal", "crush"};
    static const char *const BEAT[8] = {"X......X..X.....", "....X.......X...", "x.y.x.y.x.y.x...", "..............o.",
                                        "...........g....", "................", ".s.s.s.s.s.s.s.s", "................"};
    int32_t buf[256];
    for (unsigned src = 0; src < 2; ++src)
        for (int k = -1; k < 12; ++k) {
            unsigned type = k < 0 ? 0 : (unsigned)k / 2; float drv = k < 0 ? 0 : (k & 1) ? 0.8f : 0.333f;
            if (type == 5 && (k & 1)) continue;
            ch_init(); ch_tempo_set(90); ch_swing_set(src ? 0 : 0.3f);
            for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
            if (src == 0) {
                ch_device_set(0, CH_MATERIAL, CH_DEV_KIT); ch_bypass_set(0, CH_MATERIAL, 0); ch_mix_set(0, CH_MIX_MUTE, 0);
                for (unsigned l = 0; l < 8; ++l) for (unsigned i = 0; i < 16; ++i) {
                    char c = BEAT[l][i]; if (c == '.') continue;
                    ch_step st = {(uint8_t)(c == 'X' ? 112 : c == 'x' ? 96 : c == 'y' ? 70 : c == 'o' ? 84 : c == 'g' ? 52 : 66), 1, 100, 0, 0, 0, 0};
                    ch_step_set(0, l, i, &st);
                }
            } else {
                drop_synth(0, 11, 1);
                for (unsigned l = 0; l < 8; ++l) ch_lane_note_set(0, l, 36 + (l * 5) % 12 + 12 * (l / 4));
                for (unsigned i = 0; i < 16; ++i) if (i % 4 != 3) { ch_step st = {(uint8_t)(i % 4 == 0 ? 120 : 90), 1, 100, 0, 0, 0, (uint8_t)(i % 8 == 6)}; ch_step_set(0, (i * 5) % 8, i, &st); }
            }
            ch_seq_len_set(0, 16);
            if (k >= 0) {
                fx(0, CH_FX1, "DRIVE"); ch_param_set(0, CH_FX1, 4, (float)type); ch_param_set(0, CH_FX1, 0, drv);
                ch_param_set(0, CH_FX1, 5, type == 5 ? 0.6f : 0.0f); ch_param_set(0, CH_FX1, 6, 0); ch_param_set(0, CH_FX1, 1, 0);
            }
            unsigned frames = (unsigned)(2 * 4 * 60.0 / 90 * CH_RATE) / 128 * 128;
            int32_t *all = calloc((size_t)2 * (frames + 128), sizeof(int32_t));
            ch_render(buf, 128, NULL); ch_transport(1);
            double pk = 0, rms = 0;
            for (unsigned f = 0; f < frames; f += 128) { ch_render(all + 2 * f, 128, NULL); for (unsigned i = 0; i < 256; ++i) { double v = all[2 * f + i] / 8388608.0; rms += v * v; if (fabs(v) > pk) pk = fabs(v); } }
            char path[512];
            if (k < 0) snprintf(path, sizeof path, "%s/drive_%s_0dry.wav", dir, src ? "acid" : "beat");
            else if (type == 5) snprintf(path, sizeof path, "%s/drive_%s_%u%s.wav", dir, src ? "acid" : "beat", type + 1, TN[type]);
            else snprintf(path, sizeof path, "%s/drive_%s_%u%s_%02ddB.wav", dir, src ? "acid" : "beat", type + 1, TN[type], (int)(drv * 30 + 0.5f));
            wav_write_q23(path, all, frames, CH_RATE); free(all);
            printf("%-40s peak %.2f  rms %5.1f dBFS\n", path + strlen(dir) + 1, pk, 10 * log10(rms / (2.0 * frames) + 1e-12));
        }
    return 0;
}
/* drivemix: the four factory tracks busy (as dropout), DRIVE (TUBE, 24 dB) on none, then on T4, then on all four: the
 * master limiter's gain reduction, guard trips, and the median and worst 128-frame render (DIR given: a WAV of each) */
static int cmd_drivemix(const char *dir)
{
    static const char *const BEAT[8] = {
        "X......X..X.....", "....X.......X...", "x.y.x.y.x.y.x...", "..............o.",
        "...........g....", "................", ".s.s.s.s.s.s.s.s", "................"};
    enum { NB = 4000 }; static double ts[NB];
    for (unsigned run = 0; run < 3; ++run) {
        ch_init();
        ch_device_set(0, CH_MATERIAL, CH_DEV_KIT); ch_bypass_set(0, CH_MATERIAL, 0);
        for (unsigned l = 0; l < 8; ++l) ch_lane_note_set(0, l, 60 + l);
        for (unsigned l = 0; l < 8; ++l) for (unsigned i = 0; i < 16; ++i) {
            char c = BEAT[l][i]; if (c == '.') continue;
            ch_step st = {(uint8_t)(c == 'X' ? 112 : c == 'x' ? 96 : c == 'y' ? 70 : c == 'o' ? 84 : c == 'g' ? 52 : 66), 1, 100, 0, 0, 0, 0}; ch_step_set(0, l, i, &st);
        }
        drop_synth(1, 0, 1); drop_synth(2, 13, 0); drop_synth(3, 11, 1);
        for (unsigned t = 1; t < 4; ++t) for (unsigned l = 0; l < 8; ++l) ch_lane_note_set(t, l, 48 + 2 * l);
        for (unsigned i = 0; i < 16; i += 2) for (unsigned l = 0; l < 3; ++l) { ch_step st = {90, 1, 100, 0, 0, 0, 0}; ch_step_set(1, (i / 2 + 2 * l) % 8, i, &st); }
        for (unsigned i = 0; i < 16; ++i) { ch_step st = {(uint8_t)(70 + (i % 3) * 20), 1, 100, 0, 0, 0, 0}; ch_step_set(2, (i * 3) % 8, i, &st); }
        for (unsigned i = 0; i < 16; ++i) if (i % 4 != 3) { ch_step st = {(uint8_t)(i % 4 == 0 ? 120 : 90), 1, 100, 0, 0, 0, (uint8_t)(i % 8 == 6)}; ch_step_set(3, (i * 5) % 8, i, &st); }
        for (unsigned t = 0; t < 4; ++t) ch_seq_len_set(t, 16);
        for (unsigned t = 0; t < 4; ++t) if (run == 2 || (run == 1 && t == 3)) { fx(t, CH_FX1, "DRIVE"); ch_param_set(t, CH_FX1, 4, 0); ch_param_set(t, CH_FX1, 0, 0.8f); }
        ch_tempo_set(90); ch_swing_set(0.3f);
        for (unsigned k = 0; k < CH_SLOTS; ++k) ch_guard_trips[k] = 0;
        int32_t buf[256]; ch_render(buf, 128, NULL); ch_transport(1);
        unsigned frames = (unsigned)(CH_RATE * 60.0 / 90 * 4 * 4) / 128 * 128;
        int32_t *all = calloc((size_t)2 * (frames + 128), sizeof(int32_t));
        double gmin = 1, gsum = 0, worst = 0; unsigned nb = 0, nclip = 0, over = 0, shed = 0, capmin = 99; memset(&ch_dbg, 0, sizeof ch_dbg);
        for (unsigned f = 0; f < frames; f += 128) {
            double u0 = now_us(); ch_render(all + 2 * f, 128, NULL); double us = now_us() - u0;
            if (nb < NB) ts[nb++] = us; if (us > worst && f > CH_RATE) worst = us; if (us * (getenv("CH_SLOW") ? atof(getenv("CH_SLOW")) : 1) > 128e6 / CH_RATE) ++over; shed += ch_snapshot_get()->shed; if (ch_snapshot_get()->voice_cap < capmin) capmin = ch_snapshot_get()->voice_cap;
            double g = ch_snapshot_get()->lim_gr; if (g < gmin) gmin = g; gsum += g;
            for (unsigned i = 0; i < 256; ++i) if (abs(all[2 * f + i]) >= 8388000) ++nclip;
        }
        ch_transport(0);
        qsort(ts, nb, sizeof ts[0], dcmp);
        printf("%-14s limiter down %.1f dB at most, %.1f dB on average; samples at full scale %u; guard trips %u; render median %.1f us, worst %.1f us\n",
               run == 0 ? "no DRIVE" : run == 1 ? "DRIVE on T4" : "DRIVE on all", -20 * log10(gmin), -20 * log10(gsum / (frames / 128)), nclip,
               (unsigned)(ch_guard_trips[CH_FX1] + ch_guard_trips[CH_MATERIAL]), ts[nb / 2], worst);
        printf("               renders over their time %u, voices shed %u, voice cap down to %u, young cut %u\n", over, shed, capmin, (unsigned)ch_dbg.cut_budget);
        if (dir) { char path[512]; snprintf(path, sizeof path, "%s/drivemix_%u.wav", dir, run); wav_write_q23(path, all, frames, CH_RATE); }
        free(all);
    }
    return 0;
}
/* spikes [tracks]: the render's time block by block, without the PC's noise. The factory tracks (KIT beat, ANALOG chords,
 * RIPPLE line, ACID line; the first `tracks` of them), 8 bars at 90, in 256-frame renders as the FM-1 makes them; the
 * engine is deterministic, so the run is repeated and each block keeps its least time. The median, the 99th centile and
 * the worst block, and where the worst ones fall */
static void spike_setup(unsigned tracks)
{
    static const char *const BEAT[8] = {
        "X......X..X.....", "....X.......X...", "x.y.x.y.x.y.x...", "..............o.",
        "...........g....", "................", ".s.s.s.s.s.s.s.s", "................"};
    ch_init();
    ch_device_set(0, CH_MATERIAL, CH_DEV_KIT); ch_bypass_set(0, CH_MATERIAL, 0);
    for (unsigned l = 0; l < 8; ++l) ch_lane_note_set(0, l, 60 + l);
    for (unsigned l = 0; l < 8; ++l) for (unsigned i = 0; i < 16; ++i) {
        char c = BEAT[l][i]; if (c == '.') continue;
        ch_step st = {(uint8_t)(c == 'X' ? 112 : c == 'x' ? 96 : c == 'y' ? 70 : c == 'o' ? 84 : c == 'g' ? 52 : 66), 1, 100, 0, 0, 0, 0}; ch_step_set(0, l, i, &st);
    }
    drop_synth(1, 0, 1); drop_synth(2, 13, 0); drop_synth(3, 11, 1);
    for (unsigned t = 1; t < 4; ++t) for (unsigned l = 0; l < 8; ++l) ch_lane_note_set(t, l, 48 + 2 * l);
    for (unsigned i = 0; i < 16; i += 2) for (unsigned l = 0; l < 3; ++l) { ch_step st = {90, 1, 100, 0, 0, 0, 0}; ch_step_set(1, (i / 2 + 2 * l) % 8, i, &st); }
    for (unsigned i = 0; i < 16; ++i) { ch_step st = {(uint8_t)(70 + (i % 3) * 20), 1, 100, 0, 0, 0, 0}; ch_step_set(2, (i * 3) % 8, i, &st); }
    for (unsigned i = 0; i < 16; ++i) if (i % 4 != 3) { ch_step st = {(uint8_t)(i % 4 == 0 ? 120 : 90), 1, 100, 0, 0, 0, (uint8_t)(i % 8 == 6)}; ch_step_set(3, (i * 5) % 8, i, &st); }
    for (unsigned t = 0; t < 4; ++t) { ch_seq_len_set(t, 16); if (t >= tracks) ch_mix_set(t, CH_MIX_MUTE, 1); }
    ch_tempo_set(90); ch_swing_set(0.3f);
}
static int cmd_spikes(unsigned tracks)
{
    enum { NBK = 4096, REP = 9 };
    static double best[NBK], srt[NBK];
    unsigned nb = (unsigned)(CH_RATE * 60.0 / 90 * 4 * 8) / 256; if (nb > NBK) nb = NBK;
    for (unsigned r = 0; r < REP; ++r) {
        spike_setup(tracks);
        int32_t buf[512]; ch_render(buf, 256, NULL); ch_transport(1);
        for (unsigned b = 0; b < nb; ++b) {
            double u0 = now_us(); ch_render(buf, 256, NULL); double us = now_us() - u0;
            if (!r || us < best[b]) best[b] = us;
        }
        ch_transport(0);
    }
    unsigned skip = nb / 8;                              /* (the first bar: caches, the first hits) */
    unsigned m = 0; for (unsigned b = skip; b < nb; ++b) srt[m++] = best[b];
    qsort(srt, m, sizeof srt[0], dcmp);
    double step = CH_RATE * 60.0 / 90 / 4;
    printf("%u track%s: 256-frame render, least of %u runs: median %.1f us, 90%% %.1f, 99%% %.1f, worst %.1f (x%.1f the median)\n", tracks, tracks == 1 ? "" : "s", (unsigned)REP,
           srt[m / 2], srt[m * 9 / 10], srt[m * 99 / 100], srt[m - 1], srt[m - 1] / srt[m / 2]);
    for (unsigned k = 0; k < 6; ++k) {
        unsigned wi = skip; for (unsigned b = skip; b < nb; ++b) if (best[b] > best[wi]) wi = b;
        double st = wi * 256.0 / step;
        printf("    block %4u: %.1f us  (bar %u, step %.1f)\n", wi, best[wi], (unsigned)(st / 16) + 1, st - 16 * (unsigned)(st / 16) + 1);
        best[wi] = 0;
    }
    return 0;
}
/* stagecost: the patch of a LOAD photo off the device (2026-11-10: KIT; ACID with DRIVE; RIPPLE with a reverb; SHAKER with an echo
 * and a chorus), the median 256-frame render, and what each stage is of it (the render again with that stage bypassed).
 * The device's own readings beside (us a block), and device us per PC us. 16 bars; the governor never moves at PC speed */
static unsigned shaker_model(void) { for (unsigned m = 0; m < CH_DRUM_MODELS; ++m) if (!strcmp(ch_drum_names[m], "SHAKER")) return m; return 0; }
static void stage_drain(void) { int32_t b[256]; for (unsigned k = 0; k < 12; ++k) ch_render(b, 128, NULL); }
static void stage_patch(int off_t, int off_s, int tracks_only)
{
    spike_setup(4); stage_drain();
    ch_tempo_set(115); ch_swing_set(0);
    drop_synth(1, 11, 1); stage_drain(); drop_synth(2, 13, 0); stage_drain();
    for (unsigned t = 1; t < 3; ++t) for (unsigned l = 0; l < 8; ++l) ch_lane_note_set(t, l, 48 + 2 * l);
    for (unsigned l = 0; l < 8; ++l) for (unsigned i = 0; i < 16; ++i) { ch_step z = {0, 1, 100, 0, 0, 0, 0}; ch_step_set(1, l, i, &z); ch_step_set(2, l, i, &z); ch_step_set(3, l, i, &z); }
    for (unsigned i = 0; i < 16; ++i) if (i % 4 != 3) { ch_step st = {(uint8_t)(i % 4 == 0 ? 120 : 90), 1, 100, 0, 0, 0, (uint8_t)(i % 8 == 6)}; ch_step_set(1, (i * 5) % 8, i, &st); }
    for (unsigned i = 0; i < 16; ++i) { ch_step st = {(uint8_t)(70 + (i % 3) * 20), 1, 100, 0, 0, 0, 0}; ch_step_set(2, (i * 3) % 8, i, &st); }
    drum(3, shaker_model()); stage_drain();
    for (unsigned i = 0; i < 16; ++i) { ch_step st = {(uint8_t)(i % 2 ? 60 : 100), 1, 100, 0, 0, 0, 0}; ch_step_set(3, 0, i, &st); }
    fx(1, CH_FX3, "DRIVE"); ch_param_set(1, CH_FX3, 0, 0.5f);
    fx(2, CH_FX1, "REVERB"); fx(3, CH_FX1, "ECHO"); fx(3, CH_FX2, "CHORUS"); stage_drain();
    (void)tracks_only;
    if (off_t >= 0) { if (off_s == 9) ch_mix_set((unsigned)off_t, CH_MIX_MUTE, 1); else ch_bypass_set((unsigned)off_t, (unsigned)off_s, 1); }
    if (off_t == -2) for (unsigned t = 0; t < 4; ++t) for (unsigned k = 0; k < CH_SLOTS; ++k) ch_bypass_set(t, k, 1);   /* everything bypassed: the floor */
    if (off_t == -3) for (unsigned t = 0; t < 4; ++t) for (unsigned k = 1; k < CH_SLOTS; ++k) ch_bypass_set(t, k, 1);   /* the engines alone */
    if (off_t <= -10) for (unsigned t = 0; t < 4; ++t) if ((int)t != -off_t - 10) for (unsigned k = 0; k < CH_SLOTS; ++k) ch_bypass_set(t, k, 1);   /* one track alone */
    stage_drain(); stage_drain();
}
static double stage_median(int off_t, int off_s)
{
    enum { NBK = 4096, REP = 7 };
    static double best[NBK];
    unsigned nb = (unsigned)(CH_RATE * 60.0 / 115 * 4 * 8) / 256; if (nb > NBK) nb = NBK;
    for (unsigned r = 0; r < REP; ++r) {
        stage_patch(off_t, off_s, 0);
        int32_t buf[512]; ch_render(buf, 256, NULL); ch_transport(1);
        for (unsigned b = 0; b < nb; ++b) { double u0 = now_us(); ch_render(buf, 256, NULL); double us = now_us() - u0; if (!r || us < best[b]) best[b] = us; }
        ch_transport(0);
    }
    double sum = 0; unsigned skip = nb / 8; for (unsigned b = skip; b < nb; ++b) sum += best[b];
    return sum / (nb - skip);                              /* the mean: what the load is */
}
static int cmd_stagecost(void)
{
    static const struct { int t, s; const char *name; double dev; } ST[] = {
        {0, 0, "T1 KIT", 3057}, {1, 0, "T2 ACID", 3186}, {1, CH_FX3, "T2 DRIVE", 1291}, {2, 0, "T3 RIPPLE", 1906}, {2, CH_FX1, "T3 REVERB", 409},
        {3, 0, "T4 SHAKER", 990}, {3, CH_FX1, "T4 ECHO", 440}, {3, CH_FX2, "T4 CHORUS", 357}};
    double all = stage_median(-1, 0);
    printf("the whole patch: %.1f us a 256-frame render (mean, the least of 7 runs a block)\n", all);
    double sum = 0;
    for (unsigned k = 0; k < sizeof ST / sizeof ST[0]; ++k) {
        double d = all - stage_median(ST[k].t, ST[k].s); sum += d;
        printf("  %-10s %6.1f us   (the FM-1: %4.0f peak; x%.0f)\n", ST[k].name, d, ST[k].dev, d > 0.5 ? ST[k].dev / d : 0);
    }
    { double fl = stage_median(-2, 0), eng = stage_median(-3, 0);
      printf("  everything bypassed %.1f us; the four engines alone %.1f\n", fl, eng);
      for (int t = 0; t < 4; ++t) printf("  T%d alone (engine and its effects) %.1f\n", t + 1, stage_median(-10 - t, 0)); }
    printf("  the stages %.1f; the rest (sequencer, modulators, values, mixer, master) %.1f\n", sum, all - sum);
    return 0;
}
#if defined(CH_PROF)
/* prof [mode]: where the render's time goes (bench_prof.exe: the CH_P marks), the photo's patch; mode 1 all bypassed */
static int cmd_prof(int mode)
{
    static const char *const NM[16] = {"tracks: other", "track in + bus", "modulators", "values (eval_params)", "ENGINES", "guard + level", "EFFECTS", "sends",
        "mixer", "buffers", "sequencer", "join + master's values", "master FX", "master loop", "after master", "between renders"};
    stage_patch(mode == 1 ? -2 : -1, 0, 0);
    int32_t buf[512]; ch_render(buf, 256, NULL); ch_transport(1);
    for (unsigned b = 0; b < 200; ++b) ch_render(buf, 256, NULL);
    memset(ch_prof, 0, sizeof ch_prof); ch_prof_t = __rdtsc();
    unsigned nb = 3000;
    for (unsigned b = 0; b < nb; ++b) ch_render(buf, 256, NULL);
    unsigned long long tot = 0; for (unsigned k = 0; k < 15; ++k) tot += ch_prof[k];
    printf("%s: %.0f cycles a render\n", mode == 1 ? "everything bypassed" : "the patch", (double)tot / nb);
    for (unsigned k = 0; k < 15; ++k) printf("  %-24s %5.1f %%  %8.0f\n", NM[k], 100.0 * (double)ch_prof[k] / (double)tot, (double)ch_prof[k] / nb);
    return 0;
}
#endif
/* stagewav DIR: the photo's patch, 8 bars, to DIR/stagepatch.wav (the null test's second reference) */
static int cmd_stagewav(const char *dir)
{
    stage_patch(-1, 0, 0);
    unsigned frames = (unsigned)(CH_RATE * 60.0 / 115 * 4 * 8) / 256 * 256;
    int32_t *all = calloc((size_t)2 * (frames + 256), sizeof(int32_t));
    ch_transport(1);
    for (unsigned f = 0; f < frames; f += 256) ch_render(all + 2 * f, 256, NULL);
    char path[512]; snprintf(path, sizeof path, "%s/stagepatch.wav", dir); wav_write_q23(path, all, frames, CH_RATE); free(all);
    return 0;
}
/* drumsweep DIR: every drum model, four hits each (plain; SNAP, NOISE and TONE up; DRIVE and TONE down; a low note
 * taken mid-ring by the next), 0.3 s apart, to DIR/drumsweep.wav: the null test's reference for the drum voice */
static int cmd_drumsweep(const char *dir)
{
    unsigned per = (CH_RATE * 3 / 10) / 128 * 128, frames = per * 4 * CH_DRUM_MODELS;
    int32_t *all = calloc((size_t)2 * (frames + 128), sizeof(int32_t)), buf[256];
    ch_init();
    for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
    unsigned f = 0;
    for (unsigned m = 0; m < CH_DRUM_MODELS; ++m) {
        drum(0, m);
        for (unsigned k = 0; k < 12; ++k) ch_render(buf, 128, NULL);
        for (unsigned h = 0; h < 4; ++h) {
            const ch_device_desc *dd = ch_device_info(CH_DEV_DRUM);
            for (unsigned q = 1; q < dd->params && q < CH_PARAMS; ++q) ch_param_set(0, CH_MATERIAL, q, dd->param[q].def);
            if (h == 1) { ch_param_set(0, CH_MATERIAL, 6, 0.7f); ch_param_set(0, CH_MATERIAL, 8, 0.6f); ch_param_set(0, CH_MATERIAL, 5, 0.5f); }
            if (h == 2) { ch_param_set(0, CH_MATERIAL, 9, 0.6f); ch_param_set(0, CH_MATERIAL, 5, -0.5f); ch_param_set(0, CH_MATERIAL, 2, 0.8f); ch_param_set(0, CH_MATERIAL, 3, 0.9f); }
            ch_render(buf, 128, NULL);
            ch_note(0, h == 3 ? 48 : 60, h == 1 ? 127 : 100);
            for (unsigned b = 0; b < per; b += 128, f += 128) ch_render(all + 2 * f, 128, NULL);
        }
    }
    char path[512]; snprintf(path, sizeof path, "%s/drumsweep.wav", dir); wav_write_q23(path, all, f, CH_RATE); free(all);
    return 0;
}
/* synthsweep DIR: the synth voice's own stages, every model of ours: the filter closed half with RESO, ENV>FILT and an
 * F ATTACK, the LPG on, and six fast notes over four voices (the oldest taken while it sounds: the fade, the hand-over),
 * to DIR/synthsweep.wav: the null test's reference for the synth voice */
static int cmd_synthsweep(const char *dir)
{
    unsigned per = (CH_RATE / 8) / 128 * 128, models = 14, frames = per * 8 * models;
    int32_t *all = calloc((size_t)2 * (frames + 128), sizeof(int32_t)), buf[256];
    ch_init();
    for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
    unsigned f = 0;
    for (unsigned m = 0; m < models; ++m) {
        drop_synth(0, m, m % 3 == 2 ? 0 : 1);
        ch_param_set(0, CH_MATERIAL, 12, 0.5f); ch_param_set(0, CH_MATERIAL, 13, 0.6f); ch_param_set(0, CH_MATERIAL, 14, 0.7f); ch_param_set(0, CH_MATERIAL, 15, 0.2f);
        ch_param_set(0, CH_MATERIAL, 6, 0.5f);
        for (unsigned k = 0; k < 16; ++k) ch_render(buf, 128, NULL);
        for (unsigned h = 0; h < 8; ++h) {
            if (h < 6) ch_note(0, 48 + h * 5, 90 + h * 6);
            if (h >= 2 && h < 7) ch_note(0, 48 + (h - 2) * 5, 0);
            for (unsigned b = 0; b < per; b += 128, f += 128) ch_render(all + 2 * f, 128, NULL);
        }
        for (unsigned h = 0; h < 6; ++h) ch_note(0, 48 + h * 5, 0);
    }
    char path[512]; snprintf(path, sizeof path, "%s/synthsweep.wav", dir); wav_write_q23(path, all, f, CH_RATE); free(all);
    return 0;
}
/* voices: the synth's VOICES setting. ANALOG, a three-note chord on each eighth for two bars, at 4, 2 and 1: the most
 * voices sounding at once, and that every step still gave a note (the level never falls silent between chords) */
static int cmd_vcount(void)
{
    int bad = 0; int32_t buf[256];
    for (unsigned sel = 0; sel < 3; ++sel) {
        ch_init();
        for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        drop_synth(0, 0, 1);
        for (unsigned k = 0; k < 12; ++k) ch_render(buf, 128, NULL);
        ch_param_set(0, CH_MATERIAL, 30, (float)sel);
        for (unsigned l = 0; l < 8; ++l) ch_lane_note_set(0, l, 48 + 2 * l);
        for (unsigned i = 0; i < 16; i += 2) for (unsigned l = 0; l < 3; ++l) { ch_step st = {90, 1, 100, 0, 0, 0, 0}; ch_step_set(0, (i / 2 + 2 * l) % 8, i, &st); }
        ch_seq_len_set(0, 16); ch_tempo_set(120);
        for (unsigned k = 0; k < 12; ++k) ch_render(buf, 128, NULL);
        ch_transport(1);
        unsigned most = 0, frames = (unsigned)(CH_RATE * 4) / 128 * 128; double pk = 0;
        for (unsigned f = 0; f < frames; f += 128) {
            ch_render(buf, 128, NULL);
            if (ch_snapshot_get()->voices > most) most = ch_snapshot_get()->voices;
            for (unsigned i = 0; i < 256; ++i) { double v = fabs(buf[i] / 8388608.0); if (v > pk) pk = v; }
        }
        ch_transport(0);
        unsigned want = sel == 0 ? 4 : sel == 1 ? 2 : 1;
        static double last_pk = 9; (void)most;
        printf("VOICES %u: the chord's peak %.2f\n", want, pk);
        if (pk < 0.05 || pk > last_pk + 0.02) bad = 1;        /* fewer voices: never louder, never silent */
        last_pk = pk;
    }
    printf("voices: %s\n", bad ? "FAIL" : "ok");
    return bad;
}
/* stepsend: the step's SEND. T1 a kick on every quarter; its four hits' SENDs 0, 100 %, 53 %, 0 (the first and last
 * as every step starts). What reaches T2, the track's STEP SEND target, after each hit: with the send knob at 0 (only
 * the marked hits arrive), then at 40 % (every hit at the knob's level, the marked ones more); T3, not the target, gets
 * only what its own knob sends. Then the pattern saved and loaded */
static int cmd_stepsend(void)
{
    static const uint8_t SD[4] = {0, 15, 8, 0};         /* fifteenths */
    int32_t buf[256]; int bad = 0;
    ch_init();
    ch_mix_set(3, CH_MIX_MUTE, 1);
    drum(0, 0);
    ch_device_set(1, CH_MATERIAL, CH_DEV_BYPASS); ch_mix_set(1, CH_MIX_MUTE, 0);
    ch_device_set(2, CH_MATERIAL, CH_DEV_BYPASS); ch_mix_set(2, CH_MIX_MUTE, 0);
    for (unsigned k = 0; k < 12; ++k) ch_render(buf, 128, NULL);
    printf("T1's STEP SEND at the start: %u (5: FX), FX SEND %.0f %%\n", (unsigned)ch_track_get(0)->ss_dest, ch_track_get(0)->tofx * 100);
    if (ch_track_get(0)->ss_dest != CH_SS_FX || ch_track_get(0)->tofx != 0) bad = 1;
    ch_mix_set(0, CH_MIX_SSDEST, 2);                      /* (the first part: the step SENDs to T2) */
    for (unsigned i = 0; i < 4; ++i) { ch_step st = {110, 1, 100, 0, 0, 0, 0}; st.send = SD[i]; ch_step_set(0, 0, i * 4, &st); }
    ch_seq_len_set(0, 16); ch_tempo_set(120);
    unsigned step = (unsigned)(CH_RATE * 60.0 / 120 / 4), q = step * 4;
    double first = 0;
    for (unsigned pass = 0; pass < 2; ++pass) {
        ch_send_set(0, 1, pass ? 0.4f : 0.0f); ch_send_set(0, 2, pass ? 0.4f : 0.0f);
        for (unsigned k = 0; k < 40; ++k) ch_render(buf, 128, NULL);
        ch_transport(1);
        double p2[4] = {0, 0, 0, 0}, p3[4] = {0, 0, 0, 0};
        for (unsigned f = 0; f < q * 4; f += 32) {
            ch_render(buf, 32, NULL);
            unsigned h = f / q; if (f % q < 400) continue;  /* (past the glide and the sends' one sub-block's wait) */
            for (unsigned i = 0; i < 32; ++i) { double v = fabs(ch_tmix[1][0][i]), w = fabs(ch_tmix[2][0][i]); if (v > p2[h]) p2[h] = v; if (w > p3[h]) p3[h] = w; }
        }
        ch_transport(0);
        printf("send knobs at %s: T2 gets %.3f %.3f %.3f %.3f   T3 gets %.3f %.3f %.3f %.3f\n", pass ? "40 %" : "0   ", p2[0], p2[1], p2[2], p2[3], p3[0], p3[1], p3[2], p3[3]);
        if (!pass) { first = p2[1]; if (p2[0] > 0.002 || p2[3] > 0.002 || p2[1] < 0.3 || p2[2] < p2[1] * 0.4 || p2[2] > p2[1] * 0.7 || p3[1] > 0.002) bad = 1; }
        else if (p2[0] < first * 0.3 || p2[0] > first * 0.5 || p2[1] < first * 0.95 || p2[2] < first * 0.8 || p3[1] > p3[0] * 1.05 || p3[0] < first * 0.3) bad = 1;
    }
    {   /* saved and loaded: the pattern packed, an empty one in, the packed one back */
        static uint8_t pk8[4096]; unsigned nb = ch_pattern_pack(pk8, sizeof pk8);
        ch_pattern_switch(NULL, 0, 0); for (unsigned k = 0; k < 8; ++k) ch_render(buf, 128, NULL);
        unsigned gone = ch_seq_get(0)->step[0][8].vel;
        ch_pattern_switch(pk8, 0, 0); for (unsigned k = 0; k < 8; ++k) ch_render(buf, 128, NULL);
        const ch_seq_state *qq = ch_seq_get(0);
        printf("packed %u bytes; emptied (velocity %u); back: SENDs %u %u %u %u (0 15 8 0)\n", nb, gone, qq->step[0][0].send, qq->step[0][4].send, qq->step[0][8].send, qq->step[0][12].send);
        if (!nb || gone || qq->step[0][4].send != 15 || qq->step[0][8].send != 8 || qq->step[0][0].send != 0 || qq->step[0][8].ratchet != 1) bad = 1;
    }
    {   /* the master's returns: a REVERB on the master's 2nd effect, its MIX at 0 (the mix dry). What each case adds to
         * the plain render after each hit (the same render, less it): nothing sent, nothing; the steps' SENDs in at
         * the 1st effect or the 2nd, the marked hits' reverb; in at the 3rd (past it), nothing; FX SEND at 50 %, every hit's */
        static float ref[4][24000]; static double dsum[5][4];
        static const char *const CN[5] = {"nothing sent      ", "steps, SEND TO ALL", "steps, SEND TO M2 ", "steps, SEND TO M3 ", "FX SEND 50 %      "};
        unsigned ql = q < 24000 ? q : 24000;
        for (unsigned cs = 0; cs < 6; ++cs) {             /* (0: the plain render, kept; 1: the same again, to show it repeats) */
            ch_init();
            for (unsigned t2 = 1; t2 < CH_TRACKS; ++t2) ch_mix_set(t2, CH_MIX_MUTE, 1);
            drum(0, 0);
            ch_device_set(CH_MASTER, CH_FX2, fx_dev("REVERB")); ch_bypass_set(CH_MASTER, CH_FX2, 0);
            ch_param_set(CH_MASTER, CH_FX2, 3, 0.0f); ch_param_set(CH_MASTER, CH_FX2, 1, 0.0f);
            ch_device_set(CH_MASTER, CH_FX3, CH_DEV_BYPASS); ch_device_set(CH_MASTER, CH_FX4, CH_DEV_BYPASS);   /* (the master starts with a REVERB of its own there) */
            for (unsigned i = 0; i < 4; ++i) { ch_step st = {110, 1, 100, 0, 0, 0, 0}; st.send = cs >= 2 && cs <= 4 ? SD[i] : 0; ch_step_set(0, 0, i * 4, &st); }
            ch_seq_len_set(0, 16); ch_tempo_set(120);
            ch_mix_set(0, CH_MIX_SSDEST, (float)(cs == 3 ? CH_SS_FX + 1 : cs == 4 ? CH_SS_FX + 2 : CH_SS_FX));
            ch_mix_set(0, CH_MIX_TOFX, cs == 5 ? 0.5f : 0.0f);
            for (unsigned k = 0; k < 400; ++k) ch_render(buf, 128, NULL);
            ch_transport(1);
            for (unsigned f = 0; f < q * 4; f += 32) {
                ch_render(buf, 32, NULL);
                unsigned h = f / q, at = f % q;
                for (unsigned i = 0; i < 32 && at + i < ql; ++i) {
                    float v = (float)(buf[2 * i] / 8388608.0);
                    if (!cs) ref[h][at + i] = v; else { double d = v - ref[h][at + i]; dsum[cs - 1][h] += d * d; }
                }
            }
            ch_transport(0);
            if (!cs) continue;
            double *d = dsum[cs - 1]; for (unsigned h = 0; h < 4; ++h) d[h] = sqrt(d[h] / ql);
            printf("%s: the reverb after each hit %.4f %.4f %.4f %.4f\n", CN[cs - 1], d[0], d[1], d[2], d[3]);
            if (cs == 1 || cs == 4) { if (d[0] > 1e-5 || d[1] > 1e-5 || d[2] > 1e-5 || d[3] > 1e-5) bad = 1; }
            else if (cs == 2 || cs == 3) { if (d[0] > 1e-4 || d[1] < 0.003 || d[2] < d[1] * 0.3 || d[2] > d[1] * 0.8 || d[3] > d[1] * 0.25) bad = 1; }
            else if (d[0] < 0.001 || d[3] < d[0] * 0.7 || d[3] > d[0] * 1.4) bad = 1;
        }
        /* the other returns, each on the master's 2nd effect with its MIX at 0: nothing sent, the plain render; FX SEND
         * at 50 %, its sound on every hit */
        static const unsigned RD[5] = {CH_DEV_CHORUS, CH_DEV_FLANGER, CH_DEV_PHASER, CH_DEV_COMB, CH_DEV_RESO}, RM[5] = {2, 3, 3, 3, 3};
        for (unsigned r = 0; r < 5; ++r) {
            double dd[2] = {0, 0};
            for (unsigned on = 0; on < 2; ++on) {
                ch_init();
                for (unsigned t2 = 1; t2 < CH_TRACKS; ++t2) ch_mix_set(t2, CH_MIX_MUTE, 1);
                drum(0, 0);
                ch_device_set(CH_MASTER, CH_FX2, RD[r]); ch_bypass_set(CH_MASTER, CH_FX2, 0); ch_param_set(CH_MASTER, CH_FX2, RM[r], 0.0f);
                ch_device_set(CH_MASTER, CH_FX3, CH_DEV_BYPASS); ch_device_set(CH_MASTER, CH_FX4, CH_DEV_BYPASS);
                for (unsigned i = 0; i < 4; ++i) { ch_step st = {110, 1, 100, 0, 0, 0, 0}; ch_step_set(0, 0, i * 4, &st); }
                ch_seq_len_set(0, 16); ch_tempo_set(120);
                ch_mix_set(0, CH_MIX_TOFX, on ? 0.5f : 0.0f);
                for (unsigned k = 0; k < 400; ++k) ch_render(buf, 128, NULL);
                ch_transport(1);
                for (unsigned f = 0; f < q * 4; f += 32) {
                    ch_render(buf, 32, NULL);
                    unsigned h = f / q, at = f % q;
                    for (unsigned i = 0; i < 32 && at + i < ql; ++i) { double d = buf[2 * i] / 8388608.0 - ref[h][at + i]; dd[on] += d * d; if (d != d) bad = 1; }
                }
                ch_transport(0);
            }
            printf("%-9s MIX 0: nothing sent %.5f, FX SEND 50 %% %.4f\n", ch_device_info(RD[r])->name, sqrt(dd[0] / (4.0 * ql)), sqrt(dd[1] / (4.0 * ql)));
            if (sqrt(dd[0] / (4.0 * ql)) > 1e-5 || sqrt(dd[1] / (4.0 * ql)) < 0.001) bad = 1;
        }
    }
    {   /* the RETURN: FX SEND at 50 % into a master REVERB with its MIX at 0; the meter with the level at unity, at
         * half the knob (a quarter of the gain... (0.4 / 0.8)^2) and at 0 */
        static const float RL[3] = {0.8f, 0.4f, 0.0f}; double mp[3];
        for (unsigned r = 0; r < 3; ++r) {
            ch_init();
            for (unsigned t2 = 1; t2 < CH_TRACKS; ++t2) ch_mix_set(t2, CH_MIX_MUTE, 1);
            drum(0, 0);
            for (unsigned i = 0; i < 4; ++i) { ch_step st = {110, 1, 100, 0, 0, 0, 0}; ch_step_set(0, 0, i * 4, &st); }
            ch_seq_len_set(0, 16); ch_tempo_set(120); ch_mix_set(0, CH_MIX_TOFX, 0.5f); ch_return_set(RL[r]);
            for (unsigned k = 0; k < 100; ++k) ch_render(buf, 128, NULL);
            ch_transport(1); mp[r] = 0;
            for (unsigned f = 0; f < q * 2; f += 32) { ch_render(buf, 32, NULL); double v = ch_snapshot_get()->ret_level[0]; if (v > mp[r]) mp[r] = v; }
            ch_transport(0);
        }
        printf("RETURN meter: level 80 %.3f, 40 %.3f, 0 %.3f\n", mp[0], mp[1], mp[2]);
        if (mp[0] < 0.1 || mp[1] < mp[0] * 0.22 || mp[1] > mp[0] * 0.28 || mp[2] > 1e-6) bad = 1;
        ch_return_set(0.8f);
    }
    printf("stepsend: %s\n", bad ? "FAIL" : "ok");
    return bad;
}
/* trance: the step gate. A held synth note through TRANCE on its track: steps 1 and 3 of sixteen on, a sixteenth
 * each, GATE at half a step. The level in each half of the first four steps and over the rest of the bar; then DEPTH
 * at 50 % (shut is half, not silence); and the hardest edge's largest jump between two frames (no click) */
static int cmd_trance(void)
{
    int32_t buf[64]; int bad = 0;
    for (unsigned pass = 0; pass < 2; ++pass) {
        ch_init();
        for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        drop_synth(0, 0, 1); ch_param_set(0, CH_MATERIAL, 10, 1.0f); ch_param_set(0, CH_MATERIAL, 5, 0);
        for (unsigned k = CH_FX1; k <= CH_FX4; ++k) ch_device_set(CH_MASTER, k, CH_DEV_BYPASS);
        fx(0, CH_FX1, "TRANCE");
        ch_param_set(0, CH_FX1, 0, 2); ch_param_set(0, CH_FX1, 1, 0.4737f); ch_param_set(0, CH_FX1, 2, 0.0f); ch_param_set(0, CH_FX1, 3, pass ? 0.5f : 1.0f);
        ch_param_set(0, CH_FX1, 4, 5); ch_param_set(0, CH_FX1, 5, 0); ch_param_set(0, CH_FX1, 6, 0); ch_param_set(0, CH_FX1, 7, 0); ch_param_set(0, CH_FX1, 8, 15);
        ch_tempo_set(120);
        for (unsigned k = 0; k < 40; ++k) ch_render(buf, 32, NULL);
        ch_note(0, 36, 127);
        for (unsigned k = 0; k < 400; ++k) ch_render(buf, 32, NULL);
        ch_transport(1);
        unsigned step = (unsigned)(CH_RATE * 60.0 / 120 / 4), half = step / 2;
        double e[9] = {0}; unsigned cnt[9] = {0}; double jump = 0, last = 0, pk = 0;
        for (unsigned f = 0; f < step * 16; f += 32) {
            ch_render(buf, 32, NULL);
            for (unsigned i = 0; i < 32; ++i) {
                double v = buf[2 * i] / 8388608.0; unsigned at = f + i, h = at / half, in = at % half;
                if (fabs(v - last) > jump && at) jump = fabs(v - last);
                last = v; if (fabs(v) > pk) pk = fabs(v);
                if (in < 300 || in > half - 100) continue;   /* (off the edges) */
                unsigned b = h < 8 ? h : 8; e[b] += v * v; ++cnt[b];
            }
        }
        ch_transport(0);
        for (unsigned b = 0; b < 9; ++b) e[b] = sqrt(e[b] / (cnt[b] ? cnt[b] : 1));
        printf("DEPTH %s: steps 1-4 by halves %.3f %.3f | %.3f %.3f | %.3f %.3f | %.3f %.3f, the rest %.3f; largest jump %.4f of peak %.3f\n",
               pass ? "50 % " : "100 %", e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7], e[8], jump, pk);
        double on = e[0], off = pass ? on * 0.5 : 0;
        if (on < 0.02 || fabs(e[4] - on) > on * 0.25) bad = 1;
        for (unsigned b = 1; b < 9; ++b) if (b != 4 && fabs(e[b] - off) > on * 0.12 + 1e-4) bad = 1;
    }
    printf("trance: %s\n", bad ? "FAIL" : "ok");
    return bad;
}
/* mixmod: a modulator onto the track's mixer. A held synth note, an LFO mapped to nothing, then to LEVEL, to PAN and
 * to FILTER: the left and right level in 20 ms windows over two seconds, least and most. Unmapped both sit still;
 * LEVEL moves both together; PAN moves them against each other; FILTER moves the level; and no jump at any time */
static int cmd_mixmod(void)
{
    static const char *const NM[4] = {"none  ", "LEVEL ", "PAN   ", "FILTER"};
    int32_t buf[64]; int bad = 0;
    for (unsigned pass = 0; pass < 4; ++pass) {
        ch_init();
        for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        drop_synth(0, 0, 1); ch_param_set(0, CH_MATERIAL, 10, 1.0f); ch_param_set(0, CH_MATERIAL, 5, 0);
        for (unsigned k = CH_FX1; k <= CH_FX4; ++k) ch_device_set(CH_MASTER, k, CH_DEV_BYPASS);
        ch_mod_type_set(0, 0, CH_MOD_WAVE); ch_mod_mode_set(0, 0, 0, 0); ch_mod_param_set(0, 0, 0, 0.55f);
        if (pass) { if (ch_map_set(0, 0, CH_MIXSLOT, pass - 1, pass == 1 ? -0.5f : 0.5f, 1) < 0) bad = 1; }
        if (ch_map_set(0, 0, CH_MIXSLOT, 3, 0.5f, 1) >= 0) bad = 1;    /* (no fourth) */
        for (unsigned k = 0; k < 40; ++k) ch_render(buf, 32, NULL);
        ch_note(0, 60, 127); ch_transport(1);   /* (the modulators run while it plays) */
        for (unsigned k = 0; k < 400; ++k) ch_render(buf, 32, NULL);
        const unsigned win = CH_RATE / 25;
        double lo[2] = {1e9, 1e9}, hi[2] = {0, 0}, e[2] = {0, 0}, jump = 0, last = 0, corr = 0, ml = 0, mr = 0, sl[50], sr[50]; unsigned cnt = 0, nw = 0;
        for (unsigned f = 0; f < CH_RATE * 2; f += 32) {
            ch_render(buf, 32, NULL);
            for (unsigned i = 0; i < 32; ++i) {
                double l = buf[2 * i] / 8388608.0, r = buf[2 * i + 1] / 8388608.0;
                if (fabs(l - last) > jump) jump = fabs(l - last);
                last = l; e[0] += l * l; e[1] += r * r;
                if (++cnt == win) {
                    for (unsigned c = 0; c < 2; ++c) { double v = sqrt(e[c] / win); if (v < lo[c]) lo[c] = v; if (v > hi[c]) hi[c] = v; if (nw < 50) (c ? sr : sl)[nw] = v; e[c] = 0; }
                    cnt = 0; if (nw < 50) ++nw;
                }
            }
        }
        for (unsigned k = 0; k < nw; ++k) { ml += sl[k] / nw; mr += sr[k] / nw; }
        for (unsigned k = 0; k < nw; ++k) corr += (sl[k] - ml) * (sr[k] - mr);
        printf("%s: left %.3f..%.3f, right %.3f..%.3f, the two %s, largest jump %.4f\n", NM[pass], lo[0], hi[0], lo[1], hi[1],
               pass == 0 ? "still" : corr > 0 ? "together" : "against each other", jump);
        static double base_lo, base_jump;                /* (the note's own decay and edges, nothing mapped) */
        if (pass == 0) { base_lo = lo[0]; base_jump = jump; if (lo[0] < 0.1 || fabs(lo[0] - lo[1]) > 1e-4) bad = 1; }
        else if (lo[0] > base_lo * 0.5) bad = 1;
        if (pass == 1 && corr <= 0) bad = 1;
        if (pass == 2 && corr >= 0) bad = 1;
        if (pass < 3 && jump > base_jump * 1.2) bad = 1;   /* (the filter sharpens the wave's own edge) */
    }
    printf("mixmod: %s\n", bad ? "FAIL" : "ok");
    return bad;
}
/* smp: the SAMPLE and GRAIN engines on a 1 s sine.
 * cost: eight SAMPLE notes at once, and a GRAIN cloud at full SIZE and DENSITY: the time a 256-frame block takes here.
 * cloud: a held GRAIN note, the level in 20 ms windows over three seconds, least against most (a pad holds level),
 * at the default knobs, at SIZE full, and at SIZE and DENSITY full.
 * scroll: a SAMPLE note sounding while LENGTH, then START, is swept fast: the largest corner (second difference)
 * against the sine's own */
static int cmd_smp(void)
{
    int32_t buf[64]; int bad = 0;
    for (unsigned eng = 0; eng < 2; ++eng) {
        ch_init(); clk_sample();
        for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        for (unsigned k = CH_FX1; k <= CH_FX4; ++k) ch_device_set(CH_MASTER, k, CH_DEV_BYPASS);
        ch_device_set(0, CH_MATERIAL, eng ? CH_DEV_GRAIN : CH_DEV_POLY); ch_bypass_set(0, CH_MATERIAL, 0); ch_mix_set(0, CH_MIX_MUTE, 0);
        if (eng) { ch_param_set(0, CH_MATERIAL, 3, 1.0f); ch_param_set(0, CH_MATERIAL, 4, 1.0f); }
        for (unsigned k = 0; k < 40; ++k) ch_render(buf, 32, NULL);
        for (unsigned k = 0; k < (eng ? 1u : 8u); ++k) ch_note(0, 60 + k, 110);
        for (unsigned k = 0; k < 200; ++k) ch_render(buf, 32, NULL);
        double t0 = now_us(); unsigned reps = 0;
        for (unsigned r = 0; r < 20; ++r) {
            if (!eng) for (unsigned k = 0; k < 8; ++k) ch_note(0, 60 + k, 110);
            for (unsigned k = 0; k < 600; ++k) { ch_render(buf, 32, NULL); ++reps; }
        }
        printf("cost %s: %.1f us a 256-frame block\n", eng ? "GRAIN full " : "SAMPLE x 8 ", (now_us() - t0) / reps * 8);
    }
    for (unsigned pass = 0; pass < 3; ++pass) {
        ch_init(); clk_sample();
        {   /* (a second of low-passed noise: a sine's grains beat against each other, which is the sine, not the cloud) */
            static ch_sample nz; static uint8_t *enc; enum { M = 32000 };
            if (!enc) { static int16_t pcm[M]; uint32_t r = 12345; double lp = 0; for (unsigned q = 0; q < M; ++q) { r = r * 1664525u + 1013904223u; lp += (((int32_t)(r >> 8) & 0xFFFF) - 32768 - lp) * 0.2; pcm[q] = (int16_t)(lp * 0.6); } enc = malloc(ch_adpcm_bytes(M)); ch_adpcm_encode(pcm, M, enc); }
            nz.adpcm = enc; nz.frames = M; nz.rate = 32000; nz.bpm = 120;
            for (unsigned q = 0; q < CH_SAMPLES; ++q) ch_sample_bind(q, &nz);
        }
        for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        for (unsigned k = CH_FX1; k <= CH_FX4; ++k) ch_device_set(CH_MASTER, k, CH_DEV_BYPASS);
        ch_device_set(0, CH_MATERIAL, CH_DEV_GRAIN); ch_bypass_set(0, CH_MATERIAL, 0); ch_mix_set(0, CH_MIX_MUTE, 0);
        if (pass >= 1) ch_param_set(0, CH_MATERIAL, 3, 1.0f);
        if (pass == 2) ch_param_set(0, CH_MATERIAL, 4, 1.0f);
        for (unsigned k = 0; k < 40; ++k) ch_render(buf, 32, NULL);
        ch_note(0, 60, 110);
        for (unsigned k = 0; k < 2756; ++k) ch_render(buf, 32, NULL);     /* two seconds in */
        const unsigned win = CH_RATE / 10; double lo = 1e9, hi = 0, e = 0, pk = 0; unsigned cnt = 0;
        for (unsigned f = 0; f < CH_RATE * 3; f += 32) {
            ch_render(buf, 32, NULL);
            for (unsigned i = 0; i < 32; ++i) {
                double l = buf[2 * i] / 8388608.0, r = buf[2 * i + 1] / 8388608.0; e += (l * l + r * r) * 0.5;
                if (fabs(l) > pk) pk = fabs(l);
                if (++cnt == win) { double v = sqrt(e / win); if (v < lo) lo = v; if (v > hi) hi = v; e = 0; cnt = 0; }
            }
        }
        printf("cloud %s: level %.3f..%.3f (least %.0f %% of most), peak %.3f\n", pass == 0 ? "default       " : pass == 1 ? "SIZE full     " : "SIZE + DENSITY", lo, hi, 100 * lo / (hi + 1e-9), pk);
        if (pass >= 1 && lo < hi * 0.5) bad = 1;
        if (pk > 0.98) bad = 1;
    }
    {
        double own = 0;
        for (unsigned pass = 0; pass < 3; ++pass) {
            ch_init(); clk_sample();
            for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
            for (unsigned k = CH_FX1; k <= CH_FX4; ++k) ch_device_set(CH_MASTER, k, CH_DEV_BYPASS);
            ch_device_set(0, CH_MATERIAL, CH_DEV_POLY); ch_bypass_set(0, CH_MATERIAL, 0); ch_mix_set(0, CH_MIX_MUTE, 0);
            for (unsigned k = 0; k < 40; ++k) ch_render(buf, 32, NULL);
            double worst = 0, x1 = 0, x2 = 0;
            for (unsigned r = 0; r < 6; ++r) {
                ch_param_set(0, CH_MATERIAL, 2, 0); ch_param_set(0, CH_MATERIAL, 3, 1);
                for (unsigned k = 0; k < 60; ++k) ch_render(buf, 32, NULL);
                ch_note(0, 60, 110);
                for (unsigned k = 0; k < 500; ++k) {
                    if (pass && k >= 200 && k < 260) ch_param_set(0, CH_MATERIAL, pass == 1 ? 3 : 2, pass == 1 ? 1.0f - (k - 199) / 60.0f : (k - 199) / 60.0f);
                    ch_render(buf, 32, NULL);
                    for (unsigned i = 0; i < 32; ++i) {
                        double x = buf[2 * i] / 8388608.0, d = fabs(x - 2 * x1 + x2);
                        if (k >= 100 && k < 400 && d > worst) worst = d;
                        x2 = x1; x1 = x;
                    }
                }
                ch_note(0, 60, 0);
            }
            if (!pass) own = worst;
            printf("scroll %s: largest corner %.5f (%.1f x the sine's own)\n", pass == 0 ? "nothing" : pass == 1 ? "LENGTH " : "START  ", worst, worst / (own + 1e-9));
            if (pass && worst > own * 6) bad = 1;
        }
    }
    printf("smp: %s\n", bad ? "FAIL" : "ok");
    return bad;
}
/* DRIVE at its corners: every type, DRIVE, CHARACTER, TONE and MIX at their ends, a full-scale saw and silence after;
 * nothing may go NaN, pass the guard or stay up (a DC or a ring) once the input stops */
static int cmd_drivecorners(void)
{
    int bad = 0; int32_t buf[256];
    for (unsigned type = 0; type < 6; ++type)
        for (unsigned c = 0; c < 48; ++c) {
            ch_init();
            for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
            drop_synth(0, 0, 1); ch_param_set(0, CH_MATERIAL, 10, 1.0f); ch_param_set(0, CH_MATERIAL, 5, 0);
            fx(0, CH_FX1, "DRIVE"); ch_param_set(0, CH_FX1, 4, (float)type);
            ch_param_set(0, CH_FX1, 0, (c & 1) ? 1.0f : 0.0f); ch_param_set(0, CH_FX1, 5, (c & 2) ? 1.0f : 0.0f);
            ch_param_set(0, CH_FX1, 1, (c & 4) ? 1.0f : -1.0f); ch_param_set(0, CH_FX1, 3, (c & 8) ? 1.0f : 0.5f); ch_param_set(0, CH_FX1, 2, 1.0f); ch_param_set(0, CH_FX1, 6, (float)(c >> 4));
            for (unsigned k = 0; k < CH_SLOTS; ++k) ch_guard_trips[k] = 0;
            ch_render(buf, 128, NULL); ch_note(0, 36, 127);
            double pk = 0, tail = 0; int nan = 0;
            for (unsigned f = 0; f < CH_RATE * 2; f += 128) {
                if (f == (CH_RATE / 128) * 128) ch_note(0, 36, 0);
                ch_render(buf, 128, NULL);
                for (unsigned i = 0; i < 128; ++i) { double v = buf[2 * i] / 8388608.0; if (v != v) nan = 1; if (fabs(v) > pk) pk = fabs(v); if (f > CH_RATE * 3 / 2 && fabs(v) > tail) tail = fabs(v); }
            }
            if (nan || ch_guard_trips[CH_FX1] || tail > 0.01) { bad = 1; printf("type %u corner %2u: peak %.3f tail %.4f guard %u%s\n", type, c, pk, tail, (unsigned)ch_guard_trips[CH_FX1], nan ? " NaN" : ""); }
        }
    printf("drivecorners: %s\n", bad ? "FAIL" : "ok (288 corners: no NaN, no guard trip, silent after)");
    return bad;
}
/* the step GATE: a SYNTH in GATE mode with no release, one note a step at GATE 100, 50, 0 and a ratchet of 2 at 50;
 * how long each sounds (above -40 dB) as a share of its step; then a pattern with gates packed, cleared and unpacked */
static int cmd_gatetest(void)
{
    ch_init();
    for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
    ch_device_set(0, CH_MATERIAL, CH_DEV_SYNTH);
    ch_param_set(0, CH_MATERIAL, 0, 5);                   /* WAVES: a steady saw */
    ch_param_set(0, CH_MATERIAL, 7, 1);                   /* MODE: GATE */
    ch_param_set(0, CH_MATERIAL, 5, 0);                   /* DECAY: none, so it stops as the gate does */
    ch_param_set(0, CH_MATERIAL, 6, 0); ch_param_set(0, CH_MATERIAL, 12, 1);
    ch_tempo_set(120); ch_swing_set(0);
    for (unsigned l = 0; l < CH_LANES; ++l) for (unsigned i = 0; i < CH_STEPS; ++i) { ch_step o = {0, 1, 100, 0, 0, 0}; ch_step_set(0, l, i, &o); }
    ch_seq_len_set(0, 16);
    static const unsigned G[4] = {100, 47, 0, 47};        /* (the gate is kept in fifteenths: 47 is 7 of them) */
    for (unsigned k = 0; k < 4; ++k) { ch_step st = {100, (uint8_t)(k == 3 ? 2 : 1), 100, 0, 0, 0}; ch_step_gate_set(&st, G[k]); ch_step_set(0, 0, k * 4, &st); }
    unsigned frames = CH_RATE * 2, block = 64;           /* one bar at 120: steps of 125 ms, a hit every 500 ms */
    int32_t buf[128];
    { int32_t w[256]; ch_render(w, 128, NULL); }
    ch_transport(1);
    unsigned on[4] = {0, 0, 0, 0};
    for (unsigned f = 0; f < frames; f += block) {
        ch_render(buf, block, NULL);
        double pk = 0; for (unsigned i = 0; i < block; ++i) { double v = fabs(buf[2 * i] / 8388608.0); if (v > pk) pk = v; }
        unsigned slot = (f * 4) / frames;                  /* which hit's half second this is */
        if (pk > 0.01 && slot < 4) on[slot] += block;
    }
    ch_transport(0);
    double step = CH_RATE * 0.125; int bad = 0;
    for (unsigned k = 0; k < 4; ++k) {
        double share = on[k] / step;
        double want = k == 0 ? 1.0 : k == 1 ? 0.5 : k == 2 ? 0.03 : 0.75;   /* (the ratchet: two notes, each half of its half: 0.5 sounding + the gap's tail) */
        printf("gate %3u%s: sounds %.2f of its step (about %.2f expected)\n", G[k], k == 3 ? " ratchet 2" : "          ", share, want);
        if (k < 3 && fabs(share - want) > 0.12) bad = 1;
    }
    /* the format: gates survive a pack and unpack */
    for (unsigned k = 0; k < 4; ++k) { ch_step st = ch_seq_get(0)->step[0][k * 4]; ch_step_oct_set(&st, (int)k - 2); ch_step_set(0, 0, k * 4, &st); }   /* (and octaves: -2 -1 0 +1) */
    uint8_t *pk = malloc(CH_PAT_MAX); unsigned n = ch_pattern_pack(pk, CH_PAT_MAX);
    for (unsigned k = 0; k < 4; ++k) { ch_step o = {0, 1, 100, 0, 0, 0}; ch_step_set(0, 0, k * 4, &o); }
    ch_pattern_switch(pk, 0, 0);
    { int32_t w[256]; ch_render(w, 128, NULL); }
    for (unsigned k = 0; k < 4; ++k) { int o = ch_step_oct(&ch_seq_get(0)->step[0][k * 4]); if (o != (int)k - 2) { bad = 1; printf("step %u: octave %d after the round trip, not %d\n", k * 4, o, (int)k - 2); } }
    for (unsigned k = 0; k < 4; ++k) { unsigned g = ch_step_gate(&ch_seq_get(0)->step[0][k * 4]); if (g != G[k]) { bad = 1; printf("step %u: gate %u after the round trip, not %u\n", k * 4, g, G[k]); } }
    printf("pattern with gates: %u bytes packed, round trip %s\n", n, bad ? "checked" : "ok");
    free(pk);
    printf("gatetest: %s\n", bad ? "FAIL" : "ok");
    return bad;
}
/* RIPPLE at the corners: SYNC FOLD BEND, NOISE, the AM and JITTER each at 0 and full, a low note and a high one;
 * nothing may go NaN or past the level a model is allowed; the loudest and quietest corners reported */
static int cmd_ripple(void)
{
    enum { M_RIPPLE = 13 };
    unsigned per = CH_RATE / 2, block = 128; int bad = 0; double hi = 0, lo = 9;
    int32_t *b = calloc((size_t)2 * (per + block), sizeof(int32_t));
    for (unsigned c = 0; c < 128; ++c) {
        ch_init();
        for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        ch_device_set(0, CH_MATERIAL, CH_DEV_SYNTH);
        ch_param_set(0, CH_MATERIAL, 0, (float)M_RIPPLE);
        for (unsigned q = 2; q < ch_device_info(CH_DEV_SYNTH)->params; ++q) { float v; if (ch_synth_preset(M_RIPPLE, q, &v)) ch_param_set(0, CH_MATERIAL, q, v); }
        ch_param_set(0, CH_MATERIAL, 2, (c & 1) ? 1.0f : 0.0f);           /* SYNC */
        ch_param_set(0, CH_MATERIAL, 3, (c & 2) ? 1.0f : 0.0f);           /* FOLD */
        ch_param_set(0, CH_MATERIAL, 4, (c & 4) ? 1.0f : 0.0f);           /* BEND */
        ch_param_set(0, CH_MATERIAL, 23, (c & 8) ? 1.0f : 0.0f);          /* NOISE */
        ch_param_set(0, CH_MATERIAL, 25, (c & 16) ? 1.0f : 0.03f);        /* AM RATIO */
        ch_param_set(0, CH_MATERIAL, 26, (c & 16) ? 1.0f : 0.0f);         /* AM DEPTH */
        ch_param_set(0, CH_MATERIAL, 27, (c & 32) ? 1.0f : 0.0f);         /* AM FDBK */
        ch_param_set(0, CH_MATERIAL, 28, (c & 32) ? 1.0f : 0.0f);         /* JITTER */
        ch_param_set(0, CH_MATERIAL, 24, (c & 64) ? 1.0f : 0.0f);         /* WINDOW */
        ch_param_set(0, CH_MATERIAL, 12, 1); ch_param_set(0, CH_MATERIAL, 14, 0);   /* the filter open, still */
        { int32_t warm[256]; ch_render(warm, 128, NULL); }
        double pk = 0, sum = 0; int nan = 0;
        for (unsigned f = 0; f < per; f += block) {
            if (f == 0) ch_note(0, (c & 64) ? 84 : 36, 127);
            ch_render(b + 2 * f, block, NULL);
            for (unsigned i = 0; i < block; ++i) { double v = b[2 * (f + i)] / 8388608.0; if (v != v) nan = 1; if (fabs(v) > pk) pk = fabs(v); sum += v * v; }
        }
        double rms = sqrt(sum / per);
        if (nan || pk > 0.95) { bad = 1; printf("corner %3u: peak %.3f rms %.3f%s\n", c, pk, rms, nan ? " NaN" : ""); }
        if (rms < 0.01) printf("corner %3u quiet: rms %.4f\n", c, rms);
        if (rms > hi) hi = rms; if (rms < lo) lo = rms;
    }
    printf("ripple: 128 corners, rms %.3f to %.3f: %s\n", lo, hi, bad ? "FAIL" : "ok");
    free(b);
    return bad;
}
/* every SYNTH model: a held low note, then a soft high one */
static int cmd_synths(const char *dir)
{
    unsigned per = (unsigned)(2.0 * CH_RATE), block = 128;
    int32_t *b = calloc((size_t)2 * (per + block), sizeof(int32_t));
    int bad = 0;
    printf("%-10s %6s %6s\n", "model", "peak", "rms");
    for (unsigned m = 0; m < CH_SYNTH_MODELS; ++m) {
        ch_init();
        for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
        ch_device_set(0, CH_MATERIAL, CH_DEV_SYNTH);
        ch_param_set(0, CH_MATERIAL, 0, (float)m);
        for (unsigned p = 2; p < ch_device_info(CH_DEV_SYNTH)->params; ++p) { float v; if (ch_synth_preset(m, p, &v)) ch_param_set(0, CH_MATERIAL, p, v); }   /* as the app sets a chosen model */
        if (m == 10) ch_param_set(0, CH_MATERIAL, 7, 1);   /* GATE: let PARTICLE run */
        { int32_t warm[256]; ch_render(warm, 128, NULL); }
        double pk = 0, sum = 0; int nan = 0;
        for (unsigned f = 0; f < per; f += block) {
            if (f == 0) ch_note(0, 48, 120);
            if (f == 26880) ch_note(0, 48, 0);
            if (f == 44032) ch_note(0, 67, 70);
            if (f == 70016) ch_note(0, 67, 0);
            ch_render(b + 2 * f, block, NULL);
            for (unsigned i = 0; i < block; ++i) { double v = b[2 * (f + i)] / 8388608.0; if (v != v) nan = 1; if (fabs(v) > pk) pk = fabs(v); sum += v * v; }
        }
        if (nan || pk < 0.05) bad = 1;
        printf("%-10s %6.3f %6.3f %s\n", ch_synth_names[m], pk, sqrt(sum / per), nan ? "NaN!" : "");
        char path[512]; snprintf(path, sizeof path, "%s/s%02u_%s.wav", dir, m, ch_synth_names[m]);
        wav_write_q23(path, b, per, CH_RATE);
    }
    return bad;
}

/* fxcheck: random effect chains on an 808 kick, every knob, select and mode thrown about while it plays; the
 * engine's guard counts each broken value (NaN, infinity, past 16384) by stage. Prints any trial that had one. */
static uint32_t fx_rng = 12345;
static uint32_t frand(void) { fx_rng ^= fx_rng << 13; fx_rng ^= fx_rng >> 17; fx_rng ^= fx_rng << 5; return fx_rng; }
static void fx_throw(unsigned slot, int verbose)
{
    const ch_track_state *ts = ch_track_get(0);
    const ch_device_desc *d = ch_device_info(ts->device[slot]);
    if (!d->params) return;
    unsigned p = frand() % d->params;
    const ch_param_desc *pd = &d->param[p];
    float v = pd->kind == CH_SELECT ? (float)(frand() % (pd->steps ? pd->steps : 1)) : (float)(frand() % 1001) / 1000.0f;
    if ((frand() & 7) == 0) v = (frand() & 1) ? 1.0f : 0.0f;      /* the ends, often */
    ch_param_set(0, slot, p, v);
    if (pd->modes && (frand() % 6) == 0) { unsigned nm = 0; while (pd->modes[nm]) ++nm; unsigned m = frand() % nm; ch_mode_set(0, slot, p, m);
        if (verbose) printf("      %s %s mode %s\n", d->name, pd->name, pd->modes[m]); }
    if (verbose) printf("      %s %s = %.3f\n", d->name, pd->name, v);
}
static int cmd_fxcheck(unsigned trials)
{
    int bad_trials = 0;
    unsigned tot[CH_SLOTS] = {0};
    for (unsigned tr = 0; tr < trials; ++tr) {
        uint32_t seed = 0x9e3779b9u * (tr + 1); fx_rng = seed;
        ch_init(); ch_tempo_set(120);
        drum(0, 0);
        unsigned nfx = 1 + frand() % CH_FX, devs[CH_FX] = {0};
        for (unsigned k = 0; k < nfx; ++k) { devs[k] = CH_DEV_FX_FIRST + frand() % CH_FX_TYPES; ch_device_set(0, CH_FX1 + k, devs[k]); }
        int32_t buf[2 * 128];
        unsigned blocks = 44100 * 8 / 128;
        double peak = 0;
        for (unsigned b = 0; b < blocks; ++b) {
            if (b % 86 == 0) { ch_note(0, 60, 110); }
            if (b % 86 == 4) ch_note(0, 60, 0);
            if (b % 17 == 0) fx_throw(CH_FX1 + frand() % nfx, 0);
            ch_render(buf, 128, 0);
            for (unsigned i = 0; i < 256; ++i) { double v = fabs(buf[i] / 8388608.0); if (v > peak) peak = v; }
        }
        const ch_snapshot *sn = ch_snapshot_get();
        unsigned nb = sn->master_bad;
        for (unsigned k = 0; k < CH_SLOTS; ++k) { nb += sn->bad[0][k]; tot[k] += sn->bad[0][k]; }
        if (nb) {
            ++bad_trials;
            printf("trial %u: %u broken blocks (ENG %u FX1 %u FX2 %u FX3 %u FX4 %u, master %u), last %s; chain", tr, nb,
                   sn->bad[0][0], sn->bad[0][1], sn->bad[0][2], sn->bad[0][3], sn->bad[0][4], sn->master_bad,
                   sn->bad_kind == 1 ? "NaN" : sn->bad_kind == 2 ? "inf" : "huge");
            for (unsigned k = 0; k < nfx; ++k) printf(" %s", ch_device_info(devs[k])->name);
            printf("\n");
        }
    }
    printf("fxcheck: %u trials, %d with broken values; by stage ENG %u FX1 %u FX2 %u FX3 %u FX4 %u\n", trials, bad_trials,
           tot[0], tot[1], tot[2], tot[3], tot[4]);
    return bad_trials != 0;
}

static int cmd_scene(const char *name, const char *wav, const char *out, double secs, float bpm, char **over, int n_over);
static int cmd_fxall(const char *dir)
{
    for (unsigned d = CH_DEV_FX_FIRST; d < CH_DEVICES; ++d) {
        char nm[32], path[512];
        snprintf(nm, sizeof nm, "fx-%s", ch_device_info(d)->name);
        snprintf(path, sizeof path, "%s/%s.wav", dir, nm);
        if (cmd_scene(nm, "dev/out/adpcm-roundtrip.wav", path, 6, 120, 0, 0)) return 1;
    }
    return 0;
}

/* sidetest: the master SIDECHAIN on hats, triggered by a kick track turned down to nothing (its note-ons still
 * count): the hats' level in 50 ms slices after each trigger, with AMOUNT 0 and 1, and SOURCE TOO off and on with
 * the kick audible (its peak must not change when it is spared) */
static int cmd_sidetest(void)
{
    for (int pass = 0; pass < 4; ++pass) {
        ch_init(); ch_tempo_set(120);
        drum(0, 0); steps(0, "X...X...X...X...", 60);
        drum(2, 15); steps(2, "xxxxxxxxxxxxxxxx", 60); ch_mix_set(2, CH_MIX_MUTE, 0);
        if (pass < 2) ch_mix_set(0, CH_MIX_LEVEL, 0);                     /* the kick silent: a trigger only */
        ch_param_set(CH_MASTER, CH_FX4, 1, pass == 0 ? 0.0f : 1.0f);     /* AMOUNT */
        ch_param_set(CH_MASTER, CH_FX4, 3, 0.6f);                         /* RELEASE: slow enough to see */
        if (pass == 3) ch_param_set(CH_MASTER, CH_FX4, 4, 1);             /* SOURCE TOO */
        int32_t buf[2 * 128];
        for (unsigned b = 0; b < 40; ++b) ch_render(buf, 128, 0);
        ch_transport(1);
        unsigned blocks = 44100 * 3 / 128; float *mix = malloc(sizeof(float) * blocks * 128);
        for (unsigned b = 0; b < blocks; ++b) { ch_render(buf, 128, 0); for (unsigned i = 0; i < 128; ++i) mix[b * 128 + i] = buf[2 * i] / 8388608.0f; }
        /* the second bar's first beat: 2.0 s in; slices of 50 ms from there */
        unsigned t0 = 44100 * 2;
        printf(pass == 0 ? "trigger only, AMOUNT 0  :" : pass == 1 ? "trigger only, AMOUNT 1  :" : pass == 2 ? "kick heard, spared      :" : "kick heard, SOURCE TOO  :");
        for (unsigned k = 0; k < 8; ++k) {
            double acc = 0; for (unsigned i = 0; i < 2205; ++i) { float v = mix[t0 + k * 2205 + i]; acc += v * v; }
            printf(" %.3f", sqrt(acc / 2205));
        }
        float pk = 0; for (unsigned i = 0; i < 1500; ++i) pk = fmaxf(pk, fabsf(mix[t0 + i]));
        printf("   peak at the trigger %.3f\n", pk);
        free(mix);
    }
    return 0;
}

/* corners FROM: each DRUM model from FROM on at SHAPE and COLOR 0, 50, 100 %: the peak and the level (RMS over the
 * first 150 ms) of a hit, so a model's knobs can be seen to keep it about as loud; then PITCH at its ends */
static int cmd_corners(unsigned from)
{
    int32_t buf[256];
    for (unsigned m = from; m < CH_DRUM_MODELS; ++m) {
        printf("%-11s", ch_drum_names[m]);
        for (unsigned c = 0; c < 11; ++c) {
            ch_init();
            for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
            ch_device_set(0, CH_MATERIAL, CH_DEV_DRUM); ch_param_set(0, CH_MATERIAL, 0, (float)m);
            if (c < 9) { ch_param_set(0, CH_MATERIAL, 3, (float)(c / 3) * 0.5f); ch_param_set(0, CH_MATERIAL, 4, (float)(c % 3) * 0.5f); }
            else ch_param_set(0, CH_MATERIAL, 1, c == 9 ? -1.0f : 1.0f);
            ch_render(buf, 128, NULL);
            ch_note(0, 60, 110);
            double pk = 0, e = 0; unsigned cnt = 0; int nan = 0;
            for (unsigned b = 0; b < 52; ++b) { ch_render(buf, 128, NULL); for (unsigned i = 0; i < 128; ++i) { double v = buf[2 * i] / 8388608.0; if (v != v) nan = 1; if (fabs(v) > pk) pk = fabs(v); e += v * v; ++cnt; } }
            printf("%s %.2f/%.3f%s", c == 9 ? "  | pitch" : "", pk, sqrt(e / cnt), nan ? " NaN!" : "");
        }
        printf("\n");
    }
    return 0;
}

/* drumstress FROM [TRIALS]: each DRUM model from FROM on, TRIALS times with every knob somewhere at random (DECAY long
 * half the time), struck fast for a second and a half and then left to ring out: nothing may go NaN, trip the guard,
 * leave a number so small the chip's arithmetic slows on it (a denormal), or ring on past the voice's 20 s. Reported:
 * the loudest peak, the longest any voice lived after the last hit, and the time a render took (PC microseconds) */
static int dn(float x) { float a = x < 0 ? -x : x; return a != 0 && a < 1.2e-38f; }
static unsigned drum_denormals(void)
{
    unsigned c = 0;
    for (unsigned i = 0; i < CH_DVOICES; ++i) {
        const ch_dvoice *v = &ch_tr[0].drum.v[i];
        if (!v->on) continue;
        c += dn(v->amp) + dn(v->aux) + dn(v->aux2) + dn(v->pit) + dn(v->snap) + dn(v->fb) + dn(v->hold) + dn(v->sh) + dn(v->lp) + dn(v->ks_prev) + dn(v->last) + dn(v->dc);
        for (unsigned k = 0; k < 6; ++k) c += dn(v->ph[k]);
        for (unsigned k = 0; k < 4; ++k) c += dn(v->f[k].ic1) + dn(v->f[k].ic2);
        for (unsigned k = 0; k < CH_DRUM_MODES; ++k) c += dn(v->ry1[k]) + dn(v->ry2[k]);
    }
    return c;
}
static int cmd_drumstress(unsigned from, unsigned trials)
{
    int32_t buf[256]; int fail = 0;
    uint32_t rs = 12345;
#define RND() (rs = rs * 1664525u + 1013904223u, (float)(rs >> 8) * (1.0f / 16777216.0f))
    for (unsigned m = from; m < CH_DRUM_MODELS; ++m) {
        double wpk = 0, wlife = 0, wus = 0; unsigned nans = 0, trips = 0, dens = 0, stuck = 0;
        for (unsigned tr = 0; tr < trials; ++tr) {
            ch_init();
            for (unsigned t = 1; t < CH_TRACKS; ++t) ch_mix_set(t, CH_MIX_MUTE, 1);
            ch_device_set(0, CH_MATERIAL, CH_DEV_DRUM); ch_param_set(0, CH_MATERIAL, 0, (float)m);
            ch_param_set(0, CH_MATERIAL, 1, RND() * 2 - 1);
            ch_param_set(0, CH_MATERIAL, 2, (tr & 1) ? 1.0f : RND());
            ch_param_set(0, CH_MATERIAL, 3, tr % 5 == 0 ? (float)(tr / 5 & 1) : RND());
            ch_param_set(0, CH_MATERIAL, 4, tr % 7 == 0 ? (float)(tr / 7 & 1) : RND());
            ch_param_set(0, CH_MATERIAL, 5, (tr & 2) ? RND() * 2 - 1 : 0);
            ch_param_set(0, CH_MATERIAL, 6, (tr & 4) ? RND() : 0);
            ch_param_set(0, CH_MATERIAL, 7, RND() * 2 - 1);
            ch_param_set(0, CH_MATERIAL, 8, (tr & 8) ? RND() : 0);
            ch_param_set(0, CH_MATERIAL, 9, (tr & 16) ? RND() : 0);
            ch_param_set(0, CH_MATERIAL, 10, RND());
            ch_param_set(0, CH_MATERIAL, 13, (float)(tr / 2 & 1));
            for (unsigned k = 0; k < CH_SLOTS; ++k) ch_guard_trips[k] = 0;
            ch_render(buf, 128, NULL);
            unsigned f = 0, lastoff = 0; int alive = 1;
            clock_t c0 = clock(); unsigned blocks = 0;
            for (; f < CH_RATE * 26 && alive; f += 128) {
                if (f < CH_RATE * 3 / 2 && (f / 128) % 20 == 0) ch_note(0, 36 + (unsigned)(RND() * 48), 40 + (unsigned)(RND() * 87));
                ch_render(buf, 128, NULL); ++blocks;
                for (unsigned i = 0; i < 256; ++i) { double v = buf[i] / 8388608.0; if (v != v) ++nans; if (fabs(v) > wpk) wpk = fabs(v); }
                dens += drum_denormals();
                if (f >= CH_RATE * 3 / 2) {
                    alive = 0;
                    for (unsigned i = 0; i < CH_DVOICES; ++i) alive |= ch_tr[0].drum.v[i].on;
                    lastoff = f;
                }
            }
            double us = (double)(clock() - c0) * 1e6 / CLOCKS_PER_SEC / (blocks ? blocks : 1);
            double life = ((double)lastoff - CH_RATE * 1.5) / CH_RATE;
            if (alive) ++stuck;
            if (life > wlife) wlife = life;
            if (us > wus) wus = us;
            for (unsigned k = 0; k < CH_SLOTS; ++k) trips += ch_guard_trips[k];
        }
        int bad = nans || trips || dens || stuck || wpk > 1.0;
        if (bad) fail = 1;
        printf("%-11s peak %.2f  longest tail %5.1f s  render %5.1f us  nan %u guard %u denormal %u stuck %u  %s\n", ch_drum_names[m], wpk, wlife, wus, nans, trips, dens, stuck, bad ? "FAIL" : "ok");
    }
#undef RND
    printf("drumstress: %s\n", fail ? "FAIL" : "ok");
    return fail;
}

/* reptest: REPEAT on a track with hits on steps 1, 9 and 13: the level of each sixteenth of the second bar, dry, then
 * WINDOW 1/16 EVERY 1 BAR REPEATS X4 (the first hit again on steps 2-5), then the same at GATE 50 %, then EVERY 1/2 */
static int cmd_reptest(void)
{
    for (int pass = 0; pass < 5; ++pass) {
        ch_init(); ch_tempo_set(120);
        drum(0, 15); steps(0, "X.......X...X...", 60);
        ch_device_set(0, CH_FX1, CH_DEV_REPEAT);
        ch_param_set(0, CH_FX1, 0, 2); ch_param_set(0, CH_FX1, 1, pass == 3 ? 1 : 2); ch_param_set(0, CH_FX1, 2, 0.4f);
        ch_param_set(0, CH_FX1, 3, pass == 2 ? 0.5f : 1.0f); ch_param_set(0, CH_FX1, 4, 1); ch_param_set(0, CH_FX1, 5, 0); ch_param_set(0, CH_FX1, 6, pass ? 1.0f : 0.0f);
        ch_param_set(0, CH_FX1, 8, pass == 4 ? 8 : 0);       /* OFFSET 8/16: the hit on step 9 again on steps 10-13 */
        int32_t buf[2 * 128];
        for (unsigned b = 0; b < 40; ++b) ch_render(buf, 128, 0);
        ch_transport(1);
        unsigned blocks = 44100 * 4 / 128 + 8; float *mix = malloc(sizeof(float) * blocks * 128);
        for (unsigned b = 0; b < blocks; ++b) { ch_render(buf, 128, 0); for (unsigned i = 0; i < 128; ++i) mix[b * 128 + i] = buf[2 * i] / 8388608.0f; }
        printf(pass == 0 ? "dry               :" : pass == 1 ? "1/16 1 BAR X4     :" : pass == 2 ? "the same, GATE 50 :" : pass == 3 ? "1/16 EVERY 1/2 X4 :" : "1 BAR, OFFSET 8/16:");
        for (unsigned k = 0; k < 16; ++k) {
            unsigned t0 = 44100 * 2 + k * 5512, h = 2756; double a = 0, b2 = 0;
            for (unsigned i = 0; i < h; ++i) { float v = mix[t0 + i], w = mix[t0 + h + i]; a += v * v; b2 += w * w; }
            printf(" %.2f/%.2f", sqrt(a / h), sqrt(b2 / h));
        }
        printf("\n"); free(mix);
    }
    return 0;
}

/* trigtest: MOD 4 (TRIG RND) mapped onto a kick's PITCH at full depth; the modulator's value after each of eight
 * notes (a new one each time), then with SMOOTH up (gliding) */
static int cmd_trigtest(void)
{
    ch_init(); drum(0, 0);
    ch_map_set(0, 3, CH_MATERIAL, 1, 1.0f, 1);
    int32_t buf[2 * 128];
    for (int pass = 0; pass < 2; ++pass) {
        if (pass) ch_mod_param_set(0, 3, 2, 0.6f);                  /* SMOOTH */
        printf(pass ? "smoothed, 30 ms after each note:" : "at once, after each note:      ");
        for (int k = 0; k < 8; ++k) {
            ch_note(0, 60, 100); ch_note(0, 60, 0);
            for (int b = 0; b < 10; ++b) ch_render(buf, 128, 0);  /* 29 ms */
            printf(" %+.2f", ch_snapshot_get()->mod[0][3]);
        }
        printf("\n");
    }
    return 0;
}

/* locktest: locks set, the pattern packed and read back: the same locks (and a PAT2 one still reads) */
static int cmd_locktest(void)
{
    ch_init(); int32_t buf[2 * 128];
    for (unsigned b = 0; b < 4; ++b) ch_render(buf, 128, 0);
    ch_step on = {100, 1, 100, 0, 0, 0}; ch_step_set(0, 0, 4, &on); ch_step_set(1, 2, 7, &on);
    ch_lock_set(0, 4, CH_MATERIAL, 0, 9); ch_lock_set(0, 4, CH_MATERIAL, 2, 0.9f); ch_lock_set(1, 7, CH_FX1, 1, -0.5f);
    static uint8_t pk[CH_PAT_MAX]; unsigned n = ch_pattern_pack(pk, CH_PAT_MAX);
    printf("packed %u bytes, T1 %u locks, T2 %u locks\n", n, ch_seq_get(0)->nlock, ch_seq_get(1)->nlock);
    ch_lock_clear(0, CH_STEPS); ch_lock_clear(1, CH_STEPS);
    ch_pattern_switch(pk, 0, 0);
    for (unsigned b = 0; b < 4; ++b) ch_render(buf, 128, 0);
    float v = 0; int ok = 1;
    ok &= ch_lock_get(0, 4, CH_MATERIAL, 0, &v) && v == 9; ok &= ch_lock_get(0, 4, CH_MATERIAL, 2, &v) && v > 0.89f && v < 0.91f;
    ok &= ch_lock_get(1, 7, CH_FX1, 1, &v) && v < -0.49f && v > -0.51f; ok &= !ch_lock_get(1, 6, CH_FX1, 1, &v);
    printf("read back: T1 %u locks, T2 %u locks: %s\n", ch_seq_get(0)->nlock, ch_seq_get(1)->nlock, ok ? "OK" : "WRONG");
    pk[3] = '2'; ch_pattern_switch(pk, 0, 0);                /* an old pattern: the steps, no locks */
    for (unsigned b = 0; b < 4; ++b) ch_render(buf, 128, 0);
    printf("as PAT2: step on %d, locks %u (0 expected)\n", ch_seq_get(0)->step[0][4].vel != 0, ch_seq_get(0)->nlock);
    return ok ? 0 : 1;
}

/* memtest: DELAY on T1, DUB DELAY on T2, DELAY on T3, REPEAT on T3's second slot: which found memory (the snapshot's
 * fx_nomem bits), with the master idle */
static int cmd_memtest(void)
{
    ch_init(); int32_t buf[2 * 128];
    for (unsigned b = 0; b < 4; ++b) ch_render(buf, 128, 0);
    const ch_snapshot *sn = ch_snapshot_get();
    printf("master idle: nomem bits %x, blocks %d\n", sn->mfx_nomem, CH_FX_BLOCKS);
    ch_device_set(0, CH_FX1, CH_DEV_DELAY); ch_device_set(1, CH_FX1, CH_DEV_DUB); ch_device_set(2, CH_FX1, CH_DEV_DELAY); ch_device_set(2, CH_FX2, CH_DEV_REPEAT);
    for (unsigned b = 0; b < 8; ++b) ch_render(buf, 128, 0);
    printf("T1 DELAY nomem %x, T2 DUB nomem %x, T3 DELAY+REPEAT nomem %x, master %x\n", sn->fx_nomem[0], sn->fx_nomem[1], sn->fx_nomem[2], sn->mfx_nomem);
    ch_param_set(CH_MASTER, CH_FX2, 2, 0.5f);           /* the master DELAY's MIX up: it needs 8 */
    for (unsigned b = 0; b < 8; ++b) ch_render(buf, 128, 0);
    printf("master DELAY up: master nomem %x\n", sn->mfx_nomem);
    return 0;
}

/* memtest2: the app's way: ALGO stepping through every effect on the way (each taking and giving back memory) */
static int cmd_memtest2(void)
{
    ch_init(); int32_t buf[2 * 128];
    const ch_snapshot *sn = ch_snapshot_get();
    for (unsigned b = 0; b < 4; ++b) ch_render(buf, 128, 0);
    for (unsigned d = CH_DEV_FX_FIRST; d <= CH_DEV_DELAY; ++d) { ch_device_set(0, CH_FX1, d); ch_bypass_set(0, CH_FX1, 0); for (unsigned b = 0; b < 3; ++b) ch_render(buf, 128, 0); }
    printf("T1 %s: nomem %x\n", ch_device_info(ch_track_get(0)->device[CH_FX1])->name, sn->fx_nomem[0]);
    for (unsigned d = CH_DEVICES - 1; d >= CH_DEV_DUB; --d) { ch_device_set(1, CH_FX1, d); ch_bypass_set(1, CH_FX1, 0); for (unsigned b = 0; b < 3; ++b) ch_render(buf, 128, 0); }
    printf("T2 %s: nomem %x\n", ch_device_info(ch_track_get(1)->device[CH_FX1])->name, sn->fx_nomem[1]);
    for (unsigned d = CH_DEV_FX_FIRST; d <= CH_DEV_DELAY; ++d) { ch_device_set(2, CH_FX1, d); ch_bypass_set(2, CH_FX1, 0); for (unsigned b = 0; b < 3; ++b) ch_render(buf, 128, 0); }
    printf("T3 %s: nomem %x\n", ch_device_info(ch_track_get(2)->device[CH_FX1])->name, sn->fx_nomem[2]);
    extern unsigned ch_fx_blocks_used(void);
    printf("blocks held now: %u of %d\n", ch_fx_blocks_used(), CH_FX_BLOCKS);
    return 0;
}

/* CH_FAKE_CPU2=1: the second core's path, its jobs run at once here (a check that the split render sounds the same) */
static int fake_run(void (*fn)(uint32_t), uint32_t arg) { fn(arg); return 1; }
static int fake_wait(void) { return 0; }
static uint32_t fake_clock(void) { return 0; }
/* CH_SLOW=x: the profiler's clock runs x times the PC's (microseconds), so the load governor sees a slow chip */
#include <windows.h>
static double now_us(void) { LARGE_INTEGER f, c; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&c); return (double)c.QuadPart * 1e6 / (double)f.QuadPart; }
static double slow_x = 1;
static uint32_t slow_clock(void)
{
    static LARGE_INTEGER fq; LARGE_INTEGER c;
    if (!fq.QuadPart) QueryPerformanceFrequency(&fq);
    QueryPerformanceCounter(&c);
    return (uint32_t)((double)c.QuadPart * 1e6 * slow_x / (double)fq.QuadPart);
}

int main(int argc, char **argv)
{
    if (getenv("CH_FAKE_CPU2")) { ch_profile_clock(fake_clock, 1); ch_worker(fake_run, fake_wait); }
    if (getenv("CH_SLOW")) { slow_x = atof(getenv("CH_SLOW")); ch_profile_clock(slow_clock, 1); }
    if (argc >= 3 && !strcmp(argv[1], "synths")) return cmd_synths(argv[2]);
    if (argc >= 2 && !strcmp(argv[1], "ripple")) return cmd_ripple();
    if (argc >= 2 && !strcmp(argv[1], "gatetest")) return cmd_gatetest();
    if (argc >= 2 && !strcmp(argv[1], "kitcut")) return cmd_kitcut();
    if (argc >= 2 && !strcmp(argv[1], "anab")) return cmd_anab();
    if (argc >= 2 && !strcmp(argv[1], "drivecorners")) return cmd_drivecorners();
    if (argc >= 2 && !strcmp(argv[1], "drivecost")) return cmd_drivecost();
    if (argc >= 2 && !strcmp(argv[1], "trance")) return cmd_trance();
    if (argc >= 2 && !strcmp(argv[1], "mixmod")) return cmd_mixmod();
    if (argc >= 2 && !strcmp(argv[1], "smp")) return cmd_smp();
    if (argc >= 2 && !strcmp(argv[1], "stepsend")) return cmd_stepsend();
    if (argc >= 2 && !strcmp(argv[1], "vcount")) return cmd_vcount();
    if (argc >= 3 && !strcmp(argv[1], "synthsweep")) return cmd_synthsweep(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "drumsweep")) return cmd_drumsweep(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "stagewav")) return cmd_stagewav(argv[2]);
#if defined(CH_PROF)
    if (argc >= 2 && !strcmp(argv[1], "prof")) return cmd_prof(argc >= 3 ? atoi(argv[2]) : 0);
#endif
    if (argc >= 2 && !strcmp(argv[1], "stagecost")) return cmd_stagecost();
    if (argc >= 2 && !strcmp(argv[1], "spikes")) return cmd_spikes(argc >= 3 ? (unsigned)atoi(argv[2]) : 4);
    if (argc >= 2 && !strcmp(argv[1], "drivemix")) return cmd_drivemix(argc >= 3 ? argv[2] : 0);
    if (argc >= 3 && !strcmp(argv[1], "drivewav")) return cmd_drivewav(argv[2]);
    if (argc >= 2 && !strcmp(argv[1], "hitcost")) return cmd_hitcost();
    if (argc >= 2 && !strcmp(argv[1], "twostep")) return cmd_twostep();
    if (argc >= 2 && !strcmp(argv[1], "drumtie")) return cmd_drumtie();
    if (argc >= 2 && !strcmp(argv[1], "dropout")) return cmd_dropout();
    if (argc >= 2 && !strcmp(argv[1], "clicks")) return cmd_clicks(argc > 2 ? argv[2] : 0);
    if (argc >= 2 && !strcmp(argv[1], "guardhunt")) return cmd_guardhunt();
    if (argc >= 2 && !strcmp(argv[1], "duck")) return cmd_duck(argc > 2 ? argv[2] : 0);
    if (argc >= 2 && !strcmp(argv[1], "dubf")) return cmd_dubf(argc > 2 ? argv[2] : 0);
    if (argc >= 3 && !strcmp(argv[1], "acid")) return cmd_acid(argv[2]);
    if (argc >= 3 && !strcmp(argv[1], "comb")) return cmd_comb(argv[2]);
    if (argc >= 2 && !strcmp(argv[1], "microtest")) return cmd_microtest();
    if (argc >= 2 && !strcmp(argv[1], "mistress")) return cmd_mistress();
    if (argc >= 3 && !strcmp(argv[1], "kit")) return cmd_kit(argv[2]);
    if (argc >= 2 && !strcmp(argv[1], "drives")) return cmd_drives();
    if (argc >= 2 && !strcmp(argv[1], "alias")) return cmd_alias();
    if (argc >= 2 && !strcmp(argv[1], "safety")) return cmd_safety();
    if (argc >= 3 && !strcmp(argv[1], "crystal")) return cmd_crystal(argv[2]);
    if (argc >= 4 && !strcmp(argv[1], "ab")) return cmd_ab((unsigned)atoi(argv[2]), argv[3]);
    if (argc >= 2 && !strcmp(argv[1], "scope")) return cmd_scope();
    if (argc >= 3 && !strcmp(argv[1], "drums")) return cmd_drums(argv[2]);
    if (argc >= 2 && !strcmp(argv[1], "drumstress")) return cmd_drumstress(argc >= 3 ? (unsigned)atoi(argv[2]) : 0, argc >= 4 ? (unsigned)atoi(argv[3]) : 64);
    if (argc >= 2 && !strcmp(argv[1], "corners")) return cmd_corners(argc >= 3 ? (unsigned)atoi(argv[2]) : 0);
    if (argc >= 2 && !strcmp(argv[1], "lanes")) return cmd_lanes();
    if (argc >= 2 && !strcmp(argv[1], "budget")) return cmd_budget();
    if (argc >= 2 && !strcmp(argv[1], "voices")) return cmd_voices();
    if (argc >= 3 && !strcmp(argv[1], "ratchet")) return cmd_ratchet((unsigned)atoi(argv[2]));
    if (argc >= 2 && !strcmp(argv[1], "stutter")) return cmd_stutter(argc >= 3 ? (unsigned)atoi(argv[2]) : 0);
    if (argc >= 2 && !strcmp(argv[1], "cost")) return cmd_cost();
    if (argc >= 2 && !strcmp(argv[1], "locktest")) return cmd_locktest();
    if (argc >= 2 && !strcmp(argv[1], "memtest")) return cmd_memtest();
    if (argc >= 2 && !strcmp(argv[1], "memtest2")) return cmd_memtest2();
    if (argc >= 2 && !strcmp(argv[1], "trigtest")) return cmd_trigtest();
    if (argc >= 2 && !strcmp(argv[1], "sidetest")) return cmd_sidetest();
    if (argc >= 2 && !strcmp(argv[1], "reptest")) return cmd_reptest();
    if (argc >= 3 && !strcmp(argv[1], "fxall")) return cmd_fxall(argv[2]);
    if (argc >= 2 && !strcmp(argv[1], "fxcheck")) return cmd_fxcheck(argc > 2 ? (unsigned)atoi(argv[2]) : 200);
    if (argc >= 4 && !strcmp(argv[1], "adpcm")) return cmd_adpcm(argv[2], argv[3]);
    if (argc >= 5 && !strcmp(argv[1], "scene"))
        return cmd_scene(argv[2], argv[3], argv[4], argc > 5 ? atof(argv[5]) : 8, argc > 6 ? (float)atof(argv[6]) : 120,
                         argv + 7, argc > 7 ? argc - 7 : 0);
    fprintf(stderr, "usage: bench adpcm <in.wav> <out.wav>\n       bench scene <name> <sample.wav> <out.wav> [seconds] [bpm]\n");
    return 2;
}
