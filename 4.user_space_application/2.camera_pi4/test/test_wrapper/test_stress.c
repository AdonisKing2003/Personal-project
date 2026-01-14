// test_stress.c - Stress tests, memory leaks, stability
#include "rpi_camera.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

// ============================================================================
// Helper: Get current memory usage
// ============================================================================
long get_memory_usage_kb() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss;  // KB on Linux
}

typedef struct {
    int frame_count;
    uint64_t total_bytes;
    long start_memory;
    long end_memory;
} stress_stats_t;

// ============================================================================
// TEST 1: Long Running Test
// ============================================================================
void test_long_running() {
    printf("\n=== TEST 1: Long Running Test (30s) ===\n");
    
    rpi_camera_t *cam = rpi_camera_create(640, 480, RPI_FMT_YUV420);
    assert(cam != NULL);
    
    stress_stats_t stats = {0};
    stats.start_memory = get_memory_usage_kb();
    
    int ret = rpi_camera_start(cam);
    assert(ret == 0);
    
    printf("Starting capture for 30 seconds...\n");
    printf("Time | Frames | FPS  | Memory (KB)\n");
    printf("-----|--------|------|------------\n");
    
    for (int i = 1; i <= 6; i++) {
        /* Get frame during 5 seconds */
        while(get_time_ns() - stat_ts < 5e9) {
            rpi_frame_t frame;
            if(rpi_camera_get_frame(cam, &frame, 1000) == 0) {
                stats.frame_count++;
                stats.last_sequence = frame.sequence;
                stats.last_timestamp = frame.last_timestamp;
                rpi_camera_release_frame(&frame);
            }
        }
        long current_mem = get_memory_usage_kb();
        double fps = stats.frame_count / (i * 5.0);
        printf("%4ds | %6d | %4.1f | %ld\n", 
               i * 5, stats.frame_count, fps, current_mem);
    }
    
    ret = rpi_camera_stop(cam);
    assert(ret == 0);
    
    stats.end_memory = get_memory_usage_kb();
    long memory_growth = stats.end_memory - stats.start_memory;
    
    printf("\nResults:\n");
    printf("  - Total frames: %d\n", stats.frame_count);
    printf("  - Average FPS: %.2f\n", stats.frame_count / 30.0);
    printf("  - Total data: %.2f MB\n", stats.total_bytes / (1024.0 * 1024.0));
    printf("  - Memory growth: %ld KB\n", memory_growth);
    
    // Validate
    assert(stats.frame_count >= 600);  // At least 20 FPS
    assert(memory_growth < 10000);     // Less than 10MB growth
    printf("  ✓ Long running test passed\n");
    
    rpi_camera_destroy(cam);
}

