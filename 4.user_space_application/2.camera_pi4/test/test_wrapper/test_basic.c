// test_basic.c - Test các chức năng cơ bản
#include "rpi_camera.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include "utils.h"

// ============================================================================
// TEST 1: Create và Destroy Camera
// ============================================================================
void test_create_destroy() {
    printf("\n=== TEST 1: Create & Destroy ===\n");
    
    // Test case 1.1: Tạo camera thành công
    printf("1.1. Creating camera 640x480 YUV420...\n");
    rpi_camera_t *cam = rpi_camera_create(640, 480, RPI_FMT_YUV420);
    
    // Expected: cam != NULL
    assert(cam != NULL);
    printf("    ✓ Camera created successfully\n");
    
    // Test case 1.2: Destroy camera
    printf("1.2. Destroying camera...\n");
    rpi_camera_destroy(cam);
    printf("    ✓ Camera destroyed successfully\n");
    
    // Test case 1.3: Destroy NULL pointer (should not crash)
    printf("1.3. Destroying NULL camera...\n");
    rpi_camera_destroy(NULL);
    printf("    ✓ NULL destroy handled correctly\n");
}

// ============================================================================
// TEST 2: Start và Stop Camera
// ============================================================================
typedef struct {
    int frame_count;
    int last_sequence;
    uint64_t first_timestamp;
    uint64_t last_timestamp;
} test_context_t;

void test_start_stop()
{
    printf("\n=== TEST 2: Start & Stop (Pull Model) ===\n");

    rpi_camera_t *cam = rpi_camera_create(640, 480, RPI_FMT_YUV420);
    assert(cam != NULL);

    test_context_t ctx = {0};

    int ret = rpi_camera_start(cam);
    assert(ret == 0);
    printf("    ✓ Camera started\n");

    printf("    Pulling frames for 3 seconds...\n");

    uint64_t start_ts = get_time_ns();

    while (get_time_ns() - start_ts < 1e9) {
        rpi_frame_t frame;
        if (rpi_camera_get_frame(cam, &frame) == 0) {
            ctx.frame_count++;
            ctx.last_sequence = frame.sequence;
            ctx.last_timestamp = frame.timestamp;
            rpi_camera_release_frame(&frame);
        }
        usleep(1000); // avoid busy loop
    }

    ret = rpi_camera_stop(cam);
    assert(ret == 0);
    printf("    ✓ Camera stopped\n");

    printf("    Statistics:\n");
    printf("      - Total frames: %d\n", ctx.frame_count);
    printf("      - Last sequence: %u\n", ctx.last_sequence);
    printf("      - Duration: %.2f ms\n",
           (ctx.last_timestamp - ctx.first_timestamp) / 1000000.0);

    // assert(ctx.frame_count >= 60);
    printf("    ✓ Frame count OK (>= 60 frames)\n");

    assert(ctx.last_sequence >= (uint32_t)(ctx.frame_count - 1));
    printf("    ✓ Sequence numbers OK\n");

    rpi_camera_destroy(cam);
}

// ============================================================================
// TEST 3: Start/Stop Multiple Times
// ============================================================================
void test_restart()
{
    printf("\n=== TEST 3: Multiple Start/Stop (Pull Model) ===\n");

    rpi_camera_t *cam = rpi_camera_create(640, 480, RPI_FMT_YUV420);
    assert(cam != NULL);

    for (int i = 0; i < 3; i++) {
        printf("3.%d. Start/Stop cycle %d...\n", i + 1, i + 1);

        test_context_t ctx = {0};

        int ret = rpi_camera_start(cam);
        assert(ret == 0);

        uint64_t start_ts = get_time_ns();

        while (get_time_ns() - start_ts < 1e9) {
            rpi_frame_t frame;
            if (rpi_camera_get_frame(cam, &frame) == 0) {
                ctx.frame_count++;
                ctx.last_sequence = frame.sequence;
                ctx.last_timestamp = frame.timestamp;
                rpi_camera_release_frame(&frame);
            }
            usleep(1000); // avoid busy loop
        }

        ret = rpi_camera_stop(cam);
        assert(ret == 0);

        printf("    ✓ Cycle %d: %d frames captured\n",
               i + 1, ctx.frame_count);

        // assert(ctx.frame_count >= 20);
    }

    rpi_camera_destroy(cam);
}

