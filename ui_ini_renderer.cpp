#include "ui_ini_renderer.h"
#include "pak_engine.h"
#include "spr_decoder.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <map>

// ---------------------------------------------------------------
// Complete JX1 VLMP Vietnamese Encoding Table
// Empirically verified against 1024.pak and game strings
// ---------------------------------------------------------------
static const wchar_t kJx1VnTable[128] = {
  // 0x80 - 0x8F (Uppercase letters with diacritics / Latin-1 passthrough)
  0x00C1, // 0x80 = Á
  0x00C0, // 0x81 = À
  0x1EA2, // 0x82 = Ả
  0x00C3, // 0x83 = Ã
  0x1EA0, // 0x84 = Ạ
  0x0102, // 0x85 = Ă
  0x1EAE, // 0x86 = Ắ
  0x1EB0, // 0x87 = Ằ
  0x1EB2, // 0x88 = Ẳ
  0x1EB4, // 0x89 = Ẵ
  0x1EB6, // 0x8A = Ặ
  0x00C2, // 0x8B = Â
  0x1EA4, // 0x8C = Ấ
  0x1EA6, // 0x8D = Ầ
  0x1EA8, // 0x8E = Ẩ
  0x1EAA, // 0x8F = Ẫ
  // 0x90 - 0x9F
  0x1EAC, // 0x90 = Ậ
  0x00C9, // 0x91 = É
  0x00C8, // 0x92 = È
  0x1EBA, // 0x93 = Ẻ
  0x1EBC, // 0x94 = Ẽ
  0x1EB8, // 0x95 = Ẹ
  0x00CA, // 0x96 = Ê
  0x1EBE, // 0x97 = Ế
  0x1EC0, // 0x98 = Ề
  0x1EC2, // 0x99 = Ể
  0x1EC4, // 0x9A = Ễ
  0x1EC6, // 0x9B = Ệ
  0x00CD, // 0x9C = Í
  0x00CC, // 0x9D = Ì
  0x1EC8, // 0x9E = Ỉ
  0x0128, // 0x9F = Ĩ
  // 0xA0 - 0xAF
  0x1ECA, // 0xA0 = Ị
  0x00A1, // 0xA1 = ¡
  0x00A2, // 0xA2 = ¢
  0x00A3, // 0xA3 = £
  0x00A4, // 0xA4 = ¤
  0x00A5, // 0xA5 = ¥
  0x00A6, // 0xA6 = ¦
  0x0110, // 0xA7 = Đ (confirmed "Đăng", "Đồng", "Đổi")
  0x0103, // 0xA8 = ă (confirmed "Đăng nhập", "năng")
  0x00E2, // 0xA9 = â (confirmed "Trân", "Nhân", "Thân")
  0x00EA, // 0xAA = ê (confirmed "Tên", "Thêm", "tiêu")
  0x00F4, // 0xAB = ô (confirmed "nuôi", "Thông")
  0x01A1, // 0xAC = ơ
  0x01B0, // 0xAD = ư (confirmed "được", "thưởng", "lượng")
  0x0111, // 0xAE = đ (confirmed "được", "điểm", "hoạt động")
  0x00AF, // 0xAF = ¯
  // 0xB0 - 0xBF
  0x00B0, // 0xB0 = °
  0x00B1, // 0xB1 = ±
  0x00B2, // 0xB2 = ²
  0x00B3, // 0xB3 = ³
  0x00B4, // 0xB4 = ´
  0x00E0, // 0xB5 = à (confirmed "Hàng", "tài", "ngày", "bàn", "quà")
  0x1EA3, // 0xB6 = ả (confirmed "khoản", "phải", "ảo", "Sảnh")
  0x00E3, // 0xB7 = ã (confirmed "Mã")
  0x00E1, // 0xB8 = á (confirmed "bán", "Giá", "Các", "trái", "xác")
  0x1EA1, // 0xB9 = ạ (confirmed "hoạt", "Lại", "bạn", "trạng", "nạm")
  0x1EA7, // 0xBA = ầ
  0x1EB1, // 0xBB = ằ (confirmed "Bằng Hữu")
  0x1EB3, // 0xBC = ẳ
  0x1EB5, // 0xBD = ẵ
  0x1EAF, // 0xBE = ắ (confirmed "Bắt đầu")
  0x1EB7, // 0xBF = ặ
  // 0xC0 - 0xCF
  0x00E8, // 0xC0 = è
  0x1EBA, // 0xC1 = ẻ
  0x1EBC, // 0xC2 = ẽ
  0x00E9, // 0xC3 = é
  0x1EB9, // 0xC4 = ẹ
  0x1EC1, // 0xC5 = ề
  0x1EB7, // 0xC6 = ặ (confirmed "đặt", "tặng")
  0x1EA7, // 0xC7 = ầ (confirmed "cần", "đầu", "phần")
  0x1EA9, // 0xC8 = ẩ (confirmed "ẩn", "phẩm")
  0x1EAB, // 0xC9 = ẫ
  0x1EA5, // 0xCA = ấ (confirmed "nhất", "cấp")
  0x1EAD, // 0xCB = ậ (confirmed "nhập", "nhận", "Mật", "Vật", "Luận")
  0x00EC, // 0xCC = ì
  0x1EC9, // 0xCD = ỉ
  0x1EBB, // 0xCE = ẻ (confirmed "Thẻ")
  0x00ED, // 0xCF = í
  // 0xD0 - 0xDF
  0x1ECB, // 0xD0 = ị
  0x00F2, // 0xD1 = ò
  0x1EC1, // 0xD2 = ề (confirmed "Tiền")
  0x1EC3, // 0xD3 = ể (confirmed "thể", "điểm")
  0x1EC5, // 0xD4 = ễ
  0x1EBF, // 0xD5 = ế (confirmed "Chiến", "Phiếu", "Kiếm", "Thiết", "Tiết")
  0x1EC7, // 0xD6 = ệ (confirmed "nghệ", "Hiệp", "kiệm", "hiện", "Kiện")
  0x1ECF, // 0xD7 = ỏ
  0x00F5, // 0xD8 = õ
  0x00F3, // 0xD9 = ó
  0x1ECD, // 0xDA = ọ
  0x00F4, // 0xDB = ô
  0x1ED3, // 0xDC = ồ
  0x00ED, // 0xDD = í (confirmed "ít", "phím", "Tích")
  0x1ECB, // 0xDE = ị (confirmed "Định Giá")
  0x1EDF, // 0xDF = ở
  // 0xE0 - 0xEF
  0x1EE1, // 0xE0 = ỡ
  0x1ECF, // 0xE1 = ỏ (confirmed "bỏ", "nhỏ", "Giỏ")
  0x00F5, // 0xE2 = õ (confirmed "Võ nghệ")
  0x00F3, // 0xE3 = ó (confirmed "có", "Đóng", "khóa")
  0x1ECD, // 0xE4 = ọ (confirmed "Chọn")
  0x1ED3, // 0xE5 = ồ (confirmed "Đồng ý")
  0x1ED5, // 0xE6 = ổ (confirmed "Đổi", "Tổng")
  0x1ED7, // 0xE7 = ỗ (confirmed "Mỗi")
  0x1ED1, // 0xE8 = ố (confirmed "số", "muốn")
  0x1ED9, // 0xE9 = ộ (confirmed "chuột", "động")
  0x1EDD, // 0xEA = ờ (confirmed "Thời", "Lời")
  0x1EDF, // 0xEB = ở (confirmed "mở", "thưởng")
  0x1EE1, // 0xEC = ỡ
  0x1EDB, // 0xED = ớ (confirmed "mới", "nhớ")
  0x1EE3, // 0xEE = ợ (confirmed "được", "lượng")
  0x00F9, // 0xEF = ù (confirmed "dùng")
  // 0xF0 - 0xFF
  0x1EE7, // 0xF0 = ủ
  0x1EE7, // 0xF1 = ủ (confirmed "Hủy")
  0x0169, // 0xF2 = ũ (confirmed "lũy", "Mã cũ")
  0x00FA, // 0xF3 = ú (confirmed "thú nuôi")
  0x1EE5, // 0xF4 = ụ (confirmed "dụng")
  0x1EEB, // 0xF5 = ừ (confirmed "từ bàn phím", "Cứu/Cừu")
  0x1EED, // 0xF6 = ử (confirmed "Sửa đổi", "Sử dụng")
  0x1EEF, // 0xF7 = ữ (confirmed "chữ số", "Hữu")
  0x1EE9, // 0xF8 = ứ (confirmed "Tin tức")
  0x1EF1, // 0xF9 = ự (confirmed "Sự kiện", "trực", "ký tự", "Tinh lực")
  0x1EF3, // 0xFA = ỳ (confirmed "Kỳ Trân")
  0x1EF7, // 0xFB = ỷ
  0x1EF9, // 0xFC = ỹ (confirmed "Kỹ năng")
  0x00FD, // 0xFD = ý (confirmed "Đồng ý", "ký tự")
  0x1EF5, // 0xFE = ỵ
  0x00FF  // 0xFF = ÿ
};

