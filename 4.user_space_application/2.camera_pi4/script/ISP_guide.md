# ISP

- What "Software ISP" really means:
    - Software ISP = you implement the pipeline yourself
RAW Bayer
  ↓
Black level correction
  ↓
Demosaic
  ↓
AWB
  ↓
Color correction
  ↓
Gamma / tone map
  ↓
Noise reduction
  ↓
Sharpen
  ↓
Scale / crop
--> Every block is your code.

## 1. Capture RAW Bayer

` libcamera-still --raw -o frame.dng `

This gives you:
- RAW Bayer
- Metadata (black level, WB gains)

## 2. Understand Your RAW Data

| Item           | Where to get |
|----------------|--------------|
| Bayer pattern  | Sensor datasheet/DNG metadata     |
| Bit depth      | 10 / 12      |
| Black level    | DNG metadata |
| White level    | 2^bit - 1    |
| Active area    | Crop info    |

Example (IMX219)
- Bayer: RGB
- Bit depth: 10
- Black level: ~64
- White level: 1023

## 3. Load RAW Bayer Frame
- Python example (DNG)

```python
import rawpy
import numpy as np

raw = rawpy.imread("frame.dng")
bayer = raw.raw_image.astype(np.float32)
```

- At this point:
    - Single channel
    - Pattern like:

```python
R G R G
G B G B
```

## 4. Black Level Correction (MANDATORY)
- Remove sensor offset

```python
black = raw.black_level_per_channel[0]
bayer = np.clip(bayer - black, 0, None)
```

💡why?
- Without this -> washed colors, bad noise

## 5. Linearization / Normalization
- Convert to floating point [0-1]

```python
white = raw.white_level
bayer = bayer / white
```

## 6. Demosaicing (CORE STEP)
- Convert Bayer -> RGB

:white_check_mark: Simple: Bilinear

```python
rgb = rawpy.enhance.demosaic(bayer, raw.raw_pattern)
```

- Better options:
    - Malvar-He-Cutler
    - Edge-aware
    - AHD
- libraries:
    - rawpy (Python)
    - OpenCV
    - libdc1394
    - libraw

:warning: Demosaic defines image quality more than anything else.

## 7. Auto White Balance (AWB)
- Multiply channels separately

```python
r_gain, g_gain, b_gain = raw.camera_whitebalance
rgb[:,:,0] *= r_gain
rgb[:,:,1] *= g_gain
rgb[:,:,2] *= b_gain
```

- Manual AWB (Gray World)

```python
avg = rgb.mean(axis=(0,1))
rgb *= avg.mean() / arg
```

## 8. Color Correction Matrix (CCM)
- Convert sensor color -> sRGB
```python
ccm = np.array(raw.color_matrix)
rgb = np.tensordot(rgb, ccm.T, axes=1)
```

- Without CCM:
:x: Colors look wrong (green/magenta shift)

## 9. Gamma/Tone Mapping
- Sensor data is linear, display is non-linear

```python
gamma = 1/2.2
rgb = np.clip(rgb, 0, 1) ** gamma
```

- Optional:
    - Filmic curve
    - Log curve
    - HDR tone mapping

## 10. Noise Reduction (OPTIONAL early / better late)
- Before demosaic (advanced)
    - Temporal NR
    - Bayer-domain NR
- After demosaic (easy)
```python
import cv2
rgb = cv2.fastNlMeansDenoisingColored(
    (rgb*255).astype(np.uint8), None, 10, 10, 7, 21
).astype(np.float32)/255
```

## 11. Sharpening
```python
kernel = np.array([[0, -1, 0], [-1, 5, -1], [0, -1, 0]])
rgb = cv2.filter2D(rgb, -1, kernel)
```
Avoid oversharpen -> halos

## 12. Crop / Scale
```python
rgb = rgb[100:900, 100:900] # Crop
rgb = cv2.resize(rgb, (640, 480))
```

## 13. Output
```python
cv2.imwrite("output.png", (rgb*255).astype(np.uint8))
```

## :pushpin:14. Minimal Working Pipeline (Summary)

Raw Bayer
- Black level
- Normalize
- Demosaic
- White balance
- Color correction
- Gamma
- Denoise
- Sharpen
- Scale/Crop
- RGB output

## :gear:15. Performance on Raspberry Pi

| Method | FPS |
|--------|------|
| Python(Numpy) | 1-3 FPS |
| C++ + OpenCV | 5-15 FPS |
| NEON optimized | 15-30 FPS |
| GPU (OpenGL/Vulkan) | 30+ FPS |

## Pro tips:
- Order matters
- Don't denoise after sharpening
- Use fixed-point math for embedded
- ISP tuning = months, not days
- Hardware ISP &ne; magic, just parallelism
