
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <SDL.h>
#define GL_GLEXT_PROTOTYPES
#include <SDL_opengl.h>
#include <math.h>
#include <vector>
#include "graphics.h"
#include "util.h"


static int _render = RENDER_GL;

static GLuint kNoTextureId = (GLuint)-1;

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
	int _fmt;
	int _fixUpPalette;

	void init();
	void uploadDataCLUT(const uint8_t *data, int srcPitch, int w, int h, const Color *pal);
	void uploadDataRGB(const void *data, int srcPitch, int w, int h, int fmt, int type);
	void draw(int w, int h);
	void clear();
	void readRaw16(const uint8_t *src, const Color *pal, int w = 320, int h = 200);
	void readFont(const uint8_t *src);
};

void Texture::init() {
	_npotTex = false;
	_id = kNoTextureId;
	_w = _h = 0;
	_u = _v = 0.f;
	_rgbData = 0;
	_raw16Data = 0;
	_fmt = -1;
	_fixUpPalette = FIXUP_PALETTE_NONE;
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
		depth = 3;
		fmt = GL_RGB;
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
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (fmt == GL_RED) ? GL_NEAREST : GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (fmt == GL_RED) ? GL_NEAREST : GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		convertTextureCLUT(data, srcPitch, w, h, _rgbData, _w * depth, pal, alpha);
		glTexImage2D(GL_TEXTURE_2D, 0, fmt, _w, _h, 0, fmt, type, _rgbData);
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

static const int NUM_LISTS = 4;

struct GraphicsGL : Graphics {
	int _w, _h;
	Color _pal[16];
	Color *_alphaColor;
	GraphicsSoft _gfx;
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
	int _fixUpPalette;

	GraphicsGL();
	virtual ~GraphicsGL() {}

	virtual int getGraphicsMode() const { return _render; }
	virtual void setSize(int w, int h) { _w = w; _h = h; };
	virtual void setFont(const uint8_t *src, int w, int h);
	virtual void setPalette(const Color *colors, int count);
	virtual void setSpriteAtlas(const uint8_t *src, int w, int h, int xSize, int ySize);
	virtual void drawSprite(int listNum, int num, const Point *pt);
	virtual void drawBitmap(int listNum, const uint8_t *data, int w, int h, int fmt);
	virtual void drawPoint(int listNum, uint8_t color, const Point *pt);
	virtual void drawQuadStrip(int listNum, uint8_t color, const QuadStrip *qs);
	virtual void drawStringChar(int listNum, uint8_t color, char c, const Point *pt);
	virtual void clearBuffer(int listNum, uint8_t color);
	virtual void copyBuffer(int dstListNum, int srcListNum, int vscroll = 0);
	virtual void drawBuffer(int listNum);

	bool initFBO();
	void drawVerticesFlat(int count, const Point *vertices);
	void drawVerticesTex(int count, const Point *vertices);
	void drawVerticesToFb(uint8_t color, int count, const Point *vertices);
};

void setRender(const char *name) {
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
				break;
			}
		}
	}
}

GraphicsGL::GraphicsGL() {
	_fixUpPalette = FIXUP_PALETTE_NONE;
	memset(_pal, 0, sizeof(_pal));
	_alphaColor = &_pal[12]; /* _pal[0x8 | color] */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glShadeModel(GL_SMOOTH);
	const char *exts = (const char *)glGetString(GL_EXTENSIONS);
	const bool npotTex = hasExtension(exts, "GL_ARB_texture_non_power_of_two");
	const bool hasFbo = hasExtension(exts, "GL_ARB_framebuffer_object");
	_backgroundTex.init();
	_backgroundTex._npotTex = npotTex;
	_backgroundTex._fixUpPalette = _fixUpPalette;
	_fontTex.init();
	_fontTex._npotTex = npotTex;
	_fontTex._fixUpPalette = _fixUpPalette;
	_spritesTex.init();
	_spritesTex._npotTex = npotTex;
	_spritesTex._fixUpPalette = _fixUpPalette;
	_sprite.num = -1;
	if (hasFbo) {
		const bool err = initFBO();
		if (err) {
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			return;
		}
	}
}

bool GraphicsGL::initFBO() {
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

void GraphicsGL::setFont(const uint8_t *src, int w, int h) {
	if (src == 0) {
		_fontTex.readFont(GraphicsSoft::_font);
	} else {
		_fontTex.uploadDataRGB(src, w * 4, w, h, GL_RGBA, GL_UNSIGNED_BYTE);
	}
}

void GraphicsGL::setPalette(const Color *colors, int n) {
	assert(n <= 16);
	for (int i = 0; i < n; ++i) {
		_pal[i] = colors[i];
	}
	if (_fixUpPalette == FIXUP_PALETTE_RENDER && _render == RENDER_GL) {
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

void GraphicsGL::setSpriteAtlas(const uint8_t *src, int w, int h, int xSize, int ySize) {
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

static void drawSpriteHelper(const Point *pt, int num, int xSize, int ySize, int texId) {
	const int wSize = 18;
	const int hSize = 18;
	const int pos[4] = {
		pt->x, pt->y,
		pt->x + wSize, pt->y + hSize
	};
	int u = num % xSize;
	int v = num / ySize;
	const float uv[4] = {
		u * 1. / xSize, v * 1. / ySize,
		(u + 1) * 1. / xSize, (v + 1) * 1. / ySize
	};
	glColor4ub(255, 255, 255, 255);
	drawTexQuad(pos, uv, texId);
}

void GraphicsGL::drawSprite(int listNum, int num, const Point *pt) {
	assert(listNum < NUM_LISTS);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _fbPage0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + listNum);

	glViewport(0, 0, FB_W, FB_H);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, FB_W, 0, FB_H, 0, 1);

	glScalef((float)FB_W / SCREEN_W, (float)FB_H / SCREEN_H, 1);

	drawSpriteHelper(pt, num, _spritesSizeX, _spritesSizeY, _spritesTex._id);

	glLoadIdentity();
	glScalef(1., 1., 1.);
}

void GraphicsGL::drawBitmap(int listNum, const uint8_t *data, int w, int h, int fmt) {
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
			_backgroundTex.uploadDataRGB(data, w * 2, w, h, GL_RGB, GL_UNSIGNED_SHORT_5_6_5);
			break;
		}
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _fbPage0);
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + listNum);

		glViewport(0, 0, FB_W, FB_H);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, FB_W, 0, FB_H, 0, 1);

		_backgroundTex.draw(FB_W, FB_H);
		break;
	}

	_drawLists[listNum].clear(COL_BMP);
}