// Decode a byte string using JX1 VLMP Vietnamese encoding:
// ASCII pass-through for bytes < 0x80, table lookup for 0x80+
static std::wstring DecodeVnText(const unsigned char *data, size_t len) {
  std::wstring out;
  out.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    unsigned char b = data[i];
    if (b < 0x80) {
      out += (wchar_t)b;
    } else {
      out += kJx1VnTable[b - 0x80];
    }
  }
  return out;
}

std::wstring DecodeVnSingleByteString(const unsigned char *data, size_t len) {
  if (!data || len == 0) return {};
  return DecodeVnText(data, len);
}

// For Text/Tip/Label values: always use JX1 VN encoding (single-byte Vietnamese)
static std::wstring DecodeVietnameseText(const unsigned char *data, size_t len) {
  if (!data || len == 0) return {};
  return DecodeVnText(data, len);
}

// Decode bytes primarily for structure (section names, paths) using GBK
static std::wstring DecodeGbkBytes(const unsigned char *data, size_t len) {
  if (!data || len == 0) return {};
  int wlen = MultiByteToWideChar(936, 0, (const char *)data, (int)len, nullptr, 0);
  if (wlen > 0) {
    std::wstring w(wlen, L'\0');
    MultiByteToWideChar(936, 0, (const char *)data, (int)len, &w[0], wlen);
    return w;
  }
  // Fallback: copy as-is
  std::wstring out;
  for (size_t i = 0; i < len; ++i) out += (wchar_t)data[i];
  return out;
}

