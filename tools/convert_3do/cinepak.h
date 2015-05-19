
#ifndef CINEPAK_H__
#define CINEPAK_H__

#include <stdint.h>

struct Cinepak_YUV_Vector {
	uint8_t y[4];
	uint8_t u, v;
};

enum {
	kCinepakV1 = 0,
	kCinepakV4 = 1
};

struct CinepakDecoder {
	enum {
		kMaxStrips = 2,
		kMaxVectors = 2048
	};

	uint8_t readByte() {
		return *_data++;
	}

	uint16_t readWord() {
		uint16_t value = (_data[0] << 8) | _data[1];
		_data += 2;
		return value;
	}

	uint32_t readLong() {
		uint32_t value = (_data[0] << 24) | (_data[1] << 16) | (_data[2] << 8) | _data[3];
		_data += 4;
		return value;
	}

	void decodeFrameV4(Cinepak_YUV_Vector *v0, Cinepak_YUV_Vector *v1, Cinepak_YUV_Vector *v2, Cinepak_YUV_Vector *v3);
	void decodeFrameV1(Cinepak_YUV_Vector *v);
	void decodeVector(Cinepak_YUV_Vector *v);
	void decode(const uint8_t *data, int dataSize);

	const uint8_t *_data;
	Cinepak_YUV_Vector _vectors[2][kMaxStrips][kMaxVectors];
	int _w, _h;
	int _xPos, _yPos, _yMax;

	uint8_t *_yuvFrame;
	int _yuvPitch;
	int _yuvW, _yuvH;
};

#endif // CINEPAK_H__
