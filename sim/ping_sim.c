/* PING FM1 1.0 simulator for Windows: the whole app (app.c and the engine) on the
 * PC, with the FM-1's screen, panel and keys in a window, sound out of the
 * default device and any MIDI input. The same sources as the firmware; only
 * the platform (plat.h) is implemented here.
 *
 * Mouse: click buttons and keys (right-click latches one held, to combine it with
 * a knob); turn knobs with the wheel or by dragging up and down.
 * Keyboard: see HELP below. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#define __attribute__(x)
#define PLAT_HOST 1
#include "plat.h"
#include "app.h"

/* ---------------------------------------------------------------- display --- */
static uint32_t fb[240 * 240];                 /* 0x00RRGGBB */
static volatile LONG fb_dirty;
static void lcd_sync(void) {}
static uint32_t rgb_of(uint16_t c) { return ((uint32_t)(c >> 11 & 31) * 255 / 31) << 16 | ((uint32_t)(c >> 5 & 63) * 255 / 63) << 8 | (uint32_t)(c & 31) * 255 / 31; }
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
static volatile uint32_t btn_mouse, btn_kbd, btn_latch, key_mouse, key_kbd, key_latch;
static volatile LONG enc_acc[NE];
static uint32_t lit_b, lit_k, dim_b, dim_k;
uint32_t plat_ms(void) { return GetTickCount(); }
static int sim_batt = 2;                         /* the battery shown (script: batt N, -1..4) */
int plat_battery(void) { return sim_batt; }
static int32_t sim_braw = 575;                   /* the divider reading shown (script: braw N) */
int32_t plat_battery_raw(void) { return sim_braw; }
/* the flash's data area (plat_data: samples), a file beside the simulator (store/flash.bin), NOR-like */
static uint8_t flash_area[0x100000]; static int flash_loaded;
static void flash_load(void)
{
    if (flash_loaded) return;
    flash_loaded = 1; memset(flash_area, 0xFF, sizeof flash_area);
    FILE *f = fopen("store/flash.bin", "rb"); if (f) { fread(flash_area, 1, sizeof flash_area, f); fclose(f); }
}
static void flash_save(void) { CreateDirectoryA("store", NULL); FILE *f = fopen("store/flash.bin", "wb"); if (f) { fwrite(flash_area, 1, sizeof flash_area, f); fclose(f); } }
const uint8_t *plat_data(uint32_t off) { flash_load(); return PLAT_DATA_OK(off, 1u) ? &flash_area[off] : 0; }
int plat_data_erase(uint32_t off) { flash_load(); if ((off & 0xFFFu) || !PLAT_DATA_OK(off, 0x1000u)) return -1; memset(&flash_area[off], 0xFF, 0x1000u); flash_save(); return 0; }
int plat_data_write(uint32_t off, const void *src, uint32_t n)
{
    flash_load(); if (!PLAT_DATA_OK(off, n)) return -1;
    for (uint32_t i = 0; i < n; ++i) flash_area[off + i] &= ((const uint8_t *)src)[i];
    flash_save(); return 0;
}
int plat_host_read(uint32_t addr, uint8_t *dst, uint32_t n) { (void)addr; (void)dst; (void)n; return -1; }
static uint8_t sx_last[512]; static uint32_t sx_n; static int sx_keep;   /* (pxdump: the last frame, kept in memory) */
void plat_sysex_send(const uint8_t *frame, uint32_t n)
{
    if (sx_keep) { sx_n = n < sizeof sx_last ? n : 0; memcpy(sx_last, frame, sx_n); return; }
    FILE *o = fopen("sysex.bin", "ab"); if (o) { fwrite(frame, 1, n, o); fclose(o); }   /* (a script's `usbcmd`: what the app sends, kept) */
}
void app_usb_cmd(const uint8_t *d, uint32_t n);
void plat_host_patience(uint32_t a, uint32_t b) { (void)a; (void)b; }
/* the store: an object a file beside the simulator (store\objN.bin), as the FM-1's flash objects */
static const char *store_path(uint32_t obj, char *b) { sprintf(b, "store/obj%u.bin", (unsigned)obj); return b; }
int plat_store_load(uint32_t obj, void *dst, uint32_t max)
{
    char b[64]; FILE *f = fopen(store_path(obj, b), "rb");
    if (!f) return -1;
    int n = (int)fread(dst, 1, max, f); fclose(f);
    return n > 0 ? n : -1;
}
int plat_store_save(uint32_t obj, const void *src, uint32_t len)
{
    char b[64]; CreateDirectoryA("store", NULL);
    FILE *f = fopen(store_path(obj, b), "wb");
    if (!f) return -1;
    size_t w = fwrite(src, 1, len, f); fclose(f);
    fprintf(stderr, "store: obj %u, %u of %u bytes -> %s\n", (unsigned)obj, (unsigned)w, (unsigned)len, b);
    return w == len ? 0 : -1;
}
int plat_store_size(uint32_t obj) { char b[64]; FILE *f = fopen(store_path(obj, b), "rb"); if (!f) return -1; fseek(f, 0, SEEK_END); int n = (int)ftell(f); fclose(f); return n; }
uint32_t plat_ticks24(void)
{
    static LARGE_INTEGER fq; LARGE_INTEGER c;
    if (!fq.QuadPart) QueryPerformanceFrequency(&fq);
    QueryPerformanceCounter(&c);
    return (uint32_t)((double)c.QuadPart * 24e6 / (double)fq.QuadPart);
}
uint32_t plat_buttons(void) { return btn_mouse | btn_kbd | btn_latch; }
uint32_t plat_keys(void) { return key_mouse | key_kbd | key_latch; }
int32_t plat_enc(int role) { return (int32_t)InterlockedExchange(&enc_acc[role], 0); }
void plat_lights(uint32_t b, uint32_t k, uint32_t db, uint32_t dk, int glow) { lit_b = b; lit_k = k; dim_b = db; dim_k = dk; (void)glow; }
static uint32_t mid_b, mid_k;
void plat_lights_mid(uint32_t b, uint32_t k) { mid_b = b; mid_k = k; }

