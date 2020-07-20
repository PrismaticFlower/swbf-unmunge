#include "image_converters_unix.h"

#define DETEX_VERSION 1
#include "detex/detex.h"

#include <math.h>



#define BMASK_5BIT 0x001f
#define GMASK_6BIT 0x07e0
#define RMASK_5BIT 0xf800
#define MASK 0xff000000
#define MASK16 0xffff0000


void r5g6b5ToRGBA(int w, int h, unsigned char *src, uint32_t *sink) {

	uint16_t *inPixels = reinterpret_cast<uint16_t *>(src);

    for(int i = 0; i < w * h; i++) {

        uint32_t outPixel = 0;
    	uint16_t inPixel = inPixels[i];

        uint32_t b8 = ( inPixel & BMASK_5BIT)        << 3;
        uint32_t g8 = ((inPixel & GMASK_6BIT) >> 5)  << 2;
        uint32_t r8 = ((inPixel & RMASK_5BIT) >> 11) << 3;

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

void bc2ToRGBA(int w, int h, unsigned char *src, uint32_t *sink) {
    
    thread_local static uint32_t *blockSink = new uint32_t[16]; 

    //Iterate through each block
    for (int i = 0; i < w * h; i+=16) {
        
        //Decompresses a 4x4 block (16 bytes, but pixels are not byte-aligned)
        detexDecompressBlockBC2(src + i, 1, 1, reinterpret_cast<uint8_t *>(blockSink));
        
        int blockNum = i / 16;
        int numBlocksInRow = w / 4;

        int x = 4 * (blockNum % numBlocksInRow);
        int y = 4 * (blockNum / numBlocksInRow);

        int startIndex = y * w + x; 

        for (int j = 0; j < 16; j++) {
            int outIndex = startIndex + (j / 4) * w + (j % 4);
            sink[outIndex] = blockSink[j];
        }
    }
}


void lumToRGBA(int w, int h, uint8_t *src, uint32_t *sink, const D3DFORMAT format) {

    int stepSize = (format == D3DFMT_L16 || format == D3DFMT_A8L8) ? 2 : 1;

    for (int i = 0; i < w * h * stepSize; i+=stepSize) {
          
        uint8_t *curAddr = src + i;
        uint32_t l = 0, a = 0xff, pixel = 0;

        switch (format){
            case D3DFMT_L8:
                l |= *curAddr & MASK;
                break;
            case D3DFMT_L16:
                l |= (*(uint16_t *) curAddr) >> 8; 
                break;
            case D3DFMT_A8L8:
                l |= (*(uint16_t *) curAddr & 0xff);
                a |= (*(uint16_t *) curAddr & 0xff00) >> 8;
                break;
            default: //A4L4
                l |= (*curAddr & 0xf) << 4;
                a |= (*curAddr & 0xf0);
                break;
        }

        pixel = l;
        pixel |= (l << 8);
        pixel |= (l << 16);
        pixel |= (a << 24);

        sink[i / stepSize] = pixel;
    }
}
