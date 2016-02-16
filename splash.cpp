
#include "file.h"
#include "graphics.h"
#include "scaler.h"
#include "systemstub.h"
#include "splash.h"


Splash::Splash(const char *dataPath, SystemStub *stub)
	: _dataPath(dataPath), _stub(stub), _pal(0), _pic(0) {
}

Splash::~Splash() {
	free(_pal);
	free(_pic);
}

void Splash::decodeData(char * fname) {
	File f;
	if (f.open(fname, _dataPath, "rb")) {
		uint32 sz = f.size();
		f.seek(4);
		int xmin = f.readUint16LE();
		int ymin = f.readUint16LE();
		int xmax = f.readUint16LE();
		int ymax = f.readUint16LE();
		int w = xmax - xmin + 1;
		int h = ymax - ymin + 1;
		uint8 *pcx = (uint8 *)malloc(w * h);
		if (pcx) {
			// decode image data
			f.seek(128);
			uint8 *p = pcx;
			for (int i = 0; i < h; ++i) {
				uint8 *scanline = p;
				while (scanline < p + w) {
					uint8 code = f.readByte();
					if ((code & 0xC0) == 0xC0) {
						uint8 count = code & 0x3F;
						memset(scanline, f.readByte(), count);
						scanline += count;
					} else {
						scanline[0] = code;
						++scanline;
					}
				}
				p += w;
			}
		}
		_pal = (Color *)malloc(sizeof(Color) * 256);
		if (_pal) {
			// get palette data
			f.seek(sz - 768);
			for (int i = 0; i < 256; ++i) {
				Color *col = &_pal[i];
				col->r = f.readByte() >> 2;
				col->g = f.readByte() >> 2;
				col->b = f.readByte() >> 2;
			}
		}
		uint16 dw = Graphics::OUT_GFX_W;
		uint16 dh = Graphics::OUT_GFX_H;
		_pic = (uint8 *)malloc(dw * dh);
		if (_pic && pcx) {
			bitmap_rescale(_pic, dw, dw, dh, pcx, w, w, h);
		}
		free(pcx);
	}
}

void Splash::handle(char *fname) {
	decodeData(fname);
	_stub->processEvents();
	if (_pic && _pal) {
		_stub->setPalette(_pal, 256);
		_stub->copyRect(_pic, Graphics::OUT_GFX_W);
		while (!_stub->pi.quit && !_stub->pi.button) {
			_stub->processEvents();
			_stub->sleep(100);
		}
		if (!_stub->pi.quit) {
			int stepCount = 7;
			while (stepCount--) {
				for (int i = 0; i < 256; ++i) {
					Color *col = &_pal[i];
					col->r >>= 1;
					col->g >>= 1;
					col->b >>= 1;
				}
				_stub->setPalette(_pal, 256);
				_stub->copyRect(_pic, Graphics::OUT_GFX_W);
				_stub->sleep(60);
			}
		}
	}
}
