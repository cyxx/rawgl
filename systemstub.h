
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
	bool save, load;
	bool fastMode;
	int8_t stateSlot;
};

struct SystemStub {
	typedef void (*AudioCallback)(void *param, uint8_t *stream, int len);
	typedef uint32_t (*TimerCallback)(uint32_t delay, void *param);
	
	enum {
		COL_ALPHA = 0x10,
		COL_PAGE  = 0x11
	};

	PlayerInput _pi;

	virtual ~SystemStub() {}

	virtual void init(const char *title) = 0;
	virtual void destroy() = 0;

	virtual void setFont(const uint8_t *font) = 0;
	virtual void setPalette(const Color *colors, uint8_t n) = 0;

	virtual void addBitmapToList(uint8_t listNum, const uint8_t *data) = 0;
	virtual void addPointToList(uint8_t listNum, uint8_t color, const Point *pt) = 0;
	virtual void addLineToList(uint8_t listNum, uint8_t color, const Point *pt1, const Point *pt2) = 0;
	virtual void addQuadStripToList(uint8_t listNum, uint8_t color, const QuadStrip *qs) = 0;
	virtual void addCharToList(uint8_t listNum, uint8_t color, char c, const Point *pt) = 0;
	virtual void clearList(uint8_t listNum, uint8_t color) = 0;
	virtual void copyList(uint8_t dstListNum, uint8_t srcListNum, int16_t vscroll = 0) = 0;
	virtual void blitList(uint8_t listNum) = 0;

	virtual void processEvents() = 0;
	virtual void sleep(uint32_t duration) = 0;
	virtual uint32_t getTimeStamp() = 0;

	virtual void startAudio(AudioCallback callback, void *param) = 0;
	virtual void stopAudio() = 0;
	virtual uint32_t getOutputSampleRate() = 0;
	virtual void lockAudio() = 0;
	virtual void unlockAudio() = 0;

	virtual void *addTimer(uint32_t delay, TimerCallback callback, void *param) = 0;
	virtual void removeTimer(void *timerId) = 0;

	virtual void *createMutex() = 0;
	virtual void destroyMutex(void *mutex) = 0;
	virtual void lockMutex(void *mutex) = 0;
	virtual void unlockMutex(void *mutex) = 0;
};

struct MutexStack {
	SystemStub *_stub;
	void *_mutex;

	MutexStack(SystemStub *stub, void *mutex) 
		: _stub(stub), _mutex(mutex) {
		_stub->lockMutex(_mutex);
	}
	~MutexStack() {
		_stub->unlockMutex(_mutex);
	}
};

struct LockAudioStack {
	LockAudioStack(SystemStub *stub)
		: _stub(stub) {
		_stub->lockAudio();
	}
	~LockAudioStack() {
		_stub->unlockAudio();
	}
	SystemStub *_stub;
};

extern SystemStub *SystemStub_SDL_create();
extern SystemStub *SystemStub_OGL_create();

#endif
