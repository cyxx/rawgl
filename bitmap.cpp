
#include "bitmap.h"
#include "util.h"

static void clut(const uint8_t *src, const uint8_t *pal, int pitch, int w, int h, int bpp, bool flipY, int colorKey, uint8_t *dst) {
	int dstPitch = bpp * w;
	if (flipY) {
		dst += (h - 1) * bpp * w;
		dstPitch = -bpp * w;
	}
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			const int color = src[x];
			const int b = pal[color * 4];
			const int g = pal[color * 4 + 1];
			const int r = pal[color * 4 + 2];
			dst[x * bpp]     = r;
			dst[x * bpp + 1] = g;
			dst[x * bpp + 2] = b;
			if (bpp == 4) {
				dst[x * bpp + 3] = (color == 0 || (colorKey == ((r << 16) | (g << 8) | b))) ? 0 : 255;
			}
		}
		src += w;
		dst += dstPitch;
	}
}

uint8_t *decode_bitmap(const uint8_t *src, bool alpha, int colorKey, int *w, int *h) {
	if (memcmp(src, "BM", 2) != 0) {
		return 0;
	}
	const uint32_t imageOffset = READ_LE_UINT32(src + 0xA);
	const int width = READ_LE_UINT32(src + 0x12);
	const int height = READ_LE_UINT32(src + 0x16);
	const int depth = READ_LE_UINT16(src + 0x1C);
	const int compression = READ_LE_UINT32(src + 0x1E);
	if (depth != 8 || compression != 0) {
		warning("Unhandled bitmap depth %d compression %d", depth, compression);
		return 0;
	}
	const int bpp = (!alpha && colorKey < 0) ? 3 : 4;
	uint8_t *dst = (uint8_t *)malloc(width * height * bpp);
	if (!dst) {
		warning("Failed to allocate bitmap buffer, width %d height %d bpp %d", width, height, bpp);
		return 0;
	}
	const uint8_t *palette = src + 14 /* BITMAPFILEHEADER */ + 40 /* BITMAPINFOHEADER */;
	const bool flipY = true;
	clut(src + imageOffset, palette, (width + 3) & ~3, width, height, bpp, flipY, colorKey, dst);
	*w = width;
	*h = height;
	return dst;
}
