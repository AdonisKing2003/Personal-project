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
    
    rpi_frame_callback_t callback;
    void *userdata;
    
    int width;
    int height;
    rpi_format_t format;
    
    std::atomic<bool> running;
    std::thread capture_thread;

    uint64_t cookie; // Add cookie to store our identifier
    
    ~rpi_camera_t() {
        if (allocator) delete allocator;
    }
};

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
                
                // Gọi callback
                if (cam->callback) {
                    cam->callback(&frame, cam->userdata);
                }
                
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
    cam->callback = nullptr;
    cam->userdata = nullptr;
    cam->allocator = nullptr;
    cam->cookie = g_next_cookie++; // Assign unique cookie

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

int rpi_camera_start(rpi_camera_t *cam, rpi_frame_callback_t callback, void *userdata) {
    if (!cam || !callback) return -1;
    
    cam->callback = callback;
    cam->userdata = userdata;
    cam->running = true;
    
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
    if (!cam) return -1;
    
    cam->running = false;
    cam->camera->stop();
    
    std::cout << "Camera stopped" << std::endl;
    return 0;
}

void rpi_camera_destroy(rpi_camera_t *cam) {
    if (!cam) return;
    
    if (cam->running) {
        rpi_camera_stop(cam);
    }
    
    /* Remove from global map */
    g_camera_map.erase(cam->cookie);

    cam->camera->release();
    cam->cm->stop();
    
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