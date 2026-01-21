#include "isp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int isp_init(st_isp_params *params) {
    // Default parameters
    params->black_level = 64;      // Typical for 10-bit sensors
    params->gamma = 2.2;
    params->sharpen_amount = 0.5;
    params->denoise_strength = 5;
    
    // Gray world white balance (adjust later)
    params->wb_r_gain = 1.5;
    params->wb_g_gain = 1.0;
    params->wb_b_gain = 1.8;
    
    printf("[ISP]: Initialized with defaults\n");
    return 0;
}

/* Convert packed 10-bit Bayer to 16-bit array */
static void unpack_bayer10(const uint8_t *packed, uint16_t *unpacked, 
                           uint32_t width, uint32_t height) {
    // SBGGR10 is packed: 4 pixels in 5 bytes
    // [P0_9:2][P1_9:2][P2_9:2][P3_9:2][P3_1:0|P2_1:0|P1_1:0|P0_1:0]
    
    uint32_t pixel_count = width * height;
    uint32_t packed_idx = 0;
    
    for (uint32_t i = 0; i < pixel_count; i += 4) {
        if (i + 4 > pixel_count) break;
        
        uint8_t b0 = packed[packed_idx++];
        uint8_t b1 = packed[packed_idx++];
        uint8_t b2 = packed[packed_idx++];
        uint8_t b3 = packed[packed_idx++];
        uint8_t b4 = packed[packed_idx++];
        
        unpacked[i]     = (b0 << 2) | ((b4 >> 0) & 0x03);
        unpacked[i + 1] = (b1 << 2) | ((b4 >> 2) & 0x03);
        unpacked[i + 2] = (b2 << 2) | ((b4 >> 4) & 0x03);
        unpacked[i + 3] = (b3 << 2) | ((b4 >> 6) & 0x03);
    }
}

void isp_black_level_correction(uint16_t *data, uint32_t size, uint16_t black_level) {
    for (uint32_t i = 0; i < size; i++) {
        if (data[i] > black_level) {
            data[i] -= black_level;
        } else {
            data[i] = 0;
        }
    }
}

