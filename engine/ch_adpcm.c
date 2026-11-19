#include "ch_adpcm.h"
#include "ch_platform.h"

static const int16_t step_table[89] CH_AUDIO_DATA = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
    253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
    3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767};
static const int8_t index_table[16] CH_AUDIO_DATA = {
    -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};

static inline int clamp16(int v) { return v > 32767 ? 32767 : v < -32768 ? -32768 : v; }

static inline int step_decode(unsigned code, int *predictor, int *index)
{
    int step = step_table[*index];
    int diff = step >> 3;
    if (code & 4) diff += step;
    if (code & 2) diff += step >> 1;
    if (code & 1) diff += step >> 2;
    *predictor = clamp16(code & 8 ? *predictor - diff : *predictor + diff);
    *index += index_table[code];
    if (*index < 0) *index = 0; else if (*index > 88) *index = 88;
    return *predictor;
}

size_t ch_adpcm_encode(const int16_t *pcm, unsigned frames, uint8_t *out)
{
    size_t written = 0;
    int index = 0;
    for (unsigned start = 0; start < frames; start += CH_ADPCM_BLOCK) {
        uint8_t *b = out + written;
        int predictor = pcm[start];
        b[0] = (uint8_t)(predictor & 255); b[1] = (uint8_t)((predictor >> 8) & 255);
        b[2] = (uint8_t)index; b[3] = 0;
        for (unsigned i = 0; i < CH_ADPCM_BLOCK; ++i) {
            int target = start + i < frames ? pcm[start + i] : 0;
            /* Search the code that lands closest: a greedy IMA encoder
             * with a one-step look at the result keeps transients sharper. */
            int best = 0, best_error = 1 << 30, best_p = 0, best_i = 0;
            for (unsigned code = 0; code < 16; ++code) {
                int p = predictor, ix = index;
                step_decode(code, &p, &ix);
                int e = p - target; e = e < 0 ? -e : e;
                if (e < best_error) { best_error = e; best = (int)code; best_p = p; best_i = ix; }
            }
            predictor = best_p; index = best_i;
            if (i & 1) b[4 + i / 2] |= (uint8_t)(best << 4);
            else b[4 + i / 2] = (uint8_t)best;
        }
        written += CH_ADPCM_BLOCK_BYTES;
    }
    return written;
}

CH_AUDIO_CODE void ch_adpcm_decode_block(const uint8_t *b, int16_t out[CH_ADPCM_BLOCK])
{
    int predictor = (int16_t)(b[0] | b[1] << 8), index = b[2] > 88 ? 88 : b[2];
    for (unsigned i = 0; i < CH_ADPCM_BLOCK; i += 2) {
        unsigned byte = b[4 + i / 2];
        out[i] = (int16_t)step_decode(byte & 15, &predictor, &index);
        out[i + 1] = (int16_t)step_decode(byte >> 4, &predictor, &index);
    }
}

int16_t ch_adpcm_first(const uint8_t *b)
{
    int predictor = (int16_t)(b[0] | b[1] << 8), index = b[2] > 88 ? 88 : b[2];
    return (int16_t)step_decode(b[4] & 15, &predictor, &index);
}
