// camera.h
#pragma once

#include <stddef.h>        // <-- add this
#include <stdint.h>
#include <linux/videodev2.h>

struct buffer {
    void   *start;
    size_t  length;
};

typedef struct {
    int fd; // File descriptor for the camera device
    struct v4l2_format fmt; // Format settings
    struct v4l2_requestbuffers buf_req; // Buffer request structure
    struct buffer *buffers; // Buffer array
    uint32_t buffer_count; // Number of buffers
} st_camera;

int camera_init(st_camera *camera, const char *device_path);
int camera_start_capture(st_camera *camera, uint8_t **frame, size_t *size);
int camera_release(st_camera *camera);
