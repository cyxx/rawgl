
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <sys/param.h>
#include <sys/stat.h>

enum {
	MAX_TASKS = 64,
	MAX_FILESIZE = 0x10000,
	MAX_SHAPENAMES = 0x10000,
	MAX_OPCODES = 31, // 27 for Amiga/DOS, 31 for 3DO
};

static uint8_t _fileBuf[MAX_FILESIZE];
static char _shapeNames[MAX_SHAPENAMES][7]; // shape (polygons) names are 6 characters long
static int _histogramOp[MAX_OPCODES];

static bool _is3DO = false;

static FILE *_out = stdout;

static uint16_t readWord(const uint8_t *p) {
	if (_is3DO) {
		return (p[1] << 8) | p[0]; // LE
	} else {
		return (p[0] << 8) | p[1]; // BE
	}
}

static void readShapeNames(const uint8_t *buf, uint32_t size) {
	uint32_t offset = 0;
	while (offset < size) {
		const uint16_t addr = readWord(buf + offset); offset += 2;
		assert(addr < MAX_SHAPENAMES);
		memcpy(_shapeNames[addr], buf + offset, 6); offset += 6;
		// fprintf(stdout, "Shape '%s' at 0x%x\n", _shapeNames[addr], addr);
	}
}

#if 0
static const char *varName(uint8_t var) {
	static char name[50];
	switch (var) {
	case 0x3C:
		strcpy(name, "VAR_RANDOM_SEED");
		break;
	case 0xDA:
		strcpy(name, "VAR_LAST_KEYCHAR");
		break;
	case 0xE5:
		strcpy(name, "VAR_HERO_POS_UP_DOWN");
		break;
	case 0xF4:
		strcpy(name, "VAR_MUSIC_SYNC");
		break;
	case 0xF9:
		strcpy(name, "VAR_SCROLL_Y");
		break;
	case 0xFA:
		strcpy(name, "VAR_HERO_ACTION");
		break;
	case 0xFB:
		strcpy(name, "VAR_HERO_POS_JUMP_DOWN");
		break;
	case 0xFC:
		strcpy(name, "VAR_HERO_POS_LEFT_RIGHT");
		break;
	case 0xFD:
		strcpy(name, "VAR_HERO_POS_MASK");
		break;
	case 0xFE:
		strcpy(name, "VAR_HERO_ACTION_POS_MASK");
		break;
	case 0xFF:
		strcpy(name, "VAR_PAUSE_SLICES");
		break;
	default:
		sprintf(name, "VAR(0x%02X)", var);
		break;
	}
	return name;
}
#endif

enum {
	op_movConst,
	op_mov,
	op_add,
	op_addConst,
	// 0x04
	op_call,
	op_ret,
	op_yieldTask,
	op_jmp,
	// 0x08
	op_installTask,
	op_jmpIfVar,
	op_condJmp,
	op_setPalette,
	// 0x0C
	op_changeTasksState,
	op_selectPage,
	op_fillPage,
	op_copyPage,
	// 0x10
	op_updateDisplay,
	op_removeTask,
	op_drawString,
	op_sub,
	// 0x14
	op_and,
	op_or,
	op_shl,
	op_shr,
	// 0x18
	op_playSound,
	op_updateResources,
	op_playMusic,
	op_drawString2, // 3DO
	// 0x1C
	op_jmpIfVarZero, // 3DO
	op_jmpIfVarNonZero, // 3DO
	op_printTime, // 3DO
};

#define ADDR_FUNC   (1 << 0)
#define ADDR_LABEL  (1 << 1)
#define ADDR_TASK   (1 << 2)

static uint8_t _addr[MAX_FILESIZE];

static void checkOpcode(uint16_t addr, uint8_t opcode, int args[16]) {
	if (opcode == op_call) {
		const int offset = args[0];
		_addr[offset] |= ADDR_FUNC;
	} else if (opcode == op_jmp) {
		const int offset = args[0];
		_addr[offset] |= ADDR_LABEL;
	} else if (opcode == op_installTask) {
		const int offset = args[1];
		_addr[offset] |= ADDR_TASK;
	} else if (opcode == op_condJmp) {
		const int offset = args[3];
		_addr[offset] |= ADDR_LABEL;
	} else if (opcode == op_jmpIfVar || opcode == op_jmpIfVarZero || opcode == op_jmpIfVarNonZero) {
		const int offset = args[1];
		_addr[offset] |= ADDR_LABEL;
	}
	++_histogramOp[opcode];
}

