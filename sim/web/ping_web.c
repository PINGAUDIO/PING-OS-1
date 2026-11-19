/* PING OS-1 in a web page: the whole app (app.c and the engine) as WebAssembly, run inside the page's audio thread
 * (an AudioWorklet: ping-worklet.js), which calls web_process for each stretch of sound and gets the screen, the
 * lights and what is saved back. The same sources as the firmware; only the platform (plat.h) is implemented here,
 * as sim/ping_sim.c does it for Windows. No files, no threads: time is counted in the frames rendered.
 * Built by sim/web/build.ps1 (Emscripten in Docker) into app/web/play/ping-os-1.wasm. */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#define __attribute__(x)
#define PLAT_HOST 1
#include "plat.h"
#include "app.h"

/* what the page gives (ping-worklet.js): a saved object to keep for the next visit */
extern void js_store(uint32_t obj, const void *src, uint32_t len);

/* ---------------------------------------------------------------- display --- */
static uint32_t fb[240 * 240];                 /* R, G, B, A in memory: a canvas's ImageData as it is */
static uint32_t fb_dirty;
static void lcd_sync(void) {}
static uint32_t rgb_of(uint16_t c) { return 0xFF000000u | ((uint32_t)(c & 31) * 255 / 31) << 16 | ((uint32_t)(c >> 5 & 63) * 255 / 63) << 8 | (uint32_t)(c >> 11 & 31) * 255 / 31; }
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    for (uint32_t j = y; j < y + h && j < 240; ++j) for (uint32_t i = x; i < x + w && i < 240; ++i) fb[j * 240 + i] = rgb_of(c);
    fb_dirty = 1;
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    for (uint32_t j = 0; j < h && y + j < 240; ++j)
        for (uint32_t i = 0; i < w && x + i < 240; ++i) {
            uint16_t s = px[j * w + i];
            fb[(y + j) * 240 + x + i] = rgb_of((uint16_t)(s >> 8 | s << 8));     /* the canvas is byte-swapped */
        }
    fb_dirty = 1;
}
#include "gfx.c"

/* the screen reader's hooks: none here */
#define ANN_NOW 1u
static int ann_on(void) { return 0; }
static void ann_say(const char *a, const char *b, uint32_t f) { (void)a; (void)b; (void)f; }
static void ann_tick(uint32_t now) { (void)now; }

/* --------------------------------------------------------------- platform --- */
static uint32_t in_btn, in_keys, tap_btn, tap_keys;       /* tap_*: pressed since the last frame, so a press shorter than a frame still counts */
static int32_t enc_acc[NE];
static uint32_t lights[6];                       /* lit buttons, lit keys, dim buttons, dim keys, mid buttons, mid keys */
static uint64_t frames_done;                     /* the clock: frames of sound rendered */
static uint32_t tick_nudge;
uint32_t plat_ms(void) { return (uint32_t)(frames_done * 1000u / 44100u); }
uint32_t plat_ticks24(void) { tick_nudge += 24u; return (uint32_t)(frames_done * 24000000u / 44100u) + tick_nudge; }
int plat_battery(void) { return 4; }
int32_t plat_battery_raw(void) { return 700; }
/* the flash's data area (samples): memory only, empty at each visit */
static uint8_t flash_area[0x100000]; static int flash_loaded;
static void flash_load(void) { if (!flash_loaded) { flash_loaded = 1; memset(flash_area, 0xFF, sizeof flash_area); } }
const uint8_t *plat_data(uint32_t off) { flash_load(); return PLAT_DATA_OK(off, 1u) ? &flash_area[off] : 0; }
int plat_data_erase(uint32_t off) { flash_load(); if ((off & 0xFFFu) || !PLAT_DATA_OK(off, 0x1000u)) return -1; memset(&flash_area[off], 0xFF, 0x1000u); return 0; }
int plat_data_write(uint32_t off, const void *src, uint32_t n)
{
    flash_load(); if (!PLAT_DATA_OK(off, n)) return -1;
    for (uint32_t i = 0; i < n; ++i) flash_area[off + i] &= ((const uint8_t *)src)[i];
    return 0;
}
int plat_host_read(uint32_t addr, uint8_t *dst, uint32_t n) { (void)addr; (void)dst; (void)n; return -1; }
void plat_sysex_send(const uint8_t *frame, uint32_t n) { (void)frame; (void)n; }
void plat_host_patience(uint32_t a, uint32_t b) { (void)a; (void)b; }
/* the store: each object a block of memory, handed to the page when it is saved and given back at the next visit */
enum { WEB_OBJS = 96 };
static struct { uint8_t *p; uint32_t n; } web_obj[WEB_OBJS];
int web_store_put(uint32_t obj, const void *src, uint32_t len)
{
    if (obj >= WEB_OBJS) return -1;
    uint8_t *p = (uint8_t *)malloc(len ? len : 1u);
    if (!p) return -1;
    memcpy(p, src, len);
    free(web_obj[obj].p); web_obj[obj].p = p; web_obj[obj].n = len;
    return 0;
}
int plat_store_load(uint32_t obj, void *dst, uint32_t max)
{
    if (obj >= WEB_OBJS || !web_obj[obj].p || !web_obj[obj].n) return -1;
    uint32_t n = web_obj[obj].n < max ? web_obj[obj].n : max;
    memcpy(dst, web_obj[obj].p, n);
    return (int)n;
}
int plat_store_save(uint32_t obj, const void *src, uint32_t len)
{
    if (web_store_put(obj, src, len)) return -1;
    js_store(obj, src, len);
    return 0;
}
int plat_store_size(uint32_t obj) { return obj < WEB_OBJS && web_obj[obj].p ? (int)web_obj[obj].n : -1; }
uint32_t plat_buttons(void) { return in_btn | tap_btn; }
uint32_t plat_keys(void) { return in_keys | tap_keys; }
int32_t plat_enc(int role) { int32_t v = enc_acc[role]; enc_acc[role] = 0; return v; }
void plat_lights(uint32_t b, uint32_t k, uint32_t db, uint32_t dk, int glow) { lights[0] = b; lights[1] = k; lights[2] = db; lights[3] = dk; (void)glow; }
void plat_lights_mid(uint32_t b, uint32_t k) { lights[4] = b; lights[5] = k; }

