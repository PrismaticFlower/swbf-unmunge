/*
Naive image conversion methods. Will work tbb in soon...
*/

#include <opencv2/opencv.hpp> 

cv::Mat r5g6b5ToRGB(int height, int width, unsigned char *src);
cv::Mat a8r8g8b8ToRGB(int height, int width, unsigned char *src);
cv::Mat dxt3ToRGB(int height, int width, unsigned char *src, int size);