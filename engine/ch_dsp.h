/* Shared DSP helpers: float, single precision, no libm in the audio path
 * (the FM-1 build forbids libc calls in audio code). */
#ifndef CH_DSP_H
#define CH_DSP_H
#include <stdint.h>
#if defined(__has_include)
#if __has_include(<string.h>)
#include <string.h>
#define CH_HAVE_STRING_H 1
#endif
#endif
#ifndef CH_HAVE_STRING_H   /* the FM-1 is freestanding: felucca-bsp's chip/libc.c provides these */
void *memset(void *d, int c, unsigned n);
void *memcpy(void *d, const void *s, unsigned n);
int memcmp(const void *a, const void *b, unsigned n);
#endif
#include "ch_platform.h"

#define CH_PI 3.14159265358979f
#define CH_TWO_PI 6.28318530717959f

CH_SI float ch_clamp(float x, float a, float b) { return x < a ? a : x > b ? b : x; }
CH_SI float ch_min(float a, float b) { return a < b ? a : b; }
CH_SI float ch_max(float a, float b) { return a > b ? a : b; }
CH_SI float ch_abs(float x) { return x < 0 ? -x : x; }
CH_SI float ch_lerp(float a, float b, float t) { return a + (b - a) * t; }

/* a float's bits and back. A union, not memcpy: the firmware builds with -fno-builtin, where memcpy is a real call
 * (in a per-sample loop that cost the FM-1 more than the maths around it) */
typedef union { float f; uint32_t u; } ch_fbits;
CH_SI float ch_from_bits(uint32_t u) { ch_fbits c; c.u = u; return c.f; }
CH_SI uint32_t ch_to_bits(float f) { ch_fbits c; c.f = f; return c.u; }
CH_SI float ch_exp2(float x)
{
    x = ch_clamp(x, -126.0f, 126.0f);
    int n = (int)(x + (x < 0 ? -0.5f : 0.5f));
    float a = (x - (float)n) * 0.69314718f;
    float p = 1.0f + a * (1.0f + a * (0.5f + a * (1.0f / 6 + a * (1.0f / 24 + a * (1.0f / 120)))));
    return p * ch_from_bits((uint32_t)(n + 127) << 23);
}
CH_SI float ch_log2(float x)
{
    x = ch_max(x, 1e-30f); uint32_t bits = ch_to_bits(x);
    int e = (int)(bits >> 23) - 127;
    float m = ch_from_bits((bits & 0x7fffffu) | 0x3f800000u);
    if (m > 1.41421356f) { m *= 0.5f; ++e; }
    float z = (m - 1) / (m + 1), q = z * z;
    return (float)e + 2.88539008f * z * (1 + q * (1.0f / 3 + q * (1.0f / 5 + q * (1.0f / 7))));
}
/* Semitones to ratio. */
static inline float ch_semis(float s) { return ch_exp2(s * (1.0f / 12.0f)); }
/* sin for x in turns (0..1 wraps). */
static inline float ch_sin_turns(float x)
{
    x -= (float)(int)x; if (x < 0) x += 1;
    float a = x < 0.5f ? x : x - 1.0f;      /* -0.5..0.5 */
    a *= CH_TWO_PI;
    if (a > CH_PI * 0.5f) a = CH_PI - a; else if (a < -CH_PI * 0.5f) a = -CH_PI - a;
    float q = a * a;
    return a * (1 + q * (-1.0f / 6 + q * (1.0f / 120 + q * (-1.0f / 5040 + q * (1.0f / 362880)))));
}
static inline float ch_tanh(float x)
{
    /* Pade 7/6-ish, monotone and bounded; cheaper than exp. */
    if (x > 4.97f) return 1; if (x < -4.97f) return -1;
    float x2 = x * x;
    return x * (135135 + x2 * (17325 + x2 * (378 + x2))) / (135135 + x2 * (62370 + x2 * (3150 + x2 * 28)));
}
static inline float ch_sqrt_fast(float x)
{
    if (x <= 0) return 0;
    float y = ch_from_bits((ch_to_bits(x) >> 1) + 0x1fc00000u);
    y = 0.5f * (y + x / y); return 0.5f * (y + x / y);
}
/* Flush to zero: a value fading away (a filter's state, a follower in silence) reaches the denormal range, where
 * many small FPUs slow down a hundredfold or worse; below 1e-15 (some 300 dB down) it is set to 0 instead */