/* Simple bilinear debayer for BGGR pattern */
void isp_debayer_bilinear(const uint16_t *bayer, uint16_t *rgb,
                          uint32_t width, uint32_t height) {
    // BGGR pattern:
    // B G B G
    // G R G R
    // B G B G
    
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t bayer_idx = y * width + x;
            uint32_t rgb_idx = (y * width + x) * 3;
            
            uint16_t r, g, b;
            
            if (y % 2 == 0) {
                if (x % 2 == 0) {
                    // Blue pixel (B)
                    b = bayer[bayer_idx];
                    
                    // Green (average of neighbors)
                    uint32_t g_sum = 0, g_count = 0;
                    if (x > 0) { g_sum += bayer[bayer_idx - 1]; g_count++; }
                    if (x < width - 1) { g_sum += bayer[bayer_idx + 1]; g_count++; }
                    if (y > 0) { g_sum += bayer[bayer_idx - width]; g_count++; }
                    if (y < height - 1) { g_sum += bayer[bayer_idx + width]; g_count++; }
                    g = g_count > 0 ? g_sum / g_count : 0;
                    
                    // Red (average of diagonals)
                    uint32_t r_sum = 0, r_count = 0;
                    if (x > 0 && y > 0) { r_sum += bayer[bayer_idx - width - 1]; r_count++; }
                    if (x < width - 1 && y > 0) { r_sum += bayer[bayer_idx - width + 1]; r_count++; }
                    if (x > 0 && y < height - 1) { r_sum += bayer[bayer_idx + width - 1]; r_count++; }
                    if (x < width - 1 && y < height - 1) { r_sum += bayer[bayer_idx + width + 1]; r_count++; }
                    r = r_count > 0 ? r_sum / r_count : 0;
                    
                } else {
                    // Green pixel on blue row (Gb)
                    g = bayer[bayer_idx];
                    
                    // Blue (average horizontal)
                    b = (bayer[bayer_idx - 1] + bayer[bayer_idx + 1]) / 2;
                    
                    // Red (average vertical)
                    r = (bayer[bayer_idx - width] + bayer[bayer_idx + width]) / 2;
                }
            } else {
                if (x % 2 == 0) {
                    // Green pixel on red row (Gr)
                    g = bayer[bayer_idx];
                    
                    // Red (average horizontal)
                    r = (bayer[bayer_idx - 1] + bayer[bayer_idx + 1]) / 2;
                    
                    // Blue (average vertical)
                    b = (bayer[bayer_idx - width] + bayer[bayer_idx + width]) / 2;
                    
                } else {
                    // Red pixel (R)
                    r = bayer[bayer_idx];
                    
                    // Green (average of neighbors)
                    uint32_t g_sum = 0, g_count = 0;
                    if (x > 0) { g_sum += bayer[bayer_idx - 1]; g_count++; }
                    if (x < width - 1) { g_sum += bayer[bayer_idx + 1]; g_count++; }
                    if (y > 0) { g_sum += bayer[bayer_idx - width]; g_count++; }
                    if (y < height - 1) { g_sum += bayer[bayer_idx + width]; g_count++; }
                    g = g_count > 0 ? g_sum / g_count : 0;
                    
                    // Blue (average of diagonals)
                    uint32_t b_sum = 0, b_count = 0;
                    if (x > 0 && y > 0) { b_sum += bayer[bayer_idx - width - 1]; b_count++; }
                    if (x < width - 1 && y > 0) { b_sum += bayer[bayer_idx - width + 1]; b_count++; }
                    if (x > 0 && y < height - 1) { b_sum += bayer[bayer_idx + width - 1]; b_count++; }
                    if (x < width - 1 && y < height - 1) { b_sum += bayer[bayer_idx + width + 1]; b_count++; }
                    b = b_count > 0 ? b_sum / b_count : 0;
                }
            }
            
            rgb[rgb_idx]     = r;
            rgb[rgb_idx + 1] = g;
            rgb[rgb_idx + 2] = b;
        }
    }
}

void isp_white_balance(uint16_t *rgb, uint32_t width, uint32_t height,
                       float r_gain, float g_gain, float b_gain) {
    uint32_t pixel_count = width * height;
    
    for (uint32_t i = 0; i < pixel_count; i++) {
        uint32_t idx = i * 3;
        
        uint32_t r = (uint32_t)(rgb[idx] * r_gain);
        uint32_t g = (uint32_t)(rgb[idx + 1] * g_gain);
        uint32_t b = (uint32_t)(rgb[idx + 2] * b_gain);
        
        rgb[idx]     = r > 1023 ? 1023 : r; // Clamp to 10-bit max
        rgb[idx + 1] = g > 1023 ? 1023 : g;
        rgb[idx + 2] = b > 1023 ? 1023 : b;
    }
}

static uint16_t gamma_lut[1024];
static int lut_initialized = 0;

void init_gamma_lut(float gamma) {
    float inv_gamma = 1.0f / gamma;
    for (int i = 0; i < 1024; i++) {
        float normalized = (float)i / 1023.0f;
        gamma_lut[i] = (uint16_t)(powf(normalized, inv_gamma) * 1023.0f);
    }
    lut_initialized = 1;
}

void isp_gamma_correction_fast(uint16_t *rgb, uint32_t size) {
    if (!lut_initialized) return; // Nhớ gọi init_gamma_lut trước
    for (uint32_t i = 0; i < size; i++) {
        rgb[i] = gamma_lut[rgb[i] & 0x3FF]; // Giới hạn 10-bit để tránh tràn mảng
    }
}

