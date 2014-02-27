
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

#include "sys.h"
#include "util.h"

#define MAX(x,y) ((x)>(y)?(x):(y))
#define MIN(x,y) ((x)<(y)?(x):(y))
#define ARRAYSIZE(a) (sizeof(a)/sizeof(a[0]))

template<typename T>
inline void SWAP(T &a, T &b) {
	T tmp = a; 
	a = b;
	b = tmp;
}

struct Ptr {
	uint8_t *pc;
	
	uint8_t fetchByte() {
		return *pc++;
	}
	
	uint16_t fetchWord() {
		uint16_t i = READ_BE_UINT16(pc);
		pc += 2;
		return i;
	}
};

struct Point {
	int16_t x, y;

	Point() : x(0), y(0) {}
	Point(int16_t xx, int16_t yy) : x(xx), y(yy) {}
	Point(const Point &p) : x(p.x), y(p.y) {}
};

struct QuadStrip {
	enum {
		MAX_VERTICES = 50
	};
	
	uint8_t numVertices;
	Point vertices[MAX_VERTICES];
};

struct Color {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

#endif
