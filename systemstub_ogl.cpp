
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <SDL.h>
#include <GL/glew.h>
#include <math.h>
#include <vector>
#include "file.h"
#include "graphics.h"
#include "systemstub.h"

static int _render = RENDER_GL;
static int _newRender = _render;
static bool _canChangeRender;

static GLuint kNoTextureId = (GLuint)-1;

struct Palette {
	static const int COLORS_COUNT = 32;
	uint8_t buf[COLORS_COUNT * 3];

	GLuint _id;

	void init();
	void uploadData(const Color *colors, int count);
};

void Palette::init() {
	_id = kNoTextureId;
}

void Palette::uploadData(const Color *colors, int count) {
	if (_id == kNoTextureId) {
		glGenTextures(1, &_id);
		glBindTexture(GL_TEXTURE_1D, _id);
		glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB8, COLORS_COUNT, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	} else {
		glBindTexture(GL_TEXTURE_1D, _id);
	}
	if (count != 16) {
		warning("Palette::uploadData() Unhandled colors count %d", count);
		return;
	}
	uint8_t *p = buf;
	for (int i = 0; i < COLORS_COUNT; ++i) {
		p[0] = colors[i].r;
		p[1] = colors[i].g;
		p[2] = colors[i].b;
		p += 3;
	}
	glTexSubImage1D(GL_TEXTURE_1D, 0, 0, COLORS_COUNT, GL_RGB, GL_UNSIGNED_BYTE, buf);
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
	int _fmt;

	void init();
	void uploadDataCLUT(const uint8_t *data, int srcPitch, int w, int h, const Color *pal);
	void uploadDataRGB(const void *data, int srcPitch, int w, int h, int fmt, int type);
	void draw(int w, int h);
	void clear();
	void readRaw16(const uint8_t *src, const Color *pal, int w = 320, int h = 200);
	void readFont(const uint8_t *src);
};

void Texture::init() {
	_npotTex = GLEW_ARB_texture_non_power_of_two;
	_id = kNoTextureId;
	_w = _h = 0;
	_u = _v = 0.f;
	_rgbData = 0;
	_raw16Data = 0;
	_fmt = -1;
}

static void convertTexture16(const uint8_t *src, const int srcPitch, int w, int h, uint8_t *dst, int dstPitch) {
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			dst[x] = src[x] * 256 / 16;
		}
		dst += dstPitch;
		src += srcPitch;
	}
}

static void convertTextureCLUT(const uint8_t *src, const int srcPitch, int w, int h, uint8_t *dst, int dstPitch, const Color *pal, bool alpha) {
	for (int y = 0; y < h; ++y) {
		int offset = 0;
		for (int x = 0; x < w; ++x) {
			const uint8_t color = src[x];
			dst[offset++] = pal[color].r;
			dst[offset++] = pal[color].g;
			dst[offset++] = pal[color].b;
			if (alpha) {
				if (color == 0) {
					dst[offset++] = 0;
				} else {
					dst[offset++] = 255;
				}
			}
		}
		dst += dstPitch;
		src += srcPitch;
	}
}

void Texture::uploadDataCLUT(const uint8_t *data, int srcPitch, int w, int h, const Color *pal) {
	if (w != _w || h != _h) {
		free(_rgbData);
		_rgbData = 0;
	}
	int depth = 1;
	int fmt = GL_RGB;
	int type = GL_UNSIGNED_BYTE;
	switch (_fmt) {
	case FMT_CLUT:
		if (g_fixUpPalette == FIXUP_PALETTE_SHADER) {
			depth = 1;
			fmt = GL_RED;
		} else {
			depth = 3;
			fmt = GL_RGB;
		}
		break;
	case FMT_RGB:
		depth = 3;
		fmt = GL_RGB;
		break;
	case FMT_RGBA:
		depth = 4;
		fmt = GL_RGBA;
		break;
	default:
		return;
	}
	const bool alpha = (_fmt == FMT_RGBA);
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
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (_render == RENDER_ORIGINAL) || (fmt == GL_RED) ? GL_NEAREST : GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (_render == RENDER_ORIGINAL) || (fmt == GL_RED) ? GL_NEAREST : GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		if (fmt == GL_RED) {
			convertTexture16(data, srcPitch, w, h, _rgbData, _w);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, _w, _h, 0, fmt, type, _rgbData);
		} else {
			convertTextureCLUT(data, srcPitch, w, h, _rgbData, _w * depth, pal, alpha);
			glTexImage2D(GL_TEXTURE_2D, 0, fmt, _w, _h, 0, fmt, type, _rgbData);
		}
	} else {
		glBindTexture(GL_TEXTURE_2D, _id);
		convertTextureCLUT(data, srcPitch, w, h, _rgbData, _w * depth, pal, alpha);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _w, _h, fmt, type, _rgbData);
	}
}

