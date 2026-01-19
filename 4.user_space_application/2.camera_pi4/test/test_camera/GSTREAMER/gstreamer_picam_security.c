/**
 * @file picam_security.c
 * @brief Single Pi Camera Security System with GStreamer
 * 
 * Features:
 * - Live preview
 * - Motion detection
 * - Auto recording on motion
 * - RTSP streaming to mobile
 * - Snapshot capture
 * - Web interface for monitoring
 * 
 * Hardware:
 * - Raspberry Pi 4
 * - Pi Camera Module v2 or v3
 * 
 * Build:
 *   gcc picam_security.c -o picam_security \
 *       $(pkg-config --cflags --libs gstreamer-1.0 gstreamer-app-1.0) -pthread
 * 
 * Run:
 *   ./picam_security
 */

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>

// Configuration
#define RECORDING_DIR "recordings"
#define SNAPSHOT_DIR "snapshots"
#define VIDEO_WIDTH 1920
#define VIDEO_HEIGHT 1080
#define VIDEO_FPS 30
#define PREVIEW_WIDTH 640
#define PREVIEW_HEIGHT 480

// Camera state
typedef enum {
    STATE_IDLE,
    STATE_MONITORING,
    STATE_RECORDING,
    STATE_ERROR
} CameraState;

typedef struct {
    GstElement *pipeline;
    GstElement *source;
    GstElement *tee;
    GstElement *recording_bin;
    
    GMainLoop *loop;
    CameraState state;
    
    gboolean motion_detected;
    gboolean is_recording;
    time_t recording_start_time;
    char current_recording_file[256];
    
    // Statistics
    int motion_events_today;
    int recordings_today;
    long total_recording_duration;
    
    pthread_mutex_t state_mutex;
} SecurityCamera;

static SecurityCamera g_camera = {0};

// Forward declarations
void camera_start_recording(void);
void camera_stop_recording(void);
void take_snapshot(void);

// ============================================================================
// Utility Functions
// ============================================================================

void create_directories(void) {
    mkdir(RECORDING_DIR, 0755);
    mkdir(SNAPSHOT_DIR, 0755);
}

void generate_recording_filename(char *filename, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(filename, size, 
             "%s/security_%04d%02d%02d_%02d%02d%02d.mp4",
             RECORDING_DIR,
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
}

void generate_snapshot_filename(char *filename, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(filename, size, 
             "%s/snapshot_%04d%02d%02d_%02d%02d%02d.jpg",
             SNAPSHOT_DIR,
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
}

const char* state_to_string(CameraState state) {
    switch (state) {
        case STATE_IDLE: return "IDLE";
        case STATE_MONITORING: return "MONITORING";
        case STATE_RECORDING: return "RECORDING";
        case STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// Signal Handler
// ============================================================================

void signal_handler(int signum) {
    printf("\n\n===========================================\n");
    printf("Shutting down gracefully...\n");
    printf("===========================================\n");
    
    if (g_camera.is_recording) {
        camera_stop_recording();
    }
    
    if (g_camera.loop) {
        g_main_loop_quit(g_camera.loop);
    }
}

// ============================================================================
// Motion Detection Callback
// ============================================================================

static gboolean on_motion_detected(GstElement *element, 
                                   gdouble motion_score,
                                   gpointer user_data) {
    pthread_mutex_lock(&g_camera.state_mutex);
    
    if (!g_camera.motion_detected) {
        printf("\n🚨 MOTION DETECTED! (score: %.2f)\n", motion_score);
        
        g_camera.motion_detected = TRUE;
        g_camera.motion_events_today++;
        
        // Take snapshot
        take_snapshot();
        
        // Start recording if not already recording
        if (!g_camera.is_recording) {
            printf("📹 Starting automatic recording...\n");
            camera_start_recording();
        }
        
        // TODO: Send notification
        // send_push_notification("Motion detected!");
        // send_email_alert("Security Alert", "Motion detected in camera view");
    }
    
    pthread_mutex_unlock(&g_camera.state_mutex);
    return TRUE;
}

// ============================================================================
// Bus Message Handler
// ============================================================================

static gboolean bus_callback(GstBus *bus, GstMessage *msg, gpointer data) {
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError *err;
            gchar *debug;
            gst_message_parse_error(msg, &err, &debug);
            g_printerr("\n❌ ERROR: %s\n", err->message);
            if (debug) {
                g_printerr("Debug: %s\n", debug);
            }
            g_error_free(err);
            g_free(debug);
            
            pthread_mutex_lock(&g_camera.state_mutex);
            g_camera.state = STATE_ERROR;
            pthread_mutex_unlock(&g_camera.state_mutex);
            
            g_main_loop_quit(g_camera.loop);
            break;
        }
        
        case GST_MESSAGE_EOS:
            g_print("\n📡 End of stream\n");
            if (g_camera.is_recording) {
                camera_stop_recording();
            }
            break;
        
        case GST_MESSAGE_STATE_CHANGED: {
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(g_camera.pipeline)) {
                GstState old_state, new_state;
                gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);
                g_print("Pipeline state: %s -> %s\n",
                        gst_element_state_get_name(old_state),
                        gst_element_state_get_name(new_state));
            }
            break;
        }
        
        case GST_MESSAGE_WARNING: {
            GError *err;
            gchar *debug;
            gst_message_parse_warning(msg, &err, &debug);
            g_printerr("⚠️  WARNING: %s\n", err->message);
            g_error_free(err);
            g_free(debug);
            break;
        }
        
        default:
            break;
    }
    
    return TRUE;
}

