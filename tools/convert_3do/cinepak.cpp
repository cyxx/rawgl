
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "cinepak.h"

static void copyV4(uint8_t *dst, uint8_t y1, uint8_t y2, uint8_t u, uint8_t v) {
	dst[0] = u;
	dst[1] = y1;
	dst[2] = v;
	dst[3] = y2;
}

void CinepakDecoder::decodeFrameV4(Cinepak_YUV_Vector *v0, Cinepak_YUV_Vector *v1, Cinepak_YUV_Vector *v2, Cinepak_YUV_Vector *v3) {
	uint8_t *p = _yuvFrame + _yPos * _yuvPitch + _xPos * 2;

	copyV4(&p[0], v0->y[0], v0->y[1], v0->u, v0->v);
	copyV4(&p[4], v1->y[0], v1->y[1], v1->u, v1->v);
	p += _yuvPitch;
	copyV4(&p[0], v0->y[2], v0->y[3], v0->u, v0->v);
	copyV4(&p[4], v1->y[2], v1->y[3], v1->u, v1->v);
	p += _yuvPitch;
	copyV4(&p[0], v2->y[0], v2->y[1], v2->u, v2->v);
	copyV4(&p[4], v3->y[0], v3->y[1], v3->u, v3->v);
	p += _yuvPitch;
	copyV4(&p[0], v2->y[2], v2->y[3], v2->u, v2->v);
	copyV4(&p[4], v3->y[2], v3->y[3], v3->u, v3->v);
}

static void copyV1(uint8_t *dst, uint8_t y, uint8_t u, uint8_t v) {
	dst[0] = u;
	dst[1] = y;
	dst[2] = v;
	dst[3] = y;
}

void CinepakDecoder::decodeFrameV1(Cinepak_YUV_Vector *v) {
	uint8_t *p = _yuvFrame + _yPos * _yuvPitch + _xPos * 2;

	copyV1(&p[0], v->y[0], v->u, v->v);
	copyV1(&p[4], v->y[1], v->u, v->v);
	p += _yuvPitch;
	copyV1(&p[0], v->y[0], v->u, v->v);
	copyV1(&p[4], v->y[1], v->u, v->v);
	p += _yuvPitch;
	copyV1(&p[0], v->y[2], v->u, v->v);
	copyV1(&p[4], v->y[3], v->u, v->v);
	p += _yuvPitch;
	copyV1(&p[0], v->y[2], v->u, v->v);
	copyV1(&p[4], v->y[3], v->u, v->v);
}

void CinepakDecoder::decodeVector(Cinepak_YUV_Vector *v) {
	for (int i = 0; i < 4; ++i) {
		v->y[i] = readByte();
	}
	v->u = 128 + readByte();
	v->v = 128 + readByte();
}

