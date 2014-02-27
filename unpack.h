
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef UNPACK_H__
#define UNPACK_H__

#include "intern.h"


struct UnpackCtx {
	int size, datasize;
	uint32_t crc;
	uint32_t chk;
	uint8_t *dst;
	const uint8_t *src;
};

extern bool delphine_unpack(uint8_t *dst, const uint8_t *src, int len);


#endif // UNPACK_H__
