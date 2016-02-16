/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */

#ifndef __SCALER_H__
#define __SCALER_H__

#include "intern.h"

void bitmap_rescale(uint8 *dst, uint16 dstPitch, uint16 dstw, uint16 dsth, const uint8 *src, uint16 srcPitch, uint16 srcw, uint16 srch);

#endif
