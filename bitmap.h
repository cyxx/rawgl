
#ifndef BITMAP_H__
#define BITMAP_H__

#include "intern.h"

uint8_t *decode_bitmap(const uint8_t *src, bool alpha, bool flipY, int colorKey, int *w, int *h);

#endif