static void printOpcode(uint16_t addr, uint8_t opcode, int args[16]) {
	if (_addr[addr] & ADDR_FUNC) {
		fprintf(_out, "\n%04X: // func_%04X\n", addr, addr);
	}
	if (_addr[addr] & ADDR_TASK) {
		fprintf(_out, "\n%04X: // START OF SCRIPT TASK\n", addr);
	}
	if (_addr[addr] & ADDR_LABEL) {
		fprintf(_out, "%04X: loc_%04X:\n", addr, addr);
	}
	fprintf(_out, "%04X: (%02X) ", addr, opcode);
	switch (opcode) {
	case op_movConst:
		fprintf(_out, "VAR(0x%02X) = %d", args[0], args[1]);
		break;
	case op_mov:
		fprintf(_out, "VAR(0x%02X) = VAR(0x%02X)", args[0], args[1]);
		break;
	case op_addConst:
		if (args[1] < 0) {
			fprintf(_out, "VAR(0x%02X) -= %d", args[0], -args[1]);
		} else {
			fprintf(_out, "VAR(0x%02X) += %d", args[0], args[1]);
		}
		break;
	case op_add:
		fprintf(_out, "VAR(0x%02X) += VAR(0x%02X)", args[0], args[1]);
		break;
	case op_call:
		fprintf(_out, "call(@%04X)", args[0]);
		break;
	case op_ret:
		fprintf(_out, "ret // RETURN FROM CALL");
		break;
	case op_yieldTask:
		fprintf(_out, "yieldTask // PAUSE SCRIPT TASK");
		break;
	case op_jmp:
		fprintf(_out, "jmp(@%04X)", args[0]);
		break;
	case op_installTask:
		fprintf(_out, "installTask(%d, @%04X)", args[0], args[1]);
		break;
	case op_jmpIfVar:
		fprintf(_out, "jmpIfVar(VAR(0x%02X), @%04X)", args[0], args[1]);
		break;
	case op_condJmp:
		fprintf(_out, "jmpIf(VAR(0x%02X)", args[1]);
		switch (args[0] & 7) {
		case 0:
			fprintf(_out, " == ");
			break;
		case 1:
			fprintf(_out, " != ");
			break;
		case 2:
			fprintf(_out, " > ");
			break;
		case 3:
			fprintf(_out, " >= ");
			break;
		case 4:
			fprintf(_out, " < ");
			break;
		case 5:
			fprintf(_out, " <= ");
			break;
		default:
			fprintf(_out, " ?? ");
			break;
		}			
		if (args[0] & 0x80) {
			fprintf(_out, "VAR(0x%02X),", args[2]);
		} else if (args[0] & 0x40) {
			fprintf(_out, "%d,", (int16_t)args[2]);
		} else {
			fprintf(_out, "%d,", args[2]);
		}
		fprintf(_out, " @%04X)", args[3]);
		break;
	case op_setPalette:
		fprintf(_out, "setPalette(num=%d)", args[0]);
		break;
	case op_changeTasksState:
		assert(args[0] < MAX_TASKS && args[1] < MAX_TASKS);
		assert(args[0] <= args[1]);
		fprintf(_out, "changeTasksState(start=%d,end=%d,state=%d)", args[0], args[1], args[2]);
		break;
	case op_selectPage:
		fprintf(_out, "selectPage(page=%d)", args[0]);
		break;
        case op_fillPage:
		fprintf(_out, "fillPage(page=%d, color=%d)", args[0], args[1]);
		break;
	case op_copyPage:
		fprintf(_out, "copyPage(src=%d, dst=%d)", args[0], args[1]);
		break;
	case op_updateDisplay:
		fprintf(_out, "updateDisplay(page=%d)", args[0]);
		break;
	case op_removeTask:
		fprintf(_out, "removeTask // STOP SCRIPT TASK");
		break;
	case op_drawString:
		fprintf(_out, "drawString(str=0x%03X, x=%d, y=%d, color=%d)", args[0], args[1], args[2], args[3]);
		break;
	case op_sub:
		fprintf(_out, "VAR(0x%02X) -= VAR(0x%02X)", args[0], args[1]);
		break;
	case op_and:
		fprintf(_out, "VAR(0x%02X) &= %d", args[0], args[1]);
		break;
	case op_or:
		fprintf(_out, "VAR(0x%02X) |= %d", args[0], args[1]);
		break;
	case op_shl:
		fprintf(_out, "VAR(0x%02X) <<= %d", args[0], args[1]);
		break;
	case op_shr:
		fprintf(_out, "VAR(0x%02X) >>= %d", args[0], args[1]);
		break;
	case op_playSound:
		fprintf(_out, "playSound(res=%d, freq=%d, vol=%d, channel=%d)", args[0], args[1], args[2], args[3]);
		break;
	case op_updateResources:
		fprintf(_out, "updateResources(res=%d)", args[0]);
		break;
	case op_playMusic:
		if (_is3DO) {
			fprintf(_out, "playMusic(%d)", args[0]);
		} else {
			fprintf(_out, "playMusic(res=%d, delay=%d, pos=%d)", args[0], args[1], args[2]);
		}
		break;
	case op_drawString2:
		if (_is3DO) {
			fprintf(_out, "drawString(%d, VAR(0x%02X), VAR(0x%02X), %d)", args[0], args[1], args[2], args[3]);
		}
		break;
	case op_jmpIfVarZero:
		if (_is3DO) {
			fprintf(_out, "jmpIf(VAR(0x%02X) == 0, @%04X)", args[0], args[1]);
		}
		break;
	case op_jmpIfVarNonZero:
		if (_is3DO) {
			fprintf(_out, "jmpIf(VAR(0x%02X) != 0, @%04X)", args[0], args[1]);
		}
		break;
	case op_printTime:
		if (_is3DO) {
			fprintf(_out, "print(VAR(0xF7))");
		}
		break;
	default:
		if (opcode & 0xC0) {
			const uint16_t offset = args[0];
			fprintf(_out, "drawShape(code=0x%02X, x=%d, y=%d", opcode, args[1], args[2]);
			const char *name = _shapeNames[offset];
			if (name[0]) {
				fprintf(_out, ", name=%s", name);
			}
			fprintf(_out, "); // offset=0x%04X (bank%d.mat)", offset << 1, args[3]);
		}
		break;
	}
	fputc('\n', _out);
}

