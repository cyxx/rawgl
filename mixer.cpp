
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <SDL.h>
#include <SDL_mixer.h>
#include "file.h"
#include "mixer.h"
#include "sfxplayer.h"
#include "systemstub.h"

struct Mixer_impl {

	static const int kMixFreq = 22050;
	static const int kMixBufSize = 4096;

	Mix_Chunk *_sounds[4];
	uint8_t *_samples[4];
	Mix_Music *_music;

	void init() {
		memset(_sounds, 0, sizeof(_sounds));
		memset(_samples, 0, sizeof(_samples));
		_music = 0;

		Mix_Init(MIX_INIT_OGG);
		if (Mix_OpenAudio(kMixFreq, AUDIO_S16SYS, 2, kMixBufSize) < 0) {
			warning("Mix_OpenAudio failed: %s", Mix_GetError());
		}
		Mix_AllocateChannels(4);
	}
	void quit() {
		stopAll();
		Mix_CloseAudio();
		Mix_Quit();
	}

	static uint8_t *convertSampleRaw(const uint8_t *data, int freq, uint32_t *size) {
		SDL_AudioCVT cvt;
		memset(&cvt, 0, sizeof(cvt));
		if (SDL_BuildAudioCVT(&cvt, AUDIO_S8, 1, freq, AUDIO_S16SYS, 2, kMixFreq) < 0) {
			warning("Failed to resample from %d Hz", freq);
			return 0;
		}
		const int len = READ_BE_UINT16(data) * 2;
		const int loopLen = READ_BE_UINT16(data + 2) * 2;
		data += 8;
		if (loopLen != 0) {
			// loopPos = len;
		}
		if (cvt.needed) {
			cvt.len = len;
			cvt.buf = (uint8_t *)calloc(1, len * cvt.len_mult);
			if (cvt.buf) {
				memcpy(cvt.buf, data, len);
				SDL_ConvertAudio(&cvt);
			}
			*size = cvt.len_cvt;
			return cvt.buf;
		} else {
			uint8_t *buf = (uint8_t *)malloc(len);
			if (buf) {
				memcpy(buf, data, len);
			}
			*size = len;
			return buf;
		}
	}

	void playSoundRaw(uint8_t channel, const uint8_t *data, uint16_t freq, uint8_t volume) {
		stopSound(channel);
		uint32_t size;
		uint8_t *sample = convertSampleRaw(data, freq, &size);
		if (sample) {
			Mix_Chunk *chunk = Mix_QuickLoad_RAW(sample, size);
			playSound(channel, volume, chunk);
			_samples[channel] = sample;
		}
	}
	void playSoundWav(uint8_t channel, const uint8_t *data, uint8_t volume) {
		uint32_t size = READ_LE_UINT32(data + 4) + 8;
		// check format for QuickLoad
		bool canQuickLoad = (AUDIO_S16SYS == AUDIO_S16LSB);
		if (canQuickLoad) {
			if (memcmp(data + 8, "WAVEfmt ", 8) == 0 && READ_LE_UINT32(data + 16) == 16) {
				const uint8_t *fmt = data + 20;
				const int format = READ_LE_UINT16(fmt);
				const int channels = READ_LE_UINT16(fmt + 2);
				const int rate = READ_LE_UINT32(fmt + 4);
				const int bits = READ_LE_UINT16(fmt + 14);
				debug(DBG_SND, "wave format %d channels %d rate %d bits %d", format, channels, rate, bits);
				canQuickLoad = (format == 1 && channels == 2 && rate == kMixFreq && bits == 16);
			}
                }
		if (canQuickLoad) {
			Mix_Chunk *chunk = Mix_QuickLoad_WAV(const_cast<uint8_t *>(data));
			playSound(channel, volume, chunk);
		} else {
			SDL_RWops *rw = SDL_RWFromConstMem(data, size);
			Mix_Chunk *chunk = Mix_LoadWAV_RW(rw, 1);
			playSound(channel, volume, chunk);
		}
	}
	void playSound(uint8_t channel, int volume, Mix_Chunk *chunk) {
		if (chunk) {
			Mix_PlayChannel(channel, chunk, 0);
		}
		setChannelVolume(channel, volume);
		_sounds[channel] = chunk;
	}
	void stopSound(uint8_t channel) {
		Mix_HaltChannel(channel);
		Mix_FreeChunk(_sounds[channel]);
		_sounds[channel] = 0;
		free(_samples[channel]);
		_samples[channel] = 0;
	}
	void setChannelVolume(uint8_t channel, uint8_t volume) {
		Mix_Volume(channel, volume * MIX_MAX_VOLUME / 63);
	}

