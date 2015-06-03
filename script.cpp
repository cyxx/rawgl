
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <ctime>
#include "script.h"
#include "mixer.h"
#include "resource.h"
#include "video.h"
#include "sfxplayer.h"
#include "systemstub.h"


Script::Script(Mixer *mix, Resource *res, SfxPlayer *ply, Video *vid, SystemStub *stub)
	: _mix(mix), _res(res), _ply(ply), _vid(vid), _stub(stub) {
}

void Script::init() {
	memset(_scriptVars, 0, sizeof(_scriptVars));
	_fastMode = false;
	_ply->_markVar = &_scriptVars[VAR_MUS_MARK];
	_scriptPtr.byteSwap = _is3DO = (_res->getDataType() == Resource::DT_3DO);
	// bypass the protection
	_scriptVars[0xBC] = 0x10; // 0x4417
	_scriptVars[0xC6] = 0x80; // 0x78E0
	_scriptVars[0xF2] = (_res->getDataType() == Resource::DT_AMIGA) ? 0x1770 : 0xFA0;
	_scriptVars[0xDC] = 0x21;
	if (_is3DO) {
		_scriptVars[0xDB] = 1;
		_scriptVars[0xE2] = 1;
		_scriptVars[0xF2] = 0x1770;
	} else {
		_scriptVars[VAR_RANDOM_SEED] = time(0);
	}
}

void Script::op_movConst() {
	uint8_t i = _scriptPtr.fetchByte();
	int16_t n = _scriptPtr.fetchWord();
	debug(DBG_SCRIPT, "Script::op_movConst(0x%02X, %d)", i, n);
	_scriptVars[i] = n;
}

void Script::op_mov() {
	uint8_t i = _scriptPtr.fetchByte();
	uint8_t j = _scriptPtr.fetchByte();	
	debug(DBG_SCRIPT, "Script::op_mov(0x%02X, 0x%02X)", i, j);
	_scriptVars[i] = _scriptVars[j];
}

void Script::op_add() {
	uint8_t i = _scriptPtr.fetchByte();
	uint8_t j = _scriptPtr.fetchByte();
	debug(DBG_SCRIPT, "Script::op_add(0x%02X, 0x%02X)", i, j);
	_scriptVars[i] += _scriptVars[j];
}

void Script::op_addConst() {
	if (_res->getDataType() == Resource::DT_DOS || _res->getDataType() == Resource::DT_AMIGA) {
		if (_res->_currentPart == 16006 && _scriptPtr.pc == _res->_segCode + 0x6D48) {
			warning("Script::op_addConst() hack for non-stop looping gun sound bug");
			// The script 0x27 slot 0x17 doesn't stop the gun sound from looping.
			// This is a bug in the original game code, confirmed by Eric Chahi and
			// addressed with the anniversary editions.
			// For older releases (DOS, Amiga), we play the 'stop' sound like it is
			// done in other part of the game code.
			//
			//  (0x6D43) jmp(0x6CE5)
			//  (0x6D46) break
			//  (0x6D47) VAR(0x06) += -50
			//
			snd_playSound(0x5B, 1, 64, 1);
		}
	}
	uint8_t i = _scriptPtr.fetchByte();
	int16_t n = _scriptPtr.fetchWord();
	debug(DBG_SCRIPT, "Script::op_addConst(0x%02X, %d)", i, n);
	_scriptVars[i] += n;
}

void Script::op_call() {
	uint16_t off = _scriptPtr.fetchWord();
	debug(DBG_SCRIPT, "Script::op_call(0x%X)", off);
	if (_stackPtr == 0x40) {
		error("Script::op_call() ec=0x%X stack overflow", 0x8F);
	}
	_scriptStackCalls[_stackPtr] = _scriptPtr.pc - _res->_segCode;
	++_stackPtr;
	_scriptPtr.pc = _res->_segCode + off;
}

void Script::op_ret() {
	debug(DBG_SCRIPT, "Script::op_ret()");
	if (_stackPtr == 0) {
		error("Script::op_ret() ec=0x%X stack underflow", 0x8F);
	}	
	--_stackPtr;
	_scriptPtr.pc = _res->_segCode + _scriptStackCalls[_stackPtr];
}

