# 📊 Performance Benchmarks
| Resolution | Format | FPS | Frame Size | Bandwidth |
| --- | --- | --- | --- | --- |
| 640x480 | YUV420 | 30 | 460 KB | 13.8 MB/s |
| 1280x720 | YUV420 | 301.3 MB | 39 MB/s | 
| 1920x1080 | YUV420 | 253.0 MB | 75 MB/s |
| 640x480 | RGB888| 25900 KB | 22.5 MB/s |
| 1280x720 | MJPEG | 30~100 KB | 3 MB/s |

Tested on: Raspberry Pi 4, Camera Module v2

# RPI Camera Wrapper - Test Plan & Expected Results

## Test Overview

| Test Suite | File | Test Cases | Duration | Purpose |
|------------|------|------------|----------|---------|
| Basic Tests | test_basic.c | 5 | ~15s | API cơ bản, error handling |
| Format Tests | test_formats.c | 5 | ~20s | Các format và resolution |
| Control Tests | test_controls.c | 7 | ~25s | Camera controls |
| Stress Tests | test_stress.c | 7 | ~60s | Stability, memory leaks |

---

## Test Suite 1: Basic Tests (test_basic.c)

### Test 1.1: Create & Destroy
**Mục đích:** Kiểm tra khởi tạo và giải phóng camera

**Các bước:**
1. Tạo camera 640x480 YUV420
2. Destroy camera
3. Destroy NULL pointer

**Expected:**
- ✅ Camera tạo thành công (cam != NULL)
- ✅ Destroy không crash
- ✅ Destroy NULL không crash

---

### Test 1.2: Start & Stop
**Mục đích:** Kiểm tra start/stop capture

**Các bước:**
1. Tạo camera
2. Start với callback hợp lệ
3. Capture trong 3 giây
4. Stop camera
5. Validate frame count và sequence

**Expected:**
- ✅ Start thành công (ret == 0)
- ✅ Nhận được ít nhất 60 frames trong 3s (≥20fps)
- ✅ Sequence number tăng dần
- ✅ Timestamp tăng dần
- ✅ Stop thành công

---

### Test 1.3: Multiple Start/Stop
**Mục đích:** Kiểm tra start/stop nhiều lần

**Các bước:**
1. Tạo camera
2. Lặp 3 lần:
   - Start
   - Capture 1s
   - Stop
3. Validate mỗi cycle

**Expected:**
- ✅ Mỗi cycle capture ≥20 frames
- ✅ Không memory leak
- ✅ Không crash

---

### Test 1.4: Error Handling
**Mục đích:** Kiểm tra xử lý lỗi

**Các bước:**
1. Start với NULL callback → expect error
2. Stop camera chưa start → expect graceful
3. Start NULL camera → expect error

**Expected:**
- ✅ NULL callback bị reject (ret != 0)
- ✅ Stop camera chưa start không crash
- ✅ NULL camera bị reject

---

### Test 1.5: Frame Data Validation
**Mục đích:** Validate cấu trúc frame

**Các bước:**
1. Capture 2 giây
2. Validate mỗi frame:
   - data != NULL
   - size > 0
   - size ≈ expected (±10%)
   - timestamp tăng
   - sequence tăng

**Expected:**
- ✅ Tất cả frames hợp lệ
- ✅ Size đúng theo format
- ✅ Metadata chính xác

---

## Test Suite 2: Format Tests (test_formats.c)

### Test 2.1: YUV420 Format
**Mục đích:** Test YUV420 ở các resolution

**Test cases:**
| Resolution | Expected Size | Min FPS |
|------------|---------------|---------|
| 640x480 | 460,800 bytes | 20 |
| 1280x720 | 1,382,400 bytes | 20 |
| 1920x1080 | 3,110,400 bytes | 15 |

**Expected:**
- ✅ Frame size = width × height × 1.5
- ✅ FPS stable
- ✅ Không dropped frames

---

### Test 2.2: RGB888 Format
**Mục đích:** Test RGB888 format

**Test cases:**
| Resolution | Expected Size | Min FPS |
|------------|---------------|---------|
| 640x480 | 921,600 bytes | 15 |
| 1280x720 | 2,764,800 bytes | 10 |
| 1920x1080 | 6,220,800 bytes | 8 |

**Expected:**
- ✅ Frame size = width × height × 3
- ✅ FPS thấp hơn YUV (do data lớn hơn)
- ✅ Không corrupt data

---

### Test 2.3: MJPEG Format
**Mục đích:** Test MJPEG compression

**Expected:**
- ✅ Variable size (compression)
- ✅ Size < RGB888 (compressed)
- ✅ Compression ratio 5-20x
- ✅ FPS cao (compressed data)

---

### Test 2.4: Resolution Limits
**Mục đích:** Test min/max resolution

**Expected:**
- ✅ 320x240 works (minimum)
- ✅ 2592x1944 works hoặc reject gracefully (maximum)
- ✅ Invalid resolution bị reject

---

### Test 2.5: Format Switching
**Mục đích:** Switch giữa các format

**Expected:**
- ✅ Mỗi format hoạt động độc lập
- ✅ Không conflict
- ✅ Không memory leak

---

## Test Suite 3: Control Tests (test_controls.c)

### Test 3.1: Brightness Control
**Test:** Set brightness -0.5, 0.0, 0.5

**Expected:**
- ✅ API trả về success
- ✅ Frame brightness thay đổi tương ứng
- ✅ Trend: -0.5 < 0.0 < 0.5

