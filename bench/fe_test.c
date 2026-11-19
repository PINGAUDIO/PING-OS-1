/* PING FM-1: the Felucca engines (engine/ch_fe*) on the PC. Renders each model through a short phrase to
 * dev/out/fe/<NAME>.wav and checks it by numbers:
 *   phrase: 8 single notes (knob 0 swept over them), a 3-voice chord (knob 1 swept), 8 more notes (knob 2 swept),
 *           then the release tail; one voice for the notes, three for the chord
 *   prints peak / RMS / DC / NaN-Inf count / the largest sample step, the tail's level, whether every voice
 *   ended, the pitch of a held A3 (autocorrelation), that rendering in odd sub-block sizes gives the same
 *   samples as whole blocks, and the time a voice-block costs here.
 * Build: bench/fe_build.ps1 -> bench/build/fe_test.exe (run from the repo root). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0777)
#endif
#include "ch_fe.h"
#include "wav.h"

#define RATE 44100
#define SECS 8
#define NFR (RATE * SECS)
#define NV 3

static float buf[NFR];

static float note_hz(int midi) { return 440.0f * powf(2.0f, (midi - 69) / 12.0f); }

/* renders the phrase with sub-blocks of `chunk` frames (0 = a random 1..32 each time) */
static void phrase(unsigned model, float *out, unsigned chunk, int sweep, int *ended)
{
    static const int MEL[8] = {48, 51, 55, 58, 60, 58, 55, 51};
    static const int MEL2[8] = {60, 63, 67, 70, 72, 67, 63, 60};
    static const int CHORD[3] = {60, 63, 67};
    ch_fe_voice v[NV];
    float hz[NV] = {0};
    unsigned i, t = 0, rng = 12345;
    float last_k[3] = {0.5f, 0.5f, 0.5f};
    for (i = 0; i < NV; i++)
        ch_fe_init(&v[i], i + 1);
    memset(out, 0, sizeof(float) * NFR);
    while (t < NFR) {
        float s = (float)t / RATE, k[3];
        unsigned n, next;
        /* knob 0 swept 0..2 s, knob 1 2..4 s, knob 2 4..6 s; else 0.5 (knob 0 rests at 0.05 after its sweep) */
        k[0] = s < 2.0f ? s / 2.0f : 0.05f;
        k[1] = s >= 2.0f && s < 4.0f ? (s - 2.0f) / 2.0f : 0.5f;
        k[2] = s >= 4.0f && s < 6.0f ? (s - 4.0f) / 2.0f : 0.5f;
        if (!sweep)                            /* (the sub-block check: constant knobs) */
            k[0] = k[1] = k[2] = 0.3f;
        /* events on 0.25 s steps */
        if (t % (RATE / 4) == 0) {
            unsigned step = t / (RATE / 4);
            if (step < 8) {
                hz[0] = note_hz(MEL[step]);
                ch_fe_note_on(&v[0], model, hz[0], 0.6f + 0.05f * (float)(step % 4), k);
            } else if (step == 8) {
                for (i = 0; i < NV; i++) {
                    hz[i] = note_hz(CHORD[i]);
                    ch_fe_note_on(&v[i], model, hz[i], 0.8f, k);
                }
            } else if (step >= 16 && step < 24) {
                hz[0] = note_hz(MEL2[step - 16]);
                ch_fe_note_on(&v[0], model, hz[0], 0.7f, k);
            }
        }
        if (t % (RATE / 4) == RATE / 5) {      /* note-offs 0.2 s in (the chord's at 3.8 s) */
            unsigned step = t / (RATE / 4);
            if (step < 8 || (step >= 16 && step < 24))
                ch_fe_note_off(&v[0]);
            if (step == 15)
                for (i = 0; i < NV; i++)
                    ch_fe_note_off(&v[i]);
        }
        next = (t / (RATE / 4) + 1) * (RATE / 4);
        if (t % (RATE / 4) < RATE / 5)
            next = t - t % (RATE / 4) + RATE / 5;
        if (chunk)
            n = chunk;
        else {
            rng = rng * 1103515245u + 12345u;
            n = 1 + (rng >> 16) % 32u;
        }
        if (t + n > next)
            n = next - t;
        if (t + n > NFR)
            n = NFR - t;
        for (i = 0; i < NV; i++)
            ch_fe_render(&v[i], model, hz[i], k, out + t, n);
        memcpy(last_k, k, sizeof last_k);
        t += n;
    }
    /* then silence until every voice has ended (up to 20 s more): *ended = the time it took after the phrase, ms */
    for (t = 0; t < 20 * RATE; t += 32) {
        static float sink[32];
        int any = 0;
        for (i = 0; i < NV; i++) {
            ch_fe_render(&v[i], model, hz[i], last_k, sink, 32);
            any |= ch_fe_active(&v[i]);
        }
        if (!any)
            break;
    }
    *ended = t < 20 * RATE ? (int)(t / (RATE / 1000)) : -1;
}

