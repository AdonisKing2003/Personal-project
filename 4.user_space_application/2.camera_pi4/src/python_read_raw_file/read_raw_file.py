import numpy as np
import cv2

def view_rpi_raw(filename, width, height):
    # OV5647 10-bit packed: 5 bytes represent 4 pixels
    raw_data = np.fromfile(filename, dtype=np.uint8)
    
    # Simple visualization: treat it as 8-bit grayscale first 
    # to see if the shapes look right.
    # Note: This will look "weird" due to the packing, but you'll see the scene.
    img = raw_data[:height*width].reshape((height, width))
    
    # Convert Bayer to BGR (Assuming BGGR pattern for OV5647)
    # This is a simplified display.
    color_img = cv2.cvtColor(img, cv2.COLOR_BayerBG2BGR)
    
    cv2.imshow("Preview", color_img)
    cv2.waitKey(0)

# Replace with your actual capture resolution
view_rpi_raw('frame.raw', 1920, 1080)