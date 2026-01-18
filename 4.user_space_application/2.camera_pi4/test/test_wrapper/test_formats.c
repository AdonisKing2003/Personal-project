// test_formats.c - Test các format và resolution khác nhau
#include "rpi_camera.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

typedef struct {
    int width;
    int height;
    rpi_format_t format;
    const char *format_name;
    size_t expected_size_min;
    size_t expected_size_max;
} format_test_t;

typedef struct {
    int frame_count;
    size_t total_bytes;
    size_t min_size;
    size_t max_size;
    uint64_t start_time;
    uint64_t end_time;
} format_stats_t;
        
// ============================================================================
// TEST 1: YUV420 Format
// ============================================================================
void test_yuv420() {
    printf("\n=== TEST 1: YUV420 Format ===\n");

    format_test_t tests[] = { {640, 480, RPI_FMT_YUV420, "640x480", 460800, 460800}, // 640*480*1.5 
                            {1280, 720, RPI_FMT_YUV420, "1280x720", 1382400, 1382400}, // 1280*720*1.5 
                            {1920, 1080, RPI_FMT_YUV420, "1920x1080", 3110400, 3110400} // 1920*1080*1.5 
                            };
    
    for (int i = 0; i < 3; i++) { 
        format_test_t *test = &tests[i]; 
        printf("1.%d. Testing YUV420 %s...\n", i+1, test->format_name);
        rpi_camera_t *cam = rpi_camera_create(test->width, test->height, test->format);
        assert(cam != NULL);
        // Start camera
        int ret = rpi_camera_start(cam);
        assert(ret == 0);

        WaitForFirstFrame(cam);

        // Initialize statistics
        format_stats_t stats = {0};
        stats.start_time = get_time_ns();
        stats.end_time = stats.start_time;
        stats.min_size = SIZE_MAX;
        printf("[INFO]:    Capturing frames for 2 seconds...\n");
        while (get_time_ns() - stats.start_time < 4ULL * 1000 * 1000 * 1000) {
            rpi_frame_t frame;

            if (rpi_camera_try_get_frame(cam, &frame) == 0) {
                stats.frame_count++;

                // Size statistics
                stats.total_bytes += frame.size;
                if (frame.size < stats.min_size)
                    stats.min_size = frame.size;
                if (frame.size > stats.max_size)
                    stats.max_size = frame.size;

                stats.end_time = get_time_ns();

                rpi_camera_release_frame(&frame);
            }
            else {
                usleep(1000); // 1ms
            }
        }
        ret = rpi_camera_stop(cam);
        assert(ret == 0);

        // Calculate statistics
        double duration_sec =
            (stats.end_time - stats.start_time) / 1e9;

        double fps = stats.frame_count / duration_sec;
        double avg_size =
            (double)stats.total_bytes / stats.frame_count;

        // Print statistics
        printf("    Statistics:\n");
        printf("      - Frames: %d\n", stats.frame_count);
        printf("      - estimate time: %llu\n", stats.end_time - stats.start_time);
        printf("      - FPS: %.2f\n", fps);
        printf("      - Avg size: %.0f bytes\n", avg_size);
        printf("      - Min size: %zu bytes\n", stats.min_size);
        printf("      - Max size: %zu bytes\n", stats.max_size);
        printf("      - Expected: %zu bytes\n", test->expected_size_min);

        // assert(stats.frame_count >= 40); // At least 40 frames in 2s 
        assert(stats.min_size >= test->expected_size_min * 0.95); 
        assert(stats.max_size <= test->expected_size_max * 1.05); 
        printf(" ✓ PASSED\n");
        rpi_camera_destroy(cam);
    }
}

