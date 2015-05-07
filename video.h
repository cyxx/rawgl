
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
	static const uint16_t _stringsId15th[];
	static const char *_stringsTable15th[];

	Resource *_res;
	SystemStub *_stub;
	bool _hasHeadSprites;
	bool _displayHead;

	uint8_t _nextPal, _currentPal;
	uint8_t _listPtrs[3];
	Ptr _pData;
	uint8_t *_dataBuf;
	const StrEntry *_stringsTable;
	uint8_t _tempBitmap[320 * 200];

	Video(Resource *res, SystemStub *stub);
	void init();

	void setFont(const uint8_t *font);
	void setHeads(const uint8_t *src);
	void setDataBuffer(uint8_t *dataBuf, uint16_t offset);
	void drawShape(uint8_t color, uint16_t zoom, const Point *pt);
	void drawShape3DO(int color, int zoom, const Point *pt);
	void fillPolygon(uint16_t color, uint16_t zoom, const Point *pt);
	void drawShapeParts(uint16_t zoom, const Point *pt);
	void drawString(uint8_t color, uint16_t x, uint16_t y, uint16_t strId);
	uint8_t getPagePtr(uint8_t page);
	void setWorkPagePtr(uint8_t page);
	void fillPage(uint8_t page, uint8_t color);
	void copyPage(uint8_t src, uint8_t dst, int16_t vscroll);
	void copyPagePtr(const uint8_t *src);
	void copyBitmapPtr(const uint8_t *src, uint32_t size = 0);
	void changePal(uint8_t pal);
	void updateDisplay(uint8_t page);
	void buildPalette256();
};

#endif
