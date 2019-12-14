
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <sys/param.h>
#include <sys/stat.h>
#include "bitmap.h"

static uint8_t palette[16 * 3] = {
	0x22,0x22,0x33,0x33,0x33,0x55,0x33,0x44,0x77,0x55,0xaa,0xff,0x77,0x66,0x66,0xdd,0xbb,0xaa,0x99,0x77,0x77,0xdd,0x99,0x88,
	0x88,0x33,0x00,0x99,0xcc,0xff,0x44,0x44,0x55,0x44,0x55,0x77,0x55,0x66,0x99,0x55,0x77,0xdd,0x00,0x99,0xff,0xbb,0x55,0x00
};

enum {
	MAX_FILESIZE = 0x10000,
	MAX_SHAPES = 4096,
	MAX_VERTICES = 2048,
	SCREEN_W = 320,
	SCREEN_H = 200,
	SCALE = 64,
	HD_SCALE = 4, // hd.mat files are 1280x800
};

static uint8_t _buffer[MAX_FILESIZE];

static struct {
	uint32_t offset;
	char name[7];
} _shapes[MAX_SHAPES];

static uint8_t _screen[SCREEN_W * SCREEN_H];

static struct {
	int x;
	int y;
} _vertices[MAX_VERTICES];

static uint16_t readWord(const uint8_t *p) {
	return (p[0] << 8) | p[1];
}

static void readShapeNames(const uint8_t *buf, uint32_t size) {
	int count = 0;
	uint32_t offset = 0;
	while (offset < size) {
		const uint16_t addr = readWord(buf + offset); offset += 2;
		assert(count < MAX_SHAPES);
		_shapes[count].offset = addr << 1;
		memcpy(_shapes[count].name, buf + offset, 6); offset += 6;
		++count;
	}
}

static uint32_t calcStep(int p1_x, int p1_y, int p2_x, int p2_y, uint16_t *dy) {
	*dy = p2_y - p1_y;
	uint16_t delta = (*dy == 0) ? 1 : *dy;
	return ((p2_x - p1_x) << 16) / delta;
}

static void fillPolygon(const uint8_t *data, int color, int zoom, int x, int y) {

	int w = data[0] * zoom / SCALE;
	int h = data[1] * zoom / SCALE;
	data += 2;

	int x1 = x - w / 2;
	int x2 = x + w / 2;
	int y1 = y - h / 2;
	int y2 = y + h / 2;
	
	if (x1 >= SCREEN_W || x2 < 0 || y1 >= SCREEN_H || y2 < 0)
		return;

	int count = *data++;
	assert((count & 1) == 0);
	assert(count <= MAX_VERTICES);
	for (int i = 0; i < count; ++i) {
		_vertices[i].x = x1 + data[0] * zoom / SCALE;
		_vertices[i].y = y1 + data[1] * zoom / SCALE;
		data += 2;
	}

	if (color >= 16) {
		assert(color == 0x10 || color == 0x11);
		color = 15;
	}

	int i = 0;
	int j = count - 1;

	x2 = _vertices[i].x;
	x1 = _vertices[j].x;
	int scanline = (_vertices[i].y < _vertices[j].y) ? _vertices[i].y : _vertices[j].y;

	++i;
	--j;

	uint32_t cpt1 = x1 << 16;
	uint32_t cpt2 = x2 << 16;

	while (1) {
		count -= 2;
		if (count == 0) {
			return;
		}
		uint16_t h;
		uint32_t step1 = calcStep(_vertices[j + 1].x, _vertices[j + 1].y, _vertices[j].x, _vertices[j].y, &h);
		uint32_t step2 = calcStep(_vertices[i - 1].x, _vertices[i - 1].y, _vertices[i].x, _vertices[i].y, &h);
		
		++i;
		--j;

		cpt1 = (cpt1 & 0xFFFF0000) | 0x7FFF;
		cpt2 = (cpt2 & 0xFFFF0000) | 0x8000;

		if (h == 0) {
			cpt1 += step1;
			cpt2 += step2;
		} else {
			while (h--) {
				if (scanline >= 0) {
					x1 = (int16_t)(cpt1 >> 16);
					x2 = (int16_t)(cpt2 >> 16);
					if (x1 > x2) {
						const int tmp = x1;
						x1 = x2;
						x2 = tmp;
					}
					if (x1 < 0) {
						x1 = 0;
					}
					if (x2 > SCREEN_W - 1) {
						x2 = SCREEN_W - 1;
					}
					const int len = x2 - x1 + 1;
					if (len > 0 && x1 >= 0) {
						memset(_screen + scanline * SCREEN_W + x1, color, len);
					}
				}
				cpt1 += step1;
				cpt2 += step2;
				++scanline;
				if (scanline >= SCREEN_H) return;
			}
		}
	}
}

