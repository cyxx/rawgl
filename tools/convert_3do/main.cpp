
#include <arpa/inet.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include "cinepak.h"
extern "C" {
#include "tga.h"
}

struct OutputBuffer {
	void setup(int w, int h, CinepakDecoder *decoder) {
		_bufSize = w * h * 2;
		_buf = (uint8_t *)malloc(_bufSize);
		decoder->_yuvFrame = _buf;
		decoder->_yuvW = w;
		decoder->_yuvH = h;
		decoder->_yuvPitch = w * 2;
	}
	void dump(int num) {
		char name[16];
		snprintf(name, sizeof(name), "out/%04d.uyvy", num);
		FILE *fp = fopen(name, "wb");
		if (fp) {
			fwrite(_buf, _bufSize, 1, fp);
			fclose(fp);
			char cmd[256];
			snprintf(cmd, sizeof(cmd), "convert -size 320x100 -depth 8 out/%04d.uyvy out/%04d.png", num, num);
			system(cmd);
		}
	}

	uint8_t *_buf;
	uint32_t _bufSize;
};

static uint32_t readInt(FILE *fp) {
	uint32_t i;
	fread(&i, sizeof(i), 1, fp);
	return htonl(i);
}

static uint32_t readTag(FILE *fp, char *type) {
	fread(type, 4, 1, fp);
	uint32_t size;
	fread(&size, sizeof(size), 1, fp);
	return htonl(size);
}

static void decodeCine(FILE *fp) {
	CinepakDecoder decoder;
	OutputBuffer out;
	int frmeCounter = 0;
	while (1) {
		uint32_t pos = ftell(fp);
		char tag[4];
		uint32_t size = readTag(fp, tag);
		if (feof(fp)) {
			break;
		}
		fprintf(stdout, "Tag '%c%c%c%c' size %d\n", tag[0], tag[1], tag[2], tag[3], size);
		if (memcmp(tag, "FILM", 4) == 0) {
			fseek(fp, 8, SEEK_CUR);
			char type[4];
			fread(type, 4, 1, fp);
			if (memcmp(type, "FHDR", 4) == 0) {
				fseek(fp, 4, SEEK_CUR);
				char compression[4];
				fread(compression, 4, 1, fp);
				uint32_t height = readInt(fp);
				uint32_t width = readInt(fp);
				out.setup(width, height, &decoder);
			} else if (memcmp(type, "FRME", 4) == 0) {
				uint32_t duration = readInt(fp);
				uint32_t frameSize = readInt(fp);
				uint8_t *frameBuf = (uint8_t *)malloc(frameSize);
				if (frameBuf) {
					fread(frameBuf, 1, frameSize, fp);
					fprintf(stdout, "FRME duration %d frame %d size %d\n", duration, frameSize, size);
					decoder.decode(frameBuf, frameSize);
					free(frameBuf);
					out.dump(frmeCounter);
				}
				++frmeCounter;
			}
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

static void writeTga555(const char *path, int w, int h, const uint16_t *data) {
	struct TgaFile *f;

	f = tgaOpen(path, w, h, 16);
	if (f) {
		tgaWritePixelsData(f, data, w * h * 2);
		tgaClose(f);
	}
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
				snprintf(name, sizeof(name), "out/File%03d.tga", num);
				deinterlace555(_bitmap555, 320, 200, _bitmapDei555);
				writeTga555(name, 320, 200, _bitmapDei555);
			} else {
				fprintf(stderr, "Unexpected decoded size %d\n", decodedSize);
			}
		} else {
			fprintf(stderr, "Unexpected header %x%x%x%x\n", data[0], data[1], data[2], data[3]);
		}
		free(data);
	}
}

int main(int argc, char *argv[]) {
	if (argc >= 2) {
		struct stat st;
		if (stat(argv[1], &st) == 0 && S_ISDIR(st.st_mode)) {
			char path[MAXPATHLEN];
			for (int i = 200; i <= 340; ++i) {
				snprintf(path, sizeof(path), "%s/File%3d", argv[1], i);
				FILE *fp = fopen(path, "rb");
				if (fp) {
					decodeBitmap(fp, i);
					fclose(fp);
				} else {
					fprintf(stderr, "Failed to open '%s'\n", path);
				}
			}
			if (0) {
				static const char *cines[] = { "Logo.Cine", "ootw2.cine", "Spintitle.Cine", 0 };
				for (int i = 0; cines[i]; ++i) {
					snprintf(path, sizeof(path), "%s/%s", argv[1], cines[i]);
					FILE *fp = fopen(path, "rb");
					if (fp) {
						decodeCine(fp);
						fclose(fp);
					} else {
						fprintf(stderr, "Failed to open '%s'\n", path);
					}
				}
			}
		}
	}
	return 0;
}
