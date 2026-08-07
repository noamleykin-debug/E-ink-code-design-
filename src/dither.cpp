#include "dither.h"
#include "config.h"
#include <esp_heap_caps.h>

namespace Dither {

static inline int clamp(int val) {
    if (val < 0) return 0;
    if (val > 255) return 255;
    return val;
}

// Perceptually weighted squared distance. Green is weighted most, blue least,
// so chromatic pixels match the palette color the eye actually finds closest
// (see DITHER_W_* in config.h). Max term ~16*255*255*3 fits comfortably in u32.
static inline uint32_t distSq(int r1, int g1, int b1, int r2, int g2, int b2) {
    int dr = r1 - r2;
    int dg = g1 - g2;
    int db = b1 - b2;
    return (uint32_t)(DITHER_W_R * dr * dr + DITHER_W_G * dg * dg + DITHER_W_B * db * db);
}

// Punch up the source pixel before quantizing: a little contrast around mid-gray,
// then saturation around the pixel's own luma. This trades some realism for a
// livelier 6-color render and, crucially, fills flat light areas with color dots
// instead of leaving them solid white. All controlled by config.h (100 = off).
static inline void applyLook(int& r, int& g, int& b) {
    r = clamp(((r - 128) * DITHER_CONTRAST) / 100 + 128);
    g = clamp(((g - 128) * DITHER_CONTRAST) / 100 + 128);
    b = clamp(((b - 128) * DITHER_CONTRAST) / 100 + 128);

    int luma = (r * 30 + g * 59 + b * 11) / 100;
    r = clamp(luma + ((r - luma) * DITHER_SATURATION) / 100);
    g = clamp(luma + ((g - luma) * DITHER_SATURATION) / 100);
    b = clamp(luma + ((b - luma) * DITHER_SATURATION) / 100);
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

void processFrame(const uint16_t* rgb565_in, uint8_t* index_out) {
    if (!rgb565_in || !index_out) {
        log_e("Dither: Null buffers");
        return;
    }

    log_i("Starting Floyd-Steinberg dither to 6 colors");

    // Allocate two error rows (R,G,B for each pixel) in PSRAM.
    // int16_t prevents overflow during error accumulation.
    int16_t* err_curr = (int16_t*)heap_caps_calloc(EPD_WIDTH * 3, sizeof(int16_t), MALLOC_CAP_SPIRAM);
    int16_t* err_next = (int16_t*)heap_caps_calloc(EPD_WIDTH * 3, sizeof(int16_t), MALLOC_CAP_SPIRAM);

    if (!err_curr || !err_next) {
        log_e("Dither: Failed to allocate error buffers in PSRAM");
        if (err_curr) heap_caps_free(err_curr);
        if (err_next) heap_caps_free(err_next);
        return;
    }

    // Clear output buffer completely before packing
    memset(index_out, 0, FRAME_INDEX_BYTES);

    for (int y = 0; y < EPD_HEIGHT; y++) {
        // Serpentine scan: alternate direction each row to break up the diagonal
        // "worm" artifacts of plain left-to-right diffusion and spread dots evenly.
        bool ltr = (DITHER_SERPENTINE == 0) || ((y & 1) == 0);
        int xStart = ltr ? 0 : EPD_WIDTH - 1;
        int xEnd   = ltr ? EPD_WIDTH : -1;
        int xStep  = ltr ? 1 : -1;
        int fwd    = ltr ? 1 : -1;   // direction of travel; diffusion mirrors with it

        for (int x = xStart; x != xEnd; x += xStep) {
            int px_idx = y * EPD_WIDTH + x;
            uint16_t color16 = rgb565_in[px_idx];

            // Expand RGB565 to RGB888, replicating high bits to prevent banding
            int r5 = (color16 >> 11) & 0x1F;
            int g6 = (color16 >> 5) & 0x3F;
            int b5 = color16 & 0x1F;

            int r = (r5 << 3) | (r5 >> 2);
            int g = (g6 << 2) | (g6 >> 4);
            int b = (b5 << 3) | (b5 >> 2);

            // Perceptual pre-processing (contrast + saturation) for a punchier look
            applyLook(r, g, b);

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

            // Diffuse error (Floyd-Steinberg weights 7/3/5/1), mirrored for the
            // scan direction so serpentine rows stay symmetric.
            int xf = x + fwd;   // next pixel along travel direction
            int xb = x - fwd;   // previous pixel along travel direction

            if (xf >= 0 && xf < EPD_WIDTH) {
                err_curr[xf * 3 + 0] += (er * 7) >> 4;
                err_curr[xf * 3 + 1] += (eg * 7) >> 4;
                err_curr[xf * 3 + 2] += (eb * 7) >> 4;
            }
            if (y + 1 < EPD_HEIGHT) {
                if (xb >= 0 && xb < EPD_WIDTH) {
                    err_next[xb * 3 + 0] += (er * 3) >> 4;
                    err_next[xb * 3 + 1] += (eg * 3) >> 4;
                    err_next[xb * 3 + 2] += (eb * 3) >> 4;
                }

                err_next[x * 3 + 0] += (er * 5) >> 4;
                err_next[x * 3 + 1] += (eg * 5) >> 4;
                err_next[x * 3 + 2] += (eb * 5) >> 4;

                if (xf >= 0 && xf < EPD_WIDTH) {
                    err_next[xf * 3 + 0] += (er * 1) >> 4;
                    err_next[xf * 3 + 1] += (eg * 1) >> 4;
                    err_next[xf * 3 + 2] += (eb * 1) >> 4;
                }
            }
        }

        // Swap error rows and clear the new next row for the next line iteration
        int16_t* temp = err_curr;
        err_curr = err_next;
        err_next = temp;
        memset(err_next, 0, EPD_WIDTH * 3 * sizeof(int16_t));
    }

    // Free the working PSRAM
    heap_caps_free(err_curr);
    heap_caps_free(err_next);

    log_i("Dither complete");
}

} // namespace Dither
