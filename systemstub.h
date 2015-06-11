
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
	char lastChar;
	bool fastMode;
};

struct Graphics;

struct SystemStub {
	typedef void (*AudioCallback)(void *param, uint8_t *stream, int len);

	PlayerInput _pi;

	virtual ~SystemStub() {}

	virtual void init(const char *title) = 0;
	virtual void fini() = 0;

	virtual void getScreenSize(int &w, int &h) const = 0;
	virtual void updateScreen() = 0;

	virtual void processEvents() = 0;
	virtual void sleep(uint32_t duration) = 0;
	virtual uint32_t getTimeStamp() = 0;
};

extern SystemStub *SystemStub_OGL_create();

#endif
