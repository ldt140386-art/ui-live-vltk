#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <set>

struct ProcessEntry {
  DWORD pid = 0;
  std::wstring name;
};

struct FileEventEntry {
  std::wstring eventName;
  std::wstring path;
};

bool EnumerateProcesses(std::vector<ProcessEntry> &outList);

bool SnapshotProcessHandlesAndPaks(DWORD pid,
                                   std::vector<FileEventEntry> &outEntries,
                                   std::set<std::wstring> &knownPaks,
                                   std::wstring &errorText);

bool ScanProcessMemoryStrings(DWORD pid,
                              std::vector<FileEventEntry> &outEntries,
                              std::wstring &errorText,
                              size_t maxRows = 1000);

bool ResolveClickTarget(POINT screenPt, DWORD selectedPid, HWND &outHwnd,
                        DWORD &outPid, POINT &outClientPt,
                        std::wstring &outDesc, bool &outMatchesSelected);
