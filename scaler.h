
#ifndef SCALER_H__
#define SCALER_H__

#include "intern.h"

void point1x(uint8_t *dst, int dstPitch, const uint8_t *src, int srcPitch, int w, int h);
void point2x(uint8_t *dst, int dstPitch, const uint8_t *src, int srcPitch, int w, int h);
void point3x(uint8_t *dst, int dstPitch, const uint8_t *src, int srcPitch, int w, int h);
void scale2x(uint8_t *dst, int dstPitch, const uint8_t *src, int srcPitch, int w, int h);
void scale3x(uint8_t *dst, int dstPitch, const uint8_t *src, int srcPitch, int w, int h);
void nearest(uint8_t *dst, int dstPitch, int dstw, int dsth, const uint8_t *src, int srcPitch, int srcw, int srch);

#endif // SCALER_H__
