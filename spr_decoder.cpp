#define NOMINMAX
#include "spr_decoder.h"

extern "C" {
#include "ucl/ucl.h"
}

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

static bool SafeMulSize(size_t a, size_t b, size_t &out) {
  if (a != 0 && b > ((size_t)-1) / a)
    return false;
  out = a * b;
  return true;
}

static bool SafeAddSize(size_t a, size_t b, size_t &out) {
  if (b > ((size_t)-1) - a)
    return false;
  out = a + b;
  return true;
}

static WORD ReadLe16(const unsigned char *p) {
  return (WORD)((WORD)p[0] | ((WORD)p[1] << 8));
}

static short ReadLe16S(const unsigned char *p) {
  return (short)((WORD)p[0] | ((WORD)p[1] << 8));
}

static bool ScanSprFrameMaxAlpha(const unsigned char *src,
                                 const unsigned char *srcEnd, int frameW,
                                 int frameH, BYTE &outMaxAlpha) {
  outMaxAlpha = 0;
  if (!src || !srcEnd || frameW <= 0 || frameH <= 0 || src > srcEnd)
    return false;

  const unsigned char *p = src;
  for (int line = 0; line < frameH; ++line) {
    int x = 0;
    while (x < frameW) {
      if ((size_t)(srcEnd - p) < 2)
        return false;
      const int count = (int)*p++;
      const BYTE alpha = *p++;
      if (count <= 0)
        return false;
      if (alpha > outMaxAlpha)
        outMaxAlpha = alpha;
      if (alpha != 0) {
        if ((size_t)(srcEnd - p) < (size_t)count)
          return false;
        p += count;
      }
      x += count;
    }
  }
  return true;
}

static bool ParseSprLayout(const unsigned char *data, size_t size,
                           const SPRHEAD *&head,
                           const unsigned char *&palette24,
                           const SPROFFS *&offsets,
                           const unsigned char *&spriteBase,
                           size_t &spriteBytes,
                           size_t &headerAndTable) {
  head = nullptr;
  palette24 = nullptr;
  offsets = nullptr;
  spriteBase = nullptr;
  spriteBytes = 0;
  headerAndTable = 0;

  if (!data || size < sizeof(SPRHEAD) || !IsSprHeaderSignature(data))
    return false;

  head = reinterpret_cast<const SPRHEAD *>(data);
  if (head->Frames == 0 || head->Frames > 20000u)
    return false;

  const size_t headerSize = sizeof(SPRHEAD);
  size_t numColors = (size_t)head->Colors;
  if (numColors == 0)
    numColors = 256; // Default standard 256 palette

  size_t paletteSize = 0;
  if (!SafeMulSize(numColors, 3u, paletteSize))
    return false;
  size_t tableSize = 0;
  if (!SafeMulSize((size_t)head->Frames, sizeof(SPROFFS), tableSize))
    return false;
  size_t headerAndPalette = 0;
  if (!SafeAddSize(headerSize, paletteSize, headerAndPalette))
    return false;
  if (!SafeAddSize(headerAndPalette, tableSize, headerAndTable))
    return false;

  if (size < headerAndTable)
    return false;

  palette24 = data + headerSize;
  offsets = reinterpret_cast<const SPROFFS *>(data + headerAndPalette);
  spriteBase = data + headerAndTable;
  spriteBytes = size - headerAndTable;
  return true;
}