static void (*visitOpcode)(uint16_t addr, uint8_t opcode, int args[16]);

static int parse(const uint8_t *buf, uint32_t size) {	
	const uint8_t *p = buf;
	while (p < buf + size) {
		uint16_t addr = p - buf;
		int a, b, c, d;
		const uint8_t op = *p++;
		if (op & 0x80) {
			a = *p++; // offset_lo
			b = *p++; // x
			c = *p++; // y
			int args[4] = { (((op & 0x7F) << 8) | a), b, c, 1 };
			visitOpcode(addr, op, args);
			continue;
		}
		if (op & 0x40) {
			a = readWord(p); p += 2; // offset
			b = *p++;
			if (!(op & 0x20) && !(op & 0x10)) {
				p++;
			}
			c = *p++;
			if (!(op & 8) && !(op & 4)) {
				p++;
			}
			if ((!(op & 2) && (op & 1)) || ((op & 2) && !(op & 1))) {
				p++;
			}
			int args[4] = { a, b, c, ((op & 3) == 3) ? 2 : 1 };
			visitOpcode(addr, op, args);
			continue;
		}
		a = b = c = d = 0;
		switch (op) {
		case 0: // op_movConst
			a = *p++;
			b = (int16_t)readWord(p); p += 2;
			break;
		case 1: // op_mov
			a = *p++;
			b = *p++;
			break;
		case 2: // op_add
			a = *p++;
			b = *p++;
			break;
		case 3: // op_addConst
			a = *p++;
			b = (int16_t)readWord(p); p += 2;
			break;
		case 4: // op_call
			a = readWord(p); p += 2;
			break;
		case 5: // op_ret
			break;
		case 6: // op_yieldTask
			break;
		case 7: // op_jmp
			a = readWord(p); p += 2;
			break;
		case 8: // op_installTask
			a = *p++;
			b = readWord(p); p += 2;
			break;
		case 9: // op_jmpIfVar
			a = *p++;
			b = readWord(p); p += 2;
			break;
		case 10: // op_condJmp
			a = *p++;			
			b = *p++;
			if (a & 0x80) {
				c = *p++;
			} else if (a & 0x40) {
				c = (int16_t)readWord(p); p += 2;
			} else {
				c = *p++;
			}
			d = readWord(p); p += 2;
			break;
		case 11: // op_setPalette
			a = *p++;
			if (!_is3DO) {
				p++;
			}
			break;
		case 12: // op_changeTasksState
			a = *p++;
			b = *p++;
			c = *p++;
			break;
		case 13: // op_selectPage
			a = *p++;
			break;
		case 14: // op_fillPage
			a = *p++;
			b = *p++;
			break;
		case 15: // op_copyPage
			a = *p++;
			b = *p++;
			break;
		case 16: // op_updateDisplay
			a = *p++;
			break;
		case 17: // op_removeTask
			break;
		case 18: // op_drawString
			a = readWord(p); p += 2;
			b = *p++;
			c = *p++;
			d = *p++;
			break;
		case 19: // op_sub
			a = *p++;
			b = *p++;
			break;
		case 20: // op_and
			a = *p++;
			b = readWord(p); p += 2;
			break;
		case 21: // op_or
			a = *p++;
			b = readWord(p); p += 2;
			break;
		case 22: // op_shl
			a = *p++;
			if (_is3DO) {
				b = *p++;
			} else {
				b = readWord(p); p += 2;
			}
			break;
		case 23: // op_shr
			a = *p++;
			if (_is3DO) {
				b = *p++;
			} else {
				b = readWord(p); p += 2;
			}
			break;
		case 24: // op_playSound
			a = readWord(p); p += 2;
			b = *p++;
			c = *p++;
			d = *p++;
			break;
		case 25: // op_updateResources
			a = readWord(p); p += 2;
			break;
		case 26: // op_playMusic
			if (_is3DO) {
				a = *p++;
			} else {
				a = readWord(p); p += 2;
				b = readWord(p); p += 2;
				c = *p++;
			}
			break;
		case 27: // op_drawString2
			if (_is3DO) {
				a = readWord(p); p += 2;
				b = *p++;
				c = *p++;
				d = *p++;
				break;
			}
		case 28: // op_jmpIfVarZero
			if (_is3DO) {
				a = *p++;
				b = readWord(p); p += 2;
				break;
			}
		case 29: // op_jmpIfVarNonZero
			if (_is3DO) {
				a = *p++;
				b = readWord(p); p += 2;
				break;
			}
		default:
			fprintf(stderr, "Invalid opcode %d at 0x%04x\n", op, addr);
			break;
		}
		int args[4] = { a, b, c, d };
		visitOpcode(addr, op, args);
	}
	return 0;
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
		read = fread(_fileBuf, 1, size, fp);
		if (read != size) {
			fprintf(stderr, "Failed to read %d bytes (%d)\n", size, read);
		}
		fclose(fp);
	}
	return size;
}

