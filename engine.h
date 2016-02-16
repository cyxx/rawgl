/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */

#ifndef __ENGINE_H__
#define __ENGINE_H__

#include "intern.h"
#include "graphics.h"
#include "mixer.h"
#include "resource.h"
#include "script.h"
#include "sfxplayer.h"

struct SystemStub;

struct Engine {
	enum {
		MAX_SAVE_SLOTS = 100
	};
	
	SystemStub *_stub;
	Script _scr;
	Mixer _mix;
	Resource _res;
	SfxPlayer _ply;
	Graphics _gfx;
	const char *_dataDir, *_saveDir;
	int _stateSlot;

	Engine(SystemStub *stub, const char *dataDir, const char *saveDir);

	void run();
	void processInput();
	void makeGameStateName(uint8 slot, char *buf);
	void saveGameState(uint8 slot, const char *desc);
	void loadGameState(uint8 slot);
};

#endif
