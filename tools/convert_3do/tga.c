
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "tga.h"

#define kTgaImageTypeUncompressedTrueColorImage 2
#define kTgaDirectionRight (1 << 4)
#define kTgaDirectionTop (1 << 5)

struct TgaFile {
	FILE *fp;
	int width, height, bitdepth;
	const unsigned char *colorLookupTable;
};

int fileWriteUint16LE(FILE *fp, int n) {
        fputc(n & 255, fp);
        fputc((n >> 8) & 255, fp);
}

static void tgaWriteHeader(struct TgaFile *t) {
	fputc(0, t->fp); // 0, ID Length
	fputc(0, t->fp); // 1, ColorMap Type
	fputc(kTgaImageTypeUncompressedTrueColorImage, t->fp); // 2, Image Type
	fileWriteUint16LE(t->fp, 0); // 3, colourmapstart
	fileWriteUint16LE(t->fp, 0); // 5, colourmaplength
	fputc(0, t->fp); // 7, colourmapbits
	fileWriteUint16LE(t->fp, 0); // 8, xstart
	fileWriteUint16LE(t->fp, 0); // 10, ystart
	fileWriteUint16LE(t->fp, t->width); // 12
	fileWriteUint16LE(t->fp, t->height); // 14
	fputc(t->bitdepth, t->fp); // 16
	fputc(kTgaDirectionTop, t->fp); // 17, descriptor
}

static void tgaWriteFooter(struct TgaFile *t) {
	char hdr[26];

	memset(hdr, 0, 8); // developer, extension offsets 
	strcpy(hdr + 8, "TRUEVISION-XFILE.");
	fwrite(hdr, 1, sizeof(hdr), t->fp);
}

struct TgaFile *tgaOpen(const char *filePath, int width, int height, int depth) {
	struct TgaFile *t;

	t = malloc(sizeof(struct TgaFile));
	assert(t);
	t->fp = fopen(filePath, "wb");
	assert(t->fp);
	t->width = width;
	t->height = height;
	t->bitdepth = depth;
	t->colorLookupTable = 0;
	return t;
}

void tgaClose(struct TgaFile *t) {
	if (t->fp) {
		fclose(t->fp);
		t->fp = NULL;
	}
	free(t);
}

void tgaSetLookupColorTable(struct TgaFile *t, const unsigned char *paletteData) {
	t->colorLookupTable = paletteData;
}

void tgaWritePixelsData(struct TgaFile *t, const void *pixelsData, int pixelsDataSize) {
	int i, color, rgb;

	tgaWriteHeader(t);
	if (t->colorLookupTable) {
		assert(pixelsDataSize == t->height * t->width);
		for (i = 0; i < pixelsDataSize; ++i) {
			color = ((const unsigned char *)pixelsData)[i];
			for (rgb = 0; rgb < 3; ++rgb) {
				static const int p[] = { 2, 1, 0 };
				const uint8_t c = t->colorLookupTable[color * 3 + p[rgb]];
				fputc((c << 2) | (c & 3), t->fp);
			}
		}
	} else {
		assert(pixelsDataSize == t->height * t->width * t->bitdepth / 8);
		fwrite(pixelsData, 1, pixelsDataSize, t->fp);
	}
//	tgaWriteFooter(t);
}

