
#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include "bitmap.h"
#include "cinepak.h"
#include "opera.h"
#include "util.h"
#include "wave.h"

static const char *OUT = "out";

static uint8_t clipU8(int a) {
	if (a < 0) {
		return 0;
	} else if (a > 255) {
		return 255;
	} else {
		return a;
	}
}

static uint16_t yuv_to_rgb555(int y, int u, int v) {
	int r = int(y + (1.370705 * (v - 128)));
	r = clipU8(r) >> 3;
	int g = int(y - (0.698001 * (v - 128)) - (0.337633 * (u - 128)));
	g = clipU8(g) >> 3;
	int b = int(y + (1.732446 * (u - 128)));
	b = clipU8(b) >> 3;
	return (r << 10) | (g << 5) | b;
}

static void uyvy_to_rgb555(const uint8_t *in, int len, uint16_t *out) {
	assert((len & 3) == 0);
	for (int i = 0; i < len; i += 4, in += 4) {
		const int u  = in[0];
		const int y0 = in[1];
		const int v  = in[2];
		const int y1 = in[3];
		*out++ = yuv_to_rgb555(y0, u, v);
		*out++ = yuv_to_rgb555(y1, u, v);
	}
}

static uint16_t _cineBitmap555[320 * 200];

struct OutputBuffer {
	void setup(int w, int h, CinepakDecoder *decoder) {
		_bufSize = w * h * sizeof(uint16_t);
		_buf = (uint8_t *)malloc(_bufSize);
		decoder->_yuvFrame = _buf;
		decoder->_yuvW = w;
		decoder->_yuvH = h;
		decoder->_yuvPitch = w * sizeof(uint16_t);
	}
	void dump(const char *path, int num) {
		static const int W = 320;
		static const int H = 200;
		char name[MAXPATHLEN];
		snprintf(name, sizeof(name), "%s/%04d.bmp", path, num);
		FILE *fp = fopen(name, "wb");
		if (fp) {
			const int size = W * H * sizeof(uint16_t);
			uyvy_to_rgb555(_buf, size, _cineBitmap555);
			writeBitmap555(fp, _cineBitmap555, W, H);
			fclose(fp);
		}
	}

	uint8_t *_buf;
	uint32_t _bufSize;
};

static uint32_t freadUint32BE(FILE *fp) {
	uint8_t buf[4];
	fread(buf, sizeof(buf), 1, fp);
	return READ_BE_UINT32(buf);
}

static uint32_t freadTag(FILE *fp, char *type) {
	fread(type, 4, 1, fp);
	uint8_t buf[4];
	fread(buf, sizeof(buf), 1, fp);
	return READ_BE_UINT32(buf);
}

static void decodeCine(FILE *fp, const char *name) {
	char path[MAXPATHLEN];
	snprintf(path, sizeof(path), "%s/%s", OUT, name);
	stringLower(path + strlen(OUT) + 1);
	makeDirectory(path);
	CinepakDecoder decoder;
	OutputBuffer out;
	int frmeCounter = 0;
	while (1) {
		const uint32_t pos = ftell(fp);
		char tag[4];
		const uint32_t size = freadTag(fp, tag);
		if (feof(fp)) {
			break;
		}
		if (memcmp(tag, "FILM", 4) == 0) {
			fseek(fp, 8, SEEK_CUR);
			char type[4];
			fread(type, 4, 1, fp);
			if (memcmp(type, "FHDR", 4) == 0) {
				fseek(fp, 4, SEEK_CUR);
				char compression[4];
				fread(compression, 4, 1, fp);
				uint32_t height = freadUint32BE(fp);
				uint32_t width = freadUint32BE(fp);
				out.setup(width, height, &decoder);
			} else if (memcmp(type, "FRME", 4) == 0) {
				uint32_t duration = freadUint32BE(fp);
				uint32_t frameSize = freadUint32BE(fp);
				uint8_t *frameBuf = (uint8_t *)malloc(frameSize);
				if (frameBuf) {
					fread(frameBuf, 1, frameSize, fp);
					fprintf(stdout, "FRME duration %d frame %d size %d\n", duration, frameSize, size);
					decoder.decode(frameBuf, frameSize);
					free(frameBuf);
					out.dump(path, frmeCounter);
				}
				++frmeCounter;
			} else {
				fprintf(stdout, "Unhandled FILM chunk '%c%c%c%c'\n", type[0], type[1], type[2], type[3]);
				break;
			}
		} else if (memcmp(tag, "SNDS", 4) == 0) {
			fseek(fp, 8, SEEK_CUR);
			char type[4];
			fread(type, 4, 1, fp);
			if (memcmp(type, "SHDR", 4) == 0) {

			} else if (memcmp(type, "SSMP", 4) == 0) {

			} else {
				fprintf(stdout, "Unhandled SNDS chunk '%c%c%c%c'\n", type[0], type[1], type[2], type[3]);
				break;
			}
		} else if (memcmp(tag, "SHDR", 4) == 0) { // ignore
		} else if (memcmp(tag, "FILL", 4) == 0) { // ignore
		} else if (memcmp(tag, "CTRL", 4) == 0) { // ignore
		} else {
			fprintf(stdout, "Unhandled tag '%c%c%c%c' size %d\n", tag[0], tag[1], tag[2], tag[3], size);
			break;
		}
		fseek(fp, pos + size, SEEK_SET);
	}
}

