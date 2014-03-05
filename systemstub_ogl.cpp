
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <SDL.h>
#include <SDL_opengl.h>
#include <vector>
#include "systemstub.h"


static bool hasExtension(const char *exts, const char *name) {
	const char *p = strstr(exts, name);
	if (p) {
		p += strlen(name);
		return *p == ' ' || *p == 0;
	}
	return false;
}

static int roundPow2(int sz) {
	if (sz != 0 && (sz & (sz - 1)) == 0) {
		return sz;
	}
	int textureSize = 1;
	while (textureSize < sz) {
		textureSize <<= 1;
	}
	return textureSize;
}

struct Texture {
	bool _npotTex;
	GLuint _id;
	int _w, _h;
	float _u, _v;
	uint8_t *_rgbData;
	const uint8_t *_raw16Data;

	void init();
	void uploadData(const uint8_t *data, int srcPitch, int w, int h, const Color *pal);
	void setPalette(const Color *pal);
	void draw(int w, int h);
	void clear();
	void readRaw16(const uint8_t *src, const Color *pal);
	void readBitmap(const uint8_t *src);
};

void Texture::init() {
	const char *exts = (const char *)glGetString(GL_EXTENSIONS);
	_npotTex = hasExtension(exts, "GL_ARB_texture_non_power_of_two");
	_id = (GLuint)-1;
	_w = _h = 0;
	_rgbData = 0;
	_raw16Data = 0;
}

static void convertTexture(const uint8_t *src, const int srcPitch, int w, int h, uint8_t *dst, int dstPitch, const Color *pal) {
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			const uint8_t color = src[x];
			int offset = x * 3;
			dst[offset++] = pal[color].r;
			dst[offset++] = pal[color].g;
			dst[offset++] = pal[color].b;
		}
		dst += dstPitch;
		src += srcPitch;
	}
}

void Texture::uploadData(const uint8_t *data, int srcPitch, int w, int h, const Color *pal) {
	if (w != _w || h != _h) {
		free(_rgbData);
		_rgbData = 0;
	}
	if (!_rgbData) {
		_w = _npotTex ? w : roundPow2(w);
		_h = _npotTex ? h : roundPow2(h);
		_rgbData = (uint8_t *)malloc(_w * _h * 3);
		if (!_rgbData) {
			return;
		}
		_u = w / (float)_w;
		_v = h / (float)_h;
		glGenTextures(1, &_id);
		glBindTexture(GL_TEXTURE_2D, _id);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		convertTexture(data, srcPitch, w, h, _rgbData, _w * 3, pal);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, _w, _h, 0, GL_RGB, GL_UNSIGNED_BYTE, _rgbData);
	} else {
		glBindTexture(GL_TEXTURE_2D, _id);
		convertTexture(data, srcPitch, w, h, _rgbData, _w * 3, pal);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _w, _h, GL_RGB, GL_UNSIGNED_BYTE, _rgbData);
	}
}

void Texture::setPalette(const Color *pal) {
	if (_raw16Data) {
		uploadData(_raw16Data, 320, 320, 200, pal);
	}
}

void Texture::draw(int w, int h) {
	if (_id != (GLuint) -1) {
		glEnable(GL_TEXTURE_2D);
		glColor4ub(255, 255, 255, 255);
		glBindTexture(GL_TEXTURE_2D, _id);
		glBegin(GL_QUADS);
			glTexCoord2f(0.f, 0.f);
			glVertex2i(0, h);
			glTexCoord2f(_u, 0.f);
			glVertex2i(w, h);
			glTexCoord2f(_u, _v);
			glVertex2i(w, 0);
			glTexCoord2f(0.f, _v);
			glVertex2i(0, 0);
		glEnd();
		glDisable(GL_TEXTURE_2D);
	}
}

void Texture::clear() {
	if (_id != (GLuint)-1) {
		glDeleteTextures(1, &_id);
		_id = (GLuint)-1;
	}
	free(_rgbData);
	_rgbData = 0;
	_raw16Data = 0;
}

void Texture::readRaw16(const uint8_t *src, const Color *pal) {
	_raw16Data = src;
	uploadData(_raw16Data, 320, 320, 200, pal);
}

