/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */

#ifndef __INTERN_H__
#define __INTERN_H__

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
	uint8 *pc;
	
	uint8 fetchByte() {
		return *pc++;
	}
	
	uint16 fetchWord() {
		uint16 i = READ_BE_UINT16(pc);
		pc += 2;
		return i;
	}
};

struct Point {
	int16 x, y;

	Point() : x(0), y(0) {}
	Point(int16 xx, int16 yy) : x(xx), y(yy) {}
	Point(const Point &p) : x(p.x), y(p.y) {}
};

struct QuadStrip {
	enum {
		MAX_VERTICES = 50
	};
	
	uint8 numVertices;
	Point vertices[MAX_VERTICES];
};

struct Color {
	uint8 r;
	uint8 g;
	uint8 b;
};

#endif