static int decodeLzss(const uint8_t *src, int len, uint8_t *dst) {
	uint32_t rd = 0, wr = 0;
	int code = 0x100 | src[rd++];
	while (rd < len) {
		const bool bit = (code & 1);
		code >>= 1;
		if (bit) {
			dst[wr++] = src[rd++];
		} else {
			const int code1 = src[rd] | ((src[rd + 1] & 0xF) << 8); // offset
			const int code2 = (src[rd + 1] >> 4) + 3; // len
			rd += 2;
			const uint16_t offset = 0xF000 | code1;
			for (int i = 0; i < code2; ++i) {
				dst[wr] = dst[wr + (int16_t)offset];
				++wr;
			}
		}
		if (code == 1) {
			code = 0x100 | src[rd++];
		}
	}
	return wr;
}

static uint8_t *readFile(FILE *fp, int *size) {
	uint8_t *buf = 0;

	fseek(fp, 0, SEEK_END);
	*size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	buf = (uint8_t *)malloc(*size);
	if (buf) {
		const int count = fread(buf, 1, *size, fp);
		if (count != *size) {
			fprintf(stderr, "Failed to read %d bytes\n", count);
		}
	}
	return buf;
}

static uint8_t _bitmap555[320 * 200 * 2];
static uint16_t _bitmapDei555[320 * 200];

static uint16_t rgb555(const uint8_t *src) {
	const uint16_t color = src[0] * 256 + src[1];
	const int r = (color >> 10) & 31;
	const int g = (color >>  5) & 31;
	const int b = (color >>  0) & 31;
	return (r << 10) | (g << 5) | b;
}

static void deinterlace555(const uint8_t *src, int w, int h, uint16_t *dst) {
	for (int y = 0; y < h / 2; ++y) {
		for (int x = 0; x < w; ++x) {
			dst[x]     = rgb555(&src[0]);
			dst[w + x] = rgb555(&src[2]);
			src += 4;
		}
		dst += w * 2;
	}
}

static void decodeBitmap(FILE *fp, int num) {
	uint8_t *data;
	int dataSize, decodedSize;

	data = readFile(fp, &dataSize);
	if (data) {
		if (memcmp(data, "\x00\xf4\x01\x00", 4) == 0) {
			decodedSize = decodeLzss(data + 4, dataSize - 4, _bitmap555);
			if (decodedSize == sizeof(_bitmap555)) {
				char name[64];
				snprintf(name, sizeof(name), "%s/File%03d.bmp", OUT, num);
				FILE *fp = fopen(name, "wb");
				if (fp) {
					deinterlace555(_bitmap555, 320, 200, _bitmapDei555);
					writeBitmap555(fp, _bitmapDei555, 320, 200);
					fclose(fp);
				}
			} else {
				fprintf(stderr, "Unexpected decoded size %d\n", decodedSize);
			}
		} else if (dataSize == 320 * 200 * sizeof(uint16_t)) {
			char name[64];
			snprintf(name, sizeof(name), "%s/File%03d.bmp", OUT, num);
			FILE *fp = fopen(name, "wb");
			if (fp) {
				deinterlace555(data, 320, 200, _bitmapDei555);
				writeBitmap555(fp, _bitmapDei555, 320, 200);
				fclose(fp);
			}
		} else {
			fprintf(stderr, "Unexpected header %x%x%x%x\n", data[0], data[1], data[2], data[3]);
		}
		free(data);
	}
}

static int16_t decodeSampleSDX(int16_t prev, int8_t data) {
        const int sqr = (data * ((data < 0) ? -data : data)) * 2;
        return (data & 1) != 0 ? prev + sqr : sqr;
}

static const char *kSDX = "SDX22:1 Squareroot-Delta-Exact compression";