void Texture::uploadDataRGB(const void *data, int srcPitch, int w, int h, int fmt, int type) {
	_w = w;
	_h = h;
	_u = 1.f;
	_v = 1.f;
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glGenTextures(1, &_id);
	glBindTexture(GL_TEXTURE_2D, _id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_2D, 0, fmt, _w, _h, 0, fmt, type, data);
}

void Texture::draw(int w, int h) {
	if (_id != kNoTextureId) {
		glEnable(GL_TEXTURE_2D);
		glColor4ub(255, 255, 255, 255);
		glBindTexture(GL_TEXTURE_2D, _id);
		glBegin(GL_QUADS);
			glTexCoord2f(0.f, 0.f);
			glVertex2i(0, 0);
			glTexCoord2f(_u, 0.f);
			glVertex2i(w, 0);
			glTexCoord2f(_u, _v);
			glVertex2i(w, h);
			glTexCoord2f(0.f, _v);
			glVertex2i(0, h);
		glEnd();
		glDisable(GL_TEXTURE_2D);
	}
}

void Texture::clear() {
	if (_id != kNoTextureId) {
		glDeleteTextures(1, &_id);
		_id = kNoTextureId;
	}
	free(_rgbData);
	_rgbData = 0;
	_raw16Data = 0;
}

void Texture::readRaw16(const uint8_t *src, const Color *pal, int w, int h) {
	_raw16Data = src;
	uploadDataCLUT(_raw16Data, w, w, h, pal);
}

