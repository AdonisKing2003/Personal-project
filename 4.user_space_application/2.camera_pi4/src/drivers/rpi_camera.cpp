// ============================================================================
// rpi_camera.cpp - C++ Implementation
// ============================================================================

#include <libcamera/libcamera.h>
#include "rpi_camera.h"
#include <sys/mman.h>
#include <iostream>
#include <memory>
#include <thread>
#include <atomic>
#include <map>

using namespace libcamera;

// Global map to store camera pointers by request cookie
static std::map<uint64_t, rpi_camera_t*> g_camera_map;
static uint64_t g_next_cookie = 1;

struct rpi_camera_t {
    std::unique_ptr<CameraManager> cm;
    std::shared_ptr<Camera> camera;
    std::unique_ptr<CameraConfiguration> config;
    FrameBufferAllocator *allocator;
    std::vector<std::unique_ptr<Request>> requests;
    
    int width;
    int height;
    rpi_format_t format;
    
    std::atomic<bool> running;
    std::thread capture_thread;

    uint64_t cookie; /* Add cookie to store our identifier*/
    std::unique_ptr<FramePipeline> pipeline;
    
    ~rpi_camera_t() = default;
};

struct InternalFrame {
    std::vector<uint8_t> data;  // COPY
    uint64_t timestamp;
    uint32_t sequence;
};

class FramePipeline {
public:
    explicit FramePipeline(size_t max)
        : max_size(max) {}

    bool push(InternalFrame &&f) {
        std::lock_guard<std::mutex> lk(mtx);

        if (queue.size() >= max_size) {
            dropped++;
            return false; // DROP
        }

        queue.push(std::move(f));
        cv.notify_one();
        return true;
    }

    bool pop(InternalFrame &out) {
        std::unique_lock<std::mutex> lk(mtx);

        cv.wait(lk, [&] {
            return !queue.empty() || stopped;
        });

        if (queue.empty())
            return false;

        out = std::move(queue.front());
        queue.pop();
        return true;
    }

    bool try_pop(InternalFrame &out) {
        std::lock_guard<std::mutex> lk(mtx);
        if (queue.empty())
            return false;

        out = std::move(queue.front());
        queue.pop();
        return true;
    }

    void stop() {
        std::lock_guard<std::mutex> lk(mtx);
        stopped = true;
        cv.notify_all();
    }

    void FramePipeline::reset()
    {
        std::lock_guard<std::mutex> lk(mtx);
        std::queue<InternalFrame> empty;
        std::swap(queue, empty);
        stopped = false;
    }

    uint64_t dropped_count() const { return dropped; }

private:
    std::queue<InternalFrame> queue;
    size_t max_size;

    std::mutex mtx;
    std::condition_variable cv;
    bool stopped = false;

    std::atomic<uint64_t> dropped{0};
};

/* API for user blocking */
int rpi_camera_get_frame(rpi_camera_t *cam, rpi_frame_t *out) {
    if (!cam || !out) return -1;

    InternalFrame f;
    if (!cam->pipeline->pop(f))
        return -1;

    out->size = f.data.size();
    out->data = (uint8_t *)malloc(out->size);
    memcpy(out->data, f.data.data(), out->size);

    out->timestamp = f.timestamp;
    out->sequence  = f.sequence;

    return 0;
}

/* API for user non-blocking */
int rpi_camera_try_get_frame(rpi_camera_t *cam, rpi_frame_t *out) {
    if (!cam || !out) return -1;

    InternalFrame f;
    if (!cam->pipeline->try_pop(f))
        return -EAGAIN;

    out->size = f.data.size();
    out->data = (uint8_t *)malloc(out->size);
    memcpy(out->data, f.data.data(), out->size);

    out->timestamp = f.timestamp;
    out->sequence  = f.sequence;

    return 0;
}

/* API for user free frame */
void rpi_camera_release_frame(rpi_frame_t *f) {
    if (!f) return;
    free(f->data);
    f->data = NULL;
}

// Chuyển đổi format
static PixelFormat to_libcamera_format(rpi_format_t fmt) {
    switch (fmt) {
        case RPI_FMT_YUV420:
            return formats::YUV420;
        case RPI_FMT_RGB888:
            return formats::RGB888;
        case RPI_FMT_MJPEG:
            return formats::MJPEG;
        default:
            return formats::YUV420;
    }
}