// Line-by-line smart decoder for full UI INI preview:
// Decodes Text=/Tip=/Label= values with Vietnamese table, and section headers/paths with GBK
std::wstring DecodeIniTextWithVietnamese(const std::vector<unsigned char> &data) {
  if (data.empty()) return {};

  const unsigned char *p = data.data();
  size_t size = data.size();

  if (size >= 3 && p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF) {
    p += 3;
    size -= 3;
  }

  std::wstring result;
  result.reserve(size + 128);

  size_t lineStart = 0;
  for (size_t i = 0; i <= size; ++i) {
    if (i == size || p[i] == '\n') {
      size_t lineLen = i - lineStart;
      bool hasCr = false;
      if (lineLen > 0 && p[lineStart + lineLen - 1] == '\r') {
        lineLen--;
        hasCr = true;
      }

      if (lineLen > 0) {
        const unsigned char *line = p + lineStart;

        size_t kStart = 0;
        while (kStart < lineLen && (line[kStart] == ' ' || line[kStart] == '\t')) kStart++;

        if (kStart < lineLen && line[kStart] != '[' && line[kStart] != ';') {
          size_t eqPos = kStart;
          while (eqPos < lineLen && line[eqPos] != '=') eqPos++;

          if (eqPos < lineLen) {
            std::string key((const char *)line + kStart, eqPos - kStart);
            while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();

            std::string lowerKey = key;
            for (char &c : lowerKey) c = (char)tolower((unsigned char)c);

            bool isVn = (lowerKey == "text" || lowerKey == "tip" || lowerKey == "label" ||
                         lowerKey == "caption" || lowerKey == "title" || lowerKey == "string" ||
                         lowerKey == "name" || lowerKey == "desc" || lowerKey == "msg");

            if (isVn) {
              std::wstring prefix = DecodeGbkBytes(line, eqPos + 1);
              std::wstring val = DecodeVnText(line + eqPos + 1, lineLen - (eqPos + 1));
              result += prefix + val;
            } else {
              result += DecodeGbkBytes(line, lineLen);
            }
          } else {
            result += DecodeGbkBytes(line, lineLen);
          }
        } else {
          result += DecodeGbkBytes(line, lineLen);
        }
      }

      if (hasCr) result += L"\r";
      if (i < size) result += L"\n";
      lineStart = i + 1;
    }
  }

  return result;
}


// ---------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------
static std::wstring TrimW(const std::wstring &s) {
  size_t start = s.find_first_not_of(L" \t\r\n");
  if (start == std::wstring::npos) return {};
  size_t end = s.find_last_not_of(L" \t\r\n");
  return s.substr(start, end - start + 1);
}

