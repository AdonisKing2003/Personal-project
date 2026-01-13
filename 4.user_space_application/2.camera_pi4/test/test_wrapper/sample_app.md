# Sample Camera Application - User Guide

## 📋 Tổng quan

Sample app này demo **TẤT CẢ** các tính năng của RPI Camera Wrapper:

### ✨ Features:
- ✅ Create/Start/Stop/Destroy camera
- ✅ Capture frames với callback
- ✅ Điều chỉnh brightness, contrast, exposure, gain
- ✅ Thay đổi settings **REAL-TIME** trong khi capture
- ✅ Lưu frames ra file (YUV/RGB/MJPEG)
- ✅ Statistics: FPS, frame size, brightness
- ✅ Interactive menu để control
- ✅ Signal handling (Ctrl+C graceful shutdown)
- ✅ Multi-threading (capture + control)
- ✅ Thread-safe statistics

---

## 🚀 Build

### Thêm vào CMakeLists.txt:

```cmake
# Sample application
set(SAMPLE_APP_SOURCES
  ${PROJECT_SOURCE_DIR}/examples/sample_camera_app.c
)

add_executable(sample_camera_app
  ${SAMPLE_APP_SOURCES}
)

target_link_libraries(sample_camera_app PRIVATE
  rpi_camera_wrapper
  Threads::Threads
)
```

### Build:

```bash
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=$OE_CMAKE_TOOLCHAIN_FILE ..
make sample_camera_app

# Deploy
scp sample_camera_app librpi_camera_wrapper.so pi@raspberrypi:~/
```

---

## 🎮 Usage

### 1. Chạy với default settings (1280x720 YUV420):

```bash
ssh pi@raspberrypi
export LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH
./sample_camera_app
```

### 2. Chạy với custom resolution:

```bash
./sample_camera_app 1920 1080
```

### 3. Chạy với custom format:

```bash
./sample_camera_app 640 480 yuv     # YUV420
./sample_camera_app 640 480 rgb     # RGB888
./sample_camera_app 640 480 mjpeg   # MJPEG
```

---

## 🖥️ Interactive Menu

Khi app đang chạy, bạn có thể:

```
┌───────────────────────────────────────────┐
│         CAMERA CONTROL MENU               │
├───────────────────────────────────────────┤
│ b - Adjust brightness                     │
│ c - Adjust contrast                       │
│ e - Adjust exposure                       │
│ g - Adjust gain                           │
│ s - Toggle frame saving (ON/OFF)          │
│ i - Show current settings                 │
│ q - Quit                                  │
└───────────────────────────────────────────┘
```

### Ví dụ sử dụng:

#### Điều chỉnh brightness:
```
Enter command: b
Enter brightness (-1.0 to 1.0): 0.5
✓ Brightness set to 0.50
```

#### Bật frame saving:
```
Enter command: s
✓ Frame saving: ENABLED
```

Sau đó mỗi 30 frames (1 giây) sẽ tự động lưu vào `./captured_frames/`

#### Xem settings hiện tại:
```
Enter command: i

┌─────────────────────────────────────┐
│      Current Settings               │
├─────────────────────────────────────┤
│ Brightness:    0.50                 │
│ Contrast:      1.20                 │
│ Exposure:   15000 µs                │
│ Gain:          2.00                 │
│ Saving:      ON                     │
└─────────────────────────────────────┘
```

---

## 📊 Output Examples

### Console output:

```
╔═══════════════════════════════════════════════════════════╗
║         RPI CAMERA WRAPPER - SAMPLE APPLICATION          ║
╚═══════════════════════════════════════════════════════════╝

→ Creating output directory: ./captured_frames
✓ Output directory ready

→ Setting up signal handlers
✓ Signal handlers installed (Ctrl+C to stop)

→ Creating camera: 1280x720, YUV420
Camera created: 1280x720
✓ Camera created successfully

→ Configuring camera controls
  ✓ Brightness: 0.00
  ✓ Contrast: 1.00
  ✓ Exposure: 10000 µs
  ✓ Gain: 1.00

→ Starting camera capture
Camera started
✓ Camera started, capturing frames...

═══════════════════════════════════════════════════════════
              CAPTURING - Press 'q' to quit
═══════════════════════════════════════════════════════════

┌─────────────────────────────────────────┐
│ First frame captured!                   │
├─────────────────────────────────────────┤
│ Sequence:        0                      │
│ Size:      1382400 bytes                │
│ Timestamp: 1234567890123456             │
└─────────────────────────────────────────┘

Frame    30 | FPS:  29.8 | Size: 1382400 B | Avg: 1382400 B | Brightness: 128/255
Frame    60 | FPS:  30.1 | Size: 1382400 B | Avg: 1382400 B | Brightness: 130/255
  → Saved frame to: ./captured_frames/frame_0001_seq60.yuv
Frame    90 | FPS:  29.9 | Size: 1382400 B | Avg: 1382400 B | Brightness: 129/255
  → Saved frame to: ./captured_frames/frame_0002_seq90.yuv
```

