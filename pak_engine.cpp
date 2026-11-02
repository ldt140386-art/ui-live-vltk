#include "pak_engine.h"
#include "spr_decoder.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <mutex>
#include <vector>
#include <set>

static std::mutex g_pakCacheMutex;
static std::map<std::wstring, PakIndexCache> g_pakCaches;
static std::map<DWORD, std::unordered_map<unsigned long, std::wstring>> g_runtimePathMaps;

std::wstring ToLowerW(std::wstring text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](wchar_t ch) { return (wchar_t)towlower(ch); });
  return text;
}

std::string ToLowerA(std::string text) {
  for (char &ch : text) {
    if (ch >= 'A' && ch <= 'Z')
      ch = (char)(ch + ('a' - 'A'));
  }
  return text;
}

std::string WideToUtf8(const std::wstring &text) {
  if (text.empty())
    return std::string();
  int len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(),
                                nullptr, 0, nullptr, nullptr);
  if (len <= 0)
    return std::string();
  std::string out((size_t)len, '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &out[0], len,
                      nullptr, nullptr);
  return out;
}

std::wstring Utf8ToWide(const std::string &text) {
  if (text.empty())
    return std::wstring();
  int len = MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(),
                                nullptr, 0);
  if (len <= 0)
    return std::wstring();
  std::wstring out((size_t)len, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), &out[0], len);
  return out;
}

std::wstring CodePageToWide(UINT codePage, const std::string &text) {
  if (text.empty())
    return std::wstring();
  int len = MultiByteToWideChar(codePage, 0, text.data(), (int)text.size(),
                                nullptr, 0);
  if (len <= 0)
    return std::wstring();
  std::wstring out((size_t)len, L'\0');
  MultiByteToWideChar(codePage, 0, text.data(), (int)text.size(), &out[0], len);
  return out;
}

std::string WideToCodePage(UINT codePage, const std::wstring &text) {
  if (text.empty())
    return std::string();
  int len = WideCharToMultiByte(codePage, 0, text.c_str(), (int)text.size(),
                                nullptr, 0, nullptr, nullptr);
  if (len <= 0)
    return std::string();
  std::string out((size_t)len, '\0');
  WideCharToMultiByte(codePage, 0, text.c_str(), (int)text.size(), &out[0], len,
                      nullptr, nullptr);
  return out;
}

std::wstring GbkToWide(const std::string &text) {
  std::wstring out = CodePageToWide(936, text);
  if (!out.empty())
    return out;
  return CodePageToWide(CP_ACP, text);
}

std::string WideToGbk(const std::wstring &text) {
  std::string out = WideToCodePage(936, text);
  if (!out.empty())
    return out;
  return WideToCodePage(CP_ACP, text);
}

bool IsPakPath(const std::wstring &path) {
  if (path.size() < 4)
    return false;
  return _wcsicmp(path.c_str() + path.size() - 4, L".pak") == 0;
}

std::wstring ParentDirOfPath(const std::wstring &path) {
  size_t pos = path.find_last_of(L"\\/");
  if (pos == std::wstring::npos)
    return L"";
  return path.substr(0, pos);
}

std::wstring FileNameOfPath(const std::wstring &path) {
  size_t pos = path.find_last_of(L"\\/");
  if (pos == std::wstring::npos)
    return path;
  return path.substr(pos + 1);
}

std::string GetLowerExtFromPath(const std::wstring &path) {
  size_t dot = path.find_last_of(L'.');
  if (dot == std::wstring::npos)
    return std::string();
  return ToLowerA(WideToUtf8(path.substr(dot)));
}

unsigned int GetPakStoredSize(unsigned int flag) {
  return flag & 0x00FFFFFFu;
}

unsigned int GetPakCompressMethod(unsigned int flag) {
  unsigned int method = flag & 0xFF000000u;
  if ((method & GUI_TYPE_FRAME) != 0)
    return GUI_TYPE_FRAME;
  if ((method & GUI_TYPE_UCL2) != 0)
    return GUI_TYPE_UCL2;
  if ((method & GUI_TYPE_BZIP2) != 0)
    return GUI_TYPE_BZIP2;
  if ((method & GUI_TYPE_UCL) != 0)
    return GUI_TYPE_UCL;
  return GUI_TYPE_NONE;
}

