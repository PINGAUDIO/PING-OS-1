/* PING OS-1 1.0 (engine: Chisel): a four-track rhythm synthesis engine for the M-VAVE FM-1, on fm1-base.
 * This header is the app's half of the contract with the platform. */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "PING OS-1 1.0"
#define APP_TAGLINE "RHYTHM SYNTHESIS ENGINE"
#define APP_SPLASH_RGB RGB(255, 132, 36)
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz (fm1-base's default, SLOOP's:
                                          * twice the room for a momentary spike that 128 had) */
#define APP_CANVAS_PX (240u * 80u)       /* drawn in bands; keeps the pool for audio buffers */

/* the store (app/project.h): the project (tempo, key, master, song), the panel calibration (the platform's), a
 * settings object per track, four preset banks, sixteen patterns; each two 4 KB sectors in the flash's data area
 * (0x97000..0xDFFFF; nothing may sit in 0xE0000..0xFBFFF, the update loader's staging). 0xC9000..0xDC000 is kept
 * for the sample bank. */
enum { OBJ_SETTINGS, OBJ_PROJECT, OBJ_PANEL, OBJ_TRK0, OBJ_TRK1, OBJ_TRK2, OBJ_TRK3, OBJ_PRE0, OBJ_PRE1, OBJ_PRE2, OBJ_PRE3,
       OBJ_PT0, OBJ_PT15 = OBJ_PT0 + 15, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0x9F000u, 0xDC000u, 0x97000u, 0x99000u, 0x9B000u, 0x9D000u, 0xA1000u, 0xA3000u, 0xA5000u, 0xA7000u,                        0xA9000u, 0xAB000u, 0xAD000u, 0xAF000u, 0xB1000u, 0xB3000u, 0xB5000u, 0xB7000u,                        0xB9000u, 0xBB000u, 0xBD000u, 0xBF000u, 0xC1000u, 0xC3000u, 0xC5000u, 0xC7000u}

void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);
void app_splash_replay(void);              /* the boot screen and its ping again (the simulator: F12) */
#define APP_SPLASH 1                     /* the first screen: the logo animation's first frame (no name card) */
#define APP_USB_CMD 1                    /* the sample bank over USB (app/samples.h) */
void app_usb_cmd(const uint8_t *d, uint32_t n);
void app_splash(void);
#define APP_PALETTE 4u                    /* MONO: black and white, with one warm orange accent (app.c C_ACC) */