/* MIDI in: a small ring from the driver's thread */
static uint32_t midi_q[256];
static volatile LONG midi_w, midi_r;
static volatile LONG clk_out, clk_fa, clk_fc;                 /* what the app sent: clocks, starts, stops (the script's "clockout") */
void plat_midi_send(uint32_t pkt, uint32_t to) { (void)to; uint32_t st = pkt >> 8 & 0xFFu; if (st == 0xF8u) InterlockedIncrement(&clk_out); else if (st == 0xFAu) InterlockedIncrement(&clk_fa); else if (st == 0xFCu) InterlockedIncrement(&clk_fc); }
uint32_t plat_midi_in_from(void) { return TO_USB; }
void plat_usb_host(int on) { (void)on; }
int plat_usb_host_state(void) { return USBH_OFF; }
static LARGE_INTEGER gen_t0; static int gen_bpm10, gen_ms;                                 /* the script's "clockgen": a master, start + clocks at a tempo for a while, then stop */
static DWORD WINAPI gen_thread(LPVOID p);
int plat_midi_in(uint32_t *pkt, int *port)
{
    if (midi_r == midi_w) return 0;
    *pkt = midi_q[midi_r & 255]; InterlockedIncrement(&midi_r);
    if (port) *port = PORT_USB;
    return 1;
}
static void CALLBACK midi_cb(HMIDIIN h, UINT msg, DWORD_PTR inst, DWORD_PTR p1, DWORD_PTR p2)
{
    (void)h; (void)inst; (void)p2;
    if (msg != MIM_DATA) return;
    uint32_t m = (uint32_t)p1, st = m & 0xFF;
    if (st < 0x80) return;
    uint32_t pkt = (m << 8) | (st >> 4);
    if (midi_w - midi_r < 255) { midi_q[midi_w & 255] = pkt; InterlockedIncrement(&midi_w); }
}

#include "app.c"
/* gfx.c's RGB() is RGB565; the window below wants Windows' COLORREF */
#undef RGB
#define RGB(r, g, b) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))

