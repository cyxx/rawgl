
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef VIDEO_H__
#define VIDEO_H__

#include "intern.h"

struct StrEntry {
	uint16_t id;
	const char *str;
};

struct Resource;
struct Serializer;
struct SystemStub;

struct Video {

	static const StrEntry _stringsTableFr[];
	static const StrEntry _stringsTableEng[];

	Resource *_res;
	SystemStub *_stub;

	uint8_t _newPal, _curPal;
	uint8_t _listPtrs[3];
	Ptr _pData;
	uint8_t *_dataBuf;
	const StrEntry *_stringsTable;
	uint8_t _tempBitmap[320 * 200];

	Video(Resource *res, SystemStub *stub);
	void init();

	void setFont(const uint8_t *font);
	void setDataBuffer(uint8_t *dataBuf, uint16_t offset);
	void drawShape(uint8_t color, uint16_t zoom, const Point *pt);
	void fillPolygon(uint16_t color, uint16_t zoom, const Point *pt);
	void drawShapeParts(uint16_t zoom, const Point *pt);
	void drawString(uint8_t color, uint16_t x, uint16_t y, uint16_t strId);
	uint8_t getPagePtr(uint8_t page);
	void setWorkPagePtr(uint8_t page);
	void fillPage(uint8_t page, uint8_t color);
	void copyPage(uint8_t src, uint8_t dst, int16_t vscroll);
	void copyPagePtr(const uint8_t *src);
	void copyBitmapPtr(const uint8_t *src);
	void changePal(uint8_t pal);
	void updateDisplay(uint8_t page);
};

#endif
