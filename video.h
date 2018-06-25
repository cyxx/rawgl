
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

struct Graphics;
struct Resource;
struct SystemStub;

struct Video {

	static const StrEntry _stringsTableFr[];
	static const StrEntry _stringsTableEng[];
	static const StrEntry _stringsTableDemo[];
	static const uint16_t _stringsId15th[];
	static const char *_stringsTable15th[];
	static const StrEntry _stringsTable3DO[];
	static const char *_noteText3DO;
	static const char *_endText3DO;
	static const uint8_t *_vertices3DO[201];

	Resource *_res;
	Graphics *_graphics;
	bool _hasHeadSprites;
	bool _displayHead;

	uint8_t _nextPal, _currentPal;
	uint8_t _buffers[3];
	Ptr _pData;
	uint8_t *_dataBuf;
	const StrEntry *_stringsTable;
	uint8_t _tempBitmap[320 * 200];
	uint16_t _bitmap565[320 * 200];

	Video(Resource *res);
	void init();

	void setDefaultFont();
	void setFont(const uint8_t *font);
	void setHeads(const uint8_t *src);
	void setDataBuffer(uint8_t *dataBuf, uint16_t offset);
	void drawShape(uint8_t color, uint16_t zoom, const Point *pt);
	void drawShapePart3DO(int color, int part, const Point *pt);
	void drawShape3DO(int color, int zoom, const Point *pt);
	void fillPolygon(uint16_t color, uint16_t zoom, const Point *pt);
	void drawShapeParts(uint16_t zoom, const Point *pt);
	void drawString(uint8_t color, uint16_t x, uint16_t y, uint16_t strId);
	uint8_t getPagePtr(uint8_t page);
	void setWorkPagePtr(uint8_t page);
	void fillPage(uint8_t page, uint8_t color);
	void copyPage(uint8_t src, uint8_t dst, int16_t vscroll);
	void copyBitmapPtr(const uint8_t *src, uint32_t size = 0);
	void changePal(uint8_t pal);
	void updateDisplay(uint8_t page, SystemStub *stub);
	void captureDisplay();
};

#endif