void Script::op_yieldTask() {
	debug(DBG_SCRIPT, "Script::op_yieldTask()");
	_scriptHalted = true;
}

void Script::op_jmp() {
	uint16_t off = _scriptPtr.fetchWord();
	debug(DBG_SCRIPT, "Script::op_jmp(0x%02X)", off);
	_scriptPtr.pc = _res->_segCode + off;	
}

void Script::op_installTask() {
	uint8_t i = _scriptPtr.fetchByte();
	uint16_t n = _scriptPtr.fetchWord();
	debug(DBG_SCRIPT, "Script::op_installTask(0x%X, 0x%X)", i, n);
	assert(i < 0x40);
	_scriptTasks[1][i] = n;
}

void Script::op_jmpIfVar() {
	uint8_t i = _scriptPtr.fetchByte();
	debug(DBG_SCRIPT, "Script::op_jmpIfVar(0x%02X)", i);
	--_scriptVars[i];
	if (_scriptVars[i] != 0) {
		op_jmp();
	} else {
		_scriptPtr.fetchWord();
	}
}

void Script::op_condJmp() {
#ifdef BYPASS_PROTECTION
	if (_res->_currentPart == 16000) {
		if (_scriptVars[VAR_LOCAL_VERSION] == 0x81 && _scriptPtr.pc == _res->_segCode + 0xCB9) { // us
			// (0x0CB8) condJmp(VAR(0x29) == VAR(0x1E), 0x0CD3)
			// (0x0CB8) condJmp(VAR(0x29) != VAR(0x1E), 0x0D24)
			*(_scriptPtr.pc + 0x00) = 0x81;
			*(_scriptPtr.pc + 0x03) = 0x0D;
			*(_scriptPtr.pc + 0x04) = 0x24;
			// (0x0D4E) condJmp(VAR(0x32) < 6, 0x0DBC)
			// (0x0D4E) condJmp(VAR(0x32) < 6, 0x0D5A)
			*(_scriptPtr.pc + 0x99) = 0x0D;
			*(_scriptPtr.pc + 0x9A) = 0x5A;
			warning("Script::op_condJmp() bypassing protection");
		}
		if (_scriptVars[VAR_LOCAL_VERSION] == 0x01 && _scriptPtr.pc == _res->_segCode + 0xC4C) { // fr,eur
			// (0x0C4B) condJmp(VAR(0x29) == VAR(0x1E), 0x0C66)
			// (0x0C4B) condJmp(VAR(0x29) != VAR(0x1E), 0x0CB7)
			*(_scriptPtr.pc + 0x00) = 0x81;
			*(_scriptPtr.pc + 0x03) = 0x0C;
			*(_scriptPtr.pc + 0x04) = 0xB7;
			// (0x0CE1) condJmp(VAR(0x32) < 6, 0x0D4F)
			// (0x0CE1) condJmp(VAR(0x32) < 6, 0x0CED)
			*(_scriptPtr.pc + 0x99) = 0x0C;
			*(_scriptPtr.pc + 0x9A) = 0xED;
			warning("Script::op_condJmp() bypassing protection");
		}
	}
#endif
	uint8_t op = _scriptPtr.fetchByte();
	const uint8_t var = _scriptPtr.fetchByte();
	int16_t b = _scriptVars[var];
	int16_t a;
	if (op & 0x80) {
		a = _scriptVars[_scriptPtr.fetchByte()];
	} else if (op & 0x40) {
		a = _scriptPtr.fetchWord();
	} else {
		a = _scriptPtr.fetchByte();
	}
	debug(DBG_SCRIPT, "Script::op_condJmp(%d, 0x%02X, 0x%02X) var=0x%02X", op, b, a, var);
	bool expr = false;
	switch (op & 7) {
	case 0:
		expr = (b == a);
		break;
	case 1:
		expr = (b != a);
		break;
	case 2:
		expr = (b > a);
		break;
	case 3:
		expr = (b >= a);
		break;
	case 4:
		expr = (b < a);
		break;
	case 5:
		expr = (b <= a);
		break;
	default:
		warning("Script::op_condJmp() invalid condition %d", (op & 7));
		break;
	}
	if (expr) {
		op_jmp();
		if (!_is3DO && var == VAR_SCREEN_NUM && _screenNum != _scriptVars[VAR_SCREEN_NUM]) {
			fixUpPalette_changeScreen(_res->_currentPart, _scriptVars[VAR_SCREEN_NUM]);
			_screenNum = _scriptVars[VAR_SCREEN_NUM];
		}
	} else {
		_scriptPtr.fetchWord();
	}
}