/* ------------------------------------------------------------------ audio --- */
enum { NBUF = 4, FRAMES = 512 };
static HWAVEOUT wo;
static WAVEHDR hdr[NBUF];
static int16_t pcm[NBUF][FRAMES * 2];
static HANDLE audio_ev;
static volatile int running = 1;
static float master = 0.8f;
/* the render's load, as the device reports it (plat_cpu_pct: % of a block's time; here the PC's, much faster) */
static volatile uint32_t sim_cpu, sim_xruns;
uint32_t plat_cpu_pct(void) { return sim_cpu; }
uint32_t plat_xruns(void) { return sim_xruns; }
uint32_t plat_audio_block_max(void) { return 0; }
uint32_t plat_audio_gap_max(void) { return 0; }
uint32_t plat_tick_run_max(void) { return 0; }
static double sim_blkpeak;
static void timed_render(int32_t *blk, uint32_t g)
{
    static LARGE_INTEGER fq; LARGE_INTEGER a, b;
    if (!fq.QuadPart) QueryPerformanceFrequency(&fq);
    QueryPerformanceCounter(&a);
    app_render(blk, APP_BLOCK, g);
    { double m = 0; for (int i = 0; i < APP_BLOCK * 2; ++i) { double v = blk[i] / 8388608.0; if (v < 0) v = -v; if (v > m) m = v; } if (m > sim_blkpeak) sim_blkpeak = m; }
    QueryPerformanceCounter(&b);
    double used = (double)(b.QuadPart - a.QuadPart) / (double)fq.QuadPart, budget = (double)APP_BLOCK / 44100.0;
    uint32_t pct = (uint32_t)(100.0 * used / budget + 0.5);
    sim_cpu = (sim_cpu * 7u + pct) / 8u;
    if (pct >= 100u) ++sim_xruns;
}
static DWORD WINAPI audio_thread(void *arg)
{
    (void)arg;
    int32_t blk[APP_BLOCK * 2];
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    while (running) {
        for (int b = 0; b < NBUF; ++b) {
            if (!(hdr[b].dwFlags & WHDR_DONE) && (hdr[b].dwFlags & WHDR_PREPARED)) continue;
            for (int f = 0; f < FRAMES; f += APP_BLOCK) {
                uint32_t g = (uint32_t)(master * master * 4096);
                timed_render(blk, g);
                for (int i = 0; i < APP_BLOCK * 2; ++i) {
                    int32_t v = blk[i] >> 8;
                    pcm[b][f * 2 + i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
                }
            }
            hdr[b].dwFlags &= ~WHDR_DONE;
            waveOutWrite(wo, &hdr[b], sizeof hdr[b]);
        }
        WaitForSingleObject(audio_ev, 20);
    }
    return 0;
}
static void audio_start(void)
{
    WAVEFORMATEX wf = {WAVE_FORMAT_PCM, 2, 44100, 44100 * 4, 4, 16, 0};
    audio_ev = CreateEvent(0, FALSE, FALSE, 0);
    if (waveOutOpen(&wo, WAVE_MAPPER, &wf, (DWORD_PTR)audio_ev, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
        MessageBoxA(0, "No audio output could be opened.", "PING FM1 1.0", MB_OK);
        return;
    }
    for (int b = 0; b < NBUF; ++b) {
        memset(&hdr[b], 0, sizeof hdr[b]);
        hdr[b].lpData = (LPSTR)pcm[b]; hdr[b].dwBufferLength = sizeof pcm[b];
        waveOutPrepareHeader(wo, &hdr[b], sizeof hdr[b]);
        hdr[b].dwFlags |= WHDR_DONE;
    }
    CreateThread(0, 0, audio_thread, 0, 0, 0);
}

/* ------------------------------------------------------------------ panel --- */
#define SCALE 2
enum { SX = 16, SY = 16, WIN_W = 940, WIN_H = 660 };
typedef struct { int x, y, r, role; const char *name; } knob_t;
static const knob_t KNOBS[NE] = {
    {600, 70, 30, EN_SELECT, "SELECT"}, {700, 70, 24, EN_ALGO, "ALGO"}, {790, 70, 24, EN_PRESET, "PRESET"},
    {580, 170, 24, EN_K1, "K1"}, {670, 170, 24, EN_K2, "K2"}, {760, 170, 24, EN_K3, "K3"}, {850, 170, 24, EN_K4, "K4"}};
static int knob_pos[NE];
typedef struct { int x, y, b; const char *name, *key; } button_t;
static const button_t BUTTONS[NB] = {
    {540, 230, B_EDIT, "EDIT", "F1"}, {620, 230, B_ARP, "ARP", "F2"}, {700, 230, B_SEL, "SEL", "F3"}, {780, 230, B_ENV, "ENV", "F4"}, {860, 230, B_FX, "FX", "F5"},
    {540, 290, B_LFO, "LFO", "F6"}, {620, 290, B_GLO, "GLO", "F7"}, {700, 290, B_HOME, "HOME", "F8"}, {780, 290, B_SAVE, "SAVE", "F9"},
    {540, 350, B_SEQ, "SEQ", "TAB"}, {620, 350, B_PLAY, "PLAY", "SPACE"}, {700, 350, B_REC, "REC", "ENTER"},
    {780, 350, B_OCTDN, "OCT-", "PGDN"}, {860, 350, B_OCTUP, "OCT+", "PGUP"}};
enum { BW = 72, BH = 46 };
enum { KX = 16, KY = 520, KW = 56, KH = 120 };
static RECT key_rect(int k)
{
    static const uint8_t wk[16] = PLAT_WHITE_KEYS;
    RECT r;
    for (int w = 0; w < 16; ++w) if (wk[w] == k) { r.left = KX + w * KW; r.top = KY; r.right = r.left + KW - 2; r.bottom = KY + KH; return r; }
    /* a black key sits between the white keys either side of it */
    int below = 0;
    for (int w = 0; w < 16; ++w) if (wk[w] < k) below = w;
    r.left = KX + below * KW + KW - 17; r.right = r.left + 32; r.top = KY; r.bottom = KY + 72;
    return r;
}
static int is_black(int k) { static const uint8_t bk[11] = PLAT_BLACK_KEYS; for (int i = 0; i < 11; ++i) if (bk[i] == k) return 1; return 0; }

static const char *HELP =
    "KEYS  1-8 / Q-I: white keys 1-16    A S D F G H J K L ; ': black keys\r\n"
    "SPACE play   ENTER rec   TAB seq (hold)   F1-F5 slots   F6 lfo  F7 glo  F8 home  F9 save\r\n"
    "F12 boot screen   PGDN/PGUP octave   [ ] PRESET: track   ARROWS: select knob   Z/X C/V B/N M/,  K1-K4   ./ algo   [ ] preset\r\n"
    "Mouse: wheel or drag knobs; right-click latches a button or key";

static HWND hwnd;
static int drag_knob = -1, drag_y, drag_acc, mouse_btn = -1, mouse_key = -1;

static void draw(HDC dc)
{
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, WIN_W, WIN_H);
    HGDIOBJ old = SelectObject(mem, bmp);
    HBRUSH bg = CreateSolidBrush(RGB(24, 24, 28));
    RECT all = {0, 0, WIN_W, WIN_H}; FillRect(mem, &all, bg); DeleteObject(bg);
    /* the screen, scaled */
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = 240; bi.bmiHeader.biHeight = -240;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    SetStretchBltMode(mem, COLORONCOLOR);
    StretchDIBits(mem, SX, SY, 240 * SCALE, 240 * SCALE, 0, 0, 240, 240, fb, &bi, DIB_RGB_COLORS, SRCCOPY);
    SetBkMode(mem, TRANSPARENT);
    HFONT f = CreateFontA(15, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, 0, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    HFONT fs = CreateFontA(12, 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    SelectObject(mem, f);
    /* knobs */
    for (int i = 0; i < NE; ++i) {
        const knob_t *k = &KNOBS[i];
        HBRUSH kb = CreateSolidBrush(RGB(52, 52, 58)); SelectObject(mem, kb);
        HPEN kp = CreatePen(PS_SOLID, 2, RGB(90, 90, 98)); SelectObject(mem, kp);
        Ellipse(mem, k->x - k->r, k->y - k->r, k->x + k->r, k->y + k->r);
        double a = knob_pos[i] * 0.2618 - 1.5708;
        HPEN ip = CreatePen(PS_SOLID, 3, RGB(0, 174, 239)); SelectObject(mem, ip);
        MoveToEx(mem, k->x, k->y, 0); LineTo(mem, k->x + (int)(cos(a) * (k->r - 4)), k->y + (int)(sin(a) * (k->r - 4)));
        DeleteObject(kb); DeleteObject(kp); DeleteObject(ip);
        SetTextColor(mem, RGB(170, 170, 178));
        RECT t = {k->x - 40, k->y + k->r + 2, k->x + 40, k->y + k->r + 20};
        DrawTextA(mem, k->name, -1, &t, DT_CENTER);
    }
    /* buttons */
    for (int i = 0; i < NB; ++i) {
        const button_t *b = &BUTTONS[i];
        uint32_t bit = 1u << b->b;
        COLORREF c = (lit_b & bit) ? (b->b == B_PLAY ? RGB(220, 50, 40) : RGB(235, 235, 240))
                   : (b->b == B_PLAY && (lit_b & LED_PLAY_GREEN)) ? RGB(40, 200, 80)
                   : (mid_b & bit) ? RGB(88, 88, 94) : (dim_b & bit) ? RGB(60, 60, 66) : RGB(40, 40, 44);
        if (btn_latch & bit) c = RGB(255, 132, 36);
        HBRUSH bb = CreateSolidBrush(c);
        RECT r = {b->x, b->y, b->x + BW, b->y + BH};
        FillRect(mem, &r, bb); DeleteObject(bb);
        int light = (lit_b & bit) && b->b != B_PLAY;          /* a lit (white) button: dark lettering */
        SetTextColor(mem, light ? RGB(20, 20, 24) : RGB(240, 240, 244));
        RECT t = r; t.top += 6; SelectObject(mem, f); DrawTextA(mem, b->name, -1, &t, DT_CENTER);
        SetTextColor(mem, light ? RGB(70, 70, 76) : RGB(150, 150, 158));
        t.top += 18; SelectObject(mem, fs); DrawTextA(mem, b->key, -1, &t, DT_CENTER);
    }
    /* keys: white then black */
    uint32_t held = plat_keys();
    for (int pass = 0; pass < 2; ++pass)
        for (int k = 0; k < NKEYS; ++k) {
            if (is_black(k) != pass) continue;
            RECT r = key_rect(k);
            uint32_t bit = 1u << k;
            /* the FM-1's key LEDs are white: off, dim, full (the keys drawn darker so the light shows) */
            COLORREF c = pass ? RGB(20, 20, 22) : RGB(118, 118, 122);
            if (dim_k & bit) c = pass ? RGB(92, 92, 96) : RGB(176, 176, 180);
            if (lit_k & bit) c = RGB(255, 255, 255);
            if (held & bit) c = RGB(255, 132, 36);
            if (key_latch & bit) c = RGB(170, 80, 20);
            HBRUSH kb = CreateSolidBrush(c); FillRect(mem, &r, kb); DeleteObject(kb);
        }
    SetTextColor(mem, RGB(150, 150, 158)); SelectObject(mem, fs);
    RECT h = {540, 410, WIN_W - 10, 510};
    DrawTextA(mem, HELP, -1, &h, DT_LEFT | DT_WORDBREAK);
    char mv[64]; snprintf(mv, sizeof mv, "MASTER %d%%  (- / = keys)", (int)(master * 100 + 0.5f));
    RECT m = {540, 490, WIN_W - 10, 508}; DrawTextA(mem, mv, -1, &m, DT_LEFT);
    BitBlt(dc, 0, 0, WIN_W, WIN_H, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); DeleteObject(f); DeleteObject(fs);
}

static int hit_knob(int x, int y) { for (int i = 0; i < NE; ++i) { int dx = x - KNOBS[i].x, dy = y - KNOBS[i].y; if (dx * dx + dy * dy <= KNOBS[i].r * KNOBS[i].r + 200) return i; } return -1; }
static int hit_button(int x, int y) { for (int i = 0; i < NB; ++i) if (x >= BUTTONS[i].x && x < BUTTONS[i].x + BW && y >= BUTTONS[i].y && y < BUTTONS[i].y + BH) return BUTTONS[i].b; return -1; }
static int hit_key(int x, int y)
{
    POINT p = {x, y};
    for (int k = 0; k < NKEYS; ++k) if (is_black(k)) { RECT r = key_rect(k); if (PtInRect(&r, p)) return k; }
    for (int k = 0; k < NKEYS; ++k) if (!is_black(k)) { RECT r = key_rect(k); if (PtInRect(&r, p)) return k; }
    return -1;
}
static void turn(int i, int det) { InterlockedExchangeAdd(&enc_acc[KNOBS[i].role], det); knob_pos[i] += det; }

/* computer keys -> panel */
static int vk_button(WPARAM vk)
{
    switch (vk) {
    case VK_F1: return B_EDIT; case VK_F2: return B_ARP; case VK_F3: return B_SEL; case VK_F4: return B_ENV; case VK_F5: return B_FX;
    case VK_F6: return B_LFO; case VK_F7: return B_GLO; case VK_F8: return B_HOME; case VK_F9: return B_SAVE;
    case VK_TAB: return B_SEQ; case VK_SPACE: return B_PLAY; case VK_RETURN: return B_REC;
    case VK_NEXT: return B_OCTDN; case VK_PRIOR: return B_OCTUP;
    }
    return -1;
}
static int vk_key(WPARAM vk)
{
    static const uint8_t wk[16] = PLAT_WHITE_KEYS, bk[11] = PLAT_BLACK_KEYS;
    static const char W[] = "12345678QWERTYUI", B[] = "ASDFGHJKL";
    for (int i = 0; i < 16; ++i) if ((WPARAM)W[i] == vk) return wk[i];
    for (int i = 0; i < 9; ++i) if ((WPARAM)B[i] == vk) return bk[i];
    if (vk == VK_OEM_1) return bk[9];
    if (vk == VK_OEM_7) return bk[10];
    return -1;
}
static int vk_knob(WPARAM vk, int *det)
{
    switch (vk) {
    case VK_LEFT: *det = -1; return 0; case VK_RIGHT: *det = 1; return 0;
    case 'Z': *det = -1; return 3; case 'X': *det = 1; return 3;
    case 'C': *det = -1; return 4; case 'V': *det = 1; return 4;
    case 'B': *det = -1; return 5; case 'N': *det = 1; return 5;
    case 'M': *det = -1; return 6; case VK_OEM_COMMA: *det = 1; return 6;
    case VK_OEM_PERIOD: *det = -1; return 1; case VK_OEM_2: *det = 1; return 1;
    case VK_OEM_4: *det = -1; return 2; case VK_OEM_6: *det = 1; return 2;
    }
    return -1;
}

static LRESULT CALLBACK wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    int x = (short)LOWORD(lp), y = (short)HIWORD(lp), i, det;
    switch (msg) {
    case WM_TIMER:
        app_frame();
        InvalidateRect(h, 0, FALSE);
        return 0;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); draw(dc); EndPaint(h, &ps); return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_LBUTTONDOWN:
        SetCapture(h);
        if ((i = hit_knob(x, y)) >= 0) { drag_knob = i; drag_y = y; drag_acc = 0; return 0; }
        if ((i = hit_button(x, y)) >= 0) { mouse_btn = i; btn_mouse |= 1u << i; return 0; }
        if ((i = hit_key(x, y)) >= 0) { mouse_key = i; key_mouse |= 1u << i; }
        return 0;
    case WM_MOUSEMOVE:
        if (drag_knob >= 0) {
            drag_acc += drag_y - y; drag_y = y;
            while (drag_acc >= 8) { turn(drag_knob, 1); drag_acc -= 8; }
            while (drag_acc <= -8) { turn(drag_knob, -1); drag_acc += 8; }
        }
        return 0;
    case WM_LBUTTONUP:
        ReleaseCapture();
        drag_knob = -1;
        if (mouse_btn >= 0) { btn_mouse &= ~(1u << mouse_btn); mouse_btn = -1; }
        if (mouse_key >= 0) { key_mouse &= ~(1u << mouse_key); mouse_key = -1; }
        return 0;
    case WM_RBUTTONDOWN:
        if ((i = hit_button(x, y)) >= 0) btn_latch ^= 1u << i;
        else if ((i = hit_key(x, y)) >= 0) key_latch ^= 1u << i;
        return 0;
    case WM_MOUSEWHEEL: {
        POINT p = {x, y}; ScreenToClient(h, &p);
        if ((i = hit_knob(p.x, p.y)) >= 0) turn(i, GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 1 : -1);
        return 0;
    }
    case WM_KEYDOWN: case WM_SYSKEYDOWN:
        if (wp == VK_F12) { app_splash_replay(); return 0; }
        if (wp == VK_OEM_MINUS) { master = master > 0.05f ? master - 0.05f : 0; return 0; }
        if (wp == VK_OEM_PLUS) { master = master < 0.95f ? master + 0.05f : 1; return 0; }
        if ((i = vk_knob(wp, &det)) >= 0) { turn(i, det); return 0; }
        if (lp & (1 << 30)) return 0;                        /* auto-repeat */
        if ((i = vk_button(wp)) >= 0) { btn_kbd |= 1u << i; return 0; }
        if ((i = vk_key(wp)) >= 0) { key_kbd |= 1u << i; return 0; }
        if (wp == VK_ESCAPE) { btn_latch = 0; key_latch = 0; }
        return msg == WM_SYSKEYDOWN ? DefWindowProcA(h, msg, wp, lp) : 0;
    case WM_KEYUP: case WM_SYSKEYUP:
        if ((i = vk_button(wp)) >= 0) { btn_kbd &= ~(1u << i); return 0; }
        if ((i = vk_key(wp)) >= 0) { key_kbd &= ~(1u << i); return 0; }
        return 0;
    case WM_DESTROY:
        running = 0;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, msg, wp, lp);
}

/* ---------------------------------------------------------- script mode ---
 * PING-FM1-sim.exe --script FILE  runs without a window: one command a line
 *   wait MS | tap BTN | press BTN | release BTN | key K down|up | tapkey W (white 1-16)
 *   turn ENC N (SELECT ALGO PRESET K1..K4) | shot FILE.bmp | wav FILE.wav (from here on)
 * so the screens and sounds can be checked from the command line. */
static FILE *wav_f; static uint32_t wav_frames;
static void run_for(uint32_t ms)
{
    uint32_t end = GetTickCount() + ms;
    int32_t blk[APP_BLOCK * 2];
    static double owed;
    while ((int32_t)(end - GetTickCount()) > 0) {
        app_frame();
        { static LARGE_INTEGER qf, ql; LARGE_INTEGER qn; if (!qf.QuadPart) { QueryPerformanceFrequency(&qf); QueryPerformanceCounter(&ql); }
          QueryPerformanceCounter(&qn); double el = (double)(qn.QuadPart - ql.QuadPart) * 1000.0 / (double)qf.QuadPart; ql = qn;
          if (el > 100) el = 100;
          owed += 44.1 * el; }                    /* the audio in step with the real clock (a Sleep is longer than it says) */
        if (0) owed += 44.1 * 10;                       /* render audio in step with the clock: 10 ms per frame */
        while (owed >= APP_BLOCK) {
            timed_render(blk, 4096u * 64 / 100);
            if (wav_f) { for (int i = 0; i < APP_BLOCK * 2; ++i) { int32_t v = blk[i] >> 8; int16_t q = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); fwrite(&q, 2, 1, wav_f); } wav_frames += APP_BLOCK; }
            owed -= APP_BLOCK;
        }
        Sleep(2);
    }
}
static void wav_header(FILE *f, uint32_t frames)
{
    uint32_t d = frames * 4, r = 44100, br = r * 4; uint16_t a = 4, b = 16, ch = 2, fmt = 1; uint32_t fs = 16, riff = 36 + d;
    fseek(f, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fs, 4, 1, f);
    fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&r, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&a, 2, 1, f); fwrite(&b, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&d, 4, 1, f);
}
static void shot(const char *path)
{
    FILE *f = fopen(path, "wb"); if (!f) return;
    uint32_t w = 240, h = 240, sz = 54 + w * h * 3;
    uint8_t hd[54] = {'B', 'M'}; memcpy(hd + 2, &sz, 4); hd[10] = 54; hd[14] = 40; memcpy(hd + 18, &w, 4); memcpy(hd + 22, &h, 4);
    hd[26] = 1; hd[28] = 24;
    fwrite(hd, 1, 54, f);
    for (int y = 239; y >= 0; --y) for (int x = 0; x < 240; ++x) { uint32_t c = fb[y * 240 + x]; uint8_t p[3] = {(uint8_t)c, (uint8_t)(c >> 8), (uint8_t)(c >> 16)}; fwrite(p, 1, 3, f); }
    fclose(f);
}
static DWORD WINAPI gen_thread(LPVOID p)
{
    (void)p;
    LARGE_INTEGER f, t0, t; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t0); gen_t0 = t0;
    double per = 600.0 / (double)gen_bpm10 / 24.0, pre = 2.0;   /* (two seconds of clock before the start) */
    long sent = 0; int started = 0;
    for (;;) {
        QueryPerformanceCounter(&t); double s = (double)(t.QuadPart - t0.QuadPart) / (double)f.QuadPart;
        if (!started && s >= pre) { midi_q[midi_w & 255] = 0x0Fu | 0xFAu << 8; InterlockedIncrement(&midi_w); started = 1; }
        while ((double)sent * per <= s) { midi_q[midi_w & 255] = 0x0Fu | 0xF8u << 8; InterlockedIncrement(&midi_w); ++sent; }
        if (s >= pre + gen_ms / 1000.0) break;
        Sleep(1);
    }
    midi_q[midi_w & 255] = 0x0Fu | 0xFCu << 8; InterlockedIncrement(&midi_w);
    return 0;
}
/* the script's "pxdump FILE": the saved project pulled through the app's USB command as the Transfer page does it,
 * and written as the plugin's project file */