---

### Test 3.2: Contrast Control
**Test:** Set contrast 0.5, 1.0, 1.5

**Expected:**
- ✅ API success
- ✅ Visual contrast thay đổi
- ✅ Không affect brightness

---

### Test 3.3: Exposure Control
**Test:** Set exposure 5ms, 10ms, 20ms

**Expected:**
- ✅ API success
- ✅ Brightness tăng theo exposure
- ✅ FPS có thể giảm với long exposure

---

### Test 3.4: Gain Control
**Test:** Set gain 1.0, 4.0, 8.0

**Expected:**
- ✅ API success
- ✅ Brightness tăng theo gain
- ✅ Noise tăng với high gain

---

### Test 3.5: Combined Controls
**Test:** Set tất cả controls cùng lúc

**Expected:**
- ✅ Tất cả controls work together
- ✅ Không conflict
- ✅ Capture bình thường

---

### Test 3.6: Dynamic Changes
**Test:** Thay đổi controls trong khi capture

**Expected:**
- ✅ Changes áp dụng ngay
- ✅ Không drop frames
- ✅ Không crash

---

### Test 3.7: Invalid Values
**Test:** Out-of-range values

**Expected:**
- ✅ Clamped hoặc rejected
- ✅ Không crash
- ✅ Camera vẫn hoạt động

---

## Test Suite 4: Stress Tests (test_stress.c)

### Test 4.1: Long Running (30s)
**Mục đích:** Test stability

**Expected:**
- ✅ ≥600 frames (≥20fps)
- ✅ Memory growth < 10MB
- ✅ FPS stable
- ✅ Không crash

---

### Test 4.2: Repeated Start/Stop (100 cycles)
**Mục đích:** Test memory leak

**Expected:**
- ✅ Memory growth < 5MB
- ✅ Không accumulate leak
- ✅ Performance consistent

---

### Test 4.3: Multiple Create/Destroy (50 cycles)
**Mục đích:** Test resource cleanup

**Expected:**
- ✅ Memory growth < 5MB
- ✅ Resources properly freed
- ✅ No file descriptor leak

---

### Test 4.4: High FPS Test
**Test:** 320x240 resolution

**Expected:**
- ✅ ≥25 FPS achieved
- ✅ Stable performance
- ✅ Low CPU usage

---

### Test 4.5: Frame Drop Test
**Test:** Slow callback (5ms delay)

**Expected:**
- ✅ Drop rate < 20%
- ✅ Sequence tracking accurate
- ✅ Graceful degradation

---

### Test 4.6: Concurrent Cameras
**Test:** Open 2 cameras simultaneously

**Expected:**
- ✅ Either works hoặc reject gracefully
- ✅ No interference
- ✅ Resources properly managed

---

### Test 4.7: Rapid Format Changes (20 cycles)
**Test:** Switch YUV→RGB→MJPEG repeatedly

**Expected:**
- ✅ Memory growth < 5MB
- ✅ All formats stable
- ✅ Không corrupt data

---

## Running Tests

### Build all tests:
```bash
cd tests
make all
```

### Run individual test:
```bash
make run_basic
make run_formats
make run_controls
make run_stress
```

### Run all tests:
```bash
make run_all
```

### Clean:
```bash
make clean
```

---

## Success Criteria

### ✅ All tests must pass:
- No crashes or segfaults
- No memory leaks (< 5MB growth)
- Performance targets met
- Error handling correct
- API behavior consistent

### 📊 Performance Targets:
- **FPS:** ≥20 for YUV420 640x480
- **Latency:** < 50ms per frame
- **Memory:** < 100MB total
- **CPU:** < 50% on RPi 4

### 🐛 Known Issues to Watch:
- High resolution might reduce FPS
- MJPEG size varies with scene
- Multiple cameras might not be supported
- Controls might have hardware limits

---

## Test Results Template

```
╔════════════════════════════════════════╗
║  Test Suite: [NAME]                    ║
╠════════════════════════════════════════╣
║  Test 1: [✓] PASSED                    ║
║  Test 2: [✓] PASSED                    ║
║  Test 3: [✗] FAILED - [reason]         ║
║  Test 4: [⚠] WARNING - [note]          ║
╠════════════════════════════════════════╣
║  Total: 3/4 passed (75%)               ║
╚════════════════════════════════════════╝
```

---

## Hardware Requirements

- Raspberry Pi 4 (or newer)
- Camera Module v2/v3
- libcamera installed
- 1GB+ free RAM
- Root access (for camera)

---

## Troubleshooting

### No camera detected:
```bash
libcamera-hello --list-cameras
```

### Permission denied:
```bash
sudo usermod -a -G video $USER
```

### Build errors:
```bash
sudo apt-get install libcamera-dev
```

1. Bảng so sánh tổng quan

| Format | Loại | Nén | Kích thước (640x480) | Use case chính |
| ---    | ---  | --- | ---                  | ---            |
| RAW | Sensor data | Không | ~307 KB (10-bit) | ISP input, professional |
| RGB | Color space | Không | ~922 KB (8-bit) | Display, processing |
| YUV | Color space | Không | ~461 KB (4:2:0) | Video codec, broadcasting |
| PPM | File format | Không | ~922 KB + header | Debug, interchange |
| JPEG | File format | Có (lossy) | ~50-150 KB | Storage, web, photos |