void Script::op_setPalette() {
	uint16_t i = _scriptPtr.fetchWord();
	debug(DBG_SCRIPT, "Script::op_changePalette(%d)", i);
	const int num = i >> 8;
	if (g_fixUpPalette == FIXUP_PALETTE_RENDER) {
		if (_res->_currentPart == 16001) {
			if (num == 10 || num == 16) {
				return;
			}
		}
		_vid->_nextPal = num;
	} else {
		_vid->_nextPal = num;
	}
}

void Script::op_changeTasksState() {
	uint8_t j = _scriptPtr.fetchByte();
	uint8_t i = _scriptPtr.fetchByte();
	int8_t n = (i & 0x3F) - j;
	if (n < 0) {
		warning("Script::op_changeTasksState() ec=0x%X (n < 0)", 0x880);
		return;
	}
	++n;
	uint8_t a = _scriptPtr.fetchByte();

	debug(DBG_SCRIPT, "Script::op_changeTasksState(%d, %d, %d)", j, i, a);

	if (a == 2) {
		uint16_t *p = &_scriptTasks[1][j];
		while (n--) {
			*p++ = 0xFFFE;
		}
	} else if (a < 2) {
		uint8_t *p = &_scriptPaused[1][j];
		while (n--) {
			*p++ = a;
		}
	}
}

void Script::op_selectPage() {
	uint8_t i = _scriptPtr.fetchByte();
	debug(DBG_SCRIPT, "Script::op_selectPage(%d)", i);
	_vid->setWorkPagePtr(i);
}

void Script::op_fillPage() {
	uint8_t i = _scriptPtr.fetchByte();
	uint8_t color = _scriptPtr.fetchByte();
	debug(DBG_SCRIPT, "Script::op_fillPage(%d, %d)", i, color);
	_vid->fillPage(i, color);
}

void Script::op_copyPage() {
	uint8_t i = _scriptPtr.fetchByte();
	uint8_t j = _scriptPtr.fetchByte();
	debug(DBG_SCRIPT, "Script::op_copyPage(%d, %d)", i, j);
	_vid->copyPage(i, j, _scriptVars[VAR_SCROLL_Y]);
}

void Script::op_updateDisplay() {
	uint8_t page = _scriptPtr.fetchByte();
	debug(DBG_SCRIPT, "Script::op_updateDisplay(%d)", page);
	inp_handleSpecialKeys();
	if (_res->_currentPart == 16000 && _scriptVars[VAR_SCREEN_NUM] == 1) {
		_scriptVar_0xBF = _scriptVars[0xBF];
		_scriptVars[0xDC] = 0x21;
	}

	const int frameHz = _is3DO ? 60 : 50;
	if (!_fastMode && _scriptVars[VAR_PAUSE_SLICES] != 0) {
		const int delay = _stub->getTimeStamp() - _timeStamp;
		const int pause = _scriptVars[VAR_PAUSE_SLICES] * 1000 / frameHz - delay;
		if (pause > 0) {
			_stub->sleep(pause);
		}
	}
	_timeStamp = _stub->getTimeStamp();
	if (_is3DO) {
		_scriptVars[0xF7] = (_timeStamp - _startTime) * frameHz / 1000;
	} else {
		_scriptVars[0xF7] = 0;
	}

	_vid->_displayHead = !(_res->_currentPart == 16006 && _screenNum == 202);
	_vid->updateDisplay(page);
}

void Script::op_removeTask() {
	debug(DBG_SCRIPT, "Script::op_removeTask()");
	_scriptPtr.pc = _res->_segCode + 0xFFFF;
	_scriptHalted = true;
}

