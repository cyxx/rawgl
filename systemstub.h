
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef SYSTEMSTUB_H__
#define SYSTEMSTUB_H__

#include "intern.h"

struct PlayerInput {
	enum {
		DIR_LEFT  = 1 << 0,
		DIR_RIGHT = 1 << 1,
		DIR_UP    = 1 << 2,
		DIR_DOWN  = 1 << 3
	};

	uint8_t dirMask;
	bool button;
	bool code;
	bool pause;
	bool quit;
	bool back;
	char lastChar;
	bool fastMode;
	bool screenshot;
};

struct DisplayMode {
	enum {
		WINDOWED,
		FULLSCREEN,    // stretch
		FULLSCREEN_AR, // 4:3 aspect ratio
	} mode;
	int width, height; // window dimensions
	bool opengl;       // GL renderer
};

struct SystemStub {
	typedef void (*AudioCallback)(void *param, uint8_t *stream, int len);

	PlayerInput _pi;

	SystemStub() {
		memset(&_pi, 0, sizeof(_pi));
	}
	virtual ~SystemStub() {}

	virtual void init(const char *title, const DisplayMode *dm) = 0;
	virtual void fini() = 0;

	// GL rendering
	virtual void prepareScreen(int &w, int &h, float ar[4]) = 0;
	virtual void updateScreen() = 0;
	// framebuffer rendering
	virtual void setScreenPixels565(const uint16_t *data, int w, int h) = 0;

	virtual void processEvents() = 0;
	virtual void sleep(uint32_t duration) = 0;
	virtual uint32_t getTimeStamp() = 0;
};

extern SystemStub *SystemStub_SDL_create();

#endif
