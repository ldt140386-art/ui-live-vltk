#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include <map>

#pragma pack(push, 1)
struct GUIXPackFileHeader {
  unsigned char cSignature[4];
  unsigned long uCount;
  unsigned long uIndexTableOffset;
  unsigned long uDataOffset;
  unsigned long uCrc32;
  unsigned char cReserved[12];
};

struct GUIXPackIndexInfo {
  unsigned long uId;
  unsigned long uOffset;
  long lSize;
  long lCompressSizeFlag;
};
#pragma pack(pop)

static const unsigned int GUI_TYPE_NONE = 0x00000000;
static const unsigned int GUI_TYPE_UCL = 0x01000000;
static const unsigned int GUI_TYPE_BZIP2 = 0x02000000;
static const unsigned int GUI_TYPE_FRAME = 0x10000000;
static const unsigned int GUI_TYPE_UCL2 = 0x20000000;

struct PakOffsetEntry {
  unsigned long long start = 0;
  unsigned long long end = 0;
  unsigned long uid = 0;
  unsigned int storedSize = 0;
};

struct PakEntryHit {
  bool found = false;
  unsigned long long offset = 0;
  unsigned long uid = 0;
  unsigned int storedSize = 0;
  std::wstring internalPath;
};

struct PakIndexCache {
  std::wstring pakPath;
  std::vector<GUIXPackIndexInfo> indices;
  std::vector<PakOffsetEntry> offsetEntries;
  std::unordered_map<unsigned long, std::wstring> pathMap;
  bool valid = false;
};

unsigned long FileNameToPackId(const std::string &pathGbk);
unsigned int GetPakStoredSize(unsigned int flag);
unsigned int GetPakCompressMethod(unsigned int flag);

std::wstring ToLowerW(std::wstring text);
std::string ToLowerA(std::string text);
std::string WideToUtf8(const std::wstring &text);
std::wstring Utf8ToWide(const std::string &text);
std::wstring CodePageToWide(UINT codePage, const std::string &text);
std::string WideToCodePage(UINT codePage, const std::wstring &text);
std::wstring GbkToWide(const std::string &text);
std::string WideToGbk(const std::wstring &text);

bool IsPakPath(const std::wstring &path);
std::wstring ParentDirOfPath(const std::wstring &path);
std::wstring FileNameOfPath(const std::wstring &path);
std::string GetLowerExtFromPath(const std::wstring &path);

bool FindPakIndexInfoByUid(const std::wstring &pakPath, unsigned long uid,
                           GUIXPackIndexInfo &outInfo);

bool ResolvePakEntryByOffset(const std::wstring &pakPath, DWORD pid,
                             unsigned long long offset, PakEntryHit &outHit);

bool ReadStoredBlockFromPakByUid(const std::wstring &pakPath, unsigned long uid,
                                 std::vector<unsigned char> &outBuf,
                                 GUIXPackIndexInfo *outInfo = nullptr);

bool ReadDecodedBlockFromPakByUid(const std::wstring &pakPath, unsigned long uid,
                                  std::vector<unsigned char> &outBuf,
                                  GUIXPackIndexInfo *outInfo = nullptr);

void RememberRuntimePath(DWORD pid, unsigned long uid, const std::wstring &pathW);
bool LookupRuntimePath(DWORD pid, unsigned long uid, std::wstring &outPathW);
void ClearRuntimePathsForPid(DWORD pid);

void AddSiblingPakFiles(const std::wstring &pakPath, std::set<std::wstring> &knownPaks);
bool FindKnownPakForUid(const std::set<std::wstring> &knownPaks, unsigned long uid,
                        std::wstring &outPakPath);

bool TryReadVirtualPathAcrossKnownPakRoots(
    const std::set<std::wstring> &knownPaks, const std::wstring &virtualPath,
    std::vector<unsigned char> &outBytes, std::wstring &outPathHint,
    std::string &outExt, std::wstring &outSourceInfo);
