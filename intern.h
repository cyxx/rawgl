
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef INTERN_H__
#define INTERN_H__

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <stdint.h>
#include <sys/param.h>

#undef MAX
#define MAX(x,y) ((x)>(y)?(x):(y))
#undef MIN
#define MIN(x,y) ((x)<(y)?(x):(y))
#undef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a)/sizeof(a[0]))

template<typename T>
inline void SWAP(T &a, T &b) {
	T tmp = a; 
	a = b;
	b = tmp;
}

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

enum Language {
	LANG_FR,
	LANG_US,
};

struct Ptr {
	uint8_t *pc;
	bool byteSwap;
	
	uint8_t fetchByte() {
		return *pc++;
	}
	
	uint16_t fetchWord() {
		const uint16_t i = byteSwap ? READ_LE_UINT16(pc) : READ_BE_UINT16(pc);
		pc += 2;
		return i;
	}
};

struct Point {
	int16_t x, y;

	Point() : x(0), y(0) {}
	Point(int16_t xx, int16_t yy) : x(xx), y(yy) {}
	Point(const Point &p) : x(p.x), y(p.y) {}

	void scale(int u, int v) {
		x = (x * u) >> 16;
		y = (y * v) >> 16;
	}
};

struct QuadStrip {
	enum {
		MAX_VERTICES = 70
	};
	
	uint8_t numVertices;
	Point vertices[MAX_VERTICES];
};

struct Color {
	uint8_t r;
	uint8_t g;
	uint8_t b;

	uint16_t rgb565() const {
		return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
	}
};

struct Frac {
	static const int BITS = 16;
	uint32_t inc;
	uint64_t offset;

	int16_t interpolate(int16_t sample1, int16_t sample2) {
		static const int MASK = (1 << Frac::BITS) - 1;
		const int fp = inc & MASK;
		return (sample1 * (MASK - fp) + sample2 * fp) >> Frac::BITS;
	}
};

#endif
