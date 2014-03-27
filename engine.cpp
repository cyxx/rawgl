
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "engine.h"
#include "file.h"
#include "systemstub.h"


Engine::Engine(SystemStub *stub, const char *dataDir, int partNum)
	: _stub(stub), _log(&_mix, &_res, &_ply, &_vid, _stub), _mix(_stub), _res(&_vid, dataDir), 
	_ply(&_mix, &_res, _stub), _vid(&_res, stub), _dataDir(dataDir), _partNum(partNum) {
}

void Engine::run(Language lang) {
	_stub->init((lang == LANG_US) ? "Out Of This World" : "Another World");
	setup();
	// setup specific stuff
	switch (lang) {
	case LANG_FR:
		_vid._stringsTable = Video::_stringsTableFr;
		_log._scriptVars[Script::VAR_LOCAL_VERSION] = 0x1;
		break;
	case LANG_US:
		_vid._stringsTable = Video::_stringsTableEng;
		_log._scriptVars[Script::VAR_LOCAL_VERSION] = 0x81;
		break;
	}
	_log.restartAt(16000 + _partNum);
	while (!_stub->_pi.quit) {
		_log.setupScripts();
		_log.inp_updatePlayer();
		processInput();
		_log.runScripts();
	}
	finish();
	_stub->destroy();
}

void Engine::setup() {
	_vid.init();
	_res.allocMemBlock();
	_res.readEntries();
	_log.init();
	_mix.init();
	_ply.init();
}

void Engine::finish() {
	_ply.free();
	_mix.free();
	_res.freeMemBlock();
}

void Engine::processInput() {
	if (_stub->_pi.load) {
		_stub->_pi.load = false;
	}
	if (_stub->_pi.save) {
		_stub->_pi.save = false;
	}
	if (_stub->_pi.fastMode) {
		_log._fastMode = !_log._fastMode;
		_stub->_pi.fastMode = false;
	}
}

void Engine::saveGameState(uint8_t slot, const char *desc) {
}

void Engine::loadGameState(uint8_t slot) {
}
