/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */

#include "resource.h"
#include "serializer.h"
#include "systemstub.h"
#include "graphics.h"


Graphics::Graphics(Resource *res, SystemStub *stub) 
	: _res(res), _stub(stub), _newPal(0xFF), _curPal(0) {
	_pagePtrs[0]=(uint8 *)0xC726000;
	_pagePtrs[1]=(uint8 *)0xC739000;
	_pagePtrs[2]=(uint8 *)0xC74C000;
	_pagePtrs[3]=(uint8 *)0xC75F000;
	for (int i = 0; i < 4; ++i) {
		/*_pagePtrs[i] = (uint8 *)malloc(OUT_GFX_W * OUT_GFX_H);
		if (!_pagePtrs[i]) {
			error("Not enough memory to allocate offscreen buffers");
		}*/
		memset(_pagePtrs[i], 0, OUT_GFX_W * OUT_GFX_H);
	}
	memset(_tempBitmap, 0, (IN_GFX_W + 2) * (IN_GFX_H + 2));
	_backPagePtr = getPagePtr(1);
	_frontPagePtr = getPagePtr(2);
	setWorkPagePtr(0xFE);
}

Graphics::~Graphics() {
	for (int i = 0; i < 4; ++i) {
		free(_pagePtrs[i]);
		_pagePtrs[i] = 0;
	}
}

void Graphics::setDataBuffer(uint8 *dataBuf, uint16 offset) {
	_dataBuf = dataBuf;
	_pData.pc = dataBuf + offset;
}

void Graphics::drawShape(uint8 color, uint16 zoom, const Point &pt) {
	uint8 c = _pData.fetchByte();
	if (c >= 0xC0) {
		if (color & 0x80) {
			color = c & 0x3F;
		}
		fillPolygon(color, zoom, pt);
	} else if (c == 2) {
		drawShapeParts(zoom, pt);
	}
}

static uint32 calcStep(const Point &p1, const Point &p2, uint16 &dy) {
	dy = p2.y - p1.y;
	uint16 delta = (dy == 0) ? 1 : dy;
	return ((p2.x - p1.x) << 16) / delta;
}

void Graphics::fillPolygon(uint8 color, uint16 zoom, const Point &pt) {
	const uint8 *p = _pData.pc;

	uint16 bbw = (*p++) * zoom / 64;
	uint16 bbh = (*p++) * zoom / 64;
	
	int16 x1 = (pt.x - bbw / 2) * OUT_GFX_W / IN_GFX_W;
	int16 x2 = (pt.x + bbw / 2) * OUT_GFX_W / IN_GFX_W;
	int16 y1 = (pt.y - bbh / 2) * OUT_GFX_H / IN_GFX_H;
	int16 y2 = (pt.y + bbh / 2) * OUT_GFX_H / IN_GFX_H;

	if (x1 >= OUT_GFX_W || x2 < 0 || y1 >= OUT_GFX_H || y2 < 0)
		return;

	QuadStrip qs;
	qs.numVertices = *p++;
	assert((qs.numVertices & 1) == 0 && qs.numVertices < QuadStrip::MAX_VERTICES);

	if (bbw == 0 && bbh == 1 && qs.numVertices == 4) {
		drawPoint(x1, y1, color);
		return;
	}
	
	for (int i = 0; i < qs.numVertices; ++i) {
		Point *v = &qs.vertices[i];
		v->x = (*p++) * OUT_GFX_W * zoom / (64 * IN_GFX_W);
		v->y = (*p++) * OUT_GFX_H * zoom / (64 * IN_GFX_H);
	}

	int16 hliney = y1;

	int i = 0;
	int j = qs.numVertices - 1;
	
	x2 = qs.vertices[i++].x + x1;
	x1 = qs.vertices[j--].x + x1;

	drawLine pdl;
	if (color < 0x10) {
		pdl = &Graphics::drawLineN;
	} else if (color > 0x10) {
		pdl = &Graphics::drawLineP;
	} else {
		pdl = &Graphics::drawLineT;
	}

	uint32 cpt1 = x1 << 16;
	uint32 cpt2 = x2 << 16;

	while (1) {
		qs.numVertices -= 2;
		if (qs.numVertices == 0) {
			return;
		}
		uint16 h;
		uint32 step1 = calcStep(qs.vertices[j + 1], qs.vertices[j], h);
		uint32 step2 = calcStep(qs.vertices[i - 1], qs.vertices[i], h);
		
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
					if (x1 < OUT_GFX_W && x2 >= 0) {
						if (x1 < 0) x1 = 0;
						if (x2 >= OUT_GFX_W) x2 = OUT_GFX_W - 1;
						(this->*pdl)(x1, x2, hliney, color);
					}
				}
				cpt1 += step1;
				cpt2 += step2;
				++hliney;
				if (hliney >= OUT_GFX_H) return;
			}
		}
	}
}

