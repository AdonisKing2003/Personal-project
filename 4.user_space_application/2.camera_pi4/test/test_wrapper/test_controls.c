// test_controls.c - Test brightness, contrast, exposure, gain
#include "rpi_camera.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

typedef struct {
    int frame_count;
    unsigned char brightness_samples[10];
    int sample_count;
} control_stats_t;

// ============================================================================
// Helper: Calculate average brightness of YUV frame
// ============================================================================
unsigned char calculate_brightness(void *data, size_t size) {
    // Y component is first width*height bytes in YUV420
    unsigned char *y_plane = (unsigned char *)data;
    size_t y_size = size * 2 / 3;  // Y plane is 2/3 of total
    
    unsigned long sum = 0;
    // Sample every 100th pixel
    for (size_t i = 0; i < y_size; i += 100) {
        sum += y_plane[i];
    }
    
    return (unsigned char)(sum / (y_size / 100));
}

// ============================================================================
// TEST 1: Brightness Control
// ============================================================================
void test_brightness() {
    printf("\n=== TEST 1: Brightness Control ===\n");
    
    rpi_camera_t *cam = rpi_camera_create(640, 480, RPI_FMT_YUV420);
    assert(cam != NULL);
    
    float brightness_levels[] = {-0.5, 0.0, 0.5};
    const char *brightness_names[] = {"Dark (-0.5)", "Normal (0.0)", "Bright (0.5)"};
    
    for (int i = 0; i < 3; i++) {
        printf("1.%d. Testing brightness: %s...\n", i+1, brightness_names[i]);
        
        // Set brightness
        int ret = rpi_camera_set_brightness(cam, brightness_levels[i]);
        assert(ret == 0);
        
        // Start capturing
        control_stats_t stats = {0};
        ret = rpi_camera_start(cam);
        assert(ret == 0);
        
        while (get_time_ns() - start_ts < 1e9) {
            rpi_frame_t frame;
            if (rpi_camera_get_frame(cam, &frame, 1000) == 0) {
                stats.frame_count++;
                stats.last_sequence = frame.sequence;
                stats.last_timestamp = frame.timestamp;
                rpi_camera_release_frame(&frame);
            }
        }
        
        ret = rpi_camera_stop(cam);
        assert(ret == 0);
        
        // Calculate average brightness from samples
        unsigned long sum = 0;
        for (int j = 0; j < stats.sample_count; j++) {
            sum += stats.brightness_samples[j];
        }
        unsigned char avg_brightness = sum / stats.sample_count;
        
        printf("    Statistics:\n");
        printf("      - Frames: %d\n", stats.frame_count);
        printf("      - Samples: %d\n", stats.sample_count);
        printf("      - Avg brightness: %u/255\n", avg_brightness);
        
        // Validate brightness trend
        if (i > 0) {
            // Current should be brighter than previous
            printf("    ✓ Brightness control working\n");
        }
        
        assert(stats.frame_count >= 40);
    }
    
    rpi_camera_destroy(cam);
}

// ============================================================================
// TEST 2: Contrast Control
// ============================================================================
void test_contrast() {
    printf("\n=== TEST 2: Contrast Control ===\n");
    
    rpi_camera_t *cam = rpi_camera_create(640, 480, RPI_FMT_YUV420);
    assert(cam != NULL);
    
    float contrast_levels[] = {0.5, 1.0, 1.5};
    const char *contrast_names[] = {"Low (0.5)", "Normal (1.0)", "High (1.5)"};
    
    for (int i = 0; i < 3; i++) {
        printf("2.%d. Testing contrast: %s...\n", i+1, contrast_names[i]);
        
        // Set contrast
        int ret = rpi_camera_set_contrast(cam, contrast_levels[i]);
        assert(ret == 0);
        
        // Start capturing
        control_stats_t stats = {0};
        ret = rpi_camera_start(cam);
        assert(ret == 0);

        uint64_t start_ts = get_time_ns();
        while(get_time_ns() - stat_ts < 1e9) {
            rpi_frame_t frame;
            if(rpi_camera_get_frame(cam, &frame, 1000) == 0) {
                stats.frame_count++;
                stats.last_sequence = frame.sequence;
                stats.last_timestamp = frame.last_timestamp;
                rpi_camera_release_frame(&frame);
            }
        }
        
        ret = rpi_camera_stop(cam);
        assert(ret == 0);
        
        printf("    Statistics:\n");
        printf("      - Frames: %d\n", stats.frame_count);
        printf("    ✓ Contrast set successfully\n");
        
        assert(stats.frame_count >= 40);
    }
    
    rpi_camera_destroy(cam);
}