void GraphicsGL::drawVerticesToFb(uint8_t color, int count, const Point *vertices) {
	glScalef((float)FB_W / SCREEN_W, (float)FB_H / SCREEN_H, 1);

	if (color == COL_PAGE) {
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D,  _pageTex[0]);
		glColor4f(1., 1., 1., 1.);
		drawVerticesTex(count, vertices);
		glDisable(GL_TEXTURE_2D);
	} else {
		if (color == COL_ALPHA) {
			glColor4ub(_alphaColor->r, _alphaColor->g, _alphaColor->b, 192);
		} else {
			assert(color < 16);
			glColor4ub(_pal[color].r, _pal[color].g, _pal[color].b, 255);
		}
		drawVerticesFlat(count, vertices);
	}

	glLoadIdentity();
	glScalef(1., 1., 1.);
}

void GraphicsGL::drawPoint(int listNum, uint8_t color, const Point *pt) {
	_gfx.setWorkPagePtr(listNum);
	_gfx.drawPoint(pt->x, pt->y, color);

	assert(listNum < NUM_LISTS);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _fbPage0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + listNum);

	glViewport(0, 0, FB_W, FB_H);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, FB_W, 0, FB_H, 0, 1);

	drawVerticesToFb(color, 1, pt);

	_drawLists[listNum].append(color, 1, pt);
}

void GraphicsGL::drawQuadStrip(int listNum, uint8_t color, const QuadStrip *qs) {
	_gfx.setWorkPagePtr(listNum);
	_gfx.drawPolygon(color, *qs);

	assert(listNum < NUM_LISTS);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _fbPage0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + listNum);

	glViewport(0, 0, FB_W, FB_H);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, FB_W, 0, FB_H, 0, 1);

	drawVerticesToFb(color, qs->numVertices, qs->vertices);

	_drawLists[listNum].append(color, qs->numVertices, qs->vertices);
}

void GraphicsGL::drawStringChar(int listNum, uint8_t color, char c, const Point *pt) {
	_gfx.setWorkPagePtr(listNum);
	_gfx.drawChar(c, pt->x / 8, pt->y, color);

	assert(listNum < NUM_LISTS);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _fbPage0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + listNum);

	glViewport(0, 0, FB_W, FB_H);

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
		uv[1] = (c / 16) * 16 / 256.;
		uv[3] = uv[1] + 16 / 256.;
		drawTexQuad(pos, uv, _fontTex._id);
	}

	glLoadIdentity();
	glScalef(1., 1., 1.);
}

void GraphicsGL::drawVerticesFlat(int count, const Point *vertices) {
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

void GraphicsGL::drawVerticesTex(int count, const Point *vertices) {
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

void GraphicsGL::clearBuffer(int listNum, uint8_t color) {
	memset(_gfx.getPagePtr(listNum), color, _gfx.getPageSize());

	assert(listNum < NUM_LISTS);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _fbPage0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + listNum);

	glViewport(0, 0, FB_W, FB_H);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, FB_W, 0, FB_H, 0, 1);

	assert(color < 16);
	glClearColor(_pal[color].r / 255.f, _pal[color].g / 255.f, _pal[color].b / 255.f, 1.f);
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

void GraphicsGL::copyBuffer(int dstListNum, int srcListNum, int vscroll) {
	if (vscroll == 0) {
		memcpy(_gfx.getPagePtr(dstListNum), _gfx.getPagePtr(srcListNum), _gfx.getPageSize());
	} else if (vscroll >= -199 && vscroll <= 199) {
		const int dy = (int)round(vscroll * _gfx._h / float(GraphicsSoft::GFX_H));
		if (dy < 0) {
			memcpy(_gfx.getPagePtr(dstListNum), _gfx.getPagePtr(srcListNum) - dy * _gfx._w, (_gfx._h + dy) * _gfx._w);
		} else {
			memcpy(_gfx.getPagePtr(dstListNum) + dy * _gfx._w, _gfx.getPagePtr(srcListNum), (_gfx._h - dy) * _gfx._w);
		}
	}

	assert(dstListNum < NUM_LISTS && srcListNum < NUM_LISTS);

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _fbPage0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + dstListNum);

	glViewport(0, 0, FB_W, FB_H);

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

void GraphicsGL::drawBuffer(int listNum) {
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

		glViewport(0, 0, _w, _h);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, _w, _h, 0, 0, 1);

		drawTextureFb(_pageTex[listNum], _w, _h, 0);
		if (0) {
			glDisable(GL_TEXTURE_2D);
			dumpPalette(_pal);
		}
		break;
	}
}

Graphics *GraphicsGL_create() {
	return new GraphicsGL();
}
