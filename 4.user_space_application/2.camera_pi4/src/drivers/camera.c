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

// Hàm tìm đúng /dev/mediaX chứa sensor
int find_media_device(const char *pattern, char *out_media, char *out_entity) {
    char cmd[256];
    char line[512];
    int found = 0;

    // Quét qua các media device hiện có
    for (int i = 0; i < 10; i++) {
        sprintf(cmd, "media-ctl -d /dev/media%d -p 2>/dev/null", i);
        FILE *fp = popen(cmd, "r");
        if (!fp) continue;

        while (fgets(line, sizeof(line), fp)) {
            // Tìm dòng chứa sensor (ví dụ: ov5647)
            if (strstr(line, "entity") && strstr(line, pattern)) {
                // Parse lấy tên đầy đủ nằm giữa "entity X: " và " ("
                char *start = strchr(line, ':');
                if (start) {
                    start += 2; // Bỏ qua ": "
                    char *end = strchr(start, '(');
                    if (end) {
                        strncpy(out_entity, start, end - start - 1);
                        out_entity[end - start - 1] = '\0';
                        sprintf(out_media, "/dev/media%d", i);
                        found = 1;
                        break;
                    }
                }
            }
        }
        pclose(fp);
        if (found) return 0;
    }
    return -1;
}

/* Helper: Setup media controller pipeline */
int setup_media_pipeline(const char *unused_pattern) {
    char cmd[512];
    
    printf("[V4L2]: Pipeline is IMMUTABLE, skipping links. Setting formats...\n");

    /* 1. Set format cho Sensor (Entity 1) */
    /* Lưu ý: Dùng đúng định dạng SGBRG10 từ log media-ctl của bạn */
    snprintf(cmd, sizeof(cmd), 
        "media-ctl -d /dev/media4 --set-v4l2 '\"ov5647 10-0036\":0[fmt:SGBRG10_1X10/640x480]'");
    system(cmd);

    /* 2. Kiểm tra node video */
    printf("[V4L2]: Pipeline configured. Use /dev/video0 for capture.\n");
    return 0;
}

/* Helper: Detect camera sensor */
int detect_camera_sensor(char *sensor_name, size_t max_len) {
    FILE *fp = popen("for d in /dev/media*; do "
                 "media-ctl -d $d -p 2>/dev/null | grep -o 'ov5647\\|imx219\\|imx477\\|imx708' && break; "
                 "done | head -n1", "r");
    if(!fp) return -1;

    if(fgets(sensor_name, max_len, fp) == NULL) {
        pclose(fp);
        return -1;
    }

    /* remove newline */
    sensor_name[strcspn(sensor_name, "\n")] = 0;
    pclose(fp);

    printf("[V4L2]: Detected sensor: %s !\n", sensor_name);
    return 0;
}

int camera_init(st_camera *camera, const char *device_path) {
    memset(camera, 0, sizeof(st_camera));

    /* STEP 1: Detect and setup media pipeline */
    char sensor_name[64];
    if(detect_camera_sensor(sensor_name, sizeof(sensor_name)) < 0) {
        fprintf(stderr, "[EROR]: No camera sensor detected\n");
        fprintf(stderr, "Make sure camera is connected and enabled in /boot/config.txt\n");
        return -1;
    }

    if(setup_media_pipeline(sensor_name) < 0) {
        fprintf(stderr, "[ERROR]: Failed to setup media pipeline\n");
    }

    /* Give kernel time to setup */
    usleep(100000); /* 100ms */

    // STEP 2: Open camera device
    camera->fd = open(device_path, O_RDWR);
    if (camera->fd < 0) {
        perror("[ERROR]: Failed to open camera device");
        fprintf(stderr, "Tried to open: %s\n", device_path);
        fprintf(stderr, "Run 'ls -l /dev/video*' to see available devices\n");
        return -1;
    }
    printf("Camera device opened successfully\n");

    // STEP 3: Query capabilities
    struct v4l2_capability cap;
    if(ioctl(camera->fd, VIDIOC_QUERYCAP, &cap) == -1) {
        perror("[ERROR]: VIDIOC_QUERYCAP");
        close(camera->fd);
        return -1;
    }
    printf("[V4L2]: Driver: %s\n", cap.driver);
    printf("[V4L2]: Card: %s\n", cap.card);
    printf("[V4L2]: Bus: %s\n", cap.bus_info);
    printf("[V4L2]: Capabilities: 0x%08x\n", cap.capabilities);

    if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "[ERROR]: Does not support streaming I/O\n");
        close(camera->fd);
        return -1;
    }

    /* STEP 4: Get current format */
    memset(&camera->fmt, 0, sizeof(struct v4l2_format));
    camera->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    if (ioctl(camera->fd, VIDIOC_G_FMT, &camera->fmt) < 0) {
        perror("[ERROR]: Failed to get camera format");
        close(camera->fd);
        return -1;
    }
    
    printf("[V4L2]: Current format:\n");
    printf("        Width: %u\n", camera->fmt.fmt.pix.width);
    printf("        Height: %u\n", camera->fmt.fmt.pix.height);
    printf("        Pixel Format: %.4s\n", 
           (char*)&camera->fmt.fmt.pix.pixelformat);
    printf("        Bytes per line: %u\n", camera->fmt.fmt.pix.bytesperline);
    printf("        Size image: %u\n", camera->fmt.fmt.pix.sizeimage);

    /* STEP 5: Set desired format */

    // Set desired resolution but keep the pixel format
    camera->fmt.fmt.pix.width = CAMERA_RESOLUTION_WIDTH;
    camera->fmt.fmt.pix.height = CAMERA_RESOLUTION_HEIGHT;
    camera->fmt.fmt.pix.field = V4L2_FIELD_NONE;
    camera->fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SGBRG10; // Tương ứng với 'GB10'

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