void Graphics::drawShapeParts(uint16 zoom, const Point &pgc) {
	Point pt(pgc);
	pt.x -= _pData.fetchByte() * zoom / 64;
	pt.y -= _pData.fetchByte() * zoom / 64;
	int16 n = _pData.fetchByte();
	for ( ; n >= 0; --n) {
		uint16 off = _pData.fetchWord();
		Point po(pt);
		po.x += _pData.fetchByte() * zoom / 64;
		po.y += _pData.fetchByte() * zoom / 64;
		uint16 color = 0xFF;
		if (off & 0x8000) {
			color = *_pData.pc & 0x7F;
			_pData.pc += 2;
			off &= 0x7FFF;
		}
		uint8 *bak = _pData.pc;
		_pData.pc = _dataBuf + off * 2;
		drawShape(color, zoom, po);
		_pData.pc = bak;
	}
}

void Graphics::drawString(uint8 color, uint16 x, uint16 y, uint16 strId) {
	const StrEntry *se = _stringsTable;
	while (se->id != 0xFFFF && se->id != strId) ++se;
	uint16 xx = x;
	int len = strlen(se->str);
	for (int i = 0; i < len; ++i) {
		if (se->str[i] == '\n') {
			y += 8;
			x = xx;
		} else {
			drawChar(se->str[i], x, y, color, _workPagePtr);
			++x;
		}
	}
}

void Graphics::drawChar(uint8 c, uint16 x, uint16 y, uint8 color, uint8 *buf) {
	if (x <= 39 * OUT_GFX_W / IN_GFX_W && y <= 192 * OUT_GFX_H / IN_GFX_H) {
		const uint8 *ft = _font + (c - 0x20) * 8;
		uint8 *p = buf + x * 8 * OUT_GFX_W / IN_GFX_W + y * OUT_GFX_H / IN_GFX_H * OUT_GFX_W;
		for (int j = 0; j < 8; ++j) {
			uint8 ch = *(ft + j);
			for (int i = 0; i < 8; ++i) {
				if (ch & 0x80) {
					for (int l = 0; l < OUT_GFX_H / IN_GFX_H; ++l) {
						for (int k = 0; k < OUT_GFX_W / IN_GFX_W; ++k) {
							p[l * OUT_GFX_W + i * OUT_GFX_W / IN_GFX_W + k] = color;
						}
					}
				}
				ch <<= 1;
			}
			p += OUT_GFX_W * OUT_GFX_W / IN_GFX_W;
		}
	}
}

void Graphics::drawPoint(int16 x, int16 y, uint8 color) {
	int off = y * OUT_GFX_W + x;
	for (int j = 0; j < OUT_GFX_H / IN_GFX_H; ++j) {
		for (int i = 0; i < OUT_GFX_W / IN_GFX_W; ++i) {
			switch (color) {
			case 0x10:
				_workPagePtr[off + i] |= 0x8;
				break;
			case 0x11:
				_workPagePtr[off + i] = *(_pagePtrs[0] + off + i);
				break;
			default:
				_workPagePtr[off + i] = color;
				break;
			}
		}
		off += OUT_GFX_W;
	}
}

void Graphics::drawLineT(int16 x1, int16 x2, int16 y, uint8 color) {
	int16 xmax = MAX(x1, x2);
	int16 xmin = MIN(x1, x2);
	int w = xmax - xmin + 1;
	uint8 *p = _workPagePtr + y * OUT_GFX_W + xmin;
	while (w--) {
		*p++ |= 0x8;
	}
}

void Graphics::drawLineN(int16 x1, int16 x2, int16 y, uint8 color) {
	int16 xmax = MAX(x1, x2);
	int16 xmin = MIN(x1, x2);
	int w = xmax - xmin + 1;
	uint8 *p = _workPagePtr + y * OUT_GFX_W + xmin;
	while (w--) {
		*p++ = color;
	}
}

void Graphics::drawLineP(int16 x1, int16 x2, int16 y, uint8 color) {
	int16 xmax = MAX(x1, x2);
	int16 xmin = MIN(x1, x2);
	int w = xmax - xmin + 1;
	int off = y * OUT_GFX_W + xmin;
	uint8 *p = _workPagePtr + off;
	const uint8 *q = _pagePtrs[0] + off;
	while (w--) {
		*p++ = *q++;
	}
}

uint8 *Graphics::getPagePtr(uint8 page) {
	uint8 *p;
	if (page <= 3) {
		p = _pagePtrs[page];
	} else {
		switch (page) {
		case 0xFF:
			p = _backPagePtr;
			break;
		case 0xFE:
			p = _frontPagePtr;
			break;
		default:
			p = _pagePtrs[0];
			break;
		}
	}
	return p;
}

void Graphics::setWorkPagePtr(uint8 page) {
	_workPagePtr = getPagePtr(page);
}

