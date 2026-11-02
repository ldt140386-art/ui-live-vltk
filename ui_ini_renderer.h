#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <set>

// Parsed widget from a JX1 UI INI section
struct UiIniWidget {
  std::wstring sectionName;   // e.g. "Main", "Login", "Cancel"
  int left = 0, top = 0;
  int width = 0, height = 0;
  int imageWidth = 0, imageHeight = 0;
  std::wstring imagePath;     // Virtual path: \Spr\Ui4\...\file.spr
  int frameUp = 0;            // SPR frame index for "Up" state
  int frameDown = -1;
  int frameOver = -1;
  int overFrame = 0;
  bool isCheckbox = false;
  bool isTransparent = false;
  std::wstring text;          // Static text content (Text, Label, Caption, Title, etc.)
  std::wstring tip;           // Tooltip text
  COLORREF textColor = RGB(255, 226, 70); // Warm gold #FFE246
  COLORREF borderColor = CLR_INVALID;
  COLORREF shadowColor = RGB(10, 10, 10);
  COLORREF overColor = CLR_INVALID;
  COLORREF selectColor = CLR_INVALID;
  int labelXOffset = 0;
  int labelYOffset = 0;
  bool hasShadow = false;
  int fontSize = 12;
  int hAlign = 0;             // 0=left, 1=center, 2=right
  int vAlign = 0;             // 0=top, 1=center, 2=bottom
  bool isMain = false;        // True for [Main] section
  bool hasImage = false;
  bool isSelected = false;
};

// Complete parsed UI layout
struct UiIniLayout {
  std::vector<UiIniWidget> widgets;
  int mainWidth = 0;
  int mainHeight = 0;
  bool valid = false;
};

// Parse a JX1 UI INI text into structured layout
bool ParseUiIniLayout(const std::vector<unsigned char> &iniBytes, UiIniLayout &outLayout);

// Check if raw bytes look like a JX1 UI INI (has [Main] + Image= keys)
bool LooksLikeUiIniContent(const std::vector<unsigned char> &bytes);

// Render the composite UI as a single HBITMAP
// Resolves Image= paths across all known PAK files
HBITMAP RenderUiComposite(const UiIniLayout &layout,
                          const std::set<std::wstring> &knownPaks,
                          std::wstring &outInfo);

// Decode an entire UI INI file line-by-line: Text=/Tip= values in Vietnamese, paths in GBK
std::wstring DecodeIniTextWithVietnamese(const std::vector<unsigned char> &data);

// Decode raw single-byte string using JX1 VLMP Vietnamese character table
std::wstring DecodeVnSingleByteString(const unsigned char *data, size_t len);