void Texture::readBitmap(const uint8_t *src) {
	if (memcmp(src, "BM", 2) == 0) {
		const uint32_t imageOffset = READ_LE_UINT32(src + 0xA);
		const int w = READ_LE_UINT32(src + 0x12);
		const int h = READ_LE_UINT32(src + 0x16);
		const int depth = READ_LE_UINT16(src + 0x1C);
		const int compression = READ_LE_UINT32(src + 0x1E);
		const int colorsCount = READ_LE_UINT32(src + 0x2E);
		if (depth == 8 && compression == 0) {
			Color palette[colorsCount];
			const uint8_t *colorData = src + 0xE /* BITMAPFILEHEADER */ + 40 /* BITMAPINFOHEADER */;
			for (int i = 0; i < colorsCount; ++i) {
				palette[i].b = colorData[0];
				palette[i].g = colorData[1];
				palette[i].r = colorData[2];
				colorData += 4;
			}
			uploadData(src + imageOffset, (w + 3) & ~3, w, h, palette);
		} else {
			warning("Unsupported BMP depth=%d compression=%d", depth, compression);
		}
	}
}

struct DrawListEntry {
	uint8_t color;
	int numVertices;
	Point vertices[1024];
};

struct DrawList {
	typedef std::vector<DrawListEntry> Entries;
	
	int16_t vscroll;
	uint8_t color;
	Entries entries;
};

static const int SCREEN_W = 320;
static const int SCREEN_H = 200;

struct SystemStub_OGL : SystemStub {
	enum {
		DEF_SCREEN_W = 800,
		DEF_SCREEN_H = 600,
		SOUND_SAMPLE_RATE = 22050,
		NUM_LISTS = 4,
	};

	uint16_t _w, _h;
	SDL_Surface *_screen;
	bool _fullscreen;
	Color _pal[16];
	DrawList _drawLists[4];
	uint32_t _fontListIndex;
	Texture _backgroundTex;

	virtual ~SystemStub_OGL() {}
	virtual void init(const char *title);
	virtual void destroy();
	virtual void setFont(const uint8_t *font);
	virtual void setPalette(const Color *colors, uint8_t n);
	virtual void addBitmapToList(uint8_t listNum, const uint8_t *data);
	virtual void addPointToList(uint8_t listNum, uint8_t color, const Point *pt);
	virtual void addLineToList(uint8_t listNum, uint8_t color, const Point *pt1, const Point *pt2);
	virtual void addQuadStripToList(uint8_t listNum, uint8_t color, const QuadStrip *qs);
	virtual void addCharToList(uint8_t listNum, uint8_t color, char c, const Point *pt);
	virtual void clearList(uint8_t listNum, uint8_t color);
	virtual void copyList(uint8_t dstListNum, uint8_t srcListNum, int16_t vscroll = 0);
	virtual void blitList(uint8_t listNum);
	virtual void processEvents();
	virtual void sleep(uint32_t duration);
	virtual uint32_t getTimeStamp();
	virtual void startAudio(AudioCallback callback, void *param);
	virtual void stopAudio();
	virtual uint32_t getOutputSampleRate();
	virtual void lockAudio();
	virtual void unlockAudio();
	virtual void *addTimer(uint32_t delay, TimerCallback callback, void *param);
	virtual void removeTimer(void *timerId);
	virtual void *createMutex();
	virtual void destroyMutex(void *mutex);
	virtual void lockMutex(void *mutex);
	virtual void unlockMutex(void *mutex);

	uint16_t blitListEntries(uint8_t listNum, uint16_t offset);
	void resize(int w, int h);
	void switchGfxMode(bool fullscreen);
};

SystemStub *SystemStub_OGL_create() {
	return new SystemStub_OGL();
}

void SystemStub_OGL::init(const char *title) {
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_ShowCursor(SDL_DISABLE);
	SDL_WM_SetCaption(title, NULL);
	memset(&_pi, 0, sizeof(_pi));
	memset(&_drawLists, 0, sizeof(_drawLists));
	_fullscreen = false;
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);
	glEnable(GL_BLEND);
//	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	glShadeModel(GL_SMOOTH);
	glClearStencil(0);
	resize(DEF_SCREEN_W, DEF_SCREEN_H);
	_backgroundTex.init();
}

void SystemStub_OGL::destroy() {
	SDL_FreeSurface(_screen);
	SDL_Quit();
}

