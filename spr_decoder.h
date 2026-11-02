#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <vector>

#pragma pack(push, 1)

typedef struct {
  BYTE Comment[4]; // "SPR\0" or 0x525053
  WORD Width;
  WORD Height;
  WORD CenterX;
  WORD CenterY;
  WORD Frames;
  WORD Colors;
  WORD Directions;
  WORD Interval;
  WORD Reserved[6];
} SPRHEAD;

#define SPR_COMMENT_FLAG 0x525053

typedef struct {
  DWORD Offset;
  DWORD Length;
} SPROFFS;

typedef struct {
  WORD Width;
  WORD Height;
  WORD OffsetX;
  WORD OffsetY;
  BYTE Sprite[1];
} SPRFRAME;

typedef struct {
  long lSize;
  long lCompressSize;
} GUIXPackSprFrameInfo;

typedef struct {
  unsigned int uOffset;
  unsigned int uSize;
  unsigned int uCompressSizeFlag;
} GUIXPackFragmentInfo;

typedef struct {
  unsigned int nNumFragment;
  unsigned int uSize;
} GUIXPackFragmentHeader;

#pragma pack(pop)

struct JxSprFrameArgb {
  int w = 0;
  int h = 0;
  int frameW = 0;
  int frameH = 0;
  int offsetX = 0;
  int offsetY = 0;
  int canvasW = 0;
  int canvasH = 0;
  int centerX = 0;
  int centerY = 0;
  int frames = 0;
  int colors = 0;
  int directions = 0;
  int interval = 0;
  std::vector<DWORD> pixels;
};

bool UclDecompressBuffer(const unsigned char *src, unsigned int src_len,
                         std::vector<unsigned char> &out,
                         unsigned int expectedSize,
                         bool allowSmallerOutput = false);

bool IsSprHeaderSignature(const unsigned char *p);
bool IsLikelySprBufferAt(const unsigned char *data, size_t size);
int GetSprFrameCountFromBytes(const std::vector<unsigned char> &bytes);
bool DecompressFrameSprToUncompressedSpr(const unsigned char *data, size_t size,
                                         std::vector<unsigned char> &out);

HBITMAP CreateBitmapFromSprBytes(const unsigned char *data, size_t size,
                                 int frameIndex, std::string &outInfo,
                                 bool cleanupDarkMatte = true);