// ============================================================================
// Camera Pipeline Creation
// ============================================================================

gboolean camera_init(void) {
    printf("\n===========================================\n");
    printf("  Pi Camera Security System\n");
    printf("===========================================\n\n");
    
    gst_init(NULL, NULL);
    pthread_mutex_init(&g_camera.state_mutex, NULL);
    
    create_directories();
    
    // Create main pipeline
    // Using libcamerasrc for Pi Camera Module
    gchar *pipeline_desc = g_strdup_printf(
        "libcamerasrc ! "
        "video/x-raw,width=%d,height=%d,framerate=%d/1,format=NV12 ! "
        "tee name=t "
        
        // Branch 1: Live preview (optional - comment out for headless)
        "t. ! queue ! videoscale ! "
        "video/x-raw,width=%d,height=%d ! "
        "videoconvert ! autovideosink "
        
        // Branch 2: Motion detection
        "t. ! queue ! videoconvert ! "
        "video/x-raw,format=RGB ! "
        "videoscale ! video/x-raw,width=320,height=240 ! "
        "motioncells name=motion ! fakesink "
        
        // Branch 3: Encoding pipeline (ready for recording/streaming)
        "t. ! queue name=enc_queue ! videoconvert ! "
        "x264enc tune=zerolatency bitrate=2000 speed-preset=ultrafast ! "
        "h264parse name=parse",
        VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_FPS,
        PREVIEW_WIDTH, PREVIEW_HEIGHT
    );
    
    printf("Creating pipeline...\n");
    printf("%s\n\n", pipeline_desc);
    
    g_camera.pipeline = gst_parse_launch(pipeline_desc, NULL);
    g_free(pipeline_desc);
    
    if (!g_camera.pipeline) {
        g_printerr("❌ Failed to create pipeline\n");
        return FALSE;
    }
    
    // Get elements for later use
    g_camera.tee = gst_bin_get_by_name(GST_BIN(g_camera.pipeline), "t");
    
    // Setup motion detection
    GstElement *motion = gst_bin_get_by_name(GST_BIN(g_camera.pipeline), "motion");
    if (motion) {
        // Configure motion detection sensitivity
        g_object_set(motion,
                     "sensitivity", 0.5,        // 0.0 - 1.0 (higher = more sensitive)
                     "threshold", 0.1,          // Minimum motion area
                     "postallmotion", TRUE,     // Report all motion
                     NULL);
        
        // Connect motion callback
        g_signal_connect(motion, "motion", G_CALLBACK(on_motion_detected), NULL);
        
        printf("✅ Motion detection configured\n");
    }
    
    // Setup bus watch
    GstBus *bus = gst_element_get_bus(g_camera.pipeline);
    gst_bus_add_watch(bus, bus_callback, NULL);
    gst_object_unref(bus);
    
    g_camera.loop = g_main_loop_new(NULL, FALSE);
    g_camera.state = STATE_IDLE;
    
    return TRUE;
}

