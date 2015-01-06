
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "unpack.h"

struct UnpackCtx {
	int size, datasize;
	uint32_t crc;
	uint32_t bits;
	uint8_t *dst;
	const uint8_t *src;
};

static int shiftBit(UnpackCtx *uc, int CF) {
	int rCF = (uc->bits & 1);
	uc->bits >>= 1;
	if (CF) {
		uc->bits |= 0x80000000;
	}
	return rCF;
}

static int nextBit(UnpackCtx *uc) {
	int CF = shiftBit(uc, 0);
	if (uc->bits == 0) {
		uc->bits = READ_BE_UINT32(uc->src); uc->src -= 4;
		uc->crc ^= uc->bits;
		CF = shiftBit(uc, 1);
	}
	return CF;
}

static uint16_t getBits(UnpackCtx *uc, uint8_t num_bits) {
	uint16_t c = 0;
	while (num_bits--) {
		c <<= 1;
		if (nextBit(uc)) {
			c |= 1;
		}
	}
	return c;
}

static void unpackHelper1(UnpackCtx *uc, uint8_t num_bits, uint8_t add_count) {
	uint16_t count = getBits(uc, num_bits) + add_count + 1;
	uc->datasize -= count;
	while (count--) {
		*uc->dst = (uint8_t)getBits(uc, 8);
		--uc->dst;
	}
}

static void unpackHelper2(UnpackCtx *uc, uint8_t num_bits) {
	uint16_t i = getBits(uc, num_bits);
	uint16_t count = uc->size + 1;
	uc->datasize -= count;
	while (count--) {
		*uc->dst = *(uc->dst + i);
		--uc->dst;
	}
}

bool delphine_unpack(uint8_t *dst, const uint8_t *src, int len) {
	UnpackCtx uc;
	uc.src = src + len - 4;
	uc.datasize = READ_BE_UINT32(uc.src); uc.src -= 4;
	uc.dst = dst + uc.datasize - 1;
	uc.size = 0;
	uc.crc = READ_BE_UINT32(uc.src); uc.src -= 4;
	uc.bits = READ_BE_UINT32(uc.src); uc.src -= 4;
	uc.crc ^= uc.bits;
	do {
		if (!nextBit(&uc)) {
			uc.size = 1;
			if (!nextBit(&uc)) {
				unpackHelper1(&uc, 3, 0);
			} else {
				unpackHelper2(&uc, 8);
			}
		} else {
			uint16_t c = getBits(&uc, 2);
			if (c == 3) {
				unpackHelper1(&uc, 8, 8);
			} else if (c < 2) {
				uc.size = c + 2;
				unpackHelper2(&uc, c + 9);
			} else {
				uc.size = getBits(&uc, 8);
				unpackHelper2(&uc, 12);
			}
		}
	} while (uc.datasize > 0);
	return uc.crc == 0;
}