void Graphics::fillPage(uint8 page, uint8 color) {
	uint8 *p = getPagePtr(page);
	memset(p, color, OUT_GFX_W * OUT_GFX_H);
}

void Graphics::copyPage(uint8 src, uint8 dst, int16 vscroll) {
	vscroll = vscroll * OUT_GFX_H / IN_GFX_H;
	if (src >= 0xFE || !((src &= 0xBF) & 0x80)) {
		uint8 *p = getPagePtr(src);
		uint8 *q = getPagePtr(dst);
		if (p != q) {
			memcpy(q, p, OUT_GFX_W * OUT_GFX_H);
		}		
	} else {
		uint8 *p = getPagePtr(src & 3);
		uint8 *q = getPagePtr(dst);
		if (p != q && vscroll > -OUT_GFX_H && vscroll < OUT_GFX_H) {
			uint16 h = OUT_GFX_H;
			if (vscroll < 0) {
				h += vscroll;
				p -= vscroll * OUT_GFX_W;
			} else {
				h -= vscroll;
				q += vscroll * OUT_GFX_W;
			}
			memcpy(q, p, h * OUT_GFX_W);
		}
	}
}

void Graphics::copyBitmapToPage(const uint8 *src) {
	uint8 *dst = _tempBitmap + IN_GFX_W + 1;
	int h = 200;
	while (h--) {
		int w = 40;
		while (w--) {
			uint8 p[] = {
				*(src + 8000 * 3),
				*(src + 8000 * 2),
				*(src + 8000 * 1),
				*(src + 8000 * 0)
			};
			for(int j = 0; j < 4; ++j) {
				uint8 acc = 0;
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
	bitmap_rescale(_pagePtrs[0], OUT_GFX_W, OUT_GFX_W, OUT_GFX_H, _tempBitmap + IN_GFX_W + 1, IN_GFX_W, IN_GFX_W, IN_GFX_H);
}

void Graphics::changePalette(uint8 palNum) {
	if (palNum < 32) {
		uint8 *p = _res->_dataPal + palNum * 32;
		Color pal[16];
		for (int i = 0; i < 16; ++i) {
			uint8 c1 = *p++;
			uint8 c2 = *p++;
			pal[i].r = ((c1 & 0x0F) << 2) | ((c1 & 0x0F) >> 2);
			pal[i].g = ((c2 & 0xF0) >> 2) | ((c2 & 0xF0) >> 6);
			pal[i].b = ((c2 & 0x0F) >> 2) | ((c2 & 0x0F) << 2);
		}
		_stub->setPalette(pal, 16);
		_curPal = palNum;
	}
}

void Graphics::updateDisplay(uint8 page) {
	if (page != 0xFE) {
		if (page == 0xFF) {
			SWAP(_frontPagePtr, _backPagePtr);
		} else {
			_frontPagePtr = getPagePtr(page);
		}
	}
	if (_newPal != 0xFF) {
		changePalette(_newPal);
		_newPal = 0xFF;
	}
	_stub->copyRect(_frontPagePtr, OUT_GFX_W);
}

void Graphics::saveOrLoad(Serializer &ser) {
	uint8 mask = 0;
	if (ser._mode == Serializer::SM_SAVE) {
		for (int i = 0; i < 4; ++i) {
			if (_pagePtrs[i] == _workPagePtr)
				mask |= i << 4;
			if (_pagePtrs[i] == _frontPagePtr)
				mask |= i << 2;
			if (_pagePtrs[i] == _backPagePtr)
				mask |= i << 0;
		}
	}
	Serializer::Entry entries[] = {
		SE_INT(&_curPal, Serializer::SES_INT8, VER(1)),
		SE_INT(&_newPal, Serializer::SES_INT8, VER(1)),
		SE_INT(&mask, Serializer::SES_INT8, VER(1)),
		SE_ARRAY(_pagePtrs[0], OUT_GFX_W * OUT_GFX_H, Serializer::SES_INT8, VER(1)),
		SE_ARRAY(_pagePtrs[1], OUT_GFX_W * OUT_GFX_H, Serializer::SES_INT8, VER(1)),
		SE_ARRAY(_pagePtrs[2], OUT_GFX_W * OUT_GFX_H, Serializer::SES_INT8, VER(1)),
		SE_ARRAY(_pagePtrs[3], OUT_GFX_W * OUT_GFX_H, Serializer::SES_INT8, VER(1)),
		SE_END()
	};
	ser.saveOrLoadEntries(entries);
	if (ser._mode == Serializer::SM_LOAD) {
		_workPagePtr = _pagePtrs[(mask >> 4) & 0x3];
		_frontPagePtr = _pagePtrs[(mask >> 2) & 0x3];
		_backPagePtr = _pagePtrs[(mask >> 0) & 0x3];
		changePalette(_curPal);
	}
}
