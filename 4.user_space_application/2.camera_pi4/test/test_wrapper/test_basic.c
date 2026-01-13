// test_basic.c - Test các chức năng cơ bản
#include "rpi_camera.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>

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

void frame_callback_basic(rpi_frame_t *frame, void *userdata) {
    test_context_t *ctx = (test_context_t *)userdata;
    
    // Validate frame data
    assert(frame != NULL);
    assert(frame->data != NULL);
    assert(frame->size > 0);
    
    if (ctx->frame_count == 0) {
        ctx->first_timestamp = frame->timestamp;
        printf("    First frame received: seq=%u, size=%zu bytes, ts=%llu\n",
               frame->sequence, frame->size, frame->timestamp);
    }
    
    ctx->frame_count++;
    ctx->last_sequence = frame->sequence;
    ctx->last_timestamp = frame->timestamp;
    
    // Print every 10 frames
    if (ctx->frame_count % 10 == 0) {
        printf("    Frame %d: seq=%u, size=%zu bytes\n",
               ctx->frame_count, frame->sequence, frame->size);
    }
}

void test_start_stop() {
    printf("\n=== TEST 2: Start & Stop ===\n");
    
    // Test case 2.1: Start với callback hợp lệ
    printf("2.1. Starting camera with callback...\n");
    rpi_camera_t *cam = rpi_camera_create(640, 480, RPI_FMT_YUV420);
    assert(cam != NULL);
    
    test_context_t ctx = {0};
    int ret = rpi_camera_start(cam, frame_callback_basic, &ctx);
    
    // Expected: ret == 0
    assert(ret == 0);
    printf("    ✓ Camera started\n");
    
    // Capture trong 3 giây
    printf("    Capturing for 3 seconds...\n");
    sleep(3);
    
    // Test case 2.2: Stop camera
    printf("2.2. Stopping camera...\n");
    ret = rpi_camera_stop(cam);
    assert(ret == 0);
    printf("    ✓ Camera stopped\n");
    
    // Validate results
    printf("    Statistics:\n");
    printf("      - Total frames: %d\n", ctx.frame_count);
    printf("      - Last sequence: %u\n", ctx.last_sequence);
    printf("      - Duration: %.2f ms\n", 
           (ctx.last_timestamp - ctx.first_timestamp) / 1000000.0);
    
    // Expected: Ít nhất 60 frames trong 3s (20fps)
    assert(ctx.frame_count >= 60);
    printf("    ✓ Frame count OK (>= 60 frames)\n");
    
    // Expected: Sequence number tăng dần
    assert(ctx.last_sequence >= ctx.frame_count - 1);
    printf("    ✓ Sequence numbers OK\n");
    
    rpi_camera_destroy(cam);
}

// ============================================================================
// TEST 3: Start/Stop Multiple Times
// ============================================================================
void test_restart() {
    printf("\n=== TEST 3: Multiple Start/Stop ===\n");
    
    rpi_camera_t *cam = rpi_camera_create(640, 480, RPI_FMT_YUV420);
    assert(cam != NULL);
    
    for (int i = 0; i < 3; i++) {
        printf("3.%d. Start/Stop cycle %d...\n", i+1, i+1);
        
        test_context_t ctx = {0};
        int ret = rpi_camera_start(cam, frame_callback_basic, &ctx);
        assert(ret == 0);
        
        sleep(1);
        
        ret = rpi_camera_stop(cam);
        assert(ret == 0);
        
        printf("    ✓ Cycle %d: %d frames captured\n", i+1, ctx.frame_count);
        assert(ctx.frame_count >= 20);  // Ít nhất 20 frames trong 1s
    }
    
    rpi_camera_destroy(cam);
}

// ============================================================================
// TEST 4: Error Handling
// ============================================================================
void test_error_handling() {
    printf("\n=== TEST 4: Error Handling ===\n");
    
    // Test case 4.1: Start với NULL callback
    printf("4.1. Starting with NULL callback...\n");
    rpi_camera_t *cam = rpi_camera_create(640, 480, RPI_FMT_YUV420);
    assert(cam != NULL);
    
    int ret = rpi_camera_start(cam, NULL, NULL);
    // Expected: ret != 0 (error)
    assert(ret != 0);
    printf("    ✓ NULL callback rejected\n");
    
    // Test case 4.2: Stop camera chưa start
    printf("4.2. Stopping camera that wasn't started...\n");
    ret = rpi_camera_stop(cam);
    // Expected: Should handle gracefully (không crash)
    printf("    ✓ Handled gracefully\n");
    
    // Test case 4.3: Start camera NULL
    printf("4.3. Starting NULL camera...\n");
    ret = rpi_camera_start(NULL, frame_callback_basic, NULL);
    // Expected: ret != 0
    assert(ret != 0);
    printf("    ✓ NULL camera rejected\n");
    
    rpi_camera_destroy(cam);
}

// ============================================================================
// TEST 5: Frame Data Validation
// ============================================================================
void frame_callback_validation(rpi_frame_t *frame, void *userdata) {
    int *count = (int *)userdata;
    (*count)++;
    
    // Validate frame structure
    assert(frame != NULL);
    assert(frame->data != NULL);
    assert(frame->size > 0);
    
    // Validate YUV420 frame size
    // YUV420: width * height * 1.5 bytes
    size_t expected_size = 640 * 480 * 3 / 2;
    assert(frame->size >= expected_size * 0.9);  // Allow 10% tolerance
    assert(frame->size <= expected_size * 1.1);
    
    // Validate timestamp increases
    static uint64_t last_ts = 0;
    if (last_ts > 0) {
        assert(frame->timestamp > last_ts);
    }
    last_ts = frame->timestamp;
    
    // Validate sequence number increases
    static uint32_t last_seq = 0;
    if (last_seq > 0) {
        // Sequence có thể wrap around, nhưng không giảm quá nhiều
        assert(frame->sequence >= last_seq || 
               (last_seq - frame->sequence) > 1000000);
    }
    last_seq = frame->sequence;
    
    if (*count == 1) {
        printf("    First frame validation:\n");
        printf("      - Size: %zu bytes (expected ~%zu)\n", 
               frame->size, expected_size);
        printf("      - Timestamp: %llu\n", frame->timestamp);
        printf("      - Sequence: %u\n", frame->sequence);
    }
}

void test_frame_validation() {
    printf("\n=== TEST 5: Frame Data Validation ===\n");
    
    rpi_camera_t *cam = rpi_camera_create(640, 480, RPI_FMT_YUV420);
    assert(cam != NULL);
    
    int frame_count = 0;
    int ret = rpi_camera_start(cam, frame_callback_validation, &frame_count);
    assert(ret == 0);
    
    printf("5.1. Validating frames for 2 seconds...\n");
    sleep(2);
    
    ret = rpi_camera_stop(cam);
    assert(ret == 0);
    
    printf("    ✓ All %d frames validated successfully\n", frame_count);
    assert(frame_count >= 40);
    
    rpi_camera_destroy(cam);
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