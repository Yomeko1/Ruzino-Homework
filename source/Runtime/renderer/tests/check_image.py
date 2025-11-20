"""
Check if rendered image is all black
"""
from PIL import Image
import numpy as np
import sys

if len(sys.argv) < 2:
    print("Usage: python check_image.py <image_path>")
    sys.exit(1)

img_path = sys.argv[1]
img = Image.open(img_path)
arr = np.array(img)

print(f"Image: {img_path}")
print(f"Size: {img.size}")
print(f"Mode: {img.mode}")
print(f"Array shape: {arr.shape}")
print(f"Data type: {arr.dtype}")
print(f"Min value: {arr.min()}")
print(f"Max value: {arr.max()}")
print(f"Mean value: {arr.mean():.2f}")
print(f"Non-zero pixels: {np.count_nonzero(arr)}/{arr.size}")

if arr.max() == 0:
    print("\n⚠️  IMAGE IS COMPLETELY BLACK!")
else:
    print("\n✅ Image has non-black pixels")
    # Show some sample pixels
    print(f"\nSample pixel values (first 5 non-zero):")
    nonzero = np.argwhere(arr > 0)
    for i, pos in enumerate(nonzero[:5]):
        print(f"  Pixel {tuple(pos)}: {arr[tuple(pos)]}")
