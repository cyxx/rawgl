/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "video.h"
#include "resource.h"
#include "systemstub.h"


Video::Video(Resource *res, SystemStub *stub) 
	: _res(res), _stub(stub), _hasHeadSprites(false), _displayHead(true) {
}

void Video::init() {
	_nextPal = _currentPal = 0xFF;
	_listPtrs[2] = getPagePtr(1);
	_listPtrs[1] = getPagePtr(2);
	setWorkPagePtr(0xFE);
}

void Video::setFont(const uint8_t *font) {
	_stub->setFont(font);
}

void Video::setHeads(const uint8_t *src) {
	_stub->setSpriteAtlas(src, 2, 2);
	_hasHeadSprites = true;
}

void Video::setDataBuffer(uint8_t *dataBuf, uint16_t offset) {
	_dataBuf = dataBuf;
	_pData.pc = dataBuf + offset;
}

void Video::drawShape(uint8_t color, uint16_t zoom, const Point *pt) {
	uint8_t i = _pData.fetchByte();
	if (i >= 0xC0) {
		if (color & 0x80) {
			color = i & 0x3F;
		}
		fillPolygon(color, zoom, pt);
	} else {
		i &= 0x3F;
		if (i == 1) {
			warning("Video::drawShape() ec=0x%X (i != 2)", 0xF80);
		} else if (i == 2) {
			drawShapeParts(zoom, pt);
		} else {
			warning("Video::drawShape() ec=0x%X (i != 2)", 0xFBB);
		}
	}
}

void Video::fillPolygon(uint16_t color, uint16_t zoom, const Point *pt) {
	const uint8_t *p = _pData.pc;

	uint16_t bbw = (*p++) * zoom / 64;
	uint16_t bbh = (*p++) * zoom / 64;
	
	int16_t x1 = pt->x - bbw / 2;
	int16_t x2 = pt->x + bbw / 2;
	int16_t y1 = pt->y - bbh / 2;
	int16_t y2 = pt->y + bbh / 2;

	if (x1 > 319 || x2 < 0 || y1 > 199 || y2 < 0)
		return;

	QuadStrip qs;
	qs.numVertices = *p++;
	if ((qs.numVertices & 1) != 0) {
		warning("Unexpected number of vertices %d", qs.numVertices);
		return;
	}
	assert(qs.numVertices < QuadStrip::MAX_VERTICES);

	for (int i = 0; i < qs.numVertices; ++i) {
		Point *v = &qs.vertices[i];
		v->x = x1 + (*p++) * zoom / 64;
		v->y = y1 + (*p++) * zoom / 64;
	}

	if (qs.numVertices == 4 && bbw == 0 && bbh <= 1) {
		_stub->addPointToList(_listPtrs[0], color, pt);
	} else {
		_stub->addQuadStripToList(_listPtrs[0], color, &qs);
	}
}

void Video::drawShapeParts(uint16_t zoom, const Point *pgc) {
	Point pt;
	pt.x = pgc->x - _pData.fetchByte() * zoom / 64;
	pt.y = pgc->y - _pData.fetchByte() * zoom / 64;
	int16_t n = _pData.fetchByte();
	debug(DBG_VIDEO, "Video::drawShapeParts n=%d", n);
	for ( ; n >= 0; --n) {
		uint16_t off = _pData.fetchWord();
		Point po(pt);
		po.x += _pData.fetchByte() * zoom / 64;
		po.y += _pData.fetchByte() * zoom / 64;
		uint16_t color = 0xFF;
		if (off & 0x8000) {
			color = *_pData.pc & 0x7F;
			if (_hasHeadSprites && _stub->getRenderMode() != RENDER_ORIGINAL && _displayHead) {
				const int id = _pData.pc[1];
				switch (id) {
				case 0x4A: { // facing right
						Point pos(po.x - 4, po.y - 7);
						_stub->addSpriteToList(_listPtrs[0], 0, &pos);
					}
				case 0x4D:
					return;
				case 0x4F: { // facing left
						Point pos(po.x - 4, po.y - 7);
						_stub->addSpriteToList(_listPtrs[0], 1, &pos);
					}
				case 0x50:
					return;
				}
			}
			_pData.pc += 2;
		}
		off &= 0x7FFF;
		uint8_t *bak = _pData.pc;
		_pData.pc = _dataBuf + off * 2;
		drawShape(color, zoom, &po);
		_pData.pc = bak;
	}
}