void Script::op_drawString() {
	uint16_t strId = _scriptPtr.fetchWord();
	uint16_t x = _scriptPtr.fetchByte();
	uint16_t y = _scriptPtr.fetchByte();
	uint16_t col = _scriptPtr.fetchByte();
	debug(DBG_SCRIPT, "Script::op_drawString(0x%03X, %d, %d, %d)", strId, x, y, col);
	_vid->drawString(col, x, y, strId);
}

void Script::op_sub() {
	uint8_t i = _scriptPtr.fetchByte();
	uint8_t j = _scriptPtr.fetchByte();
	debug(DBG_SCRIPT, "Script::op_sub(0x%02X, 0x%02X)", i, j);
	_scriptVars[i] -= _scriptVars[j];
}

void Script::op_and() {
	uint8_t i = _scriptPtr.fetchByte();
	uint16_t n = _scriptPtr.fetchWord();
	debug(DBG_SCRIPT, "Script::op_and(0x%02X, %d)", i, n);
	_scriptVars[i] = (uint16_t)_scriptVars[i] & n;
}

void Script::op_or() {
	uint8_t i = _scriptPtr.fetchByte();
	uint16_t n = _scriptPtr.fetchWord();
	debug(DBG_SCRIPT, "Script::op_or(0x%02X, %d)", i, n);
	_scriptVars[i] = (uint16_t)_scriptVars[i] | n;
}

void Script::op_shl() {
	uint8_t i = _scriptPtr.fetchByte();
	uint16_t n = _scriptPtr.fetchWord();
	debug(DBG_SCRIPT, "Script::op_shl(0x%02X, %d)", i, n);
	_scriptVars[i] = (uint16_t)_scriptVars[i] << n;
}

void Script::op_shr() {
	uint8_t i = _scriptPtr.fetchByte();
	uint16_t n = _scriptPtr.fetchWord();
	debug(DBG_SCRIPT, "Script::op_shr(0x%02X, %d)", i, n);
	_scriptVars[i] = (uint16_t)_scriptVars[i] >> n;
}

void Script::op_playSound() {
	uint16_t resNum = _scriptPtr.fetchWord();
	uint8_t freq = _scriptPtr.fetchByte();
	uint8_t vol = _scriptPtr.fetchByte();
	uint8_t channel = _scriptPtr.fetchByte();
	debug(DBG_SCRIPT, "Script::op_playSound(0x%X, %d, %d, %d)", resNum, freq, vol, channel);
	snd_playSound(resNum, freq, vol, channel);
}

void Script::op_updateResources() {
	uint16_t num = _scriptPtr.fetchWord();
	debug(DBG_SCRIPT, "Script::op_updateResources(%d)", num);
	if (num == 0) {
		_ply->stop();
		_mix->stopAll();
		_res->invalidateRes();
	} else {
		_res->update(num);
	}
}

void Script::op_playMusic() {
	uint16_t resNum = _scriptPtr.fetchWord();
	uint16_t delay = _scriptPtr.fetchWord();
	uint8_t pos = _scriptPtr.fetchByte();
	debug(DBG_SCRIPT, "Script::op_playMusic(0x%X, %d, %d)", resNum, delay, pos);
	snd_playMusic(resNum, delay, pos);
}

void Script::restartAt(int part, int pos) {
	_ply->stop();
	_mix->stopAll();
	if (_res->getDataType() == Resource::DT_20TH_EDITION && part != 16001) {
		// difficulty (0 to 2)
		_scriptVars[0xBF] = 1;
		// mute sounds and enable resnum >= 2000
//		_scriptVars[0xDB] = 1;
		// playback sounds resnum >= 146
		_scriptVars[0xDE] = 1;
	}
	if (_res->getDataType() == Resource::DT_DOS || _res->getDataType() == Resource::DT_AMIGA) {
		_scriptVars[0xE4] = 0x14;
	}
	_res->setupPart(part);
	memset(_scriptTasks, 0xFF, sizeof(_scriptTasks));
	memset(_scriptPaused, 0, sizeof(_scriptPaused));
	_scriptTasks[0][0] = 0;
	_screenNum = -1;
	if (pos >= 0) {
		_scriptVars[0] = pos;
	}
	if (g_fixUpPalette == FIXUP_PALETTE_RENDER) {
		if (part == 16009) {
			_vid->changePal(5);
		}
	}
	_startTime = _timeStamp = _stub->getTimeStamp();
}

