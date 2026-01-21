#ifndef ISP_H
#define ISP_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint16_t black_level;
    float gamma;
    float sharpen_amount;
    int denoise_strength;

    /* White balance gains */
    float wb_r_gain;
    float wb_g_gain;
    float wb_b_gain;
} st_isp_params;

typedef struct {
    uint8_t *data;
    uint32_t width;
    uint32_t height;
    uint32_t channels; /* 3 for RGB */
} st_rgb_image;

/* ISP Pipeline functions */
int isp_init(st_isp_params *params);
int isp_process_bayer10(const uint8_t *raw_bayer, size_t raw_size,
                        uint32_t width, uint32_t height,
                        st_rgb_image *output, st_isp_params *params);
void isp_free_image(st_rgb_image *img);

/* Individual processing steps */
void isp_black_level_correction(uint16_t *data, uint32_t size, uint16_t black_level);
void isp_debayer_bilinear(const uint16_t *bayer, uint16_t *rgb,
                            uint32_t width, uint32_t height);
void isp_white_balance(uint16_t *rgb, uint32_t width, uint32_t height,
                       float r_gain, float g_gain, float b_gain);
void isp_sharpen(uint16_t *rgb, uint32_t width, uint32_t height, float amount);

#endif /* ISP_H */