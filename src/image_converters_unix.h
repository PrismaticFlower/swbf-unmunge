#pragma once

/*
Format conversion functions.  Kept things relatively low level
since most libraries (glaring at you GLI) that claimed to handle
this exact task kept pretty dismal or no documentation... 
*/
#include "D3D9TypesMinimal.h"
#include <stdint.h>

void r5g6b5ToRGBA(int w, int h, uint8_t *src, uint32_t *sink);
void a8r8g8b8ToRBGA(int w, int h, uint8_t *src, uint32_t *sink);
void bc2ToRGBA(int w, int h, uint8_t *src, uint32_t *sink);
void lumToRGBA(int w, int h, uint8_t *src, uint32_t *sink, const D3DFORMAT format);