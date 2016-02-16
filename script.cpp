/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */

#include <ctime>
#include "graphics.h"
#include "mixer.h"
#include "resource.h"
#include "serializer.h"
#include "sfxplayer.h"
#include "systemstub.h"
#include "script.h"


Script::Script(Mixer *mix, Resource *res, SfxPlayer *ply, Graphics *gfx, SystemStub *stub)
	: _mix(mix), _res(res), _ply(ply), _gfx(gfx), _stub(stub) {
	_ply->_markVar = &_scriptVars[VAR_MUSIC_MARK];
	memset(_scriptVars, 0, sizeof(_scriptVars));
	_scriptVars[VAR_RANDOM_SEED] = time(0);
	_scriptVars[VAR_LOCAL_VERSION] = 0x81; // US version
	 // bypass the protection
	_scriptVars[0xBC] = 0x10; // 0x4417
	_scriptVars[0xC6] = 0x80; // 0x78E0
	_scriptVars[0xF2] = 0xFA0;
	_scriptVars[0xDC] = 0x21;
}

void Script::op_movConst() {
	uint8 i = _scriptPtr.fetchByte();
	int16 n = _scriptPtr.fetchWord();
	_scriptVars[i] = n;
}

void Script::op_mov() {
	uint8 i = _scriptPtr.fetchByte();
	uint8 j = _scriptPtr.fetchByte();	
	_scriptVars[i] = _scriptVars[j];
}

void Script::op_add() {
	uint8 i = _scriptPtr.fetchByte();
	uint8 j = _scriptPtr.fetchByte();
	_scriptVars[i] += _scriptVars[j];
}

void Script::op_addConst() {
	if (_res->_curSeqId == 16006 && _scriptPtr.pc == _res->_dataScript + 0x6D48) {
		// the task 23 in data 0x27 doesn't stop the gun sound from looping. This
		// is a bug in the original script.
		//  6D43: jmp(@6CE5)
		//  6D46: yieldTask // PAUSE SCRIPT TASK
		//  6D47: VAR(0x06) += -50
		snd_playSound(0x5B, 1, 64, 1);
	}
	uint8 i = _scriptPtr.fetchByte();
	int16 n = _scriptPtr.fetchWord();
	_scriptVars[i] += n;
}

void Script::op_call() {
	uint16 off = _scriptPtr.fetchWord();
	_stack[_stackPtr] = _scriptPtr.pc - _res->_dataScript;
	assert(_stackPtr != 0xFF);
	++_stackPtr;
	_scriptPtr.pc = _res->_dataScript + off;
}

void Script::op_ret() {
	assert(_stackPtr != 0);
	--_stackPtr;
	_scriptPtr.pc = _res->_dataScript + _stack[_stackPtr];
}

void Script::op_yieldTask() {
	_stopTask = true;
}

void Script::op_jmp() {
	uint16 off = _scriptPtr.fetchWord();
	_scriptPtr.pc = _res->_dataScript + off;	
}

void Script::op_installTask() {
	uint8 i = _scriptPtr.fetchByte();
	uint16 n = _scriptPtr.fetchWord();
	assert(i < 64);
	_taskPtr[1][i] = n;
}

void Script::op_jmpIfVar() {
	uint8 i = _scriptPtr.fetchByte();
	--_scriptVars[i];
	if (_scriptVars[i] != 0) {
		op_jmp();
	} else {
		_scriptPtr.fetchWord();
	}
}

void Script::op_jmpIf() {
	uint8 op = _scriptPtr.fetchByte();
	int16 b = _scriptVars[_scriptPtr.fetchByte()];
	uint8 c = _scriptPtr.fetchByte();	
	int16 a;
	if (op & 0x80) {
		a = _scriptVars[c];
	} else if (op & 0x40) {
		a = c * 256 + _scriptPtr.fetchByte();
	} else {
		a = c;
	}
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
		break;
	}
	if (expr) {
		op_jmp();
	} else {
		_scriptPtr.fetchWord();
	}
}

void Script::op_setPalette() {
	uint16 i = _scriptPtr.fetchWord();
	_gfx->_newPal = i >> 8;
}

void Script::op_changeTasksState() {
	uint8 i = _scriptPtr.fetchByte();
	uint8 j = _scriptPtr.fetchByte() & 0x3F;
	assert(i <= j);
	uint8 a = _scriptPtr.fetchByte();
	if (a == 2) {
		while (i <= j) {
			_taskPtr[1][i] = 0xFFFE;
			++i;
		}
	} else if (a < 2) {
		while (i <= j) {
			_taskState[1][i] = a;
			++i;
		}
	}
}

void Script::op_selectPage() {
	uint8 page = _scriptPtr.fetchByte();
	_gfx->setWorkPagePtr(page);
}

void Script::op_fillPage() {
	uint8 page = _scriptPtr.fetchByte();
	uint8 color = _scriptPtr.fetchByte();
	_gfx->fillPage(page, color);
}

void Script::op_copyPage() {
	uint8 i = _scriptPtr.fetchByte();
	uint8 j = _scriptPtr.fetchByte();
	_gfx->copyPage(i, j, _scriptVars[VAR_SCROLL_Y]);
}

