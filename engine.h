
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef ENGINE_H__
#define ENGINE_H__

#include "intern.h"
#include "script.h"
#include "mixer.h"
#include "sfxplayer.h"
#include "resource.h"
#include "video.h"

struct SystemStub;

struct Engine {
	enum {
		MAX_SAVE_SLOTS = 100
	};
	
	SystemStub *_stub;
	Script _log;
	Mixer _mix;
	Resource _res;
	SfxPlayer _ply;
	Video _vid;
	const char *_dataDir;
	int _partNum;

	Engine(SystemStub *stub, const char *dataDir, int partNum);

	void run(Language lang);
	void setup();
	void finish();
	void processInput();
	
	void saveGameState(uint8_t slot, const char *desc);
	void loadGameState(uint8_t slot);
};

#endif
