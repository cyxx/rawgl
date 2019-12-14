
#include <stdio.h>
#include "bitmap.h"

static const uint16_t TAG_BM = 0x4D42;

static void fwriteByte(FILE *fp, uint8_t n) {
	fputc(n, fp);
}

static void fwriteUint16LE(FILE *fp, uint16_t n) {
	fwriteByte(fp, n & 0xFF);
	fwriteByte(fp, n >> 8);
}

static void fwriteUint32LE(FILE *fp, uint32_t n) {
	fwriteUint16LE(fp, n & 0xFFFF);
	fwriteUint16LE(fp, n >> 16);
}

void WriteFile_BMP_PAL(const char *filename, int w, int h, int pitch, const uint8_t *bits, const uint8_t *pal) {
	FILE *fp = fopen(filename, "wb");
	if (fp) {
		int alignWidth = (w + 3) & ~3;
		int imageSize = alignWidth * h;

		// Write file header
		fwriteUint16LE(fp, TAG_BM);
		fwriteUint32LE(fp, 14 + 40 + 4 * 256 + imageSize);
		fwriteUint16LE(fp, 0); // reserved1
		fwriteUint16LE(fp, 0); // reserved2
		fwriteUint32LE(fp, 14 + 40 + 4 * 256);

		// Write info header
		fwriteUint32LE(fp, 40);
		fwriteUint32LE(fp, w);
		fwriteUint32LE(fp, h);
		fwriteUint16LE(fp, 1); // planes
		fwriteUint16LE(fp, 8); // bit_count
		fwriteUint32LE(fp, 0); // compression
		fwriteUint32LE(fp, imageSize); // size_image
		fwriteUint32LE(fp, 0); // x_pels_per_meter
		fwriteUint32LE(fp, 0); // y_pels_per_meter
		fwriteUint32LE(fp, 0); // num_colors_used
		fwriteUint32LE(fp, 0); // num_colors_important

		// Write palette data
		for (int i = 0; i < 256; ++i) {
			fwriteByte(fp, pal[2]);
			fwriteByte(fp, pal[1]);
			fwriteByte(fp, pal[0]);
			fwriteByte(fp, 0);
			pal += 3;
		}

		// Write bitmap data
		bits += h * pitch;
		for (int i = 0; i < h; ++i) {
			bits -= pitch;
			fwrite(bits, w, 1, fp);
			int pad = alignWidth - w;
			while (pad--) {
				fwriteByte(fp, 0);
			}
		}
		
		fclose(fp);
	}
}
