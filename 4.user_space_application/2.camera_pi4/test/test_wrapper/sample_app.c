// sample_camera_app.c - Complete camera application demo
// Demonstrates all RPI camera wrapper features

#include "rpi_camera.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdbool.h>

// ============================================================================
// Configuration
// ============================================================================
#define DEFAULT_WIDTH       1280
#define DEFAULT_HEIGHT      720
#define DEFAULT_FORMAT      RPI_FMT_YUV420
#define DEFAULT_FPS         30
#define CAPTURE_DURATION    10  // seconds
#define OUTPUT_DIR          "./captured_frames"
#define SAVE_INTERVAL       30  // Save every 30 frames (1 second at 30fps)

// ============================================================================
// Application State
// ============================================================================
typedef struct {
    // Camera handle
    rpi_camera_t *camera;
    
    // Statistics
    int total_frames;
    int saved_frames;
    uint64_t first_timestamp;
    uint64_t last_timestamp;
    
    // Frame info
    size_t min_frame_size;
    size_t max_frame_size;
    uint64_t total_bytes;
    
    // Control
    volatile bool running;
    volatile bool save_enabled;
    
    // Settings
    int width;
    int height;
    rpi_format_t format;
    
    // Adjustable controls
    float brightness;
    float contrast;
    int exposure;
    float gain;
    
    // Thread safety
    pthread_mutex_t stats_mutex;
    
} app_state_t;

// Global state for signal handler
static app_state_t *g_app_state = NULL;

// ============================================================================
// Utility Functions
// ============================================================================

// Get current time in milliseconds
static uint64_t get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// Create output directory
static int create_output_dir(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755) == -1) {
            perror("mkdir");
            return -1;
        }
    }
    return 0;
}

// Calculate average brightness of YUV420 frame
static unsigned char calculate_brightness(const void *data, size_t size) {
    const unsigned char *y_plane = (const unsigned char *)data;
    size_t y_size = size * 2 / 3;  // Y plane is 2/3 of total
    
    unsigned long sum = 0;
    size_t samples = 0;
    
    // Sample every 100th pixel
    for (size_t i = 0; i < y_size; i += 100) {
        sum += y_plane[i];
        samples++;
    }
    
    return (unsigned char)(sum / samples);
}

// Save frame to file
static int save_frame_to_file(const app_state_t *state, 
                              const rpi_frame_t *frame) {
    char filename[256];
    const char *format_ext;
    
    // Determine file extension based on format
    switch (state->format) {
        case RPI_FMT_YUV420:
            format_ext = "yuv";
            break;
        case RPI_FMT_RGB888:
            format_ext = "rgb";
            break;
        case RPI_FMT_MJPEG:
            format_ext = "jpg";
            break;
        default:
            format_ext = "raw";
    }
    
    // Create filename with timestamp
    snprintf(filename, sizeof(filename), 
             "%s/frame_%04d_seq%u.%s",
             OUTPUT_DIR, 
             state->saved_frames,
             frame->sequence,
             format_ext);
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("fopen");
        return -1;
    }
    
    size_t written = fwrite(frame->data, 1, frame->size, fp);
    fclose(fp);
    
    if (written != frame->size) {
        fprintf(stderr, "Warning: Incomplete write (%zu/%zu bytes)\n",
                written, frame->size);
        return -1;
    }
    
    return 0;
}

