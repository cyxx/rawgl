
/*
 * Another World soundfx player
 * Copyright (C) 2004-2005 Gregory Montoir (cyx@users.sourceforge.net)
 */

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

#define PAULA_FREQ 3579545 // NTSC
//#define PAULA_FREQ 3546897 // PAL

static const int kHz = 22050; // mixer sampling rate

static const char *kSndFilePath = "DATA/data_%02x_0"; // sample
static const char *kSfxFilePath = "DATA/data_%02x_1"; // module

static uint16_t READ_BE_UINT16(const uint8_t *p) {
	return p[0] * 256 + p[1];
}

struct Buf {
	uint8_t *buf;
	uint32_t size;
	Buf()
		: buf(0), size(0) {
	}
};

static Buf readFile(const char *filename) {
	Buf b;
	FILE *fp = fopen(filename, "rb");
	if (fp) {
		fseek(fp, 0, SEEK_END);
		b.size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		b.buf = (uint8_t *)malloc(b.size);
		if (fread(b.buf, 1, b.size, fp) != b.size) {
			fprintf(stderr, "readFile(%s) read error\n", filename);
		}
		fclose(fp);
	} else {
		fprintf(stderr, "Failed to open '%s'\n", filename);
	}
	return b;
}

struct InstrumentSample {
	const uint8_t *data;
	int len;
	int loopPos;
	int loopLen;
};

static const uint16_t _freqTable[] = {
	0x0CFF, 0x0DC3, 0x0E91, 0x0F6F, 0x1056, 0x114E, 0x1259, 0x136C, 
	0x149F,	0x15D9, 0x1726, 0x1888, 0x19FD, 0x1B86, 0x1D21, 0x1EDE, 
	0x20AB, 0x229C,	0x24B3, 0x26D7, 0x293F, 0x2BB2, 0x2E4C, 0x3110, 
	0x33FB, 0x370D, 0x3A43,	0x3DDF, 0x4157, 0x4538, 0x4998, 0x4DAE, 
	0x5240, 0x5764, 0x5C9A, 0x61C8,	0x6793, 0x6E19, 0x7485, 0x7BBD
};

static const int kFracBits = 16;

struct Player {
	enum {
		NUM_CHANNELS = 4,
		NUM_INSTRUMENTS = 15
	};

	struct SfxInstrument {
		uint16_t resId;
		uint16_t volume;
		Buf buf;
	};
	struct SfxModule {
		Buf buf;
		SfxInstrument samples[NUM_INSTRUMENTS];
		uint8_t curOrder;
		uint8_t numOrder;
		uint8_t orderTable[0x80];
	};
	struct Pattern {
		uint16_t note_1;
		uint16_t note_2;
		uint16_t sample_start;
		uint8_t *sample_buffer;
		uint16_t period_value;
		uint16_t loopPos;
		uint8_t *loopData;
		uint16_t loopLen;
		uint16_t period_arpeggio; // unused by Another World tracks
		uint16_t sample_volume;
	};
	struct Channel {
		InstrumentSample sample;
		uint32_t pos;
		uint64_t inc;
		int volume;
	};

	uint16_t _delay;
	uint16_t _curPos;
	uint8_t *_bufData;
	SfxModule _sfxMod;
	Pattern _patterns[4];
	Channel _channels[NUM_CHANNELS];
	bool _playingMusic;
	
	void loadSfxModule(int num);
	void loadInstruments(const uint8_t *p);
	void stop();
	void start();
	void handleEvents();
	void handlePattern(uint8_t channel, uint8_t *&data, Pattern *pat);

	int _rate;
	int _samplesLeft;

	void Mix_playChannel(int channel, InstrumentSample *, int freq, int volume);
	void Mix_stopChannel(int channel);
	void Mix_setChannelVolume(int channel, int volume);
	void Mix_stopAll();
	void Mix_doMixChannel(int8_t *buf, int channel);
	void Mix_doMix(int8_t *samples, int count);
};