static int ParseIntW(const std::wstring &s, int def = 0) {
  if (s.empty()) return def;
  try { return std::stoi(s); }
  catch (...) { return def; }
}

static COLORREF ParseColorW(const std::wstring &s, COLORREF def = RGB(255, 255, 255)) {
  int r = 255, g = 255, b = 255;
  if (swscanf_s(s.c_str(), L"%d,%d,%d", &r, &g, &b) == 3) {
    return RGB(r, g, b);
  }
  return def;
}

static std::string TrimA(const std::string &s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return {};
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

// ---------------------------------------------------------------
// Byte-level INI Parser with mixed encoding support
// Parses structure (sections, keys) with GBK for Chinese paths
// but Text= and Tip= values with TCVN3 for Vietnamese
// ---------------------------------------------------------------
struct RawIniLine {
  std::string section;  // current section name (raw bytes)
  std::string key;      // key name (raw bytes)
  std::vector<unsigned char> valueBytes; // value (raw bytes)
};

bool LooksLikeUiIniContent(const std::vector<unsigned char> &bytes) {
  if (bytes.size() < 20 || bytes.size() > 256 * 1024) return false;
  bool hasMain = false;
  bool hasImage = false;
  const char *data = reinterpret_cast<const char *>(bytes.data());
  size_t len = bytes.size();
  for (size_t i = 0; i + 5 < len; ++i) {
    if (data[i] == '[' && (i + 5 < len) &&
        (data[i+1] == 'M' || data[i+1] == 'm') &&
        (data[i+2] == 'a' || data[i+2] == 'A') &&
        (data[i+3] == 'i' || data[i+3] == 'I') &&
        (data[i+4] == 'n' || data[i+4] == 'N') &&
        data[i+5] == ']') {
      hasMain = true;
    }
    if (i + 6 < len &&
        (data[i] == 'I' || data[i] == 'i') &&
        (data[i+1] == 'm' || data[i+1] == 'M') &&
        (data[i+2] == 'a' || data[i+2] == 'A') &&
        (data[i+3] == 'g' || data[i+3] == 'G') &&
        (data[i+4] == 'e' || data[i+4] == 'E') &&
        data[i+5] == '=') {
      hasImage = true;
    }
    if (hasMain && hasImage) return true;
  }
  return false;
}

bool ParseUiIniLayout(const std::vector<unsigned char> &iniBytes, UiIniLayout &outLayout) {
  outLayout = UiIniLayout{};
  if (iniBytes.empty()) return false;

  // Parse at byte level: split into lines
  const unsigned char *data = iniBytes.data();
  size_t size = iniBytes.size();

  // Skip BOM
  size_t start = 0;
  if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) start = 3;

  // Split into byte-level lines
  struct ByteLine {
    size_t start;
    size_t len;
  };
  std::vector<ByteLine> lines;
  size_t lineStart = start;
  for (size_t i = start; i < size; ++i) {
    if (data[i] == '\n') {
      size_t lineLen = i - lineStart;
      if (lineLen > 0 && data[lineStart + lineLen - 1] == '\r') lineLen--;
      lines.push_back({lineStart, lineLen});
      lineStart = i + 1;
    }
  }
  if (lineStart < size) {
    size_t lineLen = size - lineStart;
    if (lineLen > 0 && data[lineStart + lineLen - 1] == '\r') lineLen--;
    lines.push_back({lineStart, lineLen});
  }

  // Parse sections and key-value pairs at byte level
  std::string currentSection;
  // Map: section → key → raw value bytes
  std::map<std::string, std::vector<std::pair<std::string, std::vector<unsigned char>>>> sectionData;
  std::vector<std::string> sectionOrder;

  for (const auto &bl : lines) {
    if (bl.len == 0) continue;
    const unsigned char *lineData = data + bl.start;
    size_t lineLen = bl.len;

    // Skip whitespace
    size_t p = 0;
    while (p < lineLen && (lineData[p] == ' ' || lineData[p] == '\t')) p++;
    if (p >= lineLen || lineData[p] == ';') continue;

    // Section header?
    if (lineData[p] == '[') {
      p++;
      size_t secStart = p;
      while (p < lineLen && lineData[p] != ']') p++;
      if (p > secStart) {
        currentSection.assign((const char *)lineData + secStart, p - secStart);
        if (sectionData.find(currentSection) == sectionData.end()) {
          sectionOrder.push_back(currentSection);
        }
      }
      continue;
    }

    // Key=Value
    if (!currentSection.empty()) {
      size_t eqPos = 0;
      bool foundEq = false;
      for (size_t j = p; j < lineLen; ++j) {
        if (lineData[j] == '=') { eqPos = j; foundEq = true; break; }
      }
      if (foundEq) {
        std::string key((const char *)lineData + p, eqPos - p);
        // Trim key
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();

        // Value starts after '='
        size_t valStart = eqPos + 1;
        while (valStart < lineLen && (lineData[valStart] == ' ' || lineData[valStart] == '\t')) valStart++;

        // Find inline comment (semicolon not inside value)
        size_t valEnd = lineLen;
        for (size_t j = valStart; j < lineLen; ++j) {
          if (lineData[j] == ';') { valEnd = j; break; }
        }
        // Trim trailing whitespace
        while (valEnd > valStart && (lineData[valEnd - 1] == ' ' || lineData[valEnd - 1] == '\t' ||
                                      lineData[valEnd - 1] == '\r')) valEnd--;

        std::vector<unsigned char> valBytes(lineData + valStart, lineData + valEnd);
        sectionData[currentSection].push_back({key, valBytes});
      }
    }
  }

  if (sectionData.empty()) return false;

  // Helper: check if key is a "text" key needing Vietnamese decoding
  auto isVietnameseKey = [](const std::string &key) -> bool {
    std::string lowerKey = key;
    for (char &c : lowerKey) c = (char)tolower((unsigned char)c);
    return lowerKey == "text" || lowerKey == "tip" || lowerKey == "label" ||
           lowerKey == "caption" || lowerKey == "title" || lowerKey == "string" ||
           lowerKey == "name" || lowerKey == "desc" || lowerKey == "msg";
  };

  // Build widgets from parsed data
  for (const auto &secName : sectionOrder) {
    const auto &kvPairs = sectionData[secName];
    UiIniWidget w;
    w.sectionName = DecodeGbkBytes((const unsigned char *)secName.data(), secName.size());

    // Helper to get decoded value
    auto getVal = [&](const std::string &keyName) -> std::wstring {
      for (const auto &kv : kvPairs) {
        if (_stricmp(kv.first.c_str(), keyName.c_str()) == 0) {
          // For text/tip keys: use Vietnamese decoding
          if (isVietnameseKey(keyName)) {
            return DecodeVietnameseText(kv.second.data(), kv.second.size());
          }
          // For Image paths: use GBK (Chinese chars in paths)
          return DecodeGbkBytes(kv.second.data(), kv.second.size());
        }
      }
      return L"";
    };

    std::wstring leftStr = getVal("Left");
    if (leftStr.empty()) leftStr = getVal("PosX");
    w.left = ParseIntW(leftStr);

    std::wstring topStr = getVal("Top");
    if (topStr.empty()) topStr = getVal("PosY");
    w.top = ParseIntW(topStr);

    w.width = ParseIntW(getVal("Width"));
    w.height = ParseIntW(getVal("Height"));
    w.imagePath = getVal("Image");
    if (w.imagePath.empty()) w.imagePath = getVal("StatusImage");
    if (w.imagePath.empty()) w.imagePath = getVal("BgImage");
    if (w.imagePath.empty()) w.imagePath = getVal("BorderImage");
    w.hasImage = !w.imagePath.empty();

    std::wstring statusImg = getVal("StatusImage");
    if (!statusImg.empty()) {
      w.isCheckbox = true;
      w.imageWidth = ParseIntW(getVal("ImageWidth"), 14);
      w.imageHeight = ParseIntW(getVal("ImageHeight"), 14);
      w.frameUp = ParseIntW(getVal("StatusEnableFrame"), 0);
    } else {
      w.isCheckbox = (getVal("CheckBox") == L"1");
    }

    w.frameUp = ParseIntW(getVal("Up"), w.frameUp);
    std::wstring downStr = getVal("Down");
    w.frameDown = downStr.empty() ? -1 : ParseIntW(downStr, -1);
    std::wstring overStr = getVal("Over");
    w.frameOver = overStr.empty() ? -1 : ParseIntW(overStr, -1);
    w.overFrame = ParseIntW(getVal("OverFrame"));
    w.isTransparent = (getVal("Trans") == L"1");
    w.text = getVal("Text");
    if (w.text.empty()) w.text = getVal("Label");
    if (w.text.empty()) w.text = getVal("Caption");
    if (w.text.empty()) w.text = getVal("Title");
    if (w.text.empty()) w.text = getVal("String");
    if (w.text.empty()) w.text = getVal("Name");
    w.tip = getVal("Tip");

    std::wstring colorStr = getVal("Color");
    if (colorStr.empty()) colorStr = getVal("MsgColor");
    if (colorStr.empty()) colorStr = getVal("SelColor");
    if (colorStr.empty()) colorStr = getVal("HighLightColor");
    if (!colorStr.empty()) {
      COLORREF parsed = ParseColorW(colorStr);
      if (parsed == RGB(255, 255, 0) || parsed == RGB(255, 255, 255)) {
        w.textColor = RGB(255, 226, 70);
      } else {
        w.textColor = parsed;
      }
    } else {
      w.textColor = RGB(255, 226, 70);
    }

    std::wstring borderStr = getVal("BorderColor");
    if (borderStr.empty()) borderStr = getVal("MsgBorderColor");
    if (!borderStr.empty()) w.borderColor = ParseColorW(borderStr);

    std::wstring selColorStr = getVal("SelectColor");
    if (!selColorStr.empty()) w.selectColor = ParseColorW(selColorStr);

    std::wstring overColorStr = getVal("OverColor");
    if (!overColorStr.empty()) w.overColor = ParseColorW(overColorStr);

    w.labelXOffset = ParseIntW(getVal("LabelXOffset"), 0);
    w.labelYOffset = ParseIntW(getVal("LabelYOffset"), 0);
    w.hAlign = ParseIntW(getVal("HAlign"), 0);
    w.vAlign = ParseIntW(getVal("VAlign"), 0);

    std::wstring shadowStr = getVal("TextLineShadow");
    if (!shadowStr.empty()) {
      w.shadowColor = ParseColorW(shadowStr, RGB(10, 10, 10));
    }
    w.hasShadow = false;

    std::wstring fontStr = getVal("Font");
    if (!fontStr.empty()) {
      w.fontSize = ParseIntW(fontStr, 12);
    } else {
      w.fontSize = 12;
    }

    // Detect [Main]
    std::string secLower = secName;
    for (char &c : secLower) c = tolower((unsigned char)c);
    w.isMain = (secLower == "main");

    if (w.isMain) {
      outLayout.mainWidth = w.width;
      outLayout.mainHeight = w.height;
    }

    outLayout.widgets.push_back(w);
  }

  outLayout.valid = !outLayout.widgets.empty() && outLayout.mainWidth > 0 && outLayout.mainHeight > 0;
  return outLayout.valid;
}