// ============================================================================
// Frame Callback
// ============================================================================
static void frame_callback(rpi_frame_t *frame, void *userdata) {
    app_state_t *state = (app_state_t *)userdata;
    
    if (!state || !state->running) {
        return;
    }
    
    // Lock for thread-safe statistics update
    pthread_mutex_lock(&state->stats_mutex);
    
    // First frame initialization
    if (state->total_frames == 0) {
        state->first_timestamp = frame->timestamp;
        state->min_frame_size = frame->size;
        state->max_frame_size = frame->size;
        
        printf("\n┌─────────────────────────────────────────┐\n");
        printf("│ First frame captured!                   │\n");
        printf("├─────────────────────────────────────────┤\n");
        printf("│ Sequence:  %8u                     │\n", frame->sequence);
        printf("│ Size:      %8zu bytes               │\n", frame->size);
        printf("│ Timestamp: %llu                         │\n", frame->timestamp);
        printf("└─────────────────────────────────────────┘\n\n");
    }
    
    // Update statistics
    state->total_frames++;
    state->last_timestamp = frame->timestamp;
    state->total_bytes += frame->size;
    
    if (frame->size < state->min_frame_size) {
        state->min_frame_size = frame->size;
    }
    if (frame->size > state->max_frame_size) {
        state->max_frame_size = frame->size;
    }
    
    // Calculate current FPS
    double duration_sec = (frame->timestamp - state->first_timestamp) / 1e9;
    double current_fps = duration_sec > 0 ? state->total_frames / duration_sec : 0;
    
    // Print progress every 30 frames
    if (state->total_frames % 30 == 0) {
        double avg_size = (double)state->total_bytes / state->total_frames;
        unsigned char brightness = 0;
        
        // Calculate brightness for YUV420
        if (state->format == RPI_FMT_YUV420) {
            brightness = calculate_brightness(frame->data, frame->size);
        }
        
        printf("Frame %5d | FPS: %5.1f | Size: %7zu B | Avg: %7.0f B",
               state->total_frames, current_fps, frame->size, avg_size);
        
        if (state->format == RPI_FMT_YUV420) {
            printf(" | Brightness: %3u/255", brightness);
        }
        
        printf("\n");
    }
    
    // Save frame periodically if enabled
    if (state->save_enabled && (state->total_frames % SAVE_INTERVAL == 0)) {
        if (save_frame_to_file(state, frame) == 0) {
            state->saved_frames++;
            printf("  → Saved frame to: %s/frame_%04d_seq%u.*\n",
                   OUTPUT_DIR, state->saved_frames - 1, frame->sequence);
        }
    }
    
    pthread_mutex_unlock(&state->stats_mutex);
}

// ============================================================================
// Signal Handler
// ============================================================================
static void signal_handler(int signum) {
    (void)signum;  // Unused
    
    if (g_app_state) {
        printf("\n\n⚠ Signal received, stopping camera...\n");
        g_app_state->running = false;
    }
}

// ============================================================================
// Print Statistics
// ============================================================================
static void print_statistics(const app_state_t *state) {
    double duration_sec = (state->last_timestamp - state->first_timestamp) / 1e9;
    double avg_fps = duration_sec > 0 ? state->total_frames / duration_sec : 0;
    double avg_size = (double)state->total_bytes / state->total_frames;
    double total_mb = state->total_bytes / (1024.0 * 1024.0);
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║                    CAPTURE STATISTICS                     ║\n");
    printf("╠═══════════════════════════════════════════════════════════╣\n");
    printf("║ Total Frames:     %8d                                ║\n", state->total_frames);
    printf("║ Saved Frames:     %8d                                ║\n", state->saved_frames);
    printf("║ Duration:         %8.2f seconds                       ║\n", duration_sec);
    printf("║ Average FPS:      %8.2f                               ║\n", avg_fps);
    printf("╠═══════════════════════════════════════════════════════════╣\n");
    printf("║ Total Data:       %8.2f MB                            ║\n", total_mb);
    printf("║ Average Size:     %8.0f bytes/frame                   ║\n", avg_size);
    printf("║ Min Size:         %8zu bytes                          ║\n", state->min_frame_size);
    printf("║ Max Size:         %8zu bytes                          ║\n", state->max_frame_size);
    printf("╠═══════════════════════════════════════════════════════════╣\n");
    printf("║ Resolution:       %4dx%-4d                             ║\n", 
           state->width, state->height);
    
    const char *format_name;
    switch (state->format) {
        case RPI_FMT_YUV420: format_name = "YUV420"; break;
        case RPI_FMT_RGB888: format_name = "RGB888"; break;
        case RPI_FMT_MJPEG:  format_name = "MJPEG";  break;
        default:             format_name = "Unknown"; break;
    }
    printf("║ Format:           %-8s                               ║\n", format_name);
    printf("╠═══════════════════════════════════════════════════════════╣\n");
    printf("║ Brightness:       %8.2f                               ║\n", state->brightness);
    printf("║ Contrast:         %8.2f                               ║\n", state->contrast);
    printf("║ Exposure:         %8d µs                             ║\n", state->exposure);
    printf("║ Gain:             %8.2f                               ║\n", state->gain);
    printf("╚═══════════════════════════════════════════════════════════╝\n");
}

