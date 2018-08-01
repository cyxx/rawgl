
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <math.h>
#include "graphics.h"
#include "util.h"
#include "screenshot.h"
#include "systemstub.h"


struct GraphicsSoft: Graphics {
	typedef void (GraphicsSoft::*drawLine)(int16_t x1, int16_t x2, int16_t y, uint8_t col);

	uint8_t *_pagePtrs[4];
	uint8_t *_drawPagePtr;
	int _u, _v;
	int _w, _h;
	int _byteDepth;
	Color _pal[16];
	uint16_t *_colorBuffer;
	int _screenshotNum;

	GraphicsSoft();
	~GraphicsSoft();

	int xScale(int x) const { return (x * _u) >> 16; }
	int yScale(int y) const { return (y * _v) >> 16; }

	void setSize(int w, int h);
	void drawPolygon(uint8_t color, const QuadStrip &qs);
	void drawChar(uint8_t c, uint16_t x, uint16_t y, uint8_t color);
	void drawPoint(int16_t x, int16_t y, uint8_t color);
	void drawLineT(int16_t x1, int16_t x2, int16_t y, uint8_t color);
	void drawLineN(int16_t x1, int16_t x2, int16_t y, uint8_t color);
	void drawLineP(int16_t x1, int16_t x2, int16_t y, uint8_t color);
	uint8_t *getPagePtr(uint8_t page);
	int getPageSize() const { return _w * _h * _byteDepth; }
	void setWorkPagePtr(uint8_t page);

	virtual void init(int targetW, int targetH);

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
	virtual void drawRect(int num, uint8_t color, const Point *pt, int w, int h);
};


GraphicsSoft::GraphicsSoft() {
	_fixUpPalette = FIXUP_PALETTE_NONE;
	memset(_pagePtrs, 0, sizeof(_pagePtrs));
	_colorBuffer = 0;
	memset(_pal, 0, sizeof(_pal));
	_screenshotNum = 1;
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
	_byteDepth = _use565 ? 2 : 1;
	assert(_byteDepth == 1 || _byteDepth == 2);
	_colorBuffer = (uint16_t *)realloc(_colorBuffer, _w * _h * sizeof(uint16_t));
	if (!_colorBuffer) {
		error("Unable to allocate color buffer w %d h %d", _w, _h);
	}
	for (int i = 0; i < 4; ++i) {
		_pagePtrs[i] = (uint8_t *)realloc(_pagePtrs[i], getPageSize());
		if (!_pagePtrs[i]) {
			error("Not enough memory to allocate offscreen buffers");
		}
		memset(_pagePtrs[i], 0, getPageSize());
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
		x = xScale(x);
		y = yScale(y);
		const uint8_t *ft = _font + (c - 0x20) * 8;
		const int offset = (x + y * _w) * _byteDepth;
		if (_byteDepth == 1) {
			for (int j = 0; j < 8; ++j) {
				const uint8_t ch = ft[j];
				for (int i = 0; i < 8; ++i) {
					if (ch & (1 << (7 - i))) {
						_drawPagePtr[offset + j * _w + i] = color;
					}
				}
			}
		} else if (_byteDepth == 2) {
			const uint16_t rgbColor = _pal[color].rgb565();
			for (int j = 0; j < 8; ++j) {
				const uint8_t ch = ft[j];
				for (int i = 0; i < 8; ++i) {
					if (ch & (1 << (7 - i))) {
						((uint16_t *)(_drawPagePtr + offset))[j * _w + i] = rgbColor;
					}
				}
			}
		}
	}
}

void GraphicsSoft::drawPoint(int16_t x, int16_t y, uint8_t color) {
	x = xScale(x);
	y = yScale(y);
	const int offset = y * _w + x * _byteDepth;
	if (_byteDepth == 1) {
		switch (color) {
		case COL_ALPHA:
			_drawPagePtr[offset] |= 8;
			break;
		case COL_PAGE:
			_drawPagePtr[offset] = *(_pagePtrs[0] + offset);
			break;
		default:
			_drawPagePtr[offset] = color;
			break;
		}
	} else if (_byteDepth == 2) {
		switch (color) {
		case COL_ALPHA:
			*(uint16_t *)(_drawPagePtr + offset) |= 8;
			break;
		case COL_PAGE:
			*(uint16_t *)(_drawPagePtr + offset) = *(uint16_t *)(_pagePtrs[0] + offset);
			break;
		default:
			*(uint16_t *)(_drawPagePtr + offset) = _pal[color].rgb565();
			break;
		}
	}
}

void GraphicsSoft::drawLineT(int16_t x1, int16_t x2, int16_t y, uint8_t color) {
	int16_t xmax = MAX(x1, x2);
	int16_t xmin = MIN(x1, x2);
	int w = xmax - xmin + 1;
	const int offset = (y * _w + xmin) * _byteDepth;
	if (_byteDepth == 1) {
		for (int i = 0; i < w; ++i) {
			_drawPagePtr[offset + i] |= 8;
		}
	} else if (_byteDepth == 2) {
		for (int i = 0; i < w; ++i) {
			((uint16_t *)(_drawPagePtr + offset))[i] |= 8;
		}
	}
}

void GraphicsSoft::drawLineN(int16_t x1, int16_t x2, int16_t y, uint8_t color) {
	int16_t xmax = MAX(x1, x2);
	int16_t xmin = MIN(x1, x2);
	const int w = xmax - xmin + 1;
	const int offset = (y * _w + xmin) * _byteDepth;
	if (_byteDepth == 1) {
		memset(_drawPagePtr + offset, color, w);
	} else if (_byteDepth == 2) {
		const uint16_t rgbColor = _pal[color].rgb565();
		for (int i = 0; i < w; ++i) {
			((uint16_t *)(_drawPagePtr + offset))[i] = rgbColor;
		}
	}
}