static void decodeSong(FILE *fp, int num) {
	uint8_t buf[64];
	int count, offset, formSize, dataSize, rate = 0;

	count = fread(buf, 1, 12, fp);
	if (count == 12 && memcmp(buf, "FORM", 4) == 0 && memcmp(buf + 8, "AIFC", 4) == 0) {
		formSize = READ_BE_UINT32(buf + 4);
		for (offset = 12; offset < formSize; offset += dataSize + 8) {
			fseek(fp, offset, SEEK_SET);

			count = fread(buf, 1, 8, fp);
			dataSize = READ_BE_UINT32(buf + 4);

			if (memcmp(buf, "FVER", 4) == 0) {
				continue;
			}
			if (memcmp(buf, "COMM", 4) == 0) {
				assert(dataSize < (int)sizeof(buf));
				count = fread(buf, 1, dataSize, fp);

				const int channels = READ_BE_UINT16(buf);
				const int samplesPerFrame = READ_BE_UINT32(buf + 2);
				const int bits = READ_BE_UINT16(buf + 6);
				{
					const uint8_t *ieee754 = buf + 8;
					const uint32_t m = READ_BE_UINT32(ieee754 + 2);
					const int e = 30 - ieee754[1];
					rate = (m >> e);
				}
				if (memcmp(buf + 18, kSDX, strlen(kSDX)) == 0) {
					fprintf(stdout, "bits %d samples %d channels %d rate %d\n", bits, samplesPerFrame, channels, rate);
				}
				continue;
			}
			if (memcmp(buf, "INST", 4) == 0) {
				continue;
			}
			if (memcmp(buf, "MARK", 4) == 0) {
				continue;
			}
			if (memcmp(buf, "SSND", 4) == 0) {
				count = fread(buf, 1, 4, fp);
				// block offset
				count = fread(buf, 1, 4, fp);
				// block size

				int16_t *samples = (int16_t *)calloc(dataSize, sizeof(int16_t));
				if (samples) {
					int16_t sampleL = 0;
					int16_t sampleR = 0;
					for (int i = 0; i < dataSize; i += 2) {
						count = fread(buf, 1, 2, fp);

						samples[i]     = sampleL = decodeSampleSDX(sampleL, buf[0]);
						samples[i + 1] = sampleR = decodeSampleSDX(sampleR, buf[1]);
					}
					char path[MAXPATHLEN];
					snprintf(path, sizeof(path), "%s/song%d.wav", OUT, num);
					FILE *fp = fopen(path, "wb");
					if (fp) {
						writeWav_stereoS16(fp, samples, dataSize, rate);
						fclose(fp);
					}
					free(samples);
				}
				continue;
			}


			fprintf(stdout, "form %c%c%c%c size %d\n", buf[0], buf[1], buf[2], buf[3], dataSize);

		}
	}
}

static void decodeCcb16(int frameWidth, int frameHeight, const uint8_t *dataPtr, uint32_t dataSize, uint16_t *dest) {

	const uint8_t *scanlineStart = dataPtr;

	for (; frameHeight > 0; --frameHeight) {

		const int lineDWordSize = READ_BE_UINT16(scanlineStart) + 2;
		const uint8_t *scanlineData = scanlineStart + 2;

		int w = frameWidth;
		while (w > 0) {
			uint8_t code = *scanlineData++;
			const int count = (code & 63) + 1;
			code >>= 6;
			if (code == 0) {
				break;
			}
			switch (code) {
			case 1:
				for (int i = 0; i < count; ++i) {
					*dest++ = READ_BE_UINT16(scanlineData); scanlineData += 2;
				}
				break;
			case 2:
				memset(dest, 0, count * sizeof(uint16_t));
				dest += count;
				break;
			case 3: {
					const uint16_t color = READ_BE_UINT16(scanlineData); scanlineData += 2;
					for (int i = 0; i < count; ++i) {
						*dest++ = color;
					}
				}
				break;
			}
			w -= count;
		}
		assert(w >= 0);
		if (w > 0) {
			dest += w;
		}
		scanlineStart += lineDWordSize * 4;
	}
}

static const uint8_t _ccbBitsPerPixelTable[8] = {
        0, 1, 2, 4, 6, 8, 16, 0
};