void Script::setupScripts() {
	if (_res->_nextPart != 0) {
		restartAt(_res->_nextPart);
		_res->_nextPart = 0;
	}
	for (int i = 0; i < 0x40; ++i) {
		_scriptPaused[0][i] = _scriptPaused[1][i];
		uint16_t n = _scriptTasks[1][i];
		if (n != 0xFFFF) {
			_scriptTasks[0][i] = (n == 0xFFFE) ? 0xFFFF : n;
			_scriptTasks[1][i] = 0xFFFF;
		}
	}
}

void Script::runScripts() {
	for (int i = 0; i < 0x40 && !_stub->_pi.quit; ++i) {
		if (_scriptPaused[0][i] == 0) {
			uint16_t n = _scriptTasks[0][i];
			if (n != 0xFFFF) {
				_scriptPtr.pc = _res->_segCode + n;
				_stackPtr = 0;
				_scriptHalted = false;
				debug(DBG_SCRIPT, "Script::runScripts() i=0x%02X n=0x%02X", i, n);
				executeScript();
				_scriptTasks[0][i] = _scriptPtr.pc - _res->_segCode;
				debug(DBG_SCRIPT, "Script::runScripts() i=0x%02X pos=0x%X", i, _scriptTasks[0][i]);
			}
		}
	}
}

void Script::executeScript() {
	while (!_scriptHalted) {
		uint8_t opcode = _scriptPtr.fetchByte();
		if (opcode & 0x80) {
			uint16_t off = ((opcode << 8) | _scriptPtr.fetchByte()) * 2;
			_res->_useSegVideo2 = false;
			Point pt;
			pt.x = _scriptPtr.fetchByte();
			pt.y = _scriptPtr.fetchByte();
			int16_t h = pt.y - 199;
			if (h > 0) {
				pt.y = 199;
				pt.x += h;
			}
			debug(DBG_VIDEO, "vid_opcd_0x80 : opcode=0x%X off=0x%X x=%d y=%d", opcode, off, pt.x, pt.y);
			_vid->setDataBuffer(_res->_segVideo1, off);
			if (_is3DO) {
				_vid->drawShape3DO(0xFF, 64, &pt);
			} else {
				_vid->drawShape(0xFF, 0x40, &pt);
			}
		} else if (opcode & 0x40) {
			Point pt;
			uint16_t off = ((_scriptPtr.pc[0] << 8) | _scriptPtr.pc[1]) * 2; _scriptPtr.pc += 2;
			pt.x = _scriptPtr.fetchByte();
			_res->_useSegVideo2 = false;
			if (!(opcode & 0x20)) {
				if (!(opcode & 0x10)) {
					pt.x = (pt.x << 8) | _scriptPtr.fetchByte();
				} else {
					pt.x = _scriptVars[pt.x];
				}
			} else {
				if (opcode & 0x10) {
					pt.x += 0x100;
				}
			}
			pt.y = _scriptPtr.fetchByte();
			if (!(opcode & 8)) {
				if (!(opcode & 4)) {
					pt.y = (pt.y << 8) | _scriptPtr.fetchByte();
				} else {
					pt.y = _scriptVars[pt.y];
				}
			}
			uint16_t zoom = _scriptPtr.fetchByte();
			if (!(opcode & 2)) {
				if (!(opcode & 1)) {
					--_scriptPtr.pc;
					zoom = 0x40;
				} else {
					zoom = _scriptVars[zoom];
				}
			} else {
				if (opcode & 1) {
					_res->_useSegVideo2 = true;
					--_scriptPtr.pc;
					zoom = 0x40;
				}
			}
			debug(DBG_VIDEO, "vid_opcd_0x40 : off=0x%X x=%d y=%d", off, pt.x, pt.y);
			_vid->setDataBuffer(_res->_useSegVideo2 ? _res->_segVideo2 : _res->_segVideo1, off);
			if (_is3DO) {
				_vid->drawShape3DO(0xFF, zoom, &pt);
			} else {
				_vid->drawShape(0xFF, zoom, &pt);
			}
		} else {
			if (_is3DO) {
				switch (opcode) {
				case 11: {
						const int num = _scriptPtr.fetchByte();
						debug(DBG_SCRIPT, "Script::op11() setPalette %d", num);
						_vid->changePal(num);
					}
					continue;
				case 22: {
						const int var = _scriptPtr.fetchByte();
						const int shift = _scriptPtr.fetchByte();
						debug(DBG_SCRIPT, "Script::op22() VAR(0x%02X) <<= %d", var, shift);
						_scriptVars[var] = (uint16_t)_scriptVars[var] << shift;
					}
					continue;
				case 23: {
						const int var = _scriptPtr.fetchByte();
						const int shift  = _scriptPtr.fetchByte();
						debug(DBG_SCRIPT, "Script::op23() VAR(0x%02X) >>= %d", var, shift);
						_scriptVars[var] = (uint16_t)_scriptVars[var] >> shift;
					}
					continue;
				case 26: {
						const int num = _scriptPtr.fetchByte();
						debug(DBG_SCRIPT, "Script::op26() playMusic %d", num);
						snd_playMusic(num, 0, 0);
					}
					continue;
				case 27: {
						const int num = _scriptPtr.fetchWord();
						const int x = _scriptVars[_scriptPtr.fetchByte()];
						const int y = _scriptVars[_scriptPtr.fetchByte()];
						const int color = _scriptPtr.fetchByte();
						_vid->drawString(color, x, y, num);
					}
					continue;
				case 28: {
						const uint8_t var = _scriptPtr.fetchByte();
						debug(DBG_SCRIPT, "Script::op28() jmpIf(VAR(0x%02X) == 0)");
						if (_scriptVars[var] == 0) {
							op_jmp();
						} else {
							_scriptPtr.fetchWord();
						}
					}
					continue;
				case 29: {
						const uint8_t var = _scriptPtr.fetchByte();
						debug(DBG_SCRIPT, "Script::op29() jmpIf(VAR(0x%02X) != 0)");
						if (_scriptVars[var] != 0) {
							op_jmp();
						} else {
							_scriptPtr.fetchWord();
						}
					}
					continue;
				case 30: {
						// _scriptVars[0xF7]
						warning("Script::op30() unimplemented");
					}
					continue;
				}
			}
			if (opcode > 0x1A) {
				error("Script::executeScript() ec=0x%X invalid opcode=0x%X", 0xFFF, opcode);
			} else {
				(this->*_opTable[opcode])();
			}
		}
	}
}

