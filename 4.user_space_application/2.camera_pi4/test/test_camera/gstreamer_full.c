/**
 * @file test_gstreamer.c
 * @brief GStreamer Pipeline Test Application
 * 
 * This file demonstrates basic GStreamer usage with 5 different examples:
 * 
 * 1. Video Test Pattern Display
 *    - Shows color test pattern (SMPTE bars) on screen
 *    - Good for testing display output
 * 
 * 2. Webcam Live Display
 *    - Captures video from /dev/video0 (webcam)
 *    - Displays live video feed on screen
 * 
 * 3. Webcam Recording to File
 *    - Records video from webcam to MP4 file
 *    - Uses H.264 encoding for compression
 *    - Press Ctrl+C to stop recording
 * 
 * 4. Video File Playback
 *    - Plays any video file (MP4, AVI, etc.)
 *    - Auto-detects codec and decodes
 * 
 * 5. Manual Pipeline Construction
 *    - Demonstrates building pipeline element-by-element
 *    - Provides more control than parse_launch()
 * 
 * Key Concepts:
 * - Pipeline: Chain of elements processing media data
 * - Elements: Source → Filters → Sink
 * - Bus: Message system for pipeline events (errors, EOS, state changes)
 * - States: NULL → READY → PAUSED → PLAYING
 * 
 * Usage:
 *   ./gstreamer_test [1-5] [optional: video_file for option 4]
 * 
 * Examples:
 *   ./gstreamer_test 1              # Show test pattern
 *   ./gstreamer_test 2              # Display webcam
 *   ./gstreamer_test 3              # Record to output.mp4
 *   ./gstreamer_test 4 video.mp4    # Play video file
 *   ./gstreamer_test 5              # Manual pipeline demo
 * 
 * Requirements:
 * - GStreamer 1.0 development libraries
 * - v4l2 plugin for webcam access (tests 2-3)
 * - x264 plugin for video encoding (test 3)
 * 
 * @author Claude AI
 * @date 2026-01-07
 */
#include <gst/gst.h>
#include <stdio.h>
#include <signal.h>

// Global pipeline pointer for signal handling
static GstElement *pipeline = NULL;
static GMainLoop *loop = NULL;

// Signal handler for Ctrl+C
void signal_handler(int signum) {
    printf("\nStopping pipeline...\n");
    if (loop) {
        g_main_loop_quit(loop);
    }
}

// Bus message handler - xử lý các message từ pipeline
static gboolean bus_callback(GstBus *bus, GstMessage *msg, gpointer data) {
    GMainLoop *loop = (GMainLoop *)data;
    
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError *err;
            gchar *debug;
            gst_message_parse_error(msg, &err, &debug);
            g_printerr("Error: %s\n", err->message);
            g_error_free(err);
            g_free(debug);
            g_main_loop_quit(loop);
            break;
        }
        case GST_MESSAGE_EOS:
            g_print("End of stream\n");
            g_main_loop_quit(loop);
            break;
        case GST_MESSAGE_STATE_CHANGED: {
            GstState old_state, new_state, pending_state;
            gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline)) {
                g_print("Pipeline state changed from %s to %s\n",
                        gst_element_state_get_name(old_state),
                        gst_element_state_get_name(new_state));
            }
            break;
        }
        case GST_MESSAGE_WARNING: {
            GError *err;
            gchar *debug;
            gst_message_parse_warning(msg, &err, &debug);
            g_printerr("Warning: %s\n", err->message);
            g_error_free(err);
            g_free(debug);
            break;
        }
        case GST_MESSAGE_INFO: {
            GError *err;
            gchar *debug;
            gst_message_parse_info(msg, &err, &debug);
            g_print("Info: %s\n", err->message);
            g_error_free(err);
            g_free(debug);
            break;
        }
        default:
            break;
    }
    
    return TRUE;
}

// Example 1: Simple test video display
int test_video_test_src() {
    GstBus *bus;
    guint bus_watch_id;
    
    printf("\n=== Test 1: Video Test Source ===\n");
    printf("Displaying color test pattern...\n");
    
    // Create pipeline from description string
    pipeline = gst_parse_launch(
        "videotestsrc pattern=smpte ! "
        "videoconvert ! "
        "autovideosink",
        NULL
    );
    
    if (!pipeline) {
        g_printerr("Failed to create pipeline\n");
        return -1;
    }
    
    // Add bus watch
    bus = gst_element_get_bus(pipeline);
    bus_watch_id = gst_bus_add_watch(bus, bus_callback, loop);
    gst_object_unref(bus);
    
    // Start playing
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    
    // Run main loop
    g_main_loop_run(loop);
    
    // Cleanup
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_source_remove(bus_watch_id);
    
    return 0;
}

// Example 2: Webcam capture and display
int test_webcam_display() {
    GstBus *bus;
    guint bus_watch_id;
    
    printf("\n=== Test 2: Webcam Display ===\n");
    printf("Opening webcam and displaying video...\n");
    
    // Try v4l2src for Linux camera
    pipeline = gst_parse_launch(
        "v4l2src device=/dev/video0 ! "
        "video/x-raw,width=640,height=480,framerate=30/1 ! "
        "videoconvert ! "
        "autovideosink",
        NULL
    );
    
    if (!pipeline) {
        g_printerr("Failed to create pipeline\n");
        g_printerr("Make sure /dev/video0 exists and v4l2src plugin is available\n");
        return -1;
    }
    
    bus = gst_element_get_bus(pipeline);
    bus_watch_id = gst_bus_add_watch(bus, bus_callback, loop);
    gst_object_unref(bus);
    
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_main_loop_run(loop);
    
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_source_remove(bus_watch_id);
    
    return 0;
}