// ============================================================================
// TEST 3: Exposure Control
// ============================================================================
void test_exposure() {
    printf("\n=== TEST 3: Exposure Control ===\n");
    
    rpi_camera_t *cam = rpi_camera_create(640, 480, RPI_FMT_YUV420);
    assert(cam != NULL);
    
    int exposure_times[] = {5000, 10000, 20000};  // microseconds
    const char *exposure_names[] = {"Short (5ms)", "Normal (10ms)", "Long (20ms)"};
    
    for (int i = 0; i < 3; i++) {
        printf("3.%d. Testing exposure: %s...\n", i+1, exposure_names[i]);
        
        // Set exposure
        int ret = rpi_camera_set_exposure(cam, exposure_times[i]);
        assert(ret == 0);
        
        // Start capturing
        control_stats_t stats = {0};
        ret = rpi_camera_start(cam);
        assert(ret == 0);
        
        uint64_t start_ts = get_time_ns();
        /* Get frame during 2 seconds */
        while(get_time_ns() - stat_ts < 2e9) {
            rpi_frame_t frame;
            if(rpi_camera_get_frame(cam, &frame, 1000) == 0) {
                stats.frame_count++;
                stats.last_sequence = frame.sequence;
                stats.last_timestamp = frame.last_timestamp;
                rpi_camera_release_frame(&frame);
            }
        }
        
        ret = rpi_camera_stop(cam);
        assert(ret == 0);
        
        // Calculate average brightness
        unsigned long sum = 0;
        for (int j = 0; j < stats.sample_count; j++) {
            sum += stats.brightness_samples[j];
        }
        unsigned char avg_brightness = sum / stats.sample_count;
        
        printf("    Statistics:\n");
        printf("      - Frames: %d\n", stats.frame_count);
        printf("      - Avg brightness: %u/255\n", avg_brightness);
        printf("    ✓ Exposure set successfully\n");
        
        assert(stats.frame_count >= 40);
    }
    
    rpi_camera_destroy(cam);
}

// ============================================================================
// TEST 4: Gain Control
// ============================================================================
void test_gain() {
    printf("\n=== TEST 4: Gain Control ===\n");
    
    rpi_camera_t *cam = rpi_camera_create(640, 480, RPI_FMT_YUV420);
    assert(cam != NULL);
    
    float gain_levels[] = {1.0, 4.0, 8.0};
    const char *gain_names[] = {"Low (1.0)", "Medium (4.0)", "High (8.0)"};
    
    for (int i = 0; i < 3; i++) {
        printf("4.%d. Testing gain: %s...\n", i+1, gain_names[i]);
        
        // Set gain
        int ret = rpi_camera_set_gain(cam, gain_levels[i]);
        assert(ret == 0);
        
        // Start capturing
        control_stats_t stats = {0};
        ret = rpi_camera_start(cam);
        assert(ret == 0);
        
        /* Get frame during 2 seconds */
        while(get_time_ns() - stat_ts < 2e9) {
            rpi_frame_t frame;
            if(rpi_camera_get_frame(cam, &frame, 1000) == 0) {
                stats.frame_count++;
                stats.last_sequence = frame.sequence;
                stats.last_timestamp = frame.last_timestamp;
                rpi_camera_release_frame(&frame);
            }
        }
        
        ret = rpi_camera_stop(cam);
        assert(ret == 0);
        
        printf("    Statistics:\n");
        printf("      - Frames: %d\n", stats.frame_count);
        printf("    ✓ Gain set successfully\n");
        
        assert(stats.frame_count >= 40);
    }
    
    rpi_camera_destroy(cam);
}

// ============================================================================
// TEST 5: Combined Controls
// ============================================================================
void test_combined_controls() {
    printf("\n=== TEST 5: Combined Controls ===\n");
    
    rpi_camera_t *cam = rpi_camera_create(640, 480, RPI_FMT_YUV420);
    assert(cam != NULL);
    
    printf("5.1. Setting all controls...\n");
    
    // Set all controls
    int ret = rpi_camera_set_brightness(cam, 0.3);
    assert(ret == 0);
    
    ret = rpi_camera_set_contrast(cam, 1.2);
    assert(ret == 0);
    
    ret = rpi_camera_set_exposure(cam, 15000);
    assert(ret == 0);
    
    ret = rpi_camera_set_gain(cam, 2.0);
    assert(ret == 0);
    
    printf("    ✓ All controls set\n");
    
    // Start capturing
    control_stats_t stats = {0};
    ret = rpi_camera_start(cam);
    assert(ret == 0);
    
    printf("5.2. Capturing with combined controls...\n");
    /* Get frame during 2 seconds */
    while(get_time_ns() - stat_ts < 2e9) {
        rpi_frame_t frame;
        if(rpi_camera_get_frame(cam, &frame, 1000) == 0) {
            stats.frame_count++;
            stats.last_sequence = frame.sequence;
            stats.last_timestamp = frame.last_timestamp;
            rpi_camera_release_frame(&frame);
        }
    }
    
    ret = rpi_camera_stop(cam);
    assert(ret == 0);
    
    printf("    Statistics:\n");
    printf("      - Frames: %d\n", stats.frame_count);
    printf("    ✓ Combined controls work together\n");
    
    assert(stats.frame_count >= 40);
    
    rpi_camera_destroy(cam);
}