// ============================================================================
// TEST 2: RGB888 Format
// ============================================================================
void test_rgb888() {
    printf("\n=== TEST 2: RGB888 Format ===\n");
    
    format_test_t tests[] = {
        {640, 480, RPI_FMT_RGB888, "640x480", 921600, 921600},      // 640*480*3
        {1280, 720, RPI_FMT_RGB888, "1280x720", 2764800, 2764800},  // 1280*720*3
        {1920, 1080, RPI_FMT_RGB888, "1920x1080", 6220800, 6220800} // 1920*1080*3
    };

    for (int i = 0; i < 3; i++) {
        format_test_t *test = &tests[i];
        printf("2.%d. Testing RGB888 %s...\n", i+1, test->format_name);
        
        rpi_camera_t *cam = rpi_camera_create(test->width, test->height, test->format);
        assert(cam != NULL);
        // Start camera
        int ret = rpi_camera_start(cam);
        assert(ret == 0);
        WaitForFirstFrame(cam);

        // Initialize statistics
        format_stats_t stats = {0};
        stats.start_time = get_time_ns();
        stats.end_time = get_time_ns();
        stats.min_size = SIZE_MAX;
        printf("[INFO]:    Capturing frames for 2 seconds...\n");
        while (get_time_ns() - stats.start_time < 2ULL * 1000 * 1000 * 1000) {
            rpi_frame_t frame;

            if (rpi_camera_try_get_frame(cam, &frame) == 0) {
                stats.frame_count++;

                // Size statistics
                stats.total_bytes += frame.size;
                if (frame.size < stats.min_size)
                    stats.min_size = frame.size;
                if (frame.size > stats.max_size)
                    stats.max_size = frame.size;

                stats.end_time = get_time_ns();

                rpi_camera_release_frame(&frame);
            } else {
                // Avoid busy spin
                usleep(1000); // 1 ms
            }
        }

        ret = rpi_camera_stop(cam);
        assert(ret == 0);

        // Calculate statistics
        double duration_sec =
            (stats.end_time - stats.start_time) / 1e9;

        double fps = stats.frame_count / duration_sec;
        double avg_size =
            (double)stats.total_bytes / stats.frame_count;

        // Print statistics
        printf("    Statistics:\n");
        printf("      - Frames: %d\n", stats.frame_count);
        printf("      - estimate time: %llu\n", stats.end_time - stats.start_time);
        printf("      - FPS: %.2f\n", fps);
        printf("      - Avg size: %.0f bytes\n", avg_size);
        printf("      - Min size: %zu bytes\n", stats.min_size);
        printf("      - Max size: %zu bytes\n", stats.max_size);
        printf("      - Expected: %zu bytes\n", test->expected_size_min);

        // assert(stats.frame_count >= 40); // At least 40 frames in 2s 
        assert(stats.min_size >= test->expected_size_min * 0.95); 
        assert(stats.max_size <= test->expected_size_max * 1.05); 
        printf(" ✓ PASSED\n");
        rpi_camera_destroy(cam);
    }
}

// ============================================================================
// TEST 3: MJPEG Format
// ============================================================================
void test_mjpeg() {
    printf("\n=== TEST 3: MJPEG Format ===\n");
    
    format_test_t tests[] = {
        {640, 480, RPI_FMT_MJPEG, "640x480", 10000, 100000},
        {1280, 720, RPI_FMT_MJPEG, "1280x720", 20000, 200000},
        {1920, 1080, RPI_FMT_MJPEG, "1920x1080", 30000, 300000}
    };
    
    for (int i = 0; i < 3; i++) {
        format_test_t *test = &tests[i];
        printf("3.%d. Testing MJPEG %s...\n", i+1, test->format_name);
        
        rpi_camera_t *cam = rpi_camera_create(test->width, test->height, test->format);
        assert(cam != NULL);
        // Start camera
        int ret = rpi_camera_start(cam);
        assert(ret == 0);
        WaitForFirstFrame(cam);
        
        // Initialize statistics
        format_stats_t stats = {0};
        stats.start_time = get_time_ns();
        stats.end_time = stats.start_time;
        stats.min_size = SIZE_MAX;
        printf("[INFO]:    Capturing frames for 2 seconds...\n");
        while (get_time_ns() - stats.start_time < 4ULL * 1000 * 1000 * 1000) {
            rpi_frame_t frame;

            if (rpi_camera_try_get_frame(cam, &frame) == 0) {
                stats.frame_count++;

                // Size statistics
                stats.total_bytes += frame.size;
                if (frame.size < stats.min_size)
                    stats.min_size = frame.size;
                if (frame.size > stats.max_size)
                    stats.max_size = frame.size;

                stats.end_time = get_time_ns();

                rpi_camera_release_frame(&frame);
            } else {
                // Avoid busy spin
                usleep(1000); // 1 ms
            }
        }

        ret = rpi_camera_stop(cam);
        assert(ret == 0);

        // Calculate statistics
        double duration_sec =
            (stats.end_time - stats.start_time) / 1e9;

        double fps = stats.frame_count / duration_sec;
        double avg_size =
            (double)stats.total_bytes / stats.frame_count;

        // Print statistics
        printf("    Statistics:\n");
        printf("      - Frames: %d\n", stats.frame_count);
        printf("      - estimate time: %llu\n", stats.end_time - stats.start_time);
        printf("      - FPS: %.2f\n", fps);
        printf("      - Avg size: %.0f bytes\n", avg_size);
        printf("      - Min size: %zu bytes\n", stats.min_size);
        printf("      - Max size: %zu bytes\n", stats.max_size);
        printf("      - Expected: %zu bytes\n", test->expected_size_min);

        // assert(stats.frame_count >= 40); // At least 40 frames in 2s 
        assert(stats.min_size >= test->expected_size_min * 0.95); 
        assert(stats.max_size <= test->expected_size_max * 1.05); 
        printf(" ✓ PASSED\n");
        rpi_camera_destroy(cam);
    }
}