void GraphicsSoft::drawLineP(int16_t x1, int16_t x2, int16_t y, uint8_t color) {
	if (_drawPagePtr == _pagePtrs[0]) {
		return;
	}
	int16_t xmax = MAX(x1, x2);
	int16_t xmin = MIN(x1, x2);
	const int w = xmax - xmin + 1;
	const int offset = (y * _w + xmin) * _byteDepth;
	memcpy(_drawPagePtr + offset, _pagePtrs[0] + offset, w * _byteDepth);
}

uint8_t *GraphicsSoft::getPagePtr(uint8_t page) {
	assert(page >= 0 && page < 4);
	return _pagePtrs[page];
}

void GraphicsSoft::setWorkPagePtr(uint8_t page) {
	_drawPagePtr = getPagePtr(page);
}

void GraphicsSoft::init(int targetW, int targetH) {
	Graphics::init(targetW, targetH);
	setSize(targetW, targetH);
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
		if (_byteDepth == 1) {
			if (fmt == FMT_CLUT && _w == w && _h == h) {
				memcpy(getPagePtr(buffer), data, w * h);
			}
		}
	}
	if (_byteDepth == 2) {
		if (fmt == FMT_RGB565 && _w == w && _h == h) {
			memcpy(getPagePtr(buffer), data, getPageSize());
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
	if (_byteDepth == 1) {
		memset(getPagePtr(num), color, getPageSize());
	} else if (_byteDepth == 2) {
		const uint16_t rgbColor = _pal[color].rgb565();
		uint16_t *p = (uint16_t *)getPagePtr(num);
		for (int i = 0; i < _w * _h; ++i) {
			p[i] = rgbColor;
		}
	}
}

void GraphicsSoft::copyBuffer(int dst, int src, int vscroll) {
	if (vscroll == 0) {
		memcpy(getPagePtr(dst), getPagePtr(src), getPageSize());
	} else if (vscroll >= -199 && vscroll <= 199) {
		const int dy = yScale(vscroll);
		if (dy < 0) {
			memcpy(getPagePtr(dst), getPagePtr(src) - dy * _w * _byteDepth, (_h + dy) * _w * _byteDepth);
		} else {
			memcpy(getPagePtr(dst) + dy * _w * _byteDepth, getPagePtr(src), (_h - dy) * _w * _byteDepth);
		}
	}
}

static void dumpBuffer565(const uint16_t *src, int w, int h, int num) {
	char name[32];
	snprintf(name, sizeof(name), "screenshot-%d.tga", num);
	saveTGA(name, src, w, h);
	debug(DBG_INFO, "Written '%s'", name);
}

static void dumpPalette555(uint16_t *dst, int w, const Color *pal) {
	static const int SZ = 16;
	for (int color = 0; color < 16; ++color) {
		uint16_t *p = dst + (color & 7) * SZ;
		for (int y = 0; y < SZ; ++y) {
			for (int x = 0; x < SZ; ++x) {
				p[x] = pal[color].rgb565();
			}
			p += w;
		}
		if (color == 7) {
			dst += SZ * w;
		}
	}
}

void GraphicsSoft::drawBuffer(int num, SystemStub *stub) {
	int w, h;
	float ar[4];
	stub->prepareScreen(w, h, ar);
	if (_byteDepth == 1) {
		const uint8_t *src = getPagePtr(num);
		for (int i = 0; i < _w * _h; ++i) {
			_colorBuffer[i] = _pal[src[i]].rgb565();
		}
		if (0) {
			dumpPalette555(_colorBuffer, _w, _pal);
		}
		stub->setScreenPixels565(_colorBuffer, _w, _h);
		if (_screenshot) {
			dumpBuffer565(_colorBuffer, _w, _h, _screenshotNum);
			++_screenshotNum;
			_screenshot = false;
		}
	} else if (_byteDepth == 2) {
		const uint16_t *src = (uint16_t *)getPagePtr(num);
		stub->setScreenPixels565(src, _w, _h);
		if (_screenshot) {
			dumpBuffer565(src, _w, _h, _screenshotNum);
			++_screenshotNum;
			_screenshot = false;
		}
	}
	stub->updateScreen();
}

void GraphicsSoft::drawRect(int num, uint8_t color, const Point *pt, int w, int h) {
	assert(_byteDepth == 2);
	setWorkPagePtr(num);
	const uint16_t rgbColor = _pal[color].rgb565();
	const int x1 = xScale(pt->x);
	const int y1 = yScale(pt->y);
	const int x2 = xScale(pt->x + w - 1);
	const int y2 = yScale(pt->y + h - 1);
	// horizontal
	for (int x = x1; x <= x2; ++x) {
		*(uint16_t *)(_drawPagePtr + (y1 * _w + x) * _byteDepth) = rgbColor;
		*(uint16_t *)(_drawPagePtr + (y2 * _w + x) * _byteDepth) = rgbColor;
	}
	// vertical
	for (int y = y1; y <= y2; ++y) {
		*(uint16_t *)(_drawPagePtr + (y * _w + x1) * _byteDepth) = rgbColor;
		*(uint16_t *)(_drawPagePtr + (y * _w + x2) * _byteDepth) = rgbColor;
	}
}

Graphics *GraphicsSoft_create() {
	return new GraphicsSoft();
}
