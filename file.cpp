/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */

#include "zlib.h"
#include "file.h"


struct File_impl {
	bool _ioErr;
	File_impl() : _ioErr(false) {}
	virtual bool open(const char *path, const char *mode) = 0;
	virtual void close() = 0;
	virtual uint32 size() = 0;
	virtual void seek(int32 off) = 0;
	virtual void read(void *ptr, uint32 sz) = 0;
	virtual void write(void *ptr, uint32 sz) = 0;
};

struct stdFile : File_impl {
	FILE *_fp;
	stdFile() : _fp(0) {}
	bool open(const char *path, const char *mode) {
		_ioErr = false;
		_fp = fopen(path, mode);
		return (_fp != NULL);
	}
	void close() {
		if (_fp) {
			fclose(_fp);
			_fp = 0;
		}
	}
	uint32 size() {
		uint32 sz = 0;
		if (_fp) {
			int pos = ftell(_fp);
			fseek(_fp, 0, SEEK_END);
			sz = ftell(_fp);
			fseek(_fp, pos, SEEK_SET);
		}
		return sz;
	}
	void seek(int32 off) {
		if (_fp) {
			fseek(_fp, off, SEEK_SET);
		}
	}
	void read(void *ptr, uint32 sz) {
		if (_fp) {
			uint32 r = fread(ptr, 1, sz, _fp);
			if (r != sz) {
				_ioErr = true;
			}
		}
	}
	void write(void *ptr, uint32 sz) {
		if (_fp) {
			uint32 r = fwrite(ptr, 1, sz, _fp);
			if (r != sz) {
				_ioErr = true;
			}
		}
	}
};

struct zlibFile : File_impl {
	gzFile _fp;
	zlibFile() : _fp(0) {}
	bool open(const char *path, const char *mode) {
		_ioErr = false;
		_fp = gzopen(path, mode);
		return (_fp != NULL);
	}
	void close() {
		if (_fp) {
			gzclose(_fp);
			_fp = 0;
		}
	}
	uint32 size() {
		uint32 sz = 0;
		if (_fp) {
			int pos = gztell(_fp);
			gzseek(_fp, 0, SEEK_END);
			sz = gztell(_fp);
			gzseek(_fp, pos, SEEK_SET);
		}
		return sz;
	}
	void seek(int32 off) {
		if (_fp) {
			gzseek(_fp, off, SEEK_SET);
		}
	}
	void read(void *ptr, uint32 sz) {
		if (_fp) {
			uint32 r = gzread(_fp, ptr, sz);
			if (r != sz) {
				_ioErr = true;
			}
		}
	}
	void write(void *ptr, uint32 sz) {
		if (_fp) {
			uint32 r = gzwrite(_fp, ptr, sz);
			if (r != sz) {
				_ioErr = true;
			}
		}
	}
};

File::File(bool gzipped) {
	if (gzipped) {
		_impl = new zlibFile;
	} else {
		_impl = new stdFile;
	}
}

File::~File() {
	_impl->close();
	delete _impl;
}

bool File::open(const char *filename, const char *directory, const char *mode) {	
	_impl->close();
	char buf[512];
	sprintf(buf, "%s/%s", directory, filename);
	char *p = buf + strlen(directory) + 1;
	string_lower(p);
	bool opened = _impl->open(buf, mode);
	if (!opened) { // let's try uppercase
		string_upper(p);
		opened = _impl->open(buf, mode);
	}
	return opened;
}

void File::close() {
	_impl->close();
}

bool File::ioErr() const {
	return _impl->_ioErr;
}

uint32 File::size() {
	return _impl->size();
}

void File::seek(int32 off) {
	_impl->seek(off);
}

void File::read(void *ptr, uint32 sz) {
	_impl->read(ptr, sz);
}

uint8 File::readByte() {
	uint8 b;
	read(&b, 1);
	return b;
}

uint16 File::readUint16LE() {
	uint8 lo = readByte();
	uint8 hi = readByte();
	return (hi << 8) | lo;
}

uint32 File::readUint32LE() {
	uint16 lo = readUint16BE();
	uint16 hi = readUint16BE();
	return (hi << 16) | lo;	
}

uint16 File::readUint16BE() {
	uint8 hi = readByte();
	uint8 lo = readByte();
	return (hi << 8) | lo;
}

uint32 File::readUint32BE() {
	uint16 hi = readUint16BE();
	uint16 lo = readUint16BE();
	return (hi << 16) | lo;
}

void File::write(void *ptr, uint32 sz) {
	_impl->write(ptr, sz);
}

void File::writeByte(uint8 b) {
	write(&b, 1);
}

void File::writeUint16BE(uint16 n) {
	writeByte(n >> 8);
	writeByte(n & 0xFF);
}

void File::writeUint32BE(uint32 n) {
	writeUint16BE(n >> 16);
	writeUint16BE(n & 0xFFFF);
}