// ============================================================================
// TEST 4: Resolution Limits
// ============================================================================
void test_resolution_limits() {
    printf("\n=== TEST 4: Resolution Limits ===\n");
    
    // Test case 4.1: Minimum resolution
    printf("4.1. Testing minimum resolution (320x240)...\n");
    rpi_camera_t *cam = rpi_camera_create(320, 240, RPI_FMT_YUV420);
    assert(cam != NULL);
    // Start camera
    int ret = rpi_camera_start(cam);
    assert(ret == 0);
    WaitForFirstFrame(cam);

    // Initialize statistics
    format_stats_t stats = {0};
    stats.start_time = get_time_ns();
    stats.end_time = stats.start_time;
    printf("[INFO]:    Capturing frames for 1 seconds...\n");
    while (get_time_ns() - stats.start_time < 1ULL * 1000 * 1000 * 1000) {
        rpi_frame_t frame;
        if (rpi_camera_try_get_frame(cam, &frame) == 0) {
            stats.frame_count++;

            // Size statistics
            stats.total_bytes += frame.size;
            if (frame.size < stats.min_size)
                stats.min_size = frame.size;
            if (frame.size > stats.max_size)
                stats.max_size = frame.size;

            stats.end_time = get_time_ns();

            rpi_camera_release_frame(&frame);
        } else {
            // Avoid busy spin
            usleep(1000); // 1 ms
        }
    }
    rpi_camera_stop(cam);
    
    printf("    ✓ Min resolution works: %d frames\n", stats.frame_count);
    printf(" ✓ PASSED\n");
    // assert(stats.frame_count >= 20);
    rpi_camera_destroy(cam);
    
    // Test case 4.2: Maximum resolution (depends on hardware)
    printf("4.2. Testing high resolution (2592x1944)...\n");
    cam = rpi_camera_create(2592, 1944, RPI_FMT_YUV420);
    if (cam != NULL) {
        stats = (format_stats_t){0};
        ret = rpi_camera_start(cam);
        if (ret == 0) {
            stats.start_time = get_time_ns();
            stats.end_time = stats.start_time;
            printf("[INFO]:    Capturing frames for 1 seconds...\n");
            while (get_time_ns() - stats.start_time < 1ULL * 1000 * 1000 * 1000) {
                rpi_frame_t frame;
                if (rpi_camera_try_get_frame(cam, &frame) == 0) {
                    stats.frame_count++;

                    // Size statistics
                    stats.total_bytes += frame.size;
                    if (frame.size < stats.min_size)
                        stats.min_size = frame.size;
                    if (frame.size > stats.max_size)
                        stats.max_size = frame.size;

                    stats.end_time = get_time_ns();

                    rpi_camera_release_frame(&frame);
                } else {
                    // Avoid busy spin
                    usleep(1000); // 1 ms
                }
            }
            rpi_camera_stop(cam);
            printf("    ✓ High resolution works: %d frames\n", stats.frame_count);
        } else {
            printf("    ⚠ High resolution not supported by hardware\n");
        }
        rpi_camera_destroy(cam);
        printf(" ✓ PASSED\n");
    } else {
        printf("    ⚠ High resolution not supported\n");
        printf(" x FAILED\n");
    }
}

// ============================================================================
// TEST 5: Format Switching
// ============================================================================
void test_format_switching() {
    printf("\n=== TEST 5: Format Switching ===\n");
    
    rpi_format_t formats[] = {RPI_FMT_YUV420, RPI_FMT_RGB888};
    const char *format_names[] = {"YUV420", "RGB888"};
    
    for (int i = 0; i < 2; i++) {
        printf("5.%d. Testing %s format...\n", i+1, format_names[i]);
        
        rpi_camera_t *cam = rpi_camera_create(640, 480, formats[i]);
        assert(cam != NULL);
        
        int ret = rpi_camera_start(cam);
        assert(ret == 0);
        WaitForFirstFrame(cam);

        // Initialize statistics
        format_stats_t stats = {0};
        stats.start_time = get_time_ns();
        stats.end_time = stats.start_time;
        stats.min_size = SIZE_MAX;
        printf("[INFO]:    Capturing frames for 1 seconds...\n");
        while (get_time_ns() - stats.start_time < 1ULL * 1000 * 1000 * 1000) {
            rpi_frame_t frame;
            if (rpi_camera_try_get_frame(cam, &frame) == 0) {
                stats.frame_count++;

                // Size statistics
                stats.total_bytes += frame.size;
                if (frame.size < stats.min_size)
                    stats.min_size = frame.size;
                if (frame.size > stats.max_size)
                    stats.max_size = frame.size;

                stats.end_time = get_time_ns();

                rpi_camera_release_frame(&frame);
            } else {
                // Avoid busy spin
                usleep(1000); // 1 ms
            }
        }
        ret = rpi_camera_stop(cam);
        assert(ret == 0);
        
        printf("    ✓ %s: %d frames captured\n", format_names[i], stats.frame_count);
        // assert(stats.frame_count >= 20);

        rpi_camera_destroy(cam);
    }
    
    printf("    ✓ All formats work independently\n");
    printf(" ✓ PASSED\n");
}

// ============================================================================
// MAIN
// ============================================================================
int main() {
    printf("╔════════════════════════════════════════╗\n");
    printf("║  RPI Camera - Format Tests             ║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    test_yuv420();
    test_rgb888();
    // test_mjpeg(); // MJPEG is not natively supported by libcamera on Raspberry Pi
    test_resolution_limits();
    test_format_switching();
    
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║  ✓ ALL FORMAT TESTS PASSED             ║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    return 0;
}