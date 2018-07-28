
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "engine.h"
#include "file.h"
#include "graphics.h"
#include "systemstub.h"
#include "util.h"


Engine::Engine(const char *dataDir, int partNum)
	: _graphics(0), _stub(0), _script(&_mix, &_res, &_ply, &_vid), _mix(&_ply), _res(&_vid, dataDir),
	_ply(&_res), _vid(&_res), _partNum(partNum) {
	_res.detectVersion();
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

void Engine::setSystemStub(SystemStub *stub, Graphics *graphics) {
	_stub = stub;
	_script._stub = stub;
	_graphics = graphics;
}

void Engine::run() {
	_script.setupTasks();
	_script.updateInput();
	processInput();
	_script.runTasks();
	_mix.update();
}

void Engine::setup(Language lang) {
	_vid._graphics = _graphics;
	_graphics->init();
	if (_res.getDataType() != Resource::DT_3DO) {
		_vid._graphics->_fixUpPalette = FIXUP_PALETTE_REDRAW;
	}
	_vid.init();
	_res.allocMemBlock();
	_res.readEntries();
	_res.dumpEntries();
	if (_res.getDataType() == Resource::DT_15TH_EDITION || _res.getDataType() == Resource::DT_20TH_EDITION) {
		_res.loadFont();
		_res.loadHeads();
	} else {
		_vid.setDefaultFont();
	}
	_script.init();
	_mix.init();
	if (_res.getDataType() == Resource::DT_DOS || _res.getDataType() == Resource::DT_AMIGA || _res.getDataType() == Resource::DT_MAC) {
		switch (lang) {
		case LANG_FR:
			_vid._stringsTable = Video::_stringsTableFr;
			break;
		case LANG_US:
		default:
			_vid._stringsTable = Video::_stringsTableEng;
			break;
		}
	}
	if (_res.getDataType() == Resource::DT_3DO) {
		titlePage();
	}
	const int num = _partNum;
	if (num < 36) {
		_script.restartAt(_restartPos[num * 2], _restartPos[num * 2 + 1]);
	} else {
		_script.restartAt(num);
	}
}

void Engine::finish() {
	_graphics->fini();
	_ply.stop();
	_mix.quit();
	_res.freeMemBlock();
}

void Engine::processInput() {
	if (_stub->_pi.fastMode) {
		_script._fastMode = !_script._fastMode;
		_stub->_pi.fastMode = false;
	}
	if (_stub->_pi.screenshot) {
		_vid.captureDisplay();
		_stub->_pi.screenshot = false;
	}
}

void Engine::titlePage() {
	// doThreeScreens();
	// scrollText(0, 380, Video::_noteText3DO);
	// playCpak("GameData/Logo.Cine");
	// playCpak("GameData/SpinTitle.Cine");
	_res.loadBmp(70);
	_vid.updateDisplay(0, _stub);
	enum {
		kPartNewGame  = 16002,
		kPartPassword = 16008
	};
	while (!_stub->_pi.quit) {
		_stub->processEvents();
		if (_stub->_pi.dirMask & PlayerInput::DIR_DOWN) {
			_stub->_pi.dirMask &= ~PlayerInput::DIR_DOWN;
			_partNum = kPartPassword;
		}
		if (_stub->_pi.dirMask & PlayerInput::DIR_UP) {
			_stub->_pi.dirMask &= ~PlayerInput::DIR_UP;
			_partNum = kPartNewGame;
		}
		if (_stub->_pi.button) {
			_stub->_pi.button = false;
			break;
		}
		_stub->sleep(50);
	}
}

void Engine::saveGameState(uint8_t slot, const char *desc) {
}

void Engine::loadGameState(uint8_t slot) {
}
