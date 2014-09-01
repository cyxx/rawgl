
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#ifndef SCRIPT_H__
#define SCRIPT_H__

#include "intern.h"

struct Mixer;
struct Resource;
struct Serializer;
struct SfxPlayer;
struct SystemStub;
struct Video;

struct Script {
	typedef void (Script::*OpcodeStub)();

	enum ScriptVars {
		VAR_RANDOM_SEED          = 0x3C,
		
		VAR_LOCAL_VERSION        = 0x54,

		VAR_SCREEN_NUM           = 0x67,
		
		VAR_LAST_KEYCHAR         = 0xDA,

		VAR_HERO_POS_UP_DOWN     = 0xE5,

		VAR_MUS_MARK             = 0xF4,

		VAR_SCROLL_Y             = 0xF9,
		VAR_HERO_ACTION          = 0xFA,
		VAR_HERO_POS_JUMP_DOWN   = 0xFB,
		VAR_HERO_POS_LEFT_RIGHT  = 0xFC,
		VAR_HERO_POS_MASK        = 0xFD,
		VAR_HERO_ACTION_POS_MASK = 0xFE,
		VAR_PAUSE_SLICES         = 0xFF
	};
	
	static const OpcodeStub _opTable[];
	static const uint16_t _freqTable[];

	Mixer *_mix;
	Resource *_res;
	SfxPlayer *_ply;
	Video *_vid;
	SystemStub *_stub;

	int16_t _scriptVar_0xBF;
	int16_t _scriptVars[0x100];
	uint16_t _scriptStackCalls[0x40];
	uint16_t _scriptSlotsPos[2][0x40];
	uint8_t _scriptPaused[2][0x40];
	Ptr _scriptPtr;
	uint8_t _stackPtr;
	bool _scriptHalted;
	bool _fastMode;
	int _screenNum;

	Script(Mixer *mix, Resource *res, SfxPlayer *ply, Video *vid, SystemStub *stub);
	void init();
	
	void op_movConst();
	void op_mov();
	void op_add();
	void op_addConst();
	void op_call();
	void op_ret();
	void op_break();
	void op_jmp();
	void op_setScriptSlot();
	void op_jmpIfVar();
	void op_condJmp();
	void op_setPalette();
	void op_resetScript();
	void op_selectPage();
	void op_fillPage();
	void op_copyPage();
	void op_updateDisplay();
	void op_halt();	
	void op_drawString();
	void op_sub();
	void op_and();
	void op_or();
	void op_shl();
	void op_shr();
	void op_playSound();
	void op_updateMemList();
	void op_playMusic();

	void restartAt(int part, int pos = -1);
	void setupPart(int num);
	void setupScripts();
	void runScripts();
	void executeScript();

	void inp_updatePlayer();
	void inp_handleSpecialKeys();
	
	void snd_playSound(uint16_t resNum, uint8_t freq, uint8_t vol, uint8_t channel);
	void snd_playMusic(uint16_t resNum, uint16_t delay, uint8_t pos);

	void fixUpPalette_changeScreen(int part, int screen = -1);
	void fixUpPalette_selectPage(int part, int page, int pal);
	void fixUpPalette_fillPage(int part, int page, int color, int currentPal);
};

#endif