static bool DecodeSprFrameInternal(const unsigned char *data, size_t size,
                                   int frameIndex, bool placeOnCanvas,
                                   JxSprFrameArgb &outFrame,
                                   bool premulOutput = false,
                                   bool cleanupDarkMatte = false) {
  outFrame = JxSprFrameArgb{};

  const SPRHEAD *head = nullptr;
  const unsigned char *palette24 = nullptr;
  const SPROFFS *offsets = nullptr;
  const unsigned char *spriteBase = nullptr;
  size_t spriteBytes = 0;
  size_t headerAndTable = 0;
  if (!ParseSprLayout(data, size, head, palette24, offsets, spriteBase, spriteBytes, headerAndTable))
    return false;

  int fi = frameIndex;
  if (fi < 0 || fi >= (int)head->Frames)
    fi = 0;

  const SPROFFS &offs = offsets[fi];
  size_t frameOffset = (size_t)offs.Offset;
  size_t frameSize = (size_t)offs.Length;

  // Handle offset relative to file start vs relative to spriteBase
  if (frameOffset >= headerAndTable && frameOffset < size) {
    frameOffset -= headerAndTable;
  }

  if (frameOffset > spriteBytes || frameSize < 8 || frameSize > spriteBytes - frameOffset)
    return false;

  const unsigned char *frameData = spriteBase + frameOffset;
  const unsigned char *src = frameData + 8;
  const unsigned char *srcEnd = frameData + frameSize;

  const int frameW = (int)ReadLe16(frameData + 0);
  const int frameH = (int)ReadLe16(frameData + 2);
  const int offsetX = (int)ReadLe16S(frameData + 4);
  const int offsetY = (int)ReadLe16S(frameData + 6);
  if (frameW <= 0 || frameH <= 0 || frameW > 4096 || frameH > 4096)
    return false;

  BYTE frameMaxAlpha = 0;
  if (!ScanSprFrameMaxAlpha(src, srcEnd, frameW, frameH, frameMaxAlpha))
    return false;

  int canvasW = (int)head->Width;
  int canvasH = (int)head->Height;
  if (canvasW > 4096) canvasW = 4096;
  if (canvasH > 4096) canvasH = 4096;

  // Ensure canvas is large enough to contain the frame at its offset
  if (placeOnCanvas && canvasW > 0 && canvasH > 0) {
    int neededW = frameW + (std::max)(0, offsetX);
    int neededH = frameH + (std::max)(0, offsetY);
    if (neededW > canvasW) canvasW = (std::min)(4096, neededW);
    if (neededH > canvasH) canvasH = (std::min)(4096, neededH);
  } else {
    // Fallback: no valid header canvas
    if (canvasW <= 0) canvasW = (std::min)(4096, frameW + (std::max)(0, offsetX));
    if (canvasH <= 0) canvasH = (std::min)(4096, frameH + (std::max)(0, offsetY));
  }

  int dstW = placeOnCanvas ? canvasW : frameW;
  int dstH = placeOnCanvas ? canvasH : frameH;
  if (dstW <= 0 || dstH <= 0 || dstW > 4096 || dstH > 4096)
    return false;

  size_t pixelCount = (size_t)dstW * (size_t)dstH;
  size_t numColors = (size_t)head->Colors > 0 ? (size_t)head->Colors : 256;

  std::vector<DWORD> palette(numColors);
  for (size_t i = 0; i < numColors; ++i) {
    const BYTE r = palette24[i * 3 + 0];
    const BYTE g = palette24[i * 3 + 1];
    const BYTE b = palette24[i * 3 + 2];
    // True 24-bit original RGB color (no downsampling or color reduction)
    palette[i] = 0xFF000000u | ((DWORD)r << 16) | ((DWORD)g << 8) | (DWORD)b;
  }

  std::vector<DWORD> pixels(pixelCount, 0x00000000u);
  const unsigned char *p = src;
  for (int line = 0; line < frameH; ++line) {
    int x = 0;
    while (x < frameW) {
      if ((size_t)(srcEnd - p) < 2)
        return false;
      const int count = (int)*p++;
      const BYTE alpha = *p++;
      if (count <= 0)
        return false;

      if (alpha == 0) {
        x += count;
        continue;
      }

      if ((size_t)(srcEnd - p) < (size_t)count)
        return false;

      BYTE drawAlpha = alpha;
      if (frameMaxAlpha <= 32) {
        if (alpha >= 32)
          drawAlpha = 255;
        else
          drawAlpha = (BYTE)(((unsigned int)alpha * 255u + 16u) / 32u);
      }

      for (int i = 0; i < count; ++i) {
        const unsigned char colorIndex = *p++;
        if ((size_t)colorIndex >= palette.size())
          continue;

        const int targetX = placeOnCanvas ? (offsetX + x + i) : (x + i);
        const int targetY = placeOnCanvas ? (offsetY + line) : line;
        if (targetX < 0 || targetX >= dstW || targetY < 0 || targetY >= dstH)
          continue;

        const size_t dstIndex = (size_t)targetY * (size_t)dstW + (size_t)targetX;
        DWORD rgb = palette[colorIndex];
        BYTE r = (BYTE)((rgb >> 16) & 0xFF);
        BYTE g = (BYTE)((rgb >> 8) & 0xFF);
        BYTE b = (BYTE)(rgb & 0xFF);

        if (premulOutput) {
          BYTE pr = (BYTE)(((UINT)r * (UINT)drawAlpha + 127u) / 255u);
          BYTE pg = (BYTE)(((UINT)g * (UINT)drawAlpha + 127u) / 255u);
          BYTE pb = (BYTE)(((UINT)b * (UINT)drawAlpha + 127u) / 255u);
          pixels[dstIndex] = ((DWORD)drawAlpha << 24) | ((DWORD)pr << 16) |
                             ((DWORD)pg << 8) | pb;
        } else {
          pixels[dstIndex] = ((DWORD)drawAlpha << 24) | ((DWORD)r << 16) |
                             ((DWORD)g << 8) | b;
        }
      }
      x += count;
    }
  }

  outFrame.w = dstW;
  outFrame.h = dstH;
  outFrame.frameW = frameW;
  outFrame.frameH = frameH;
  outFrame.offsetX = offsetX;
  outFrame.offsetY = offsetY;
  outFrame.canvasW = canvasW;
  outFrame.canvasH = canvasH;
  outFrame.centerX = (int)head->CenterX;
  outFrame.centerY = (int)head->CenterY;
  outFrame.frames = (int)head->Frames;
  outFrame.colors = (int)numColors;
  outFrame.directions = (int)head->Directions;
  outFrame.interval = (int)head->Interval;
  outFrame.pixels = std::move(pixels);
  return true;
}

