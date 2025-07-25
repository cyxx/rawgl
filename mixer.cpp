
/*
 * Another World engine rewrite
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <SDL.h>
#define MIX_INIT_FLUIDSYNTH MIX_INIT_MID // renamed with SDL2_mixer >= 2.0.2
#include <SDL_mixer.h>
#include <map>
#include "aifcplayer.h"
#include "mixer.h"
#include "sfxplayer.h"
#include "util.h"
#define MT32EMU_API_TYPE 1
#include <mt32emu.h>

enum {
	TAG_RIFF = 0x46464952,
	TAG_WAVE = 0x45564157,
	TAG_fmt  = 0x20746D66,
	TAG_data = 0x61746164
};

static const bool kAmigaStereoChannels = false; // 0,3:left 1,2:right

static int16_t toS16(int a) {
	return ((a << 8) | a) - 32768;
}

static int16_t mixS16(int sample1, int sample2) {
	const int sample = sample1 + sample2;
	return sample < -32768 ? -32768 : ((sample > 32767 ? 32767 : sample));
}

struct MixerChannel {
	const uint8_t *_data;
	Frac _pos;
	uint32_t _len;
	uint32_t _loopLen, _loopPos;
	int _volume;
	void (MixerChannel::*_mixWav)(int16_t *sample, int count);

	void initRaw(const uint8_t *data, int freq, int volume, int mixingFreq) {
		_data = data + 8;
		_pos.reset(freq, mixingFreq);

		const int len = READ_BE_UINT16(data) * 2;
		_loopLen = READ_BE_UINT16(data + 2) * 2;
		_loopPos = _loopLen ? len : 0;
		_len = len;

		_volume = volume;
	}

	void initWav(const uint8_t *data, int freq, int volume, int mixingFreq, int len, bool bits16, bool stereo, bool loop) {
		_data = data;
		_pos.reset(freq, mixingFreq);

		_len = len;
		_loopLen = loop ? len : 0;
		_loopPos = 0;
		_volume = volume;
		_mixWav = bits16 ? (stereo ? &MixerChannel::mixWav<16, true> : &MixerChannel::mixWav<16, false>) : (stereo ? &MixerChannel::mixWav<8, true> : &MixerChannel::mixWav<8, false>);
	}
	void mixRaw(int16_t &sample) {
		if (_data) {
			uint32_t pos = _pos.getInt();
			_pos.offset += _pos.inc;
			if (_loopLen != 0) {
				if (pos >= _loopPos + _loopLen) {
					pos = _loopPos;
					_pos.offset = (_loopPos << Frac::BITS) + _pos.inc;
				}
			} else {
				if (pos >= _len) {
					_data = 0;
					return;
				}
			}
			sample = mixS16(sample, toS16(_data[pos] ^ 0x80) * _volume / 64);
		}
	}

	template<int bits, bool stereo>
	void mixWav(int16_t *samples, int count) {
		for (int i = 0; i < count; i += 2) {
			uint32_t pos = _pos.getInt();
			_pos.offset += _pos.inc;
			if (pos >= _len) {
				if (_loopLen != 0) {
					pos = 0;
					_pos.offset = _pos.inc;
				} else {
					_data = 0;
					break;
				}
			}
			if (stereo) {
				pos *= 2;
			}
			int valueL;
			if (bits == 8) { // U8
				valueL = toS16(_data[pos]) * _volume / 64;
			} else { // S16
				valueL = ((int16_t)READ_LE_UINT16(&_data[pos * sizeof(int16_t)])) * _volume / 64;
			}
			*samples = mixS16(*samples, valueL);
			++samples;

			int valueR;
			if (!stereo) {
				valueR = valueL;
			} else {
				if (bits == 8) { // U8
					valueR = toS16(_data[pos + 1]) * _volume / 64;
				} else { // S16
					valueR = ((int16_t)READ_LE_UINT16(&_data[(pos + 1) * sizeof(int16_t)])) * _volume / 64;
				}
			}
			*samples = mixS16(*samples, valueR);
			++samples;
		}
	}
};

static const uint8_t *loadWav(const uint8_t *data, int &freq, int &len, bool &bits16, bool &stereo) {
	uint32_t riffMagic = READ_LE_UINT32(data);
	if (riffMagic != TAG_RIFF) return 0;
	uint32_t riffLength = READ_LE_UINT32(data + 4);
	uint32_t waveMagic = READ_LE_UINT32(data + 8);
	if (waveMagic != TAG_WAVE) return 0;
	uint32_t offset = 12;
	uint32_t chunkMagic, chunkLength = 0;
	// find fmt chunk
	do {
		offset += chunkLength + (chunkLength & 1);
		if (offset >= riffLength) return 0;
		chunkMagic = READ_LE_UINT32(data + offset);
		chunkLength = READ_LE_UINT32(data + offset + 4);
		offset += 8;
	} while (chunkMagic != TAG_fmt);

	if (chunkLength < 14) return 0;
	if (offset + chunkLength >= riffLength) return 0;

	// read format
	int formatTag = READ_LE_UINT16(data + offset);
	int channels = READ_LE_UINT16(data + offset + 2);
	int samplesPerSec = READ_LE_UINT32(data + offset + 4);
	int bitsPerSample = 0;
	if (chunkLength >= 16) {
		bitsPerSample = READ_LE_UINT16(data + offset + 14);
	} else if (formatTag == 1 && channels != 0) {
		int blockAlign = READ_LE_UINT16(data + offset + 12);
		bitsPerSample = (blockAlign * 8) / channels;
	}
	// check supported format
	if ((formatTag != 1) || // PCM
		(channels != 1 && channels != 2) || // mono or stereo
		(bitsPerSample != 8 && bitsPerSample != 16)) { // 8bit or 16bit
		warning("Unsupported wave file");
		return 0;
	}

	// find data chunk
	do {
		offset += chunkLength + (chunkLength & 1);
		if (offset >= riffLength) return 0;
		chunkMagic = READ_LE_UINT32(data + offset);
		chunkLength = READ_LE_UINT32(data + offset + 4);
		offset += 8;
	} while (chunkMagic != TAG_data);

	uint32_t lengthSamples = chunkLength;
	if (offset + lengthSamples - 4 > riffLength) {
		lengthSamples = riffLength + 4 - offset;
	}
	if (channels == 2) lengthSamples >>= 1;
	if (bitsPerSample == 16) lengthSamples >>= 1;

	freq = samplesPerSec;
	len = lengthSamples;
	bits16 = (bitsPerSample == 16);
	stereo = (channels == 2);

	return data + offset;
}

struct Mixer_impl {

	static const int kMixFreq = 44100;
	static const SDL_AudioFormat kMixFormat = AUDIO_S16SYS;
	static const int kMixSoundChannels = 2;
	static const int kMixBufSize = 4096;
	static const int kMixChannels = 4;

	Mix_Chunk *_sounds[kMixChannels];
	Mix_Music *_music;
	MixerChannel _channels[kMixChannels];
	SfxPlayer *_sfx;
	std::map<int, Mix_Chunk *> _preloads; // AIFF preloads (3DO)
	mt32emu_context _mt32;

	void init(MixerType mixerType) {
		memset(_sounds, 0, sizeof(_sounds));
		_music = 0;
		memset(_channels, 0, sizeof(_channels));
		for (int i = 0; i < kMixChannels; ++i) {
			_channels[i]._mixWav = &MixerChannel::mixWav<8, false>;
		}
		_sfx = 0;
		_mt32 = 0;

		Mix_Init(MIX_INIT_OGG | MIX_INIT_FLUIDSYNTH);
		if (Mix_OpenAudio(kMixFreq, kMixFormat, kMixSoundChannels, kMixBufSize) < 0) {
			warning("Mix_OpenAudio failed: %s", Mix_GetError());
		}
		switch (mixerType) {
		case kMixerTypeRaw:
			Mix_HookMusic(mixAudio, this);
			break;
		case kMixerTypeWav:
			Mix_SetPostMix(mixAudioWav, this);
			break;
		case kMixerTypeAiff:
			Mix_AllocateChannels(kMixChannels);
			break;
		case kMixerTypeMt32: {
				mt32emu_report_handler_i report = { 0 };
				_mt32 = mt32emu_create_context(report, this);
				mt32emu_add_rom_file(_mt32, "CM32L_CONTROL.ROM");
				mt32emu_add_rom_file(_mt32, "CM32L_PCM.ROM");
				mt32emu_set_stereo_output_samplerate(_mt32, kMixFreq);
				mt32emu_open_synth(_mt32);
				mt32emu_set_midi_delay_mode(_mt32, MT32EMU_MDM_IMMEDIATE);
			}
			Mix_HookMusic(mixAudio, this);
			break;
		}
	}
	void quit() {
		if (_mt32) {
			mt32emu_close_synth(_mt32);
			mt32emu_free_context(_mt32);
		}
		Mix_CloseAudio();
		Mix_Quit();
	}

	void update() {
		for (int i = 0; i < kMixChannels; ++i) {
			if (_sounds[i] && !Mix_Playing(i)) {
				freeSound(i);
			}
		}
	}

	void playSoundRaw(uint8_t channel, const uint8_t *data, int freq, uint8_t volume) {
		SDL_LockAudio();
		_channels[channel].initRaw(data, freq, volume, kMixFreq);
		SDL_UnlockAudio();
	}
	void playSoundWav(uint8_t channel, const uint8_t *data, int freq, uint8_t volume, bool loop) {
		int wavFreq, len;
		bool bits16, stereo;
		const uint8_t *wavData = loadWav(data, wavFreq, len, bits16, stereo);
		if (!wavData) return;

		if (wavFreq == 22050 || wavFreq == 44100 || wavFreq == 48000) {
			freq = (int)(freq * (wavFreq / 9943.0f));
		}

		SDL_LockAudio();
		_channels[channel].initWav(wavData, freq, volume, kMixFreq, len, bits16, stereo, loop);
		SDL_UnlockAudio();
	}
	void playSound(uint8_t channel, int volume, Mix_Chunk *chunk, int loops = 0) {
		stopSound(channel);
		if (chunk) {
			Mix_PlayChannel(channel, chunk, loops);
		}
		setChannelVolume(channel, volume);
		_sounds[channel] = chunk;
	}
	void stopSound(uint8_t channel) {
		if (_mt32) {
			stopSoundMt32();
		}
		SDL_LockAudio();
		_channels[channel]._data = 0;
		SDL_UnlockAudio();
		Mix_HaltChannel(channel);
		freeSound(channel);
	}
	static const uint8_t *findMt32Sound(int num) {
		for (const uint8_t *data = Mixer::_mt32SoundsTable; data[0]; data += 4) {
			if (data[0] == num) {
				return data;
			}
		}
		return 0;
	}
	void playSoundMt32(int num) {
		const uint8_t *data = findMt32Sound(num);
		if (data) {
			for (; data[0] == num; data += 4) {
				int8_t note = data[1];

				uint32_t noteOn = 0x99;
				noteOn |= ABS(note) << 8;
				noteOn |= 0x7f << 16;
				mt32emu_play_msg(_mt32, noteOn);

				uint32_t pitchBend = 0xe9;
				pitchBend |= (READ_LE_UINT16(data) & 0x7f) << 8;
				pitchBend |= (0x3f80 >> 7) << 16;
				mt32emu_play_msg(_mt32, pitchBend);

				if (note < 0) {
					uint32_t noteVel = 0x99;
					noteVel |= ABS(note) << 8;
					noteVel |= 0 << 16;
					mt32emu_play_msg(_mt32, noteVel);
				}
			}
		}
	}
	void stopSoundMt32() {
		uint32_t controlChange = 0xb9;
		controlChange |= 0x7b << 8;
		controlChange |= 0 << 16;
		mt32emu_play_msg(_mt32, controlChange);
	}
	void freeSound(int channel) {
		Mix_FreeChunk(_sounds[channel]);
		_sounds[channel] = 0;
	}
	void setChannelVolume(uint8_t channel, uint8_t volume) {
		SDL_LockAudio();
		_channels[channel]._volume = volume;
		SDL_UnlockAudio();
		Mix_Volume(channel, volume * MIX_MAX_VOLUME / 63);
	}

	void playMusic(const char *path, int loops = 0) {
		stopMusic();
		_music = Mix_LoadMUS(path);
		if (_music) {
			Mix_VolumeMusic(MIX_MAX_VOLUME / 2);
			Mix_PlayMusic(_music, loops);
		} else {
			warning("Failed to load music '%s', %s", path, Mix_GetError());
		}
	}
	void stopMusic() {
		Mix_HaltMusic();
		Mix_FreeMusic(_music);
		_music = 0;
	}

	static void mixAifcPlayer(void *data, uint8_t *s16buf, int len) {
		((AifcPlayer *)data)->readSamples((int16_t *)s16buf, len / 2);
	}
	void playAifcMusic(AifcPlayer *aifc) {
		Mix_HookMusic(mixAifcPlayer, aifc);
	}
	void stopAifcMusic() {
		Mix_HookMusic(0, 0);
	}

	void playSfxMusic(SfxPlayer *sfx) {
		SDL_LockAudio();
		_sfx = sfx;
		_sfx->play(kMixFreq);
		SDL_UnlockAudio();
	}
	void stopSfxMusic() {
		SDL_LockAudio();
		if (_sfx) {
			_sfx->stop();
			_sfx = 0;
		}
		SDL_UnlockAudio();
	}

	void mixChannels(int16_t *samples, int count) {
		if (kAmigaStereoChannels) {
			for (int i = 0; i < count; i += 2) {
				_channels[0].mixRaw(*samples);
				_channels[3].mixRaw(*samples);
				++samples;
				_channels[1].mixRaw(*samples);
				_channels[2].mixRaw(*samples);
				++samples;
			}
		}  else {
			for (int i = 0; i < count; i += 2) {
				for (int j = 0; j < kMixChannels; ++j) {
					_channels[j].mixRaw(samples[i]);
				}
				samples[i + 1] = samples[i];
			}
		}
	}

	static void mixAudio(void *data, uint8_t *s16buf, int len) {
		memset(s16buf, 0, len);
		Mixer_impl *mixer = (Mixer_impl *)data;
		if (mixer->_mt32) {
			mt32emu_render_bit16s(mixer->_mt32, (int16_t *)s16buf, len / (2 * sizeof(int16_t)));
		}
		mixer->mixChannels((int16_t *)s16buf, len / sizeof(int16_t));
		if (mixer->_sfx) {
			mixer->_sfx->readSamples((int16_t *)s16buf, len / sizeof(int16_t));
		}
	}

	void mixChannelsWav(int16_t *samples, int count) {
		for (int i = 0; i < kMixChannels; ++i) {
			if (_channels[i]._data) {
				(_channels[i].*_channels[i]._mixWav)(samples, count);
			}
		}
	}

	static void mixAudioWav(void *data, uint8_t *s16buf, int len) {
		Mixer_impl *mixer = (Mixer_impl *)data;
		mixer->mixChannelsWav((int16_t *)s16buf, len / sizeof(int16_t));
	}

	void stopAll(AifcPlayer *aifc, SfxPlayer *sfx) {
		for (int i = 0; i < kMixChannels; ++i) {
			stopSound(i);
		}
		stopMusic();
		if (sfx) {
			stopSfxMusic();
		}
		if (aifc) {
			stopAifcMusic();
		}
		for (std::map<int, Mix_Chunk *>::iterator it = _preloads.begin(); it != _preloads.end(); ++it) {
			debug(DBG_SND, "Flush preload %d", it->first);
			Mix_FreeChunk(it->second);
		}
		_preloads.clear();
	}

	void preloadSoundAiff(int num, const uint8_t *data) {
		if (_preloads.find(num) != _preloads.end()) {
			warning("AIFF sound %d is already preloaded", num);
		} else {
			const uint32_t size = READ_BE_UINT32(data + 4) + 8;
			SDL_RWops *rw = SDL_RWFromConstMem(data, size);
			Mix_Chunk *chunk = Mix_LoadWAV_RW(rw, 1);
			_preloads[num] = chunk;
		}
	}

	void playSoundAiff(int channel, int num, int volume) {
		if (_preloads.find(num) == _preloads.end()) {
			warning("AIFF sound %d is not preloaded", num);
		} else {
			Mix_Chunk *chunk = _preloads[num];
			Mix_PlayChannel(channel, chunk, 0);
			Mix_Volume(channel, volume * MIX_MAX_VOLUME / 63);
		}
	}
};

Mixer::Mixer(SfxPlayer *sfx)
	: _aifc(0), _sfx(sfx) {
}

void Mixer::init(MixerType mixerType) {
	_impl = new Mixer_impl();
	_impl->init(mixerType);
}

void Mixer::quit() {
	stopAll();
	if (_impl) {
		_impl->quit();
		delete _impl;
	}
	delete _aifc;
}

void Mixer::update() {
	if (_impl) {
		_impl->update();
	}
}

bool Mixer::hasMt32() const {
	return _impl && _impl->_mt32;
}

bool Mixer::hasMt32SoundMapping(int num) {
	return Mixer_impl::findMt32Sound(num) != 0;
}

void Mixer::playSoundRaw(uint8_t channel, const uint8_t *data, uint16_t freq, uint8_t volume) {
	debug(DBG_SND, "Mixer::playSoundRaw(%d, %d, %d)", channel, freq, volume);
	if (_impl) {
		return _impl->playSoundRaw(channel, data, freq, volume);
	}
}

void Mixer::playSoundWav(uint8_t channel, const uint8_t *data, uint16_t freq, uint8_t volume, uint8_t loop) {
	debug(DBG_SND, "Mixer::playSoundWav(%d, %d, %d)", channel, volume, loop);
	if (_impl) {
		return _impl->playSoundWav(channel, data, freq, volume, loop);
	}
}

void Mixer::stopSound(uint8_t channel) {
	debug(DBG_SND, "Mixer::stopSound(%d)", channel);
	if (_impl) {
		return _impl->stopSound(channel);
	}
}

void Mixer::playSoundMt32(int num) {
	debug(DBG_SND, "Mixer::playSoundMt32(%d)", num);
	if (_impl) {
		return _impl->playSoundMt32(num);
	}
}

void Mixer::setChannelVolume(uint8_t channel, uint8_t volume) {
	debug(DBG_SND, "Mixer::setChannelVolume(%d, %d)", channel, volume);
	if (_impl) {
		return _impl->setChannelVolume(channel, volume);
	}
}

void Mixer::playMusic(const char *path, uint8_t loop) {
	debug(DBG_SND, "Mixer::playMusic(%s, %d)", path, loop);
	if (_impl) {
		return _impl->playMusic(path, (loop != 0) ? -1 : 0);
	}
}

void Mixer::stopMusic() {
	debug(DBG_SND, "Mixer::stopMusic()");
	if (_impl) {
		return _impl->stopMusic();
	}
}

void Mixer::playAifcMusic(const char *path, uint32_t offset) {
	debug(DBG_SND, "Mixer::playAifcMusic(%s)", path);
	if (!_aifc) {
		_aifc = new AifcPlayer();
	}
	if (_impl) {
		_impl->stopAifcMusic();
		if (_aifc->play(Mixer_impl::kMixFreq, path, offset)) {
			_impl->playAifcMusic(_aifc);
		}
	}
}

void Mixer::stopAifcMusic() {
	debug(DBG_SND, "Mixer::stopAifcMusic()");
	if (_impl && _aifc) {
		_aifc->stop();
		_impl->stopAifcMusic();
	}
}

void Mixer::playSfxMusic(int num) {
	debug(DBG_SND, "Mixer::playSfxMusic(%d)", num);
	if (_impl && _sfx) {
		return _impl->playSfxMusic(_sfx);
	}
}

void Mixer::stopSfxMusic() {
	debug(DBG_SND, "Mixer::stopSfxMusic()");
	if (_impl && _sfx) {
		return _impl->stopSfxMusic();
	}
}

void Mixer::stopAll() {
	debug(DBG_SND, "Mixer::stopAll()");
	if (_impl) {
		return _impl->stopAll(_aifc, _sfx);
	}
}

void Mixer::preloadSoundAiff(uint8_t num, const uint8_t *data) {
	debug(DBG_SND, "Mixer::preloadSoundAiff(num:%d, data:%p)", num, data);
	if (_impl) {
		return _impl->preloadSoundAiff(num, data);
	}
}

void Mixer::playSoundAiff(uint8_t channel, uint8_t num, uint8_t volume) {
	debug(DBG_SND, "Mixer::playSoundAiff()");
	if (_impl) {
		return _impl->playSoundAiff(channel, num, volume);
	}
}