// ============================================================================
// Interactive Menu
// ============================================================================
static void print_menu() {
    printf("\n");
    printf("┌───────────────────────────────────────────┐\n");
    printf("│         CAMERA CONTROL MENU               │\n");
    printf("├───────────────────────────────────────────┤\n");
    printf("│ b - Adjust brightness                     │\n");
    printf("│ c - Adjust contrast                       │\n");
    printf("│ e - Adjust exposure                       │\n");
    printf("│ g - Adjust gain                           │\n");
    printf("│ s - Toggle frame saving (ON/OFF)          │\n");
    printf("│ i - Show current settings                 │\n");
    printf("│ q - Quit                                  │\n");
    printf("└───────────────────────────────────────────┘\n");
    printf("Enter command: ");
    fflush(stdout);
}

// ============================================================================
// Interactive Control Thread
// ============================================================================
static void* control_thread(void *arg) {
    app_state_t *state = (app_state_t *)arg;
    char cmd;
    
    // Give camera time to start
    sleep(2);
    
    while (state->running) {
        print_menu();
        
        if (scanf(" %c", &cmd) != 1) {
            continue;
        }
        
        switch (cmd) {
            case 'b': {
                printf("Enter brightness (-1.0 to 1.0): ");
                float brightness;
                if (scanf("%f", &brightness) == 1) {
                    if (rpi_camera_set_brightness(state->camera, brightness) == 0) {
                        state->brightness = brightness;
                        printf("✓ Brightness set to %.2f\n", brightness);
                    } else {
                        printf("✗ Failed to set brightness\n");
                    }
                }
                break;
            }
            
            case 'c': {
                printf("Enter contrast (0.0 to 2.0): ");
                float contrast;
                if (scanf("%f", &contrast) == 1) {
                    if (rpi_camera_set_contrast(state->camera, contrast) == 0) {
                        state->contrast = contrast;
                        printf("✓ Contrast set to %.2f\n", contrast);
                    } else {
                        printf("✗ Failed to set contrast\n");
                    }
                }
                break;
            }
            
            case 'e': {
                printf("Enter exposure time (microseconds, e.g., 10000): ");
                int exposure;
                if (scanf("%d", &exposure) == 1) {
                    if (rpi_camera_set_exposure(state->camera, exposure) == 0) {
                        state->exposure = exposure;
                        printf("✓ Exposure set to %d µs\n", exposure);
                    } else {
                        printf("✗ Failed to set exposure\n");
                    }
                }
                break;
            }
            
            case 'g': {
                printf("Enter gain (1.0 to 16.0): ");
                float gain;
                if (scanf("%f", &gain) == 1) {
                    if (rpi_camera_set_gain(state->camera, gain) == 0) {
                        state->gain = gain;
                        printf("✓ Gain set to %.2f\n", gain);
                    } else {
                        printf("✗ Failed to set gain\n");
                    }
                }
                break;
            }
            
            case 's': {
                state->save_enabled = !state->save_enabled;
                printf("✓ Frame saving: %s\n", 
                       state->save_enabled ? "ENABLED" : "DISABLED");
                break;
            }
            
            case 'i': {
                printf("\n┌─────────────────────────────────────┐\n");
                printf("│      Current Settings               │\n");
                printf("├─────────────────────────────────────┤\n");
                printf("│ Brightness:  %6.2f                 │\n", state->brightness);
                printf("│ Contrast:    %6.2f                 │\n", state->contrast);
                printf("│ Exposure:    %6d µs              │\n", state->exposure);
                printf("│ Gain:        %6.2f                 │\n", state->gain);
                printf("│ Saving:      %s                    │\n", 
                       state->save_enabled ? "ON " : "OFF");
                printf("└─────────────────────────────────────┘\n");
                break;
            }
            
            case 'q': {
                printf("Quitting...\n");
                state->running = false;
                return NULL;
            }
            
            default:
                printf("Unknown command: %c\n", cmd);
                break;
        }
    }
    
    return NULL;
}