CH_SI float ch_ftz(float x) { return x > -1e-15f && x < 1e-15f ? 0.0f : x; }
CH_SI float ch_softclip(float x) { x = ch_clamp(x, -1.5f, 1.5f); return x - x * x * x * (4.0f / 27.0f); }
/* the master's ceiling: straight through up to 0.85, then a knee that never reaches 1 (the slope is continuous at the
 * knee). Unlike ch_softclip it adds nothing to a mix that stays under the knee */
CH_SI float ch_knee(float x)
{
    float a = ch_abs(x);
    if (a <= 0.85f) return x;
    float t = (a - 0.85f) * (1.0f / 0.15f), y = 0.85f + 0.15f * t / (1 + t);
    return x < 0 ? -y : y;
}

typedef struct { uint32_t s; } ch_rng_t;
CH_SI uint32_t ch_rand(ch_rng_t *r) { uint32_t n = r->s; n ^= n << 13; n ^= n >> 17; n ^= n << 5; return r->s = n; }
CH_SI float ch_randf(ch_rng_t *r) { return (float)(ch_rand(r) >> 8) * (1.0f / 16777216.0f); }   /* 0..1 */
CH_SI float ch_rands(ch_rng_t *r) { return ch_randf(r) * 2 - 1; }                               /* -1..1 */

/* One-pole parameter smoother; coefficient from a time constant in ms. */
static inline float ch_smooth_coef(float ms, float rate) { return ms <= 0 ? 1 : 1 - ch_exp2(-1.442695f * 1000.0f / (ms * rate)); }

/* Cytomic/Simper TPT state-variable filter: stable under fast modulation. */
typedef struct { float ic1, ic2, g, k, a1; } ch_svf_t;   /* state, and the coefficient for the last g, k */
typedef struct { float lp, bp, hp; } ch_svf_out_t;
static inline float ch_svf_g(float hz) { float w = CH_PI * ch_clamp(hz, 10.0f, 20000.0f) / CH_RATE; return w * (1 + w * w * (1.0f / 3 + w * w * (2.0f / 15 + w * w * (17.0f / 315)))); }
CH_SI ch_svf_out_t ch_svf_tick(ch_svf_t *f, float x, float g, float k)
{
    /* g = tan(pi fc/fs); k = 1/Q (2 = no resonance, ~0 = self-oscillation). The division is done again only when
     * g or k change: most filters keep theirs for a block or more, and on the FM-1 a division every sample in
     * every filter was a good part of what made the audio miss its deadline */
    if (g != f->g || k != f->k || f->a1 == 0) { f->g = g; f->k = k; f->a1 = 1 / (1 + g * (g + k)); }
    float a1 = f->a1, a2 = g * a1, a3 = g * a2;
    float v3 = x - f->ic2;
    float v1 = a1 * f->ic1 + a2 * v3;
    float v2 = f->ic2 + a2 * f->ic1 + a3 * v3;
    f->ic1 = ch_ftz(2 * v1 - f->ic1); f->ic2 = ch_ftz(2 * v2 - f->ic2);
    ch_svf_out_t o = {v2, v1, x - k * v1 - v2};
    return o;
}

/* Cubic Hermite interpolation of four neighbours, t in 0..1. */
static inline float ch_hermite(float xm1, float x0, float x1, float x2, float t)
{
    float c = (x1 - xm1) * 0.5f, v = x0 - x1, w = c + v, a = w + v + (x2 - x0) * 0.5f, b = w + a;
    return ((a * t - b) * t + c) * t + x0;
}

/* Delay line over int16 storage: halves RAM, plenty for wet paths. */
typedef struct { int16_t *buf; unsigned size, mask, write; } ch_delay_t;
static inline void ch_delay_init(ch_delay_t *d, int16_t *buf, unsigned pow2) { d->buf = buf; d->size = pow2; d->mask = pow2 - 1; d->write = 0; memset(buf, 0, pow2 * 2); }
static inline void ch_delay_push(ch_delay_t *d, float x)
{
    float s = ch_clamp(x, -1.0f, 1.0f) * 32767.0f;
    d->buf[d->write] = (int16_t)(s + (s < 0 ? -0.5f : 0.5f)); d->write = (d->write + 1) & d->mask;
}
static inline float ch_delay_tap(const ch_delay_t *d, unsigned samples) { return d->buf[(d->write - 1 - samples) & d->mask] * (1.0f / 32768.0f); }
static inline float ch_delay_read(const ch_delay_t *d, float samples)
{
    int i = (int)samples; float t = samples - (float)i;
    float a = ch_delay_tap(d, (unsigned)i), b = ch_delay_tap(d, (unsigned)i + 1);
    return a + (b - a) * t;
}
#endif