void Script::op_updateDisplay() {
	uint8 page = _scriptPtr.fetchByte();
	inp_handleSpecialKeys();
	static uint32 tstamp = 0;
	int32 delay = _stub->getTimeStamp() - tstamp;
	int32 pause = _scriptVars[VAR_PAUSE_SLICES] * 20 - delay;
	if (pause > 0) {
		_stub->sleep(pause);
	}
	tstamp = _stub->getTimeStamp();
	_scriptVars[0xF7] = 0;
	_gfx->updateDisplay(page);
}

void Script::op_removeTask() {
	_scriptPtr.pc = _res->_dataScript + 0xFFFF;
	_stopTask = true;
}

void Script::op_drawString() {
	uint16 strId = _scriptPtr.fetchWord();
	uint16 x = _scriptPtr.fetchByte();
	uint16 y = _scriptPtr.fetchByte();
	uint16 col = _scriptPtr.fetchByte();
	_gfx->drawString(col, x, y, strId);
}

void Script::op_sub() {
	uint8 i = _scriptPtr.fetchByte();
	uint8 j = _scriptPtr.fetchByte();
	_scriptVars[i] -= _scriptVars[j];
}

void Script::op_and() {
	uint8 i = _scriptPtr.fetchByte();
	uint16 n = _scriptPtr.fetchWord();
	_scriptVars[i] = (uint16)_scriptVars[i] & n;
}

void Script::op_or() {
	uint8 i = _scriptPtr.fetchByte();
	uint16 n = _scriptPtr.fetchWord();
	_scriptVars[i] = (uint16)_scriptVars[i] | n;
}

void Script::op_shl() {
	uint8 i = _scriptPtr.fetchByte();
	uint16 n = _scriptPtr.fetchWord();
	_scriptVars[i] = (uint16)_scriptVars[i] << n;
}

void Script::op_shr() {
	uint8 i = _scriptPtr.fetchByte();
	uint16 n = _scriptPtr.fetchWord();
	_scriptVars[i] = (uint16)_scriptVars[i] >> n;
}

void Script::op_playSound() {
	uint16 resNum = _scriptPtr.fetchWord();
	uint8 freq = _scriptPtr.fetchByte();
	uint8 vol = _scriptPtr.fetchByte();
	uint8 channel = _scriptPtr.fetchByte();
	snd_playSound(resNum, freq, vol, channel);
}

void Script::op_updateResources() {
	uint16 num = _scriptPtr.fetchWord();
	if (num == 0) {
		_ply->stop();
		_mix->stopAll();
		_res->invalidateSnd();
	} else {
		_res->loadData(num);
	}
}

void Script::op_playMusic() {
	uint16 resNum = _scriptPtr.fetchWord();
	uint16 delay = _scriptPtr.fetchWord();
	uint8 pos = _scriptPtr.fetchByte();
	snd_playMusic(resNum, delay, pos);
}

void Script::restartAt(uint16 seqId) {
	_ply->stop();
	_mix->stopAll();
	_scriptVars[0xE4] = 0x14;
	_res->loadDataSeq(seqId);
	memset(_taskPtr, 0xFF, sizeof(_taskPtr));
	memset(_taskState, 0, sizeof(_taskState));
	_taskPtr[0][0] = 0;	
}

void Script::setupTasks() {
	if (_res->_newSeqId != 0) {
		restartAt(_res->_newSeqId);
		_res->_newSeqId = 0;
	}
	for (int i = 0; i < 64; ++i) {
		_taskState[0][i] = _taskState[1][i];
		uint16 n = _taskPtr[1][i];
		if (n != 0xFFFF) {
			_taskPtr[0][i] = (n == 0xFFFE) ? 0xFFFF : n;
			_taskPtr[1][i] = 0xFFFF;
		}
	}
}

void Script::runTasks() {
	for (int i = 0; i < 64 && !_stub->pi.quit; ++i) {
		if (_taskState[0][i] == 0) {
			uint16 n = _taskPtr[0][i];
			if (n != 0xFFFF) {
				_scriptPtr.pc = _res->_dataScript + n;
				_stackPtr = 0;
				_stopTask = false;
				executeTask();
				_taskPtr[0][i] = _scriptPtr.pc - _res->_dataScript;
			}
		}
	}
}