// Request completion handler
static void request_complete(Request *request) {
    if (request->status() == Request::RequestCancelled)
        return;

    // Get camera from global map using cookie
     uint64_t cookie = request->cookie();
     auto it = g_camera_map.find(cookie);
     if (it == g_camera_map.end()) {
         std::cerr << "Camera not found for cookie: " << cookie << std::endl;
         return;
     }
     rpi_camera_t *cam = it->second;
     FramePipeline *pipeline = cam->pipeline.get();

    const Request::BufferMap &buffers = request->buffers();
    
    for (auto bufferPair : buffers) {
        FrameBuffer *buffer = bufferPair.second;
        const FrameMetadata &metadata = buffer->metadata();
        
        // Lấy dữ liệu frame
        const std::vector<FrameBuffer::Plane> &planes = buffer->planes();
        
        if (!planes.empty()) {
            void *data = mmap(NULL, planes[0].length, PROT_READ, 
                            MAP_SHARED, planes[0].fd.get(), 0);
            
            if (data != MAP_FAILED) {
                rpi_frame_t frame;
                frame.data = data;
                frame.size = planes[0].length;
                frame.timestamp = metadata.timestamp;
                frame.sequence = metadata.sequence;
                
                /* -------- COPY FRAME -------- */
                InternalFrame copy;
                copy.data.resize(plane.length);
                memcpy(copy.data.data(), data, plane.length);

                copy.timestamp = metadata.timestamp;
                copy.sequence  = metadata.sequence;

                pipeline->push(std::move(copy));
                
                munmap(data, planes[0].length);
            }
        }
    }
    
    // Requeue request nếu vẫn đang chạy
    if (cam->running) {
        request->reuse(Request::ReuseBuffers);
        cam->camera->queueRequest(request);
    }
}

