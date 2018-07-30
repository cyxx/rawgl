
#ifndef WAVE_H__
#define WAVE_H__

#include "util.h"

int writeWav_stereoS16(FILE *fp, const int16_t *samples, int count, int frequency);

#endif
