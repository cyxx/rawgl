/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */

#include "scaler.h"


static void point1x(uint8 *dst, uint16 dstPitch, const uint8 *src, uint16 srcPitch, uint16 w, uint16 h) {
	while (h--) {
		memcpy(dst, src, w);
		dst += dstPitch;
		src += srcPitch;
	}
}

static void scale2x(uint8 *dst, uint16 dstPitch, const uint8 *src, uint16 srcPitch, uint16 w, uint16 h) {
	while (h--) {
		uint8 *p = dst;
		for (int i = 0; i < w; ++i, p += 2) {
			uint8 B = *(src + i - srcPitch);
			uint8 D = *(src + i - 1);
			uint8 E = *(src + i);
			uint8 F = *(src + i + 1);
			uint8 H = *(src + i + srcPitch);
			if (B != H && D != F) {
				*(p) = D == B ? D : E;
				*(p + 1) = B == F ? F : E;
				*(p + dstPitch) = D == H ? D : E;
				*(p + dstPitch + 1) = H == F ? F : E;
			} else {
				*(p) = E;
				*(p + 1) = E;
				*(p + dstPitch) = E;
				*(p + dstPitch + 1) = E;
			}
		}
		dst += dstPitch * 2;
		src += srcPitch;
	}
}

static void scale3x(uint8 *dst, uint16 dstPitch, const uint8 *src, uint16 srcPitch, uint16 w, uint16 h) {
	while (h--) {
		uint8 *p = dst;
		for (int i = 0; i < w; ++i, p += 3) {
			uint8 A = *(src + i - srcPitch - 1);
			uint8 B = *(src + i - srcPitch);
			uint8 C = *(src + i - srcPitch + 1);
			uint8 D = *(src + i - 1);
			uint8 E = *(src + i);
			uint8 F = *(src + i + 1);
			uint8 G = *(src + i + srcPitch - 1);
			uint8 H = *(src + i + srcPitch);
			uint8 I = *(src + i + srcPitch + 1);
			if (B != H && D != F) {
				*(p) = D == B ? D : E;
				*(p + 1) = (D == B && E != C) || (B == F && E != A) ? B : E;
				*(p + 2) = B == F ? F : E;
				*(p + dstPitch) = (D == B && E != G) || (D == B && E != A) ? D : E;
				*(p + dstPitch + 1) = E;
				*(p + dstPitch + 2) = (B == F && E != I) || (H == F && E != C) ? F : E;
				*(p + 2 * dstPitch) = D == H ? D : E;
				*(p + 2 * dstPitch + 1) = (D == H && E != I) || (H == F && E != G) ? H : E;
				*(p + 2 * dstPitch + 2) = H == F ? F : E;
			} else {
				*(p) = E;
				*(p + 1) = E;
				*(p + 2) = E;
				*(p + dstPitch) = E;
				*(p + dstPitch + 1) = E;
				*(p + dstPitch + 2) = E;
				*(p + 2 * dstPitch) = E;
				*(p + 2 * dstPitch + 1) = E;
				*(p + 2 * dstPitch + 2) = E;
			}
		}
		dst += dstPitch * 3;
		src += srcPitch;
	}
}

static void nearest(uint8 *dst, uint16 dstPitch, uint16 dstw, uint16 dsth, const uint8 *src, uint16 srcPitch, uint16 srcw, uint16 srch) {
	for (int j = 0; j < dsth; ++j) {
		int ys = j * srch / dsth;
		for (int i = 0; i < dstw; ++i) {
			int xs = i * srcw / dstw;
			*(dst + i) = *(src + ys * srcPitch + xs);
		}
		dst += dstPitch;
	}
}

void bitmap_rescale(uint8 *dst, uint16 dstPitch, uint16 dstw, uint16 dsth, const uint8 *src, uint16 srcPitch, uint16 srcw, uint16 srch) {
	if (dstw == srcw && dsth == srch) {
		point1x(dst, dstPitch, src, srcPitch, srcw, srch);
	} else if (dstw == srcw * 2 && dsth == srch * 2) {
		scale2x(dst, dstPitch, src, srcPitch, srcw, srch);
	} else if (dstw == srcw * 3 && dsth == srch * 3) {
		scale3x(dst, dstPitch, src, srcPitch, srcw, srch);
	} else {
		nearest(dst, dstPitch, dstw, dsth, src, srcPitch, srcw, srch);
	}
}
