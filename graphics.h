
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef GRAPHICS_H__
#define GRAPHICS_H__

#include "intern.h"

struct Graphics {
	typedef void (Graphics::*drawLine)(int16_t x1, int16_t x2, int16_t y, uint8_t col);

	enum {
		GFX_W = 320,
		GFX_H = 200,
	};

	static const uint8_t _font[];

	uint8_t *_pagePtrs[4];
	uint8_t *_workPagePtr;

	Graphics();
	~Graphics();

	void drawPolygon(uint8_t color, const QuadStrip &qs);
	void drawChar(uint8_t c, uint16_t x, uint16_t y, uint8_t color);
	void drawPoint(int16_t x, int16_t y, uint8_t color);
	void drawLineT(int16_t x1, int16_t x2, int16_t y, uint8_t color);
	void drawLineN(int16_t x1, int16_t x2, int16_t y, uint8_t color);
	void drawLineP(int16_t x1, int16_t x2, int16_t y, uint8_t color);
	uint8_t *getPagePtr(uint8_t page);
	void setWorkPagePtr(uint8_t page);
};

#endif