### Final statistics:

```
╔═══════════════════════════════════════════════════════════╗
║                    CAPTURE STATISTICS                     ║
╠═══════════════════════════════════════════════════════════╣
║ Total Frames:          450                                ║
║ Saved Frames:           15                                ║
║ Duration:            15.02 seconds                        ║
║ Average FPS:         29.96                                ║
╠═══════════════════════════════════════════════════════════╣
║ Total Data:         593.26 MB                             ║
║ Average Size:      1382400 bytes/frame                    ║
║ Min Size:          1382400 bytes                          ║
║ Max Size:          1382400 bytes                          ║
╠═══════════════════════════════════════════════════════════╣
║ Resolution:       1280x720                                ║
║ Format:           YUV420                                  ║
╠═══════════════════════════════════════════════════════════╣
║ Brightness:          0.50                                 ║
║ Contrast:            1.20                                 ║
║ Exposure:         15000 µs                                ║
║ Gain:                2.00                                 ║
╚═══════════════════════════════════════════════════════════╝
```

---

## 📁 Saved Frames

Khi enable frame saving (command `s`), frames sẽ được lưu vào:

```
./captured_frames/
├── frame_0001_seq30.yuv
├── frame_0002_seq60.yuv
├── frame_0003_seq90.yuv
└── ...
```

### View saved frames:

#### YUV420 frames:
```bash
# Single frame
ffplay -f rawvideo -pixel_format yuv420p \
       -video_size 1280x720 \
       frame_0001_seq30.yuv

# All frames as video
ffmpeg -f rawvideo -pixel_format yuv420p \
       -video_size 1280x720 -framerate 30 \
       -i frame_%04d_seq*.yuv \
       -c:v libx264 output.mp4
```

#### RGB888 frames:
```bash
ffplay -f rawvideo -pixel_format rgb24 \
       -video_size 1280x720 \
       frame_0001_seq30.rgb
```

#### MJPEG frames:
```bash
# Đã là JPEG, view trực tiếp
eog frame_0001_seq30.jpg
# hoặc
feh frame_0001_seq30.jpg
```

---

## 🎯 Workflow Demo

### Scenario 1: Chụp timelapse

```bash
./sample_camera_app 1920 1080 yuv

# Trong app:
s  # Enable saving
# Wait 5 minutes
q  # Quit

# Convert to video
cd captured_frames
ffmpeg -f rawvideo -pixel_format yuv420p \
       -video_size 1920x1080 -framerate 1 \
       -i frame_%04d_seq*.yuv \
       -c:v libx264 timelapse.mp4
```

### Scenario 2: Test exposure settings

```bash
./sample_camera_app 640 480 yuv

# Test các exposure khác nhau:
e
5000    # 5ms - bright scene

e
20000   # 20ms - dark scene

e
50000   # 50ms - very dark

# Compare brightness values in output
```

### Scenario 3: Compare formats

```bash
# YUV420
./sample_camera_app 1280 720 yuv
# Note: Size và FPS

# RGB888
./sample_camera_app 1280 720 rgb
# Note: Size lớn hơn, FPS thấp hơn

# MJPEG
./sample_camera_app 1280 720 mjpeg
# Note: Variable size, high FPS
```

---

## 🔧 Customization

### Thay đổi save interval:

```c
// Line 17
#define SAVE_INTERVAL       30  // Save every 30 frames

// Change to:
#define SAVE_INTERVAL       1   // Save every frame (careful: lots of disk!)
#define SAVE_INTERVAL       300 // Save every 10 seconds (at 30fps)
```

### Thay đổi capture duration:

```c
// Line 16
#define CAPTURE_DURATION    10  // seconds

// Change to:
#define CAPTURE_DURATION    60  // 1 minute
#define CAPTURE_DURATION    -1  // Infinite (until quit)
```

### Add auto-adjust brightness:

```c
// In frame_callback(), after brightness calculation:

if (state->format == RPI_FMT_YUV420) {
    brightness = calculate_brightness(frame->data, frame->size);
    
    // Auto-adjust
    if (brightness < 100) {
        // Too dark, increase brightness
        state->brightness += 0.1;
        rpi_camera_set_brightness(state->camera, state->brightness);
    } else if (brightness > 180) {
        // Too bright, decrease brightness
        state->brightness -= 0.1;
        rpi_camera_set_brightness(state->camera, state->brightness);
    }
}
```

