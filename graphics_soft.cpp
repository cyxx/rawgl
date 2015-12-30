
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <math.h>
#include "graphics.h"
#include "util.h"
#include "systemstub.h"


struct GraphicsSoft: Graphics {
	typedef void (GraphicsSoft::*drawLine)(int16_t x1, int16_t x2, int16_t y, uint8_t col);

	enum {
		GFX_W = 320,
		GFX_H = 200,
	};

	uint8_t *_pagePtrs[4];
	uint8_t *_workPagePtr;
	int _u, _v;
	int _w, _h;
	Color _pal[16];
	uint16_t *_colorBuffer;

	GraphicsSoft();
	~GraphicsSoft();

	void setSize(int w, int h);
	void drawPolygon(uint8_t color, const QuadStrip &qs);
	void drawChar(uint8_t c, uint16_t x, uint16_t y, uint8_t color);
	void drawPoint(int16_t x, int16_t y, uint8_t color);
	void drawLineT(int16_t x1, int16_t x2, int16_t y, uint8_t color);
	void drawLineN(int16_t x1, int16_t x2, int16_t y, uint8_t color);
	void drawLineP(int16_t x1, int16_t x2, int16_t y, uint8_t color);
	uint8_t *getPagePtr(uint8_t page);
	int getPageSize() const { return _w * _h; }
	void setWorkPagePtr(uint8_t page);

	virtual void setFont(const uint8_t *src, int w, int h);
	virtual void setPalette(const Color *colors, int count);
	virtual void setSpriteAtlas(const uint8_t *src, int w, int h, int xSize, int ySize);
	virtual void drawSprite(int buffer, int num, const Point *pt);
	virtual void drawBitmap(int buffer, const uint8_t *data, int w, int h, int fmt);
	virtual void drawPoint(int buffer, uint8_t color, const Point *pt);
	virtual void drawQuadStrip(int buffer, uint8_t color, const QuadStrip *qs);
	virtual void drawStringChar(int buffer, uint8_t color, char c, const Point *pt);
	virtual void clearBuffer(int num, uint8_t color);
	virtual void copyBuffer(int dst, int src, int vscroll = 0);
	virtual void drawBuffer(int num, SystemStub *stub);
};


GraphicsSoft::GraphicsSoft() {
	_fixUpPalette = FIXUP_PALETTE_NONE;
	memset(_pagePtrs, 0, sizeof(_pagePtrs));
	_colorBuffer = 0;
	memset(_pal, 0, sizeof(_pal));
	setSize(GFX_W, GFX_H);
}

GraphicsSoft::~GraphicsSoft() {
	for (int i = 0; i < 4; ++i) {
		free(_pagePtrs[i]);
		_pagePtrs[i] = 0;
	}
	free(_colorBuffer);
}

void GraphicsSoft::setSize(int w, int h) {
	_u = (w << 16) / GFX_W;
	_v = (h << 16) / GFX_H;
	_w = w;
	_h = h;
	_colorBuffer = (uint16_t *)realloc(_colorBuffer, _w * _h * sizeof(uint16_t));
	if (!_colorBuffer) {
		error("Unable to allocate color buffer w %d h %d", _w, _h);
	}
	for (int i = 0; i < 4; ++i) {
		_pagePtrs[i] = (uint8_t *)realloc(_pagePtrs[i], _w * _h);
		if (!_pagePtrs[i]) {
			error("Not enough memory to allocate offscreen buffers");
		}
		memset(_pagePtrs[i], 0, _w * _h);
	}
	setWorkPagePtr(2);
}

static uint32_t calcStep(const Point &p1, const Point &p2, uint16_t &dy) {
	dy = p2.y - p1.y;
	uint16_t delta = (dy == 0) ? 1 : dy;
	return ((p2.x - p1.x) << 16) / delta;
}

void GraphicsSoft::drawPolygon(uint8_t color, const QuadStrip &quadStrip) {
	QuadStrip qs = quadStrip;
	if (_w != GFX_W || _h != GFX_H) {	
		for (int i = 0; i < qs.numVertices; ++i) {
			qs.vertices[i].scale(_u, _v);
		}
	}

	int i = 0;
	int j = qs.numVertices - 1;

	int16_t x2 = qs.vertices[i].x;
	int16_t x1 = qs.vertices[j].x;
	int16_t hliney = MIN(qs.vertices[i].y, qs.vertices[j].y);

	++i;
	--j;

	drawLine pdl;
	switch (color) {
	default:
		pdl = &GraphicsSoft::drawLineN;
		break;
	case COL_PAGE:
		pdl = &GraphicsSoft::drawLineP;
		break;
	case COL_ALPHA:
		pdl = &GraphicsSoft::drawLineT;
		break;
	}

	uint32_t cpt1 = x1 << 16;
	uint32_t cpt2 = x2 << 16;

	int numVertices = qs.numVertices;
	while (1) {
		numVertices -= 2;
		if (numVertices == 0) {
			return;
		}
		uint16_t h;
		uint32_t step1 = calcStep(qs.vertices[j + 1], qs.vertices[j], h);
		uint32_t step2 = calcStep(qs.vertices[i - 1], qs.vertices[i], h);
		
		++i;
		--j;

		cpt1 = (cpt1 & 0xFFFF0000) | 0x7FFF;
		cpt2 = (cpt2 & 0xFFFF0000) | 0x8000;

		if (h == 0) {
			cpt1 += step1;
			cpt2 += step2;
		} else {
			while (h--) {
				if (hliney >= 0) {
					x1 = cpt1 >> 16;
					x2 = cpt2 >> 16;
					if (x1 < _w && x2 >= 0) {
						if (x1 < 0) x1 = 0;
						if (x2 >= _w) x2 = _w - 1;
						(this->*pdl)(x1, x2, hliney, color);
					}
				}
				cpt1 += step1;
				cpt2 += step2;
				++hliney;
				if (hliney >= _h) return;
			}
		}
	}
}