unsigned long FileNameToPackId(const std::string &pathGbk) {
  if (pathGbk.empty())
    return 0;

  char buf[MAX_PATH * 2] = {};
  strncpy_s(buf, pathGbk.c_str(), _TRUNCATE);
  for (char *p = buf; *p; ++p) {
    if (*p == '/')
      *p = '\\';
    if (*p >= 'A' && *p <= 'Z')
      *p = (char)(*p + ('a' - 'A'));
  }

  unsigned long id = 0;
  for (int i = 0; buf[i]; ++i) {
    signed char c = (signed char)buf[i];
    id = (id + (unsigned long)((i + 1) * (int)c)) % 0x8000000bUL * 0xffffffefUL;
  }
  return (id ^ 0x12345678UL);
}

static void TrimAsciiInPlace(std::string &text) {
  size_t first = 0;
  while (first < text.size() &&
         (text[first] == ' ' || text[first] == '\t' || text[first] == '\r' || text[first] == '\n'))
    ++first;
  size_t last = text.size();
  while (last > first &&
         (text[last - 1] == ' ' || text[last - 1] == '\t' || text[last - 1] == '\r' || text[last - 1] == '\n'))
    --last;
  if (first > 0 || last < text.size())
    text = text.substr(first, last - first);
}

static bool ReadWholeFileBinary(const std::wstring &path, std::vector<unsigned char> &outBytes) {
  outBytes.clear();
  HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE)
    return false;
  LARGE_INTEGER li{};
  if (!GetFileSizeEx(h, &li) || li.QuadPart < 0 || li.QuadPart > 128ll * 1024ll * 1024ll) {
    CloseHandle(h);
    return false;
  }
  outBytes.resize((size_t)li.QuadPart);
  DWORD read = 0;
  BOOL ok = TRUE;
  if (!outBytes.empty())
    ok = ReadFile(h, outBytes.data(), (DWORD)outBytes.size(), &read, nullptr);
  CloseHandle(h);
  return ok && read == outBytes.size();
}

static bool NormalizeInternalPath(std::string &pathGbk) {
  TrimAsciiInPlace(pathGbk);
  if (pathGbk.empty() || pathGbk[0] == ';' || pathGbk[0] == '#')
    return false;
  bool hasSlash = false;
  for (char &ch : pathGbk) {
    if (ch == '/')
      ch = '\\';
    if (ch == '\\')
      hasSlash = true;
  }
  if (!hasSlash)
    return false;
  if (pathGbk[0] != '\\' && !(pathGbk.size() >= 2 && pathGbk[1] == ':')) {
    pathGbk.insert(pathGbk.begin(), '\\');
  }
  return true;
}

static void ParseFileListIntoMap(const std::string &gbkText,
                                 std::unordered_map<unsigned long, std::wstring> &map) {
  size_t pos = 0;
  while (pos < gbkText.size()) {
    size_t end = gbkText.find_first_of("\r\n", pos);
    if (end == std::string::npos)
      end = gbkText.size();
    std::string line = gbkText.substr(pos, end - pos);
    pos = end;
    while (pos < gbkText.size() && (gbkText[pos] == '\r' || gbkText[pos] == '\n'))
      ++pos;

    TrimAsciiInPlace(line);
    if (line.empty() || line[0] == ';' || line[0] == '#')
      continue;

    if (line.find('\t') != std::string::npos) {
      std::vector<std::string> fields;
      size_t start = 0;
      while (start <= line.size()) {
        size_t tab = line.find('\t', start);
        if (tab == std::string::npos) {
          fields.push_back(line.substr(start));
          break;
        }
        fields.push_back(line.substr(start, tab - start));
        start = tab + 1;
      }
      if (fields.size() >= 4) {
        std::string idText = fields[1];
        TrimAsciiInPlace(idText);
        unsigned long uidHex = std::strtoul(idText.c_str(), nullptr, 16);
        unsigned long uidDec = std::strtoul(idText.c_str(), nullptr, 10);
        std::string pathGbk = fields[3];
        if (NormalizeInternalPath(pathGbk)) {
          std::wstring w = GbkToWide(pathGbk);
          if (!w.empty()) {
            if (uidHex != 0) map[uidHex] = w;
            if (uidDec != 0) map[uidDec] = w;
          }
          continue;
        }
      }
    }

    std::string pathGbk = line;
    if (NormalizeInternalPath(pathGbk)) {
      unsigned long uid = FileNameToPackId(pathGbk);
      if (uid != 0) {
        std::wstring w = GbkToWide(pathGbk);
        if (!w.empty()) map[uid] = w;
      }
    }
  }
}

