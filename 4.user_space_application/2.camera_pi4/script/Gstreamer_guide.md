```bash
# Ví dụ: Phát video từ file
gst-launch-1.0 filesrc location=video.mp4 ! decodebin ! autovideosink

# Ví dụ: Ghi âm từ microphone
gst-launch-1.0 autoaudiosrc ! audioconvert ! audioresample ! vorbisenc ! oggmux ! filesink location=recording.ogg

# Ví dụ: Webcam đơn giản
gst-launch-1.0 autovideosrc ! videoconvert ! autovideosink
```

# Some pipeline popular

## 1. Play video:
```bash
gst-launch-1.0 filesrc location=video.mp4 ! qtdemux ! h264parse ! avdec_h264 ! videoconvert ! autovideosink
```

## 2. Streaming RTSP
```bash
gst-launch-1.0 rtspsrc location=rtsp://example.com/stream ! rtph264depay ! h264parse ! avdec_h264 ! autovideosink
```

## 3. Change format
```bash
gst-launch-1.0 filesrc location=input.avi ! decodebin ! x264enc ! mp4mux ! filesink location=output.mp4
```

## 4. Simple pipeline structure
```bash
Source → Filter → Filter → ... → Sink
```

- Source  : Input data (filesrc, videotestsrc, autovideosrc, v.v.)
- Filter  : Processing data (videoconvert, audioresample, encoder, v.v.)
- Sink    : Destination (filesink, autovideosink, autoaudiosink, v.v.)
