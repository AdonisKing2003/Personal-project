#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/mman.h>   // mmap, munmap, PROT_*, MAP_*, MAP_FAILED
#include "camera.h"
#include "camera_config.h"

static int xioctl(int fd, int request, void *arg)
{
    int r;

    do {
        r = ioctl(fd, request, arg);
    } while (r == -1 && errno == EINTR);

    return r;
}

/**
* @brief Safe wrapper for ioctl system call.
* @param fd File descriptor
* @param request IOCTL request code
* @param arg Pointer to the argument for the IOCTL call
* @return Result of the IOCTL call:
*         - On success, returns the result of the ioctl call.
*         - On failure, returns -1 and sets errno appropriately.
*/
static int ioctl_with_retry(int fd, int request, void *arg) {
    int r;
    do {
        r = ioctl(fd, request, arg);
    } while (r == -1 && errno == EINTR);
    return r;
}

int camera_init(st_camera *camera, const char *device_path) {
    memset(camera, 0, sizeof(st_camera));

    // Open camera device
    camera->fd = open(device_path, O_RDWR);
    if (camera->fd < 0) {
        perror("[ERROR]: Failed to open camera device");
        return -1;
    }

    // Set camera format
    memset(&camera->fmt, 0, sizeof(struct v4l2_format));
    camera->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    camera->fmt.fmt.pix.width = CAMERA_RESOLUTION_WIDTH;
    camera->fmt.fmt.pix.height = CAMERA_RESOLUTION_HEIGHT;
    camera->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV; // Assuming YUYV format
    camera->fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (ioctl_with_retry(camera->fd, VIDIOC_S_FMT, &camera->fmt) < 0) {
        perror("[ERROR]: Failed to set camera format");
        close(camera->fd);
        return -1;
    }

    // Request buffers
    struct v4l2_requestbuffers buf_req;
    memset(&buf_req, 0, sizeof(buf_req));
    buf_req.count = 4; // Request 4 buffers
    buf_req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf_req.memory = V4L2_MEMORY_MMAP;
    camera->buf_req = buf_req;

    if (xioctl(camera->fd, VIDIOC_REQBUFS, &camera->buf_req) < 0) {
        perror("[ERROR]: Failed to request buffers");
        close(camera->fd);
        return -1;
    }
    camera->buffers = calloc(camera->buf_req.count, sizeof(camera->buf_req.count));

    for (camera->buffer_count = 0; camera->buffer_count < camera->buf_req.count; camera->buffer_count++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = camera->buffer_count;

        if (ioctl_with_retry(camera->fd, VIDIOC_QUERYBUF, &buf) == -1) {
            perror("[ERROR]: Querying Buffer");
            return -1;
        }

        camera->buffers[camera->buffer_count].length = buf.length;
        camera->buffers[camera->buffer_count].start = mmap(NULL, buf.length,
                                                     PROT_READ | PROT_WRITE, MAP_SHARED,
                                                     camera->fd, buf.m.offset);
        if (camera->buffers[camera->buffer_count].start == MAP_FAILED) {
            perror("[ERROR]: mmap");
            return -1;
        }

        if (xioctl(camera->fd, VIDIOC_QBUF, &buf) == -1) {
            perror("[ERROR]: Queue Buffer");
            return -1;
        }
    }

    return 0;
}

/** 
*@brief lifecycle of buffer during capture:
Queue (QBUF) → buffer is ready for capture.
Driver fills buffer with a frame.
Dequeue (DQBUF) → you get the buffer and process the frame.
Requeue (QBUF) → buffer goes back to the driver for reuse.
*/
int camera_start_capture(st_camera *camera, uint8_t **frame, size_t *size) {
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    // Dequeue a buffer
    if (ioctl_with_retry(camera->fd, VIDIOC_DQBUF, &buf) == -1) {
        perror("[ERROR]: Dequeue Buffer");
        return -1;
    }

    // Process the frame (for demonstration, we just print the buffer index)
    printf("Captured frame in buffer index: %d\n", buf.index);
    *frame = camera->buffers[buf.index].start;
    *size = camera->buffers[buf.index].length;

    // Re-queue the buffer
    if (ioctl_with_retry(camera->fd, VIDIOC_QBUF, &buf) == -1) {
        perror("[ERROR]: Re-queue Buffer");
        return -1;
    }
    
    return 0;
}

int camera_release(st_camera *camera) {
    // Unmap buffers
    for (uint32_t i = 0; i < camera->buffer_count; i++) {
        munmap(camera->buffers[i].start, camera->buffers[i].length);
    }
    free(camera->buffers);

    // Close camera device
    if (camera->fd >= 0) {
        close(camera->fd);
        camera->fd = -1;
    }

    return 0;
}