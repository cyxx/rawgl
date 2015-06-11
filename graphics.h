
#ifndef GRAPHICS_H__
#define GRAPHICS_H__

#include "intern.h"

enum {
	RENDER_ORIGINAL, // 320x200
	RENDER_GL,
};

enum {
	FMT_CLUT,
	FMT_RGB565,
	FMT_RGB,
	FMT_RGBA,
};

enum {
	FIXUP_PALETTE_NONE,
	FIXUP_PALETTE_RENDER, // redraw all primitives on setPal script call
};

enum {
	COL_ALPHA = 0x10,
	COL_PAGE  = 0x11,
	COL_BMP   = 0xFF,
};

struct Graphics {
	int _fixUpPalette;

	virtual ~Graphics() {};

	virtual int getGraphicsMode() const = 0;
	virtual void setSize(int w, int h) = 0;
	virtual void setFont(const uint8_t *src, int w, int h) = 0;
	virtual void setPalette(const Color *colors, int count) = 0;
	virtual void setSpriteAtlas(const uint8_t *src, int w, int h, int xSize, int ySize) = 0;
	virtual void drawSprite(int buffer, int num, const Point *pt) = 0;
	virtual void drawBitmap(int buffer, const uint8_t *data, int w, int h, int fmt) = 0;
	virtual void drawPoint(int buffer, uint8_t color, const Point *pt) = 0;
	virtual void drawQuadStrip(int buffer, uint8_t color, const QuadStrip *qs) = 0;
	virtual void drawStringChar(int buffer, uint8_t color, char c, const Point *pt) = 0;
	virtual void clearBuffer(int num, uint8_t color) = 0;
	virtual void copyBuffer(int dst, int src, int vscroll = 0) = 0;
	virtual void drawBuffer(int num) = 0;
};

Graphics *GraphicsGL_create();

struct GraphicsSoft {
	typedef void (GraphicsSoft::*drawLine)(int16_t x1, int16_t x2, int16_t y, uint8_t col);

	enum {
		GFX_W = 320,
		GFX_H = 200,
	};

	static const uint8_t _font[];

	uint8_t *_pagePtrs[4];
	uint8_t *_workPagePtr;
	int _u, _v;
	int _w, _h;

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
};

#endif