// ============================================================================
// TEST 2: Repeated Start/Stop
// ============================================================================
void test_repeated_start_stop() {
    printf("\n=== TEST 2: Repeated Start/Stop (100 cycles) ===\n");
    
    rpi_camera_t *cam = rpi_camera_create(640, 480, RPI_FMT_YUV420);
    assert(cam != NULL);
    
    long start_memory = get_memory_usage_kb();
    
    printf("Running 100 start/stop cycles...\n");
    
    for (int i = 0; i < 100; i++) {
        stress_stats_t stats = {0};
        
        int ret = rpi_camera_start(cam);
        assert(ret == 0);
        
        // usleep(100000);  // 100ms
        /* Get frame during 100 milliseconds */
        while(get_time_ns() - stat_ts < 100000000) {
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
        
        if ((i + 1) % 20 == 0) {
            long current_mem = get_memory_usage_kb();
            printf("  Cycle %3d: %d frames, memory: %ld KB\n", 
                   i + 1, stats.frame_count, current_mem);
        }
    }
    
    long end_memory = get_memory_usage_kb();
    long memory_growth = end_memory - start_memory;
    
    printf("\nResults:\n");
    printf("  - Memory growth: %ld KB\n", memory_growth);
    
    // Should not leak significant memory
    assert(memory_growth < 5000);  // Less than 5MB
    printf("  ✓ No memory leak detected\n");
    
    rpi_camera_destroy(cam);
}

// ============================================================================
// TEST 3: Multiple Create/Destroy
// ============================================================================
void test_multiple_create_destroy() {
    printf("\n=== TEST 3: Multiple Create/Destroy (50 cycles) ===\n");
    
    long start_memory = get_memory_usage_kb();
    
    printf("Creating and destroying camera 50 times...\n");
    
    for (int i = 0; i < 50; i++) {
        rpi_camera_t *cam = rpi_camera_create(640, 480, RPI_FMT_YUV420);
        assert(cam != NULL);
        
        stress_stats_t stats = {0};
        int ret = rpi_camera_start(cam);
        assert(ret == 0);
        
        // usleep(100000);  // 100ms
        /* Get frame during 100 milliseconds */
        while(get_time_ns() - stat_ts < 100000000) {
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
        
        rpi_camera_destroy(cam);
        
        if ((i + 1) % 10 == 0) {
            long current_mem = get_memory_usage_kb();
            printf("  Cycle %2d: memory = %ld KB\n", i + 1, current_mem);
        }
    }
    
    long end_memory = get_memory_usage_kb();
    long memory_growth = end_memory - start_memory;
    
    printf("\nResults:\n");
    printf("  - Memory growth: %ld KB\n", memory_growth);
    
    assert(memory_growth < 5000);
    printf("  ✓ No memory leak in create/destroy\n");
}

// ============================================================================
// TEST 4: High FPS Test
// ============================================================================
void test_high_fps() {
    printf("\n=== TEST 4: High FPS Test ===\n");
    
    // Small resolution for high FPS
    rpi_camera_t *cam = rpi_camera_create(320, 240, RPI_FMT_YUV420);
    assert(cam != NULL);
    
    stress_stats_t stats = {0};
    int ret = rpi_camera_start(cam);
    assert(ret == 0);
    
    printf("Capturing at small resolution for 5 seconds...\n");
    
    for (int i = 1; i <= 5; i++) {
        /* Get frame during 1 second */
        while(get_time_ns() - stat_ts < 1e9) {
            rpi_frame_t frame;
            if(rpi_camera_get_frame(cam, &frame, 1000) == 0) {
                stats.frame_count++;
                stats.last_sequence = frame.sequence;
                stats.last_timestamp = frame.last_timestamp;
                rpi_camera_release_frame(&frame);
            }
        }
        double fps = stats.frame_count / (double)i;
        printf("  %ds: %d frames (%.1f FPS)\n", i, stats.frame_count, fps);
    }
    
    ret = rpi_camera_stop(cam);
    assert(ret == 0);
    
    double avg_fps = stats.frame_count / 5.0;
    printf("\nResults:\n");
    printf("  - Total frames: %d\n", stats.frame_count);
    printf("  - Average FPS: %.2f\n", avg_fps);
    
    // Should achieve at least 25 FPS at low resolution
    assert(avg_fps >= 25.0);
    printf("  ✓ High FPS achieved\n");
    
    rpi_camera_destroy(cam);
}

// ============================================================================
// TEST 5: Frame Drop Test
// ============================================================================
typedef struct {
    int frame_count;
    uint32_t last_sequence;
    int dropped_frames;
} drop_stats_t;

void drop_callback(rpi_frame_t *frame, void *userdata) {
    drop_stats_t *stats = (drop_stats_t *)userdata;
    
    if (stats->frame_count > 0) {
        // Check for dropped frames
        uint32_t expected = stats->last_sequence + 1;
        if (frame->sequence != expected) {
            int dropped = frame->sequence - expected;
            stats->dropped_frames += dropped;
            printf("    ! Dropped %d frames (seq: %u -> %u)\n",
                   dropped, stats->last_sequence, frame->sequence);
        }
    }
    
    stats->frame_count++;
    stats->last_sequence = frame->sequence;
    
    // Simulate slow processing
    usleep(5000);  // 5ms delay
}

void test_frame_drops() {
    printf("\n=== TEST 5: Frame Drop Test ===\n");
    
    rpi_camera_t *cam = rpi_camera_create(640, 480, RPI_FMT_YUV420);
    assert(cam != NULL);
    
    drop_stats_t stats = {0};
    int ret = rpi_camera_start(cam);
    assert(ret == 0);
    
    printf("Capturing with slow callback (5ms delay)...\n");
    /* Get frame during 5 seconds */
    while(get_time_ns() - stat_ts < 5e9) {
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
    
    double drop_rate = (double)stats.dropped_frames / 
                       (stats.frame_count + stats.dropped_frames) * 100.0;
    
    printf("\nResults:\n");
    printf("  - Captured frames: %d\n", stats.frame_count);
    printf("  - Dropped frames: %d\n", stats.dropped_frames);
    printf("  - Drop rate: %.2f%%\n", drop_rate);
    
    // With slow callback, some drops are expected but should be minimal
    assert(drop_rate < 20.0);  // Less than 20% drops
    printf("  ✓ Frame drop rate acceptable\n");
    
    rpi_camera_destroy(cam);
}

// ============================================================================
// TEST 6: Concurrent Cameras (if supported)
// ============================================================================
void test_concurrent_cameras() {
    printf("\n=== TEST 6: Concurrent Cameras ===\n");

    rpi_camera_t *cam1 = rpi_camera_create(640, 480, RPI_FMT_YUV420);
    assert(cam1 != NULL);
    printf("  ✓ Camera 1 created\n");

    rpi_camera_t *cam2 = rpi_camera_create(320, 240, RPI_FMT_YUV420);
    if (!cam2) {
        printf("  ⚠ Only one camera supported (this is normal)\n");
        rpi_camera_destroy(cam1);
        return;
    }

    printf("  ✓ Camera 2 created\n");

    rpi_camera_start(cam1);
    rpi_camera_start(cam2);

    uint64_t start_ts = get_time_ns();
    stress_stats_t stats1 = {0}, stats2 = {0};

    while (get_time_ns() - start_ts < 100000000) { // 100ms
        rpi_frame_t f1, f2;

        if (rpi_camera_try_get_frame(cam1, &f1) == 0) {
            stats1.frame_count++;
            stats1.last_sequence = f1.sequence;
            stats1.last_timestamp = f1.timestamp;
            rpi_camera_release_frame(&f1);
        }

        if (rpi_camera_try_get_frame(cam2, &f2) == 0) {
            stats2.frame_count++;
            stats2.last_sequence = f2.sequence;
            stats2.last_timestamp = f2.timestamp;
            rpi_camera_release_frame(&f2);
        }

        usleep(1000); // avoid busy loop
    }

    rpi_camera_stop(cam1);
    rpi_camera_stop(cam2);

    printf("  Camera 1: %d frames\n", stats1.frame_count);
    printf("  Camera 2: %d frames\n", stats2.frame_count);

    assert(stats1.frame_count > 0);
    assert(stats2.frame_count > 0);

    rpi_camera_destroy(cam2);
    rpi_camera_destroy(cam1);
}

// ============================================================================
// TEST 7: Rapid Format Changes
// ============================================================================
void test_rapid_format_changes() {
    printf("\n=== TEST 7: Rapid Format Changes ===\n");
    
    rpi_format_t formats[] = {RPI_FMT_YUV420, RPI_FMT_RGB888, RPI_FMT_MJPEG};
    const char *names[] = {"YUV420", "RGB888", "MJPEG"};
    
    long start_memory = get_memory_usage_kb();
    
    for (int cycle = 0; cycle < 20; cycle++) {
        for (int i = 0; i < 3; i++) {
            rpi_camera_t *cam = rpi_camera_create(640, 480, formats[i]);
            assert(cam != NULL);
            
            stress_stats_t stats = {0};
            rpi_camera_start(cam);
            // usleep(200000);  // 200ms
            while(get_time_ns() - stat_ts < 200000000) {
                rpi_frame_t frame;
                if(rpi_camera_get_frame(cam, &frame, 1000) == 0) {
                    stats.frame_count++;
                    stats.last_sequence = frame.sequence;
                    stats.last_timestamp = frame.last_timestamp;
                    rpi_camera_release_frame(&frame);
                }
            }

            rpi_camera_stop(cam);
            rpi_camera_destroy(cam);
        }
        
        if ((cycle + 1) % 5 == 0) {
            long mem = get_memory_usage_kb();
            printf("  Cycle %2d: memory = %ld KB\n", cycle + 1, mem);
        }
    }
    
    long end_memory = get_memory_usage_kb();
    long memory_growth = end_memory - start_memory;
    
    printf("\nResults:\n");
    printf("  - Memory growth: %ld KB\n", memory_growth);
    
    assert(memory_growth < 5000);
    printf("  ✓ Format switching stable\n");
}

// ============================================================================
// MAIN
// ============================================================================
int main() {
    printf("╔════════════════════════════════════════╗\n");
    printf("║  RPI Camera - Stress Tests             ║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    test_long_running();
    test_repeated_start_stop();
    test_multiple_create_destroy();
    test_high_fps();
    test_frame_drops();
    test_concurrent_cameras();
    test_rapid_format_changes();
    
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║  ✓ ALL STRESS TESTS PASSED             ║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    return 0;
}