static const int NTH_EDITION_STRINGS_COUNT = 157;

static const char *findString15th(int id) {
	for (int i = 0; i < NTH_EDITION_STRINGS_COUNT; ++i) {
		if (Video::_stringsId15th[i] == id) {
			return Video::_stringsTable15th[i];
		}
	}
	return 0;
}

static const char *findString20th(Resource *res, int id) {
	for (int i = 0; i < NTH_EDITION_STRINGS_COUNT; ++i) {
		if (Video::_stringsId15th[i] == id) {
			return res->getString(i);
		}
	}
	return 0;
}

static const char *findString(const StrEntry *stringsTable, int id) {
	for (const StrEntry *se = stringsTable; se->id != 0xFFFF; ++se) {
		if (se->id == id) {
			return se->str;
		}
	}
	return 0;
}

void Video::drawString(uint8_t color, uint16_t x, uint16_t y, uint16_t strId) {
	bool escapedChars = false;
	const char *str;
	if (_res->getDataType() == Resource::DT_15TH_EDITION) {
		str = findString15th(strId);
	} else if (_res->getDataType() == Resource::DT_20TH_EDITION) {
		str = findString20th(_res, strId);
		escapedChars = true;
	} else if (_res->getDataType() == Resource::DT_WIN31) {
		str = _res->getString(strId);
	} else {
		str = findString(_stringsTable, strId);
	}
	if (!str) {
		warning("Unknown string id 0x%x", strId);
		return;
	}
	debug(DBG_VIDEO, "drawString(%d, %d, %d, '%s')", color, x, y, str);
	uint16_t xx = x;
	int len = strlen(str);
	for (int i = 0; i < len; ++i) {
		if (str[i] == '\n') {
			y += 8;
			x = xx;
		} else if (str[i] == '\\' && escapedChars) {
			++i;
			if (i < len) {
				switch (str[i]) {
				case 'n':
					y += 8;
					x = xx;
					break;
				}
			}
		} else {
			Point pt(x * 8, y);
			_stub->addCharToList(_listPtrs[0], color, str[i], &pt);
			++x;
		}
	}
}

uint8_t Video::getPagePtr(uint8_t page) {
	uint8_t p;
	if (page <= 3) {
		p = page;
	} else {
		switch (page) {
		case 0xFF:
			p = _listPtrs[2];
			break;
		case 0xFE:
			p = _listPtrs[1];
			break;
		default:
			p = 0; // XXX check
			warning("Video::getPagePtr() p != [0,1,2,3,0xFF,0xFE] == 0x%X", page);
			break;
		}
	}
	return p;
}

void Video::setWorkPagePtr(uint8_t page) {
	debug(DBG_VIDEO, "Video::setWorkPagePtr(%d)", page);
	_listPtrs[0] = getPagePtr(page);
}

void Video::fillPage(uint8_t page, uint8_t color) {
	debug(DBG_VIDEO, "Video::fillPage(%d, %d)", page, color);
	_stub->clearList(getPagePtr(page), color);
}

void Video::copyPage(uint8_t src, uint8_t dst, int16_t vscroll) {
	debug(DBG_VIDEO, "Video::copyPage(%d, %d)", src, dst);
	if (src >= 0xFE || !((src &= 0xBF) & 0x80)) {
		_stub->copyList(getPagePtr(dst), getPagePtr(src));
	} else {
		uint8_t sl = getPagePtr(src & 3);
		uint8_t dl = getPagePtr(dst);
		if (sl != dl && vscroll >= -199 && vscroll <= 199) {
			_stub->copyList(dl, sl, vscroll);
		}
	}
}