	void playMusic(const char *path) {
		stopMusic();
		_music = Mix_LoadMUS(path);
		if (_music) {
			Mix_VolumeMusic(MIX_MAX_VOLUME / 2);
			Mix_PlayMusic(_music, 0);
		}
	}
	void stopMusic() {
		Mix_HaltMusic();
		Mix_FreeMusic(_music);
		_music = 0;
	}

	static void mixSfxPlayer(void *data, uint8_t *s16buf, int len) {
		len /= 2;
		int8_t s8buf[len];
		memset(s8buf, 0, sizeof(s8buf));
		((SfxPlayer *)data)->readSamples(s8buf, len / 2);
		for (int i = 0; i < len; ++i) {
			*(int16_t *)&s16buf[i * 2] = 256 * (int16_t)s8buf[i];
		}
	}

	void playSfxMusic(SfxPlayer *sfx) {
		Mix_HookMusic(mixSfxPlayer, sfx);
	}
	void stopSfxMusic() {
		Mix_HookMusic(0, 0);
	}

	void stopAll() {
		for (int i = 0; i < 4; ++i) {
			stopSound(i);
		}
		stopMusic();
		stopSfxMusic();
	}
};


struct MixerChannel {
	virtual ~MixerChannel() {}
	virtual bool load(int rate) = 0;
	virtual bool readSample(int16_t &sampleL, int16_t &sampleR) = 0;
};

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

struct MixerChannel_S8Buffer: MixerChannel {
	const uint8_t *_buf;
	int _freq;
	Frac _pos;
	int _len;
	int _loopPos;
	int _loopLen;

	virtual bool load(int rate) {
		_len = READ_BE_UINT16(_buf) * 2;
		_loopLen = READ_BE_UINT16(_buf + 2) * 2;
		if (_loopLen != 0) {
			_loopPos = _len;
		}
		_buf += 8; // skip header
		_pos.offset = 0;
		_pos.inc = (_freq << Frac::BITS) / (uint32_t)rate;
		return true;
	}
	virtual bool readSample(int16_t &sampleL, int16_t &sampleR) {
		int pos = _pos.offset >> Frac::BITS;
		_pos.offset += _pos.inc;
		if (_loopLen != 0) {
			if (pos == _loopPos + _loopLen) {
				pos = _loopPos;
				_pos.offset = _loopPos << Frac::BITS;
			}
		} else {
			if (pos == _len) {
				return false;
			}
		}
		sampleL = sampleR = _buf[pos] << 8; /* S8 to S16 */
		return true;
	}
};

struct MixerChannel_WavBuffer: MixerChannel {
	const uint8_t *_buf;
	Frac _pos;
	WavInfo _wi;
	int _size;

	virtual bool load(int rate) {
		memset(&_wi, 0, sizeof(_wi));
		_wi.read(_buf);
		_pos.offset = 0;
		_pos.inc = (_wi._rate << Frac::BITS) / (uint32_t)rate;
		_size = (_wi._channels == 2) ? 2 : 1;
		if (_wi._bits == 16) {
			_size *= 2;
		}
		return _wi._dataSize > 0;
	}
	virtual bool readSample(int16_t &sampleL, int16_t &sampleR) {
		const int pos = _pos.offset >> Frac::BITS;
		if (pos * _size >= _wi._dataSize) {
			return false;
		}
		_pos.offset += _pos.inc;
		int offset = pos * _size;
		if (_wi._bits == 8) {
			sampleL = (_wi._data[offset] ^ 0x80) << 8; /* U8 to S16 */
			++offset;
		} else {
			sampleL = READ_LE_UINT16(_wi._data + offset);
			offset += 2;
		}
		if (_wi._channels == 1) { /* mono */
			sampleR = sampleL;
		} else {
			if (_wi._bits == 8) {
				sampleR = (_wi._data[offset] ^ 0x80) << 8;
			} else {
				sampleR = READ_LE_UINT16(_wi._data + offset);
			}
		}
		return true;
	}
};

static int8_t addclamp(int a, int b) {
	int add = a + b;
	if (add < -128) {
		add = -128;
	} else if (add > 127) {
		add = 127;
	}
	return (int8_t)add;
}

Mixer::Mixer(SystemStub *stub) 
	: _sfx(0), _stub(stub) {
}

void Mixer::init() {
	memset(_channels, 0, sizeof(_channels));
	memset(_volumes, 0, sizeof(_volumes));
	_music = 0;
//	_stub->startAudio(Mixer::mixCallback, this);
	_impl = new Mixer_impl();
	_impl->init();
}

