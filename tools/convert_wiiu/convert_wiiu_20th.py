#!/usr/bin/env python
#
# Convert data files from the WiiU version of 'Another World 20th Anniversary Edition'
# to the Linux/Mac/Windows releases directory structure.
#
# $ tree -d game/
#  game/
#  |-- BGZ
#  |   |-- data1152x720
#  |   |-- data1280x800
#  |-- DAT
#  |-- OGG
#  |   |-- original
#  |-- PNG
#  |-- TXT
#  |   |-- Linux
#  |-- WGZ
#      |-- original

import gzip
import os
import os.path
import shutil
import sys
from PIL import Image

OUT = 'out/game/'

PARTS = {
	'intro2011' : [ 0x17, 0x18, 0x19 ], # 16001
	'eau2011'   : [ 0x1A, 0x1B, 0x1C ], # 16002
	'pri2011'   : [ 0x1D, 0x1E, 0x1F ], # 16003
	'cite2011'  : [ 0x20, 0x21, 0x22 ], # 16004
	'arene2011' : [ 0x23, 0x24, 0x25 ], # 16005
	'luxe2011'  : [ 0x26, 0x27, 0x28 ], # 16006
	'final2011' : [ 0x29, 0x2A, 0x2B ]  # 16007
}

BITMAP_SIZE = '1728x1080'

def convert_awt(dir, out):
	dir = os.path.join(dir, 'data' + BITMAP_SIZE)
	for png in os.listdir(dir):
		(name, ext) = os.path.splitext(png)
		if ext == '.png':
			source = os.path.join(dir, png)
			target = os.path.join(out, BITMAP_SIZE + '_' + name + '.bmp')
			with Image.open(source) as im:
				im.save(target)
			source = target
			target = os.path.join(out, BITMAP_SIZE + '_' + name + '.bgz')
			with gzip.open(target, 'wb') as f:
				shutil.copyfileobj(file(source), f)

def convert_bmp(dir, out):
	for bmp in os.listdir(dir):
		(name, ext) = os.path.splitext(bmp)
		if ext == '.bmp':
			if name.startswith('font') or name.startswith('heads'):
				name = name.capitalize()
			source = os.path.join(dir, bmp)
			target = os.path.join(out, name + '.bgz')
			with gzip.open(target, 'wb') as f:
				shutil.copyfileobj(file(source), f)

def convert_dat(dir, out):
	for dat in os.listdir(dir):
		num = -1
		if dat == 'bank2.mat':
			num = 0x11
		else:
			(name, ext) = os.path.splitext(dat)
			name = name.lower()
			if PARTS.has_key(name):
				ext = ext.lower()
				if ext == '.pal':
					num = PARTS[name][0]
				elif ext == '.mac':
					num = PARTS[name][1]
				elif ext == '.mat':
					num = PARTS[name][2]
		if num != -1:
			source = os.path.join(dir, dat)
			target = os.path.join(out, 'FILE%03d.DAT' % num)
			shutil.copy(source, target)


def convert_txt(dir, out):
	for txt in os.listdir(dir):
		(name, ext) = os.path.splitext(txt)
		if ext == '.txt':
			source = os.path.join(dir, txt)
			target = os.path.join(out, name.upper() + '.txt')
			shutil.copy(source, target)

def convert_wgz(dir, out):
	for wav in os.listdir(dir):
		(name, ext) = os.path.splitext(wav)
		if ext == '.wav':
			source = os.path.join(dir, wav)
			target = os.path.join(out, name + '.wgz')
			with gzip.open(target, 'wb') as f:
				shutil.copyfileobj(file(source), f)

TYPES = {
	'awt': [ convert_awt, 'BGZ/data' + BITMAP_SIZE ],
	'bmp': [ convert_bmp, 'BGZ' ],
	'dat': [ convert_dat, 'DAT' ],
	'txt': [ convert_txt, 'TXT' ],
	'wgz_mixed': [ convert_wgz, 'WGZ' ]
}

dir = 'game'
if len(sys.argv) > 1:
	dir = sys.argv[1]

for f in os.listdir(dir):
	if TYPES.has_key(f):
		print "Converting '%s'" % f
		out = os.path.join(OUT, TYPES[f][1])
		try:
			os.makedirs(out)
		except:
			pass
		TYPES[f][0](os.path.join(dir, f), out)