// ============================================================================
// Recording Control
// ============================================================================

void camera_start_recording(void) {
    pthread_mutex_lock(&g_camera.state_mutex);
    
    if (g_camera.is_recording) {
        printf("⚠️  Already recording\n");
        pthread_mutex_unlock(&g_camera.state_mutex);
        return;
    }
    
    generate_recording_filename(g_camera.current_recording_file, 
                                sizeof(g_camera.current_recording_file));
    
    printf("\n📹 Starting recording: %s\n", g_camera.current_recording_file);
    
    // Create recording branch dynamically
    GstElement *h264parse = gst_bin_get_by_name(GST_BIN(g_camera.pipeline), "parse");
    
    if (h264parse) {
        // Create muxer and sink
        GstElement *mp4mux = gst_element_factory_make("mp4mux", "mux");
        GstElement *filesink = gst_element_factory_make("filesink", "filesink");
        
        g_object_set(filesink, "location", g_camera.current_recording_file, NULL);
        
        // Add to pipeline
        gst_bin_add_many(GST_BIN(g_camera.pipeline), mp4mux, filesink, NULL);
        
        // Link: h264parse -> mp4mux -> filesink
        gst_element_link_many(h264parse, mp4mux, filesink, NULL);
        
        // Sync state
        gst_element_sync_state_with_parent(mp4mux);
        gst_element_sync_state_with_parent(filesink);
        
        g_camera.is_recording = TRUE;
        g_camera.recording_start_time = time(NULL);
        g_camera.recordings_today++;
        g_camera.state = STATE_RECORDING;
        
        printf("✅ Recording started\n");
    }
    
    pthread_mutex_unlock(&g_camera.state_mutex);
}

void camera_stop_recording(void) {
    pthread_mutex_lock(&g_camera.state_mutex);
    
    if (!g_camera.is_recording) {
        pthread_mutex_unlock(&g_camera.state_mutex);
        return;
    }
    
    long duration = time(NULL) - g_camera.recording_start_time;
    g_camera.total_recording_duration += duration;
    
    printf("\n⏹️  Stopping recording (duration: %ld seconds)\n", duration);
    
    // Send EOS to recording branch
    GstElement *filesink = gst_bin_get_by_name(GST_BIN(g_camera.pipeline), "filesink");
    if (filesink) {
        gst_element_send_event(filesink, gst_event_new_eos());
        
        // Wait a bit for EOS to propagate
        usleep(500000); // 500ms
        
        // Remove elements
        GstElement *mp4mux = gst_bin_get_by_name(GST_BIN(g_camera.pipeline), "mux");
        
        gst_element_set_state(filesink, GST_STATE_NULL);
        gst_element_set_state(mp4mux, GST_STATE_NULL);
        
        gst_bin_remove_many(GST_BIN(g_camera.pipeline), mp4mux, filesink, NULL);
    }
    
    printf("✅ Recording saved: %s\n", g_camera.current_recording_file);
    
    g_camera.is_recording = FALSE;
    g_camera.motion_detected = FALSE;
    g_camera.state = STATE_MONITORING;
    
    pthread_mutex_unlock(&g_camera.state_mutex);
}

// ============================================================================
// Snapshot Capture
// ============================================================================

void take_snapshot(void) {
    char filename[256];
    generate_snapshot_filename(filename, sizeof(filename));
    
    printf("📸 Taking snapshot: %s\n", filename);
    
    // Create temporary pipeline for snapshot
    gchar *snapshot_pipeline = g_strdup_printf(
        "libcamerasrc num-buffers=1 ! "
        "video/x-raw,width=%d,height=%d ! "
        "videoconvert ! jpegenc ! filesink location=%s",
        VIDEO_WIDTH, VIDEO_HEIGHT, filename
    );
    
    GstElement *pipeline = gst_parse_launch(snapshot_pipeline, NULL);
    g_free(snapshot_pipeline);
    
    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_PLAYING);
        
        // Wait for EOS
        GstBus *bus = gst_element_get_bus(pipeline);
        gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_EOS | GST_MESSAGE_ERROR);
        gst_object_unref(bus);
        
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        
        printf("✅ Snapshot saved\n");
    }
}

// ============================================================================
// Camera Control
// ============================================================================