// Example 3: Record video from webcam to file
int test_webcam_record(const char *output_file) {
    GstBus *bus;
    guint bus_watch_id;
    
    printf("\n=== Test 3: Webcam Recording ===\n");
    printf("Recording video to: %s\n", output_file);
    printf("Press Ctrl+C to stop recording...\n");
    
    // Create pipeline string
    gchar *pipeline_str = g_strdup_printf(
        "v4l2src device=/dev/video0 ! "
        "video/x-raw,width=640,height=480,framerate=30/1 ! "
        "videoconvert ! "
        "x264enc tune=zerolatency bitrate=2000 ! "
        "mp4mux ! "
        "filesink location=%s",
        output_file
    );
    
    pipeline = gst_parse_launch(pipeline_str, NULL);
    g_free(pipeline_str);
    
    if (!pipeline) {
        g_printerr("Failed to create pipeline\n");
        return -1;
    }
    
    bus = gst_element_get_bus(pipeline);
    bus_watch_id = gst_bus_add_watch(bus, bus_callback, loop);
    gst_object_unref(bus);
    
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_main_loop_run(loop);
    
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_source_remove(bus_watch_id);
    
    printf("Recording saved to %s\n", output_file);
    return 0;
}

// Example 4: Play video file
int test_play_video(const char *video_file) {
    GstBus *bus;
    guint bus_watch_id;
    
    printf("\n=== Test 4: Play Video File ===\n");
    printf("Playing: %s\n", video_file);
    
    gchar *pipeline_str = g_strdup_printf(
        "filesrc location=%s ! "
        "decodebin ! "
        "videoconvert ! "
        "autovideosink",
        video_file
    );
    
    pipeline = gst_parse_launch(pipeline_str, NULL);
    g_free(pipeline_str);
    
    if (!pipeline) {
        g_printerr("Failed to create pipeline\n");
        return -1;
    }
    
    bus = gst_element_get_bus(pipeline);
    bus_watch_id = gst_bus_add_watch(bus, bus_callback, loop);
    gst_object_unref(bus);
    
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_main_loop_run(loop);
    
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_source_remove(bus_watch_id);
    
    return 0;
}

// Example 5: Manual pipeline construction (more control)
int test_manual_pipeline() {
    GstElement *source, *convert, *sink;
    GstBus *bus;
    guint bus_watch_id;
    
    printf("\n=== Test 5: Manual Pipeline Construction ===\n");
    
    // Create empty pipeline
    pipeline = gst_pipeline_new("manual-pipeline");
    
    // Create individual elements
    source = gst_element_factory_make("videotestsrc", "source");
    convert = gst_element_factory_make("videoconvert", "convert");
    sink = gst_element_factory_make("autovideosink", "sink");
    
    if (!pipeline || !source || !convert || !sink) {
        g_printerr("Failed to create elements\n");
        return -1;
    }
    
    // Configure elements
    g_object_set(source, "pattern", 18, NULL);  // Pattern 18 = ball
    
    // Add elements to pipeline
    gst_bin_add_many(GST_BIN(pipeline), source, convert, sink, NULL);
    
    // Link elements together
    if (!gst_element_link_many(source, convert, sink, NULL)) {
        g_printerr("Failed to link elements\n");
        gst_object_unref(pipeline);
        return -1;
    }
    
    bus = gst_element_get_bus(pipeline);
    bus_watch_id = gst_bus_add_watch(bus, bus_callback, loop);
    gst_object_unref(bus);
    
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_main_loop_run(loop);
    
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_source_remove(bus_watch_id);
    
    return 0;
}

int main(int argc, char *argv[]) {
    int choice = 0;
    
    // Initialize GStreamer
    gst_init(&argc, &argv);
    
    // Create main loop
    loop = g_main_loop_new(NULL, FALSE);
    
    // Setup signal handler
    signal(SIGINT, signal_handler);
    
    // Menu selection
    if (argc > 1) {
        choice = atoi(argv[1]);
    } else {
        printf("\n=== GStreamer Test Menu ===\n");
        printf("1. Video test pattern\n");
        printf("2. Webcam display\n");
        printf("3. Record webcam to file\n");
        printf("4. Play video file\n");
        printf("5. Manual pipeline construction\n");
        printf("\nUsage: %s [choice]\n", argv[0]);
        printf("Example: %s 1\n\n", argv[0]);
        
        printf("Enter choice (1-5): ");
        scanf("%d", &choice);
    }
    
    // Run selected test
    switch(choice) {
        case 1:
            test_video_test_src();
            break;
        case 2:
            test_webcam_display();
            break;
        case 3:
            test_webcam_record("output.mp4");
            break;
        case 4:
            if (argc > 2) {
                test_play_video(argv[2]);
            } else {
                printf("Please provide video file path\n");
                printf("Usage: %s 4 <video_file>\n", argv[0]);
            }
            break;
        case 5:
            test_manual_pipeline();
            break;
        default:
            printf("Invalid choice\n");
            break;
    }
    
    // Cleanup
    g_main_loop_unref(loop);
    
    return 0;
}