void Mixer::free() {
	stopAll();
//	_stub->stopAudio();
	_impl->quit();
	delete _impl;
}

void Mixer::playSoundRaw(uint8_t channel, const uint8_t *data, uint16_t freq, uint8_t volume) {
	debug(DBG_SND, "Mixer::playChannel(%d, %d, %d)", channel, freq, volume);
	if (_impl) {
		return _impl->playSoundRaw(channel, data, freq, volume);
	}
	assert(channel < NUM_CHANNELS);
	LockAudioStack las(_stub);
	MixerChannel_S8Buffer *ch = new MixerChannel_S8Buffer;
	ch->_buf = data;
	ch->_freq = freq;
	if (!ch->load(_stub->getOutputSampleRate())) {
		delete ch;
		return;
	}
	_channels[channel] = ch;
	_volumes[channel] = volume;
}

void Mixer::playSoundWav(uint8_t channel, const uint8_t *data, uint8_t volume) {
	debug(DBG_SND, "Mixer::playSoundWav(%d, %d)", channel, volume);
	if (_impl) {
		return _impl->playSoundWav(channel, data, volume);
	}
	LockAudioStack las(_stub);
	delete _channels[channel];
	MixerChannel_WavBuffer *ch = new MixerChannel_WavBuffer;
	ch->_buf = data;
	if (!ch->load(_stub->getOutputSampleRate())) {
		delete ch;
		return;
	}
	_channels[channel] = ch;
	_volumes[channel] = volume;
}

void Mixer::stopSound(uint8_t channel) {
	debug(DBG_SND, "Mixer::stopChannel(%d)", channel);
	if (_impl) {
		return _impl->stopSound(channel);
	}
	assert(channel < NUM_CHANNELS);
	LockAudioStack las(_stub);
	delete _channels[channel];
	_channels[channel] = 0;
}

void Mixer::setChannelVolume(uint8_t channel, uint8_t volume) {
	debug(DBG_SND, "Mixer::setChannelVolume(%d, %d)", channel, volume);
	if (_impl) {
		return _impl->setChannelVolume(channel, volume);
	}
	assert(channel < NUM_CHANNELS);
	LockAudioStack las(_stub);
	_volumes[channel] = volume;
}

void Mixer::playMusic(const char *path) {
	debug(DBG_SND, "Mixer::playMusic(%s)", path);
	if (_impl) {
		return _impl->playMusic(path);
	}
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
	if (_impl) {
		return _impl->stopMusic();
	}
	LockAudioStack las(_stub);
	delete _music;
	_music = 0;
}

void Mixer::playSfxMusic(int num) {
	debug(DBG_SND, "Mixer::playSfxMusic(%d)", num);
	if (_impl && _sfx) {
		_sfx->play(Mixer_impl::kMixFreq);
		return _impl->playSfxMusic(_sfx);
	}
	LockAudioStack las(_stub);
	if (_sfx) {
		_sfx->play(_stub->getOutputSampleRate());
	}
}

void Mixer::stopSfxMusic() {
	debug(DBG_SND, "Mixer::stopSfxMusic()");
	if (_impl && _sfx) {
		_sfx->stop();
		return _impl->stopSfxMusic();
	}
	LockAudioStack las(_stub);
	if (_sfx) {
		_sfx->stop();
	}
}

void Mixer::stopAll() {
	debug(DBG_SND, "Mixer::stopAll()");
	if (_impl) {
		return _impl->stopAll();
	}
	LockAudioStack las(_stub);
	for (uint8_t i = 0; i < NUM_CHANNELS; ++i) {
		delete _channels[i];
	}
	memset(_channels, 0, sizeof(_channels));
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
	if (_sfx && _sfx->_playing) {
		_sfx->readSamples(buf, len);
	}
	for (uint8_t i = 0; i < NUM_CHANNELS; ++i) {
		MixerChannel *ch = _channels[i];
		if (ch) {
			int8_t *pBuf = buf;
			for (int j = 0; j < len; ++j) {
				int16_t sbuf[2];
				if (!ch->readSample(sbuf[0], sbuf[1])) {
					delete ch;
					_channels[i] = 0;
					break;
				}
				int16_t sample;
				sample = (sbuf[0] >> 8) * _volumes[i] / 64;
				*pBuf = addclamp(sample, *pBuf);
				++pBuf;
				sample = (sbuf[1] >> 8) * _volumes[i] / 64;
				*pBuf = addclamp(sample, *pBuf);
				++pBuf;
			}
		}
	}
}

void Mixer::mixCallback(void *param, uint8_t *buf, int len) {
	memset(buf, 0, len);
	((Mixer *)param)->mix((int8_t *)buf, len / 2);
}