static const char *NAMES[] = { 0, "intro", "eau", "pri", "cite", "arene", "luxe", "final", 0 };
static const uint8_t RES[] = { 0x15, 0x18, 0x1B, 0x1E, 0x21, 0x24, 0x27, 0x2A, 0x7E };

int main(int argc, char *argv[]) {
	if (argc >= 3) {
		if (strcmp(argv[1], "-3do") == 0) {
			_is3DO = true;
		}
		--argc;
		++argv;
	}
	if (argc >= 2) {
		char path[MAXPATHLEN];
		struct stat st;
		if (stat(argv[1], &st) == 0 && S_ISREG(st.st_mode)) {
			strcpy(path, argv[1]);
			char *ext = strrchr(path, '.');
			if (ext) {
				strcpy(ext, ".nom");
				const int size = readFile(path);
				if (size != 0) {
					readShapeNames(_fileBuf, size);
				}
			}
			const int size = readFile(argv[1]);
			if (size != 0) {
				visitOpcode = checkOpcode;
				parse(_fileBuf, size);
				visitOpcode = printOpcode;
				parse(_fileBuf, size);
			}
		} else {
			for (int i = 0; i < 9; ++i) {
				snprintf(path, sizeof(path), argv[1], RES[i]);
				const int size = readFile(path);
				if (size == 0) {
					continue;
				}
				if (NAMES[i]) {
					snprintf(path, sizeof(path), "%d_%s.asm", 16000 + i, NAMES[i]);
				} else {
					snprintf(path, sizeof(path), "%d.asm", 16000 + i);
				}
				_out = fopen(path, "wb");
				if (_out) {
					memset(_addr, 0, sizeof(_addr));
					visitOpcode = checkOpcode;
					parse(_fileBuf, size);
					visitOpcode = printOpcode;
					parse(_fileBuf, size);
					fclose(_out);
				}
			}
		}
		for (int i = 0; i < MAX_OPCODES; ++i) {
			if (_histogramOp[i] != 0) {
				fprintf(stdout, "%d:%d ", i, _histogramOp[i]);
			}
		}
		fprintf(stdout, "\n");
	} else {
		fprintf(stdout, "Usage: %s [-3do] /path/to/File%%d\n", argv[0]);
	}
	return 0;
}
