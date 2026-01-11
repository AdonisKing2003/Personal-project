# 1. Test camera with libcamera
```bash
root@raspberrypi4-64:~# libcamera-hello list-cameras
[0:17:39.870824143] [458]  INFO Camera camera_manager.cpp:327 libcamera v0.4.0+dirty (2025-12-07T15:39:35UTC)
[0:17:39.985680127] [459]  WARN RPiSdn sdn.cpp:40 Using legacy SDN tuning - please consider moving SDN inside rpi.denoise
[0:17:39.988998489] [459]  INFO RPI vc4.cpp:401 Registered camera /base/soc/i2c0mux/i2c@1/ov5647@36 to Unicam device /dev/media4 and ISP device /dev/media0
Preview window unavailable
Mode selection for 1296:972:12:P
    SGBRG10_CSI2P,640x480/0 - Score: 3296
    SGBRG10_CSI2P,1296x972/0 - Score: 1000
    SGBRG10_CSI2P,1920x1080/0 - Score: 1349.67
    SGBRG10_CSI2P,2592x1944/0 - Score: 1567
Stream configuration adjusted
[0:17:40.006543482] [458]  INFO Camera camera.cpp:1202 configuring streams: (0) 1296x972-YUV420 (1) 1296x972-SGBRG10_CSI2P
[0:17:40.008539788] [459]  INFO RPI vc4.cpp:570 Sensor: /base/soc/i2c0mux/i2c@1/ov5647@36 - Selected sensor format: 1296x972-SGBRG10_1X10 - Selected unicam format: 1296x972-pGAA
#0 (0.00 fps) exp 33239.00 ag 8.00 dg 1.00
#1 (30.01 fps) exp 33239.00 ag 8.00 dg 1.00
#2 (30.01 fps) exp 33239.00 ag 8.00 dg 1.00
#3 (30.01 fps) exp 33239.00 ag 8.00 dg 1.00
#4 (30.01 fps) exp 33239.00 ag 8.00 dg 1.00
#5 (30.01 fps) exp 33239.00 ag 8.00 dg 1.00
#6 (30.01 fps) exp 33239.00 ag 8.00 dg 1.00
#7 (30.01 fps) exp 33239.00 ag 8.00 dg 1.00
#8 (30.01 fps) exp 33239.00 ag 8.00 dg 1.00
...
```
:white_check_mark: Can detect camera


# 2. Test camera with cam -l
```bash
root@raspberrypi4-64:~# cam -l
[0:18:36.830680051] [467]  INFO Camera camera_manager.cpp:327 libcamera v0.4.0+dirty (2025-12-07T15:39:35UTC)
[0:18:36.871035709] [468]  WARN RPiSdn sdn.cpp:40 Using legacy SDN tuning - please consider moving SDN inside rpi.denoise
[0:18:36.873631415] [468]  INFO RPI vc4.cpp:401 Registered camera /base/soc/i2c0mux/i2c@1/ov5647@36 to Unicam device /dev/media4 and ISP device /dev/media0
Available cameras:
1: 'ov5647' (/base/soc/i2c0mux/i2c@1/ov5647@36)
...
```

:white_check_mark: Pi4 can recognize camera

# 3. Test gstreamer
```bash
root@raspberrypi4-64:~# gst-launch-1.0 videotestsrc ! videoconvert ! autovideosink
Setting pipeline to PAUSED ...
warning: queue 0x7fac000be0 destroyed while proxies still attached:
  xdg_wm_base@7 still attached
  wl_seat@6 still attached
  wl_subcompositor@5 still attached
  wl_compositor@4 still attached
  wl_registry@2 still attached
Pipeline is PREROLLING ...
Got context from element 'autovideosink0': gst.gl.GLDisplay=context, gst.gl.GLDisplay=(GstGLDisplay)"\(GstGLDisplayWayland\)\ gldisplaywayland0";
Pipeline is PREROLLED ...
Setting pipeline to PLAYING ...
Redistribute latency...
New clock: GstSystemClock
0:00:00.0 / 99:99:99.
^Chandling interrupt.
Interrupt: Stopping pipeline ...
Execution ended after 0:00:32.698699355
Setting pipeline to NULL ...
...
```

```bash
gst-launch-1.0 videotestsrc ! videoconvert ! autovideosink
```

- Uses videotestsrc, which is a synthetic pattern generator; it does not touch the Pi camera at all.
- autovideosink on a modern desktop usually picks a Wayland-capable sink, so any Wayland-related warnings are about the display backend only.
- The 0:00:00.0 / 99:99:99. and the clean shutdown after Ctrl‑C are expected gst-launch-1.0 status messages.
​
:white_check_mark: Gstreamer oke, but do not have **libcamerasrc**
👉 videotestsrc ! videoconvert ! autovideosink only checks that GStreamer runs and can render video to your display; it never touches the Pi camera driver or libcamera stack.