static bool ResolveSprHeaderBuffer(const unsigned char *data, size_t size,
                                   const unsigned char *&outSpr,
                                   size_t &outSize) {
  outSpr = nullptr;
  outSize = 0;
  if (!data || size < sizeof(SPRHEAD))
    return false;

  if (IsLikelySprBufferAt(data, size)) {
    outSpr = data;
    outSize = size;
    return true;
  }
  if (size >= 8 && IsLikelySprBufferAt(data + 8, size - 8)) {
    outSpr = data + 8;
    outSize = size - 8;
    return true;
  }
  for (size_t off = 0; off + sizeof(SPRHEAD) <= size && off <= 64; ++off) {
    if (IsLikelySprBufferAt(data + off, size - off)) {
      outSpr = data + off;
      outSize = size - off;
      return true;
    }
  }
  return false;
}

static bool DecompressFragmentedSprBuffer(const unsigned char *data, size_t size,
                                          std::vector<unsigned char> &out) {
  out.clear();
  if (!data || size < sizeof(GUIXPackFragmentHeader))
    return false;

  const GUIXPackFragmentHeader &head = *(const GUIXPackFragmentHeader *)data;
  if (head.nNumFragment == 0 || head.nNumFragment > 4096u)
    return false;
  if (head.uSize == 0 || head.uSize > 512u * 1024u * 1024u)
    return false;

  const size_t tableSize =
      (size_t)head.nNumFragment * sizeof(GUIXPackFragmentInfo);
  if (size < sizeof(GUIXPackFragmentHeader) + tableSize)
    return false;

  const GUIXPackFragmentInfo *fragList =
      (const GUIXPackFragmentInfo *)(data + sizeof(GUIXPackFragmentHeader));
  out.reserve(head.uSize);

  for (unsigned int i = 0; i < head.nNumFragment; ++i) {
    const GUIXPackFragmentInfo &frag = fragList[i];
    unsigned int stored = frag.uCompressSizeFlag & 0x00FFFFFF;
    if (stored == 0)
      stored = frag.uSize;
    if (frag.uOffset + stored > size)
      return false;

    const unsigned char *src = data + frag.uOffset;
    if ((frag.uCompressSizeFlag & 0xFF000000) == 0) {
      if (stored < frag.uSize)
        return false;
      out.insert(out.end(), src, src + frag.uSize);
      continue;
    }

    std::vector<unsigned char> tmp;
    if (!UclDecompressBuffer(src, stored, tmp, frag.uSize)) {
      if (!UclDecompressBuffer(src, stored, tmp, frag.uSize, true))
        return false;
    }
    out.insert(out.end(), tmp.begin(), tmp.end());
  }
  return true;
}

