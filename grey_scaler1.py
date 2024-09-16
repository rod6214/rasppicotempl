from PIL import Image
import sys

if len(sys.argv) < 2:
    print("No image path provided.")
    sys.exit()

img_path = sys.argv[1]

# Open the image
image = Image.open(img_path)

# Convert to grayscale
gray_image = image.convert('L')

# Save or display the grayscale image
gray_image.save('gray_image.jpg')
# or use gray_image.show() to display the image
# gray_image.show()