# 4. Test v4l2
**1. Check v4l2**
```bash
root@raspberrypi4-64:~/workspace# v4l2-ctl --list-devices
bcm2835-codec-decode (platform:bcm2835-codec):
	/dev/video10
	/dev/video11
	/dev/video12
	/dev/video18
	/dev/video31
	/dev/media3

bcm2835-isp (platform:bcm2835-isp):
	/dev/video13
	/dev/video14
	/dev/video15
	/dev/video16
	/dev/video20
	/dev/video21
	/dev/video22
	/dev/video23
	/dev/media0
	/dev/media1

unicam (platform:fe801000.csi):
	/dev/video0
	/dev/media4

rpivid (platform:rpivid):
	/dev/video19
	/dev/media2
...
```
**2. Test v4l2**
```bash
root@raspberrypi4-64:~/workspace# gst-launch-1.0 v4l2src device=/dev/video0 ! videoconvert ! autovideosink
Setting pipeline to PAUSED ...
warning: queue 0x7f7c000be0 destroyed while proxies still attached:
  xdg_wm_base@7 still attached
  wl_seat@6 still attached
  wl_subcompositor@5 still attached
  wl_compositor@4 still attached
  wl_registry@2 still attached
Pipeline is live and does not need PREROLL ...
Got context from element 'autovideosink0': gst.gl.GLDisplay=context, gst.gl.GLDisplay=(GstGLDisplay)"\(GstGLDisplayWayland\)\ gldisplaywayland0";
Pipeline is PREROLLED ...
Setting pipeline to PLAYING ...
New clock: GstSystemClock
ERROR: from element /GstPipeline:pipeline0/GstV4l2Src:v4l2src0: Failed to allocate required memory.
Additional debug info:
/usr/src/debug/gstreamer1.0-plugins-good/1.22.12/sys/v4l2/gstv4l2src.c(950): gst_v4l2src_decide_allocation (): /GstPipeline:pipeline0/GstV4l2Src:v4l2src0:
Buffer pool activation failed
ERROR: from element /GstPipeline:pipeline0/GstV4l2Src:v4l2src0: Internal data stream error.
Additional debug info:
/usr/src/debug/gstreamer1.0/1.22.12/libs/gst/base/gstbasesrc.c(3134): gst_base_src_loop (): /GstPipeline:pipeline0/GstV4l2Src:v4l2src0:
streaming stopped, reason not-negotiated (-4)
Execution ended after 0:00:00.055619996
Setting pipeline to NULL ...
warning: queue 0x7f7c0dae30 destroyed while proxies still attached:
  xdg_wm_base@13 still attached
  wl_seat@12 still attached
  wl_subcompositor@11 still attached
  wl_compositor@10 still attached
  wl_registry@14 still attached
Freeing pipeline ...
...
```

hits memory/buffer‑pool negotiation errors because /dev/video0 in this libcamera‑based stack is not a simple UVC‑style capture node designed for direct use by applications like GStreamer.

The unicam node expects to be driven as part of the libcamera pipeline, not by arbitrary user‑space buffer allocation.

That is why you see Failed to allocate required memory and not-negotiated; GStreamer’s v4l2src does not match what the driver/libcamera expect.
​
So:

Yes, libcamera-still ultimately talks through the same hardware path that /dev/video0 represents.

But no, libcamera-still is not just “a thin wrapper over /dev/video0” in the way v4l2src is; it uses libcamera’s full pipeline and controls, while v4l2src tries to use /dev/video0 directly and fails on this modern stack.
​
:warning: **Error**

# 5. Get image from camera using libcamera
```bash
libcamera-still -n -t 1000 -o test.jpg
```

-n disables preview (no window).
-t 1000 makes it run for 1 second.
:warning:**Let it finish by itself; do not press Ctrl‑C. Then check for test.jpg in the current directory.**



# 6. Stream from pi to laptop

**1. Setup On ubbuntu**

```bash
sudo apt update
sudo apt install \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad \
  gstreamer1.0-plugins-ugly \
  gstreamer1.0-libav
```

**2. Start stream on pi4:**
```bash
root@raspberrypi4-64:~/workspace# libcamera-vid -t 0   --width 1280 --height 720 --framerate 25   --codec h264 --bitrate 2000000 -o - | gst-launch-1.0 -v fdsrc ! h264parse ! rtph264pay config-interval=1 pt=96 ! udpsink host=10.42.0.1 port=5000
```
**3. Get stream on ubuntu:**
```bash
adonisking@adonisking-Nitro-AN515-55:~$ gst-launch-1.0 -v udpsrc port=5000 caps="application/x-rtp,media=video,encoding-name=H264,payload=96" ! rtph264depay ! avdec_h264 ! videoconvert ! autovideosink
```
---> Test OK, can stream


# 7. Test camera app
```bash
root@raspberrypi4-64:~/workspace# ./camera_test 
malloc(): corrupted top size
Aborted (core dumped)
```