void Script::inp_updatePlayer() {
	_stub->processEvents();
	if (_res->_currentPart == 16009) {
		char c = _stub->_pi.lastChar;
		if (c == 8 || /*c == 0xD ||*/ c == 0 || (c >= 'a' && c <= 'z')) {
			_scriptVars[VAR_LAST_KEYCHAR] = c & ~0x20;
			_stub->_pi.lastChar = 0;
		}
	}
	int16_t lr = 0;
	int16_t m = 0;
	int16_t ud = 0;
	if (_stub->_pi.dirMask & PlayerInput::DIR_RIGHT) {
		lr = 1;
		m |= 1;
	}
	if (_stub->_pi.dirMask & PlayerInput::DIR_LEFT) {
		lr = -1;
		m |= 2;
	}
	if (_stub->_pi.dirMask & PlayerInput::DIR_DOWN) {
		ud = 1;
		m |= 4;
	}
	_scriptVars[VAR_HERO_POS_UP_DOWN] = ud;
	if (_stub->_pi.dirMask & PlayerInput::DIR_UP) {
		_scriptVars[VAR_HERO_POS_UP_DOWN] = -1;
	}
	if (_stub->_pi.dirMask & PlayerInput::DIR_UP) { // inpJump
		ud = -1;
		m |= 8;
	}
	_scriptVars[VAR_HERO_POS_JUMP_DOWN] = ud;
	_scriptVars[VAR_HERO_POS_LEFT_RIGHT] = lr;
	_scriptVars[VAR_HERO_POS_MASK] = m;
	int16_t button = 0;
	if (_stub->_pi.button) { // inpButton
		button = 1;
		m |= 0x80;
	}
	_scriptVars[VAR_HERO_ACTION] = button;
	_scriptVars[VAR_HERO_ACTION_POS_MASK] = m;
}