void Texture::readFont(const uint8_t *src) {
	_fmt = FMT_RGBA;
	const int W = 768 * 2;
	const int H = 8;
	uint8_t *out = (uint8_t *)malloc(W * H);
	if (out) {
		for (int i = 0; i < 96; ++i) {
			for (int y = 0; y < 8; ++y) {
				uint8_t mask = *src++;
				for (int x = 0; x < 8; ++x) {
					out[y * W + i * 16 + x] = (mask >> (7 - x)) & 1;
				}
			}
		}
		Color pal[2];
		pal[0].r = pal[0].g = pal[0].b = 0;
		pal[1].r = pal[1].g = pal[1].b = 255;
		uploadDataCLUT(out, W, W, H, pal);
		free(out);
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
	int yOffset;

	DrawList()
		: fillColor(0), yOffset(0) {
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
		NUM_LISTS = 4,
	};

	uint16_t _w, _h;
	bool _fullscreen;
	Color _pal[16];
	Color *_alphaColor;
	Graphics _gfx;
	GLuint _palShader;
	Palette _palTex;
	Texture _backgroundTex;
	Texture _fontTex;
	Texture _spritesTex;
	int _spritesSizeX, _spritesSizeY;
	GLuint _fbPage0;
	GLuint _pageTex[NUM_LISTS];
	DrawList _drawLists[NUM_LISTS];
	struct {
		int num;
		Point pos;
	} _sprite;

	virtual ~SystemStub_OGL() {}
	virtual void init(const char *title);
	virtual void destroy();
	virtual int getRenderMode() const { return _render; }
	virtual void setFont(const uint8_t *src, int w, int h);
	virtual void setPalette(const Color *colors, uint8_t n);
	virtual void setSpriteAtlas(const uint8_t *src, int w, int h, int xSize, int ySize);
	virtual void addSpriteToList(uint8_t listNum, int num, const Point *pt);
	virtual void addBitmapToList(uint8_t listNum, const uint8_t *data, int w, int h, int fmt);
	virtual void addPointToList(uint8_t listNum, uint8_t color, const Point *pt);
	virtual void addQuadStripToList(uint8_t listNum, uint8_t color, const QuadStrip *qs);
	virtual void addCharToList(uint8_t listNum, uint8_t color, char c, const Point *pt);
	virtual void clearList(uint8_t listNum, uint8_t color);
	virtual void copyList(uint8_t dstListNum, uint8_t srcListNum, int16_t vscroll = 0);
	virtual void blitList(uint8_t listNum);
	virtual void processEvents();
	virtual void sleep(uint32_t duration);
	virtual uint32_t getTimeStamp();

	bool initFBO();
	void initPaletteShader();
	void drawVerticesFlat(int count, const Point *vertices);
	void drawVerticesTex(int count, const Point *vertices);
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
	memset(_pal, 0, sizeof(_pal));
	_alphaColor = &_pal[12]; /* _pal[0x8 | color] */
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
	_spritesTex.init();
	_sprite.num = -1;
	if (g_fixUpPalette == FIXUP_PALETTE_SHADER) {
		_palTex.init();
		initPaletteShader();
	}
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
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, g_fixUpPalette == FIXUP_PALETTE_SHADER ? GL_NEAREST : GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, g_fixUpPalette == FIXUP_PALETTE_SHADER ? GL_NEAREST : GL_LINEAR);
		if (g_fixUpPalette == FIXUP_PALETTE_SHADER) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, FB_W, FB_H, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
		} else {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, FB_W, FB_H, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		}
		glBindTexture(GL_TEXTURE_2D, 0);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT + i, GL_TEXTURE_2D, _pageTex[i], 0);
		int status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
		if (status != GL_FRAMEBUFFER_COMPLETE_EXT) {
			warning("glCheckFramebufferStatusEXT failed, ret %d", status);
			return false;
		}
	}

	glViewport(0, 0, FB_W, FB_H);

	const float r = (float)FB_W / SCREEN_W;
	glLineWidth(r);
	glPointSize(r);

	return true;
}

static void printShaderLog(GLuint obj) {
	int len = 0;
	char buf[1024];

	if (glIsShader(obj)) {
		glGetShaderInfoLog(obj, sizeof(buf), &len, buf);
	} else {
		glGetProgramInfoLog(obj, sizeof(buf), &len, buf);
	}
	if (len > 0) {
		warning("shader log : %s", buf);
	}
}

static char *loadFile(const char *path) {
	char *buf = 0;
	File f;
	if (f.open(path)) {
		const int sz = f.size();
		buf = (char *)malloc(sz + 1);
		if (buf) {
			const int count = f.read(buf, sz);
			if (count != sz) {
				warning("Failed to read %d bytes (ret %d)", sz, count);
				free(buf);
				buf = 0;
			} else {
				buf[count] = 0;
			}
		}
	}
	return buf;
}

void SystemStub_OGL::initPaletteShader() {
	char *ptr;
	const char *buf[1];

	buf[0] = ptr = loadFile("palette.vert");
	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, buf, NULL);
	glCompileShader(vs);
	free(ptr);
	printShaderLog(vs);

	buf[0] = ptr = loadFile("palette.frag");
	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, buf, NULL);
	glCompileShader(fs);
	free(ptr);
	printShaderLog(fs);

	_palShader = glCreateProgram();
	glAttachShader(_palShader, vs);
	glAttachShader(_palShader, fs);
	glLinkProgram(_palShader);
	printShaderLog(_palShader);
}

void SystemStub_OGL::destroy() {
	SDL_Quit();
}

void SystemStub_OGL::setFont(const uint8_t *src, int w, int h) {
	if (g_fixUpPalette == FIXUP_PALETTE_SHADER) {
		// TODO:
		return;
	}
	if (src == 0) {
		_fontTex.readFont(Graphics::_font);
	} else {
		_fontTex.uploadDataRGB(src, w * 4, w, h, GL_RGBA, GL_UNSIGNED_BYTE);
	}
}