static int px_piece(unsigned kind, unsigned id, uint8_t *dst, uint32_t max, uint32_t *len)
{
    uint32_t got = 0, total = 0;
    for (;;) {
        uint8_t d[12] = {0, 0x59, 0x52, 5, 0, 0, (uint8_t)kind, (uint8_t)id, (uint8_t)got, (uint8_t)(got >> 8), (uint8_t)(got >> 16), 0};
        uint8_t u[256]; unsigned n = 0, nb = 0, s = 0; uint32_t acc = 0;
        sx_n = 0; app_usb_cmd(d, 12);
        if (sx_n < 4 || sx_last[0] != 0xF0 || sx_last[sx_n - 1] != 0xF7) return -1;
        for (uint32_t i = 1; i + 1 < sx_n; ++i) { acc |= (uint32_t)sx_last[i] << nb; nb += 7; while (nb >= 8 && n < sizeof u) { u[n++] = (uint8_t)acc; acc >>= 8; nb -= 8; } }
        if (n < 13 || u[2] != 0x52 || u[3] != kind || u[4] != id) return -1;
        unsigned k = u[11]; uint32_t off = u[5] | (uint32_t)u[6] << 8 | (uint32_t)u[7] << 16;
        for (unsigned i = 3; i < 12 + k; ++i) s += u[i];
        if ((uint8_t)~s != u[12 + k] || off != got) return -1;
        total = u[8] | (uint32_t)u[9] << 8 | (uint32_t)u[10] << 16;
        if (!total) { *len = 0; return 0; }
        if (total > max || !k) return -1;
        memcpy(dst + off, u + 12, k); got += k;
        if (got >= total) { *len = total; return 0; }
    }
}
static void px_dump(const char *path)
{
    static uint8_t piece[0x20000]; uint32_t len; uint8_t h[9];
    FILE *f = fopen(path, "wb"); if (!f) return;
    unsigned objs = 0; uint32_t bank = 0;
    sx_keep = 1;
    fwrite("PNGP\1\0\0\0", 1, 8, f);
    for (unsigned o = 0; o < OBJ_NOBJ; ++o) {
        if (o == OBJ_SETTINGS || o == OBJ_PANEL) continue;
        if (px_piece(1, o, piece, sizeof piece, &len)) { fprintf(stderr, "pxdump: object %u failed\n", o); continue; }
        if (!len) continue;
        h[0] = 1; memcpy(h + 1, &o, 4); memcpy(h + 5, &len, 4); fwrite(h, 1, 9, f); fwrite(piece, 1, len, f); ++objs;
    }
    if (!px_piece(2, 0, piece, sizeof piece, &len) && len) { uint32_t at = SMP_BASE; h[0] = 2; memcpy(h + 1, &at, 4); memcpy(h + 5, &len, 4); fwrite(h, 1, 9, f); fwrite(piece, 1, len, f); bank = len; }
    fputc(0, f); fclose(f);
    sx_keep = 0;
    fprintf(stderr, "pxdump: %u objects, sample bank %u bytes -> %s\n", objs, (unsigned)bank, path);
}
static int btn_named(const char *n)
{
    static const char *const N[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC", "OCT-", "OCT+"};
    for (int i = 0; i < NB; ++i) if (!_stricmp(n, N[i])) return i;
    return -1;
}
static int run_script(const char *file)
{
    FILE *f = fopen(file, "r");
    if (!f) return 2;
    char line[256], a[64], b[64]; int n;
    static const uint8_t wk[16] = PLAT_WHITE_KEYS, bk[11] = PLAT_BLACK_KEYS;
    static const char *const EN[NE] = {"SELECT", "ALGO", "PRESET", "K1", "K2", "K3", "K4"};
    run_for(50);
    while (fgets(line, sizeof line, f)) {
        a[0] = b[0] = 0; n = 0;
        if (line[0] == '#' || sscanf(line, "%63s", a) < 1) continue;
        if (!strcmp(a, "wait") && sscanf(line, "%*s %d", &n) == 1) run_for((uint32_t)n);
        else if (!strcmp(a, "raw")) {                       /* the engine straight: a note, a block, its peak; the track's state */
            int32_t rb[2 * APP_BLOCK]; double m = 0; const ch_track_state *ts = ch_track_get(0);
            ch_note(0, 60, 100); for (int k = 0; k < 4; ++k) ch_render(rb, APP_BLOCK, 0);
            for (int i = 0; i < 2 * APP_BLOCK; ++i) { double v = rb[i] / 8388608.0; if (v < 0) v = -v; if (v > m) m = v; }
            fprintf(stderr, "raw: peak %.4f after 4 blocks; track0 dev %u byp %u mute %u level %.2f model %.1f\n", m, ts->device[0], ts->bypass[0], ts->mute, ts->level, ts->param[ch_row(0, ts->device[0])][0]);
        }
        else if (!strcmp(a, "note")) { int r = ch_note(0, 60, 100); run_for(30); fprintf(stderr, "note: ch_note returned %d\n", r); }
        else if (!strcmp(a, "seq")) { const ch_seq_state *q = ch_seq_get(0); unsigned on = 0; for (unsigned l = 0; l < CH_LANES; ++l) for (unsigned i = 0; i < 16; ++i) on += q->step[l][i].vel != 0; fprintf(stderr, "seq: len %u dir %u rate %u set steps %u lane0 note %u step0 vel %u chance %u\n", q->len, q->dir, q->rate, on, q->note[0], q->step[0][0].vel, q->step[0][0].chance); }
        else if (!strcmp(a, "probe")) {                     /* the engine's state, for a headless check */
            const ch_snapshot *sn = ch_snapshot_get();
            fprintf(stderr, "probe: playing %u step %d hits %u %u %u %u env %.3f %.3f %.3f %.3f level %.3f %.3f %.3f %.3f voice_cap %u shed %u bad %u render %u us blkpeak %.4f\n",
                    sn->playing, sn->step[0], sn->hits[0], sn->hits[1], sn->hits[2], sn->hits[3], sn->env[0], sn->env[1], sn->env[2], sn->env[3],
                    sn->level[0][0], sn->level[1][0], sn->level[2][0], sn->level[3][0], sn->voice_cap, sn->shed, sn->bad[0][0], sn->render_us, sim_blkpeak);
            sim_blkpeak = 0;   /* the peak since the last probe */
        }
        else if (!strcmp(a, "data") && sscanf(line, "%*s %63s %i", b, &n) == 2) {   /* a file into the flash's data area at an offset */
            FILE *df = fopen(b, "rb");
            if (df) { static uint8_t tmp[0x20000]; size_t got = fread(tmp, 1, sizeof tmp, df); fclose(df);
                      for (size_t i = 0; i < got; i += 0x1000) plat_data_erase((uint32_t)(n + i) & ~0xFFFu);
                      plat_data_write((uint32_t)n, tmp, (uint32_t)got); smp_bind_all(); }
        }
        else if (!strcmp(a, "tap") && sscanf(line, "%*s %63s", b) == 1) { int i = btn_named(b); if (i >= 0) { btn_kbd |= 1u << i; run_for(60); btn_kbd &= ~(1u << i); run_for(60); } }
        else if (!strcmp(a, "press") && sscanf(line, "%*s %63s", b) == 1) { int i = btn_named(b); if (i >= 0) btn_kbd |= 1u << i; run_for(30); }
        else if (!strcmp(a, "release") && sscanf(line, "%*s %63s", b) == 1) { int i = btn_named(b); if (i >= 0) btn_kbd &= ~(1u << i); run_for(30); }
        else if (!strcmp(a, "tapkey") && sscanf(line, "%*s %d", &n) == 1 && n >= 1 && n <= 16) { key_kbd |= 1u << wk[n - 1]; run_for(60); key_kbd &= ~(1u << wk[n - 1]); run_for(60); }
        else if (!strcmp(a, "splash")) app_splash_replay();
        else if (!strcmp(a, "pxdump") && sscanf(line, "%*s %63s", b) == 1) px_dump(b);
        else if (!strcmp(a, "usbcmd") && sscanf(line, "%*s %d", &n) == 1) { uint8_t d[7] = {0, 0x59, (uint8_t)n, 0, 0, 0, 0xFF}; app_usb_cmd(d, 7); run_for(60); }
        else if (!strcmp(a, "knobs") && sscanf(line, "%*s %d", &n) == 1) { extern void app_ui_knobs(int); app_ui_knobs(n); run_for(60); }
        else if (!strcmp(a, "pal") && sscanf(line, "%*s %d", &n) == 1) { extern void app_ui_pal(int); app_ui_pal(n); run_for(60); }
        else if (!strcmp(a, "meter") && sscanf(line, "%*s %d", &n) == 1) { extern void app_ui_meter(int); app_ui_meter(n); run_for(60); }
        else if (!strcmp(a, "clockgen") && sscanf(line, "%*s %d %d", &gen_bpm10, &gen_ms) == 2) CreateThread(0, 0, gen_thread, 0, 0, 0);   /* tenths of a BPM, ms */
        else if (!strcmp(a, "clockout")) { fprintf(stderr, "clockout: %ld clocks %ld starts %ld stops\n", clk_out, clk_fa, clk_fc); clk_out = 0; }
        else if (!strcmp(a, "syncp")) { const ch_snapshot *sn = ch_snapshot_get(); LARGE_INTEGER f, t; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t); double mb = ((double)(t.QuadPart - gen_t0.QuadPart) / (double)f.QuadPart - 2.0) * gen_bpm10 / 600.0; fprintf(stderr, "syncp: playing %u beat %.3f master %.3f diff %+.1f ms tempo %.2f\n", sn->playing, sn->beat, mb, (sn->beat - mb) * 600000.0 / (gen_bpm10 ? gen_bpm10 : 1200), ch_tempo()); }
        else if (!strcmp(a, "lights")) {             /* the panel's lights now: white keys 1-16, black keys 1-11 */
            static const uint8_t wk[16] = PLAT_WHITE_KEYS, bk[11] = PLAT_BLACK_KEYS;
            char w[17], k[12];
            for (int i = 0; i < 16; ++i) w[i] = (lit_k >> wk[i] & 1u) ? '#' : (dim_k >> wk[i] & 1u) ? '-' : '.';
            for (int i = 0; i < 11; ++i) k[i] = (lit_k >> bk[i] & 1u) ? '#' : (dim_k >> bk[i] & 1u) ? '-' : '.';
            w[16] = k[11] = 0;
            fprintf(stderr, "white %s  black %s  PLAY %s  REC %s\n", w, k,
                    (lit_b & LED_PLAY_GREEN) ? "green" : (lit_b >> B_PLAY & 1u) ? "red" : "off", (lit_b >> B_REC & 1u) ? "on" : "off");
        }
        else if (!strcmp(a, "batt") && sscanf(line, "%*s %d", &n) == 1) { sim_batt = n; run_for(30); }
        else if (!strcmp(a, "braw") && sscanf(line, "%*s %d", &n) == 1) { sim_braw = n; run_for(30); }
        else if (!strcmp(a, "tapblack") && sscanf(line, "%*s %d", &n) == 1 && n >= 1 && n <= 11) { key_kbd |= 1u << bk[n - 1]; run_for(60); key_kbd &= ~(1u << bk[n - 1]); run_for(60); }
        else if (!strcmp(a, "black") && sscanf(line, "%*s %d %63s", &n, b) == 2 && n >= 1 && n <= 11) { if (!strcmp(b, "down")) key_kbd |= 1u << bk[n - 1]; else key_kbd &= ~(1u << bk[n - 1]); run_for(30); }
        else if (!strcmp(a, "white") && sscanf(line, "%*s %d %63s", &n, b) == 2 && n >= 1 && n <= 16) { if (!strcmp(b, "down")) key_kbd |= 1u << wk[n - 1]; else key_kbd &= ~(1u << wk[n - 1]); run_for(30); }
        else if (!strcmp(a, "key") && sscanf(line, "%*s %d %63s", &n, b) == 2) { if (!strcmp(b, "down")) key_kbd |= 1u << n; else key_kbd &= ~(1u << n); run_for(30); }
        else if (!strcmp(a, "turn") && sscanf(line, "%*s %63s %d", b, &n) == 2) { for (int i = 0; i < NE; ++i) if (!_stricmp(b, EN[i])) InterlockedExchangeAdd(&enc_acc[i], n); run_for(60); }
        else if (!strcmp(a, "shot") && sscanf(line, "%*s %63s", b) == 1) { run_for(60); shot(b); }
        else if (!strcmp(a, "wav") && sscanf(line, "%*s %63s", b) == 1) { wav_f = fopen(b, "wb+"); wav_frames = 0; if (wav_f) wav_header(wav_f, 0); }
    }
    if (wav_f) { wav_header(wav_f, wav_frames); fclose(wav_f); }
    fclose(f);
    return 0;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    (void)prev; (void)cmd;
    timeBeginPeriod(1);
    palette_set(APP_PALETTE);
    app_init();
    lcd_fill(0, 0, 240, 240, 0);
    app_ui_init();
    if (cmd && !strncmp(cmd, "--script ", 9)) return run_script(cmd + 9);
    HMIDIIN mi[8]; UINT nmi = midiInGetNumDevs();
    for (UINT d = 0; d < nmi && d < 8; ++d)
        if (midiInOpen(&mi[d], d, (DWORD_PTR)midi_cb, 0, CALLBACK_FUNCTION) == MMSYSERR_NOERROR) midiInStart(mi[d]);
    audio_start();
    WNDCLASSA wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = wndproc; wc.hInstance = inst; wc.lpszClassName = "PingFM1Sim";
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    RegisterClassA(&wc);
    RECT r = {0, 0, WIN_W, WIN_H};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, FALSE);
    hwnd = CreateWindowA("PingFM1Sim", "PING FM1 1.0  (simulator)", WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
                         CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, 0, 0, inst, 0);
    ShowWindow(hwnd, show);
    SetTimer(hwnd, 1, 10, 0);
    MSG m;
    while (GetMessageA(&m, 0, 0, 0)) { TranslateMessage(&m); DispatchMessageA(&m); }
    if (wo) { waveOutReset(wo); waveOutClose(wo); }
    return 0;
}
