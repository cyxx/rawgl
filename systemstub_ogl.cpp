
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <SDL.h>
#include <GL/glew.h>
#include <vector>
#include "graphics.h"
#include "scaler.h"
#include "systemstub.h"

static int _render = RENDER_GL;
static int _newRender = _render;
static bool _canChangeRender;

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
	bool _alpha;
	GLuint _id;
	int _w, _h;
	float _u, _v;
	uint8_t *_rgbData;
	const uint8_t *_raw16Data;

	void init();
	void uploadData(const uint8_t *data, int srcPitch, int w, int h, const Color *pal);
	void draw(int w, int h);
	void clear();
	void readRaw16(const uint8_t *src, const Color *pal, int w = 320, int h = 200);
};

void Texture::init() {
	const char *exts = (const char *)glGetString(GL_EXTENSIONS);
	_npotTex = hasExtension(exts, "GL_ARB_texture_non_power_of_two");
	_alpha = false;
	_id = (GLuint)-1;
	_w = _h = 0;
	_rgbData = 0;
	_raw16Data = 0;
}

static void convertTexture(const uint8_t *src, const int srcPitch, int w, int h, uint8_t *dst, int dstPitch, const Color *pal, bool alpha) {
	for (int y = 0; y < h; ++y) {
		int offset = 0;
		for (int x = 0; x < w; ++x) {
			const uint8_t color = src[x];
			dst[offset++] = pal[color].r;
			dst[offset++] = pal[color].g;
			dst[offset++] = pal[color].b;
			if (alpha) {
				dst[offset++] = (color == 0) ? 0 : 255;
			}
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
	const int depth = _alpha ? 4 : 3;
	const int fmt = _alpha ? GL_RGBA : GL_RGB;
	if (!_rgbData) {
		_w = _npotTex ? w : roundPow2(w);
		_h = _npotTex ? h : roundPow2(h);
		_rgbData = (uint8_t *)malloc(_w * _h * depth);
		if (!_rgbData) {
			return;
		}
		_u = w / (float)_w;
		_v = h / (float)_h;
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glGenTextures(1, &_id);
		glBindTexture(GL_TEXTURE_2D, _id);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (_render == RENDER_ORIGINAL) ? GL_NEAREST : GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (_render == RENDER_ORIGINAL) ? GL_NEAREST : GL_LINEAR);
		convertTexture(data, srcPitch, w, h, _rgbData, _w * depth, pal, _alpha);
		glTexImage2D(GL_TEXTURE_2D, 0, fmt, _w, _h, 0, fmt, GL_UNSIGNED_BYTE, _rgbData);
	} else {
		glBindTexture(GL_TEXTURE_2D, _id);
		convertTexture(data, srcPitch, w, h, _rgbData, _w * depth, pal, _alpha);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _w, _h, fmt, GL_UNSIGNED_BYTE, _rgbData);
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

void Texture::readRaw16(const uint8_t *src, const Color *pal, int w, int h) {
	_raw16Data = src;
	uploadData(_raw16Data, w, w, h, pal);
}

struct BitmapInfo {
	int _w, _h;
	Color _palette[256];
	const uint8_t *_data;

	void read(const uint8_t *src);
};

void BitmapInfo::read(const uint8_t *src) {
	const uint32_t imageOffset = READ_LE_UINT32(src + 0xA);
	_w = READ_LE_UINT32(src + 0x12);
	_h = READ_LE_UINT32(src + 0x16);
	const int depth = READ_LE_UINT16(src + 0x1C);
	const int compression = READ_LE_UINT32(src + 0x1E);
	const int colorsCount = READ_LE_UINT32(src + 0x2E);
	if (depth == 8 && compression == 0) {
		const uint8_t *colorData = src + 0xE /* BITMAPFILEHEADER */ + 40 /* BITMAPINFOHEADER */;
		for (int i = 0; i < colorsCount; ++i) {
			_palette[i].b = colorData[0];
			_palette[i].g = colorData[1];
			_palette[i].r = colorData[2];
			colorData += 4;
		}
		_data = src + imageOffset;
	} else {
		_data = 0;
		warning("Unsupported BMP depth=%d compression=%d", depth, compression);
	}
}

struct DrawListEntry {
	static const int NUM_VERTICES = 1024;

	int color;
	int numVertices;
	Point vertices[NUM_VERTICES];
};

struct DrawList {
	typedef std::vector<DrawListEntry> Entries;

	int fillColor;
	Entries entries;

	DrawList()
		: fillColor(0) {
	}

	void clear(uint8_t color) {
		fillColor = color;
		entries.clear();
	}

	void append(uint8_t color, int count, const Point *vertices) {
		if (count <= DrawListEntry::NUM_VERTICES) {
			DrawListEntry e;
			e.color = color;
			e.numVertices = count;
			memcpy(e.vertices, vertices, count * sizeof(Point));
			entries.push_back(e);
		}
	}
};

static const int SCREEN_W = 320;
static const int SCREEN_H = 200;

static const int FB_W = 1280;
static const int FB_H = 800;

struct SystemStub_OGL : SystemStub {
	enum {
		DEF_SCREEN_W = 800,
		DEF_SCREEN_H = 600,
		SOUND_SAMPLE_RATE = 22050,
		NUM_LISTS = 4,
	};

	uint16_t _w, _h;
	bool _fullscreen;
	Color _pal[16];
	Graphics _gfx;
	Texture _backgroundTex;
	Texture _fontTex;
	Texture _spritesTex;
	int _spritesSizeX, _spritesSizeY;
	GLuint _fbPage0;
	GLuint _pageTex[NUM_LISTS];
	DrawList _drawLists[NUM_LISTS];

	virtual ~SystemStub_OGL() {}
	virtual void init(const char *title);
	virtual void destroy();
	virtual void setFont(const uint8_t *font);
	virtual void setPalette(const Color *colors, uint8_t n);
	virtual void setSpriteAtlas(const uint8_t *src, int xSize, int ySize);
	virtual void addSpriteToList(uint8_t listNum, int num, const Point *pt);
	virtual void addBitmapToList(uint8_t listNum, const uint8_t *data);
	virtual void addPointToList(uint8_t listNum, uint8_t color, const Point *pt);
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

	bool initFBO();
	void drawVertices(int count, const Point *vertices);
	void drawVerticesToFb(uint8_t color, int count, const Point *vertices);
	void resize(int w, int h);
	void switchGfxMode(bool fullscreen);
};

SystemStub *SystemStub_OGL_create(const char *name) {
	if (name) {
		struct {
			const char *name;
			int render;
		} modes[] = {
			{ "original", RENDER_ORIGINAL },
			{ "gl", RENDER_GL },
			{ 0, 0 }
		};
		for (int i = 0; modes[i].name; ++i) {
			if (strcasecmp(modes[i].name, name) == 0) {
				_render = modes[i].render;
				_newRender = _render;
				break;
			}
		}
	}
	return new SystemStub_OGL();
}

void SystemStub_OGL::init(const char *title) {
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_ShowCursor(SDL_DISABLE);
	SDL_WM_SetCaption(title, NULL);
	memset(&_pi, 0, sizeof(_pi));
	_fullscreen = false;
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);
	resize(DEF_SCREEN_W, DEF_SCREEN_H);
	GLenum ret = glewInit();
	if (ret != GLEW_OK) {
		error("glewInit() returned %d", ret);
	}
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glShadeModel(GL_SMOOTH);
	_backgroundTex.init();
	_fontTex.init();
	_fontTex._alpha = true;
	_spritesTex.init();
	_spritesTex._alpha = true;
	if (GLEW_ARB_framebuffer_object) {
		const bool err = initFBO();
		if (err) {
			_canChangeRender = true;
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			return;
		}
	}
	warning("Error initializing GL FBO, using default renderer");
	_canChangeRender = false;
	_render = RENDER_ORIGINAL;
}

bool SystemStub_OGL::initFBO() {

	// TODO: check extension availability GL_EXT_framebuffer_object

	GLint buffersCount;
	glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &buffersCount);
	if (buffersCount < NUM_LISTS) {
		warning("GL_MAX_COLOR_ATTACHMENTS is %d", buffersCount);
		return false;
	}

	glGenFramebuffersEXT(1, &_fbPage0);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _fbPage0);

	glGenTextures(NUM_LISTS, _pageTex);
	for (int i = 0; i < NUM_LISTS; ++i) {
		glBindTexture(GL_TEXTURE_2D, _pageTex[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, FB_W, FB_H, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT + i, GL_TEXTURE_2D, _pageTex[i], 0);
		int status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
		if (status != GL_FRAMEBUFFER_COMPLETE_EXT) {
			warning("glCheckFramebufferStatusEXT failed, ret %d\n", status);
			return false;
		}
	}

	glViewport(0, 0, FB_W, FB_H);

	const float r = (float)FB_W / SCREEN_W;
	glLineWidth(r);
	glPointSize(r);

	return true;
}