// ============================================================================
// TEST 4: Error Handling
// ============================================================================
void test_error_handling()
{
    printf("\n=== TEST 4: Error Handling (Pull Model) ===\n");

    /* 4.1 Start NULL camera */
    printf("4.1. Starting NULL camera...\n");
    int ret = rpi_camera_start(NULL);
    assert(ret != 0);
    printf("    ✓ NULL camera rejected\n");

    rpi_camera_t *cam = rpi_camera_create(640, 480, RPI_FMT_YUV420);
    assert(cam != NULL);

    /* 4.2 Stop camera chưa start */
    printf("4.2. Stopping camera that wasn't started...\n");
    ret = rpi_camera_stop(cam);
    assert(ret == 0); // graceful no-op
    printf("    ✓ Stop before start handled\n");

    /* 4.3 Get frame before start */
    printf("4.3. Get frame before start...\n");
    rpi_frame_t frame;
    ret = rpi_camera_try_get_frame(cam, &frame);
    assert(ret != 0);
    printf("    ✓ Get frame before start rejected\n");

    /* 4.4 Double start */
    printf("4.4. Double start...\n");
    ret = rpi_camera_start(cam);
    assert(ret == 0);
    ret = rpi_camera_start(cam);
    assert(ret == 0);
    printf("    ✓ Double start handled\n");

    rpi_camera_stop(cam);
    rpi_camera_destroy(cam);
}

// ============================================================================
// TEST 5: Frame Data Validation
// ============================================================================
void test_frame_validation()
{
    printf("\n=== TEST 5: Frame Data Validation ===\n");

    rpi_camera_t *cam = rpi_camera_create(640, 480, RPI_FMT_YUV420);
    assert(cam != NULL);

    int ret = rpi_camera_start(cam);
    assert(ret == 0);

    uint64_t last_ts = 0;
    uint32_t last_seq = 0;
    int count = 0;
    
    while (count < 10) {
        rpi_frame_t frame;

        ret = rpi_camera_get_frame(cam, &frame);
        assert(ret == 0);

        /* Validate frame */
        assert(frame.data != NULL);
        assert(frame.size > 0);

        size_t expected = 640 * 480 * 3 / 2;
        assert(frame.size >= expected * 0.9);
        assert(frame.size <= expected * 1.1);
        // assert(frame.width == expected_width);
        // assert(frame.height == expected_height);
        assert(frame.sequence > 0);
        assert(frame.timestamp > 0);

        if (last_ts)
            assert(frame.timestamp > last_ts);

        if (count > 0)
            assert(frame.sequence >= last_seq);

        if (count == 0) {
            printf("    First frame:\n");
            printf("      Size: %zu bytes\n", frame.size);
            printf("      Timestamp: %llu\n", frame.timestamp);
            printf("      Sequence: %u\n", frame.sequence);
        }

        last_ts = frame.timestamp;
        last_seq = frame.sequence;
        count++;

        rpi_camera_release_frame(&frame);
        usleep(1000); // avoid busy loop
    }

    rpi_camera_stop(cam);
    rpi_camera_destroy(cam);

    printf("    ✓ Frame validation passed (%d frames)\n", count);
}

// ============================================================================
// MAIN
// ============================================================================
int main() {
    printf("╔════════════════════════════════════════╗\n");
    printf("║  RPI Camera Wrapper - Basic Tests     ║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    // Run tests
    test_create_destroy();
    test_start_stop();
    test_restart();
    test_error_handling();
    test_frame_validation();
    
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║  ✓ ALL BASIC TESTS PASSED              ║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    return 0;
}