/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */

#ifndef __SCRIPT_H__
#define __SCRIPT_H__

#include "intern.h"

struct Graphics;
struct Mixer;
struct Resource;
struct Serializer;
struct SfxPlayer;
struct SystemStub;

struct Script {
	typedef void (Script::*OpcodeStub)();

	enum ScriptVars {
		VAR_RANDOM_SEED          = 0x3C,
		VAR_LOCAL_VERSION        = 0x54,
		VAR_LAST_KEYCHAR         = 0xDA,
		VAR_HERO_POS_UP_DOWN     = 0xE5,
		VAR_MUSIC_MARK           = 0xF4,
		VAR_SCROLL_Y             = 0xF9,
		VAR_HERO_ACTION          = 0xFA,
		VAR_HERO_POS_JUMP_DOWN   = 0xFB,
		VAR_HERO_POS_LEFT_RIGHT  = 0xFC,
		VAR_HERO_POS_MASK        = 0xFD,
		VAR_HERO_ACTION_POS_MASK = 0xFE,
		VAR_PAUSE_SLICES         = 0xFF
	};

	static const OpcodeStub _opTable[];
	static const uint16 _freqTable[];

	Mixer *_mix;
	Resource *_res;
	SfxPlayer *_ply;
	Graphics *_gfx;
	SystemStub *_stub;

	int16 _scriptVars[256];
	uint16 _stack[64];
	uint16 _taskPtr[2][64];
	uint8 _taskState[2][64];
	Ptr _scriptPtr;
	uint8 _stackPtr;
	bool _stopTask;

	Script(Mixer *mix, Resource *res, SfxPlayer *ply, Graphics *gfx, SystemStub *stub);

	void op_movConst();
	void op_mov();
	void op_add();
	void op_addConst();
	void op_call();
	void op_ret();
	void op_yieldTask();
	void op_jmp();
	void op_installTask();
	void op_jmpIfVar();
	void op_jmpIf();
	void op_setPalette();
	void op_changeTasksState();
	void op_selectPage();
	void op_fillPage();
	void op_copyPage();
	void op_updateDisplay();
	void op_removeTask();	
	void op_drawString();
	void op_sub();
	void op_and();
	void op_or();
	void op_shl();
	void op_shr();
	void op_playSound();
	void op_updateResources();
	void op_playMusic();

	void restartAt(uint16 seqId);
	void setupTasks();
	void runTasks();
	void executeTask();

	void inp_updatePlayer();
	void inp_handleSpecialKeys();

	void snd_playSound(uint16 resNum, uint8 freq, uint8 vol, uint8 channel);
	void snd_playMusic(uint16 resNum, uint16 delay, uint8 pos);
	
	void saveOrLoad(Serializer &ser);
};

#endif