void Player::loadSfxModule(int num) {
	char path[MAXPATHLEN];
	snprintf(path, sizeof(path), kSfxFilePath, num);
	_sfxMod.buf = readFile(path);
	if (_sfxMod.buf.buf) {
		uint8_t *p = _sfxMod.buf.buf;
		_sfxMod.curOrder = 0;
		_sfxMod.numOrder = READ_BE_UINT16(p + 0x3E);
		fprintf(stdout, "curOrder = 0x%X numOrder = 0x%X\n", _sfxMod.curOrder, _sfxMod.numOrder);
		for (int i = 0; i < 0x80; ++i) {
			_sfxMod.orderTable[i] = *(p + 0x40 + i);
		}
		_delay = READ_BE_UINT16(p);
//		_delay = 15700;
		_delay = _delay * 60 * 1000 / (PAULA_FREQ * 2);
		_bufData = p + 0xC0;
		fprintf(stdout, "eventDelay = %d\n", _delay);
		loadInstruments(p + 2);
		stop();
		start();
	}
}

void Player::loadInstruments(const uint8_t *p) {
	memset(_sfxMod.samples, 0, sizeof(_sfxMod.samples));
	for (int i = 0; i < 0xF; ++i) {
		SfxInstrument *ins = &_sfxMod.samples[i];
		ins->resId = READ_BE_UINT16(p); p += 2;
		if (ins->resId != 0) {
			ins->volume = READ_BE_UINT16(p);
			char path[MAXPATHLEN];
			snprintf(path, sizeof(path), kSndFilePath, ins->resId);
			ins->buf = readFile(path);
			if (ins->buf.buf) {
				memset(ins->buf.buf + 8, 0, 4);
				fprintf(stdout, "Loaded instrument '%s' n=%d volume=%d\n", path, i, ins->volume);
			} else {
				fprintf(stderr, "Error loading instrument '%s'\n", path);
			}
		}
		p += 2; // volume
	}
}

void Player::stop() {
}

void Player::start() {
	_curPos = 0;
	_playingMusic = true;
	memset(&_channels, 0, sizeof(_channels));
	_rate = kHz;
	_samplesLeft = 0;
}

void Player::handleEvents() {
	uint8_t order = _sfxMod.orderTable[_sfxMod.curOrder];
	uint8_t *patternData = _bufData + _curPos + order * 1024;
	memset(_patterns, 0, sizeof(_patterns));
	for (uint8_t ch = 0; ch < 4; ++ch) {
		handlePattern(ch, patternData, &_patterns[ch]);
	}
	_curPos += 4 * 4;
	fprintf(stdout, "order = 0x%X curPos = 0x%X\n", order, _curPos);
	if (_curPos >= 1024) {
		_curPos = 0;
		order = _sfxMod.curOrder + 1;
		if (order == _sfxMod.numOrder) {
			Mix_stopAll();
			order = 0;
			_playingMusic = false;
		}
		_sfxMod.curOrder = order;
	}
}

void Player::handlePattern(uint8_t channel, uint8_t *&data, Pattern *pat) {
	pat->note_1 = READ_BE_UINT16(data + 0);
	pat->note_2 = READ_BE_UINT16(data + 2);
	data += 4;
	if (pat->note_1 != 0xFFFD) {
		uint16_t sample = (pat->note_2 & 0xF000) >> 12;
		if (sample != 0) {
			uint8_t *ptr = _sfxMod.samples[sample - 1].buf.buf;
			fprintf(stdout, "Preparing sample %d ptr = %p\n", sample, ptr);
			if (ptr != 0) {
				pat->sample_volume = _sfxMod.samples[sample - 1].volume;
				pat->sample_start = 8;
				pat->sample_buffer = ptr;
				pat->period_value = READ_BE_UINT16(ptr) * 2;
				uint16_t loopLen = READ_BE_UINT16(ptr + 2) * 2;
				if (loopLen != 0) {
					pat->loopPos = pat->period_value;
					pat->loopData = ptr;
					pat->loopLen = loopLen;
				} else {
					pat->loopPos = 0;
					pat->loopData = 0;
					pat->loopLen = 0;
				}
				int16_t m = pat->sample_volume;
				uint8_t effect = pat->note_2 & 0x0F00;
				fprintf(stdout, "effect = %d\n", effect);
				if (effect == 5) { // volume up
					uint8_t volume = (pat->note_2 & 0xFF);
					m += volume;
					if (m > 0x3F) {
						m = 0x3F;
					}
				} else if (effect == 6) { // volume down
					uint8_t volume = (pat->note_2 & 0xFF);
					m -= volume;
					if (m < 0) {
						m = 0;
					}	
				} else if (effect != 0) {
					fprintf(stderr, "Unhandled effect %d\n", effect);
				}
				Mix_setChannelVolume(channel, m);
				pat->sample_volume = m;
			}
		}
	}
	if (pat->note_1 == 0xFFFD) { // 'PIC'
		pat->note_2 = 0;
	} else if (pat->note_1 != 0) {
		pat->period_arpeggio = pat->note_1;
		if (pat->period_arpeggio == 0xFFFE) {
			Mix_stopChannel(channel);
		} else if (pat->sample_buffer != 0) {
			InstrumentSample sample;
			memset(&sample, 0, sizeof(sample));
			sample.data = pat->sample_buffer + pat->sample_start;
			sample.len = pat->period_value;
			sample.loopPos = pat->loopPos;
			sample.loopLen = pat->loopLen;
			assert(pat->note_1 < 0x1000);
			uint16_t freq = PAULA_FREQ / pat->note_1;
			fprintf(stdout, "Adding sample indFreq = %d freq = %d dataPtr = %p\n", pat->note_1, freq, sample.data);
			Mix_playChannel(channel, &sample, freq, pat->sample_volume);
		}
	}
}

