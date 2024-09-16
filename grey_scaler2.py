import cv2

# Load the image
image = cv2.imread('path_to_your_image.jpg')

# Convert to grayscale
gray_image = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)

# Save or display the grayscale image
cv2.imwrite('path_to_save_gray_image.jpg', gray_image)
# or use cv2.imshow to display the image
# cv2.imshow('Grayscale Image', gray_image)
# cv2.waitKey(0)
# cv2.destroyAllWindows()