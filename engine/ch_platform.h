/* Placement and build constants shared by the engine.
 * On the FM-1 (fm1-base) big zero-initialised buffers go to the 336 KB pool
 * region; on the PC bench and in the simulators they are ordinary BSS. */
#ifndef CH_PLATFORM_H
#define CH_PLATFORM_H
#if defined(CHISEL_FM1)
#include "plat.h"
#define CH_POOL PLAT_POOL
#else
#define CH_POOL
#endif
/* CH_AUDIO_CODE: code that runs in every render whatever is playing. On the FM-1 the program runs in place from the
 * flash through a 32 KB instruction cache (8 ways of 4 KB), which the screen's and the keys' code share: the render's
 * code is some 150 KB in all, the drum voice alone 50 KB, so what a block needs was fetched from the flash again and
 * again (about 2 us a line), and a second or third track pushed the render over its time. The code every block runs
 * goes to RAM instead (.c1_text: in fm1-base's .ram_text, copied at the start, 32 KB in all, where its check lets
 * code call into the flash), which leaves the cache to the models and effects in use */
/* (Off unless CH_RAM_CODE is defined: fm1.toml's o2_cflags, with the far calls it needs. Not yet run on a device) */
#if defined(CHISEL_FM1) && defined(CH_RAM_CODE)
#define CH_AUDIO_CODE __attribute__((section(".c1_text.ch")))
#else
#define CH_AUDIO_CODE
#endif
#define CH_AUDIO_DATA
/* CH_P(i): the bench's profile (CH_PROF builds): the cycles since the last mark go to section i */
#if defined(CH_PROF)
#include <intrin.h>
extern unsigned long long ch_prof[32], ch_prof_t;
#define CH_P(i) do { unsigned long long t_ = __rdtsc(); ch_prof[i] += t_ - ch_prof_t; ch_prof_t = t_; } while (0)
#else
#define CH_P(i) do { } while (0)
#endif
/* CH_SI: a small helper the RAM code uses per sample. The FM-1's compiler inlines nothing from the flash's sections
 * into a function with a section of its own, so these are forced in (else each was a call out to the flash, per sample) */
#if defined(_MSC_VER)
#define CH_SI static __forceinline
#else
#define CH_SI static inline __attribute__((always_inline))
#endif
#if defined(_MSC_VER)
#define CH_NOINLINE __declspec(noinline)
#else
#define CH_NOINLINE __attribute__((noinline))
#endif
#define CH_RATE 44100
#define CH_SUB 64            /* the longest a sub-block can be: every buffer of one has this room. How long one is now is
                             * ch_sub: 32 frames (a 1378 Hz control rate), or 64 (half the trips between the tracks' code) */
#define CH_MAX_BLOCK 256

/* RAM budget. Everything that scales with time lives here so the device
 * build can trade lengths for memory in one place. Buffers are int16 mono
 * unless noted. Bytes at the defaults (44.1 kHz):
 *   the effects' line memory: CH_FX_BLOCKS blocks of CH_FX_BLOCK frames (DELAY takes 8, ECHO and REVERB 4,
 *   CHORUS and FLANGER 1). */
#define CH_FX_BLOCK 4096
#if defined(CHISEL_FM1)
/* the FM-1: about 260 KB of pool for everything; first numbers, tuned on the device */
#define CH_FX_BLOCKS      24              /* 192 KB; an idle effect (MIX 0, bypassed) holds none, so the master's DELAY and
                                           * REVERB cost nothing until used: DELAY 8, GRAINS 6, ECHO/REVERB/SPRING/DUB 4, CHORUS/FLANGER/TAPE 1 */
#else
#define CH_FX_BLOCKS      24              /* as the FM-1: the simulator runs out where it does */
#endif
#endif