void SystemStub_OGL::destroy() {
	SDL_Quit();
}

void SystemStub_OGL::setFont(const uint8_t *font) {
	if (memcmp(font, "BM", 2) == 0) {
		BitmapInfo bi;
		bi.read(font);
		if (bi._data) {
			_fontTex.uploadData(bi._data, (bi._w + 3) & ~3, bi._w, bi._h, bi._palette);
		}
	}
}

void SystemStub_OGL::setPalette(const Color *colors, uint8_t n) {
	assert(n <= 16);
	for (int i = 0; i < n; ++i) {
		_pal[i] = colors[i];
	}

	if (g_fixUpPalette == FIXUP_PALETTE_RENDER && _render == RENDER_GL) {
		for (int i = 0; i < NUM_LISTS; ++i) {
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _fbPage0);
			glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + i);

			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glOrtho(0, FB_W, 0, FB_H, 0, 1);

			const int color = _drawLists[i].fillColor;
			if (color != COL_BMP) {
				assert(color < 16);
				glClearColor(_pal[color].r / 255.f, _pal[color].g / 255.f, _pal[color].b / 255.f, 1.f);
				glClear(GL_COLOR_BUFFER_BIT);
			}

			glScalef((float)FB_W / SCREEN_W, (float)FB_H / SCREEN_H, 1);

			DrawList::Entries::const_iterator it = _drawLists[i].entries.begin();
			for (; it != _drawLists[i].entries.end(); ++it) {
				DrawListEntry e = *it;
				if (e.color < 16) {
					glColor4ub(_pal[e.color].r, _pal[e.color].g, _pal[e.color].b, 255);
					drawVertices(e.numVertices, e.vertices);
				}
			}

			glLoadIdentity();
			glScalef(1., 1., 1.);
		}
	}
}