// ============================================================================
// Main Application
// ============================================================================
int main(int argc, char *argv[]) {
    int ret;
    pthread_t ctrl_thread;
    
    // ========================================================================
    // 1. Print banner
    // ========================================================================
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║         RPI CAMERA WRAPPER - SAMPLE APPLICATION          ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // ========================================================================
    // 2. Initialize application state
    // ========================================================================
    app_state_t state = {
        .camera = NULL,
        .total_frames = 0,
        .saved_frames = 0,
        .first_timestamp = 0,
        .last_timestamp = 0,
        .min_frame_size = 0,
        .max_frame_size = 0,
        .total_bytes = 0,
        .running = true,
        .save_enabled = false,
        .width = DEFAULT_WIDTH,
        .height = DEFAULT_HEIGHT,
        .format = DEFAULT_FORMAT,
        .brightness = 0.0f,
        .contrast = 1.0f,
        .exposure = 10000,  // 10ms
        .gain = 1.0f,
    };
    
    pthread_mutex_init(&state.stats_mutex, NULL);
    g_app_state = &state;
    
    // Parse command line arguments (simple)
    if (argc >= 3) {
        state.width = atoi(argv[1]);
        state.height = atoi(argv[2]);
    }
    
    if (argc >= 4) {
        if (strcmp(argv[3], "yuv") == 0) {
            state.format = RPI_FMT_YUV420;
        } else if (strcmp(argv[3], "rgb") == 0) {
            state.format = RPI_FMT_RGB888;
        } else if (strcmp(argv[3], "mjpeg") == 0) {
            state.format = RPI_FMT_MJPEG;
        }
    }
    
    // ========================================================================
    // 3. Create output directory
    // ========================================================================
    printf("→ Creating output directory: %s\n", OUTPUT_DIR);
    if (create_output_dir(OUTPUT_DIR) != 0) {
        fprintf(stderr, "✗ Failed to create output directory\n");
        return 1;
    }
    printf("✓ Output directory ready\n\n");
    
    // ========================================================================
    // 4. Setup signal handler
    // ========================================================================
    printf("→ Setting up signal handlers\n");
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    printf("✓ Signal handlers installed (Ctrl+C to stop)\n\n");
    
    // ========================================================================
    // 5. Create camera
    // ========================================================================
    const char *format_name;
    switch (state.format) {
        case RPI_FMT_YUV420: format_name = "YUV420"; break;
        case RPI_FMT_RGB888: format_name = "RGB888"; break;
        case RPI_FMT_MJPEG:  format_name = "MJPEG";  break;
        default:             format_name = "Unknown"; break;
    }
    
    printf("→ Creating camera: %dx%d, %s\n", 
           state.width, state.height, format_name);
    
    state.camera = rpi_camera_create(state.width, state.height, state.format);
    if (!state.camera) {
        fprintf(stderr, "✗ Failed to create camera\n");
        fprintf(stderr, "  Check:\n");
        fprintf(stderr, "    1. Camera is connected\n");
        fprintf(stderr, "    2. Camera is enabled (raspi-config)\n");
        fprintf(stderr, "    3. libcamera is installed\n");
        fprintf(stderr, "    4. User is in 'video' group\n");
        return 1;
    }
    printf("✓ Camera created successfully\n\n");
    
    // ========================================================================
    // 6. Configure camera controls
    // ========================================================================
    printf("→ Configuring camera controls\n");
    
    ret = rpi_camera_set_brightness(state.camera, state.brightness);
    if (ret == 0) {
        printf("  ✓ Brightness: %.2f\n", state.brightness);
    } else {
        printf("  ⚠ Brightness: failed\n");
    }
    
    ret = rpi_camera_set_contrast(state.camera, state.contrast);
    if (ret == 0) {
        printf("  ✓ Contrast: %.2f\n", state.contrast);
    } else {
        printf("  ⚠ Contrast: failed\n");
    }
    
    ret = rpi_camera_set_exposure(state.camera, state.exposure);
    if (ret == 0) {
        printf("  ✓ Exposure: %d µs\n", state.exposure);
    } else {
        printf("  ⚠ Exposure: failed\n");
    }
    
    ret = rpi_camera_set_gain(state.camera, state.gain);
    if (ret == 0) {
        printf("  ✓ Gain: %.2f\n", state.gain);
    } else {
        printf("  ⚠ Gain: failed\n");
    }
    
    printf("\n");
    
    // ========================================================================
    // 7. Start camera
    // ========================================================================
    printf("→ Starting camera capture\n");
    ret = rpi_camera_start(state.camera, frame_callback, &state);
    if (ret != 0) {
        fprintf(stderr, "✗ Failed to start camera\n");
        rpi_camera_destroy(state.camera);
        return 1;
    }
    printf("✓ Camera started, capturing frames...\n");
    
    // ========================================================================
    // 8. Start control thread
    // ========================================================================
    printf("→ Starting interactive control\n");
    printf("  (You can adjust settings while capturing)\n\n");
    
    pthread_create(&ctrl_thread, NULL, control_thread, &state);
    
    // ========================================================================
    // 9. Main capture loop
    // ========================================================================
    printf("═══════════════════════════════════════════════════════════\n");
    printf("              CAPTURING - Press 'q' to quit\n");
    printf("═══════════════════════════════════════════════════════════\n\n");
    
    // Wait for user to quit or duration to elapse
    pthread_join(ctrl_thread, NULL);
    
    // ========================================================================
    // 10. Stop camera
    // ========================================================================
    printf("\n→ Stopping camera\n");
    ret = rpi_camera_stop(state.camera);
    if (ret != 0) {
        fprintf(stderr, "⚠ Warning: Stop returned error\n");
    } else {
        printf("✓ Camera stopped\n");
    }
    
    // ========================================================================
    // 11. Print statistics
    // ========================================================================
    if (state.total_frames > 0) {
        print_statistics(&state);
    }
    
    // ========================================================================
    // 12. Cleanup
    // ========================================================================
    printf("\n→ Cleaning up\n");
    rpi_camera_destroy(state.camera);
    pthread_mutex_destroy(&state.stats_mutex);
    printf("✓ Cleanup complete\n");
    
    // ========================================================================
    // 13. Final message
    // ========================================================================
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║                  APPLICATION FINISHED                     ║\n");
    printf("╠═══════════════════════════════════════════════════════════╣\n");
    
    if (state.saved_frames > 0) {
        printf("║ Saved %d frames to: %-33s ║\n", 
               state.saved_frames, OUTPUT_DIR);
        printf("║                                                           ║\n");
        printf("║ View frames with:                                         ║\n");
        
        switch (state.format) {
            case RPI_FMT_YUV420:
                printf("║   ffplay -f rawvideo -pixel_format yuv420p           ║\n");
                printf("║          -video_size %dx%-4d frame_XXXX.yuv       ║\n",
                       state.width, state.height);
                break;
            case RPI_FMT_RGB888:
                printf("║   ffplay -f rawvideo -pixel_format rgb24             ║\n");
                printf("║          -video_size %dx%-4d frame_XXXX.rgb       ║\n",
                       state.width, state.height);
                break;
            case RPI_FMT_MJPEG:
                printf("║   Any image viewer (frame_XXXX.jpg)                   ║\n");
                break;
        }
    }
    
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    return 0;
}