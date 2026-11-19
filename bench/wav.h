/* Minimal WAV reader/writer for the PC bench (16/24-bit PCM, 32-bit float). */
#ifndef CHISEL_WAV_H
#define CHISEL_WAV_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Reads any PCM16/24/float WAV into mono float at its own rate. */
static float *wav_read_mono(const char *path, unsigned *frames, unsigned *rate)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    uint8_t h[12];
    if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) { fclose(f); return NULL; }
    unsigned fmt = 0, ch = 0, bits = 0; float *out = NULL;
    for (;;) {
        uint8_t c[8];
        if (fread(c, 1, 8, f) != 8) break;
        uint32_t size = c[4] | c[5] << 8 | c[6] << 16 | (uint32_t)c[7] << 24;
        if (!memcmp(c, "fmt ", 4)) {
            uint8_t b[40] = {0};
            fread(b, 1, size < 40 ? size : 40, f);
            if (size > 40) fseek(f, size - 40, SEEK_CUR);
            fmt = b[0] | b[1] << 8; ch = b[2] | b[3] << 8;
            *rate = b[4] | b[5] << 8 | b[6] << 16 | (uint32_t)b[7] << 24;
            bits = b[14] | b[15] << 8;
            if (fmt == 0xFFFE) fmt = b[24] | b[25] << 8;
        } else if (!memcmp(c, "data", 4)) {
            unsigned bps = bits / 8, n = size / (bps * ch);
            uint8_t *raw = malloc(size);
            n = (unsigned)(fread(raw, 1, size, f) / (bps * ch));
            out = malloc(sizeof(float) * (n ? n : 1));
            for (unsigned i = 0; i < n; ++i) {
                float s = 0;
                for (unsigned k = 0; k < ch; ++k) {
                    const uint8_t *p = raw + (i * ch + k) * bps;
                    if (fmt == 3 && bits == 32) { float v; memcpy(&v, p, 4); s += v; }
                    else if (bits == 16) s += (int16_t)(p[0] | p[1] << 8) / 32768.0f;
                    else if (bits == 24) s += ((int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24) >> 8) / 8388608.0f;
                }
                out[i] = s / ch;
            }
            free(raw); *frames = n; break;
        } else fseek(f, size + (size & 1), SEEK_CUR);
    }
    fclose(f);
    return out;
}
static void put16(FILE *f, unsigned v) { fputc(v & 255, f); fputc(v >> 8 & 255, f); }
static void put32(FILE *f, uint32_t v) { put16(f, v & 65535); put16(f, v >> 16); }
/* Writes interleaved stereo Q23 as 24-bit PCM. */
static int wav_write_q23(const char *path, const int32_t *pcm, unsigned frames, unsigned rate)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    uint32_t data = frames * 6;
    fwrite("RIFF", 1, 4, f); put32(f, 36 + data); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 2); put32(f, rate); put32(f, rate * 6); put16(f, 6); put16(f, 24);
    fwrite("data", 1, 4, f); put32(f, data);
    for (unsigned i = 0; i < frames * 2; ++i) {
        int32_t v = pcm[i];
        fputc(v & 255, f); fputc(v >> 8 & 255, f); fputc(v >> 16 & 255, f);
    }
    fclose(f);
    return 0;
}
#endif
