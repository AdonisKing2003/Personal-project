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
    printf("Camera device opened successfully\n");

    // Query current format first
    memset(&camera->fmt, 0, sizeof(struct v4l2_format));
    camera->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    if (ioctl(camera->fd, VIDIOC_G_FMT, &camera->fmt) < 0) {
        perror("[ERROR]: Failed to get camera format");
        close(camera->fd);
        return -1;
    }
    
    printf("[DEBUG]: Current format: %c%c%c%c\n",
        camera->fmt.fmt.pix.pixelformat & 0xFF,
        (camera->fmt.fmt.pix.pixelformat >> 8) & 0xFF,
        (camera->fmt.fmt.pix.pixelformat >> 16) & 0xFF,
        (camera->fmt.fmt.pix.pixelformat >> 24) & 0xFF);

    // Set desired resolution but keep the pixel format
    camera->fmt.fmt.pix.width = CAMERA_RESOLUTION_WIDTH;
    camera->fmt.fmt.pix.height = CAMERA_RESOLUTION_HEIGHT;
    camera->fmt.fmt.pix.field = V4L2_FIELD_NONE;
    // camera->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SBGGR10;
    // Don't change pixelformat - use what the driver provides

    if (ioctl_with_retry(camera->fd, VIDIOC_S_FMT, &camera->fmt) < 0) {
        perror("[ERROR]: Failed to set camera format");
        close(camera->fd);
        return -1;
    }
    else {
        printf("Camera format set successfully: %dx%d\n", 
               camera->fmt.fmt.pix.width, camera->fmt.fmt.pix.height);
        printf("[DEBUG]: Driver format: %c%c%c%c\n",
            camera->fmt.fmt.pix.pixelformat & 0xFF,
            (camera->fmt.fmt.pix.pixelformat >> 8) & 0xFF,
            (camera->fmt.fmt.pix.pixelformat >> 16) & 0xFF,
            (camera->fmt.fmt.pix.pixelformat >> 24) & 0xFF);

        printf("[DEBUG]: Field: %d\n", camera->fmt.fmt.pix.field);
        printf("[DEBUG]: Field enum value = %d\n", camera->fmt.fmt.pix.field);

        switch (camera->fmt.fmt.pix.field) {
        case V4L2_FIELD_NONE:
            printf("[DEBUG]: Field = NONE\n");
            break;
        case V4L2_FIELD_INTERLACED:
            printf("[DEBUG]: Field = INTERLACED\n");
            break;
        case V4L2_FIELD_ANY:
            printf("[DEBUG]: Field = ANY\n");
            break;
        default:
            printf("[DEBUG]: Field = OTHER (%d)\n",
                camera->fmt.fmt.pix.field);
        }
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
    else {
        printf("Requested %d buffers successfully\n", camera->buf_req.count);
    }
    camera->buffers = calloc(camera->buf_req.count, sizeof(struct buffer));

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
    printf("All buffers queued successfully\n");

    printf("[DEBUG]: Buffers requested: %u\n", camera->buf_req.count);
    printf("[DEBUG]: Buffers queued: %u\n", camera->buffer_count);
    struct v4l2_capability cap;
    if (ioctl(camera->fd, VIDIOC_QUERYCAP, &cap) == -1) {
        perror("[ERROR]: Querying Capabilities");
        return -1;
    }
    
    printf("[DEBUG]: Driver: %s\n", cap.driver);
    printf("[DEBUG]: Card: %s\n", cap.card);
    printf("[DEBUG]: Capabilities: 0x%08x\n", cap.capabilities);
    
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        printf("[ERROR]: Device does not support video capture\n");
        return -1;
    }
    
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        printf("[ERROR]: Device does not support streaming\n");
        return -1;
    }

    // ADD THIS: Verify format before streaming
    struct v4l2_format verify_fmt;
    memset(&verify_fmt, 0, sizeof(verify_fmt));
    verify_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    if (ioctl(camera->fd, VIDIOC_G_FMT, &verify_fmt) == -1) {
        perror("[ERROR]: Failed to verify format");
        return -1;
    }
    
    printf("[DEBUG]: Verified format before streaming:\n");
    printf("  Width: %u\n", verify_fmt.fmt.pix.width);
    printf("  Height: %u\n", verify_fmt.fmt.pix.height);
    printf("  Pixelformat: %c%c%c%c\n",
        verify_fmt.fmt.pix.pixelformat & 0xFF,
        (verify_fmt.fmt.pix.pixelformat >> 8) & 0xFF,
        (verify_fmt.fmt.pix.pixelformat >> 16) & 0xFF,
        (verify_fmt.fmt.pix.pixelformat >> 24) & 0xFF);
    printf("  Bytesperline: %u\n", verify_fmt.fmt.pix.bytesperline);
    printf("  Sizeimage: %u\n", verify_fmt.fmt.pix.sizeimage);


    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(camera->fd, VIDIOC_STREAMON, &type) == -1) {
        perror("[ERROR]: STREAMON");
        
        // ADD THIS: More detailed error info
        printf("[ERROR]: errno = %d (%s)\n", errno, strerror(errno));
        printf("[ERROR]: This usually means:\n");
        printf("  - Wrong video device (try /dev/video1, /dev/video2, etc.)\n");
        printf("  - Format not fully supported\n");
        printf("  - Device doesn't support MMAP streaming\n");
        
        return -1;
    }
    printf("Camera streaming started\n");
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
    printf("Starting camera capture...\n");
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    // Dequeue a buffer
    if (ioctl_with_retry(camera->fd, VIDIOC_DQBUF, &buf) == -1) {
        perror("[ERROR]: Dequeue Buffer");
        return -1;
    }
    else {
        printf("Buffer dequeued successfully\n");
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