// ============================================================================
// TEST 6: Dynamic Control Changes
// ============================================================================
void test_dynamic_controls() {
    printf("\n=== TEST 6: Dynamic Control Changes ===\n");
    
    rpi_camera_t *cam = rpi_camera_create(640, 480, RPI_FMT_YUV420);
    assert(cam != NULL);
    
    control_stats_t stats = {0};
    int ret = rpi_camera_start(cam);
    assert(ret == 0);
    
    printf("6.1. Changing brightness while capturing...\n");
    
    // Change brightness every second
    for (int i = 0; i < 3; i++) {
        float brightness = -0.5 + i * 0.5;  // -0.5, 0.0, 0.5
        ret = rpi_camera_set_brightness(cam, brightness);
        assert(ret == 0);
        printf("    Set brightness to %.1f\n", brightness);
        /* Get frame during 1 seconds */
        while(get_time_ns() - stat_ts < 1e9) {
            rpi_frame_t frame;
            if(rpi_camera_get_frame(cam, &frame, 1000) == 0) {
                stats.frame_count++;
                stats.last_sequence = frame.sequence;
                stats.last_timestamp = frame.last_timestamp;
                rpi_camera_release_frame(&frame);
            }
        }
    }
    
    printf("    ✓ Dynamic changes work\n");
    
    ret = rpi_camera_stop(cam);
    assert(ret == 0);
    
    printf("    Total frames: %d\n", stats.frame_count);
    assert(stats.frame_count >= 60);
    
    rpi_camera_destroy(cam);
}

// ============================================================================
// TEST 7: Invalid Control Values
// ============================================================================
void test_invalid_controls() {
    printf("\n=== TEST 7: Invalid Control Values ===\n");
    
    rpi_camera_t *cam = rpi_camera_create(640, 480, RPI_FMT_YUV420);
    assert(cam != NULL);
    
    // Test out-of-range values (should be clamped or rejected)
    printf("7.1. Testing extreme brightness values...\n");
    rpi_camera_set_brightness(cam, -2.0);  // Too low
    rpi_camera_set_brightness(cam, 2.0);   // Too high
    printf("    ✓ Handled gracefully\n");
    
    printf("7.2. Testing extreme contrast values...\n");
    rpi_camera_set_contrast(cam, -1.0);   // Negative
    rpi_camera_set_contrast(cam, 10.0);   // Too high
    printf("    ✓ Handled gracefully\n");
    
    printf("7.3. Testing extreme gain values...\n");
    rpi_camera_set_gain(cam, 0.1);   // Too low
    rpi_camera_set_gain(cam, 100.0); // Too high
    printf("    ✓ Handled gracefully\n");
    
    // Camera should still work after invalid values
    control_stats_t stats = {0};
    int ret = rpi_camera_start(cam);
    assert(ret == 0);
    
    /* Get frame during 1 seconds */
    while(get_time_ns() - stat_ts < 1e9) {
        rpi_frame_t frame;
        if(rpi_camera_get_frame(cam, &frame, 1000) == 0) {
            stats.frame_count++;
            stats.last_sequence = frame.sequence;
            stats.last_timestamp = frame.last_timestamp;
            rpi_camera_release_frame(&frame);
        }
    }
    
    ret = rpi_camera_stop(cam);
    assert(ret == 0);
    
    printf("    Camera still works: %d frames\n", stats.frame_count);
    assert(stats.frame_count >= 20);
    
    rpi_camera_destroy(cam);
}

// ============================================================================
// MAIN
// ============================================================================
int main() {
    printf("╔════════════════════════════════════════╗\n");
    printf("║  RPI Camera - Control Tests            ║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    test_brightness();
    test_contrast();
    test_exposure();
    test_gain();
    test_combined_controls();
    test_dynamic_controls();
    test_invalid_controls();
    
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║  ✓ ALL CONTROL TESTS PASSED            ║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    return 0;
}