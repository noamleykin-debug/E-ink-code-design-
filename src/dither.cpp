#include "dither.h"
#include "config.h"
#include <esp_heap_caps.h>

namespace Dither {

static inline int clamp(int val) {
    if (val < 0) return 0;
    if (val > 255) return 255;
    return val;
}

static inline uint32_t distSq(int r1, int g1, int b1, int r2, int g2, int b2) {
    int dr = r1 - r2;
    int dg = g1 - g2;
    int db = b1 - b2;
    return dr * dr + dg * dg + db * db;
}

static uint8_t findNearestColor(int r, int g, int b) {
    uint32_t minDist = 0xFFFFFFFF;
    uint8_t bestIdx = 0;

    for (uint8_t i = 0; i < COL_COUNT; i++) {
        uint32_t d = distSq(r, g, b, EPD_PALETTE[i].r, EPD_PALETTE[i].g, EPD_PALETTE[i].b);
        if (d < minDist) {
            minDist = d;
            bestIdx = i;
        }
    }

    // Safety check: ensure we strictly return one of the 6 colors
    if (bestIdx >= COL_COUNT) return 0;
    return bestIdx;
}

void processFrame(const uint16_t* rgb565_in, uint8_t* index_out, DitherMode mode) {
    if (!rgb565_in || !index_out) {
        log_e("Dither: Null buffers");
        return;
    }

    log_i("Starting %s dither to 6 colors",
          mode == DITHER_ATKINSON ? "Atkinson" : "Floyd-Steinberg");

    // Three error rows (R,G,B per pixel) in PSRAM: Floyd-Steinberg touches
    // rows y and y+1, Atkinson also reaches y+2. int16_t prevents overflow
    // during error accumulation.
    const size_t row_elems = (size_t)EPD_WIDTH * 3;
    int16_t* err_curr  = (int16_t*)heap_caps_calloc(row_elems, sizeof(int16_t), MALLOC_CAP_SPIRAM);
    int16_t* err_next  = (int16_t*)heap_caps_calloc(row_elems, sizeof(int16_t), MALLOC_CAP_SPIRAM);
    int16_t* err_next2 = (int16_t*)heap_caps_calloc(row_elems, sizeof(int16_t), MALLOC_CAP_SPIRAM);

    if (!err_curr || !err_next || !err_next2) {
        log_e("Dither: Failed to allocate error buffers in PSRAM");
        if (err_curr)  heap_caps_free(err_curr);
        if (err_next)  heap_caps_free(err_next);
        if (err_next2) heap_caps_free(err_next2);
        return;
    }

    // Clear output buffer completely before packing
    memset(index_out, 0, FRAME_INDEX_BYTES);

    for (int y = 0; y < EPD_HEIGHT; y++) {
        // Serpentine scan: odd rows run right-to-left with the error kernel
        // mirrored. Plain raster scanning pushes all error in one direction,
        // which shows up as diagonal "worm" artifacts, very visible with
        // only 6 colors. Alternating direction breaks the pattern up.
        const bool reverse = (y & 1) != 0;
        const int dir = reverse ? -1 : 1;

        for (int i = 0; i < EPD_WIDTH; i++) {
            const int x = reverse ? (EPD_WIDTH - 1 - i) : i;
            int px_idx = y * EPD_WIDTH + x;
            uint16_t color16 = rgb565_in[px_idx];

            // Expand RGB565 to RGB888, replicating high bits to prevent banding
            int r5 = (color16 >> 11) & 0x1F;
            int g6 = (color16 >> 5) & 0x3F;
            int b5 = color16 & 0x1F;

            int r = (r5 << 3) | (r5 >> 2);
            int g = (g6 << 2) | (g6 >> 4);
            int b = (b5 << 3) | (b5 >> 2);

            // Add accumulated error from current row
            r = clamp(r + err_curr[x * 3 + 0]);
            g = clamp(g + err_curr[x * 3 + 1]);
            b = clamp(b + err_curr[x * 3 + 2]);

            // Quantize to nearest hardware palette color
            uint8_t pal_idx = findNearestColor(r, g, b);

            // Calculate quantization error
            int er = r - EPD_PALETTE[pal_idx].r;
            int eg = g - EPD_PALETTE[pal_idx].g;
            int eb = b - EPD_PALETTE[pal_idx].b;

            // Pack into 4bpp output buffer (even-x high nibble, odd-x low nibble)
            int byte_idx = px_idx / 2;
            if (x % 2 == 0) {
                index_out[byte_idx] = (index_out[byte_idx] & 0x0F) | (pal_idx << 4);
            } else {
                index_out[byte_idx] = (index_out[byte_idx] & 0xF0) | (pal_idx & 0x0F);
            }

            // Diffuse (num/2^shift of the error) to one neighbor, bounds-checked.
            auto spread = [&](int16_t* row, int xx, int num, int shift) {
                if (xx < 0 || xx >= EPD_WIDTH) return;
                row[xx * 3 + 0] += (er * num) >> shift;
                row[xx * 3 + 1] += (eg * num) >> shift;
                row[xx * 3 + 2] += (eb * num) >> shift;
            };

            // Neighbor positions, mirrored on reversed rows: "ahead" is
            // x+dir, "behind" is x-dir.
            const int xf  = x + dir;
            const int xf2 = x + 2 * dir;
            const int xb  = x - dir;
            const bool hasRow1 = (y + 1 < EPD_HEIGHT);
            const bool hasRow2 = (y + 2 < EPD_HEIGHT);

            if (mode == DITHER_ATKINSON) {
                // Atkinson: 1/8 to six neighbors, the remaining 2/8 is
                // deliberately discarded. Less error in flight means flat
                // areas stay clean (no stray dark speckles), at the cost of
                // slightly compressed extreme tones.
                //
                //        *   1   1
                //    1   1   1
                //        1          (all /8)
                spread(err_curr,  xf,  1, 3);
                spread(err_curr,  xf2, 1, 3);
                if (hasRow1) {
                    spread(err_next, xb, 1, 3);
                    spread(err_next, x,  1, 3);
                    spread(err_next, xf, 1, 3);
                }
                if (hasRow2) {
                    spread(err_next2, x, 1, 3);
                }
            } else {
                // Floyd-Steinberg: 7/16, 3/16, 5/16, 1/16, all error kept.
                spread(err_curr, xf, 7, 4);
                if (hasRow1) {
                    spread(err_next, xb, 3, 4);
                    spread(err_next, x,  5, 4);
                    spread(err_next, xf, 1, 4);
                }
            }
        }

        // Rotate the error rows: next becomes current, next2 becomes next,
        // and the retired row is cleared for reuse as the new next2.
        int16_t* tmp = err_curr;
        err_curr  = err_next;
        err_next  = err_next2;
        err_next2 = tmp;
        memset(err_next2, 0, row_elems * sizeof(int16_t));
    }

    // Free the working PSRAM
    heap_caps_free(err_curr);
    heap_caps_free(err_next);
    heap_caps_free(err_next2);

    log_i("Dither complete");
}

} // namespace Dither
