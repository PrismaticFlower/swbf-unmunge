#include "image_converters_unix.h"

#define DETEX_VERSION 1
#include "detex.h"

#include <math.h>

#define BMASK_5BIT 0x001f
#define GMASK_6BIT 0x07e0
#define RMASK_5BIT 0xf800
#define MASK 0xff000000
#define BITMULT5 8.226
#define BITMULT6 4.048


void r5g6b5ToRGBA(int w, int h, unsigned char *src, uint32_t *sink) {

	uint16_t *inPixels = reinterpret_cast<uint16_t *>(src);

    for(int i = 0; i < w * h; i++) {

        uint32_t outPixel = 0;

    	auto inPixel = inPixels[i];

        uint32_t b8 = floor(( inPixel & BMASK_5BIT)        * BITMULT5 + 0.1);
        uint32_t g8 = floor(((inPixel & GMASK_6BIT) >> 5)  * BITMULT6 + 0.1);
        uint32_t r8 = floor(((inPixel & RMASK_5BIT) >> 11) * BITMULT5 + 0.1);

        outPixel |= (MASK >> 8  & b8 << 16);
        outPixel |= (MASK >> 16 & g8 << 8);
        outPixel |= (MASK >> 24 & r8);
        outPixel |= MASK;

        sink[i] = outPixel;     
    }
}


void a8r8g8b8ToRBGA(int w, int h, unsigned char *src, uint32_t *sink) { 

    uint32_t *inPixels = reinterpret_cast<uint32_t *>(src);

    for (int i = 0; i < w * h; i++) {

        uint32_t inPixel = inPixels[i];
        uint32_t outPixel = 0;

        outPixel |= (MASK >> 8    & inPixel << 16);
        outPixel |= (MASK >> 16   & inPixel << 8);
        outPixel |= (MASK >> 24   & inPixel << 0);
        outPixel |= (MASK - (MASK & inPixel << 24));

        sink[i] = outPixel;       
    }
}


//Output dimensions don't change
void bc2ToRGBA(int w, int h, unsigned char *src, uint32_t *sink) {
    
    thread_local static uint32_t *blockSink = new uint32_t[16]; 

    //Iterates through each block
    for (int i = 0; i < w * h; i+=16) {
        
        //Decompresses a 4x4 block (16 bytes, although not exactly 1 aligned
        //byte per pixel)
        detexDecompressBlockBC2(src + i, 1, 1, reinterpret_cast<uint8_t *>(blockSink));
        
        int blockNum = i / 16;
        int numBlocksInRow = w / 4;

        int blockX = blockNum % numBlocksInRow;
        int blockY = blockNum / numBlocksInRow;

        int indexX = blockX * 4;
        int indexY = blockY * 4;

        int startIndex = indexY * w + indexX; 

        for (int j = 0; j < 16; j++) {
            int outIndex = startIndex + (j / 4) * w + (j % 4);
            sink[outIndex] = blockSink[j];
        }
    }
}