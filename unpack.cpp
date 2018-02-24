
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "unpack.h"

struct UnpackCtx {
	int datasize;
	uint32_t crc;
	uint32_t bits;
	uint8_t *dst;
	const uint8_t *src;
};

static bool nextBit(UnpackCtx *uc) {
	bool carry = (uc->bits & 1) != 0;
	uc->bits >>= 1;
	if (uc->bits == 0) {
		uc->bits = READ_BE_UINT32(uc->src); uc->src -= 4;
		uc->crc ^= uc->bits;
		carry = (uc->bits & 1) != 0;
		uc->bits = (1 << 31) | (uc->bits >> 1);
	}
	return carry;
}

static uint16_t getBits(UnpackCtx *uc, int bitsCount) {
	uint16_t c = 0;
	for (int i = 0; i < bitsCount; ++i) {
		c <<= 1;
		if (nextBit(uc)) {
			c |= 1;
		}
	}
	return c;
}

static void copyLiteral(UnpackCtx *uc, int bitsCount, int len) {
	const int count = getBits(uc, bitsCount) + len + 1;
	for (int i = 0; i < count; ++i) {
		*uc->dst = (uint8_t)getBits(uc, 8);
		--uc->dst;
	}
	uc->datasize -= count;
}

static void copyReference(UnpackCtx *uc, int bitsCount, int count) {
	const uint16_t offset = getBits(uc, bitsCount);
	for (int i = 0; i < count; ++i) {
		*uc->dst = *(uc->dst + offset);
		--uc->dst;
	}
	uc->datasize -= count;
}

bool delphine_unpack(uint8_t *dst, const uint8_t *src, int len) {
	UnpackCtx uc;
	uc.src = src + len - 4;
	uc.datasize = READ_BE_UINT32(uc.src); uc.src -= 4;
	uc.dst = dst + uc.datasize - 1;
	uc.crc = READ_BE_UINT32(uc.src); uc.src -= 4;
	uc.bits = READ_BE_UINT32(uc.src); uc.src -= 4;
	uc.crc ^= uc.bits;
	do {
		if (!nextBit(&uc)) {
			if (!nextBit(&uc)) {
				copyLiteral(&uc, 3, 0);
			} else {
				copyReference(&uc, 8, 2);
			}
		} else {
			const int code = getBits(&uc, 2);
			switch (code) {
			case 3:
				copyLiteral(&uc, 8, 8);
				break;
			case 2:
				copyReference(&uc, 12, getBits(&uc, 8) + 1);
				break;
			case 1:
				copyReference(&uc, 10, 4);
				break;
			case 0:
				copyReference(&uc, 9, 3);
				break;
			}
		}
	} while (uc.datasize > 0);
	return uc.crc == 0;
}