void SystemStub_OGL::setSpriteAtlas(const uint8_t *src, int xSize, int ySize) {
	if (memcmp(src, "BM", 2) == 0) {
		BitmapInfo bi;
		bi.read(src);
		if (bi._data) {
			_spritesTex.uploadData(bi._data, (bi._w + 3) & ~3, bi._w, bi._h, bi._palette);
		}
	}
	_spritesSizeX = xSize * 2;
	_spritesSizeY = ySize * 2;
}

static void drawSprite(const int *pos, const float *uv, GLuint tex) {
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tex);
	glBegin(GL_QUADS);
		glTexCoord2f(uv[0], uv[1]);
		glVertex2i(pos[0], pos[1]);
		glTexCoord2f(uv[2], uv[1]);
		glVertex2i(pos[2], pos[1]);
		glTexCoord2f(uv[2], uv[3]);
		glVertex2i(pos[2], pos[3]);
		glTexCoord2f(uv[0], uv[3]);
		glVertex2i(pos[0], pos[3]);
	glEnd();
	glDisable(GL_TEXTURE_2D);
}

void SystemStub_OGL::addSpriteToList(uint8_t listNum, int num, const Point *pt) {
	assert(listNum < NUM_LISTS);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _fbPage0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + listNum);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, FB_W, 0, FB_H, 0, 1);

	glScalef((float)FB_W / SCREEN_W, (float)FB_H / SCREEN_H, 1);

	glColor4ub(255, 255, 255, 255);
	const int xSize = 9;
	const int ySize = FB_H * xSize / FB_H;
	const int pos[4] = {
		pt->x, pt->y,
		pt->x + xSize, pt->y + ySize
	};
	int u = num % _spritesSizeX;
	int v = num / _spritesSizeY;
	const float uv[4] = {
		u * 1. / _spritesSizeX, 1. - v * 1. / _spritesSizeY,
		(u + 1) * 1. / _spritesSizeX, 1. - (v + 1) * 1. / _spritesSizeY
	};
	drawSprite(pos, uv, _spritesTex._id);

	glLoadIdentity();
	glScalef(1., 1., 1.);
}