void Player::Mix_playChannel(int channel, InstrumentSample *sample, int freq, int volume) {
	_channels[channel].sample = *sample;
	_channels[channel].pos = 0;
	_channels[channel].inc = (freq << kFracBits) / _rate;
	_channels[channel].volume = volume;
}

void Player::Mix_stopChannel(int channel) {
	memset(&_channels[channel], 0, sizeof(Channel));
}

void Player::Mix_setChannelVolume(int channel, int volume) {
	_channels[channel].volume = volume;
}

void Player::Mix_stopAll() {
	memset(_channels, 0, sizeof(_channels));
}

void Player::Mix_doMixChannel(int8_t *buf, int channel) {
	InstrumentSample *sample = &_channels[channel].sample;
	if (sample->data == 0) {
		return;
	}
	const int pos = _channels[channel].pos >> kFracBits;
	_channels[channel].pos += _channels[channel].inc;
	if (sample->loopLen != 0) {
		if (pos == sample->loopPos + sample->len - 1) {
			_channels[channel].pos = sample->loopPos << kFracBits;
		}
	} else {
		if (pos == sample->len - 1) {
			sample->data = 0;
			return;
		}
	}
	int s = (int8_t)sample->data[pos];
	s = *buf + s * _channels[channel].volume / 64;
	if (s < -128) {
		s = -128;
	} else if (s > 127) {
		s = 127;
	}
	*buf = (int8_t)s;
}

void Player::Mix_doMix(int8_t *buf, int len) {
	memset(buf, 0, 2 * len);
	if (_delay != 0) {
		const int samplesPerTick = _rate / (1000 / _delay);
		while (len != 0) {
			if (_samplesLeft == 0) {
				handleEvents();
				_samplesLeft = samplesPerTick;
			}
			int count = _samplesLeft;
			if (count > len) {
				count = len;
			}
			_samplesLeft -= count;
			len -= count;
			for (int i = 0; i < count; ++i) {
				Mix_doMixChannel(buf, 0);
				Mix_doMixChannel(buf, 3);
				++buf;
				Mix_doMixChannel(buf, 1);
				Mix_doMixChannel(buf, 2);
				++buf;
			}
		}
	}
}

static void nr_stereo(const int8_t *in, int len, int8_t *out) {
	static int prevL = 0;
	static int prevR = 0;
	for (int i = 0; i < len; ++i) {
		const int sL = *in++ >> 1;
		*out++ = sL + prevL;
		prevL = sL;
		const int sR = *in++ >> 1;
		*out++ = sR + prevR;
		prevR = sR;
        }
}

static void playSoundfx(int num) {
	Player p;
	p.loadSfxModule(num);
	p.start();
	FILE *fp = fopen("out.raw", "wb");
	if (fp) {
		while (p._playingMusic) {
			static const int kBufSize = 2048;
			int8_t inbuf[kBufSize * 2];
			p.Mix_doMix(inbuf, kBufSize);
			int8_t nrbuf[kBufSize * 2];
			nr_stereo(inbuf, kBufSize, nrbuf);
			fwrite(nrbuf, 1, sizeof(nrbuf), fp);
		}
		fclose(fp);
	}
	p.stop();
}

int main(int argc, char *argv[]) {
	if (argc == 2) {
		const int num = atoi(argv[1]);
		playSoundfx(num);
	}
	return 0;
}