void isp_sharpen(uint16_t *rgb, uint32_t width, uint32_t height, float amount) {
    // Simple unsharp masking
    // (Simplified - for production use proper Gaussian blur)
    
    uint16_t *temp = malloc(width * height * 3 * sizeof(uint16_t));
    memcpy(temp, rgb, width * height * 3 * sizeof(uint16_t));
    
    for (uint32_t y = 1; y < height - 1; y++) {
        for (uint32_t x = 1; x < width - 1; x++) {
            for (int c = 0; c < 3; c++) {
                uint32_t idx = (y * width + x) * 3 + c;
                
                // Simple 3x3 box blur
                int32_t sum = 
                    temp[(y - 1) * width * 3 + (x - 1) * 3 + c] +
                    temp[(y - 1) * width * 3 + x * 3 + c] +
                    temp[(y - 1) * width * 3 + (x + 1) * 3 + c] +
                    temp[y * width * 3 + (x - 1) * 3 + c] +
                    temp[y * width * 3 + x * 3 + c] +
                    temp[y * width * 3 + (x + 1) * 3 + c] +
                    temp[(y + 1) * width * 3 + (x - 1) * 3 + c] +
                    temp[(y + 1) * width * 3 + x * 3 + c] +
                    temp[(y + 1) * width * 3 + (x + 1) * 3 + c];
                
                int32_t blurred = sum / 9;
                int32_t detail = temp[idx] - blurred;
                int32_t sharpened = temp[idx] + (int32_t)(detail * amount);
                
                if (sharpened < 0) sharpened = 0;
                if (sharpened > 1023) sharpened = 1023;
                
                rgb[idx] = (uint16_t)sharpened;
            }
        }
    }
    
    free(temp);
}

int isp_process_bayer10(const uint8_t *raw_bayer, size_t raw_size,
                        uint32_t width, uint32_t height,
                        st_rgb_image *output, st_isp_params *params) {
    printf("[ISP]: Processing %ux%u frame...\n", width, height);
    
    // Step 1: Unpack 10-bit to 16-bit
    uint16_t *unpacked = malloc(width * height * sizeof(uint16_t));
    if (!unpacked) {
        fprintf(stderr, "[ISP ERROR]: Memory allocation failed\n");
        return -1;
    }
    
    printf("[ISP]: Unpacking 10-bit data...\n");
    unpack_bayer10(raw_bayer, unpacked, width, height);
    
    // Step 2: Black level correction
    printf("[ISP]: Black level correction...\n");
    isp_black_level_correction(unpacked, width * height, params->black_level);
    
    // Step 3: Debayer
    uint16_t *rgb16 = malloc(width * height * 3 * sizeof(uint16_t));
    if (!rgb16) {
        free(unpacked);
        return -1;
    }
    
    printf("[ISP]: Demosaicing...\n");
    isp_debayer_bilinear(unpacked, rgb16, width, height);
    free(unpacked);
    
    // Step 4: White balance
    printf("[ISP]: White balance...\n");
    isp_white_balance(rgb16, width, height,
                      params->wb_r_gain, params->wb_g_gain, params->wb_b_gain);
    
    // Step 5: Gamma correction
    printf("[ISP]: Gamma correction (Fast)...\n");
    isp_gamma_correction_fast(rgb16, width * height * 3);
    
    // Step 6: Sharpen
    if (params->sharpen_amount > 0) {
        printf("[ISP]: Sharpening...\n");
        isp_sharpen(rgb16, width, height, params->sharpen_amount);
    }
    
    // Step 7: Convert to 8-bit RGB
    output->width = width;
    output->height = height;
    output->channels = 3;
    output->data = malloc(width * height * 3);
    
    if (!output->data) {
        free(rgb16);
        return -1;
    }
    
    for (uint32_t i = 0; i < width * height * 3; i++) {
        output->data[i] = (uint8_t)(rgb16[i] >> 2); // 10-bit to 8-bit
    }
    
    free(rgb16);
    printf("[ISP]: Processing complete!\n");
    return 0;
}

void isp_free_image(st_rgb_image *img) {
    if (img && img->data) {
        free(img->data);
        img->data = NULL;
    }
}