void SystemStub_OGL::addBitmapToList(uint8_t listNum, const uint8_t *data) {
	switch (_render) {
	case RENDER_ORIGINAL:
		if (memcmp(data, "BM", 2) == 0) {
			BitmapInfo bi;
			bi.read(data);
			if (bi._w == 320 && bi._h == 200 && bi._data) {
				for (int y = 0; y < 200; ++y) {
					memcpy(_gfx.getPagePtr(listNum) + y * _gfx._w, bi._data + (199 - y) * _gfx._w, 320);
				}
			}
		} else {
			memcpy(_gfx.getPagePtr(listNum), data, _gfx.getPageSize());
		}
		break;
	case RENDER_GL:
		assert(listNum == 0);
		if (memcmp(data, "BM", 2) == 0) {
			BitmapInfo bi;
			bi.read(data);
			if (bi._data) {
				_backgroundTex.uploadData(bi._data, (bi._w + 3) & ~3, bi._w, bi._h, bi._palette);
			}
		} else {
			_backgroundTex.readRaw16(data, _pal);
		}
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _fbPage0);
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + listNum);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, FB_W, 0, FB_H, 0, 1);

		_backgroundTex.draw(FB_W, FB_H);
		break;
	}

	_drawLists[listNum].clear(COL_BMP);
}

static void drawBackgroundTexture(int count, const Point *vertices);
static void drawFontChar(uint8_t ch, int x, int y, GLuint tex);

void SystemStub_OGL::drawVerticesToFb(uint8_t color, int count, const Point *vertices) {
	glScalef((float)FB_W / SCREEN_W, (float)FB_H / SCREEN_H, 1);

	if (color == COL_PAGE) {
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D,  _pageTex[0]);
		glColor4f(1., 1., 1., 1.);
		drawBackgroundTexture(count, vertices);
		glDisable(GL_TEXTURE_2D);
	} else {
		if (color == COL_ALPHA) {
			glColor4ub(_pal[8].r, _pal[8].g, _pal[8].b, 192);
		} else {
			assert(color < 16);
			glColor4ub(_pal[color].r, _pal[color].g, _pal[color].b, 255);
		}
		drawVertices(count, vertices);
	}

	glLoadIdentity();
	glScalef(1., 1., 1.);
}

void SystemStub_OGL::addPointToList(uint8_t listNum, uint8_t color, const Point *pt) {
	_gfx.setWorkPagePtr(listNum);
	_gfx.drawPoint(pt->x, pt->y, color);

	assert(listNum < NUM_LISTS);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _fbPage0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + listNum);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, FB_W, 0, FB_H, 0, 1);

	drawVerticesToFb(color, 1, pt);

	_drawLists[listNum].append(color, 1, pt);
}

void SystemStub_OGL::addQuadStripToList(uint8_t listNum, uint8_t color, const QuadStrip *qs) {
	_gfx.setWorkPagePtr(listNum);
	_gfx.drawPolygon(color, *qs);

	assert(listNum < NUM_LISTS);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _fbPage0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + listNum);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, FB_W, 0, FB_H, 0, 1);

	drawVerticesToFb(color, qs->numVertices, qs->vertices);

	_drawLists[listNum].append(color, qs->numVertices, qs->vertices);
}

