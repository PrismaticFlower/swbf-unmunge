/*
Naive image conversion methods.
*/

#include <stdint.h>

void r5g6b5ToRGBA(int height, int width, unsigned char *src, uint32_t *sink);
void a8r8g8b8ToRBGA(int height, int width, unsigned char *src, uint32_t *sink);