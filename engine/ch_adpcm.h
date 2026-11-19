/* Chisel sample format: blocked IMA ADPCM, mono.
 *
 * Granular and tape playback jump around a sample constantly, so the format
 * has to be cheap to enter anywhere. Each block holds CH_ADPCM_BLOCK frames:
 * a 4-byte header (int16 predictor, uint8 step index, reserved) followed by
 * CH_ADPCM_BLOCK/2 bytes of 4-bit codes. Entering a block costs at most
 * CH_ADPCM_BLOCK decode steps, and the data can be read straight out of
 * memory-mapped flash with no decompressed copy in RAM.
 *
 * 4 bits per frame at 32 kHz is 16 KB/s, so the FM-1's ~456 KB sample area
 * holds about 28 seconds, against ~19.5 s for the stock codec. */
#ifndef CH_ADPCM_H
#define CH_ADPCM_H
#include <stdint.h>
#include <stddef.h>

#define CH_ADPCM_BLOCK 64u
#define CH_ADPCM_BLOCK_BYTES (4u + CH_ADPCM_BLOCK / 2u)

static inline size_t ch_adpcm_bytes(unsigned frames)
{
    return (size_t)((frames + CH_ADPCM_BLOCK - 1) / CH_ADPCM_BLOCK) * CH_ADPCM_BLOCK_BYTES;
}
/* Encode int16 PCM; returns bytes written (ch_adpcm_bytes(frames)). */
size_t ch_adpcm_encode(const int16_t *pcm, unsigned frames, uint8_t *out);
/* Decode one whole block (CH_ADPCM_BLOCK frames). */
void ch_adpcm_decode_block(const uint8_t *block, int16_t out[CH_ADPCM_BLOCK]);
/* A block's first frame alone. */
int16_t ch_adpcm_first(const uint8_t *block);

#endif
