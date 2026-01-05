# 1. Check camera available

` libcamera-hello list-cameras `

` cam -l `

# 2. Install GStreamer:

sudo apt-get update
sudo apt-get install libx264-dev libjpeg-dev
sudo apt-get install libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libgstreamer-plugins-bad1.0-dev \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-tools \
    gstreamer1.0-gl \
    gstreamer1.0-gtk3 \
    gstreamer1.0-qt5 \
    gstreamer1.0-pulseaudio

# 3. Check after Install:

gst-launch-1.0 videotestsrc ! videoconvert ! autovideosink

##### If SSH-only (no HDMI):

autovideosink → fakesink

# 4. Stream video from pi camera or USB:

gst-device-monitor-1.0

##### You should see something like:

libcamerasrc

v4l2src (/dev/video0) (USB cam)

# 5. Native GStreamer (No libcamera-vid)

# Pi Camera directly via GStreamer

gst-launch-1.0 libcamerasrc ! video/x-raw,width=1280,height=720,framerate=30/1 ! videoconvert ! autovideosink

## To encode & stream:

gst-launch-1.0 libcamerasrc ! \
video/x-raw,width=1280,height=720,framerate=30/1 ! \
v4l2h264enc ! h264parse ! rtph264pay ! \
udpsink host=`YOUR_LAPTOP_IP` port=5000

# 6. Stream from camera Pi

libcamera-vid -t 0 \
  --width 1280 --height 720 --framerate 25 \
  --codec h264 --bitrate 2000000 -o - | \
gst-launch-1.0 -v fdsrc ! h264parse ! \
rtph264pay config-interval=1 pt=96 ! \
udpsink host=`YOUR_LAPTOP_IP` port=5000

# laptop:

gst-launch-1.0 -v \
udpsrc port=5000 caps="application/x-rtp,media=video,encoding-name=H264,payload=96" ! \
rtph264depay ! avdec_h264 ! videoconvert ! autovideosink


# COMMON PI4 ISSUE and FIXES

## 1. Black screen
HDMI not connected

--> Try: ` export DISPLAY=:0 `

## 2. High CPU usage
Use hardware encoder

- Try: `v4l2h264enc`
- Avoid x264enc on Pi

## 3. Lag

- Use UDP instead of TCP

- Reduce resolution / bitrate

# PROJECT TO DO

## 1. What is RTSP?

- RTSP (Real Time Streaming Protocol) is a control protocol used for live video streaming.

Part	                    Role
RTSP	                    Control (PLAY / PAUSE / TEARDOWN)
RTP	                        Actual video/audio packets
UDP/TCP	                    Transport

👉 RTSP ≠ video data
👉 RTP carries the H.264/H.265 video
👉 RTSP tells when and how to stream

Why IP cameras use RTSP

- Standard protocol (VLC, ffplay, ONVIF tools)

- Low latency

- Supports authentication

- Multiple clients

## 2. How IP cameras are built (High-level)

Typical IP Camera Architecture:

Sensor
  ↓
ISP (AWB, AE, Demosaic, NR, Sharpen)
  ↓
YUV/RGB
  ↓
Video Encoder (H.264/H.265)
  ↓
RTP
  ↓
RTSP Server
  ↓
Client (VLC, NVR, VMS)

### On Raspberry Pi:

- Sensor + ISP → handled by libcamera

- Encoder → V4L2 hardware encoder

- RTSP → user-space server (GStreamer / FFmpeg / rtsp-simple-server)

## 3. Build a real IP Camera on Raspberry Pi 4

#### Option A - Easiest & Recommended (Production-like)

- Use `libcamera + GStreamer RTSP server`

sudo apt install -y libcamera-apps libcamera-dev libcamera-gstreamer gstreamer1.0-rtsp

- RTSO Server using GStreamer (Pi4)

    - Start RTSP server:
    gst-rtsp-launch "( \
libcamerasrc ! \
video/x-raw,width=1280,height=720,framerate=30/1 ! \
v4l2h264enc ! h264parse ! \
rtph264pay name=pay0 pt=96 )"

- On laptop:
    - vlx rtsp://PI_IP:8554/test

#### Option B - Ultra simple (libcamera-vid built-in RTSP)

libcamera-vid \
  --width 1280 --height 720 --framerate 30 \
  --codec h264 \
  --inline \
  --listen -o tcp://0.0.0.0:8554

- Client:
ffplay tcp://PI_IP:8554

--> Not standard RTSP, but useful for testing.

## 4. What is ISP?
ISP (Image Signal Processor) transforms raw Bayer data into usable images.

Typical ISP blocks:
    - Black level correction
    - Demosaic
    - AWB (Auto White Balance)
    - AE (Auto Exposure)
    - Gamma
    - Noise reduction
    - Sharpen
    - Color correction
    - Scaling

## 5. ISP on Raspberry Pi (Reality Check)

- Raspberry Pi does have ISP

    - Inside BCM2711
    - Closed firmware
    - Exposed via libcamera

👉 You cannot directly program ISP registers.
👉 You control ISP via libcamera controls.

## 6. How ISP works on Pi Camera

Pi Camera Sensor (RAW)
  ↓
Pi ISP (firmware, closed)
  ↓
libcamera (control interface)
  ↓
GStreamer / OpenCV / Encoder

- What you can control:
    - AWB mode
    - Exposure
    - Gain
    - Brightness / Contrast
    - Sharpness
    - Saturation
    - Noise reduction
    - Scaler / crop
    - HDR (on supported sensors)

- What you cannot control:
    - Write custom demosaic
    - Insert your own ISP block
    - Access raw ISP internals

## 7. Controlling ISP parameters (Examples)
#### AWB
libcamera-vid --awb tungsten

libcamera-vid --awb auto

#### Exposure
libcamera-vid --exposure normal

libcamera-vid --shutter 10000

libcamera-vid --gain 4

#### Disable ISP tuning (RAW)
libcamera-vid --raw -o test.h264

## 8. ISP tuning files (Advanced & poswerful)
Each sensor uses a JSON tuning file.

Example:
` /usr/share/libcamera/ipa/rpi/`

You can:
- Adjust AWB gains
- Modify noise reduction
- Tune sharpening
- Control color matrices

--> This is how real camera vendors tune ISP.

## 9. Build your own ISP pipeline (Software ISP)
If you want full ISP control (research/learning):
#### Option A -- RAW + OpenCV
`libcamera-raw -o frame.raw`
Then:
- Demosaic
- AWB
- Gamma
- Scale
- Encode

⚠️ CPU heavy, not real-time at high res.

#### Option B -- Use external ISP Soc
Professional cameras:
- Sony ISP
- TI ISP
- NXP ISP

--> Pi is not suitable for this level.

