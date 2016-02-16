/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */
#include <rtems.h>
#include <tmacros.h>

#include "mixer.h"
#include "resource.h"
#include "systemstub.h"
#include "sfxplayer.h"


SfxPlayer::SfxPlayer(Mixer *mix, Resource *res, SystemStub *stub)
	: _mix(mix), _res(res), _stub(stub), _delay(0), _resNum(0) {
}

void SfxPlayer::init() {
	_mutex = _stub->createMutex();
}

void SfxPlayer::free() {
	stop();
	_stub->destroyMutex(_mutex);
}

void SfxPlayer::setEventsDelay(uint16 delay) {
	MutexStack(_stub, _mutex);
	_delay = delay * 60 / 7050;
}

void SfxPlayer::loadSfxModule(uint16 resNum, uint16 delay, uint8 pos) {
	MutexStack(_stub, _mutex);
	ResDataEntry *rde = &_res->_resDataList[resNum];
	if (rde->state == Resource::RS_LOADED && rde->type == Resource::RT_MUSIC) {
		_resNum = resNum;
		memset(&_sfxMod, 0, sizeof(SfxModule));
		_sfxMod.curOrder = pos;
		_sfxMod.numOrder = READ_BE_UINT16(rde->bufPtr + 0x3E);
		memcpy(_sfxMod.orderTable, rde->bufPtr + 0x40, 0x80);
		if (delay == 0) {
			_delay = READ_BE_UINT16(rde->bufPtr);
		} else {
			_delay = delay;
		}
		_delay = _delay * 60 / 7050;
		_sfxMod.data = rde->bufPtr + 0xC0;
		prepareInstruments(rde->bufPtr + 2);
	} else {
		warning("Invalid resData 0x%X", resNum);
	}
}

void SfxPlayer::prepareInstruments(const uint8 *p) {
	memset(_sfxMod.samples, 0, sizeof(_sfxMod.samples));
	for (int i = 0; i < 15; ++i) {
		SfxInstrument *ins = &_sfxMod.samples[i];
		uint16 resNum = READ_BE_UINT16(p); p += 2;
		if (resNum != 0) {
			ins->volume = READ_BE_UINT16(p);
			ResDataEntry *rde = &_res->_resDataList[resNum];
			if (rde->state == Resource::RS_LOADED && rde->type == Resource::RT_SOUND) {
				ins->data = rde->bufPtr;
				memset(ins->data + 8, 0, 4);
			} else {
				error("Error loading instrument 0x%X", resNum);
			}
		}
		p += 2; // skip volume
	}
}

void SfxPlayer::start() {
	MutexStack(_stub, _mutex);
	_sfxMod.curPos = 0;
	_timerId = _stub->addTimer(this, _delay);			
}

void SfxPlayer::stop() {
	MutexStack(_stub, _mutex);
	if (_resNum != 0) {
		_resNum = 0;
		_mix->stopAll();
		_stub->removeTimer(_timerId);
	}
}

void SfxPlayer::handleEvents() {
	MutexStack(_stub, _mutex);
	uint8 order = _sfxMod.orderTable[_sfxMod.curOrder];
	const uint8 *patternData = _sfxMod.data + _sfxMod.curPos + order * 1024;
	for (uint8 ch = 0; ch < 4; ++ch) {
		handlePattern(ch, patternData);
		patternData += 4;
	}
	_sfxMod.curPos += 4 * 4;
	if (_sfxMod.curPos >= 1024) {
		_sfxMod.curPos = 0;
		order = _sfxMod.curOrder + 1;
		if (order == _sfxMod.numOrder) {
			_resNum = 0;
			_stub->removeTimer(_timerId);
			_mix->stopAll();
		}
		_sfxMod.curOrder = order;
	}
}

void SfxPlayer::handlePattern(uint8 channel, const uint8 *data) {
	SfxPattern pat;
	memset(&pat, 0, sizeof(SfxPattern));
	pat.note_1 = READ_BE_UINT16(data + 0);
	pat.note_2 = READ_BE_UINT16(data + 2);
	if (pat.note_1 != 0xFFFD) {
		uint16 sample = (pat.note_2 & 0xF000) >> 12;
		if (sample != 0) {
			uint8 *ptr = _sfxMod.samples[sample - 1].data;
			if (ptr != 0) {
				pat.sampleVolume = _sfxMod.samples[sample - 1].volume;
				pat.sampleStart = 8;
				pat.sampleBuffer = ptr;
				pat.sampleLen = READ_BE_UINT16(ptr) * 2;
				uint16 loopLen = READ_BE_UINT16(ptr + 2) * 2;
				if (loopLen != 0) {
					pat.loopPos = pat.sampleLen;
					pat.loopData = ptr;
					pat.loopLen = loopLen;
				} else {
					pat.loopPos = 0;
					pat.loopData = 0;
					pat.loopLen = 0;
				}
				int16 m = pat.sampleVolume;
				uint8 effect = (pat.note_2 & 0x0F00) >> 8;
				if (effect == 5) { // volume up
					uint8 volume = (pat.note_2 & 0xFF);
					m += volume;
					if (m > 0x3F) {
						m = 0x3F;
					}
				} else if (effect == 6) { // volume down
					uint8 volume = (pat.note_2 & 0xFF);
					m -= volume;
					if (m < 0) {
						m = 0;
					}	
				}
				_mix->setChannelVolume(channel, m);
				pat.sampleVolume = m;
			}
		}
	}
	if (pat.note_1 == 0xFFFD) {
		*_markVar = pat.note_2;
	} else if (pat.note_1 != 0) {
		if (pat.note_1 == 0xFFFE) {
			_mix->stopChannel(channel);
		} else if (pat.sampleBuffer != 0) {
			MixerChunk mc;
			memset(&mc, 0, sizeof(mc));
			mc.data = pat.sampleBuffer + pat.sampleStart;
			mc.len = pat.sampleLen;
			mc.loopPos = pat.loopPos;
			mc.loopLen = pat.loopLen;
			assert(pat.note_1 >= 0x37 && pat.note_1 < 0x1000);
			// convert amiga period value to hz
			uint16 freq = 7159092 / (pat.note_1 * 2);
			_mix->playChannel(channel, &mc, freq, pat.sampleVolume);
		}
	}
}

void SfxPlayer::eventsCallback(void *param) {
	SfxPlayer *p = (SfxPlayer *)param;
	while(p->_delay) {
		rtems_task_wake_after((rtems_interval)(p->_delay*TICKS_PER_SECOND)/1000);
		p->handleEvents();
	}
	rtems_task_delete(RTEMS_SELF);
	/*if(p->_delay)
		rtems_timer_fire_after(
			timer,
			(rtems_interval)(p->_delay*TICKS_PER_SECOND)/1000,
			(rtems_timer_service_routine_entry) SfxPlayer::eventsCallback,
			param
		);*/
}
