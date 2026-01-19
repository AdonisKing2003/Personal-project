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

int validate_yuv_frame(uint8_t *buffer, int width, int height) {
    // YUV420: Y plane phải có giá trị hợp lý (16-235 cho video)
    // U,V planes có thể check range
    
    size_t y_size = width * height;
    
    // Quick sanity check
    if (buffer[0] < 5 || buffer[0] > 250) {
        return 0;  // Invalid
    }
    
    if (buffer[y_size] < 5 || buffer[y_size] > 250) {
        return 0;  // Invalid U plane start
    }
    
    return 1;  // Looks valid
}

int capture_video_frames(const char *output_file, int duration_ms) {
    char cmd[512];
    
    // Capture video using libcamera-vid
    // Thêm flag --verbose vào libcamera-vid để có frame info
    snprintf(cmd, sizeof(cmd),
         "libcamera-vid -o - --width 640 --height 480 "
         "-t %d --nopreview --codec yuv420 --flush 2>/dev/null",
         duration_ms);
    
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

    // DISCARD first frame for sync
    uint8_t *discard_buffer = malloc(stream->frame_size);
    if (discard_buffer) {
        fread(discard_buffer, 1, stream->frame_size, stream->pipe);
        free(discard_buffer);
    }
    
    printf("Camera stream started: %dx%d\n", width, height);
    return stream;
}

int camera_stream_read_frame(camera_stream_t *stream, uint8_t *buffer) {
    size_t total_read = 0;
    int retry_count = 0;
    const int MAX_RETRIES = 10;
    static int frame_count = 0;
    printf("Reading frame %d: expecting %zu bytes\n", 
        ++frame_count, stream->frame_size);
    
    while (total_read < stream->frame_size) {
        size_t remaining = stream->frame_size - total_read;
        size_t bytes_read = fread(
            buffer + total_read,
            1,
            remaining,
            stream->pipe
        );
        
        if (bytes_read == 0) {
            if (feof(stream->pipe)) {
                fprintf(stderr, "End of stream\n");
                return -1;
            }
            
            if (ferror(stream->pipe)) {
                fprintf(stderr, "Stream error\n");
                return -1;
            }
            
            // Data not ready yet
            if (++retry_count >= MAX_RETRIES) {
                fprintf(stderr, "Timeout reading frame\n");
                return -1;
            }
            
            usleep(5000);  // Wait 5ms
            continue;
        }
        
        total_read += bytes_read;
        retry_count = 0;  // Reset on successful read
    }
    printf("Frame %d: read %zu bytes (Y[0]=%d, U[0]=%d)\n",
       frame_count, total_read, 
       buffer[0], 
       buffer[stream->width * stream->height]);
       
    // Optional: Validate frame
    if (!validate_yuv_frame(buffer, stream->width, stream->height)) {
        fprintf(stderr, "Invalid frame detected\n");
        return -1;
    }
    else {
        printf("[INFO]: Validate yuv frame passed!\n");
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