static bool TryDecompressClassicFrameSprPayload(
    const unsigned char *sprData, size_t sprSize,
    std::vector<unsigned char> &out) {
  out.clear();
  if (!sprData || sprSize < sizeof(SPRHEAD) || !IsSprHeaderSignature(sprData))
    return false;

  const SPRHEAD *head = (const SPRHEAD *)sprData;
  if (head->Frames == 0 || head->Frames > 4096u)
    return false;

  size_t numColors = (size_t)head->Colors > 0 ? (size_t)head->Colors : 256;
  const size_t headerSize = sizeof(SPRHEAD);
  const size_t paletteSize = numColors * 3u;
  const size_t frameInfoSize =
      (size_t)head->Frames * sizeof(GUIXPackSprFrameInfo);
  const size_t headerAndPal = headerSize + paletteSize;
  if (sprSize < headerAndPal + frameInfoSize)
    return false;

  const GUIXPackSprFrameInfo *frameList =
      (const GUIXPackSprFrameInfo *)(sprData + headerAndPal);
  const unsigned char *readPtr = sprData + headerAndPal + frameInfoSize;

  std::vector<unsigned char> spriteData;
  std::vector<SPROFFS> offsets(head->Frames);
  DWORD runningOffset = 0;

  for (unsigned int i = 0; i < head->Frames; ++i) {
    const long compSize = frameList[i].lCompressSize;
    const long rawSize = frameList[i].lSize;
    if (compSize <= 0)
      return false;
    if ((size_t)(readPtr - sprData) + (size_t)compSize > sprSize)
      return false;

    if (rawSize > 0) {
      std::vector<unsigned char> decompressed;
      if (UclDecompressBuffer(readPtr, (unsigned int)compSize, decompressed,
                              (unsigned int)rawSize)) {
        offsets[i].Offset = runningOffset;
        offsets[i].Length = (DWORD)rawSize;
        runningOffset += (DWORD)rawSize;
        spriteData.insert(spriteData.end(), decompressed.begin(),
                          decompressed.end());
      } else if (rawSize == compSize) {
        offsets[i].Offset = runningOffset;
        offsets[i].Length = (DWORD)compSize;
        runningOffset += (DWORD)compSize;
        spriteData.insert(spriteData.end(), readPtr, readPtr + compSize);
      } else {
        return false;
      }
    } else {
      const DWORD rawLen = (DWORD)(-rawSize);
      offsets[i].Offset = runningOffset;
      offsets[i].Length = rawLen;
      runningOffset += rawLen;
      size_t copyLen = (size_t)compSize;
      if (copyLen > (size_t)rawLen)
        copyLen = (size_t)rawLen;
      spriteData.insert(spriteData.end(), readPtr, readPtr + copyLen);
      if (copyLen < (size_t)rawLen)
        spriteData.insert(spriteData.end(), (size_t)rawLen - copyLen, 0);
    }
    readPtr += compSize;
  }

  out.resize(headerSize + paletteSize + (size_t)head->Frames * sizeof(SPROFFS) +
             spriteData.size());
  unsigned char *dst = out.data();
  std::memcpy(dst, sprData, headerSize + paletteSize);
  dst += headerSize + paletteSize;
  std::memcpy(dst, offsets.data(), (size_t)head->Frames * sizeof(SPROFFS));
  dst += (size_t)head->Frames * sizeof(SPROFFS);
  if (!spriteData.empty())
    std::memcpy(dst, spriteData.data(), spriteData.size());
  return true;
}

static int SafeUclCall(int (*fn)(const ucl_bytep, ucl_uint, ucl_bytep, ucl_uintp, ucl_voidp),
                       const unsigned char *src, unsigned int src_len,
                       unsigned char *dst, ucl_uint *dst_len) {
  __try {
    return fn((const ucl_bytep)src, (ucl_uint)src_len, (ucl_bytep)dst, dst_len, nullptr);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -999;
  }
}

} // namespace

bool UclDecompressBuffer(const unsigned char *src, unsigned int src_len,
                         std::vector<unsigned char> &out,
                         unsigned int expectedSize,
                         bool allowSmallerOutput) {
  out.clear();
  if (!src || src_len == 0 || expectedSize == 0 || expectedSize > 64u * 1024u * 1024u)
    return false;

  typedef int (*UclDecSafeFn)(const ucl_bytep, ucl_uint, ucl_bytep, ucl_uintp,
                              ucl_voidp);
  // ONLY safe decompressors with strict boundary checking to prevent buffer overrun
  const UclDecSafeFn candidates[] = {
      ucl_nrv2b_decompress_safe_8,
      ucl_nrv2d_decompress_safe_8,
      ucl_nrv2e_decompress_safe_8,
  };


  const unsigned int prefixSkips[] = {0u, 4u, 8u, 12u, 16u};
  for (UclDecSafeFn fn : candidates) {
    for (unsigned int skip : prefixSkips) {
      if (skip >= src_len)
        continue;
      try {
        out.resize(expectedSize);
      } catch (...) {
        out.clear();
        return false;
      }
      ucl_uint dst_len = expectedSize;
      int r = SafeUclCall(fn, src + skip, src_len - skip, out.data(), &dst_len);
      if (r != UCL_E_OK)
        continue;
      if (dst_len == expectedSize) {
        return true;
      }
      if (allowSmallerOutput && dst_len > 0 && dst_len <= expectedSize) {
        try {
          out.resize((size_t)dst_len);
        } catch (...) {
          out.clear();
          return false;
        }
        return true;
      }
    }
  }
  out.clear();
  return false;
}