static float pitch_of(unsigned model, float hz, const float *k)
{
    static float x[8192];
    static double cor[1201];
    ch_fe_voice v;
    int lag, best = 0;
    double bc = -1;
    ch_fe_init(&v, 7);
    memset(x, 0, sizeof x);
    ch_fe_note_on(&v, model, hz, 0.8f, k);
    ch_fe_render(&v, model, hz, k, x, 8192);
    for (lag = 40; lag < 1200; lag++) {   /* normalised autocorrelation over the second half */
        double s = 0, e0 = 0, e1 = 0;
        int i;
        for (i = 4096; i < 8192 - 1200; i++) {
            s += x[i] * x[i + lag];
            e0 += x[i] * x[i];
            e1 += x[i + lag] * x[i + lag];
        }
        s = e0 > 0 && e1 > 0 ? s / sqrt(e0 * e1) : 0;
        cor[lag] = s;
        if (s > bc) {
            bc = s;
            best = lag;
        }
    }
    for (lag = 40; lag < 1200; lag++)    /* the shortest lag nearly as good as the best: the period */
        if (cor[lag] > 0.97 * bc && cor[lag] >= cor[lag - 1] && cor[lag] >= cor[lag + 1]) {
            best = lag;
            break;
        }
    return best ? (float)RATE / (float)best : 0;
}