static void decodeShapeCcb(FILE *fp, const char *shape) {
	int dataSize;
	uint8_t *data = readFile(fp, &dataSize);
	if (data) {
		int offset = 0;

		uint32_t ccb_Flags = READ_BE_UINT32(data + offset); offset += 4;
		offset += 4; // ccb_NextPtr
		uint32_t ccb_CelData = READ_BE_UINT32(data + offset); offset += 4; // ccb_CelData
		offset += 4; // ccb_PLUTPtr
		offset += 4; // ccb_X
		offset += 4; // ccb_Y
		offset += 4; // ccb_hdx
		offset += 4; // ccb_hdy
		offset += 4; // ccb_vdx
		offset += 4; // ccb_vdy
		offset += 4; // ccb_ddx
		offset += 4; // ccb_ddy
		offset += 4; // ccb_PPMPC;
		const uint32_t ccb_PRE0 = READ_BE_UINT32(data + offset); offset += 4;
		const uint32_t ccb_PRE1 = READ_BE_UINT32(data + offset); offset += 4;
		assert(ccb_CelData == 0x30);
		assert(ccb_Flags & (1 << 9));
		const int bpp = _ccbBitsPerPixelTable[ccb_PRE0 & 7];
		const uint32_t ccbPRE0_height = ((ccb_PRE0 >> 6) & 0x3FF) + 1;
		const uint32_t ccbPRE1_width  = (ccb_PRE1 & 0x3FF) + 1;
		assert(bpp == 16);

		decodeCcb16(ccbPRE1_width, ccbPRE0_height, data + offset, dataSize - offset, _bitmapDei555);

		char name[64];
		snprintf(name, sizeof(name), "%s/%s.bmp", OUT, shape);
		FILE *out = fopen(name, "wb");
		if (out) {
			writeBitmap555(out, _bitmapDei555, ccbPRE1_width, ccbPRE0_height);
			fclose(out);
		}

		free(data);
	}
}

int main(int argc, char *argv[]) {
	bool doDecodeSong = false;
	bool doDecodeBitmap = false;
	bool doDecodeCine = false;
	bool doDecodeShape = false;
	while (1) {
		static struct option options[] = {
			{ "decode_song",   no_argument, 0, 1 },
			{ "decode_bitmap", no_argument, 0, 2 },
			{ "decode_cine",   no_argument, 0, 3 },
			{ "decode_shape",  no_argument, 0, 4 },
			{ 0, 0, 0, 0 }
		};
		int index;
		const int c = getopt_long(argc, argv, "", options, &index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 1:
			doDecodeSong = true;
			break;
		case 2:
			doDecodeBitmap = true;
			break;
		case 3:
			doDecodeCine = true;
			break;
		case 4:
			doDecodeShape = true;
			break;
		}
	}
        if (optind < argc) {
		struct stat st;
		if (stat(argv[optind], &st) != 0) {
			fprintf(stderr, "Failed to stat '%s'\n", argv[optind]);
			return -1;
		}
		if (S_ISREG(st.st_mode)) { // .iso file
			FILE *fp = fopen(argv[optind], "rb");
			if (fp) {
				extractOperaIso(fp);
				fclose(fp);
			} else {
				fprintf(stderr, "Failed to open '%s'\n", argv[optind]);
			}
			return 0;
		}
		if (S_ISDIR(st.st_mode)) { // extracted .iso file
			char path[MAXPATHLEN];
			if (doDecodeSong) {
				for (int i = 1; i <= 30; ++i) {
					snprintf(path, sizeof(path), "%s/song%d", argv[optind], i);
					FILE *fp = fopen(path, "rb");
					if (fp) {
						decodeSong(fp, i);
						fclose(fp);
					} else {
						fprintf(stderr, "Failed to open '%s'\n", path);
					}
				}
			}
			if (doDecodeBitmap) {
				for (int i = 1; i <= 340; ++i) {
					snprintf(path, sizeof(path), "%s/File%d", argv[optind], i);
					FILE *fp = fopen(path, "rb");
					if (fp) {
						decodeBitmap(fp, i);
						fclose(fp);
					} else {
						fprintf(stderr, "Failed to open '%s'\n", path);
					}
				}
			}
			if (doDecodeCine) {
				static const char *cines[] = { "Logo.Cine", "ootw2.cine", "Spintitle.Cine", 0 };
				for (int i = 0; cines[i]; ++i) {
					snprintf(path, sizeof(path), "%s/%s", argv[optind], cines[i]);
					FILE *fp = fopen(path, "rb");
					if (fp) {
						decodeCine(fp, cines[i]);
						fclose(fp);
					} else {
						fprintf(stderr, "Failed to open '%s'\n", path);
					}
				}
			}
			if (doDecodeShape) {
				static const char *names[] = { "EndShape1", "EndShape2", "PauseShape", "Logo3do", 0 };
				for (int i = 0; names[i]; ++i) {
					snprintf(path, sizeof(path), "%s/%s", argv[optind], names[i]);
					FILE *fp = fopen(path, "rb");
					if (fp) {
						decodeShapeCcb(fp, names[i]);
						fclose(fp);
					} else {
						fprintf(stderr, "Failed to open '%s'\n", path);
					}
				}
			}
			return 0;
		}
	}
	return 0;
}