void SystemStub_OGL::setFont(const uint8_t *font) {
	_fontListIndex = glGenLists(96);
	for (int n = 0; n < 96; ++n) {
		const uint8_t *ft = font + n * 8;
		glNewList(_fontListIndex + n, GL_COMPILE);
#ifdef OLD_FONT_CODE
			glBegin(GL_POINTS);
			for (int j = 0; j < 8; ++j) {
				uint8_t ch = *(ft + j);
				for (int i = 0; i < 8; ++i) {
					if (ch & 0x80) {
						glVertex2i(i, 7 - j - 7);
					}
					ch <<= 1;
				}
			}
			glEnd();
#else
			int i, j;
			uint8_t charFont[64];
			for (j = 0; j < 8; ++j) {
				uint8_t ch = *(ft + j);
				for (i = 0; i < 8; ++i) {
					charFont[j * 8 + i] = (ch & 0x80);
					ch <<= 1;
				}
			}
			glBegin(GL_LINES);
			// look for horizontal lines
			for (j = 0; j < 8; ++j) {
				int px, cx;
				px = cx = -1;
				for (i = 0; i < 8; ++i) {
					if (charFont[j * 8 + i]) {
						if (cx != -1) {
							cx = i;
						} else {
							px = cx = i;
						}
					} else {
						if (cx != -1) {
							glVertex2i(px, -j);
							glVertex2i(cx, -j);
						}
						px = cx = -1;
					}
				}
			}
			// look for vertical lines
			for (i = 0; i < 8; ++i) {
				int py, cy;
				py = cy = -1;
				for (j = 0; j < 8; ++j) {
					if (charFont[j * 8 + i]) {
						if (cy != -1) {
							cy = j;
						} else {
							py = cy = j;
						}
					} else {
						if (cy != -1) {
							glVertex2i(i, -py);
							glVertex2i(i, -cy);
						}
						py = cy = -1;
					}
				}
			}
			// XXX look for diagonals
			glEnd();
#endif			
		glEndList();
	}
}

void SystemStub_OGL::setPalette(const Color *colors, uint8_t n) {
	assert(n <= 16);
	for (int i = 0; i < n; ++i) {
		const Color *c = &colors[i];
		_pal[i].r = (c->r << 2) | (c->r & 3);
		_pal[i].g = (c->g << 2) | (c->g & 3);
		_pal[i].b = (c->b << 2) | (c->b & 3);
	}
	_backgroundTex.setPalette(_pal);
}

void SystemStub_OGL::addBitmapToList(uint8_t listNum, const uint8_t *data) {
	assert(listNum == 0);
	if (memcmp(data, "BM", 2) == 0) {
		_backgroundTex.readBitmap(data);
	} else {
		_backgroundTex.readRaw16(data, _pal);
	}
	_drawLists[listNum].vscroll = 0;
	_drawLists[listNum].color = 0;
	_drawLists[listNum].entries.clear();
}

void SystemStub_OGL::addPointToList(uint8_t listNum, uint8_t color, const Point *pt) {
	assert(listNum < NUM_LISTS);
	DrawListEntry e;
	e.color = color;
	e.numVertices = 1;
	e.vertices[0] = *pt;
	_drawLists[listNum].entries.push_back(e);
}

void SystemStub_OGL::addLineToList(uint8_t listNum, uint8_t color, const Point *pt1, const Point *pt2) {
	assert(listNum < NUM_LISTS);
	DrawListEntry e;
	e.color = color;
	e.numVertices = 2;
	e.vertices[0] = *pt1;
	e.vertices[1] = *pt2;
	_drawLists[listNum].entries.push_back(e);
}

void SystemStub_OGL::addQuadStripToList(uint8_t listNum, uint8_t color, const QuadStrip *qs) {
	assert(listNum < NUM_LISTS);
	DrawListEntry e;
	e.color = color;
	e.numVertices = qs->numVertices;
	memcpy(e.vertices, qs->vertices, qs->numVertices * sizeof(Point));
	_drawLists[listNum].entries.push_back(e);
}

void SystemStub_OGL::addCharToList(uint8_t listNum, uint8_t color, char c, const Point *pt) {
	// TODO:
}

static void drawVertices(int count, const Point *vertices) {
	switch (count) {
	case 1:
		glBegin(GL_POINTS);
			glVertex2i(vertices[0].x, vertices[0].y);
		glEnd();
		break;
	case 2:
		glBegin(GL_LINES);
			glVertex2i(vertices[0].x, vertices[0].y);
			glVertex2i(vertices[1].x, vertices[1].y);
		glEnd();
		break;
	default:
		glBegin(GL_QUAD_STRIP);
			for (int i = 0; i < count / 2; ++i) {
				const int j = count - 1 - i;
				if (vertices[j].x > vertices[i].x) {
					glVertex2f(vertices[i].x, vertices[i].y);
					glVertex2f(vertices[j].x + 1, vertices[j].y);
				} else {
					glVertex2f(vertices[j].x, vertices[j].y);
					glVertex2f(vertices[i].x + 1, vertices[i].y);
				}
			}
		glEnd();
		break;
	}
}