void Script::inp_handleSpecialKeys() {
	if (_stub->_pi.pause) {
		if (_res->_currentPart != 16000 && _res->_currentPart != 16001) {
			_stub->_pi.pause = false;
			while (!_stub->_pi.pause && !_stub->_pi.quit) {
				_stub->processEvents();
				_stub->sleep(50);
			}
		}
		_stub->_pi.pause = false;
	}
	if (_stub->_pi.code) {
		_stub->_pi.code = false;
		if (_res->_currentPart != 16009 && _res->_currentPart != 16000) {
			_res->_nextPart = 16009;
		}
	}
	if (_scriptVars[0xC9] == 1) {
		// this happens on french/europeans versions when the user does not select
		// any symbols for the protection, the disassembly shows a simple 'hlt' here.
	}
}

void Script::snd_playSound(uint16_t resNum, uint8_t freq, uint8_t vol, uint8_t channel) {
	debug(DBG_SND, "snd_playSound(0x%X, %d, %d, %d)", resNum, freq, vol, channel);
	if (vol == 0) {
		_mix->stopSound(channel);
		return;
	}
	if (_res->getDataType() == Resource::DT_15TH_EDITION || _res->getDataType() == Resource::DT_20TH_EDITION || _res->getDataType() == Resource::DT_WIN31) {
		uint8_t *buf = _res->loadWav(resNum);
		if (buf) {
			_mix->playSoundWav(channel & 3, buf, MIN(vol, 63));
		}
	} else if (_res->getDataType() == Resource::DT_3DO) {
		MemEntry *me = &_res->_memList[resNum];
		if (me->status == Resource::STATUS_LOADED) {
			_mix->playSoundAiff(channel & 3, me->bufPtr, MIN(vol, 63));
		}
	} else { // DT_AMIGA, DT_DOS
		MemEntry *me = &_res->_memList[resNum];
		if (me->status == Resource::STATUS_LOADED) {
			assert(freq < 40);
			_mix->playSoundRaw(channel & 3, me->bufPtr, _freqTable[freq], MIN(vol, 63));
		}
	}
}

void Script::snd_playMusic(uint16_t resNum, uint16_t delay, uint8_t pos) {
	debug(DBG_SND, "snd_playMusic(0x%X, %d, %d)", resNum, delay, pos);
	switch (_res->getDataType()) {
	case Resource::DT_15TH_EDITION:
	case Resource::DT_20TH_EDITION:
	case Resource::DT_WIN31:
		if (resNum == 0) {
			_mix->stopMusic();
		} else {
			char path[512];
			const char *p = _res->getMusicPath(resNum, path, sizeof(path));
			if (p) {
				_mix->playMusic(p);
			}
		}
		break;
	case Resource::DT_3DO:
		if (resNum == 0) {
			_mix->stopAifcMusic();
		} else {
			char path[512];
			const char *p = _res->getMusicPath(resNum, path, sizeof(path));
			if (p) {
				_mix->playAifcMusic(p);
			}
		}
		break;
	default: // DT_AMIGA, DT_DOS
		if (resNum != 0) {
			_ply->loadSfxModule(resNum, delay, pos);
			_ply->start();
			_mix->playSfxMusic(resNum);
		} else if (delay != 0) {
			_ply->setEventsDelay(delay);
		} else {
			_mix->stopSfxMusic();
		}
		break;
	}
}

void Script::fixUpPalette_changeScreen(int part, int screen) {
	int pal = -1;
	switch (part) {
	case 16004:
		if (screen == 0x47) { // bitmap resource #68
			pal = 8;
		}
		break;
	case 16006:
		if (screen == 0x4A) { // bitmap resources #144, #145
			pal = 1;
		}
		break;
	}
	if (pal != -1) {
		debug(DBG_SCRIPT, "Setting palette %d for part %d screen %d", pal, part, screen);
		_vid->changePal(pal);
	}
}
