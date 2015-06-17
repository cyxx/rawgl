
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include "opera.h"

static const char *GAMEDATA_DIRECTORY = "GameData";

static void dumpGameData(const char *name, FILE *in, uint32_t offset, uint32_t size) {
	fprintf(stdout, "%s at 0x%x size %d bytes\n", name, offset, size);
	mkdir(GAMEDATA_DIRECTORY, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
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

static uint32_t READ_BE_UINT32(const uint8_t *p) {
	return htobe32(*(const uint32_t *)p);
}

static const int BLOCK_SIZE = 2048;

static void readIso(FILE *fp, int block, int flags) {
	if (block < 0) {
		uint8_t buf[128];
		fread(buf, sizeof(buf), 1, fp);
		if (buf[0] == 1 && memcmp(buf + 40, "CD-ROM", 6) == 0) {
			const int offset = READ_BE_UINT32(buf + 100);
			readIso(fp, offset, 0);
		}
	}  else {
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
			} while (attr < 256);
			++block;
		} while ((attr >> 24) == 0x40);
	}
}

int extractOperaIso(FILE *fp) {
	readIso(fp, -1, 0);
	return 0;
}
