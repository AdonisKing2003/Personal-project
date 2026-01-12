// example.c - Pure C application using the wrapper
#include "rpi_camera.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

static volatile int keep_running = 1;

void signal_handler(int sig) {
    keep_running = 0;
}

// Callback xử lý frame
void on_frame_received(rpi_frame_t *frame, void *userdata) {
    static int frame_count = 0;
    frame_count++;
    
    printf("Frame #%d: size=%zu bytes, timestamp=%lu, sequence=%u\n",
           frame_count, frame->size, frame->timestamp, frame->sequence);
    
    // TODO: Xử lý frame data tại đây
    // Ví dụ: lưu vào file, phân tích, gửi qua network, etc.
    
    // Ví dụ: Lưu frame đầu tiên ra file
    if (frame_count == 1) {
        FILE *f = fopen("frame_001.yuv", "wb");
        if (f) {
            fwrite(frame->data, 1, frame->size, f);
            fclose(f);
            printf("Saved first frame to frame_001.yuv\n");
        }
    }
    
    // Ví dụ: Tính histogram đơn giản (chỉ lấy Y channel của YUV)
    if (frame_count % 30 == 0) {
        uint8_t *pixels = (uint8_t*)frame->data;
        int histogram[256] = {0};
        
        // Chỉ phân tích 1000 pixels đầu để demo
        for (int i = 0; i < 1000 && i < frame->size; i++) {
            histogram[pixels[i]]++;
        }
        
        // Tìm brightness trung bình
        long sum = 0;
        for (int i = 0; i < 256; i++) {
            sum += i * histogram[i];
        }
        int avg_brightness = sum / 1000;
        printf("  -> Average brightness: %d/255\n", avg_brightness);
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, signal_handler);
    
    printf("=== Raspberry Pi Camera C Wrapper Demo ===\n\n");
    
    // Tạo camera với resolution 640x480, format YUV420
    int width = 640;
    int height = 480;
    
    if (argc >= 3) {
        width = atoi(argv[1]);
        height = atoi(argv[2]);
    }
    
    printf("Creating camera: %dx%d\n", width, height);
    rpi_camera_t *camera = rpi_camera_create(width, height, RPI_FMT_YUV420);
    
    if (!camera) {
        fprintf(stderr, "Failed to create camera\n");
        return 1;
    }
    
    // Cấu hình camera (tùy chọn)
    printf("Configuring camera...\n");
    rpi_camera_set_brightness(camera, 0.0);    // -1.0 to 1.0
    rpi_camera_set_contrast(camera, 1.0);      // 0.0 to 2.0
    rpi_camera_set_exposure(camera, 10000);    // 10ms
    rpi_camera_set_gain(camera, 2.0);          // 2x gain
    
    // Start capture với callback
    printf("Starting camera...\n");
    if (rpi_camera_start(camera, on_frame_received, NULL) < 0) {
        fprintf(stderr, "Failed to start camera\n");
        rpi_camera_destroy(camera);
        return 1;
    }
    
    printf("\nCapturing frames... Press Ctrl+C to stop\n\n");
    
    // Main loop - chỉ đợi signal
    while (keep_running) {
        sleep(1);
    }
    
    printf("\n\nStopping camera...\n");
    rpi_camera_stop(camera);
    rpi_camera_destroy(camera);
    
    printf("Done!\n");
    return 0;
}

/*
===============================================================================
ADVANCED USAGE EXAMPLES
===============================================================================

// 1. Lưu video stream ra file
void save_video_callback(rpi_frame_t *frame, void *userdata) {
    FILE *f = (FILE*)userdata;
    fwrite(frame->data, 1, frame->size, f);
}

FILE *video_file = fopen("output.yuv", "wb");
rpi_camera_start(camera, save_video_callback, video_file);
// ...
fclose(video_file);


// 2. Object detection với OpenCV
#include <opencv2/opencv.hpp>

void opencv_callback(rpi_frame_t *frame, void *userdata) {
    cv::Mat yuv(height + height/2, width, CV_8UC1, frame->data);
    cv::Mat bgr;
    cv::cvtColor(yuv, bgr, cv::COLOR_YUV2BGR_I420);
    
    // TODO: Run detection
    cv::imshow("Camera", bgr);
    cv::waitKey(1);
}


// 3. Network streaming
#include <sys/socket.h>

void network_callback(rpi_frame_t *frame, void *userdata) {
    int sock = *(int*)userdata;
    send(sock, frame->data, frame->size, 0);
}


// 4. Multi-threading với queue
#include <pthread.h>

typedef struct {
    void *data;
    size_t size;
} frame_queue_item_t;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
frame_queue_item_t queue[10];
int queue_head = 0, queue_tail = 0;

void threaded_callback(rpi_frame_t *frame, void *userdata) {
    pthread_mutex_lock(&queue_mutex);
    
    // Copy frame to queue
    queue[queue_tail].data = malloc(frame->size);
    memcpy(queue[queue_tail].data, frame->data, frame->size);
    queue[queue_tail].size = frame->size;
    
    queue_tail = (queue_tail + 1) % 10;
    
    pthread_mutex_unlock(&queue_mutex);
}

// Worker thread xử lý frames
void* worker_thread(void *arg) {
    while (keep_running) {
        pthread_mutex_lock(&queue_mutex);
        
        if (queue_head != queue_tail) {
            frame_queue_item_t item = queue[queue_head];
            queue_head = (queue_head + 1) % 10;
            
            pthread_mutex_unlock(&queue_mutex);
            
            // Process frame
            process_frame(item.data, item.size);
            free(item.data);
        } else {
            pthread_mutex_unlock(&queue_mutex);
            usleep(1000);
        }
    }
    return NULL;
}
*/