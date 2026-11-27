#include "ping_font.h"   /* the wordmark: Jost (app/gen_wordmark.py) */
/* PING OS-1 1.0's boot: the PING Audio logo animation (after the studio's logo film), drawn live,
 * and its sound logo, synthesised. Part of app.c (after gfx.c and fastmath.h are in scope).
 *
 * The picture: rings are born as a dot in the middle and grow outward into concentric circles,
 * one after another, slowing as they go; meanwhile a square closes in from beyond the screen's
 * edges and the rings' centre drifts left, until the square frames them as the PING mark (four
 * half rings 37.35 units apart, centred on the inside of the frame's left edge, in a 173.3-unit
 * frame with lines 5.23 thick) and everything comes to rest. Then the name wipes in on the hit
 * and the tagline comes up in orange. Every frame is computed: one square root a pixel.
 *
 * The sound: the studio's own sound logo, from flash (below); its hit lands on LOGO_HIT. */

enum { LOGO_MS = 4300, LG_RINGS = 6 };
#define LOGO_HIT 2.47f                    /* s: the hit, the name */
#define LG_U (1.0f / 173.3f)              /* the mark's SVG unit, in frames */
#define LG_SETTLE 2.30f                   /* s: everything at rest, the mark complete */

static float lg_smooth(float x) { x = fm_clampf(x, 0, 1); return x * x * (3 - 2 * x); }
static float lg_out(float x) { x = fm_clampf(x, 0, 1); x = 1 - x; return 1 - x * x * x; }
static float lg_mix(float a, float b, float m) { return a + (b - a) * m; }

typedef struct {
    float cx, cy, S, w;                   /* the frame: centre, outer size, line width (px) */
    float rx, ry;                         /* the rings' centre */
    float r[LG_RINGS];                    /* radii (line centres), < 0 not born yet */
    float a0;                             /* the first dot's fade in */
} lg_state;

static void lg_at(float t, lg_state *g)
{
    const float S_end = 120, X = 120, Y = 78;        /* the mark 120 px wide (18..138): the wordmark and tagline match it */
    float sp = 37.35f * LG_U * S_end;                         /* the mark's ring spacing, px */
    g->w = 5.23f * LG_U * S_end;
    g->cx = X; g->cy = lg_mix(120, Y, lg_smooth((t - 0.6f) / (LG_SETTLE - 0.6f)));   /* from the screen's middle up to the mark's place */
    /* the square: from well outside the screen to the mark's frame */
    g->S = lg_mix(470, S_end, lg_smooth((t - 0.25f) / (LG_SETTLE - 0.25f)));
    /* the rings' centre: from the middle to the inside of the frame's left edge */
    float shift = lg_smooth((t - 0.6f) / (LG_SETTLE - 0.6f));
    g->rx = lg_mix(X, X - S_end * 0.5f + g->w, shift); g->ry = g->cy;
    /* the rings: born at the centre one after another, slowing to rest on the mark's radii */
    float x = fm_clampf((t - 0.2f) / (LG_SETTLE - 0.2f), 0, 1);
    float p = (float)LG_RINGS * (1 - (1 - x) * (1 - x));
    for (int k = 0; k < LG_RINGS; ++k) {
        float age = p - (float)k;                             /* in spacings */
        g->r[k] = age < 0 ? -1 : age * sp - 2.615f * LG_U * S_end * shift;
        if (g->r[k] < 0 && age >= 0) g->r[k] = 0;
    }
    g->a0 = fm_clampf(t / 0.15f, 0, 1);
}

