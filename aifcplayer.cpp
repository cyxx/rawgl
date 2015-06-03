
#include "aifcplayer.h"
#include "util.h"

static uint32_t READ_IEEE754(const uint8_t *p) {
	const uint32_t m = READ_BE_UINT32(p + 2);
	const int exp = 30 - p[1];
	return (m >> exp);
}

AifcPlayer::AifcPlayer() {
}

bool AifcPlayer::play(int mixRate, const char *path) {
	_ssndSize = 0;
	if (_f.open(path)) {
		uint8_t buf[12];
		_f.read(buf, sizeof(buf));
		if (memcmp(buf, "FORM", 4) == 0 && memcmp(buf + 8, "AIFC", 4) == 0) {
			const uint32_t size = READ_BE_UINT32(buf + 4);
			for (int offset = 12; offset < size; ) {
				_f.seek(offset);
				_f.read(buf, 8);
				const uint32_t sz = READ_BE_UINT32(buf + 4);
				if (memcmp(buf, "COMM", 4) == 0) {
					const int channels = _f.readUint16BE();
					_f.readUint32BE(); // samples per frame
					const int bits = _f.readUint16BE();
					_f.read(buf, 10);
					const int rate = READ_IEEE754(buf);
					if (channels != 2 || rate != mixRate) {
						warning("Unsupported aifc channels %d rate %d", channels, rate);
						break;
					}
					_f.read(buf, 4);
					if (memcmp(buf, "SDX2", 4) != 0) {
						warning("Unsupported compression");
						break;
					}
					debug(DBG_SND, "aifc channels %d rate %d bits %d", channels, rate, bits);
				} else if (memcmp(buf, "SSND", 4) == 0) {
					const int blockOffset = _f.readUint32BE(); // offset
					const int blockSize = _f.readUint32BE(); // block size
					_ssndOffset = offset + 8 + 8;
					_ssndSize = sz;
					debug(DBG_SND, "aifc ssnd size %d", _ssndSize);
					break;
				} else {
//					warning("Ignoring aifc tag '%c%c%c%c' size %d", buf[0], buf[1], buf[2], buf[3], sz);
				}
				offset += sz + 8;
			}
		}
	}
	_pos = 0;
	memset(_samples, 0, sizeof(_samples));
	return _ssndSize != 0;
}

void AifcPlayer::stop() {
	_f.close();
}

static int16_t decodeSDX2(int16_t prev, int8_t data) {
	const int sqr = (data * ((data < 0) ? -data : data)) * 2;
	return (data & 1) != 0 ? prev + sqr : sqr;
}

int8_t AifcPlayer::readData() {
	if (_pos >= _ssndSize) {
		_pos = 0;
		_f.seek(_ssndOffset);
	}
	const int8_t data = _f.readByte();
	++_pos;
	return data;
}

void AifcPlayer::readSamples(int16_t *buf, int len) {
	for (int i = 0; i < len; i += 2) {
		buf[i]     = _samples[0] = decodeSDX2(_samples[0], readData());
		buf[i + 1] = _samples[1] = decodeSDX2(_samples[1], readData());
	}
}
