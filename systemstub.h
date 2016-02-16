/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */

#ifndef __SYSTEMSTUB_H__
#define __SYSTEMSTUB_H__

#include "intern.h"

struct PlayerInput {
	enum {
		DIR_LEFT  = 1 << 0,
		DIR_RIGHT = 1 << 1,
		DIR_UP    = 1 << 2,
		DIR_DOWN  = 1 << 3
	};

	uint8 dirMask;
	bool button;
	bool code;
	bool pause;
	bool quit;
	char lastChar;
	bool save, load;
	int8 stateSlot;
};

struct SystemStub {
	typedef void (*AudioCallback)(void *param, void *sbuf);
	typedef uint32 (*TimerCallback)(uint32 delay, void *param);
	
	PlayerInput pi;

	virtual ~SystemStub() {}

	virtual void init(const char *title, uint16 w, uint16 h) = 0;
	virtual void destroy() = 0;

	virtual void setPalette(const Color *colors, int n) = 0;
	virtual void copyRect(const uint8 *buf, uint32 pitch) = 0;

	virtual void processEvents() = 0;
	virtual void sleep(uint32 duration) = 0;
	virtual uint32 getTimeStamp() = 0;

	virtual void startAudio(AudioCallback callback, void *param) = 0;
	virtual void stopAudio() = 0;
	virtual uint32 getOutputSampleRate() = 0;

	virtual void *addTimer(void *param, uint32 delay) = 0;
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

extern SystemStub *SystemStub_GP32_create();

#endif
