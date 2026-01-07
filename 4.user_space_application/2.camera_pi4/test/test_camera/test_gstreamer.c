#include <gst/gst.h>

int main(int argc, char *argv[]) {
    GstElement *pipeline, *source, *sink;
    GstBus *bus;
    GstMessage *msg;
    
    // Khởi tạo GStreamer
    gst_init(&argc, &argv);
    
    // Tạo elements
    source = gst_element_factory_make("videotestsrc", "source");
    sink = gst_element_factory_make("autovideosink", "sink");
    
    // Tạo pipeline
    pipeline = gst_pipeline_new("test-pipeline");
    
    if (!pipeline || !source || !sink) {
        g_printerr("Không thể tạo elements\n");
        return -1;
    }
    
    // Thêm elements vào pipeline
    gst_bin_add_many(GST_BIN(pipeline), source, sink, NULL);
    
    // Link elements
    if (!gst_element_link(source, sink)) {
        g_printerr("Không thể link elements\n");
        gst_object_unref(pipeline);
        return -1;
    }
    
    // Chạy pipeline
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    
    // Đợi lỗi hoặc EOS
    bus = gst_element_get_bus(pipeline);
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                                      GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
    
    // Dọn dẹp
    if (msg != NULL)
        gst_message_unref(msg);
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    
    return 0;
}