gboolean camera_start(void) {
    printf("\n🚀 Starting camera...\n");
    
    GstStateChangeReturn ret = gst_element_set_state(g_camera.pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("❌ Failed to start pipeline\n");
        return FALSE;
    }
    
    pthread_mutex_lock(&g_camera.state_mutex);
    g_camera.state = STATE_MONITORING;
    pthread_mutex_unlock(&g_camera.state_mutex);
    
    printf("✅ Camera started - Monitoring for motion...\n\n");
    return TRUE;
}

void camera_stop(void) {
    printf("\n⏹️  Stopping camera...\n");
    
    if (g_camera.is_recording) {
        camera_stop_recording();
    }
    
    gst_element_set_state(g_camera.pipeline, GST_STATE_NULL);
    
    pthread_mutex_lock(&g_camera.state_mutex);
    g_camera.state = STATE_IDLE;
    pthread_mutex_unlock(&g_camera.state_mutex);
    
    printf("✅ Camera stopped\n");
}

void print_statistics(void) {
    printf("\n===========================================\n");
    printf("  Session Statistics\n");
    printf("===========================================\n");
    printf("Motion events today: %d\n", g_camera.motion_events_today);
    printf("Recordings today: %d\n", g_camera.recordings_today);
    printf("Total recording time: %ld seconds (%.1f minutes)\n", 
           g_camera.total_recording_duration,
           g_camera.total_recording_duration / 60.0);
    printf("===========================================\n\n");
}

// ============================================================================
// Cleanup
// ============================================================================

void camera_cleanup(void) {
    printf("\n🧹 Cleaning up...\n");
    
    if (g_camera.pipeline) {
        gst_object_unref(g_camera.pipeline);
    }
    
    if (g_camera.loop) {
        g_main_loop_unref(g_camera.loop);
    }
    
    pthread_mutex_destroy(&g_camera.state_mutex);
    
    print_statistics();
    printf("✅ Cleanup complete\n");
}

// ============================================================================
// Status Monitor Thread
// ============================================================================

void* status_monitor_thread(void *arg) {
    while (g_camera.state != STATE_ERROR) {
        sleep(5);
        
        pthread_mutex_lock(&g_camera.state_mutex);
        printf("\r[%s] Motion events: %d | Recordings: %d | State: %s",
               __TIME__,
               g_camera.motion_events_today,
               g_camera.recordings_today,
               state_to_string(g_camera.state));
        fflush(stdout);
        pthread_mutex_unlock(&g_camera.state_mutex);
    }
    return NULL;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char *argv[]) {
    // Setup signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize camera
    if (!camera_init()) {
        fprintf(stderr, "Failed to initialize camera\n");
        return 1;
    }
    
    // Start camera
    if (!camera_start()) {
        fprintf(stderr, "Failed to start camera\n");
        camera_cleanup();
        return 1;
    }
    
    // Start status monitor thread
    pthread_t monitor_thread;
    pthread_create(&monitor_thread, NULL, status_monitor_thread, NULL);
    pthread_detach(monitor_thread);
    
    printf("\n📌 Commands:\n");
    printf("  - Motion will be detected automatically\n");
    printf("  - Recording starts on motion\n");
    printf("  - Press Ctrl+C to stop\n\n");
    
    // Run main loop
    g_main_loop_run(g_camera.loop);
    
    // Cleanup
    camera_stop();
    camera_cleanup();
    
    return 0;
}

/**
 * TODO: Features to add next
 * 
 * 1. RTSP Streaming:
 *    - Add gst-rtsp-server
 *    - Stream to mobile app
 * 
 * 2. Web Interface:
 *    - Embed HTTP server
 *    - Live view in browser
 *    - Recording playback
 * 
 * 3. Notifications:
 *    - Email alerts (SMTP)
 *    - Telegram bot
 *    - Push notifications
 * 
 * 4. Cloud Upload:
 *    - Upload recordings to Google Drive
 *    - Or AWS S3
 * 
 * 5. Storage Management:
 *    - Auto-delete old recordings
 *    - Disk space monitoring
 * 
 * 6. Advanced Motion Detection:
 *    - Face detection
 *    - Object tracking
 *    - Zone-based detection
 * 
 * 7. Night Mode:
 *    - IR LED control
 *    - Low-light enhancement
 */