void SystemStub_OGL::clearList(uint8_t listNum, uint8_t color) {
	assert(listNum < NUM_LISTS);
	_drawLists[listNum].vscroll = 0;
	_drawLists[listNum].color = color;
	_drawLists[listNum].entries.clear();
	if (listNum == 0) {
		_backgroundTex.clear();
	}
}

void SystemStub_OGL::copyList(uint8_t dstListNum, uint8_t srcListNum, int16_t vscroll) {
	assert(dstListNum < NUM_LISTS && srcListNum < NUM_LISTS);
	DrawList *dst = &_drawLists[dstListNum];
	DrawList *src = &_drawLists[srcListNum];
	dst->color = src->color;
	dst->entries = src->entries;
	dst->vscroll = -vscroll;
	if (dstListNum == 0) {
		_backgroundTex.clear();
	}
}

uint16_t SystemStub_OGL::blitListEntries(uint8_t listNum, uint16_t offset) {
	assert(listNum < NUM_LISTS);
	DrawList *dl = &_drawLists[listNum];
	assert(offset <= dl->entries.size());
	DrawList::Entries::const_iterator it = dl->entries.begin() + offset;
	for (; it != dl->entries.end(); ++it) {
		DrawListEntry entry = *it;
		if (entry.color != COL_PAGE) {
			Color *col;
			uint8_t a;
			if (entry.color == COL_ALPHA) {
				a = 128;
				col = &_pal[8]; // XXX
			} else {
				a = 255;
				col = &_pal[entry.color];
			}
			glColor4ub(col->r, col->g, col->b, a);
			drawVertices(entry.numVertices, entry.vertices);
		}
	}
	return dl->entries.size();
}

void SystemStub_OGL::blitList(uint8_t listNum) {
	assert(listNum < NUM_LISTS);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	DrawList *dl = &_drawLists[listNum];
	Color *col = &_pal[dl->color];
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTranslatef(0, dl->vscroll, 0);
	glClearColor(col->r / 255.f, col->g / 255.f, col->b / 255.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	_backgroundTex.draw(_w, _h);

	glScalef((float)_w / SCREEN_W, (float)_h / SCREEN_H, 1);

	DrawList::Entries::const_iterator it;
	bool needStencil = false;
//	for (it = dl->entries.begin(); it != dl->entries.end(); ++it) {
//		DrawListEntry entry = *it;
//		if (entry.color == COL_PAGE) {
//			needStencil = true;
//		}
//	}
	
	if (!needStencil) {
		blitListEntries(listNum, 0);
	} else {
		// 1st step : blit list
		blitListEntries(listNum, 0);
		// 2nd step : setup stencil buffer
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glEnable(GL_STENCIL_TEST);
		glStencilFunc(GL_ALWAYS, 1, 1);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);					
		for (it = dl->entries.begin(); it != dl->entries.end(); ++it) {
			DrawListEntry entry = *it;
			if (entry.color == COL_PAGE) {
//				glColor3f(1.f, 1.f, 0.f);
				drawVertices(entry.numVertices, entry.vertices);
			}
		}
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glStencilFunc(GL_EQUAL, 1, 1);
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
		// 3rd step : blit list 0
		blitListEntries(0, 0);
		glDisable(GL_STENCIL_TEST);
	}
	SDL_GL_SwapBuffers();
}