void SystemStub_OGL::addCharToList(uint8_t listNum, uint8_t color, char c, const Point *pt) {
	_gfx.setWorkPagePtr(listNum);
	_gfx.drawChar(c, pt->x / 8, pt->y, color);

	if (!_fontTex._rgbData) {
		return;
	}

	assert(listNum < NUM_LISTS);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _fbPage0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + listNum);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, FB_W, 0, FB_H, 0, 1);

	glScalef((float)FB_W / SCREEN_W, (float)FB_H / SCREEN_H, 1);

	glColor4ub(_pal[color].r, _pal[color].g, _pal[color].b, 255);
	drawFontChar(c, pt->x, pt->y, _fontTex._id);

	glLoadIdentity();
	glScalef(1., 1., 1.);

	// TODO: _drawLists
}

void SystemStub_OGL::drawVertices(int count, const Point *vertices) {
	switch (count) {
	case 1:
		glBegin(GL_POINTS);
			glVertex2i(vertices[0].x, vertices[0].y);
		glEnd();
		break;
	case 2:
		glBegin(GL_LINES);
			if (vertices[1].x > vertices[0].x) {
				glVertex2i(vertices[0].x, vertices[0].y);
				glVertex2i(vertices[1].x + 1, vertices[1].y);
			} else {
				glVertex2i(vertices[1].x, vertices[1].y);
				glVertex2i(vertices[0].x + 1, vertices[0].y);
			}
		glEnd();
		break;
	default:
		glBegin(GL_QUAD_STRIP);
			for (int i = 0; i < count / 2; ++i) {
				const int j = count - 1 - i;
				if (vertices[j].x > vertices[i].x) {
					glVertex2i(vertices[i].x, vertices[i].y);
					glVertex2i(vertices[j].x + 1, vertices[j].y);
				} else {
					glVertex2i(vertices[j].x, vertices[j].y);
					glVertex2i(vertices[i].x + 1, vertices[i].y);
				}
			}
		glEnd();
		break;
	}
}

static void drawBackgroundTexture(int count, const Point *vertices) {
	if (count < 4) {
		warning("Invalid vertices count for drawing mode 0x11", count);
		return;
	}
	glBegin(GL_QUAD_STRIP);
		for (int i = 0; i < count / 2; ++i) {
			const int j = count - 1 - i;
			int v1 = i;
			int v2 = j;
			if (vertices[v2].x < vertices[v1].x) {
				SWAP(v1, v2);
			}
			glTexCoord2f(vertices[v1].x / 320., vertices[v1].y / 200.);
			glVertex2i(vertices[v1].x, vertices[v1].y);
			glTexCoord2f((vertices[v2].x + 1) / 320., vertices[v2].y / 200.);
			glVertex2i((vertices[v2].x + 1), vertices[v2].y);
		}
	glEnd();
}

void SystemStub_OGL::clearList(uint8_t listNum, uint8_t color) {
	memset(_gfx.getPagePtr(listNum), color, _gfx.getPageSize());

	assert(listNum < NUM_LISTS);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _fbPage0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + listNum);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, FB_W, 0, FB_H, 0, 1);

	assert(color < 16);
	glClearColor(_pal[color].r / 255.f, _pal[color].g / 255.f, _pal[color].b / 255.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);

	_drawLists[listNum].clear(color);
}

static void drawTextureFb(GLuint tex, int w, int h, int vscroll) {
	glEnable(GL_TEXTURE_2D);
	glColor4ub(255, 255, 255, 255);
	glBindTexture(GL_TEXTURE_2D, tex);
	glBegin(GL_QUADS);
		glTexCoord2f(0, 0);
		glVertex2i(0, vscroll);
		glTexCoord2f(1, 0);
		glVertex2i(w, vscroll);
		glTexCoord2f(1, 1);
		glVertex2i(w, h + vscroll);
		glTexCoord2f(0, 1);
		glVertex2i(0, h + vscroll);
	glEnd();
	glDisable(GL_TEXTURE_2D);
}

