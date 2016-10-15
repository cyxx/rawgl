
#include "file.h"
#include "resource_mac.h"
#include "util.h"

const char *ResourceMac::FILE017 = "FILE17.mat";

ResourceMac::ResourceMac(const char *dataPath)
	: _dataPath(dataPath) {
}

ResourceMac::~ResourceMac() {
}

uint8_t *ResourceMac::loadFile(int num, uint8_t *dst, uint32_t *size) {
	char name[16];

	if (num == 17) {
		strcpy(name, FILE017);
	} else {
		snprintf(name, sizeof(name), "FILE%04d", num);
	}
	File f;
	if (f.open(name, _dataPath)) {
		*size = f.size();
		if (!dst) {
			dst = (uint8_t *)malloc(*size);
			if (!dst) {
				warning("Unable to allocate %d bytes", *size);
				return 0;
			}
		}
		f.read(dst, *size);
		return dst;
	}
	warning("Unable to load resource #%d", num);
	return 0;
}
