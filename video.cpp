
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "video.h"
#include "resource.h"
#include "systemstub.h"


Video::Video(Resource *res, SystemStub *stub) 
	: _res(res), _stub(stub) {
}

void Video::init() {
	_newPal = 0xFF;
	_listPtrs[2] = getPagePtr(1);
	_listPtrs[1] = getPagePtr(2);
	changePagePtr1(0xFE);
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
	assert((qs.numVertices & 1) == 0 && qs.numVertices < QuadStrip::MAX_VERTICES);

	for (int i = 0; i < qs.numVertices; ++i) {
		Point *v = &qs.vertices[i];
		v->x = x1 + (*p++) * zoom / 64;
		v->y = y1 + (*p++) * zoom / 64;
	}

	if (qs.numVertices == 4 && bbw == 0) {
		if (bbh <= 1) {
			_stub->addPointToList(_listPtrs[0], color, pt);
		} else {
			_stub->addLineToList(_listPtrs[0], color, &qs.vertices[0], &qs.vertices[1]);			
		}
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
			_pData.pc += 2;
		}
		off &= 0x7FFF;
		uint8_t *bak = _pData.pc;
		_pData.pc = _dataBuf + off * 2;
		drawShape(color, zoom, &po);
		_pData.pc = bak;
	}
}

void Video::drawString(uint8_t color, uint16_t x, uint16_t y, uint16_t strId) {
	const StrEntry *se = _stringsTable;
	while (se->id != 0xFFFF && se->id != strId) ++se;
	debug(DBG_VIDEO, "drawString(%d, %d, %d, '%s')", color, x, y, se->str);
	uint16_t xx = x;
	int len = strlen(se->str);
	for (int i = 0; i < len; ++i) {
		if (se->str[i] == '\n') {
			y += 8;
			x = xx;
		} else {
			Point pt(x * 8, y);
			_stub->addCharToList(_listPtrs[0], color, se->str[i], &pt);
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

void Video::changePagePtr1(uint8_t page) {
	debug(DBG_VIDEO, "Video::changePagePtr1(%d)", page);
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
	uint8_t *dst = _tempBitmap;
	int h = 200;
	while (h--) {
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

void Video::changePal(uint8_t palNum) {
	if (palNum < 32) {
		uint8_t *p = _res->_segVideoPal + palNum * 32;
		Color pal[16];
		for (int i = 0; i < 16; ++i) {
			uint8_t c1 = *p++;
			uint8_t c2 = *p++;
			pal[i].r = ((c1 & 0x0F) << 2) | ((c1 & 0x0F) >> 2);
			pal[i].g = ((c2 & 0xF0) >> 2) | ((c2 & 0xF0) >> 6);
			pal[i].b = ((c2 & 0x0F) >> 2) | ((c2 & 0x0F) << 2);
		}
		_stub->setPalette(pal, 16);
		_curPal = palNum;
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
	if (_newPal != 0xFF) {
		changePal(_newPal);
		_newPal = 0xFF;
	}
	_stub->blitList(_listPtrs[1]);
}
