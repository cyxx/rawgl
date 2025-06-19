
# Another World 3DO Technical Notes

Another World has been ported on many platforms. The way the game was written (interpreted game logic) clearly helped.

This document focuses on the 3DO release made by Interplay in 1994. This version was not a straight port. In addition to reworking the assets, the game code was modified.

- [Assets](#assets)
- [Bytecode](#bytecode)
- [Resources](#resources)
- [Rendering](#rendering)
- [Introduction Sequence Synchronization](#introduction-sequence-synchronization)
- [Game Passwords](#game-passwords)

## Assets

Type     | Amiga/DOS | 3DO
-------- | --------- | ---
Music    | 4 channels tracker with raw signed 8bits samples (3 files) | AIFF SDX2 compressed audio (30 files)
Sound    | raw signed 8bits mono (103 files)                          | AIFF signed 16 bits (92 files)
Bitmap   | 4 bits depth paletted 320x200 (8 files)                    | True-color RGB555 320x200 (139 files)
Bytecode | Big-endian (Motorola 68000)                                | Little-endian (ARMv3)

## Bytecode

The game engine is based on a virtual machine with 30 opcodes.

### Opcodes

The game was developed on an Amiga. As the Motorola 68000 CPU was big endian, this is the byte order found in the bytecode on the original Amiga/DOS versions.
Most of the opcodes operand size is 2 bytes, matching the register size of the target 68000 CPU.

* ALU

Num | Name | Parameters | Description
--- | ---- | ---------- | ---
0   | movConst | var: byte, value: word           | var := value
1   | mov      | var(dst): byte, var(src): byte   | dst := src
2   | add      | var(dst): byte, var(src): byte   | dst += src
3   | addConst | var: byte; value: word           | var += value
19  | sub      | var(dst): byte, var(src): byte   | var -= src
20  | and      | var: byte, value: word           | var &= value
21  | or       | var: byte, value: word           | var |= value
22  | shl      | var: byte, count: word           | var <<= count
23  | shr      | var: byte, count: word           | var >>= count

* Control flow

Num | Name | Parameters | Description
--- | ---- | ---------- | ---
4   | call     | offset: word            | function call
5   | ret      |                         | return from function call
7   | jmp      | offset: word            | pc = offset
9   | jmpIfVar | var: byte, offset: word | --var != 0: pc = offset (dbra)
10  | condJmp  | operator: byte, var(operand1): byte, operand2: byte/word, offset: word | cmp op1, op2: pc = offset

* Coroutines

Num | Name | Parameters | Description
--- | ---- | ---------- | ---
8   | installTask      | num: byte, offset: word            | setup a coroutine, pc = offset
6   | yieldTask        |                                    | pause current coroutine
12  | changeTasksState | num1: byte, num2: byte, state:byte | change coroutines num1..num2 state
17  | removeTask       |                                    | abort current coroutine

* Display

Num | Name | Parameters | Description
--- | ---- | ---------- | ---
11  | setPalette    | num: word                                        | set palette (16 colors)
13  | selectPage    | num: byte                                        | set current drawing page
14  | fillPage      | num: byte, color: byte                           | fill page with color
15  | copyPage      | num(dst): byte, num(src): byte                   | copy page content
16  | updateDisplay | num: byte                                        | present page to screen
18  | drawString    | num: word, x(/8): byte, y(/8): byte, color: byte | draw string

* Assets

Num | Name | Parameters | Description
--- | ---- | ---------- | ---
24  | playSound       | num: word, frequency: byte, volume: byte, channel: byte | play sound
25  | updateResources | num: word                                               | load asset resource or section
26  | playMusic       | num: word, pitch: word, position: byte                  | play music

### 3DO Specific Opcodes

The 3DO port does not use the same bytecode. The game code was modified, recompiled and saved under the target endianness of the target ARM CPU of the 3DO.
In the process, some opcodes have been added and some others have been optimized for size by using one byte operand when applicable.

The new shift opcode is a good example : the original bytecode was using a 16bits value to specify the number of bits to shift on the target 16 bits register.
8 bits were twice as enough.

Num | Name | Parameters | Description
--- | ---- | -----------| ---
11  | setPalette    | num: byte                                          |
22  | shiftLeft     | var: byte, count: byte                             | var <<= count
23  | shiftRight    | var: byte, count: byte                             | var >>= count
26  | playMusic     | num: byte                                          |
27  | drawString    | num: byte, var(x): byte, var(y): byte, color: byte |
28  | jmpIfVarFalse | var: byte, offset: word                            | if var == 0: pc = offset
29  | jmpIfVarTrue  | var: byte, offset: word                            | if var != 0: pc = offset
30  | printTime     |                                                    | output running time (via SWI 0x1000E), not referenced in game code

## Resources

As any 3DO game, the data-files are read from an OperaFS CD-ROM.

Inside the GameData/ directory
* The game data-files are numbered from 1 to 340 (File%d)
* The song files (AIFF-C) are numbered from 1 to 30 (song%d)
* Three cinematics files : Logo.Cine, Spintitle.Cine, ootw2.cine

The files are stored uncompressed at the exception of the background bitmaps.
The decompression code can be found in the DOOM 3DO source code - [dlzss.s](https://github.com/Olde-Skuul/doom3do/blob/master/lib/burger/dlzss.s)

## Rendering

### Transparency

The engine can display semi-transparent shapes such as the car lights in the introduction.

![Screenshot Intro Amiga](screenshot-intro-amiga.png) ![Screenshot Intro 3DO](screenshot-intro-3do.png)

The original Amiga/DOS game used a palette of 16 colors. The semi-transparency is achieved by allocating the upper half of the palette to the transparent colors.
The palette indexes 0 to 7 hold the scene colors, indexes 8 to 15 the blended colors.

![Palette Intro Amiga](palette-intro-amiga.png)

Turning a pixel semi-transparent is simply done by setting to 1 the bit 3 of the palette color index (|= 8).

This is not directly applicable to the 3DO which renders everything with true-color buffers.
The 3DO engine leverages the console capabilities to render an alpha blended shape. Interestingly, the color of the lights is not yellow in that version.

![Screenshot Intro Amiga](screenshot-intro-amiga-light.png) ![Screenshot Intro 3DO](screenshot-intro-3do-light.png)

### Background Bitmaps

While the original Amiga/DOS game relied on polygons for most of its graphics, the game engine supports using a raster bitmap to be used as the background.
In the original game version, this is only used for 8 different screens.

![file067](file067.png) ![file068](file068.png) ![file069](file069.png) ![file070](file070.png)

![file072](file072.png) ![file073](file073.png) ![file144](file144.png) ![file145](file145.png)

The 3DO version uses the feature for all the screens of the game. For comparison, the same screens in RGB555.

![File246](File246.png) ![File224](File224.png) ![File225](File225.png) ![File226](File226.png)

![File221](File221.png) ![File220](File220.png) ![File306](File306.png) ![File302](File302.png)

### Drawing Primitives

The original Amiga/DOS used only one primitive for all its drawing : a quad (4 vertices).

The 3DO reworked that format, probably to optimize shapes made of single pixels or straight lines.
The upper nibble of the shape color byte specifies the primitive to draw.

```
0x00 : nested/composite shape
0x20 : rectangle
0x40 : pixel
0xC0 : polygon/quad
```

## Introduction Sequence Synchronization

On Amiga/DOS, the introduction is synchronized to the music.

The music module contains 2 bytes patterns, that are copied to the variable 0xF4.

```
if (pat.note_1 == -3) {
    _vars[0xF4] = pat.note_2;
}
```

The condition can be found in the original 68000 SoundFX player [routine](https://github.com/cyxx/rawgl/blob/master/docs/fxplayer.s#L555).

The bytecode contains checks on this variable to wait and continue.

```
000B: (00) VAR(0xF4) = 0
0F66: (0A) jmpIf(VAR(0xF4) == 43, @0F91)
...
1399: (0A) jmpIf(VAR(0xF4) == 46, @13AA)
13AE: (0A) jmpIf(VAR(0xF4) == 47, @13BF)
13C3: (0A) jmpIf(VAR(0xF4) == 48, @13D4)
13F6: (0A) jmpIf(VAR(0xF4) != 49, @1406)
```


On the 3DO, the tracker based music has been replaced with digital tracks.
The synchronization had to be modified.

That version relies on the variable 0xF7. It holds the total number of VBLs since the game started.

```
nbtrame = readTick() - OldVBL;
if (_vars[0xFF] != 0) {
    while (_vars[0xFF] > nbtrame) {
       nbtrame = readTick() - OldVBL;
    }
}
_vars[0xF7] += nbtrame;
OldVBL += nbtrame;
```

The introduction bytecode has the checks against this variable for the timing.

```
0068: jmpIf(VAR(0xF7) < 3450, @0067)
043E: jmpIf(VAR(0xF7) < 7484, @043D)
046A: jmpIf(VAR(0xF7) < 8602, @0469)
jmpIf(VAR(0xF7) < 9212, @0576)
...
jmpIf(VAR(0xF7) < 400, @1C40)
```

## Game Passwords

Game passwords extracted from the 16008 part bytecode.

Code | Part  | Checkpoint | Notes
---- | ----- | ---------- | -----
LDKD | 16002 | 10 |
HTDC | 16003 | 20 |
CLLD | 16004 | 30 |
FXLC | 16004 | 35 |
KRFK | 16004 | 37 |
XDDJ | 16004 | 33 |
LBKG | 16004 | 31 |
KLFB | 16004 | 39 |
TTCT | 16004 | 41 |
DDRX | 16004 | 42 |
TBHK | 16004 | 43 |
BRTD | 16004 | 49 |
CKJL | 16005 | 50 |
LFCK | 16006 | 60 |
BFLX | 16004 | 44 |
XJRT | 16004 | 45 |
HRTB | 16004 | 46 |
HBHK | 16004 | 47 |
JCGB | 16004 | 48 |
HHFL | 16006 | 62 |
TFBB | 16006 | 64 |
TXHF | 16006 | 66 |
KRTD | 16007 | 70 |
BRGR | 16010 | -1 | Stalactites
