#include "camera.h"
#include <stdio.h>

char *device_path = "/dev/video0";

int main()
{
    st_camera cam;
    if(camera_init(&cam, device_path) != 0) {
        printf("[ERROR]: Camera initialization failed\n");
        return -1;
    }
    else {
        printf("Camera initialized successfully\n");
    }
    uint8_t *frame;
    size_t size;
    if(camera_start_capture(&cam, &frame, &size) != 0) {
        printf("[ERROR]: Camera capture failed\n");
        camera_release(&cam);
        return -1;
    }
    printf("Captured frame size: %zu bytes\n", size);
    // Process the frame data in 'frame' buffer as needed
    FILE *fp = fopen("captured_frame.raw", "wb");
    if (fp) {
        fwrite(frame, 1, size, fp);
        fclose(fp);
        printf("[INFO]: Saved raw frame to captured_frame.raw\n");
    }
    else {
        printf("[ERROR]: Failed to open file for writing\n");
    }
    // Do nothing, wait to develop further
    camera_release(&cam);
    return 0;
}