
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "mixer.h"
#include "systemstub.h"


void MixerChunk::readRaw(uint8_t *buf) {
	data = buf + 8; // skip header
	len = READ_BE_UINT16(buf) * 2;
	loopLen = READ_BE_UINT16(buf + 2) * 2;
	if (loopLen != 0) {
		loopPos = len;
	}
}

void MixerChunk::readWav(uint8_t *buf) {
	if (memcmp(buf, "RIFF", 4) == 0 && memcmp(buf + 8, "WAVEfmt ", 8) == 0) {
		const int format = READ_LE_UINT16(buf + 20);
		const int channels = READ_LE_UINT16(buf + 22);
		const int rate = READ_LE_UINT16(buf + 24);
		const int bits = READ_LE_UINT16(buf + 34);
		if ((format & 255) == 1 && channels == 1 && bits == 8 && memcmp(buf + 36, "data", 4) == 0) {
			len = READ_LE_UINT32(buf + 40);
			data = buf + 44;
			loopLen = 0;
			if (buf[21] == 0) { /* S8 to U8 */
				for (int i = 0; i < len; ++i) {
					buf[44 + i] ^= 0x80;
				}
				buf[21] = 1;
			}
		} else {
			warning("Unsupported WAV format=%d channels=%d bits=%d rate=%d", format, channels, bits, rate);
		}
	}
}

static int8_t addclamp(int a, int b) {
	int add = a + b;
	if (add < -128) {
		add = -128;
	}
	else if (add > 127) {
		add = 127;
	}
	return (int8_t)add;
}

Mixer::Mixer(SystemStub *stub) 
	: _stub(stub) {
}

void Mixer::init() {
	memset(_channels, 0, sizeof(_channels));
	_stub->startAudio(Mixer::mixCallback, this);
}

void Mixer::free() {
	stopAll();
	_stub->stopAudio();
}

void Mixer::playChannel(uint8_t channel, const MixerChunk *mc, uint16_t freq, uint8_t volume) {
	debug(DBG_SND, "Mixer::playChannel(%d, %d, %d)", channel, freq, volume);
	assert(channel < NUM_CHANNELS);
	LockAudioStack las(_stub);
	MixerChannel *ch = &_channels[channel];
	ch->active = true;
	ch->volume = volume;
	ch->chunk = *mc;
	ch->chunkPos = 0;
	ch->chunkInc = (freq << 8) / _stub->getOutputSampleRate();
}

void Mixer::stopChannel(uint8_t channel) {
	debug(DBG_SND, "Mixer::stopChannel(%d)", channel);
	assert(channel < NUM_CHANNELS);
	LockAudioStack las(_stub);
	_channels[channel].active = false;
}

void Mixer::setChannelVolume(uint8_t channel, uint8_t volume) {
	debug(DBG_SND, "Mixer::setChannelVolume(%d, %d)", channel, volume);
	assert(channel < NUM_CHANNELS);
	LockAudioStack las(_stub);
	_channels[channel].volume = volume;
}

void Mixer::stopAll() {
	debug(DBG_SND, "Mixer::stopAll()");
	LockAudioStack las(_stub);
	for (uint8_t i = 0; i < NUM_CHANNELS; ++i) {
		_channels[i].active = false;		
	}
}

void Mixer::mix(int8_t *buf, int len) {
	memset(buf, 0, len);
	for (uint8_t i = 0; i < NUM_CHANNELS; ++i) {
		MixerChannel *ch = &_channels[i];
		if (ch->active) {
			int8_t *pBuf = buf;
			for (int j = 0; j < len; ++j, ++pBuf) {
				uint16_t p1, p2;
				uint16_t ilc = (ch->chunkPos & 0xFF);
				p1 = ch->chunkPos >> 8;
				ch->chunkPos += ch->chunkInc;
				if (ch->chunk.loopLen != 0) {
					if (p1 == ch->chunk.loopPos + ch->chunk.loopLen - 1) {
						debug(DBG_SND, "Looping sample on channel %d", i);
						ch->chunkPos = p2 = ch->chunk.loopPos;
					} else {
						p2 = p1 + 1;
					}
				} else {
					if (p1 == ch->chunk.len - 1) {
						debug(DBG_SND, "Stopping sample on channel %d", i);
						ch->active = false;
						break;
					} else {
						p2 = p1 + 1;
					}
				}
				// interpolate
				int8_t b1 = *(int8_t *)(ch->chunk.data + p1);
				int8_t b2 = *(int8_t *)(ch->chunk.data + p2);
				int8_t b = (int8_t)((b1 * (0xFF - ilc) + b2 * ilc) >> 8);
				// set volume and clamp
				*pBuf = addclamp(*pBuf, (int)b * ch->volume / 0x40);
			}
		}
	}
}

void Mixer::mixCallback(void *param, uint8_t *buf, int len) {
	((Mixer *)param)->mix((int8_t *)buf, len);
}