void SystemStub_OGL::processEvents() {
	SDL_Event ev;
	while(SDL_PollEvent(&ev)) {
		switch (ev.type) {
		case SDL_QUIT:
			_pi.quit = true;
			break;
		case SDL_VIDEORESIZE:
			resize(ev.resize.w, ev.resize.h);
			break;			
		case SDL_KEYUP:
			switch(ev.key.keysym.sym) {
			case SDLK_LEFT:
				_pi.dirMask &= ~PlayerInput::DIR_LEFT;
				break;
			case SDLK_RIGHT:
				_pi.dirMask &= ~PlayerInput::DIR_RIGHT;
				break;
			case SDLK_UP:
				_pi.dirMask &= ~PlayerInput::DIR_UP;
				break;
			case SDLK_DOWN:
				_pi.dirMask &= ~PlayerInput::DIR_DOWN;
				break;
			case SDLK_SPACE:
			case SDLK_RETURN:
				_pi.button = false;
				break;
			default:
				break;
			}
			break;
		case SDL_KEYDOWN:
			if (ev.key.keysym.mod & KMOD_ALT) {
				if (ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_KP_ENTER) {
					switchGfxMode(!_fullscreen);
				}
				break;
			} else if (ev.key.keysym.mod & KMOD_CTRL) {
				if (ev.key.keysym.sym == SDLK_s) {
					_pi.save = true;
				} else if (ev.key.keysym.sym == SDLK_l) {
					_pi.load = true;
				} else if (ev.key.keysym.sym == SDLK_f) {
					_pi.fastMode = true;
				} else if (ev.key.keysym.sym == SDLK_KP_PLUS) {
					_pi.stateSlot = 1;
				} else if (ev.key.keysym.sym == SDLK_KP_MINUS) {
					_pi.stateSlot = -1;
				}
				break;
			}
			_pi.lastChar = ev.key.keysym.sym;
			switch(ev.key.keysym.sym) {
			case SDLK_LEFT:
				_pi.dirMask |= PlayerInput::DIR_LEFT;
				break;
			case SDLK_RIGHT:
				_pi.dirMask |= PlayerInput::DIR_RIGHT;
				break;
			case SDLK_UP:
				_pi.dirMask |= PlayerInput::DIR_UP;
				break;
			case SDLK_DOWN:
				_pi.dirMask |= PlayerInput::DIR_DOWN;
				break;
			case SDLK_SPACE:
			case SDLK_RETURN:
				_pi.button = true;
				break;
			case SDLK_c:
				_pi.code = true;
				break;
			case SDLK_p:
				_pi.pause = true;
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
	}
}

void SystemStub_OGL::sleep(uint32_t duration) {
	SDL_Delay(duration);
}

uint32_t SystemStub_OGL::getTimeStamp() {
	return SDL_GetTicks();
}

void SystemStub_OGL::startAudio(AudioCallback callback, void *param) {
	SDL_AudioSpec desired;
	memset(&desired, 0, sizeof(desired));
	desired.freq = SOUND_SAMPLE_RATE;
	desired.format = AUDIO_S8;
	desired.channels = 1;
	desired.samples = 2048;
	desired.callback = callback;
	desired.userdata = param;
	if (SDL_OpenAudio(&desired, NULL) == 0) {
		SDL_PauseAudio(0);
	} else {
		error("SystemStub_OGL::startAudio() unable to open sound device");
	}
}

void SystemStub_OGL::stopAudio() {
	SDL_CloseAudio();
}

uint32_t SystemStub_OGL::getOutputSampleRate() {
	return SOUND_SAMPLE_RATE;
}

void SystemStub_OGL::lockAudio() {
	SDL_LockAudio();
}

void SystemStub_OGL::unlockAudio() {
	SDL_UnlockAudio();
}

void *SystemStub_OGL::addTimer(uint32_t delay, TimerCallback callback, void *param) {
	return SDL_AddTimer(delay, (SDL_NewTimerCallback)callback, param);
}

void SystemStub_OGL::removeTimer(void *timerId) {
	SDL_RemoveTimer((SDL_TimerID)timerId);
}

void *SystemStub_OGL::createMutex() {
	return SDL_CreateMutex();
}

void SystemStub_OGL::destroyMutex(void *mutex) {
	SDL_DestroyMutex((SDL_mutex *)mutex);
}

void SystemStub_OGL::lockMutex(void *mutex) {
	SDL_mutexP((SDL_mutex *)mutex);
}

void SystemStub_OGL::unlockMutex(void *mutex) {
	SDL_mutexV((SDL_mutex *)mutex);
}

void SystemStub_OGL::resize(int w, int h) {
	_w = w;
	_h = h;

	_screen = SDL_SetVideoMode(_w, _h, 0, SDL_OPENGL | (_fullscreen ? SDL_FULLSCREEN : SDL_RESIZABLE));
	glViewport(0, 0, _w, _h);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, _w, _h, 0, 0, 1);

	float r = (float)w / SCREEN_W;
	glLineWidth(r);
	glPointSize(r);	
}

void SystemStub_OGL::switchGfxMode(bool fullscreen) {
	_fullscreen = fullscreen;
	uint32_t flags = _fullscreen ? SDL_FULLSCREEN : SDL_RESIZABLE;
	_screen = SDL_SetVideoMode(_w, _h, 0, SDL_OPENGL | flags);
}
