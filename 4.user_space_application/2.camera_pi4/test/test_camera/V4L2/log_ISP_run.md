root@raspberrypi4-64:~/workspace# ./isp_test 
[V4L2]: Detected sensor: ov5647 !
[V4L2]: Pipeline is IMMUTABLE, skipping links. Setting formats...
[V4L2]: Pipeline configured. Use /dev/video0 for capture.
Camera device opened successfully
[V4L2]: Driver: unicam
[V4L2]: Card: unicam
[V4L2]: Bus: platform:fe801000.csi
[V4L2]: Capabilities: 0xa5a00001
[V4L2]: Current format:
        Width: 640
        Height: 480
        Pixel Format: GB10
        Bytes per line: 1280
        Size image: 614400
Camera format set successfully: 640x480
[DEBUG]: Driver format: GB10
[DEBUG]: Field: 1
[DEBUG]: Field enum value = 1
[DEBUG]: Field = NONE
Requested 4 buffers successfully
All buffers queued successfully
[DEBUG]: Buffers requested: 4
[DEBUG]: Buffers queued: 4
[DEBUG]: Driver: unicam
[DEBUG]: Card: unicam
[DEBUG]: Capabilities: 0xa5a00001
[DEBUG]: Verified format before streaming:
  Width: 640
  Height: 480
  Pixelformat: GB10
  Bytesperline: 1280
  Sizeimage: 614400
Camera streaming started
Camera initialized successfully
[ISP]: Initialized with defaults
Starting camera capture...
Buffer dequeued successfully
Captured frame in buffer index: 0
Captured frame size: 614400 bytes
[ISP]: Initializing Gamma LUT (gamma=2.20)...
[ISP]: Processing 640x480 frame...
[ISP]: Unpacking 10-bit data...
[ISP]: Black level correction...
[ISP]: Demosaicing...
Segmentation fault (core dumped)