static void LoadInternalPathMapForPak(const std::wstring &pakPath,
                                     std::unordered_map<unsigned long, std::wstring> &outMap) {
  outMap.clear();
  std::wstring dir = ParentDirOfPath(pakPath);
  std::wstring base = FileNameOfPath(pakPath);

  std::vector<std::wstring> candidates;
  candidates.push_back(pakPath + L".txt");
  candidates.push_back(dir + L"\\" + base + L".txt");
  candidates.push_back(dir + L"\\filelist.txt");
  candidates.push_back(dir + L"\\filelist_all.txt");

  wchar_t exePath[MAX_PATH] = {};
  GetModuleFileNameW(nullptr, exePath, MAX_PATH);
  std::wstring exeDir = ParentDirOfPath(exePath);
  candidates.push_back(exeDir + L"\\" + base + L".txt");
  candidates.push_back(exeDir + L"\\filelist.txt");

  for (const std::wstring &txtPath : candidates) {
    std::vector<unsigned char> bytes;
    if (ReadWholeFileBinary(txtPath, bytes) && !bytes.empty()) {
      std::string text((const char *)bytes.data(), bytes.size());
      ParseFileListIntoMap(text, outMap);
    }
  }
}

static bool LoadPakIndexCache(const std::wstring &pakPath, PakIndexCache &cache) {
  cache = PakIndexCache{};
  cache.pakPath = pakPath;

  HANDLE h = CreateFileW(pakPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE)
    return false;

  GUIXPackFileHeader header{};
  DWORD read = 0;
  if (!ReadFile(h, &header, sizeof(header), &read, nullptr) || read != sizeof(header)) {
    CloseHandle(h);
    return false;
  }

  if (std::memcmp(header.cSignature, "PACK", 4) != 0 || header.uCount == 0 ||
      header.uCount > 2000000u) {
    CloseHandle(h);
    return false;
  }

  LARGE_INTEGER li{};
  li.QuadPart = header.uIndexTableOffset;
  if (!SetFilePointerEx(h, li, nullptr, FILE_BEGIN)) {
    CloseHandle(h);
    return false;
  }

  std::vector<GUIXPackIndexInfo> indices(header.uCount);
  DWORD indexBytes = (DWORD)(header.uCount * sizeof(GUIXPackIndexInfo));
  if (!ReadFile(h, indices.data(), indexBytes, &read, nullptr) || read != indexBytes) {
    CloseHandle(h);
    return false;
  }
  CloseHandle(h);

  std::vector<PakOffsetEntry> offsets;
  offsets.reserve(indices.size());
  for (const auto &info : indices) {
    PakOffsetEntry entry;
    entry.start = info.uOffset;
    entry.storedSize = GetPakStoredSize((unsigned int)info.lCompressSizeFlag);
    if (entry.storedSize == 0)
      entry.storedSize = (unsigned int)(info.lSize < 0 ? -info.lSize : info.lSize);
    entry.end = entry.start + entry.storedSize;
    entry.uid = info.uId;
    offsets.push_back(entry);
  }

  std::sort(offsets.begin(), offsets.end(),
            [](const PakOffsetEntry &a, const PakOffsetEntry &b) {
              return a.start < b.start;
            });

  cache.indices = std::move(indices);
  cache.offsetEntries = std::move(offsets);
  LoadInternalPathMapForPak(pakPath, cache.pathMap);
  cache.valid = true;
  return true;
}