void SystemStub_OGL::setPalette(const Color *colors, uint8_t n) {
	assert(n <= 16);
	for (int i = 0; i < n; ++i) {
		_pal[i] = colors[i];
	}
	if (g_fixUpPalette == FIXUP_PALETTE_SHADER) {
		_palTex.uploadData(colors, n);
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
					drawVerticesFlat(e.numVertices, e.vertices);
				} else if (e.color == COL_ALPHA) {
					glColor4ub(_alphaColor->r, _alphaColor->g, _alphaColor->b, 192);
					drawVerticesFlat(e.numVertices, e.vertices);
				}
			}

			glLoadIdentity();
			glScalef(1., 1., 1.);
		}
	}
}

void SystemStub_OGL::setSpriteAtlas(const uint8_t *src, int w, int h, int xSize, int ySize) {
	_spritesTex.uploadDataRGB(src, w * 4, w, h, GL_RGBA, GL_UNSIGNED_BYTE);
	_spritesSizeX = xSize;
	_spritesSizeY = ySize;
}

static void drawTexQuad(const int *pos, const float *uv, GLuint tex) {
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

static void drawSprite(const Point *pt, int num, int xSize, int ySize, int texId);

void SystemStub_OGL::addSpriteToList(uint8_t listNum, int num, const Point *pt) {
	if (g_fixUpPalette == FIXUP_PALETTE_SHADER) {
		_sprite.num = num;
		_sprite.pos = *pt;
		return;
	}
	assert(listNum < NUM_LISTS);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _fbPage0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + listNum);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, FB_W, 0, FB_H, 0, 1);

	glScalef((float)FB_W / SCREEN_W, (float)FB_H / SCREEN_H, 1);

	drawSprite(pt, num, _spritesSizeX, _spritesSizeY, _spritesTex._id);

	glLoadIdentity();
	glScalef(1., 1., 1.);
}

static void drawSprite(const Point *pt, int num, int xSize, int ySize, int texId) {
	const int wSize = 18;
	const int hSize = 18;
	const int pos[4] = {
		pt->x, pt->y,
		pt->x + wSize, pt->y + hSize
	};
	int u = num % xSize;
	int v = num / ySize;
	const float uv[4] = {
		u * 1. / xSize, 1. - v * 1. / ySize,
		(u + 1) * 1. / xSize, 1. - (v + 1) * 1. / ySize
	};
	glColor4ub(255, 255, 255, 255);
	drawTexQuad(pos, uv, texId);
}

void SystemStub_OGL::addBitmapToList(uint8_t listNum, const uint8_t *data, int w, int h, int fmt) {
	switch (_render) {
	case RENDER_ORIGINAL:
		if (fmt == FMT_CLUT) {
			memcpy(_gfx.getPagePtr(listNum), data, _gfx.getPageSize());
		}
		break;
	case RENDER_GL:
		assert(listNum == 0);
		_backgroundTex._fmt = fmt;
		switch (fmt) {
		case FMT_CLUT:
			_backgroundTex.readRaw16(data, _pal);
			break;
		case FMT_RGB:
			_backgroundTex.clear();
			_backgroundTex.uploadDataRGB(data, w * 3, w, h, GL_RGB, GL_UNSIGNED_BYTE);
			break;
		case FMT_RGB565:
			_backgroundTex.clear();
			_backgroundTex.uploadDataRGB(data, 320 * 2, 320, 200, GL_RGB, GL_UNSIGNED_SHORT_5_6_5);
			break;
		}
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _fbPage0);
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + listNum);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, FB_W, 0, FB_H, 0, 1);

		if (g_fixUpPalette == FIXUP_PALETTE_SHADER && _backgroundTex._fmt == FMT_RGB) {
			glClearColor(1., 0., 0., 1.);
			glClear(GL_COLOR_BUFFER_BIT);
		} else {
			_backgroundTex.draw(FB_W, FB_H);
		}
		break;
	}

	_drawLists[listNum].clear(COL_BMP);
}

