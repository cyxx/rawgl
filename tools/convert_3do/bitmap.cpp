
#include "bitmap.h"

static void TO_LE16(uint8_t *dst, uint16_t value) {
	for (int i = 0; i < 2; ++i) {
		dst[i] = value & 255;
		value >>= 8;
	}
}

static void TO_LE32(uint8_t *dst, uint32_t value) {
	for (int i = 0; i < 4; ++i) {
		dst[i] = value & 255;
		value >>= 8;
	}
}

static uint32_t TO_ARGB(const uint16_t rgb555) {
	const int r = ((rgb555 >> 10) & 31) << 3;
	const int g = ((rgb555 >>  5) & 31) << 3;
	const int b = ( rgb555        & 31) << 3;
	return 0xFF000000 | (r << 16) | (g << 8) | b;
}

void writeBitmap555(FILE *fp, const uint16_t *src, int w, int h) {
	static const int FILE_HEADER_SIZE = 14;
	static const int INFO_HEADER_SIZE = 40;
	const int alignedWidth = w * sizeof(uint32_t);
	const int imageSize = alignedWidth * h;
	const int bufferSize = FILE_HEADER_SIZE + INFO_HEADER_SIZE + imageSize;
	uint8_t *buffer = (uint8_t *)calloc(bufferSize, 1);
	if (buffer) {
		int offset = 0;

		// file header
		TO_LE16(buffer + offset, 0x4D42); offset += 2;
		TO_LE32(buffer + offset, bufferSize); offset += 4;
		TO_LE16(buffer + offset, 0); offset += 2; // reserved1
		TO_LE16(buffer + offset, 0); offset += 2; // reserved2
		TO_LE32(buffer + offset, FILE_HEADER_SIZE + INFO_HEADER_SIZE); offset += 4;

		// info header
		TO_LE32(buffer + offset, INFO_HEADER_SIZE); offset += 4;
		TO_LE32(buffer + offset, w);  offset += 4;
		TO_LE32(buffer + offset, h);  offset += 4;
		TO_LE16(buffer + offset, 1);  offset += 2; // planes
		TO_LE16(buffer + offset, 32); offset += 2; // bit_count
		TO_LE32(buffer + offset, 0);  offset += 4; // compression
		TO_LE32(buffer + offset, imageSize); offset += 4; // size_image
		TO_LE32(buffer + offset, 0);  offset += 4; // x_pels_per_meter
		TO_LE32(buffer + offset, 0);  offset += 4; // y_pels_per_meter
		TO_LE32(buffer + offset, 0);  offset += 4; // num_colors_used
		TO_LE32(buffer + offset, 0);  offset += 4; // num_colors_important

		// bitmap data
		for (int y = 0; y < h; ++y) {
			const uint16_t *p = src + (h - 1 - y) * w;
			for (int x = 0; x < w; ++x) {
				TO_LE32(buffer + offset + x * 4, TO_ARGB(p[x]));
			}
			offset += alignedWidth;
		}
	}
	const int count = fwrite(buffer, 1, bufferSize, fp);
	if (count != bufferSize) {
		fprintf(stderr, "Failed to write %d bytes, ret %d\n", bufferSize, count);
	}
	free(buffer);
}