static PakIndexCache GetPakCache(const std::wstring &pakPath) {
  std::lock_guard<std::mutex> lock(g_pakCacheMutex);
  std::wstring key = ToLowerW(pakPath);
  auto it = g_pakCaches.find(key);
  if (it != g_pakCaches.end() && it->second.valid)
    return it->second;

  PakIndexCache cache;
  if (LoadPakIndexCache(pakPath, cache)) {
    g_pakCaches[key] = cache;
    return cache;
  }
  return PakIndexCache{};
}

bool FindPakIndexInfoByUid(const std::wstring &pakPath, unsigned long uid,
                           GUIXPackIndexInfo &outInfo) {
  outInfo = GUIXPackIndexInfo{};
  if (pakPath.empty() || uid == 0)
    return false;

  PakIndexCache cache = GetPakCache(pakPath);
  if (!cache.valid)
    return false;

  for (const auto &it : cache.indices) {
    if (it.uId == uid) {
      outInfo = it;
      return true;
    }
  }
  return false;
}

bool ResolvePakEntryByOffset(const std::wstring &pakPath, DWORD pid,
                             unsigned long long offset, PakEntryHit &outHit) {
  outHit = PakEntryHit{};
  if (pakPath.empty() || offset == 0)
    return false;

  PakIndexCache cache = GetPakCache(pakPath);
  if (!cache.valid || cache.offsetEntries.empty())
    return false;

  size_t left = 0;
  size_t right = cache.offsetEntries.size();
  size_t bestIndex = (size_t)-1;

  while (left < right) {
    size_t mid = left + (right - left) / 2;
    const PakOffsetEntry &entry = cache.offsetEntries[mid];
    if (offset >= entry.start && offset < entry.end) {
      bestIndex = mid;
      break;
    }
    if (offset < entry.start)
      right = mid;
    else
      left = mid + 1;
  }

  if (bestIndex == (size_t)-1) {
    const unsigned long long kTolerance = 1024 * 1024;
    for (size_t i = 0; i < cache.offsetEntries.size(); ++i) {
      const PakOffsetEntry &e = cache.offsetEntries[i];
      if (offset >= e.start && offset <= e.end + kTolerance) {
        bestIndex = i;
        break;
      }
    }
  }

  if (bestIndex != (size_t)-1) {
    const PakOffsetEntry &e = cache.offsetEntries[bestIndex];
    outHit.found = true;
    outHit.offset = e.start;
    outHit.uid = e.uid;
    outHit.storedSize = e.storedSize;

    std::wstring runtimePath;
    if (LookupRuntimePath(pid, e.uid, runtimePath)) {
      outHit.internalPath = runtimePath;
      return true;
    }

    auto it = cache.pathMap.find(e.uid);
    if (it != cache.pathMap.end() && !it->second.empty()) {
      outHit.internalPath = it->second;
      return true;
    }
    return true;
  }
  return false;
}

bool ReadStoredBlockFromPakByUid(const std::wstring &pakPath, unsigned long uid,
                                 std::vector<unsigned char> &outBuf,
                                 GUIXPackIndexInfo *outInfo) {
  outBuf.clear();
  GUIXPackIndexInfo info{};
  if (!FindPakIndexInfoByUid(pakPath, uid, info))
    return false;

  if (outInfo)
    *outInfo = info;

  unsigned int storedSize = GetPakStoredSize((unsigned int)info.lCompressSizeFlag);
  if (storedSize == 0)
    storedSize = (unsigned int)(info.lSize < 0 ? -info.lSize : info.lSize);
  if (storedSize == 0 || storedSize > 64u * 1024u * 1024u)
    return false;

  HANDLE h = CreateFileW(pakPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE)
    return false;

  LARGE_INTEGER off{};
  off.QuadPart = info.uOffset;
  if (!SetFilePointerEx(h, off, nullptr, FILE_BEGIN)) {
    CloseHandle(h);
    return false;
  }

  try {
    outBuf.resize(storedSize);
  } catch (...) {
    CloseHandle(h);
    outBuf.clear();
    return false;
  }

  DWORD read = 0;
  BOOL ok = ReadFile(h, outBuf.data(), storedSize, &read, nullptr);
  CloseHandle(h);
  return ok && read == storedSize;
}

