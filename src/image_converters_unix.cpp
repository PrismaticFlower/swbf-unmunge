#include "image_converters_unix.h"
//#include "gli/gli.hpp"

#define RED_MASK 0xF800
#define GREEN_MASK 0x07E0
#define BLUE_MASK 0x001F

/*
TODO: REMOVE CODE DUPLICATION WITH LAMBDA BASED SOLUTION
*/


//Adapted from  https://stackoverflow.com/questions/34042112/opencv-mat-data-member-access
cv::Mat r5g6b5ToRGB(int height, int width, unsigned char *src) {

	unsigned short *srcUS = reinterpret_cast<unsigned short*>(src);

	cv::Mat imageRGB(height, width, CV_8UC3, cv::Scalar(10, 100, 150));
	unsigned char *sink = imageRGB.data;

    float factor5Bit = 255.0 / 31.0;
    float factor6Bit = 255.0 / 63.0;

    for(int i = 0; i < height; i++) {

        for(int j = 0; j < width; j++) {

        	int index = i * width + j;
            int outIndex = 3 * (i * width + j); 

        	unsigned short rgb565 = srcUS[index];
            uchar r5 = (rgb565 & RED_MASK)   >> 11;
            uchar g6 = (rgb565 & GREEN_MASK) >> 5;
            uchar b5 = (rgb565 & BLUE_MASK);

            // round answer to closest intensity in 8-bit space...
            uchar r8 = floor((r5 * factor5Bit) + 0.5);
            uchar g8 = floor((g6 * factor6Bit) + 0.5);
            uchar b8 = floor((b5 * factor5Bit) + 0.5);

            sink[outIndex] = r8;
            sink[outIndex + 1] = g8;
            sink[outIndex + 2] = b8;
        }
    }

    return imageRGB;
}

/*
cv::Mat dxt3ToRBG(std::size_t size, char *src) {

    gli::texture texTest = gli::load(const_cast<const char*>(src), size);


}
*/



cv::Mat a8r8g8b8ToRBG(int height, int width, unsigned char *src) { 

    cv::Mat imageRGB(height, width, CV_8UC3, cv::Scalar(10, 100, 150));
    unsigned char *sink = imageRGB.data;

    for(int i = 0; i < height; i++) {

        for(int j = 0; j < width; j++) {

            int index = 4 * i * width + j;
            int outIndex = 3 * (i * width + j); 

            sink[outIndex] = src[index + 1];
            sink[outIndex + 1] = src[index + 2];
            sink[outIndex + 2] = src[index + 3];
        }
    }

    return imageRGB;
}