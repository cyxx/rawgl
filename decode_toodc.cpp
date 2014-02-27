
#include "intern.h"

// static uint32_t XOR_KEY1 = 0x31111612;
static uint32_t XOR_KEY2 = 0x22683297;

uint8_t *decode_toodc(uint8_t *p, int count) {
	uint32_t key = XOR_KEY2;
	uint32_t acc = 0;
	for (int i = 0; i < count; ++i) {
		uint8_t *q = p + i * 4;
		const uint32_t data = READ_LE_UINT32(q) ^ key;
		uint32_t r = (q[2] + q[1] + q[0]) ^ q[3];
		r += acc;
		key += r;
		acc += 0x4D;
		WRITE_LE_UINT32(q, data);
	}
	return p + 4;
}