bool IsSprHeaderSignature(const unsigned char *p) {
  if (!p)
    return false;
  return ((*(const int *)&p[0] == SPR_COMMENT_FLAG) ||
          (std::memcmp(p, ".SPR", 4) == 0) ||
          (std::memcmp(p, "SPR\0", 4) == 0));
}

bool IsLikelySprBufferAt(const unsigned char *data, size_t size) {
  if (!data || size < sizeof(SPRHEAD) || !IsSprHeaderSignature(data))
    return false;

  const SPRHEAD *head = reinterpret_cast<const SPRHEAD *>(data);
  if (head->Frames == 0 || head->Frames > 20000u)
    return false;
  return true;
}

int GetSprFrameCountFromBytes(const std::vector<unsigned char> &bytes) {
  if (bytes.size() < sizeof(SPRHEAD))
    return 0;
  const SPRHEAD *head = reinterpret_cast<const SPRHEAD *>(bytes.data());
  if (!IsSprHeaderSignature(bytes.data()))
    return 0;
  if (head->Frames == 0 || head->Frames > 20000u)
    return 0;
  return (int)head->Frames;
}

bool DecompressFrameSprToUncompressedSpr(const unsigned char *data, size_t size,
                                         std::vector<unsigned char> &out) {
  out.clear();
  if (IsLikelySprBufferAt(data, size)) {
    out.assign(data, data + size);
    return true;
  }
  if (size >= 8 && IsLikelySprBufferAt(data + 8, size - 8)) {
    out.assign(data + 8, data + size);
    return true;
  }
  {
    std::vector<unsigned char> fragDecoded;
    if (DecompressFragmentedSprBuffer(data, size, fragDecoded)) {
      const unsigned char *sprOut = nullptr;
      size_t sprOutSize = 0;
      if (ResolveSprHeaderBuffer(fragDecoded.data(), fragDecoded.size(), sprOut,
                                 sprOutSize)) {
        out.assign(sprOut, sprOut + sprOutSize);
        return true;
      }
    }
  }

  if (TryDecompressClassicFrameSprPayload(data, size, out))
    return true;
  if (size >= 8 &&
      TryDecompressClassicFrameSprPayload(data + 8, size - 8, out))
    return true;

  const unsigned char *sprData = nullptr;
  size_t sprSize = 0;
  if (!ResolveSprHeaderBuffer(data, size, sprData, sprSize))
    return false;
  return TryDecompressClassicFrameSprPayload(sprData, sprSize, out);
}

HBITMAP CreateBitmapFromSprBytes(const unsigned char *data, size_t size,
                                 int frameIndex, std::string &outInfo,
                                 bool cleanupDarkMatte) {
  outInfo.clear();
  if (!data || size < sizeof(SPRHEAD))
    return nullptr;

  std::vector<unsigned char> sprDecomp;
  const unsigned char *renderData = data;
  size_t renderSize = size;
  if (DecompressFrameSprToUncompressedSpr(data, size, sprDecomp) &&
      !sprDecomp.empty()) {
    renderData = sprDecomp.data();
    renderSize = sprDecomp.size();
  }

  JxSprFrameArgb frame;
  if (!DecodeSprFrameInternal(renderData, renderSize, frameIndex, true, frame,
                              true, cleanupDarkMatte)) {
    return nullptr;
  }

  BITMAPINFO bmi{};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = frame.w;
  bmi.bmiHeader.biHeight = -frame.h;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  void *bits = nullptr;
  HDC hdc = GetDC(nullptr);
  HBITMAP hbmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  ReleaseDC(nullptr, hdc);
  if (!hbmp || !bits) {
    if (hbmp)
      DeleteObject(hbmp);
    return nullptr;
  }

  std::memcpy(bits, frame.pixels.data(), frame.pixels.size() * sizeof(DWORD));

  char buf[256] = {};
  std::snprintf(buf, sizeof(buf), "SPR %dx%d (Frame=%dx%d, Frames=%d, Colors=%d)",
                frame.w, frame.h, frame.frameW, frame.frameH, frame.frames, frame.colors);
  outInfo = buf;
  return hbmp;
}