void SystemStub_OGL::drawVerticesToFb(uint8_t color, int count, const Point *vertices) {
	if (color >= 18) {
		warning("drawVerticesToFb() - unhandled color %d (bits %x)", color, color >> 4);
		return;
	}
	glScalef((float)FB_W / SCREEN_W, (float)FB_H / SCREEN_H, 1);

	if (color == COL_PAGE) {
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D,  _pageTex[0]);
		glColor4f(1., 1., 1., 1.);
		drawVerticesTex(count, vertices);
		glDisable(GL_TEXTURE_2D);
	} else {
		if (g_fixUpPalette == FIXUP_PALETTE_SHADER) {
			if (color == COL_ALPHA) {
				color = 16;
			}
			glColor3f(color / 31., 0, 0);
		} else {
			if (color == COL_ALPHA) {
				glColor4ub(_alphaColor->r, _alphaColor->g, _alphaColor->b, 192);
			} else {
				assert(color < 16);
				glColor4ub(_pal[color].r, _pal[color].g, _pal[color].b, 255);
			}
		}
		drawVerticesFlat(count, vertices);
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
	if (g_fixUpPalette == FIXUP_PALETTE_SHADER) {
		// TODO:
		return;
	}
	_gfx.setWorkPagePtr(listNum);
	_gfx.drawChar(c, pt->x / 8, pt->y, color);

	assert(listNum < NUM_LISTS);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _fbPage0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + listNum);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, FB_W, 0, FB_H, 0, 1);

	glScalef((float)FB_W / SCREEN_W, (float)FB_H / SCREEN_H, 1);

	glColor4ub(_pal[color].r, _pal[color].g, _pal[color].b, 255);
	if (_fontTex._h == 8) {
		const int pos[4] = {
			pt->x, pt->y,
			pt->x + 8, pt->y + 8
		};
		const float uv[4] = {
			(c - 0x20) * 16. / _fontTex._w, 0.,
			(c - 0x20) * 16. / _fontTex._w + 1 * 8. / _fontTex._w, 1.
		};
		drawTexQuad(pos, uv, _fontTex._id);
	} else {
		const int pos[4] = {
			pt->x - 8, pt->y,
			pt->x, pt->y + 8
		};
		float uv[4];
		uv[0] = (c % 16) * 16 / 256.;
		uv[2] = uv[0] + 16 / 256.;
		uv[1] = (16 - c / 16) * 16 / 256.;
		uv[3] = uv[1] - 16 / 256.;
		drawTexQuad(pos, uv, _fontTex._id);
	}

	glLoadIdentity();
	glScalef(1., 1., 1.);
}

void SystemStub_OGL::drawVerticesFlat(int count, const Point *vertices) {
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

void SystemStub_OGL::drawVerticesTex(int count, const Point *vertices) {
	if (count < 4) {
		warning("Invalid vertices count for drawing mode 0x11", count);
		return;
	}
	glBegin(GL_QUAD_STRIP);
		for (int i = 0; i < count / 2; ++i) {
			const int j = count - 1 - i;
			if (vertices[j].x > vertices[i].y) {
				glTexCoord2f(vertices[i].x / 320., vertices[i].y / 200.);
				glVertex2i(vertices[i].x, vertices[i].y);
				glTexCoord2f((vertices[j].x + 1) / 320., vertices[j].y / 200.);
				glVertex2i((vertices[j].x + 1), vertices[j].y);
			} else {
				glTexCoord2f(vertices[j].x / 320., vertices[j].y / 200.);
				glVertex2i(vertices[j].x, vertices[j].y);
				glTexCoord2f((vertices[i].x + 1) / 320., vertices[i].y / 200.);
				glVertex2i((vertices[i].x + 1), vertices[i].y);
			}
		}
	glEnd();
}

void SystemStub_OGL::clearList(uint8_t listNum, uint8_t color) {
	if (color >= 16) {
		warning("clearList() - unhandled color %d (bits %x)", color, color >> 4);
		return;
	}
	memset(_gfx.getPagePtr(listNum), color, _gfx.getPageSize());

	assert(listNum < NUM_LISTS);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _fbPage0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + listNum);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, FB_W, 0, FB_H, 0, 1);

	assert(color < 16);
	if (g_fixUpPalette == FIXUP_PALETTE_SHADER) {
		glClearColor(color / 31., 0, 0, 1.);
	} else {
		glClearColor(_pal[color].r / 255.f, _pal[color].g / 255.f, _pal[color].b / 255.f, 1.f);
	}
	glClear(GL_COLOR_BUFFER_BIT);

	_drawLists[listNum].clear(color);
}