void Script::executeTask() {
	while (!_stopTask) {
		uint8 opcode = _scriptPtr.fetchByte();
		if (opcode & 0x80) {
			uint16 off = ((opcode << 8) | _scriptPtr.fetchByte()) * 2;
			int16 x = _scriptPtr.fetchByte();
			int16 y = _scriptPtr.fetchByte();
			int16 h = y - 199;
			if (h > 0) {
				y = 199;
				x += h;
			}
			_gfx->setDataBuffer(_res->_dataGfx1, off);
			_gfx->drawShape(0xFF, 64, Point(x, y));
		} else if (opcode & 0x40) {
			uint8 *gfxData = _res->_dataGfx1;
			uint16 off = _scriptPtr.fetchWord() * 2;
			int16 x = _scriptPtr.fetchByte();
			if (!(opcode & 0x20)) {
				if (!(opcode & 0x10)) {
					x = (x << 8) | _scriptPtr.fetchByte();
				} else {
					x = _scriptVars[x];
				}
			} else {
				if (opcode & 0x10) {
					x += 256;
				}
			}
			int16 y = _scriptPtr.fetchByte();
			if (!(opcode & 8)) {
				if (!(opcode & 4)) {
					y = (y << 8) | _scriptPtr.fetchByte();
				} else {
					y = _scriptVars[y];
				}
			}
			uint16 zoom = _scriptPtr.fetchByte();
			if (!(opcode & 2)) {
				if (!(opcode & 1)) {
					--_scriptPtr.pc;
					zoom = 64;
				} else {
					zoom = _scriptVars[zoom];
				}
			} else {
				if (opcode & 1) {
					gfxData = _res->_dataGfx2;
					--_scriptPtr.pc;
					zoom = 64;
				}
			}
			_gfx->setDataBuffer(gfxData, off);
			_gfx->drawShape(0xFF, zoom, Point(x, y));
		} else {
			assert(opcode <= 0x1A);
			(this->*_opTable[opcode])();
		}
	}
}

void Script::inp_updatePlayer() {
	_stub->processEvents();
	if (_res->_curSeqId == 16009) {
		char c = _stub->pi.lastChar;
		if (c == 8 | c == 0 | (c >= 'a' && c <= 'z')) {
			_scriptVars[VAR_LAST_KEYCHAR] = c & ~0x20;
			_stub->pi.lastChar = 0;
		}
	}
	int16 lr = 0;
	int16 m = 0;
	int16 ud = 0;
	if (_stub->pi.dirMask & PlayerInput::DIR_RIGHT) {
		lr = 1;
		m |= 1;
	}
	if (_stub->pi.dirMask & PlayerInput::DIR_LEFT) {
		lr = -1;
		m |= 2;
	}
	if (_stub->pi.dirMask & PlayerInput::DIR_DOWN) {
		ud = 1;
		m |= 4;
	}
	_scriptVars[VAR_HERO_POS_UP_DOWN] = ud;
	if (_stub->pi.dirMask & PlayerInput::DIR_UP) {
		_scriptVars[VAR_HERO_POS_UP_DOWN] = -1;
		ud = -1;
		m |= 8;
	}
	_scriptVars[VAR_HERO_POS_JUMP_DOWN] = ud;
	_scriptVars[VAR_HERO_POS_LEFT_RIGHT] = lr;
	_scriptVars[VAR_HERO_POS_MASK] = m;
	int16 button = 0;
	if (_stub->pi.button) {
		button = 1;
		m |= 0x80;
	}
	_scriptVars[VAR_HERO_ACTION] = button;
	_scriptVars[VAR_HERO_ACTION_POS_MASK] = m;
}

void Script::inp_handleSpecialKeys() {
	if (_stub->pi.pause) {
		if (_res->_curSeqId != 16001) {
			_stub->pi.pause = false;
			while (!_stub->pi.quit && !_stub->pi.pause) {
				_stub->processEvents();
				_stub->sleep(200);
			}
		}
		_stub->pi.pause = false;
	}
	if (_stub->pi.code) {
		_stub->pi.code = false;
		if (_res->_curSeqId != 16009) {
			_res->_newSeqId = 16009;
		}
	}
}

void Script::snd_playSound(uint16 resNum, uint8 freq, uint8 vol, uint8 channel) {
	ResDataEntry *rde = &_res->_resDataList[resNum];
	if (rde->state == Resource::RS_LOADED) {
		if (vol == 0) {
			_mix->stopChannel(channel);
		} else {
			MixerChunk mc;
			memset(&mc, 0, sizeof(mc));
			mc.data = rde->bufPtr + 8; // skip header
			mc.len = READ_BE_UINT16(rde->bufPtr) * 2;
			mc.loopLen = READ_BE_UINT16(rde->bufPtr + 2) * 2;
			if (mc.loopLen != 0) {
				mc.loopPos = mc.len;
			}
			assert(freq < 40);
			_mix->playChannel(channel & 3, &mc, _freqTable[freq], MIN(vol, 0x3F));
		}
	}
}

void Script::snd_playMusic(uint16 resNum, uint16 delay, uint8 pos) {
	if (resNum != 0) {
		_ply->loadSfxModule(resNum, delay, pos);
		_ply->start();
	} else if (delay != 0) {
		_ply->setEventsDelay(delay);
	} else {
		_ply->stop();
	}
}

void Script::saveOrLoad(Serializer &ser) {
	Serializer::Entry entries[] = {
		SE_ARRAY(_scriptVars, 256, Serializer::SES_INT16, VER(1)),
		SE_ARRAY(_stack, 256, Serializer::SES_INT16, VER(1)),
		SE_ARRAY(_taskPtr, 64 * 2, Serializer::SES_INT16, VER(1)),
		SE_ARRAY(_taskState, 64 * 2, Serializer::SES_INT8, VER(1)),
		SE_END()
	};
	ser.saveOrLoadEntries(entries);
}
