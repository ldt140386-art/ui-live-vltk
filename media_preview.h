#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <vector>

bool LoadWicBitmapFromBytes(const std::vector<unsigned char> &data,
                           HBITMAP &outBitmap, std::wstring &outInfo);

bool LoadTgaBitmapFromBytes(const std::vector<unsigned char> &data,
                           HBITMAP &outBitmap, std::wstring &outInfo);

void PreviewPlayMedia(const unsigned char *data, size_t size, const char *tag,
                      bool loop = false, const char *extHint = nullptr);

void StopPreviewMedia();

bool LooksLikeTextBytes(const std::vector<unsigned char> &data);
std::wstring DecodeTextBytesForPreview(const std::vector<unsigned char> &data);
std::wstring HexDumpPreview(const std::vector<unsigned char> &data);