---

## 🐛 Troubleshooting

### Problem 1: No camera detected

```
✗ Failed to create camera
```

**Check:**
```bash
# 1. Camera enabled?
libcamera-hello --list-cameras

# 2. Permissions?
groups
# Should include 'video'

# 3. Cable connected?
vcgencmd get_camera
# Should show: supported=1 detected=1
```

### Problem 2: Cannot create output directory

```
✗ Failed to create output directory
```

**Solution:**
```bash
# Check permissions
ls -ld ./captured_frames
# or
mkdir -p captured_frames
chmod 755 captured_frames
```

### Problem 3: Saved frames corrupted

```
ffplay: Invalid data found
```

**Check:**
- Đúng format? (yuv420p vs rgb24)
- Đúng resolution?
- File size đúng? (width × height × 1.5 cho YUV420)

```bash
ls -l captured_frames/
# YUV420 1280x720 = 1,382,400 bytes
# RGB888 1280x720 = 2,764,800 bytes
```

### Problem 4: Low FPS

**Possible causes:**
- High resolution (try 640x480)
- RGB format (try YUV420)
- Slow callback processing
- Disk I/O (saving too many frames)

**Test:**
```bash
# Disable saving
# Command 's' to toggle OFF
# Check if FPS improves
```

---

## 📚 Code Structure

```
main()
  ├─ Initialize state
  ├─ Create output directory
  ├─ Setup signal handler
  ├─ Create camera ──────────────► rpi_camera_create()
  ├─ Configure controls:
  │    ├─ Set brightness ────────► rpi_camera_set_brightness()
  │    ├─ Set contrast ──────────► rpi_camera_set_contrast()
  │    ├─ Set exposure ──────────► rpi_camera_set_exposure()
  │    └─ Set gain ──────────────► rpi_camera_set_gain()
  ├─ Start camera ───────────────► rpi_camera_start()
  │                                    │
  │                                    └─► frame_callback()
  │                                          ├─ Update statistics
  │                                          ├─ Calculate brightness
  │                                          └─ Save frame (if enabled)
  ├─ Start control thread
  │    └─ control_thread()
  │         └─ Interactive menu
  │              ├─ Adjust settings (real-time)
  │              └─ Toggle saving
  ├─ Wait for quit
  ├─ Stop camera ────────────────► rpi_camera_stop()
  ├─ Print statistics
  └─ Cleanup ────────────────────► rpi_camera_destroy()
```

---

## 💡 Learning Points

### 1. **Camera lifecycle:**
```c
create → configure → start → [capturing] → stop → destroy
```

### 2. **Real-time control:**
Có thể thay đổi settings (brightness, exposure, etc.) **trong khi** đang capture.

### 3. **Callback design:**
Callback phải **NHANH**. Nếu chậm → drop frames.
```c
// Good: Quick processing
void callback(frame) {
    update_stats(frame);  // Fast
    if (should_save) queue_save(frame);  // Async
}

// Bad: Slow processing
void callback(frame) {
    heavy_processing(frame);  // Slow!
    write_to_disk(frame);     // Blocking I/O!
}
```

### 4. **Thread safety:**
Multiple threads truy cập shared data → cần mutex:
```c
pthread_mutex_lock(&state->stats_mutex);
state->total_frames++;  // Critical section
pthread_mutex_unlock(&state->stats_mutex);
```

### 5. **Format tradeoffs:**
- YUV420: Nhanh, nhẹ, cần convert để view
- RGB888: Chậm, nặng, ready-to-use
- MJPEG: Variable size, good for streaming

---

## 🎓 Exercises

### Exercise 1: Add motion detection
Tính difference giữa frames liên tiếp, alert nếu > threshold.

### Exercise 2: Add network streaming
Gửi frames qua UDP/TCP socket.

### Exercise 3: Add face detection
Integrate OpenCV để detect faces.

### Exercise 4: Add auto-exposure
Adjust exposure dựa trên brightness histogram.

### Exercise 5: Add timestamp overlay
Draw timestamp lên frame trước khi save.

---

## ✅ Summary

Sample app này demo:
- ✅ **ALL** wrapper APIs
- ✅ Real-time control
- ✅ Frame saving
- ✅ Statistics tracking
- ✅ Interactive UI
- ✅ Signal handling
- ✅ Multi-threading
- ✅ Thread-safe design

Đây là **production-ready** template để bạn bắt đầu project camera của mình! 🚀