bool ReadDecodedBlockFromPakByUid(const std::wstring &pakPath, unsigned long uid,
                                  std::vector<unsigned char> &outBuf,
                                  GUIXPackIndexInfo *outInfo) {
  GUIXPackIndexInfo info{};
  if (!ReadStoredBlockFromPakByUid(pakPath, uid, outBuf, &info))
    return false;

  unsigned int rawFlag = (unsigned int)info.lCompressSizeFlag;
  bool hasFrame = (rawFlag & GUI_TYPE_FRAME) != 0;
  unsigned int method = GetPakCompressMethod(rawFlag);
  unsigned long rawSize = (unsigned long)(info.lSize < 0 ? -info.lSize : info.lSize);
  if (rawSize > 64u * 1024u * 1024u)
    rawSize = 64u * 1024u * 1024u;

  try {
    std::vector<unsigned char> decoded;
    if (hasFrame) {
      if (DecompressFrameSprToUncompressedSpr(outBuf.data(), outBuf.size(), decoded) && !decoded.empty()) {
        outBuf.swap(decoded);
      }
    } else if (method != GUI_TYPE_NONE && rawSize > 0) {
      if (UclDecompressBuffer(outBuf.data(), (unsigned int)outBuf.size(), decoded, rawSize, false) && !decoded.empty()) {
        outBuf.swap(decoded);
      }
    }
  } catch (...) {
    // If decoding failed with an exception, keep whatever bytes were read safely
  }

  if (outInfo)
    *outInfo = info;
  return true;
}


static std::mutex g_runtimePathMutex;

void RememberRuntimePath(DWORD pid, unsigned long uid, const std::wstring &pathW) {
  if (pid == 0 || uid == 0 || pathW.empty())
    return;
  std::lock_guard<std::mutex> lock(g_runtimePathMutex);
  g_runtimePathMaps[pid][uid] = pathW;
}

bool LookupRuntimePath(DWORD pid, unsigned long uid, std::wstring &outPathW) {
  outPathW.clear();
  std::lock_guard<std::mutex> lock(g_runtimePathMutex);
  auto pidIt = g_runtimePathMaps.find(pid);
  if (pidIt == g_runtimePathMaps.end())
    return false;
  auto it = pidIt->second.find(uid);
  if (it == pidIt->second.end() || it->second.empty())
    return false;
  outPathW = it->second;
  return true;
}

void ClearRuntimePathsForPid(DWORD pid) {
  if (pid != 0) {
    std::lock_guard<std::mutex> lock(g_runtimePathMutex);
    g_runtimePathMaps.erase(pid);
  }
}

void AddSiblingPakFiles(const std::wstring &pakPath, std::set<std::wstring> &knownPaks) {
  if (pakPath.empty())
    return;

  std::vector<std::wstring> searchDirs;
  std::wstring dir = ParentDirOfPath(pakPath);
  if (!dir.empty()) {
    searchDirs.push_back(dir);
    std::wstring parent = ParentDirOfPath(dir);
    if (!parent.empty()) {
      searchDirs.push_back(parent);
      searchDirs.push_back(parent + L"\\data");
      searchDirs.push_back(parent + L"\\pak");
    }
  }

  for (const std::wstring &d : searchDirs) {
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW((d + L"\\*.pak").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
      do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
          knownPaks.insert(d + L"\\" + fd.cFileName);
      } while (FindNextFileW(h, &fd));
      FindClose(h);
    }
  }
}

bool FindKnownPakForUid(const std::set<std::wstring> &knownPaks, unsigned long uid,
                        std::wstring &outPakPath) {
  outPakPath.clear();
  if (uid == 0)
    return false;
  for (const std::wstring &pak : knownPaks) {
    GUIXPackIndexInfo info{};
    if (FindPakIndexInfoByUid(pak, uid, info)) {
      outPakPath = pak;
      return true;
    }
  }
  return false;
}

