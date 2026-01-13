// rpi_camera.h - C API Header
#ifndef RPI_CAMERA_H
#define RPI_CAMERA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

typedef struct rpi_camera_t rpi_camera_t;

typedef enum {
    RPI_FMT_YUV420,
    RPI_FMT_RGB888,
    RPI_FMT_MJPEG
} rpi_format_t;

typedef struct {
    void *data;
    size_t size;
    uint64_t timestamp;
    uint32_t sequence;
} rpi_frame_t;

// Callback khi có frame mới
typedef void (*rpi_frame_callback_t)(rpi_frame_t *frame, void *userdata);

// API functions
rpi_camera_t* rpi_camera_create(int width, int height, rpi_format_t format);
int rpi_camera_start(rpi_camera_t *cam, rpi_frame_callback_t callback, void *userdata);
int rpi_camera_stop(rpi_camera_t *cam);
void rpi_camera_destroy(rpi_camera_t *cam);

// Cấu hình nâng cao
int rpi_camera_set_brightness(rpi_camera_t *cam, float value); // -1.0 to 1.0
int rpi_camera_set_contrast(rpi_camera_t *cam, float value);   // 0.0 to 2.0
int rpi_camera_set_exposure(rpi_camera_t *cam, int microseconds);
int rpi_camera_set_gain(rpi_camera_t *cam, float value);       // 1.0 to 16.0

#ifdef __cplusplus
}
#endif

#endif // RPI_CAMERA_H