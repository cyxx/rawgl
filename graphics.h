
#ifndef GRAPHICS_H__
#define GRAPHICS_H__

#include "intern.h"

enum {
	FMT_CLUT,
	FMT_RGB565,
	FMT_RGB,
	FMT_RGBA,
};

enum {
	FIXUP_PALETTE_NONE,
	FIXUP_PALETTE_REDRAW, // redraw all primitives on setPal script call
};

enum {
	COL_ALPHA = 0x10, // transparent pixel (OR'ed with 0x8)
	COL_PAGE  = 0x11, // buffer 0 pixel
	COL_BMP   = 0xFF, // bitmap in buffer 0 pixel
};

enum {
	GRAPHICS_ORIGINAL,
	GRAPHICS_SOFTWARE,
	GRAPHICS_GL
};

enum {
	GFX_W = 320,
	GFX_H = 200
};

struct SystemStub;

struct Graphics {

	static const uint8_t _font[];
	static bool _is1991; // draw graphics as in the original 1991 game release
	static bool _use565; // use 16bits graphics buffer (for 3DO)

	int _fixUpPalette;
	bool _screenshot;

	virtual ~Graphics() {};

	virtual void init(int targetW, int targetH) { _screenshot = false; }
	virtual void fini() {}

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
	virtual void drawBuffer(int num, SystemStub *) = 0;
	virtual void drawRect(int num, uint8_t color, const Point *pt, int w, int h) = 0;
};

Graphics *GraphicsGL_create();
Graphics *GraphicsSoft_create();

#endif
