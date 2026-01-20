#include "camera.h"
#include "isp.h"
#include <stdio.h>

void save_ppm(const char *filename, st_rgb_image *img) {
    FILE *fp = fopen(filename, "wb");
    if(!fp) {
        perror("Failed to open output file");
        return;
    }

    fprintf(fp, "P6\n%U %U\n255\n", image->width, img->height);
    fwrite(img->data, 1, img->width * img->height * 3, fp);
    fclose(fp);

    printf("Image saved to %s\n", filename);
    printf("Convert PPM to JPEG: convert output_000.ppm output_000.jpg \n");
    printf("Or watch PPM: display output_000.ppm \n");
}

char *device_path = "/dev/video0";

int main(void)
{
    st_camera cam;
    st_isp_params isp_params;

    if(camera_init(&cam, device_path) != 0) {
        printf("[ERROR]: Camera initialization failed\n");
        return -1;
    }
    else {
        printf("Camera initialized successfully\n");
    }

    isp_init(&isp_params);

    for(int i=0; i<10; i++) {
        uint8_t *frame;
        size_t size;
        if(camera_start_capture(&cam, &frame, &size) != 0) {
            printf("[ERROR]: Camera capture failed\n");
            camera_release(&cam);
            return -1;
        }
        printf("Captured frame size: %zu bytes\n", size);
        /* Process with ISP */
        st_rgb_image output;
        if(isp_process_bayer10(raw_frame, frame_size,
                                camera.fmt.fmt.pix.width,
                                camera.fmt.fmt.pix.height,
                                &output, &isp_params) < 0) 
        {
            fprintf(stderr, "ISP processing failed\n");
            continue;
        }

        /* Save result */
        char filename[256];
        snprintf(filename, sizeof(filename), "Output/output_%03d.ppm", i);
        save_ppm(filename, &output);

        isp_free_image(&output);
    }

    camera_release(&cam);
    return 0;
}