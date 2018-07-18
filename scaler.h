
#ifndef SCALER_H__
#define SCALER_H__

#include "intern.h"

typedef void (*ScaleProc)(int factor, int byteDepth, uint8_t *dst, int dstPitch, const uint8_t *src, int srcPitch, int w, int h);

#define SCALER_TAG 1

struct Scaler {
	uint32_t tag;
	const char *name;
	int factorMin, factorMax;
	int bpp;
	ScaleProc scale;
};

const Scaler *findScaler(const char *name);

#endif // SCALER_H__