void SystemStub_OGL::copyList(uint8_t dstListNum, uint8_t srcListNum, int16_t vscroll) {
	if (vscroll == 0) {
		memcpy(_gfx.getPagePtr(dstListNum), _gfx.getPagePtr(srcListNum), _gfx.getPageSize());
	} else if (vscroll >= -199 && vscroll <= 199) {
		const int dy = vscroll * _gfx._h / Graphics::GFX_H;
		if (dy < 0) {
			memcpy(_gfx.getPagePtr(dstListNum), _gfx.getPagePtr(srcListNum) - dy * _gfx._w, (_gfx._h + dy) * _gfx._w);
		} else {
			memcpy(_gfx.getPagePtr(dstListNum) + dy * _gfx._w, _gfx.getPagePtr(srcListNum), (_gfx._h - dy) * _gfx._w);
		}
	}

	assert(dstListNum < NUM_LISTS && srcListNum < NUM_LISTS);

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _fbPage0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + dstListNum);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, FB_W, 0, FB_H, 0, 1);

	const int yoffset = vscroll * FB_H / SCREEN_H;
	drawTextureFb(_pageTex[srcListNum], FB_W, FB_H, yoffset);

	_drawLists[dstListNum] = _drawLists[srcListNum];
}

static void drawFontChar(uint8_t ch, int x, int y, GLuint tex) {
	const int x1 = x;
	const int x2 = x1 + 8;
	const int y1 = y;
	const int y2 = y1 + 8;
	const float u1 = (ch % 16) * 16 / 256.;
	const float u2 = u1 + 16 / 256.;
	const float v1 = (16 - ch / 16) * 16 / 256.;
	const float v2 = v1 - 16 / 256.;
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tex);
	glBegin(GL_QUADS);
		glTexCoord2f(u1, v1);
		glVertex2i(x1, y1);
		glTexCoord2f(u2, v1);
		glVertex2i(x2, y1);
		glTexCoord2f(u2, v2);
		glVertex2i(x2, y2);
		glTexCoord2f(u1, v2);
		glVertex2i(x1, y2);
	glEnd();
	glDisable(GL_TEXTURE_2D);
}

void SystemStub_OGL::blitList(uint8_t listNum) {
	assert(listNum < NUM_LISTS);

	switch (_render){
	case RENDER_ORIGINAL:
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

		glViewport(0, 0, _w, _h);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, _w, 0, _h, 0, 1);

		_backgroundTex.readRaw16(_gfx.getPagePtr(listNum), _pal);
		_backgroundTex.draw(_w, _h);
		break;
	case RENDER_GL:
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

		glPushAttrib(GL_VIEWPORT_BIT);
		glViewport(0, 0, _w, _h);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, _w, _h, 0, 0, 1);

		drawTextureFb(_pageTex[listNum], _w, _h, 0);

		glLoadIdentity();
		glOrtho(0, FB_W, 0, FB_H, 0, 1);

		glPopAttrib();
		break;
	}

	SDL_GL_SwapBuffers();

	if (_newRender != _render) {
		_render = _newRender;
		if (_render == RENDER_GL) {
			glViewport(0, 0, FB_W, FB_H);
		}
	}
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
				} else if (ev.key.keysym.sym == SDLK_r) {
					if (!_canChangeRender) {
						break;
					}
					switch (_render) {
					case RENDER_ORIGINAL:
						_newRender = RENDER_GL;
						break;
					case RENDER_GL:
						_newRender = RENDER_ORIGINAL;
						break;
					}
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
	SDL_SetVideoMode(_w, _h, 0, SDL_OPENGL | (_fullscreen ? SDL_FULLSCREEN : SDL_RESIZABLE));
}

void SystemStub_OGL::switchGfxMode(bool fullscreen) {
	_fullscreen = fullscreen;
	uint32_t flags = _fullscreen ? SDL_FULLSCREEN : SDL_RESIZABLE;
	SDL_SetVideoMode(_w, _h, 0, SDL_OPENGL | flags);
}
