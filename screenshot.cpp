
#include <assert.h>
#include "screenshot.h"
#include "file.h"

static void TO_LE16(uint8_t *dst, uint16_t value) {
	for (int i = 0; i < 2; ++i) {
		dst[i] = value & 255;
		value >>= 8;
	}
}

#define kTgaImageTypeUncompressedTrueColor 2
#define kTgaImageTypeRunLengthEncodedTrueColor 10
#define kTgaDirectionTop (1 << 5)

static const int TGA_HEADER_SIZE = 18;

// 16 bits TGA expects 15 bits color depth
static uint16_t rgb565_to_555(const uint16_t color) {
	const int r = (color >> 11) & 31;
	const int g = (color >>  6) & 31;
	const int b = (color >>  0) & 31;
	return (r << 10) | (g << 5) | b;
}

void saveTGA(const char *filename, const uint16_t *rgb, int w, int h) {

	static const uint8_t kImageType = kTgaImageTypeRunLengthEncodedTrueColor;
	uint8_t buffer[TGA_HEADER_SIZE];
	buffer[0]            = 0; // ID Length
	buffer[1]            = 0; // ColorMap Type
	buffer[2]            = kImageType;
	TO_LE16(buffer +  3,   0); // ColorMap Start
	TO_LE16(buffer +  5,   0); // ColorMap Length
	buffer[7]            = 0;  // ColorMap Bits
	TO_LE16(buffer +  8,   0); // X-origin
	TO_LE16(buffer + 10,   0); // Y-origin
	TO_LE16(buffer + 12,   w); // Image Width
	TO_LE16(buffer + 14,   h); // Image Height
	buffer[16]           = 16; // Pixel Depth
	buffer[17]           = kTgaDirectionTop;  // Descriptor

	File f;
	if (f.openForWriting(filename)) {
		f.write(buffer, sizeof(buffer));
		if (kImageType == kTgaImageTypeUncompressedTrueColor) {
			for (int i = 0; i < w * h; ++i) {
				uint16_t color = rgb565_to_555(*rgb++);
				f.writeByte(color & 255);
				f.writeByte(color >> 8);
			}
		} else {
			assert(kImageType == kTgaImageTypeRunLengthEncodedTrueColor);
			uint16_t prevColor = rgb565_to_555(*rgb++);
			int count = 0;
			for (int i = 1; i < w * h; ++i) {
				uint16_t color = rgb565_to_555(*rgb++);
				if (prevColor == color && count < 127) {
					++count;
					continue;
				}
				f.writeByte(count | 0x80);
				f.writeByte(prevColor & 255);
				f.writeByte(prevColor >> 8);
				count = 0;
				prevColor = color;
			}
			if (count != 0) {
				f.writeByte(count | 0x80);
				f.writeByte(prevColor & 255);
				f.writeByte(prevColor >> 8);
			}
		}
	}
}