void Video::copyPagePtr(const uint8_t *src) {
	for (int y = 0; y < 200; ++y) {
		uint8_t *dst = _tempBitmap + (199 - y) * 320;
		int w = 40;
		while (w--) {
			uint8_t p[] = {
				*(src + 8000 * 3),
				*(src + 8000 * 2),
				*(src + 8000 * 1),
				*(src + 8000 * 0)
			};
			for(int j = 0; j < 4; ++j) {
				uint8_t acc = 0;
				for (int i = 0; i < 8; ++i) {
					acc <<= 1;
					acc |= (p[i & 3] & 0x80) ? 1 : 0;
					p[i & 3] <<= 1;
				}
				*dst++ = acc >> 4;
				*dst++ = acc & 0xF;
			}
			++src;
		}
	}	
	_stub->addBitmapToList(0, _tempBitmap);
}

void Video::copyBitmapPtr(const uint8_t *src) {
	_stub->addBitmapToList(0, src);
}

static void readPaletteWin31(const uint8_t *buf, int num, Color pal[16]) {
	const uint8_t *p = buf + num * 16 * sizeof(uint16_t);
	for (int i = 0; i < 16; ++i) {
		const uint16_t index = READ_LE_UINT16(p); p += 2;
		const uint32_t color = READ_LE_UINT32(buf + 0xC04 + index * sizeof(uint32_t));
		pal[i].r =  color        & 255;
		pal[i].g = (color >>  8) & 255;
		pal[i].b = (color >> 16) & 255;
	}
}

static void readPaletteAmiga(const uint8_t *buf, int num, Color pal[16]) {
	const uint8_t *p = buf + num * 16 * sizeof(uint16_t);
	for (int i = 0; i < 16; ++i) {
		const uint16_t color = READ_BE_UINT16(p); p += 2;
		const uint8_t r = (color >> 8) & 0xF;
		const uint8_t g = (color >> 4) & 0xF;
		const uint8_t b =  color       & 0xF;
		pal[i].r = (r << 4) | r;
		pal[i].g = (g << 4) | g;
		pal[i].b = (b << 4) | b;
	}
}

void Video::changePal(uint8_t palNum) {
	if (palNum < 32 && palNum != _currentPal) {
		Color pal[16];
		if (_res->getDataType() == Resource::DT_WIN31) {
			readPaletteWin31(_res->_segVideoPal, palNum, pal);
		} else {
			readPaletteAmiga(_res->_segVideoPal, palNum, pal);
		}
		_stub->setPalette(pal, 16);
		_currentPal = palNum;
	}
}

void Video::updateDisplay(uint8_t page) {
	debug(DBG_VIDEO, "Video::updateDisplay(%d)", page);
	if (page != 0xFE) {
		if (page == 0xFF) {
			SWAP(_listPtrs[1], _listPtrs[2]);
		} else {
			_listPtrs[1] = getPagePtr(page);
		}
	}
	if (_nextPal != 0xFF) {
		changePal(_nextPal);
		_nextPal = 0xFF;
	}
	_stub->blitList(_listPtrs[1]);
}

void Video::buildPalette256() {
	Color palette[512];
	int count = 0;

	const uint8_t *data = _res->_segVideoPal;
	// 32 palettes of 16 colors
	for (int i = 0; i < 32 * 16; ++i) {
		uint16_t color = READ_BE_UINT16(data + i * 2);
		Color c;
		c.b = color & 15; color >>= 4;
		c.g = color & 15; color >>= 4;
		c.r = color & 15;
		// look for exact match
		int match = -1;
		for (int j = 0; j < count; ++j) {
			if (palette[j].r == c.r && palette[j].g == c.g && palette[j].b == c.b) {
				match = j;
				break;
			}
		}
		if (match < 0) {
			palette[count] = c;
			++count;
		}
	}
	// all palette resources have less than 256 unique colors
	debug(DBG_VIDEO, "%d unique colors", count);
}
