#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <time.h>
#include <stdint.h>

// No libcamera headers needed - uses system calls

typedef struct {
    FILE *pipe;
    int width;
    int height;
    size_t frame_size;
} camera_stream_t;

int capture_image_simple(const char *output_file) {
    char cmd[512];
    
    // Capture a single frame using libcamera-still
    snprintf(cmd, sizeof(cmd), 
             "libcamera-still -o %s --width 640 --height 480 --timeout 1 --nopreview 2>/dev/null",
             output_file);
    
    printf("Capturing image...\n");
    int ret = system(cmd);
    
    if (ret != 0) {
        fprintf(stderr, "Failed to capture image\n");
        return -1;
    }
    
    printf("Image captured to: %s\n", output_file);
    return 0;
}

int capture_video_frames(const char *output_file, int duration_ms) {
    char cmd[512];
    
    // Capture video using libcamera-vid
    snprintf(cmd, sizeof(cmd),
             "libcamera-vid -o %s --width 640 --height 480 -t %d --nopreview --codec yuv420 2>/dev/null",
             output_file, duration_ms);
    
    printf("Capturing video...\n");
    int ret = system(cmd);
    
    if (ret != 0) {
        fprintf(stderr, "Failed to capture video\n");
        return -1;
    }
    
    printf("Video captured to: %s\n", output_file);
    return 0;
}

camera_stream_t* camera_stream_start(int width, int height) {
    camera_stream_t *stream = malloc(sizeof(camera_stream_t));
    if (!stream) {
        return NULL;
    }
    
    stream->width = width;
    stream->height = height;
    stream->frame_size = width * height * 3 / 2; // YUV420 format
    
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "libcamera-vid --width %d --height %d -t 0 --nopreview "
             "--codec yuv420 -o - 2>/dev/null",
             width, height);
    
    stream->pipe = popen(cmd, "r");
    if (!stream->pipe) {
        fprintf(stderr, "Failed to start camera stream\n");
        free(stream);
        return NULL;
    }
    
    printf("Camera stream started: %dx%d\n", width, height);
    return stream;
}

int camera_stream_read_frame(camera_stream_t *stream, uint8_t *buffer) {
    if (!stream || !stream->pipe || !buffer) {
        return -1;
    }
    
    size_t bytes_read = fread(buffer, 1, stream->frame_size, stream->pipe);
    if (bytes_read != stream->frame_size) {
        if (feof(stream->pipe)) {
            fprintf(stderr, "Camera stream ended\n");
            return -1;
        }
        fprintf(stderr, "Incomplete frame read: %zu/%zu bytes\n", 
                bytes_read, stream->frame_size);
        return -1;
    }
    
    return 0;
}

void camera_stream_stop(camera_stream_t *stream) {
    if (stream) {
        if (stream->pipe) {
            pclose(stream->pipe);
        }
        free(stream);
    }
    printf("Camera stream stopped\n");
}

// Example usage
int main() {
    printf("=== Libcamera Simple Camera Test ===\n\n");
    
    // Example 1: Capture a single image
    printf("Example 1: Capturing single image...\n");
    if (capture_image_simple("test_image.jpg") == 0) {
        printf("Success!\n\n");
    }
    
    // Example 2: Capture short video
    printf("Example 2: Capturing 2 second video...\n");
    if (capture_video_frames("test_video.yuv", 2000) == 0) {
        printf("Success!\n\n");
    }
    
    // Example 3: Stream frames continuously
    printf("Example 3: Streaming frames for 5 seconds...\n");
    camera_stream_t *stream = camera_stream_start(640, 480);
    if (stream) {
        uint8_t *frame_buffer = malloc(stream->frame_size);
        if (frame_buffer) {
            int frame_count = 0;
            time_t start_time = time(NULL);
            
            while (time(NULL) - start_time < 5) {
                if (camera_stream_read_frame(stream, frame_buffer) == 0) {
                    frame_count++;
                    printf("\rCaptured frame %d", frame_count);
                    fflush(stdout);
                    
                    // Save first frame for testing
                    if (frame_count == 1) {
                        FILE *fp = fopen("frame_001.yuv", "wb");
                        if (fp) {
                            fwrite(frame_buffer, 1, stream->frame_size, fp);
                            fclose(fp);
                            printf("\nSaved frame_001.yuv\n");
                        }
                    }
                } else {
                    break;
                }
            }
            
            printf("\nTotal frames captured: %d\n", frame_count);
            free(frame_buffer);
        }
        camera_stream_stop(stream);
    }
    
    printf("\nAll tests completed!\n");
    return 0;
}