void GraphicsSoft::drawChar(uint8_t c, uint16_t x, uint16_t y, uint8_t color) {
	if (x <= GFX_W - 8 && y <= GFX_H - 8) {
		const uint8_t *ft = _font + (c - 0x20) * 8;
		uint8_t *p = _workPagePtr + x + y * _w;
		for (int j = 0; j < 8; ++j) {
			uint8_t ch = *(ft + j);
			for (int i = 0; i < 8; ++i) {
				if (ch & 0x80) {
					p[j * _w + i] = color;
				}
				ch <<= 1;
			}
		}
	}
}

void GraphicsSoft::drawPoint(int16_t x, int16_t y, uint8_t color) {
	const int off = y * _w + x;
	switch (color) {
	case COL_ALPHA:
		_workPagePtr[off] |= 0x8;
		break;
	case COL_PAGE:
		_workPagePtr[off] = *(_pagePtrs[0] + off);
		break;
	default:
		_workPagePtr[off] = color;
		break;
	}
}

void GraphicsSoft::drawLineT(int16_t x1, int16_t x2, int16_t y, uint8_t color) {
	int16_t xmax = MAX(x1, x2);
	int16_t xmin = MIN(x1, x2);
	int w = xmax - xmin + 1;
	uint8_t *p = _workPagePtr + y * _w + xmin;
	while (w--) {
		*p++ |= 0x8;
	}
}

void GraphicsSoft::drawLineN(int16_t x1, int16_t x2, int16_t y, uint8_t color) {
	int16_t xmax = MAX(x1, x2);
	int16_t xmin = MIN(x1, x2);
	const int w = xmax - xmin + 1;
	const int off = y * _w + xmin;
	memset(_workPagePtr + off, color, w);
}

void GraphicsSoft::drawLineP(int16_t x1, int16_t x2, int16_t y, uint8_t color) {
	if (_workPagePtr == _pagePtrs[0]) {
		return;
	}
	int16_t xmax = MAX(x1, x2);
	int16_t xmin = MIN(x1, x2);
	const int w = xmax - xmin + 1;
	const int off = y * _w + xmin;
	memcpy(_workPagePtr + off, _pagePtrs[0] + off, w);
}

uint8_t *GraphicsSoft::getPagePtr(uint8_t page) {
	assert(page >= 0 && page < 4);
	return _pagePtrs[page];
}

void GraphicsSoft::setWorkPagePtr(uint8_t page) {
	_workPagePtr = getPagePtr(page);
}

void GraphicsSoft::setFont(const uint8_t *src, int w, int h) {
	if (_is1991) {
		// no-op for 1991
	}
}

void GraphicsSoft::setPalette(const Color *colors, int count) {
	memcpy(_pal, colors, sizeof(Color) * MIN(count, 16));
}

void GraphicsSoft::setSpriteAtlas(const uint8_t *src, int w, int h, int xSize, int ySize) {
	if (_is1991) {
		// no-op for 1991
	}
}

void GraphicsSoft::drawSprite(int buffer, int num, const Point *pt) {
	if (_is1991) {
		// no-op for 1991
	}
}

void GraphicsSoft::drawBitmap(int buffer, const uint8_t *data, int w, int h, int fmt) {
	if (_is1991) {
		if (fmt == FMT_CLUT && _w == w && _h == h) {
			memcpy(getPagePtr(buffer), data, w * h);
		}
	}
}

void GraphicsSoft::drawPoint(int buffer, uint8_t color, const Point *pt) {
	setWorkPagePtr(buffer);
	drawPoint(pt->x, pt->y, color);
}

void GraphicsSoft::drawQuadStrip(int buffer, uint8_t color, const QuadStrip *qs) {
	setWorkPagePtr(buffer);
	drawPolygon(color, *qs);
}

void GraphicsSoft::drawStringChar(int buffer, uint8_t color, char c, const Point *pt) {
	setWorkPagePtr(buffer);
	drawChar(c, pt->x, pt->y, color);
}

void GraphicsSoft::clearBuffer(int num, uint8_t color) {
	memset(getPagePtr(num), color, getPageSize());
}

void GraphicsSoft::copyBuffer(int dst, int src, int vscroll) {
	if (vscroll == 0) {
		memcpy(getPagePtr(dst), getPagePtr(src), getPageSize());
	} else if (vscroll >= -199 && vscroll <= 199) {
		const int dy = (int)round(vscroll * _h / float(GFX_H));
		if (dy < 0) {
			memcpy(getPagePtr(dst), getPagePtr(src) - dy * _w, (_h + dy) * _w);
		} else {
			memcpy(getPagePtr(dst) + dy * _w, getPagePtr(src), (_h - dy) * _w);
		}
	}
}

void GraphicsSoft::drawBuffer(int num, SystemStub *stub) {
	const uint8_t *src = getPagePtr(num);
	for (int i = 0; i < _w * _h; ++i) {
		_colorBuffer[i] = _pal[src[i]].rgb565();
	}
	stub->setScreenPixels565(_colorBuffer, _w, _h);
	stub->updateScreen();
}

Graphics *GraphicsSoft_create() {
	return new GraphicsSoft();
}
