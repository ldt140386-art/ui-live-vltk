#include "media_preview.h"
#include "pak_engine.h"
#include "ui_ini_renderer.h"

#include <wincodec.h>
#include <mmsystem.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Msimg32.lib")

namespace {

static char g_previewMediaTempPath[MAX_PATH] = {};
static bool g_previewMediaMciOpened = false;
static const char *g_previewMciAlias = "FindInProcessMedia";

} // namespace

bool LoadWicBitmapFromBytes(const std::vector<unsigned char> &data,
                            HBITMAP &outBitmap, std::wstring &outInfo) {
  outBitmap = nullptr;
  outInfo.clear();
  if (data.empty())
    return false;

  IWICImagingFactory *factory = nullptr;
  IWICStream *stream = nullptr;
  IWICBitmapDecoder *decoder = nullptr;
  IWICBitmapFrameDecode *frame = nullptr;
  IWICFormatConverter *converter = nullptr;
  HDC hdc = nullptr;

  HRESULT coHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
  if (FAILED(hr))
    return false;

  hr = factory->CreateStream(&stream);
  if (SUCCEEDED(hr))
    hr = stream->InitializeFromMemory((WICInProcPointer)data.data(), (DWORD)data.size());
  if (SUCCEEDED(hr))
    hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
  if (SUCCEEDED(hr))
    hr = decoder->GetFrame(0, &frame);
  if (SUCCEEDED(hr))
    hr = factory->CreateFormatConverter(&converter);
  if (SUCCEEDED(hr))
    hr = converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA,
                               WICBitmapDitherTypeNone, nullptr, 0.0,
                               WICBitmapPaletteTypeCustom);
  UINT w = 0, h = 0;
  if (SUCCEEDED(hr))
    hr = converter->GetSize(&w, &h);

  if (SUCCEEDED(hr) && w > 0 && h > 0) {
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = (LONG)w;
    bmi.bmiHeader.biHeight = -(LONG)h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void *bits = nullptr;
    hdc = GetDC(nullptr);
    outBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (outBitmap && bits) {
      const UINT stride = w * 4;
      const UINT sizeImage = stride * h;
      hr = converter->CopyPixels(nullptr, stride, sizeImage, (BYTE *)bits);
      if (FAILED(hr)) {
        DeleteObject(outBitmap);
        outBitmap = nullptr;
      } else {
        wchar_t info[96] = {};
        swprintf_s(info, L"Image %ux%u", w, h);
        outInfo = info;
      }
    }
  }

  if (hdc) ReleaseDC(nullptr, hdc);
  if (converter) converter->Release();
  if (frame) frame->Release();
  if (decoder) decoder->Release();
  if (stream) stream->Release();
  if (factory) factory->Release();
  if (SUCCEEDED(coHr)) CoUninitialize();
  return outBitmap != nullptr;
}

bool LoadTgaBitmapFromBytes(const std::vector<unsigned char> &data,
                            HBITMAP &outBitmap, std::wstring &outInfo) {
  outBitmap = nullptr;
  outInfo.clear();
  if (data.size() < 18)
    return false;

  const unsigned char *p = data.data();
  unsigned char idLen = p[0];
  unsigned char colorMapType = p[1];
  unsigned char imageType = p[2];
  if (colorMapType != 0 || (imageType != 2 && imageType != 3))
    return false;

  int w = p[12] | (p[13] << 8);
  int h = p[14] | (p[15] << 8);
  int bpp = p[16];
  bool topOrigin = (p[17] & 0x20) != 0;
  if (w <= 0 || h <= 0 || (bpp != 24 && bpp != 32 && bpp != 8))
    return false;

  size_t off = 18u + idLen;
  size_t strideSrc = (size_t)w * (size_t)(bpp / 8);
  if (off + strideSrc * (size_t)h > data.size())
    return false;

  BITMAPINFO bmi{};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = w;
  bmi.bmiHeader.biHeight = -h;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void *bits = nullptr;
  HDC hdc = GetDC(nullptr);
  HBITMAP hbmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  ReleaseDC(nullptr, hdc);
  if (!hbmp || !bits)
    return false;

  unsigned char *dst = (unsigned char *)bits;
  for (int y = 0; y < h; ++y) {
    int srcY = topOrigin ? y : (h - 1 - y);
    const unsigned char *src = p + off + (size_t)srcY * strideSrc;
    unsigned char *row = dst + (size_t)y * (size_t)w * 4u;
    for (int x = 0; x < w; ++x) {
      if (bpp == 8) {
        unsigned char v = src[x];
        row[x * 4 + 0] = v;
        row[x * 4 + 1] = v;
        row[x * 4 + 2] = v;
        row[x * 4 + 3] = 255;
      } else {
        const unsigned char *px = src + (size_t)x * (size_t)(bpp / 8);
        row[x * 4 + 0] = px[0];
        row[x * 4 + 1] = px[1];
        row[x * 4 + 2] = px[2];
        row[x * 4 + 3] = (bpp == 32) ? px[3] : 255;
      }
    }
  }

  wchar_t info[96] = {};
  swprintf_s(info, L"TGA %dx%d (%dbpp)", w, h, bpp);
  outInfo = info;
  outBitmap = hbmp;
  return true;
}

