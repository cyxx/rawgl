
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include "file.h"
#include "mixer.h"
#include "systemstub.h"


struct MixerChannel {
	virtual ~MixerChannel() {}
	virtual bool load(int rate) = 0;
	virtual bool readSample(int16_t &sampleL, int16_t &sampleR) = 0;
};

struct Frac {
	static const int BITS = 16;
	uint32_t inc;
	uint64_t offset;
};

void MixerChunk::readRaw(uint8_t *buf) {
	data = buf + 8; // skip header
	len = READ_BE_UINT16(buf) * 2;
	loopLen = READ_BE_UINT16(buf + 2) * 2;
	if (loopLen != 0) {
		loopPos = len;
	}
}

struct WavInfo {
	int _channels;
	int _rate;
	int _bits;
	const uint8_t *_data;
	int _dataSize;

	void read(const uint8_t *src);
};

void WavInfo::read(const uint8_t *src) {
	_data = 0;
	if (memcmp(src, "RIFF", 4) == 0 && memcmp(src + 8, "WAVE", 4) == 0) {
		src += 12;
		while (_data == 0) {
			const int size = READ_LE_UINT32(src + 4);
			if (memcmp(src, "fmt ", 4) == 0) {
				const int format = READ_LE_UINT16(src + 8);
				if (format != 1) {
					warning("Unsupported WAV compression %d", format);
					return;
				}
				_channels = READ_LE_UINT16(src + 10);
				_rate = READ_LE_UINT16(src + 12);
				_bits = READ_LE_UINT16(src + 22);
			} else if (memcmp(src, "data", 4) == 0) {
				_dataSize = size;
				_data = src + 8;
			} else {
				// 'minf' 'elm1' 'bext'
			}
			src += size + 8;
		}
	}
}

void MixerChunk::readWav(uint8_t *buf) {
	WavInfo wi;
	wi.read(buf);
	if (wi._data) {
		if (wi._channels != 1 || (wi._bits != 8 && wi._bits != 16)) {
			warning("Unsupported WAV channels %d bits %d", wi._channels, wi._bits);
			return;
		}
		len = wi._dataSize;
		data = wi._data;
		if (wi._bits == 16) {
			fmt = FMT_S16;
			len /= 2;
		} else {
			fmt = FMT_U8;
		}
	}
}

static int16_t interpolate(int16_t sample1, int16_t sample2, const Frac &f) {
	static const int MASK = (1 << Frac::BITS) - 1;
	const int fp = f.inc & MASK;
	return (sample1 * (MASK - fp) + sample2 * fp) >> Frac::BITS;
}

struct MixerChannel_Wav: MixerChannel {
	File _f;
	Frac _sfrac;
	WavInfo _wi;
	int16_t _sbuf[2];
	int _pos;

	virtual bool load(int rate) {
		uint8_t buf[44];
		_f.read(buf, sizeof(buf));
		_wi.read(buf);
		const bool valid = (_wi._dataSize != 0 && (_wi._channels == 1 || _wi._channels == 2) && (_wi._bits == 8 || _wi._bits == 16));
		if (valid) {
			readNextSample(&_sbuf[0]);
			_pos = 0;
			_sfrac.offset = 0;
			_sfrac.inc = (_wi._rate << Frac::BITS) / (uint32_t)rate;
		}
		return valid;
	}
	void readNextSample(int16_t *samples) {
		for (int i = 0; i < 2; ++i) {
			if (_wi._bits == 8) {
				samples[i] = (_f.readByte() ^ 0x80) << 8; /* U8 to S16 */
			} else {
				samples[i] = _f.readUint16LE();
			}
			if (i == 0 && _wi._channels == 1) { /* mono */
				samples[1] = samples[0];
				break;
			}
		}
	}
	virtual bool readSample(int16_t &sampleL, int16_t &sampleR) {
		const int pos = _sfrac.offset >> Frac::BITS;
		if (pos >= _wi._dataSize) {
			return false;
		} else if (pos > _pos) {
			for (; _pos < pos; ++_pos) {
				readNextSample(&_sbuf[0]);
			}
			if (_f.ioErr()) {
				sampleL = _sbuf[0];
				sampleR = _sbuf[1];
				return true;
			}
		}
		sampleL = _sbuf[0];
		sampleR = _sbuf[1];
		_sfrac.offset += _sfrac.inc;
		return true;
	}
};

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
	_music = 0;
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
	OldMixerChannel *ch = &_channels[channel];
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

void Mixer::playMusic(const char *path) {
	debug(DBG_SND, "Mixer::playMusic(%s)", path);
	LockAudioStack las(_stub);
	const char *ext = strrchr(path, '.');
	if (ext) {
		if (strcasecmp(ext, ".wav") == 0) {
			MixerChannel_Wav *ch = new MixerChannel_Wav;
			if (ch->_f.open(path) && ch->load(_stub->getOutputSampleRate())) {
				_music = ch;
			} else {
				warning("Unable to load '%s'", path);
				delete ch;
			}
		} else if (strcasecmp(ext, ".ogg") == 0) {
			warning("Unsupported .ogg playback for '%s'", path);
		}
	}
}

void Mixer::stopMusic() {
	debug(DBG_SND, "Mixer::stopMusic()");
	LockAudioStack las(_stub);
	delete _music;
	_music = 0;
}

void Mixer::stopAll() {
	debug(DBG_SND, "Mixer::stopAll()");
	LockAudioStack las(_stub);
	for (uint8_t i = 0; i < NUM_CHANNELS; ++i) {
		_channels[i].active = false;		
	}
}

void Mixer::mix(int8_t *buf, int len) {
	if (_music) {
		int8_t *pBuf = buf;
		for (int j = 0; j < len; ++j) {
			int16_t sbuf[2];
			if (!_music->readSample(sbuf[0], sbuf[1])) {
				delete _music;
				_music = 0;
				break;
			}
			*pBuf++ = sbuf[0] >> 8;
			*pBuf++ = sbuf[1] >> 8;
		}
	}
	for (uint8_t i = 0; i < NUM_CHANNELS; ++i) {
		OldMixerChannel *ch = &_channels[i];
		if (ch->active) {
			int8_t *pBuf = buf;
			for (int j = 0; j < len; ++j) {
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
				if (ch->chunk.fmt == MixerChunk::FMT_S16) { /* S16 to S8, skip LSB */
					p1 = (p1 * 2) + 1;
					p2 = (p2 * 2) + 1;
				}
				// interpolate
				int8_t b1 = *(int8_t *)(ch->chunk.data + p1);
				int8_t b2 = *(int8_t *)(ch->chunk.data + p2);
				if (ch->chunk.fmt == MixerChunk::FMT_U8) { /* U8 to S8 */
					b1 ^= 0x80;
					b2 ^= 0x80;
				}
				int8_t b = (int8_t)((b1 * (0xFF - ilc) + b2 * ilc) >> 8);
				// set volume and clamp
				pBuf[0] = addclamp(pBuf[0], (int)b * ch->volume / 0x40);
				// stereo samples
				pBuf[1] = pBuf[0];
				pBuf += 2;
			}
		}
	}
}

void Mixer::mixCallback(void *param, uint8_t *buf, int len) {
	memset(buf, 0, len);
	((Mixer *)param)->mix((int8_t *)buf, len / 2);
}