static void drawTextureFb(GLuint tex, int w, int h, int vscroll) {
	glColor4ub(255, 255, 255, 255);
	const int pos[] = {
		0, vscroll,
		w, h + vscroll
	};
	const float uv[] = {
		0., 0.,
		1., 1.
	};
	drawTexQuad(pos, uv, tex);
}

void SystemStub_OGL::copyList(uint8_t dstListNum, uint8_t srcListNum, int16_t vscroll) {
	if (vscroll == 0) {
		memcpy(_gfx.getPagePtr(dstListNum), _gfx.getPagePtr(srcListNum), _gfx.getPageSize());
	} else if (vscroll >= -199 && vscroll <= 199) {
		const int dy = (int)round(vscroll * _gfx._h / float(Graphics::GFX_H));
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

	const int yoffset = (int)round(vscroll * FB_H / float(SCREEN_H));
	drawTextureFb(_pageTex[srcListNum], FB_W, FB_H, yoffset);

	_drawLists[dstListNum] = _drawLists[srcListNum];
	_drawLists[dstListNum].yOffset = vscroll;
}

static void dumpPalette(const Color *pal) {
	static const int SZ = 32;
	int x2, x1 = 0;
	for (int i = 0; i < 16; ++i) {
		x2 = x1 + SZ;
		glColor4ub(pal[i].r, pal[i].g, pal[i].b, 255);
		glBegin(GL_QUADS);
			glVertex2i(x1, 0);
			glVertex2i(x2, 0);
			glVertex2i(x2, SZ);
			glVertex2i(x1, SZ);
		glEnd();
		x1 = x2;
	}
}

void SystemStub_OGL::blitList(uint8_t listNum) {
	assert(listNum < NUM_LISTS);

	switch (_render){
	case RENDER_ORIGINAL:
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

		glViewport(0, 0, _w, _h);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, _w, _h, 0, 0, 1);

		_backgroundTex._fmt = FMT_CLUT;
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

		if (g_fixUpPalette == FIXUP_PALETTE_SHADER) {
			const int yoffset = _drawLists[listNum].yOffset * _h / SCREEN_H;
			glPushMatrix();
			glTranslatef(0, yoffset, 0);

			if (_backgroundTex._fmt == FMT_RGB) {
				_backgroundTex.draw(_w, _h);
			}
			if (_sprite.num != -1) {
				glPushMatrix();
				glScalef((float)_w / SCREEN_W, (float)_h / SCREEN_H, 1);
				drawSprite(&_sprite.pos, _sprite.num, _spritesSizeX, _spritesSizeY, _spritesTex._id);
				glPopMatrix();
				_sprite.num = -1;
			}
			glPopMatrix();

			glUseProgram(_palShader);

			glActiveTexture(GL_TEXTURE1);
			glUniform1i(glGetUniformLocation(_palShader, "Palette"), 1);
			glBindTexture(GL_TEXTURE_1D, _palTex._id);

			glActiveTexture(GL_TEXTURE0);
			glUniform1i(glGetUniformLocation(_palShader, "IndexedColorTexture"), 0);
		}

		drawTextureFb(_pageTex[listNum], _w, _h, 0);

		if (g_fixUpPalette == FIXUP_PALETTE_SHADER) {
			glUseProgram(0);
		}

		if (0) {
			glDisable(GL_TEXTURE_2D);
			dumpPalette(_pal);
		}

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
		_backgroundTex.clear();
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
				if (ev.key.keysym.sym == SDLK_f) {
					_pi.fastMode = true;
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
