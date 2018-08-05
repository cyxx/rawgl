
#include <sys/param.h>
#include "opera.h"

static const char *GAMEDATA_DIRECTORY = "GameData";

static void dumpGameData(const char *name, FILE *in, uint32_t offset, uint32_t size) {
	fprintf(stdout, "%s at 0x%x size %d bytes\n", name, offset, size);
	makeDirectory(GAMEDATA_DIRECTORY);
	char path[MAXPATHLEN];
	snprintf(path, sizeof(path), "%s/%s", GAMEDATA_DIRECTORY, name);
	FILE *out = fopen(path, "wb");
	if (out) {
		uint8_t buf[4096];
		int count;
		fseek(in, offset, SEEK_SET);
		while ((count = fread(buf, 1, size < sizeof(buf) ? size : sizeof(buf), in)) > 0) {
			fwrite(buf, 1, count, out);
			size -= count;
		}
		fclose(out);
	}
}

static const int BLOCK_SIZE = 2048;

static void readIso(FILE *fp, int block, int flags) {
	uint32_t attr = 0;
	do {
		fseek(fp, block * BLOCK_SIZE + 20, SEEK_SET);
		do {
			uint8_t buf[72];
			fread(buf, sizeof(buf), 1, fp);
			attr = READ_BE_UINT32(buf);
			const char *name = (const char *)buf + 32;
			const uint32_t count = READ_BE_UINT32(buf + 64);
			const uint32_t offset = READ_BE_UINT32(buf + 68);
			fseek(fp, count * 4, SEEK_CUR);
			switch (attr & 255) {
			case 2:
				if (flags & 1) {
					const int pos = ftell(fp);
					dumpGameData(name, fp, offset * BLOCK_SIZE, READ_BE_UINT32(buf + 16));
					fseek(fp, pos, SEEK_SET);
				}
				break;
			case 7:
				if (strcmp(name, GAMEDATA_DIRECTORY) == 0) {
					readIso(fp, offset, 1);
				}
				break;
			}
		} while (attr != 0 && attr < 256);
		++block;
	} while ((attr >> 24) == 0x40);
}

int extractOperaIso(FILE *fp) {
	uint8_t buf[128];
	const int count = fread(buf, 1, sizeof(buf), fp);
	if (count != sizeof(buf)) {
		fprintf(stderr, "Failed to read %d bytes, ret %d\n", (int)sizeof(buf), count);
		return -1;
	}
	if (buf[0] != 1 || memcmp(buf + 40, "CD-ROM", 6) != 0) {
		fprintf(stderr, "Unexpected Opera ISO signature\n");
		return -1;
	}
	const int offset = READ_BE_UINT32(buf + 100);
	readIso(fp, offset, 0);
	return 0;
}