// ---------------------------------------------------------------
// Composite Renderer with SPR Bitmap Caching
// ---------------------------------------------------------------
HBITMAP RenderUiComposite(const UiIniLayout &layout,
                          const std::set<std::wstring> &knownPaks,
                          std::wstring &outInfo) {
  outInfo.clear();
  if (!layout.valid || layout.mainWidth <= 0 || layout.mainHeight <= 0)
    return nullptr;

  int canvasW = layout.mainWidth;
  int canvasH = layout.mainHeight;
  if (canvasW > 4096) canvasW = 4096;
  if (canvasH > 4096) canvasH = 4096;

  // Filter overlapping alternate widgets at the exact same position (e.g. alternate tabs)
  std::vector<UiIniWidget> activeWidgets;
  std::set<std::tuple<int, int, int, int>> seenBounds;
  for (const auto &w : layout.widgets) {
    if (w.isMain) {
      activeWidgets.push_back(w);
      continue;
    }
    if (w.width > 0 && w.height > 0) {
      auto boundKey = std::make_tuple(w.left, w.top, w.width, w.height);
      if (seenBounds.count(boundKey)) {
        // Skip duplicate overlapping alternate tab (e.g. BtnTitlePage overlapping BtnJudgePage at 142,29)
        continue;
      }
      seenBounds.insert(boundKey);
    }
    activeWidgets.push_back(w);
  }

  // === SPR Bitmap Cache: resolve each unique path only once ===
  struct CachedSpr {
    HBITMAP hBmp = nullptr;
    int w = 0, h = 0;
    bool valid = false;
  };
  std::map<std::wstring, CachedSpr> sprCache;

  // Pre-resolve all unique Image paths
  for (const auto &w : activeWidgets) {
    if (!w.hasImage) continue;
    std::wstring pathLower = w.imagePath;
    for (wchar_t &c : pathLower) c = towlower(c);
    if (sprCache.count(pathLower)) continue;

    CachedSpr cs;
    std::vector<unsigned char> sprBytes;
    std::wstring pathHint, sourceInfo;
    std::string ext;
    if (TryReadVirtualPathAcrossKnownPakRoots(knownPaks, w.imagePath, sprBytes, pathHint, ext, sourceInfo)) {
      std::string frameInfo;
      cs.hBmp = CreateBitmapFromSprBytes(sprBytes.data(), sprBytes.size(), w.frameUp, frameInfo, false);
      if (cs.hBmp) {
        BITMAP bm{};
        GetObjectW(cs.hBmp, sizeof(bm), &bm);
        cs.w = bm.bmWidth;
        cs.h = bm.bmHeight < 0 ? -bm.bmHeight : bm.bmHeight;
        cs.valid = true;
      }
    }
    sprCache[pathLower] = cs;
  }

  // Create canvas
  HDC screenDC = GetDC(nullptr);
  HDC memDC = CreateCompatibleDC(screenDC);

  BITMAPINFO bmi{};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = canvasW;
  bmi.bmiHeader.biHeight = -canvasH;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  void *canvasBits = nullptr;
  HBITMAP hCanvas = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &canvasBits, nullptr, 0);
  if (!hCanvas || !canvasBits) {
    for (auto &kv : sprCache) { if (kv.second.hBmp) DeleteObject(kv.second.hBmp); }
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
    return nullptr;
  }

  // Fill dark background
  DWORD *pixels = (DWORD *)canvasBits;
  int totalPx = canvasW * canvasH;
  for (int i = 0; i < totalPx; ++i)
    pixels[i] = 0xFF1A1A2E;

  HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hCanvas);

  int sprResolved = 0;
  int sprFailed = 0;

  // Stable sort widgets by visual layer: Main -> Background Panels -> Functional elements -> Buttons
  auto getWidgetLayer = [](const UiIniWidget &w) -> int {
    if (w.isMain) return 0;
    std::wstring s = w.sectionName;
    for (wchar_t &c : s) c = towlower(c);
    if (s.rfind(L"main", 0) == 0) return 0;
    if (s.rfind(L"imgbackground", 0) == 0 || s.rfind(L"bg", 0) == 0) return 1;
    if (s.rfind(L"img", 0) == 0 || s.rfind(L"pic", 0) == 0) return 2;
    if (s.rfind(L"objbox", 0) == 0 || s.rfind(L"itembox", 0) == 0 || s.rfind(L"list", 0) == 0) return 3;
    if (s.rfind(L"scrollbar", 0) == 0) return 4;
    return 5; // buttons, checkboxes, interactive controls
  };

  std::vector<const UiIniWidget*> sprRenderOrder;
  for (const auto &w : activeWidgets) {
    if (w.hasImage) sprRenderOrder.push_back(&w);
  }
  std::stable_sort(sprRenderOrder.begin(), sprRenderOrder.end(),
                   [&](const UiIniWidget* a, const UiIniWidget* b) {
                     return getWidgetLayer(*a) < getWidgetLayer(*b);
                   });

  // Render widgets using cached bitmaps in layer order
  for (const auto *pWidget : sprRenderOrder) {
    const auto &w = *pWidget;

    std::wstring pathLower = w.imagePath;
    for (wchar_t &c : pathLower) c = towlower(c);
    auto it = sprCache.find(pathLower);
    if (it == sprCache.end() || !it->second.valid) {
      sprFailed++;
      continue;
    }

    const CachedSpr &cs = it->second;
    int sprW = cs.w, sprH = cs.h;

    HDC sprDC = CreateCompatibleDC(screenDC);
    HBITMAP oldSpr = (HBITMAP)SelectObject(sprDC, cs.hBmp);

    int destX = w.isMain ? 0 : w.left;
    int destY = w.isMain ? 0 : w.top;
    int destW = (w.width > 0 && !w.isMain) ? w.width : sprW;
    int destH = (w.height > 0 && !w.isMain) ? w.height : sprH;

    // If checkbox with specific icon dimensions, use exact icon size
    if (w.isCheckbox && w.imageWidth > 0 && w.imageHeight > 0) {
      destW = w.imageWidth;
      destH = w.imageHeight;
    }

    if (destX < 0) destX = 0;
    if (destY < 0) destY = 0;
    if (destX + destW > canvasW) destW = canvasW - destX;
    if (destY + destH > canvasH) destH = canvasH - destY;

    if (destW > 0 && destH > 0) {
      BLENDFUNCTION blend{};
      blend.BlendOp = AC_SRC_OVER;
      blend.SourceConstantAlpha = 255;
      blend.AlphaFormat = AC_SRC_ALPHA;
      SetStretchBltMode(memDC, HALFTONE);
      SetBrushOrgEx(memDC, 0, 0, nullptr);
      if (!AlphaBlend(memDC, destX, destY, destW, destH,
                      sprDC, 0, 0, sprW, sprH, blend)) {
        StretchBlt(memDC, destX, destY, destW, destH,
                   sprDC, 0, 0, sprW, sprH, SRCCOPY);
      }
    }

    SelectObject(sprDC, oldSpr);
    DeleteDC(sprDC);
    sprResolved++;
  }

  // Draw text labels (crisp, properly centered, no overlap, no clipping)
  SetBkMode(memDC, TRANSPARENT);
  for (const auto &w : activeWidgets) {
    if (w.isMain) continue;
    std::wstring strToDraw = w.text;
    if (strToDraw.empty() && !w.tip.empty() && !w.hasImage) {
      strToDraw = w.tip;
    }
    if (strToDraw.empty()) continue;

    int drawX = w.left;
    int drawY = w.top;

    // For checkboxes with status icon, offset label to the right of the icon
    if (w.isCheckbox && w.hasImage) {
      drawX = w.left + (w.imageWidth > 0 ? w.imageWidth + 4 : 18);
    }

    int drawW = w.width > 0 ? w.width : (canvasW > drawX ? canvasW - drawX : 300);
    int drawH = w.height > 0 ? w.height : 16;
    RECT textRc{drawX, drawY, drawX + drawW, drawY + drawH};

    int fSize = w.fontSize > 0 ? w.fontSize : 12;
    // Clean, natural typography: Segoe UI with FW_NORMAL (400)
    HFONT hFont = CreateFontW(-fSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(memDC, hFont);

    UINT uFormat = DT_NOPREFIX | DT_NOCLIP;
    if (w.hAlign == 1) uFormat |= DT_CENTER;
    else if (w.hAlign == 2) uFormat |= DT_RIGHT;
    else uFormat |= DT_LEFT;

    // Center vertically inside button bounds without clipping
    if (w.height > 0) uFormat |= DT_VCENTER | DT_SINGLELINE;
    else uFormat |= DT_WORDBREAK;

    // Text Color: warm gold RGB(255, 226, 70) (#FFE246) or specified color
    COLORREF fgColor = w.textColor;
    if (fgColor == RGB(255, 255, 0) || fgColor == RGB(255, 255, 255)) {
      fgColor = RGB(255, 226, 70);
    }
    SetTextColor(memDC, fgColor);
    DrawTextW(memDC, strToDraw.c_str(), (int)strToDraw.size(), &textRc, uFormat);

    SelectObject(memDC, oldFont);
    DeleteObject(hFont);
  }







  // Draw input field outlines
  for (const auto &w : layout.widgets) {
    if (w.borderColor == CLR_INVALID || w.width <= 0 || w.height <= 0 || w.isMain) continue;
    HPEN pen = CreatePen(PS_SOLID, 1, w.borderColor);
    HPEN oldPen = (HPEN)SelectObject(memDC, pen);
    HBRUSH oldBr = (HBRUSH)SelectObject(memDC, GetStockObject(NULL_BRUSH));
    Rectangle(memDC, w.left, w.top, w.left + w.width, w.top + w.height);
    SelectObject(memDC, oldBr);
    SelectObject(memDC, oldPen);
    DeleteObject(pen);
  }

  SelectObject(memDC, oldBmp);
  DeleteDC(memDC);
  ReleaseDC(nullptr, screenDC);

  // Cleanup cached SPR bitmaps
  for (auto &kv : sprCache) {
    if (kv.second.hBmp) DeleteObject(kv.second.hBmp);
  }

  wchar_t infoBuf[128] = {};
  swprintf_s(infoBuf, L"UI Composite %dx%d (%d SPR loaded, %d failed)",
             canvasW, canvasH, sprResolved, sprFailed);
  outInfo = infoBuf;

  return hCanvas;
}
