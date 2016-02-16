/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */

#include "engine.h"
#include "file.h"
#include "serializer.h"
#include "splash.h"
#include "systemstub.h"


Engine::Engine(SystemStub *stub, const char *dataDir, const char *saveDir)
	: _stub(stub), _scr(&_mix, &_res, &_ply, &_gfx, _stub), _mix(_stub), 
	_res(&_gfx, dataDir), _ply(&_mix, &_res, _stub), _gfx(&_res, stub), 
	_dataDir(dataDir), _saveDir(saveDir), _stateSlot(0) {
}

void Engine::run() {
	_stub->init("Another World", Graphics::OUT_GFX_W, Graphics::OUT_GFX_H);
	_res.prepare();
	_mix.init();
	_ply.init();
	//Splash(_dataDir, _stub).handle((char*)"gbax2005.pcx");
	Splash(_dataDir, _stub).handle((char*)Splash::_splashFileName);
	_scr.restartAt(16001);
	while (!_stub->pi.quit) {
		_scr.setupTasks();
		_scr.inp_updatePlayer();
		_scr.runTasks();
		processInput();
	}
	_ply.free();
	_mix.free();
	_stub->destroy();
}

void Engine::processInput() {
#ifdef ENABLE_SAVE_LOAD
	if (_stub->pi.load) {
		loadGameState(_stateSlot);
		_stub->pi.load = false;
	}
	if (_stub->pi.save) {
		saveGameState(_stateSlot, "quicksave");
		_stub->pi.save = false;
	}
	if (_stub->pi.stateSlot != 0) {
		int8 slot = _stateSlot + _stub->pi.stateSlot;
		if (slot >= 0 && slot < MAX_SAVE_SLOTS) {
			_stateSlot = slot;
			info("Current game state slot is %d", _stateSlot);
		}
		_stub->pi.stateSlot = 0;
	}
#endif
}

void Engine::makeGameStateName(uint8 slot, char *buf) {
	sprintf(buf, "aw-%02d.state", slot);
}

void Engine::saveGameState(uint8 slot, const char *desc) {
	char stateFile[20];
	makeGameStateName(slot, stateFile);
	File f(true);
	if (!f.open(stateFile, _saveDir, "wb")) {
		warning("Unable to save state file '%s'", stateFile);
	} else {
		// header
		f.writeUint32BE('AWSV');
		f.writeUint16BE(Serializer::CUR_VER);
		f.writeUint16BE(0);
		char hdrdesc[32];
		strncpy(hdrdesc, desc, sizeof(hdrdesc) - 1);
		f.write(hdrdesc, sizeof(hdrdesc));
		// contents
		Serializer s(&f, Serializer::SM_SAVE);
		_scr.saveOrLoad(s);
		_res.saveOrLoad(s);
		_gfx.saveOrLoad(s);
		if (f.ioErr()) {
			warning("I/O error when saving game state");
		} else {
			info("Saved state to slot %d", _stateSlot);
		}
	}
}

void Engine::loadGameState(uint8 slot) {
	char stateFile[20];
	makeGameStateName(slot, stateFile);
	File f(true);
	if (!f.open(stateFile, _saveDir, "rb")) {
		warning("Unable to open state file '%s'", stateFile);
	} else {
		uint32 id = f.readUint32BE();
		if (id != 'AWSV') {
			warning("Bad savegame format");
		} else {
			// mute
			_ply.stop();
			_mix.stopAll();
			// header
			uint16 ver = f.readUint16BE();
			f.readUint16BE();
			char hdrdesc[32];
			f.read(hdrdesc, sizeof(hdrdesc));
			// contents
			Serializer s(&f, Serializer::SM_LOAD, ver);
			_scr.saveOrLoad(s);
			_res.saveOrLoad(s);
			_gfx.saveOrLoad(s);
		}
		if (f.ioErr()) {
			warning("I/O error when loading game state");
		} else {
			info("Loaded state from slot %d", _stateSlot);
		}
	}
}