static void dumpShape(const uint8_t *data, const char *name, int color, int zoom, int x, int y);

static void dumpShapeParts(const uint8_t *data, int zoom, int x, int y) {
	int x0 = x - data[0] * zoom / SCALE;
	int y0 = y - data[1] * zoom / SCALE;
	data += 2;
	int count = *data++;
	for (int i = 0; i <= count; ++i) {
		uint16_t offset = readWord(data); data += 2;
		int x1 = x0 + data[0] * zoom / SCALE;
		int y1 = y0 + data[1] * zoom / SCALE;
		data += 2;
		int color = 0xFF;
		if (offset & 0x8000) {
			color = data[0] & 0x7F;
			data += 2;
		}
		offset <<= 1;
		dumpShape(_buffer + offset, 0, color, zoom, x1, y1);
	}
}

static void dumpShape(const uint8_t *data, const char *name, int color, int zoom, int x, int y) {
	const int code = *data++;
	if (code >= 0xC0) {
		if (color & 0x80) {
			color = code & 0x3F;
		}
		fillPolygon(data, color, zoom, x, y);
	} else {
		assert((code & 0x3F) == 2);
		dumpShapeParts(data, zoom, x, y);
	}
}

static int readFile(const char *path) {
	int read, size = 0;
	FILE *fp;

	fp = fopen(path, "rb");
	if (fp) {
		fseek(fp, 0, SEEK_END);
		size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		assert(size <= MAX_FILESIZE);
		read = fread(_buffer, 1, size, fp);
		if (read != size) {
			fprintf(stderr, "Failed to read %d bytes (%d)\n", size, read);
		}
		fclose(fp);
	}
	return size;
}

int main(int argc, char *argv[]) {
	int scale = SCALE;
	if (argc == 3) {
		if (strcmp(argv[1], "-hd") == 0) {
			scale = SCALE / HD_SCALE;
			++argv;
			--argc;
		}
	}
	if (argc == 2) {
		char path[MAXPATHLEN];
		struct stat st;
		if (stat(argv[1], &st) == 0 && S_ISREG(st.st_mode)) {
			strcpy(path, argv[1]);
			int count = 0;
			char *ext = strrchr(path, '.');
			if (ext) {
				strcpy(ext, ".nom");
				const int size = readFile(path);
				if (size != 0) {
					assert((size & 7) == 0);
					count = size / 8;
					readShapeNames(_buffer, size);
				}
			}
			const int size = readFile(argv[1]);
			if (size != 0) {
				for (int i = 0; i < count; ++i) {
					fprintf(stdout, "Dumping shape '%s'\n", _shapes[i].name);
					memset(_screen, 0, sizeof(_screen));
					dumpShape(_buffer + _shapes[i].offset, _shapes[i].name, 0xFF, scale, SCREEN_W / 2, SCREEN_H / 2);
					char filename[64];
					snprintf(filename, sizeof(filename), "%s.bmp", _shapes[i].name);
					WriteFile_BMP_PAL(filename, SCREEN_W, SCREEN_H, SCREEN_W, _screen, palette);
				}
			}
		}
	}
	return 0;
}
