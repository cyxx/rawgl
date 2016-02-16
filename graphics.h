/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */

#ifndef __GRAPHICS_H__
#define __GRAPHICS_H__

#include "intern.h"
#include "scaler.h"

struct StrEntry {
	uint16 id;
	const char *str;
};

struct Resource;
struct Serializer;
struct SystemStub;

struct Graphics {
	typedef void (Graphics::*drawLine)(int16 x1, int16 x2, int16 y, uint8 col);

	enum {
#ifdef GP32_PORT
		OUT_GFX_W = 320,
		OUT_GFX_H = 240,
#elif DC_PORT
		OUT_GFX_W = 640,
		OUT_GFX_H = 400,
#endif
		IN_GFX_W = 320,
		IN_GFX_H = 200
	};

	static const uint8 _font[];
	static const StrEntry _stringsTable[];

	Resource *_res;
	SystemStub *_stub;

	uint8 _newPal, _curPal;
	uint8 _tempBitmap[(IN_GFX_W + 2) * (IN_GFX_H + 2)];
	uint8 *_pagePtrs[4];
	uint8 *_workPagePtr, *_frontPagePtr, *_backPagePtr;
	Ptr _pData;
	uint8 *_dataBuf;

	Graphics(Resource *res, SystemStub *stub);
	~Graphics();

	void setDataBuffer(uint8 *dataBuf, uint16 offset);
	void drawShape(uint8 color, uint16 zoom, const Point &pt);
	void fillPolygon(uint8 color, uint16 zoom, const Point &pt);
	void drawShapeParts(uint16 zoom, const Point &pt);
	void drawString(uint8 color, uint16 x, uint16 y, uint16 strId);
	void drawChar(uint8 c, uint16 x, uint16 y, uint8 color, uint8 *buf);
	void drawPoint(int16 x, int16 y, uint8 color);
	void drawLineT(int16 x1, int16 x2, int16 y, uint8 color);
	void drawLineN(int16 x1, int16 x2, int16 y, uint8 color);
	void drawLineP(int16 x1, int16 x2, int16 y, uint8 color);
	uint8 *getPagePtr(uint8 page);
	void setWorkPagePtr(uint8 page);
	void fillPage(uint8 page, uint8 color);
	void copyPage(uint8 src, uint8 dst, int16 vscroll);
	void copyBitmapToPage(const uint8 *src);
	void changePalette(uint8 pal);
	void updateDisplay(uint8 page);
	void saveOrLoad(Serializer &ser);
};

#endif
