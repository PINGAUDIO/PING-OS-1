/* A bench for the SYNTH's ANALOG model: the engine alone, one track, a few phrases into WAV files, and its time.
 *   vabench OUTDIR [param=value ...]      (param: the SYNTH parameter's number; values as the engine holds them)
 * Built by dev/vabench.ps1 against any engine directory, so two versions of the model can be heard side by side. */
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "chisel.h"

static FILE *wf; static unsigned wframes; static double peak, sumsq; static double spent; static double sl, sr, slr;
static void put(unsigned frames)
{
    frames = (frames + 63u) & ~63u;                        /* whole sub-blocks, as the device and the plugin always ask */
    int32_t blk[128 * 2];
    while (frames) {
        unsigned n = frames > 128 ? 128 : frames;
        LARGE_INTEGER a, b, fq; QueryPerformanceFrequency(&fq); QueryPerformanceCounter(&a);
        ch_render(blk, n, 0);
        QueryPerformanceCounter(&b); spent += (double)(b.QuadPart - a.QuadPart) / (double)fq.QuadPart;
        for (unsigned i = 0; i < n * 2; ++i) {
            double v = blk[i] / 8388608.0; if (fabs(v) > peak) peak = fabs(v); sumsq += v * v;
            int32_t q = blk[i] >> 8; int16_t s = (int16_t)(q > 32767 ? 32767 : q < -32768 ? -32768 : q); if (wf) fwrite(&s, 2, 1, wf);
        }
        for (unsigned i = 0; i < n; ++i) { double l = blk[2 * i] / 8388608.0, r = blk[2 * i + 1] / 8388608.0; sl += l * l; sr += r * r; slr += l * r; }
        wframes += n; frames -= n;
    }
}
static void ms(unsigned m) { put(m * 441 / 10); }
static void wav_open(const char *dir, const char *name)
{
    char p[512]; snprintf(p, sizeof p, "%s/%s.wav", dir, name);
    wf = fopen(p, "wb+"); wframes = 0; peak = 0; sumsq = 0; spent = 0; sl = sr = slr = 0;
    uint8_t h[44] = {0}; fwrite(h, 1, 44, wf);
}
static unsigned bad_sum(void);
static void wav_close(const char *name)
{
    uint32_t d = wframes * 4, r = 44100, br = r * 4, fs = 16, riff = 36 + d; uint16_t a = 4, b = 16, ch = 2, fmt = 1;
    fseek(wf, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, wf); fwrite(&riff, 4, 1, wf); fwrite("WAVEfmt ", 1, 8, wf); fwrite(&fs, 4, 1, wf); fwrite(&fmt, 2, 1, wf); fwrite(&ch, 2, 1, wf);
    fwrite(&r, 4, 1, wf); fwrite(&br, 4, 1, wf); fwrite(&a, 2, 1, wf); fwrite(&b, 2, 1, wf); fwrite("data", 1, 4, wf); fwrite(&d, 4, 1, wf);
    fclose(wf); wf = 0;
    double sec = wframes / 44100.0;
    printf("%-8s %.1f s  peak %.3f  rms %.3f  render %.2f %% of real time\n", name, sec, peak, sqrt(sumsq / (wframes * 2.0)), 100.0 * spent / sec);
    if (bad_sum()) printf("         BAD %u\n", bad_sum());
}
static void setup(int argc, char **argv)
{
    ch_init();
    ch_device_set(0, CH_MATERIAL, CH_DEV_SYNTH);
    put(128);
    unsigned model = 0;                                   /* MODEL: ANALOG, or model=N */
    for (int i = 2; i < argc; ++i) sscanf(argv[i], "model=%u", &model);
    ch_param_set(0, CH_MATERIAL, 0, (float)model);
    for (unsigned p = 2; p < 32; ++p) { float v; if (ch_synth_preset(model, p, &v)) ch_param_set(0, CH_MATERIAL, p, v); }
    for (int i = 2; i < argc; ++i) { int p; float v; if (sscanf(argv[i], "%d=%f", &p, &v) == 2) ch_param_set(0, CH_MATERIAL, (unsigned)p, v); }
#ifdef CH_BENCH_UNISON
    for (int i = 2; i < argc; ++i) { int p; float v;                       /* unison: the effect on the track; fP=V: its parameters */
        if (!strcmp(argv[i], "unison")) ch_device_set(0, CH_FX1, CH_DEV_UNISON);
        else if (!strcmp(argv[i], "erode")) ch_device_set(0, CH_FX1, CH_DEV_ERODE);
        else if (!strcmp(argv[i], "repeat")) ch_device_set(0, CH_FX1, CH_DEV_REPEAT);
#ifndef CH_BENCH_NOSUB
        else if (!strcmp(argv[i], "sub64")) ch_sub_set(64);
#endif
        else if (sscanf(argv[i], "m%d=%f", &p, &v) == 2 && p >= 1 && p <= 4) ch_device_set(CH_MASTER, CH_FX1 + (unsigned)p - 1, (unsigned)v);   /* mN=D: effect D in the master's slot N */
        else if (sscanf(argv[i], "send=%f", &v) == 1) ch_send_set(0, 0, v);
        else if (sscanf(argv[i], "x%d=%f", &p, &v) == 2 && p >= 1 && p <= 4) ch_device_set(0, CH_FX1 + (unsigned)p - 1, (unsigned)v);   /* xN=D: effect D (its number) in slot N */
        else if (!strcmp(argv[i], "reverb2")) ch_device_set(0, CH_FX2, CH_DEV_REVERB);
        else if (sscanf(argv[i], "g%d=%f", &p, &v) == 2) ch_param_set(0, CH_FX2, (unsigned)p, v);   /* gP=V: the second effect's */
        else if (!strcmp(argv[i], "wbi")) ch_mode_set(0, CH_MATERIAL, 3, 1);   /* WOBBLE's DEPTH: BIPOLAR */
        else if (sscanf(argv[i], "f%d=%f", &p, &v) == 2) ch_param_set(0, CH_FX1, (unsigned)p, v); }
#endif
    put(8820);                                            /* (the values settle: they glide) */
}
static void play(unsigned note, unsigned vel, unsigned on_ms, unsigned off_ms) { ch_note(0, note, vel); ms(on_ms); ch_note(0, note, 0); ms(off_ms); }

