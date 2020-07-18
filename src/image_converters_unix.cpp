#include "image_converters_unix.h"

#include <math.h>

#define BMASK 0x001f
#define GMASK 0x07e0
#define RMASK 0xf800
#define MASK 0xff000000
#define BITMULT5 8.226
#define BITMULT6 4.048


void r5g6b5ToRGBA(int w, int h, unsigned char *src, uint32_t *sink) {

	uint16_t *inPixels = reinterpret_cast<uint16_t *>(src);

    for(int i = 0; i < w * h; i++) {

        uint32_t outPixel = 0;

    	auto inPixel = inPixels[i];

        uint32_t b8 = floor(( inPixel & BMASK)        * BITMULT5 + 0.1);
        uint32_t g8 = floor(((inPixel & GMASK) >> 5)  * BITMULT6 + 0.1);
        uint32_t r8 = floor(((inPixel & RMASK) >> 11) * BITMULT5 + 0.1);

        outPixel |= (MASK >> 8  & b8 << 16);
        outPixel |= (MASK >> 16 & g8 << 8);
        outPixel |= (MASK >> 24 & r8);
        outPixel |= MASK;

        sink[i] = outPixel;     
    }
}


void a8r8g8b8ToRBGA(int w, int h, unsigned char *src, uint32_t *sink) { 

    uint32_t *inPixels = reinterpret_cast<uint32_t *>(src);

    for(int i = 0; i < w * h; i++) {

        uint32_t inPixel = inPixels[i];
        uint32_t outPixel = 0;

        outPixel |= (MASK >> 8    & inPixel << 16);
        outPixel |= (MASK >> 16   & inPixel << 8);
        outPixel |= (MASK >> 24   & inPixel << 0);
        outPixel |= (MASK - (MASK & inPixel << 24));

        sink[i] = outPixel;       
    }
}


void bc2ToRGBA(int w, int h, unsigned char *src, uint32_t *sink){

    
}