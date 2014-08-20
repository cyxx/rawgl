
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

static const int _restartPos[36 * 2] = {
	16008,  0, 16001,  0, 16002, 10, 16002, 12, 16002, 14,
	16003, 20, 16003, 24, 16003, 26, 16004, 30, 16004, 31,
	16004, 32, 16004, 33, 16004, 34, 16004, 35, 16004, 36,
	16004, 37, 16004, 38, 16004, 39, 16004, 40, 16004, 41,
	16004, 42, 16004, 43, 16004, 44, 16004, 45, 16004, 46,
	16004, 47, 16004, 48, 16004, 49, 16006, 64, 16006, 65,
	16006, 66, 16006, 67, 16006, 68, 16005, 50, 16006, 60,
	16007, 0
};

void Engine::run(Language lang) {
	_stub->init((lang == LANG_US) ? "Out Of This World" : "Another World");
	setup();
	if (_res.getDataType() == Resource::DT_DOS) {
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
	}
	const int num = _partNum;
	if (num < 36) {
		_log.restartAt(_restartPos[num * 2]);
		_log._scriptVars[0] = _restartPos[num * 2 + 1];
	} else {
		_log.restartAt(num);
	}
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
	_res.detectVersion();
	_res.allocMemBlock();
	_res.readEntries();
	if (_res.getDataType() == Resource::DT_15TH_EDITION) {
		_res.loadFont();
	}
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