static void AddPathStringVariants(const std::wstring &inputRel, std::vector<std::wstring> &variants) {
  auto add = [&](const std::wstring &v) {
    if (v.empty()) return;
    for (const auto &it : variants) { if (_wcsicmp(it.c_str(), v.c_str()) == 0) return; }
    variants.push_back(v);
  };

  std::wstring cleanRel = inputRel;
  while (!cleanRel.empty() && (cleanRel[0] == L'\\' || cleanRel[0] == L'/' || cleanRel[0] == L'-' || cleanRel[0] == L'.' || cleanRel[0] == L' ' || cleanRel[0] == L'\t'))
    cleanRel.erase(cleanRel.begin());

  add(cleanRel);
  add(L"\\" + cleanRel);

  // If path contains internal anchor like \settings\ or \ui\ or \script\ after some garbage prefix
  static const wchar_t *kAnchors[] = {
      L"\\settings\\", L"settings\\", L"\\setting\\", L"setting\\",
      L"\\ui\\", L"ui\\", L"\\ui3\\", L"ui3\\", L"\\ui33\\", L"ui33\\",
      L"\\ui3_800\\", L"ui3_800\\", L"\\ui3_1024\\", L"ui3_1024\\",
      L"\\script\\", L"script\\", L"\\scripts\\", L"scripts\\",
      L"\\spr\\", L"spr\\", L"\\image\\", L"image\\",
      L"\\sound\\", L"sound\\", L"\\music\\", L"music\\",
      L"\\maps\\", L"maps\\", L"\\resource\\", L"resource\\",
      L"\\data\\", L"data\\"
  };

  std::wstring lowClean = ToLowerW(cleanRel);
  for (const wchar_t *anc : kAnchors) {
    size_t pos = lowClean.find(anc);
    if (pos != std::wstring::npos) {
      std::wstring subAnc = cleanRel.substr(pos);
      while (!subAnc.empty() && (subAnc[0] == L'\\' || subAnc[0] == L'/')) subAnc.erase(subAnc.begin());
      add(subAnc);
      add(L"\\" + subAnc);
    }
  }

  static const wchar_t *kPrefixes[] = {
      // UI variants (very common in JX/VoLam)
      L"Ui\\", L"\\Ui\\", L"ui\\", L"\\ui\\",
      L"ui\\ui3\\", L"\\ui\\ui3\\", L"Ui\\Ui3\\", L"\\Ui\\Ui3\\",
      L"ui\\ui3_800\\", L"\\ui\\ui3_800\\", L"Ui\\Ui3_800\\", L"\\Ui\\Ui3_800\\",
      L"ui\\ui3_1024\\", L"\\ui\\ui3_1024\\", L"Ui\\Ui3_1024\\", L"\\Ui\\Ui3_1024\\",
      L"ui\\ui33\\", L"\\ui\\ui33\\", L"Ui\\Ui33\\", L"\\Ui\\Ui33\\",
      L"ui\\ui_3\\", L"\\ui\\ui_3\\",
      L"ui3\\", L"\\ui3\\", L"Ui3\\", L"\\Ui3\\",
      L"ui33\\", L"\\ui33\\", L"Ui33\\", L"\\Ui33\\",
      L"ui3_800\\", L"\\ui3_800\\", L"ui3_1024\\", L"\\ui3_1024\\",
      L"ui_800\\", L"\\ui_800\\", L"ui_1024\\", L"\\ui_1024\\",
      L"ui\\auction\\", L"\\ui\\auction\\",
      L"ui\\ui3\\auction\\", L"\\ui\\ui3\\auction\\",
      L"ui3\\auction\\", L"\\ui3\\auction\\",
      L"ui33\\auction\\", L"\\ui33\\auction\\",
      L"ui3_800\\auction\\", L"\\ui3_800\\auction\\",
      L"ui3_1024\\auction\\", L"\\ui3_1024\\auction\\",
      L"ui\\ui3\\herotower\\", L"\\ui\\ui3\\herotower\\",
      L"ui\\ui3\\mail\\", L"\\ui\\ui3\\mail\\",
      L"ui\\ui3\\mantleimplicit\\", L"\\ui\\ui3\\mantleimplicit\\",
      L"ui\\ui3\\horseimplicit\\", L"\\ui\\ui3\\horseimplicit\\",
      L"ui\\ui3\\liansai\\", L"\\ui\\ui3\\liansai\\",
      L"ui\\ui3\\bossfightreport\\", L"\\ui\\ui3\\bossfightreport\\",
      L"ui\\ui3\\meridian\\", L"\\ui\\ui3\\meridian\\",

      // Settings variants
      L"settings\\", L"\\settings\\",
      L"setting\\", L"\\setting\\",
      L"settings\\03d\\", L"\\settings\\03d\\",
      L"setting\\03d\\", L"\\setting\\03d\\",
      L"settings\\ui\\", L"\\settings\\ui\\",
      L"settings\\ui3\\", L"\\settings\\ui3\\",
      L"settings\\ui3_800\\", L"\\settings\\ui3_800\\",
      L"settings\\item\\", L"\\settings\\item\\",
      L"settings\\player\\", L"\\settings\\player\\",
      L"settings\\npc\\", L"\\settings\\npc\\",
      L"settings\\skill\\", L"\\settings\\skill\\",
      L"settings\\skills\\", L"\\settings\\skills\\",
      L"settings\\task\\", L"\\settings\\task\\",
      L"settings\\maps\\", L"\\settings\\maps\\",
      L"settings\\auction\\", L"\\settings\\auction\\",

      // Script variants
      L"script\\", L"\\script\\",
      L"scripts\\", L"\\scripts\\",
      L"script\\ui\\", L"\\script\\ui\\",
      L"script\\lib\\", L"\\script\\lib\\",
      L"script\\item\\", L"\\script\\item\\",
      L"script\\npc\\", L"\\script\\npc\\",
      L"script\\task\\", L"\\script\\task\\",
      L"script\\missions\\", L"\\script\\missions\\",
      L"script\\mission\\", L"\\script\\mission\\",
      L"script\\event\\", L"\\script\\event\\",
      L"script\\skill\\", L"\\script\\skill\\",

      // SPR & Image variants
      L"spr\\", L"\\spr\\",
      L"spr\\ui\\", L"\\spr\\ui\\",
      L"spr\\ui3\\", L"\\spr\\ui3\\",
      L"spr\\ui33\\", L"\\spr\\ui33\\",
      L"spr\\npc\\", L"\\spr\\npc\\",
      L"spr\\item\\", L"\\spr\\item\\",
      L"spr\\skill\\", L"\\spr\\skill\\",
      L"image\\", L"\\image\\",
      L"image\\ui\\", L"\\image\\ui\\",
      L"image\\ui3\\", L"\\image\\ui3\\",

      // Sound & Music variants
      L"sound\\", L"\\sound\\",
      L"sound\\ui\\", L"\\sound\\ui\\",
      L"music\\", L"\\music\\",

      // Maps, Resource, Data variants
      L"maps\\", L"\\maps\\",
      L"resource\\", L"\\resource\\",
      L"data\\", L"\\data\\"
  };

  for (const wchar_t *pref : kPrefixes) {
    add(pref + cleanRel);
    add(L"\\" + std::wstring(pref) + cleanRel);
  }

  size_t slash = cleanRel.find_first_of(L"\\/");
  if (slash != std::wstring::npos) {
    std::wstring sub = cleanRel.substr(slash + 1);
    while (!sub.empty() && (sub[0] == L'\\' || sub[0] == L'/')) sub.erase(sub.begin());
    if (!sub.empty()) {
      add(sub);
      add(L"\\" + sub);
      for (const wchar_t *pref : kPrefixes) {
        add(pref + sub);
        add(L"\\" + std::wstring(pref) + sub);
      }
    }
  }
}

