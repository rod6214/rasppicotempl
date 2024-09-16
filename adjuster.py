import cv2
import sys

OLED_HEIGHT = 32
OLED_WIDTH = 32

if len(sys.argv) < 2:
    print("No image path provided.")
    sys.exit()

img_path = sys.argv[1]
# Load the image
image = cv2.imread(img_path)

# Define the new size (width, height)
new_size = (OLED_WIDTH, OLED_HEIGHT)  # for example, resizing to 800x600

# Resize the image
resized_image = cv2.resize(image, new_size, interpolation=cv2.INTER_AREA)

# Save or display the resized image
cv2.imwrite('resized_image.jpg', resized_image)
# or use cv2.imshow to display the image
# cv2.imshow('Resized Image', resized_image)
# cv2.waitKey(0)
# cv2.destroyAllWindows()