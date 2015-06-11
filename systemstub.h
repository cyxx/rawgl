
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef SYSTEMSTUB_H__
#define SYSTEMSTUB_H__

#include "intern.h"

struct PlayerInput {
	enum {
		DIR_LEFT  = 1 << 0,
		DIR_RIGHT = 1 << 1,
		DIR_UP    = 1 << 2,
		DIR_DOWN  = 1 << 3
	};

	uint8_t dirMask;
	bool button;
	bool code;
	bool pause;
	bool quit;
	char lastChar;
	bool fastMode;
};

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

struct SystemStub {
	typedef void (*AudioCallback)(void *param, uint8_t *stream, int len);

	Graphics *_g;
	PlayerInput _pi;

	virtual ~SystemStub() {}

	virtual void init(const char *title) = 0;
	virtual void fini() = 0;

	virtual void updateScreen() = 0;

	virtual void processEvents() = 0;
	virtual void sleep(uint32_t duration) = 0;
	virtual uint32_t getTimeStamp() = 0;
};

extern SystemStub *SystemStub_OGL_create(const char *name);

#endif
