/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */

#include "systemstub.h"
#include "mixer.h"
#include "sound.h"

static int8 addclamp(int a, int b) {
	int add = a + b;
	if (add < -128) {
		add = -128;
	}
	else if (add > 127) {
		add = 127;
	}
	return (int8)add;
}

Mixer::Mixer(SystemStub *stub) 
	: _stub(stub) {
}

void Mixer::init() {
	memset(_channels, 0, sizeof(_channels));
	_mutex = _stub->createMutex();
	_stub->startAudio(Mixer::mixCallback, this);
}

void Mixer::free() {
	stopAll();
	_stub->stopAudio();
	_stub->destroyMutex(_mutex);
}

void Mixer::playChannel(uint8 channel, const MixerChunk *mc, uint16 freq, uint8 volume) {
	assert(channel < NUM_CHANNELS);
	MutexStack(_stub, _mutex);
	MixerChannel *ch = &_channels[channel];
	ch->active = true;
	ch->volume = volume;
	ch->chunk = *mc;
	ch->chunkPos = 0;
	ch->chunkInc = (freq << 8) / _stub->getOutputSampleRate();
}

void Mixer::stopChannel(uint8 channel) {
	assert(channel < NUM_CHANNELS);
	MutexStack(_stub, _mutex);	
	_channels[channel].active = false;
}

void Mixer::setChannelVolume(uint8 channel, uint8 volume) {
	assert(channel < NUM_CHANNELS);
	MutexStack(_stub, _mutex);
	_channels[channel].volume = volume;
}

void Mixer::stopAll() {
	MutexStack(_stub, _mutex);
	for (uint8 i = 0; i < NUM_CHANNELS; ++i) {
		_channels[i].active = false;		
	}
}

void Mixer::mix(void *sbuf) {
	MutexStack(_stub, _mutex);
	struct soundBufParams * _sbuf = (struct soundBufParams *) sbuf;
	int len, n;
	len = _sbuf->len >> 1;
	int8 *buf = (int8*)_sbuf->tmp;
	int8 *d8 = (int8*)_sbuf->buf[_sbuf->frame];

	for (n = 0; n < len; n++) {
		buf[n] = 0;
	}

	for (uint8 i = 0; i < NUM_CHANNELS; ++i) {
		MixerChannel *ch = &_channels[i];
		if (ch->active) {
			for (int j = 0; j < len; ++j) {
				uint16 p1, p2;
				uint16 ilc = (ch->chunkPos & 0xFF);
				p1 = ch->chunkPos >> 8;
				ch->chunkPos += ch->chunkInc;
				if (ch->chunk.loopLen != 0) {
					if (p1 >= ch->chunk.loopPos + ch->chunk.loopLen - 1) {
						ch->chunkPos = p2 = ch->chunk.loopPos;
					} else {
						p2 = p1 + 1;
					}
				} else {
					if (p1 >= ch->chunk.len - 1) {
						ch->active = false;
						break;
					} else {
						p2 = p1 + 1;
					}
				}
				// interpolate
				int8 b1 = *(int8 *)(ch->chunk.data + p1);
				int8 b2 = *(int8 *)(ch->chunk.data + p2);
				int8 b = (int8)((b1 * (0xFF - ilc) + b2 * ilc) >> 8);
				// set volume and clamp
				buf[j] = addclamp(buf[j], (int)b * ch->volume / 64);
			}
		}
	}

	for (n = 0; n < len; n++) {
		int8 smp = *buf++;
		*d8++ = smp;
		*d8++ = smp;
	}  

}

void Mixer::mixCallback(void *param, void *sbuf) {
	((Mixer *)param)->mix((void *)sbuf);
}
