
#ifndef BITMAP_H__
#define BITMAP_H__

#include <stdint.h>

void WriteFile_BMP_PAL(const char *filename, int w, int h, int pitch, const uint8_t *bits, const uint8_t *pal);

#endif // BITMAP_H__
