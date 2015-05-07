
#include "file.h"
#include "resource_3do.h"

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

Resource3do::Resource3do(const char *dataPath)
	: _dataPath(dataPath) {
}

Resource3do::~Resource3do() {
}

uint8_t *Resource3do::loadFile(int num, uint8_t *dst, uint32_t *size) {
	char path[512];
	snprintf(path, sizeof(path), "%s/GameData/File%d", _dataPath, num);
	File f;
	if (f.open(path)) {
		uint8_t *in = dst;
		const int sz = f.size();
		if (!dst) {
			dst = (uint8_t *)malloc(sz);
			if (!dst) {
				warning("Unable to allocate %d bytes", sz);
				return 0;
			}
		}
		f.read(dst, sz);
		if (memcmp(dst, "\x00\xf4\x01\x00", 4) == 0) {
			static const int SZ = 64000 * 2;
			uint8_t *tmp = (uint8_t *)calloc(1, SZ);
			if (!tmp) {
				warning("Unable to allocate %d bytes", SZ);
				if (in != dst) free(dst);
				return 0;
			}
			const int decodedSize = decodeLzss(dst + 4, sz - 4, tmp);
			if (in != dst) free(dst);
			if (decodedSize != SZ) {
				warning("Unexpected LZSS decoded size %d", decodedSize);
				return 0;
			}
			*size = decodedSize;
			return tmp;
		}
		*size = sz;
		return dst;
	}
	warning("Failed to load '%s'", path);
	return 0;
}

