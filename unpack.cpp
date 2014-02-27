
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "unpack.h"


static int rcr(UnpackCtx *uc, int CF) {
	int rCF = (uc->chk & 1);
	uc->chk >>= 1;
	if (CF) {
		uc->chk |= 0x80000000;
	}
	return rCF;
}

static int next_chunk(UnpackCtx *uc) {
	int CF = rcr(uc, 0);
	if (uc->chk == 0) {
		uc->chk = READ_BE_UINT32(uc->src); uc->src -= 4;
		uc->crc ^= uc->chk;
		CF = rcr(uc, 1);
	}
	return CF;
}

static uint16_t get_code(UnpackCtx *uc, uint8_t num_chunks) {
	uint16_t c = 0;
	while (num_chunks--) {
		c <<= 1;
		if (next_chunk(uc)) {
			c |= 1;
		}
	}
	return c;
}

static void dec_unk1(UnpackCtx *uc, uint8_t num_chunks, uint8_t add_count) {
	uint16_t count = get_code(uc, num_chunks) + add_count + 1;
	uc->datasize -= count;
	while (count--) {
		*uc->dst = (uint8_t)get_code(uc, 8);
		--uc->dst;
	}
}

static void dec_unk2(UnpackCtx *uc, uint8_t num_chunks) {
	uint16_t i = get_code(uc, num_chunks);
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
	uc.chk = READ_BE_UINT32(uc.src); uc.src -= 4;
	uc.crc ^= uc.chk;
	do {
		if (!next_chunk(&uc)) {
			uc.size = 1;
			if (!next_chunk(&uc)) {
				dec_unk1(&uc, 3, 0);
			} else {
				dec_unk2(&uc, 8);
			}
		} else {
			uint16_t c = get_code(&uc, 2);
			if (c == 3) {
				dec_unk1(&uc, 8, 8);
			} else if (c < 2) {
				uc.size = c + 2;
				dec_unk2(&uc, c + 9);
			} else {
				uc.size = get_code(&uc, 8);
				dec_unk2(&uc, 12);
			}
		}
	} while (uc.datasize > 0);
	return uc.crc == 0;
}