/* sweep: every effect in slot 3 (another before it), at its defaults, with every knob at nothing and at full, and with
 * its knobs moved while it plays; the guard's count of broken values (NaN, infinity, too large) for each */
static unsigned bad_sum(void) { const ch_snapshot *sn = ch_snapshot_get(); unsigned b = sn->master_bad; for (unsigned t = 0; t < CH_TRACKS; ++t) for (unsigned k = 0; k < CH_SLOTS; ++k) b += sn->bad[t][k]; return b; }
static void sweep(void)
{
    static const uint8_t C[4] = {48, 55, 60, 63};
    for (unsigned dev = CH_DEV_FILTER; dev < CH_DEVICES; ++dev) {
        const ch_device_desc *d = ch_device_info(dev);
        for (int mode = 0; mode < 9 + 16; ++mode) {   /* 9..24: every knob at nothing or full, in each of the sixteen ways of its first four */   /* 5..8: a modulator on every knob, full depth, the knobs at nothing or at full, the sequencer running */
            ch_init(); ch_device_set(0, CH_MATERIAL, CH_DEV_SYNTH); put(128);
            ch_param_set(0, CH_MATERIAL, 0, 0);
            for (unsigned p = 2; p < 32; ++p) { float v; if (ch_synth_preset(0, p, &v)) ch_param_set(0, CH_MATERIAL, p, v); }
            ch_device_set(0, CH_FX2, CH_DEV_DRIVE); ch_device_set(0, CH_FX3, dev); put(4410);
            if (mode == 1 || mode == 2) for (unsigned p = 0; p < d->params; ++p) ch_param_set(0, CH_FX3, p, mode == 1 ? 0.0f : d->param[p].kind == CH_SELECT ? (float)(d->param[p].steps - 1) : 1.0f);
            if (mode >= 9) { for (unsigned p = 0; p < d->params; ++p) { int hi = p < 4 ? (mode - 9) >> p & 1 : (mode - 9) & 1; ch_param_set(0, CH_FX3, p, !hi ? 0.0f : d->param[p].kind == CH_SELECT ? (float)(d->param[p].steps - 1) : 1.0f); } }
            else if (mode >= 5) {
                for (unsigned p = 0; p < d->params; ++p) ch_param_set(0, CH_FX3, p, (mode & 1) ? 0.0f : d->param[p].kind == CH_SELECT ? (float)(d->param[p].steps - 1) : 1.0f);
                ch_mod_param_set(0, 0, 0, mode >= 7 ? 1.0f : 0.5f);
                for (unsigned p = 0; p < d->params && p < 16; ++p) ch_map_set(0, 0, CH_FX3, p, (mode & 1) ? 1.0f : -1.0f, 1);
            }
            if (mode >= 4) ch_transport(1);
            for (int k = 0; k < 4; ++k) ch_note(0, C[k], 110);
            for (int step = 0; step < 60; ++step) {
                if (mode == 3) for (unsigned p = 0; p < d->params; ++p) { float r = (float)((step * 37 + p * 101) % 97) / 96.0f; ch_param_set(0, CH_FX3, p, d->param[p].kind == CH_SELECT ? (float)(int)(r * (float)(d->param[p].steps - 1) + 0.5f) : d->param[p].kind == CH_BI ? r * 2 - 1 : r); }
                if (step == 30) for (int k = 0; k < 4; ++k) ch_note(0, C[k], 0);
                if (step == 34) for (int k = 0; k < 4; ++k) ch_note(0, C[k] + 12, 127);
                put(2205);
            }
            unsigned b = bad_sum();
            if (b) printf("BAD %-10s mode %d: %u\n", d->name, mode, b);
        }
    }
    printf("sweep done\n");
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    if (argc > 1 && !strcmp(argv[1], "sweep")) { sweep(); return 0; }
    /* a bass line: short notes, low, the filter's envelope on each */
    setup(argc, argv); wav_open(dir, "bass");
    { static const uint8_t N[16] = {48, 48, 60, 48, 51, 48, 58, 48, 46, 46, 58, 46, 53, 55, 58, 60};
      for (int r = 0; r < 2; ++r) for (int i = 0; i < 16; ++i) play(N[i], i % 4 == 0 ? 127 : 90, 90, 35); }
    ms(600); wav_close("bass");
    /* a chord held: four notes, three seconds, then let go */
    setup(argc, argv); wav_open(dir, "chord");
    { static const uint8_t C[4] = {60, 63, 67, 70};
      for (int k = 0; k < 4; ++k) ch_note(0, C[k], 100);
      ms(3000);
      for (int k = 0; k < 4; ++k) ch_note(0, C[k], 0);
      ms(1500); }
    wav_close("chord");
    /* one long low note: the two oscillators beating */
    setup(argc, argv); wav_open(dir, "drone");
    play(48, 110, 4000, 800); wav_close("drone");
    /* a lead: the same note at three velocities, then a run up */
    setup(argc, argv); wav_open(dir, "lead");
    play(72, 40, 300, 100); play(72, 80, 300, 100); play(72, 127, 300, 100);
    { static const uint8_t N[8] = {72, 75, 77, 79, 82, 84, 87, 91}; for (int i = 0; i < 8; ++i) play(N[i], 110, 180, 40); }
    ms(800); wav_close("lead");
    /* notes that overlap: each played while the one before is still held (LEGATO: one voice carried on) */
    setup(argc, argv); wav_open(dir, "slide");
    { static const uint8_t N[6] = {48, 55, 51, 60, 53, 48};
      ch_note(0, N[0], 110); ms(400);
      for (int i = 1; i < 6; ++i) { ch_note(0, N[i], 110); ms(60); ch_note(0, N[i - 1], 0); ms(340); }
      ch_note(0, N[5], 0); ms(800); }
    wav_close("slide");
    return 0;
}