void CinepakDecoder::decode(const uint8_t *data, int dataSize) {
	_data = data;

	const uint8_t flags = readByte();
	_data += 3;
	_w = readWord();
	_h = readWord();
	assert(_w == _yuvW && _h == _yuvH);
	const int strips = readWord();
	assert(strips <= kMaxStrips);

	if (memcmp(_data, "\xFE\x00\x00\x06\x00\x00", 6) == 0) {
		_data += 6;
	}
	
	_xPos = _yPos = 0;
	int yMax = 0;
	for (int strip = 0; strip < strips; ++strip) {
		if (strip != 0 && (flags & 1) == 0) {
			memcpy(&_vectors[kCinepakV1][strip][0], &_vectors[kCinepakV1][strip - 1][0], sizeof(Cinepak_YUV_Vector) * kMaxVectors);
			memcpy(&_vectors[kCinepakV4][strip][0], &_vectors[kCinepakV4][strip - 1][0], sizeof(Cinepak_YUV_Vector) * kMaxVectors);
		}

		readWord();
		int size = readWord();
		const int stripTopY = readWord();
		const int stripTopX = readWord();
		const int stripBottomY = readWord();
		const int stripBottomX = readWord();

		size -= 12;
		_xPos = 0;
		yMax += stripBottomY;
		int v, i;
		while (size > 0) {
			int chunkType = readWord();
			int chunkSize = readWord();
			if (chunkSize > size) {
				chunkSize = size;
			}
			size -= chunkSize;
			chunkSize -= 4;
			switch (chunkType) {
			case 0x2000:
			case 0x2200:
				v = (chunkType == 0x2200) ? kCinepakV1 : kCinepakV4;
				for (i = 0; i < chunkSize / 6; ++i) {
					decodeVector(&_vectors[v][strip][i]);
				}
				chunkSize = 0;
				break;
			case 0x2100:
			case 0x2300:
				v = (chunkType == 0x2300) ? kCinepakV1 : kCinepakV4;
				i = 0;
				while (chunkSize > 0) {
					const uint32_t mask = readLong();
					chunkSize -= 4;
					for (int bit = 0; bit < 32; ++bit) {
						if (mask & (1 << (31 - bit))) {
							decodeVector(&_vectors[v][strip][i]);
							chunkSize -= 6;
						}
						++i;
					}
				}
				break;
			case 0x3000:
				while (chunkSize > 0 && _yPos < yMax) {
					uint32_t mask = readLong();
					chunkSize -= 4;
					for (int bit = 0; bit < 32 && _yPos < yMax; ++bit) {
						if (mask & (1 << (31 - bit))) {
							Cinepak_YUV_Vector *v0 = &_vectors[kCinepakV4][strip][readByte()];
							Cinepak_YUV_Vector *v1 = &_vectors[kCinepakV4][strip][readByte()];
							Cinepak_YUV_Vector *v2 = &_vectors[kCinepakV4][strip][readByte()];
							Cinepak_YUV_Vector *v3 = &_vectors[kCinepakV4][strip][readByte()];
							chunkSize -= 4;
							decodeFrameV4(v0, v1, v2, v3);
						} else {
							Cinepak_YUV_Vector *v0 = &_vectors[kCinepakV1][strip][readByte()];
							--chunkSize;
							decodeFrameV1(v0);
						}
						_xPos += 4;
						if (_xPos >= _w) {
							_xPos = 0;
							_yPos += 4;
						}
					}
				}
				break;
			case 0x3100:
				while (chunkSize > 0 && _yPos < yMax) {
					uint32_t mask = readLong();
					chunkSize -= 4;
					for (int bit = 0; bit < 32 && chunkSize >= 0 && _yPos < yMax; ) {
						if (mask & (1 << (31 - bit))) {
							++bit;
							if (bit == 32) {
								assert(chunkSize >= 4);
								mask = readLong();
								chunkSize -= 4;
								bit = 0;
							}
							if (mask & (1 << (31 - bit))) {
								Cinepak_YUV_Vector *v0 = &_vectors[kCinepakV4][strip][readByte()];
								Cinepak_YUV_Vector *v1 = &_vectors[kCinepakV4][strip][readByte()];
								Cinepak_YUV_Vector *v2 = &_vectors[kCinepakV4][strip][readByte()];
								Cinepak_YUV_Vector *v3 = &_vectors[kCinepakV4][strip][readByte()];
								chunkSize -= 4;
								decodeFrameV4(v0, v1, v2, v3);
							} else {
								Cinepak_YUV_Vector *v0 = &_vectors[kCinepakV1][strip][readByte()];
								--chunkSize;
								decodeFrameV1(v0);
							}
						}
						++bit;
						_xPos += 4;
						if (_xPos >= _w) {
							_xPos = 0;
							_yPos += 4;
						}
					}
				}
				break;
			case 0x3200:
				while (chunkSize > 0 && _yPos < yMax) {
					Cinepak_YUV_Vector *v0 = &_vectors[kCinepakV1][strip][readByte()];
					--chunkSize;
					decodeFrameV1(v0);
					_xPos += 4;
					if (_xPos >= _w) {
						_xPos = 0;
						_yPos += 4;
					}
				}
				break;
			}
			_data += chunkSize;
		}
	}
}
