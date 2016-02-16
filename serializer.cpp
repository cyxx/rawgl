/* 
 * Another World Interpreter 
 * (c) 2004-2005 Gregory Montoir
 */

#include "serializer.h"
#include "file.h"


Serializer::Serializer(File *stream, Mode mode, uint16 saveVer)
	: _stream(stream), _mode(mode), _saveVer(saveVer) {
}

void Serializer::saveOrLoadEntries(Entry *entry) {
	_bytesCount = 0;
	switch (_mode) {
	case SM_SAVE:
		saveEntries(entry);
		break;
	case SM_LOAD:
		loadEntries(entry);
		break;	
	}
}

void Serializer::saveEntries(Entry *entry) {
	for (; entry->type != SET_END; ++entry) {
		if (entry->maxVer == CUR_VER) {
			switch (entry->type) {
			case SET_INT:
				saveInt(entry->size, entry->data);
				_bytesCount += entry->size;
				break;
			case SET_ARRAY:
				if (entry->size == Serializer::SES_INT8) {
					_stream->write(entry->data, entry->n);
					_bytesCount += entry->n;
				} else {
					uint8 *p = (uint8 *)entry->data;
					for (int i = 0; i < entry->n; ++i) {
						saveInt(entry->size, p);
						p += entry->size;
						_bytesCount += entry->size;
					}
				}
				break;
			case SET_END:
				break;
			}
		}
	}
}

void Serializer::loadEntries(Entry *entry) {
	for (; entry->type != SET_END; ++entry) {
		if (_saveVer >= entry->minVer && _saveVer <= entry->maxVer) {
			switch (entry->type) {
			case SET_INT:
				loadInt(entry->size, entry->data);
				_bytesCount += entry->size;
				break;
			case SET_ARRAY:
				if (entry->size == Serializer::SES_INT8) {
					_stream->read(entry->data, entry->n);
					_bytesCount += entry->n;
				} else {
					uint8 *p = (uint8 *)entry->data;
					for (int i = 0; i < entry->n; ++i) {
						loadInt(entry->size, p);
						p += entry->size;
						_bytesCount += entry->size;
					}
				}
				break;
			case SET_END:
				break;				
			}
		}
	}
}

void Serializer::saveInt(uint8 es, void *p) {
	switch (es) {
	case 1:
		_stream->writeByte(*(uint8 *)p);
		break;
	case 2:
		_stream->writeUint16BE(*(uint16 *)p);
		break;
	case 4:
		_stream->writeUint32BE(*(uint32 *)p);
		break;
	}
}

void Serializer::loadInt(uint8 es, void *p) {
	switch (es) {
	case 1:
		*(uint8 *)p = _stream->readByte();
		break;
	case 2:
		*(uint16 *)p = _stream->readUint16BE();
		break;
	case 4:
		*(uint32 *)p = _stream->readUint32BE();
		break;
	}
}
