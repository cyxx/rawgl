
#include "scaler.h"

void point1x(uint8_t *dst, int dstPitch, const uint8_t *src, int srcPitch, int w, int h) {
	while (h--) {
		memcpy(dst, src, w * 2);
		dst += dstPitch;
		src += srcPitch;
	}
}

void point2x(uint8_t *dst, int dstPitch, const uint8_t *src, int srcPitch, int w, int h) {
	while (h--) {
		int i;
		uint8_t *p = dst;
		for (i = 0; i < w; ++i, p += 2) {
			uint8_t c = *(src + i);
			*(p) = c;
			*(p + 1) = c;
			*(p + dstPitch) = c;
			*(p + dstPitch + 1) = c;
		}
		dst += dstPitch * 2;
		src += srcPitch;
	}
}

void point3x(uint8_t *dst, int dstPitch, const uint8_t *src, int srcPitch, int w, int h) {
	while (h--) {
		int i;
		uint8_t *p = dst;
		for (i = 0; i < w; ++i, p += 3) {
			uint8_t c = *(src + i);
			*(p) = c;
			*(p + 1) = c;
			*(p + 2) = c;
			*(p + dstPitch) = c;
			*(p + dstPitch + 1) = c;
			*(p + dstPitch + 2) = c;
			*(p + 2 * dstPitch) = c;
			*(p + 2 * dstPitch + 1) = c;
			*(p + 2 * dstPitch + 2) = c;
		}
		dst += dstPitch * 3;
		src += srcPitch;
	}
}

void scale2x(uint8_t *dst, int dstPitch, const uint8_t *src, int srcPitch, int w, int h) {
	for (int y = 0; y < h; ++y) {
		uint8_t *p = dst;
		for (int x = 0; x < w; ++x, p += 2) {
			const uint8_t E = *(src + x);
			uint8_t B = (y == 0) ? E : *(src + x - srcPitch);
			uint8_t D = (x == 0) ? E : *(src + x - 1);
			uint8_t F = (x == w - 1) ? E : *(src + x + 1);
			uint8_t H = (y == h - 1) ? E : *(src + x + srcPitch);
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

void scale3x(uint8_t *dst, int dstPitch, const uint8_t *src, int srcPitch, int w, int h) {
	for (int y = 0; y < h; ++y) {
		uint8_t *p = dst;
		for (int x = 0; x < w; ++x, p += 3) {
			const uint8_t E = *(src + x);
			uint8_t B = (y == 0) ? E : *(src + x - srcPitch);
			uint8_t D = (x == 0) ? E : *(src + x - 1);
			uint8_t F = (x == w - 1) ? E : *(src + x + 1);
			uint8_t H = (y == h - 1) ? E : *(src + x + srcPitch);
			uint8_t A, C;
			if (y == 0) {
				A = D;
				C = F;
			} else {
				A = (x == 0) ? B : *(src + x - srcPitch - 1);
				C = (x == w - 1) ? B : *(src + x - srcPitch + 1);
			}
			uint8_t G, I;
			if (y == h - 1) {
				G = D;
				I = F;
			} else {
				G = (x == 0) ? H : *(src + x + srcPitch - 1);
				I = (x == w - 1) ? H : *(src + x + srcPitch + 1);
			}
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

void nearest(uint8_t *dst, int dstPitch, int dstw, int dsth, const uint8_t *src, int srcPitch, int srcw, int srch) {
	for (int j = 0; j < dsth; ++j) {
		int ys = j * srch / dsth;
		for (int i = 0; i < dstw; ++i) {
			int xs = i * srcw / dstw;
			*(dst + i) = *(src + ys * srcPitch + xs);
		}
		dst += dstPitch;
	}
}
