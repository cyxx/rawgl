#!/usr/bin/env python
#
# Convert data files from the ISO of Heart of the Alien to the DOS version memlist/bank structure.

import os
import os.path
import shutil
import struct
import sys

PARTS = {
	'intro7' : [ 0x17, 0x18, 0x19 ], # 16001
	'eau3'   : [ 0x1A, 0x1B, 0x1C ], # 16002
	'pri3'   : [ 0x1D, 0x1E, 0x1F ], # 16003
	'cite1'  : [ 0x20, 0x21, 0x22 ], # 16004
	'arene2' : [ 0x23, 0x24, 0x25 ], # 16005
	'luxe2'  : [ 0x26, 0x27, 0x28 ], # 16006
	'final3' : [ 0x29, 0x2A, 0x2B ], # 16007
	'code'   : [ 0x7D, 0x7E, 0x7F ]  # 16008
}

COUNT = 146

TYPE_SND = 0
TYPE_PAL = 3
TYPE_MAC = 4
TYPE_MAT = 5
TYPE_BNK = 6 # bank2.mat

class Resource(object):
	def __init__(self, rtype, data):
		self.rtype  = rtype
		self.data   = data
		self.offset = 0
		self.size   = len(data)

def convert_snd_amiga(data, length):
	assert (length & 1) == 0
	size_words = length / 2
	s  = struct.pack('>H', size_words) # length
	s += struct.pack('>H', size_words) # length
	# convert sign/magnitude to signed PCM
	for b in data:
		pcm = ord(b)
		if (pcm & 0x80) != 0:
			s += chr((-(pcm & 0x7F)) & 0xFF)
		else:
			s += b
	return s

def convert_snd(filepath, resources):
	print filepath
	with open(filepath, 'rb') as f:
		# sound offsets
		offset = struct.unpack('>I', f.read(4))[0]
		assert (offset & 3) == 0
		count = offset / 4
		offsets = [ offset ]
		for i in range(1, count):
			offset = struct.unpack('>I', f.read(4))[0]
			offsets.append(offset)
		offsets.append( os.path.getsize(filepath) )
		# sound num table
		length = offsets[1] - offsets[0]
		f.seek(offsets[0])
		mapping = f.read(length)
		first_num = ord(mapping[0])
		last_num  = ord(mapping[1])
		length -= 2
		count = last_num + 1 - first_num + 2
		assert length >= count
		for i in range(2, count):
			num = ord(mapping[i])
			if num != 0:
				sound_num = first_num + i - 2
				f.seek(offsets[num])
				header = struct.unpack('>I', f.read(4))[0]
				length = offsets[num + 1] - offsets[num]
				print 'sound #%d num 0x%02x header %d length %d' % (num, sound_num, header, length)
				assert length >= header
				resources[ sound_num ] = Resource(TYPE_SND, convert_snd_amiga(f.read(header), length))

def convert(dirpath):
	resources = [ None for i in range(COUNT) ]
	for name, indexes in PARTS.items():
		mac = os.path.join(dirpath, name + '.mac')
		resources[ indexes[1] ] = Resource(TYPE_MAC, file(mac).read())
		mat = os.path.join(dirpath, name + '.mat')
		resources[ indexes[2] ] = Resource(TYPE_MAT, file(mat).read())
		if name[-1].isdigit():
			snd = os.path.join(dirpath, name[:-1] + '.snd')
			if os.path.exists(snd):
				convert_snd(snd, resources)
		# palettes are possibly stored in make2s.bin (0x5A88, 0x5D28), use external files for now
		pal = 'data_%02x_%d' % (indexes[0], TYPE_PAL)
		resources[ indexes[0] ] = Resource(TYPE_PAL, file(pal).read())
	# also use an external file for bank2.mat
	bnk = 'data_11_%d' % TYPE_BNK
	resources[ 0x11 ] = Resource(TYPE_BNK, file(bnk).read())
	# generate bank0f
	with open('bank0f', 'wb') as f:
		offset = 0
		for r in resources:
			if r:
				r.offset = offset
				f.write(r.data)
				offset += r.size
	# generate memlist.bin
	with open('memlist.bin', 'wb') as f:
		dummy = '\x00' * 20
		for r in resources:
			if r:
				s  = '\x00' # status
				s += chr(r.rtype)
				s += struct.pack('>I', 0) # pointer
				s += '\x00' # rank
				s += chr(0xF) # bank
				s += struct.pack('>I', r.offset) # data offset
				s += struct.pack('>I', r.size) # compressed size
				s += struct.pack('>I', r.size) # uncompresssed size
				assert len(s) == 20
				f.write(s)
			else:
				f.write(dummy)
		f.write('\xff')

convert(sys.argv[1])