enum { WORD_Y = 146, TAG_Y = 176 };   /* the name under the mark (it ends at 138), the tagline; v1.0 in the corner */
#define APP_VERSION_SHORT "1.0.2"
static void logo_frame(uint32_t ms)
{
    float t = (float)ms * 0.001f;
    lg_state g; lg_at(t, &g);
    float hw = g.w * 0.5f + 0.5f;
    float in_half = g.S * 0.5f - g.w;                          /* the rings show inside the frame */
    for (int32_t band = 0; band < 3; ++band) {
        int32_t y0 = band * 80;
        cv_begin(240, 80, C_BLACK);
        for (int32_t yy = 0; yy < 80; ++yy) {
            float y = (float)(y0 + yy) + 0.5f, dy = y - g.ry, fy = fm_fabsf(y - g.cy);
            for (int32_t xx = 0; xx < 240; ++xx) {
                float x = (float)xx + 0.5f, dx = x - g.rx, fx = fm_fabsf(x - g.cx);
                float q = fm_maxf(fx, fy) - (g.S * 0.5f - g.w * 0.5f);
                float cov = hw - fm_fabsf(q);                     /* the square */
                if (fx < in_half && fy < in_half) {
                    float d = fm_sqrtf(dx * dx + dy * dy);
                    for (int k = 0; k < LG_RINGS; ++k)
                        if (g.r[k] >= 0) cov = fm_maxf(cov, (hw - fm_fabsf(d - g.r[k])) * (k ? 1 : g.a0));
                }
                if (cov > 0.03f) {
                    uint32_t v = (uint32_t)(fm_minf(cov, 1) * 245.0f);
                    cv_pset(xx, yy, RGB(v, v, v));
                }
            }
        }
        /* the name wipes in on the hit; the tagline comes up after it */
        int32_t oy = -y0;
        if (t >= LOGO_HIT) {
            /* PING OS-1 in white, under the mark and exactly as wide as it (Jost 26 px: app/gen_wordmark.py) */
            const char *nm = "PING OS-1";
            int32_t tw = text_w(&FONT_WORD, nm), x0 = (240 - tw) / 2 + 2;   /* (+2: its ink, not its advance, edge to edge with the mark, 60..179) */
            cv_text(x0, WORD_Y + oy, &FONT_WORD, nm, C_WHITE);
            int32_t wipe = x0 + (int32_t)((float)(tw + 4) * lg_out((t - LOGO_HIT) / 0.30f));
            if (wipe < 240) cv_rect(wipe, WORD_Y + oy, 240 - wipe, FONT_WORD.h, C_BLACK);
        }
        float ta = fm_clampf((t - 2.75f) / 0.45f, 0, 1);
        if (ta > 0) {                                      /* the version, as small as it goes, in the corner */
            /* the version and the build's stamp, MMDD.HHMM from the compiler's clock: which firmware this is */
            static const char MON[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
            const char *dt = __DATE__, *tm = __TIME__; unsigned mo = 0;
            for (unsigned k = 0; k < 12; ++k) if (dt[0] == MON[k * 3] && dt[1] == MON[k * 3 + 1] && dt[2] == MON[k * 3 + 2]) mo = k + 1;
            char fn[24] = "v" APP_VERSION_SHORT "  ";
            unsigned n = 0; while (fn[n]) ++n;
            fn[n++] = (char)('0' + mo / 10); fn[n++] = (char)('0' + mo % 10);
            fn[n++] = dt[4] == ' ' ? '0' : dt[4]; fn[n++] = dt[5]; fn[n++] = '.';
            fn[n++] = tm[0]; fn[n++] = tm[1]; fn[n++] = tm[3]; fn[n++] = tm[4]; fn[n] = 0;
            cv_text(240 - text_w(&FONT_FOOT, fn) - 3, 240 - FONT_FOOT.h - 1 + oy, &FONT_FOOT, fn, RGB((uint32_t)(140 * ta), (uint32_t)(140 * ta), (uint32_t)(140 * ta)));
            const char *tg = APP_TAGLINE;
            /* solid orange, the theme's accent, fading up */
            int32_t ww = text_w(&FONT_TAG, tg);
            cv_text((240 - ww) / 2, TAG_Y + oy, &FONT_TAG, tg, RGB((uint32_t)(255 * ta), (uint32_t)(132 * ta), (uint32_t)(36 * ta)));
        }
        cv_blit(0, (uint32_t)y0);
    }
}

/* ---- the sound logo: the studio's recording (app/sound_logo.h, tools/make_sound_logo.py), mono ADPCM read
 * straight out of flash, 64-frame blocks decoded as they come, resampled to the output rate by interpolation ---- */
#include "ch_adpcm.h"
#include "sound_logo.h"
static struct {
    volatile uint8_t start, stop;         /* from the UI */
    uint8_t on;
    float pos, fade;                      /* frames into the recording (its own rate) */
    int32_t blk;                          /* the block decoded in cur[], then nxt[] holds the one after */
    int16_t cur[CH_ADPCM_BLOCK], nxt[CH_ADPCM_BLOCK];
} lgs;
static void logo_sound_start(void) { lgs.start = 1; }
static void logo_sound_stop(void) { lgs.stop = 1; }
static void lg_load(int32_t b)
{
    const uint32_t nb = (SLOGO_FRAMES + CH_ADPCM_BLOCK - 1) / CH_ADPCM_BLOCK;
    lgs.blk = b;
    if ((uint32_t)b < nb) ch_adpcm_decode_block(slogo + (uint32_t)b * CH_ADPCM_BLOCK_BYTES, lgs.cur);
    else memset(lgs.cur, 0, sizeof lgs.cur);
    if ((uint32_t)b + 1u < nb) ch_adpcm_decode_block(slogo + ((uint32_t)b + 1u) * CH_ADPCM_BLOCK_BYTES, lgs.nxt);
    else memset(lgs.nxt, 0, sizeof lgs.nxt);
}
/* the recording is mono (flash): its place between left and right follows the picture and the lights instead. It
 * leans left as the rings' centre drifts left (0.6 s .. LG_SETTLE), runs left to right with the light over the keys
 * (the power-on sweep, started 60 ms before the hit: its head on F3 22 ms in, on G5 449 ms in, at a steady speed),
 * then comes back to the middle. Gentle: 0.38 of the way at most (3.5 dB between the sides), the power the same. */
#define LG_PAN_L (-0.30f)
#define LG_PAN_R 0.38f
static float lg_pan(float t)
{
    const float s0 = LOGO_HIT - 0.060f + 0.022f, s1 = LOGO_HIT - 0.060f + 0.449f;
    if (t < s0) return LG_PAN_L * lg_smooth((t - 0.6f) / (LG_SETTLE - 0.6f));
    if (t < s1) return lg_mix(LG_PAN_L, LG_PAN_R, (t - s0) / (s1 - s0));
    return LG_PAN_R * (1 - lg_smooth((t - s1) / 1.0f));
}
static void logo_sound(int32_t *lr, uint32_t n)
{
    if (lgs.start) { lgs.start = 0; lgs.stop = 0; lgs.on = 1; lgs.pos = 0; lgs.fade = 1; lg_load(0); }
    if (!lgs.on) return;
    const float step = (float)SLOGO_RATE / 44100.0f;
    float pan = lg_pan(lgs.pos * (1.0f / (float)SLOGO_RATE));      /* (a block at a time: it moves 0.005 a block at its fastest) */
    const float gl = fm_sqrtf(1 - pan) * 8388607.0f, gr = fm_sqrtf(1 + pan) * 8388607.0f;
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t k = (uint32_t)lgs.pos;
        if (k + 1u >= SLOGO_FRAMES) { lgs.on = 0; return; }
        int32_t b = (int32_t)(k / CH_ADPCM_BLOCK);
        if (b != lgs.blk) lg_load(b);
        uint32_t o = k % CH_ADPCM_BLOCK;
        float f = lgs.pos - (float)k;
        float a = lgs.cur[o], z = o + 1u < CH_ADPCM_BLOCK ? lgs.cur[o + 1u] : lgs.nxt[0];
        float v = (a + (z - a) * f) * (1.0f / 32768.0f);
        if (lgs.stop) { lgs.fade *= 0.9985f; if (lgs.fade < 1e-3f) { lgs.on = 0; lgs.stop = 0; return; } }
        v *= lgs.fade;
        lr[2 * i] += (int32_t)(v * gl); lr[2 * i + 1] += (int32_t)(v * gr);
        lgs.pos += step;
    }
}
