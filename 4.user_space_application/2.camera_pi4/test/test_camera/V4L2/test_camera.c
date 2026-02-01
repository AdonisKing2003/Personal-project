#include "camera.h"
#include <stdio.h>
#include <stdint.h>
#include <linux/v4l2-controls.h>

char *device_path = "/dev/video0";

/**
 * @brief Unpack RAW10 (5 bytes for 4 pixels) to 16-bit (2 bytes per pixel)
 * @param input: buffer từ camera (SGBRG10)
 * @param output: buffer uint16_t đã được cấp phát bộ nhớ (width * height * 2 bytes)
 */
void unpack_raw10_to_16bit(const uint8_t *input, uint16_t *output, int width, int height) {
    int i, j;
    // Mỗi vòng lặp xử lý 5 byte input để tạo ra 4 pixel output
    for (i = 0, j = 0; j < width * height; i += 5, j += 4) {
        // Pixel 1: 8 bit cao từ byte 0, 2 bit thấp từ byte 4 (bit 0-1)
        output[j]   = (uint16_t)(input[i] << 2) | (input[i+4] & 0x03);
        
        // Pixel 2: 8 bit cao từ byte 1, 2 bit thấp từ byte 4 (bit 2-3)
        output[j+1] = (uint16_t)(input[i+1] << 2) | ((input[i+4] >> 2) & 0x03);
        
        // Pixel 3: 8 bit cao từ byte 2, 2 bit thấp từ byte 4 (bit 4-5)
        output[j+2] = (uint16_t)(input[i+2] << 2) | ((input[i+4] >> 4) & 0x03);
        
        // Pixel 4: 8 bit cao từ byte 3, 2 bit thấp từ byte 4 (bit 6-7)
        output[j+3] = (uint16_t)(input[i+3] << 2) | ((input[i+4] >> 6) & 0x03);
    }
}

void save_example_frame(const char *filename, uint16_t *data, int width, int height) {
    FILE *fp = fopen(filename, "wb");
    if (fp) {
        // Lưu toàn bộ mảng uint16_t xuống file
        fwrite(data, sizeof(uint16_t), width * height, fp);
        fclose(fp);
        printf("[INFO]: Saved Example Frame to %s (%d bytes)\n", 
               filename, width * height * 2);
    }
}

int main()
{
    st_camera cam;
    if(camera_init(&cam, device_path) != 0) {
        printf("[ERROR]: Camera initialization failed\n");
        return -1;
    }
    else {
        printf("Camera initialized successfully\n");
    }
    // Sau khi gọi VIDIOC_STREAMON
    int skip_frames = 10; // Bỏ qua 10 frame đầu để ổn định ánh sáng
    for (int i = 0; i < skip_frames; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        // Dequeue
        ioctl(cam.fd, VIDIOC_DQBUF, &buf);
        
        // Không xử lý gì cả, chỉ in log nhẹ để theo dõi
        printf("Skipping warm-up frame %d...\n", i);

        // Re-queue ngay lập tức để driver tiếp tục làm việc
        ioctl(cam.fd, VIDIOC_QBUF, &buf);
    }

    uint8_t *frame;
    size_t size;
    if(camera_start_capture(&cam, &frame, &size) != 0) {
        printf("[ERROR]: Camera capture failed\n");
        camera_release(&cam);
        return -1;
    }
    int width = cam.fmt.fmt.pix.width;
    int height = cam.fmt.fmt.pix.height;
    
    // 1. Cấp phát bộ nhớ cho mảng đã unpack
    uint16_t *unpacked_buffer = malloc(width * height * sizeof(uint16_t));
    
    if (unpacked_buffer) {
        // 2. Thực hiện giải mã bit
        unpack_raw10_to_16bit(frame, unpacked_buffer, width, height);
        
        // 3. Lưu thành file Example Frame
        save_example_frame("example_frame_16bit.raw", unpacked_buffer, width, height);
        
        // 4. (Tùy chọn) Kiểm tra giá trị pixel đầu tiên để xem độ sáng
        printf("[DEBUG]: Pixel[0] value: %u / 1023\n", unpacked_buffer[0]);
        
        free(unpacked_buffer);
    }
    else {
        printf("[ERROR]: Memory allocation for unpacked buffer failed\n");
    }
    printf("Captured frame size: %zu bytes\n", size);
    printf("[DEBUG]: First 20 bytes of raw data:\n");
    for (int i = 0; i < 20; i++) {
        printf("%02X ", frame[i]);
    }
    printf("\n");

    // Tính giá trị trung bình để xem độ sáng
    unsigned long sum = 0;
    for (size_t i = 0; i < size; i++) {
        sum += frame[i];
    }
    printf("[DEBUG]: Average byte value: %lu\n", sum / size);
    // Process the frame data in 'frame' buffer as needed
    FILE *fp = fopen("captured_frame.raw", "wb");
    if (fp) {
        fwrite(frame, 1, size, fp);
        fclose(fp);
        printf("[INFO]: Saved raw frame to captured_frame.raw\n");
    }
    else {
        printf("[ERROR]: Failed to open file for writing\n");
    }
    // Do nothing, wait to develop further
    camera_release(&cam);
    return 0;
}