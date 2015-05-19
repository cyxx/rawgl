
#ifndef TGA_H__
#define TGA_H__

struct TgaFile;

struct TgaFile *tgaOpen(const char *filePath, int width, int height, int depth);
void tgaClose(struct TgaFile *tgaData);
void tgaSetLookupColorTable(struct TgaFile *tgaData, const unsigned char *paletteData);
void tgaWritePixelsData(struct TgaFile *tgaData, const void *pixelsData, int pixelsDataSize);

#endif // TGA_H__