extern "C" {

rpi_camera_t* rpi_camera_create(int width, int height, rpi_format_t format) {
    rpi_camera_t *cam = new rpi_camera_t();
    cam->width = width;
    cam->height = height;
    cam->format = format;
    cam->running = false;
    cam->allocator = nullptr;
    cam->cookie = g_next_cookie++; // Assign unique cookie
    cam->pipeline = std::make_unique<FramePipeline>(4); // Example queue 4 frame

    // Register in global map
    g_camera_map[cam->cookie] = cam;

    // Khởi tạo CameraManager
    cam->cm = std::make_unique<CameraManager>();
    int ret = cam->cm->start();
    if (ret) {
        std::cerr << "Failed to start camera manager" << std::endl;
        delete cam;
        return nullptr;
    }
    
    // Tìm camera
    if (cam->cm->cameras().empty()) {
        std::cerr << "No cameras available" << std::endl;
        delete cam;
        return nullptr;
    }
    
    cam->camera = cam->cm->cameras()[0];
    ret = cam->camera->acquire();
    if (ret) {
        std::cerr << "Failed to acquire camera" << std::endl;
        delete cam;
        return nullptr;
    }
    else {
        std::cout << "Find camera: " << cam->camera->id() << std::endl;
    }
    
    // Cấu hình camera
    cam->config = cam->camera->generateConfiguration({StreamRole::VideoRecording}); /* StillCapture | Raw | Viewfinder | VideoRecording */
    StreamConfiguration &streamConfig = cam->config->at(0);
    
    streamConfig.size.width = width;
    streamConfig.size.height = height;
    streamConfig.pixelFormat = to_libcamera_format(format);
    
    CameraConfiguration::Status validation = cam->config->validate();
    if (validation == CameraConfiguration::Invalid) {
        std::cerr << "[ERROR]: Camera configuration invalid" << std::endl;
        delete cam;
        return nullptr;
    }
    else if (validation == CameraConfiguration::Adjusted) {
        std::cout << "[INFO]: Camera configuration adjusted by libcamera!" << std::endl;
    }
    else {
        std::cout<< "[INFO]: Camera config validate!" << std::endl;
    }
    
    ret = cam->camera->configure(cam->config.get());
    if (ret) {
        std::cerr << "[ERROR]: Failed to configure camera" << std::endl;
        delete cam;
        return nullptr;
    }
    else {
        std::cout<< "[INFO]: Camera configuration!" << std::endl;
    }
    
    // Allocate buffers
    cam->allocator = new FrameBufferAllocator(cam->camera);
    Stream *stream = streamConfig.stream();
    ret = cam->allocator->allocate(stream);
    if (ret < 0) {
        std::cerr << "Failed to allocate buffers" << std::endl;
        delete cam;
        return nullptr;
    }
    else {
        std::cout<<"[INFO]: Allocate buffers...!\n" << std::endl;
    }
    
    // Tạo requests
    const std::vector<std::unique_ptr<FrameBuffer>> &buffers = 
        cam->allocator->buffers(stream);
    
    for (unsigned int i = 0; i < buffers.size(); ++i) {
        std::unique_ptr<Request> request = cam->camera->createRequest(cam->cookie);
        if (!request) {
            std::cerr << "Failed to create request" << std::endl;
            delete cam;
            return nullptr;
        }
        
        const std::unique_ptr<FrameBuffer> &buffer = buffers[i];
        ret = request->addBuffer(stream, buffer.get());
        if (ret < 0) {
            std::cerr << "Failed to add buffer to request" << std::endl;
            delete cam;
            return nullptr;
        }
        
        cam->requests.push_back(std::move(request));
    }
    
    std::cout << "Camera created: " << width << "x" << height << std::endl;
    return cam;
}

int rpi_camera_start(rpi_camera_t *cam) {
    if (!cam || !cam->camera || !cam->pipeline) {
        std::cout<<"[ERROR]: NULL ptr...!"<<std::endl;
        return -1;
    }
    
    if (cam->running) {
        std::cout<<"[INFO]: Camera still running...!"<<std::endl;
        return 0;
    }

    /* Reset pipeline state */
    cam->pipeline->reset();
    
    // Connect request completion signal
    cam->camera->requestCompleted.connect(request_complete);
    
    // Start camera
    int ret = cam->camera->start();
    if (ret) {
        std::cerr << "Failed to start camera" << std::endl;
        return -1;
    }
    else {
        std::cout<<"[INFO]: Camera Start...!"
    }

    cam->running = true;
    
    // Queue all requests
    for (std::unique_ptr<Request> &request : cam->requests) {
        ret = cam->camera->queueRequest(request.get());
        if (ret < 0) {
            std::cerr << "[ERROR]: Failed to queue request" << std::endl;
            return -1;
        }
    }

    std::cout << "[INFO]: Camera started...!" << std::endl;
    return 0;
}

int rpi_camera_stop(rpi_camera_t *cam) {
    if (!cam || !cam->camera) {
        std::cout<<"[ERROR]: NULL ptr...!"<<std::endl;
        return -1;
    }

    if (!cam->running) {
        std::cout<<"[INFO]: Camera has not been running...!"<<std::endl;
        return 0;
    }
    
    cam->running = false;
    /* Stop camera first (stop producing frames) */
    cam->camera->stop();
    /* Stop pipeline to unblock get_frame() */
    if (cam->pipeline) {
        cam->pipeline->stop();
    }
    
    std::cout << "Camera stopped" << std::endl;
    return 0;
}

void rpi_camera_destroy(rpi_camera_t *cam) {
    if (!cam) return;
    /* 1. Stop camera */
    if (cam->running) {
        rpi_camera_stop(cam);
    }
    /* 2. Ensure pipeline stopped */
    if (cam->pipeline) {
        cam->pipeline->stop();
        cam->pipeline.reset();
    }
    /* 3. Join capture thread (if you really use it) */
    if(cam->capture_thread.joinable()) {
        cam->capture_thread.join();
    }
    /* 4. Clear requests (release FrameBuffer refs) */
    cam->requests.clear();
    /* 5. Delete allocator (owns buffers)*/
    delete cam->allocator;
    cam->allocator = nullptr;

    /* 6. Remove from global map */
    g_camera_map.erase(cam->cookie);
    /* 7. Release camera */
    if(cam->camera) {
        cam->camera->release();
        cam->camera.reset();
    }
    /* 8. Stop manager*/
    if(cam->cm) {
        cam->cm->stop();
        cam->cm.reset();
    }
    /* 9. Delete cam */
    delete cam;
}

// Các hàm cấu hình nâng cao
int rpi_camera_set_brightness(rpi_camera_t *cam, float value) {
    if (!cam) return -1;
    
    // ControlList controls;
    // controls.set(controls::Brightness, value);
    
    // Create a request and add controls
    for (auto &req : cam->requests) {
        req->controls().set(controls::Brightness, value);
    }
    // Request *request = cam->requests[0].get();
    // request->controls() = controls;
    
    return 0;
}

int rpi_camera_set_contrast(rpi_camera_t *cam, float value) {
    if (!cam) return -1;
    
    // ControlList controls;
    // controls.set(controls::Contrast, value);
    
    // Request *request = cam->requests[0].get();
    // request->controls() = controls;

    for (auto &req : cam->requests) {
        req->controls().set(controls::Contrast, value);
    }
    
    return 0;
}

int rpi_camera_set_exposure(rpi_camera_t *cam, int microseconds) {
    if (!cam) return -1;
    
    // ControlList controls;
    // controls.set(controls::ExposureTime, microseconds);
    
    // Request *request = cam->requests[0].get();
    // request->controls() = controls;
    for (auto &req : cam->requests) {
        req->controls().set(controls::ExposureTime, microseconds);
    }
    
    return 0;
}

int rpi_camera_set_gain(rpi_camera_t *cam, float value) {
    if (!cam) return -1;
    
    // ControlList controls;
    // controls.set(controls::AnalogueGain, value);
    
    // Request *request = cam->requests[0].get();
    // request->controls() = controls;
    for (auto &req : cam->requests) {
        req->controls().set(controls::AnalogueGain, value);
    }
    
    return 0;
}

} // extern "C"