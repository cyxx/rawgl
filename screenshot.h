
#ifndef SCREENSHOT_H__
#define SCREENSHOT_H__

#include <stdint.h>

void saveTGA(const char *filename, const uint16_t *rgb, int w, int h);
void saveBMP(const char *filename, const uint8_t *bits, const uint8_t *pal, int w, int h);

#endif
