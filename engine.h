
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

struct Graphics;
struct SystemStub;

struct Engine {

	Graphics *_graphics;
	SystemStub *_stub;
	Script _script;
	Mixer _mix;
	Resource _res;
	SfxPlayer _ply;
	Video _vid;
	int _partNum;

	Engine(const char *dataDir, int partNum);

	void setSystemStub(SystemStub *, Graphics *);

	const char *getGameTitle(Language lang) const { return _res.getGameTitle(lang); }

	void run();
	void setup(Language lang);
	void finish();
	void processInput();

	// 3DO
	void doThreeScreens();
	void doEndCredits();
	void playCpak(const char *name);
	void scrollText(int a, int b, const char *text);
	void titlePage();
	
	void saveGameState(uint8_t slot, const char *desc);
	void loadGameState(uint8_t slot);
};

#endif
