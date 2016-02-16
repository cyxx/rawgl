/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */

#ifndef __SERIALIZER_H__
#define __SERIALIZER_H__

#include "intern.h"

#define VER(x) x

#define SE_INT(i,sz,ver)     { Serializer::SET_INT, sz, 1, i, ver, Serializer::CUR_VER }
#define SE_ARRAY(a,n,sz,ver) { Serializer::SET_ARRAY, sz, n, a, ver, Serializer::CUR_VER }
#define SE_END()             { Serializer::SET_END, 0, 0, 0, 0, 0 }

struct File;

struct Serializer {
	enum {
		CUR_VER = 1
	};

	enum EntryType {
		SET_INT,
		SET_ARRAY,
		SET_END
	};

	enum {
		SES_BOOL  = 1,
		SES_INT8  = 1,
		SES_INT16 = 2,
		SES_INT32 = 4
	};

	enum Mode {
		SM_SAVE,
		SM_LOAD
	};

	struct Entry {
		EntryType type;
		int size;
		int n;
		void *data;
		uint16 minVer;
		uint16 maxVer;
	};

	File *_stream;
	Mode _mode;
	uint16 _saveVer;
	int _bytesCount;
	
	Serializer(File *stream, Mode mode, uint16 saveVer = CUR_VER);

	void saveOrLoadEntries(Entry *entry);

	void saveEntries(Entry *entry);
	void loadEntries(Entry *entry);

	void saveInt(uint8 es, void *p);
	void loadInt(uint8 es, void *p);
};

#endif
