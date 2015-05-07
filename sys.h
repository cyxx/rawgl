
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef SYS_H__
#define SYS_H__

#include <stdint.h>

inline uint16_t SWAP_UINT16(uint16_t n) {
	return ((n >> 8) & 255) | ((n & 255) << 8);
}

inline uint16_t READ_BE_UINT16(const void *ptr) {
	const uint8_t *b = (const uint8_t *)ptr;
	return (b[0] << 8) | b[1];
}

inline uint16_t READ_LE_UINT16(const uint8_t *ptr) {
	return (ptr[1] << 8) | ptr[0];
}

inline uint32_t READ_BE_UINT32(const void *ptr) {
	const uint8_t *b = (const uint8_t *)ptr;
	return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}

inline uint32_t READ_LE_UINT32(const uint8_t *ptr) {
	return (ptr[3] << 24) | (ptr[2] << 16) | (ptr[1] << 8) | ptr[0];
}

inline void WRITE_LE_UINT32(uint8_t *ptr, uint32_t value) {
	for (int i = 0; i < 4; ++i) {
		ptr[i] = value & 255; value >>= 8;
	}
}

#endif