bool TryReadVirtualPathAcrossKnownPakRoots(
    const std::set<std::wstring> &knownPaks, const std::wstring &virtualPath,
    std::vector<unsigned char> &outBytes, std::wstring &outPathHint,
    std::string &outExt, std::wstring &outSourceInfo) {
  outBytes.clear();
  outPathHint = virtualPath;
  outExt = GetLowerExtFromPath(virtualPath);

  std::wstring cleanRel = virtualPath;
  for (wchar_t &ch : cleanRel) {
    if (ch == L'/') ch = L'\\';
  }
  while (!cleanRel.empty() && (cleanRel[0] == L'\\' || cleanRel[0] == L'/'))
    cleanRel.erase(cleanRel.begin());

  if (cleanRel.empty())
    return false;

  // 1. Try reading from disk relative to game root folders discovered from knownPaks
  std::set<std::wstring> gameDirs;
  for (const std::wstring &pak : knownPaks) {
    std::wstring pakDir = ParentDirOfPath(pak);
    if (!pakDir.empty()) {
      gameDirs.insert(pakDir);
      std::wstring clientDir = ParentDirOfPath(pakDir);
      if (!clientDir.empty()) gameDirs.insert(clientDir);
    }
  }

  std::vector<std::wstring> diskVariants;
  AddPathStringVariants(cleanRel, diskVariants);

  for (const std::wstring &gDir : gameDirs) {
    for (const std::wstring &relVar : diskVariants) {
      std::wstring diskPath = gDir + L"\\" + relVar;
      for (wchar_t &c : diskPath) { if (c == L'/') c = L'\\'; }
      DWORD attr = GetFileAttributesW(diskPath.c_str());
      if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        if (ReadWholeFileBinary(diskPath, outBytes) && !outBytes.empty()) {
          outSourceInfo = L"Disk: " + diskPath;
          outPathHint = diskPath;
          return true;
        }
      }
    }
  }

  // 2. Generate multi-candidate UIDs for PAK archive search
  std::vector<unsigned long> candidateUids;
  auto addUid = [&](unsigned long u) {
    if (u == 0) return;
    for (unsigned long e : candidateUids) { if (e == u) return; }
    candidateUids.push_back(u);
  };

  for (const std::wstring &v : diskVariants) {
    std::string gbk = WideToCodePage(936, v);
    if (!gbk.empty()) {
      addUid(FileNameToPackId(gbk));
    }
    std::string acp = WideToCodePage(CP_ACP, v);
    if (!acp.empty() && acp != gbk) {
      addUid(FileNameToPackId(acp));
    }
    std::string u8 = WideToUtf8(v);
    if (!u8.empty() && u8 != gbk && u8 != acp) {
      addUid(FileNameToPackId(u8));
    }
  }

  // 3. Search across all known PAK archives (prioritizing update and resource paks)
  std::vector<std::wstring> sortedPaks(knownPaks.begin(), knownPaks.end());
  std::sort(sortedPaks.begin(), sortedPaks.end(),
            [](const std::wstring &a, const std::wstring &b) {
              std::wstring la = ToLowerW(a);
              std::wstring lb = ToLowerW(b);
              int scoreA = 0, scoreB = 0;
              if (la.find(L"update") != std::wstring::npos) scoreA = 10;
              else if (la.find(L"resource") != std::wstring::npos) scoreA = 9;
              else if (la.find(L"settings") != std::wstring::npos) scoreA = 8;
              else if (la.find(L"spr") != std::wstring::npos) scoreA = 7;
              if (lb.find(L"update") != std::wstring::npos) scoreB = 10;
              else if (lb.find(L"resource") != std::wstring::npos) scoreB = 9;
              else if (lb.find(L"settings") != std::wstring::npos) scoreB = 8;
              else if (lb.find(L"spr") != std::wstring::npos) scoreB = 7;
              return scoreA > scoreB;
            });

  for (const std::wstring &pak : sortedPaks) {
    for (unsigned long uid : candidateUids) {
      GUIXPackIndexInfo info{};
      if (ReadDecodedBlockFromPakByUid(pak, uid, outBytes, &info)) {
        wchar_t uidBuf[32] = {};
        swprintf_s(uidBuf, L"0x%08lX", uid);
        outSourceInfo = L"PAK " + FileNameOfPath(pak) + L" (UID=" + uidBuf + L")";
        return true;
      }
    }
  }

  return false;
}