void StopPreviewMedia() {
  PlaySoundA(nullptr, nullptr, SND_PURGE);
  if (g_previewMediaMciOpened) {
    char cmd[128] = {};
    std::snprintf(cmd, sizeof(cmd), "stop %s", g_previewMciAlias);
    mciSendStringA(cmd, nullptr, 0, nullptr);
    std::snprintf(cmd, sizeof(cmd), "close %s", g_previewMciAlias);
    mciSendStringA(cmd, nullptr, 0, nullptr);
    g_previewMediaMciOpened = false;
  }
  if (g_previewMediaTempPath[0]) {
    DeleteFileA(g_previewMediaTempPath);
    g_previewMediaTempPath[0] = '\0';
  }
}

void PreviewPlayMedia(const unsigned char *data, size_t size, const char *tag,
                      bool loop, const char *extHint) {
  StopPreviewMedia();
  if (!data || size == 0)
    return;

  char tempDir[MAX_PATH] = {};
  if (GetTempPathA(MAX_PATH, tempDir) == 0)
    return;

  std::string ext = ".wav";
  if (extHint && extHint[0])
    ext = extHint;
  else if (tag && _stricmp(tag, "MP3") == 0)
    ext = ".mp3";
  else if (tag && _stricmp(tag, "MID") == 0)
    ext = ".mid";

  char tempFile[MAX_PATH] = {};
  std::snprintf(tempFile, sizeof(tempFile), "%sfindinproc_preview_%lu%s",
                tempDir, GetTickCount(), ext.c_str());

  HANDLE h = CreateFileA(tempFile, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE)
    return;
  DWORD written = 0;
  WriteFile(h, data, (DWORD)size, &written, nullptr);
  CloseHandle(h);

  strncpy_s(g_previewMediaTempPath, tempFile, _TRUNCATE);

  if (ext == ".wav") {
    PlaySoundA(tempFile, nullptr, SND_ASYNC | SND_FILENAME | (loop ? SND_LOOP : 0));
    return;
  }

  char cmd[512] = {};
  std::snprintf(cmd, sizeof(cmd), "open \"%s\" type mpegvideo alias %s",
                tempFile, g_previewMciAlias);
  if (mciSendStringA(cmd, nullptr, 0, nullptr) == 0) {
    g_previewMediaMciOpened = true;
    std::snprintf(cmd, sizeof(cmd), "play %s%s", g_previewMciAlias,
                  loop ? " repeat" : "");
    mciSendStringA(cmd, nullptr, 0, nullptr);
  }
}

bool LooksLikeTextBytes(const std::vector<unsigned char> &data) {
  if (data.empty())
    return true;
  size_t limit = data.size() < 4096 ? data.size() : 4096;
  size_t bad = 0;
  for (size_t i = 0; i < limit; ++i) {
    unsigned char ch = data[i];
    if (ch == 0)
      return false;
    if (ch >= 32 || ch == '\r' || ch == '\n' || ch == '\t' || ch >= 0x80)
      continue;
    ++bad;
  }
  return bad < (limit / 20 + 1);
}

std::wstring DecodeTextBytesForPreview(const std::vector<unsigned char> &data) {
  if (data.empty())
    return L"";

  // If this is a UI INI file, decode with mixed Vietnamese (text/tip) + GBK (paths)
  if (LooksLikeUiIniContent(data)) {
    std::wstring iniVn = DecodeIniTextWithVietnamese(data);
    if (!iniVn.empty())
      return iniVn;
  }

  // Check UTF-8 BOM
  const char *p = (const char *)data.data();
  int size = (int)data.size();
  if (size >= 3 && (unsigned char)p[0] == 0xEF &&
      (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) {
    p += 3;
    size -= 3;
  }

  int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, p, size, nullptr, 0);
  if (wlen > 0) {
    std::wstring out((size_t)wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, p, size, &out[0], wlen);
    return out;
  }

  std::string s(data.begin(), data.end());
  std::wstring gbk = GbkToWide(s);
  if (!gbk.empty())
    return gbk;

  return CodePageToWide(CP_ACP, s);
}


std::wstring HexDumpPreview(const std::vector<unsigned char> &data) {
  std::wstring out;
  size_t n = data.size() < 512 ? data.size() : 512;
  wchar_t line[128] = {};
  for (size_t i = 0; i < n; i += 16) {
    swprintf_s(line, L"%08X  ", (unsigned)i);
    out += line;
    size_t j = 0;
    for (; j < 16 && i + j < n; ++j) {
      swprintf_s(line, L"%02X ", (unsigned)data[i + j]);
      out += line;
    }
    out += L"\r\n";
  }
  if (data.size() > n)
    out += L"...\r\n";
  return out;
}
