
#include "wave.h"

static void TO_LE16(uint8_t *dst, uint16_t value) {
	for (int i = 0; i < 2; ++i) {
		dst[i] = (value >> (8 * i)) & 255;
	}
}

static void TO_LE32(uint8_t *dst, uint32_t value) {
	for (int i = 0; i < 4; ++i) {
		dst[i] = (value >> (8 * i)) & 255;
	}
}

static void TO_BE32(uint8_t *dst, uint32_t value) {
	for (int i = 0; i < 4; ++i) {
		dst[i] = (value >> (8 * (3 - i))) & 255;
	}
}

int writeWav_stereoS16(FILE *fp, const int16_t *samples, int count, int frequency) {
	static const int FMT_CHUNK_SIZE = 16;
	int bufferSize = 4 + (8 + FMT_CHUNK_SIZE) + (8 + count * sizeof(int16_t));
	uint8_t *buffer = (uint8_t *)calloc(8 + bufferSize, 1);
	if (buffer) {
		int offset = 0;

		TO_BE32(buffer + offset, 0x52494646); offset += 4; // 'RIFF'
		TO_LE32(buffer + offset, bufferSize); offset += 4;

		TO_BE32(buffer + offset, 0x57415645); offset += 4; // 'WAVE'
		TO_BE32(buffer + offset, 0x666d7420); offset += 4; // 'fmt '
		TO_LE32(buffer + offset, FMT_CHUNK_SIZE); offset += 4;
		TO_LE16(buffer + offset, 1); offset += 2; // audio_format
		TO_LE16(buffer + offset, 2); offset += 2; // num_channels
		TO_LE32(buffer + offset, frequency); offset += 4; // sample_rate
		TO_LE32(buffer + offset, frequency * 2 * sizeof(int16_t)); offset += 4; // byte_rate
		TO_LE16(buffer + offset, 2 * sizeof(int16_t)); offset += 2; // block_align
		TO_LE16(buffer + offset, 16); offset += 2; // bits_per_sample

		TO_BE32(buffer + offset, 0x64617461); offset += 4; // 'data'
		TO_LE32(buffer + offset, count * sizeof(int16_t)); offset += 4;
		memcpy(buffer + offset, samples, count * sizeof(int16_t));

		fwrite(buffer, 1, 8 + bufferSize, fp);

		free(buffer);
	}
	return 0;
}