/* MIDI in: a small ring, filled by the page (a keyboard plugged into the computer) */
static uint32_t midi_q[256], midi_w, midi_r;
void plat_midi_send(uint32_t pkt, uint32_t to) { (void)pkt; (void)to; }
uint32_t plat_midi_in_from(void) { return TO_USB; }
void plat_usb_host(int on) { (void)on; }
int plat_usb_host_state(void) { return USBH_OFF; }
int plat_midi_in(uint32_t *pkt, int *port)
{
    if (midi_r == midi_w) return 0;
    *pkt = midi_q[midi_r++ & 255u];
    if (port) *port = PORT_USB;
    return 1;
}
uint32_t plat_cpu_pct(void) { return 0; }
uint32_t plat_xruns(void) { return 0; }
uint32_t plat_audio_block_max(void) { return 0; }
uint32_t plat_audio_gap_max(void) { return 0; }
uint32_t plat_tick_run_max(void) { return 0; }

#include "app.c"

/* ------------------------------------------------------------- the page's calls --- */
enum { WEB_MAX = 2048 };
static float out_lr[2][WEB_MAX];
static int32_t blk[APP_BLOCK * 2];
static uint32_t blk_pos = APP_BLOCK, frame_due;
static float master = 0.8f;

void web_init(void)
{
    palette_set(APP_PALETTE);
    app_init();
    lcd_fill(0, 0, 240, 240, 0);
    app_ui_init();
}
/* n frames at 44.1 kHz into web_out(0) and web_out(1); the panel's frame run every 10 ms of them */
void web_process(uint32_t n)
{
    if (n > WEB_MAX) n = WEB_MAX;
    for (uint32_t i = 0; i < n; ++i) {
        if (blk_pos >= APP_BLOCK) {
            if (frames_done >= frame_due) { app_frame(); tap_btn = tap_keys = 0; frame_due = (uint32_t)frames_done + 441u; }
            app_render(blk, APP_BLOCK, (uint32_t)(master * master * 4096));
            frames_done += APP_BLOCK; blk_pos = 0;
        }
        out_lr[0][i] = (float)blk[blk_pos * 2] * (1.0f / 8388608.0f);
        out_lr[1][i] = (float)blk[blk_pos * 2 + 1] * (1.0f / 8388608.0f);
        ++blk_pos;
    }
}
float *web_out(int ch) { return out_lr[ch & 1]; }
uint32_t *web_fb(void) { return fb; }
uint32_t web_fb_take(void) { uint32_t d = fb_dirty; fb_dirty = 0; return d; }
uint32_t *web_lights(void) { return lights; }
void web_input(uint32_t buttons, uint32_t keys) { in_btn = buttons; in_keys = keys; tap_btn |= buttons; tap_keys |= keys; }
void web_turn(int role, int det) { if (role >= 0 && role < NE) enc_acc[role] += det; }
void web_master(float v) { master = v < 0 ? 0 : v > 1 ? 1 : v; }
void web_midi(uint32_t status, uint32_t d1, uint32_t d2)
{
    uint32_t m = status | d1 << 8 | d2 << 16;
    if (status >= 0x80u && midi_w - midi_r < 255u) midi_q[midi_w++ & 255u] = (m << 8) | (status >> 4);
}
void web_splash(void) { app_splash_replay(); }
void *web_alloc(uint32_t n) { return malloc(n); }
void web_free(void *p) { free(p); }