int main(void)
{
    static float alt[NFR];
    static int32_t pcm[NFR * 2];
    unsigned m, i;
    int fails = 0;
    MKDIR("dev");
    MKDIR("dev/out");
    MKDIR("dev/out/fe");
    printf("sizeof(ch_fe_voice) = %u bytes (16 voices: %u bytes)\n", (unsigned)sizeof(ch_fe_voice),
           (unsigned)(16 * sizeof(ch_fe_voice)));
    printf("%-6s %6s %6s %8s %4s %6s %8s %5s %8s %6s %8s\n", "model", "peak", "rms", "dc", "nan", "step", "tail",
           "end ms", "A3 pitch", "chunks", "us/blk");
    for (m = 0; m < CH_FE_MODELS; m++) {
        double sum = 0, sq = 0, tail = 0, maxstep = 0;
        float peak = 0, f0, sp[4] = {0, 0, 0, 0};
        int nan = 0, ended, ended2, same = 1;
        char path[256];
        const float kp[3] = {0, 0, 0};
        clock_t c0;
        double us;
        phrase(m, alt, 32, 0, &ended2);       /* whole blocks vs random sub-blocks, the same knobs */
        phrase(m, buf, 0, 0, &ended2);
        for (i = 0; i < NFR; i++)
            if (alt[i] != buf[i])
                same = 0;
        phrase(m, buf, 32, 1, &ended);
        for (i = 0; i < NFR; i++) {
            float x = buf[i];
            if (x != x || x > 1e9f || x < -1e9f) {
                nan++;
                continue;
            }
            if (fabsf(x) > peak)
                peak = fabsf(x);
            if (fabsf(x) > sp[i / (2 * RATE)])
                sp[i / (2 * RATE)] = fabsf(x);
            sum += x;
            sq += (double)x * x;
            if (i && fabs((double)x - buf[i - 1]) > maxstep)
                maxstep = fabs((double)x - buf[i - 1]);
            if (i >= NFR - RATE / 5)
                tail += (double)x * x;
        }
        f0 = pitch_of(m, 220.0f, kp);
        {   /* cost: 16 voices held for 1 s */
            static float sink[CH_FE_BLOCK];
            ch_fe_voice v[16];
            const float kc[3] = {0.5f, 0.5f, 0.5f};
            unsigned b, j;
            for (j = 0; j < 16; j++) {
                ch_fe_init(&v[j], j);
                ch_fe_note_on(&v[j], m, note_hz(48 + (int)j * 2), 0.8f, kc);
            }
            c0 = clock();
            for (b = 0; b < RATE / CH_FE_BLOCK; b++)
                for (j = 0; j < 16; j++)
                    ch_fe_render(&v[j], m, note_hz(48 + (int)j * 2), kc, sink, CH_FE_BLOCK);
            us = (double)(clock() - c0) * 1e6 / CLOCKS_PER_SEC / (16.0 * (RATE / CH_FE_BLOCK));
        }
        printf("%-6s %6.3f %6.3f %8.5f %4d %6.3f %8.1e %6d %8.1f %6s %8.2f\n", ch_fe_names[m], peak,
               sqrt(sq / NFR), sum / NFR, nan, maxstep, sqrt(tail / (RATE / 5)), ended, f0,
               same ? "same" : "DIFF", us);
        printf("       peaks: notes %.3f  chord %.3f  notes %.3f  tail %.3f\n", sp[0], sp[1], sp[2], sp[3]);
        if (nan || peak > 1.0f || peak < 0.2f || ended < 0 || !same || fabs(sum / NFR) > 0.01)
            fails++;
        for (i = 0; i < NFR; i++) {
            float x = buf[i] != buf[i] ? 0 : buf[i];
            int32_t q = (int32_t)(fmaxf(-1.0f, fminf(1.0f, x)) * 8388607.0f);
            pcm[2 * i] = pcm[2 * i + 1] = q;
        }
        snprintf(path, sizeof path, "dev/out/fe/%s.wav", ch_fe_names[m]);
        wav_write_q23(path, pcm, NFR, RATE);
    }
    /* stress: every knob at 0 / 0.5 / 1, a loud 3-note chord low, mid and high, 1 s held + 1 s released */
    printf("stress (27 knob corners x 3 registers, 3 voices at velocity 1):\n");
    for (m = 0; m < CH_FE_MODELS; m++) {
        float worst = 0, wk[4] = {0, 0, 0, 0};
        int bad = 0, c, r;
        for (c = 0; c < 27; c++)
            for (r = 0; r < 3; r++) {
                static const int ROOT[3] = {28, 52, 88};
                ch_fe_voice v[3];
                float k[3], hz[3], pk = 0;
                unsigned j, b;
                k[0] = (float)(c % 3) * 0.5f;
                k[1] = (float)(c / 3 % 3) * 0.5f;
                k[2] = (float)(c / 9) * 0.5f;
                for (j = 0; j < 3; j++) {
                    hz[j] = note_hz(ROOT[r] + (int)j * 4);
                    ch_fe_init(&v[j], j + 3);
                    ch_fe_note_on(&v[j], m, hz[j], 1.0f, k);
                }
                for (b = 0; b < 2 * RATE / 32; b++) {
                    float x[32] = {0};
                    if (b == RATE / 32)
                        for (j = 0; j < 3; j++)
                            ch_fe_note_off(&v[j]);
                    for (j = 0; j < 3; j++)
                        ch_fe_render(&v[j], m, hz[j], k, x, 32);
                    for (j = 0; j < 32; j++) {
                        if (x[j] != x[j] || fabsf(x[j]) > 1e9f)
                            bad++;
                        else if (fabsf(x[j]) > pk)
                            pk = fabsf(x[j]);
                    }
                }
                if (pk > worst) {
                    worst = pk;
                    wk[0] = k[0], wk[1] = k[1], wk[2] = k[2], wk[3] = (float)ROOT[r];
                }
            }
        printf("  %-6s worst peak %.3f (knobs %.1f %.1f %.1f, root note %d), NaN/Inf %d\n", ch_fe_names[m], worst,
               wk[0], wk[1], wk[2], (int)wk[3], bad);
        if (bad || worst > 2.0f)   /* (three voices at full velocity: up to ~0.65 each) */
            fails++;
    }
    printf("labels:\n");
    for (m = 0; m < CH_FE_MODELS; m++) {
        printf("  %-6s", ch_fe_names[m]);
        for (i = 0; i < 3; i++) {
            float x;
            printf(" | %-8s:", ch_fe_label(m, i));
            for (x = 0.0f; x <= 1.0f; x += 0.125f) {
                const char *s = ch_fe_value(m, i, x);
                if (s)
                    printf(" %s", s);
            }
        }
        printf("\n");
    }
    printf(fails ? "FAIL: %d model(s)\n" : "ok\n", fails);
    return fails ? 1 : 0;
}
