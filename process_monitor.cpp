#include "process_monitor.h"
#include "pak_engine.h"

#include <tlhelp32.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <map>

namespace {

static const LONG kStatusInfoLengthMismatch = (LONG)0xC0000004L;
static const ULONG kSystemExtendedHandleInformation = 64;
static const ULONG kFilePositionInformationClass = 14;

typedef LONG NTSTATUS;
typedef NTSTATUS(NTAPI *NtQuerySystemInformationFn)(ULONG, PVOID, ULONG, PULONG);
typedef NTSTATUS(NTAPI *NtQueryInformationFileFn)(HANDLE, PVOID, PVOID, ULONG, ULONG);

struct SystemHandleEntryEx {
  PVOID Object;
  ULONG_PTR UniqueProcessId;
  ULONG_PTR HandleValue;
  ULONG GrantedAccess;
  USHORT CreatorBackTraceIndex;
  USHORT ObjectTypeIndex;
  ULONG HandleAttributes;
  ULONG Reserved;
};

struct SystemHandleInfoEx {
  ULONG_PTR NumberOfHandles;
  ULONG_PTR Reserved;
  SystemHandleEntryEx Handles[1];
};

struct IoStatusBlockLocal {
  union {
    NTSTATUS Status;
    PVOID Pointer;
  };
  ULONG_PTR Information;
};

struct FilePositionInfoLocal {
  LARGE_INTEGER CurrentByteOffset;
};

static std::wstring FormatLastErrorText(DWORD err) {
  if (err == 0)
    return L"";
  wchar_t *buf = nullptr;
  DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD len = FormatMessageW(flags, nullptr, err, 0, (LPWSTR)&buf, 0, nullptr);
  std::wstring out;
  if (len && buf)
    out.assign(buf, len);
  if (buf)
    LocalFree(buf);
  while (!out.empty() && (out.back() == L'\r' || out.back() == L'\n' ||
                          out.back() == L' ' || out.back() == L'\t')) {
    out.pop_back();
  }
  return out;
}

static std::wstring FormatHex64(unsigned long long value) {
  wchar_t buf[32] = {};
  swprintf_s(buf, L"0x%llX", value);
  return buf;
}

static std::wstring FormatUid(unsigned long value) {
  wchar_t buf[32] = {};
  swprintf_s(buf, L"0x%08lX", value);
  return buf;
}

static std::vector<std::pair<std::wstring, std::wstring>> BuildDeviceMap() {
  std::vector<std::pair<std::wstring, std::wstring>> map;
  for (wchar_t ch = L'A'; ch <= L'Z'; ++ch) {
    wchar_t drive[] = {ch, L':', L'\0'};
    wchar_t target[1024] = {};
    DWORD len = QueryDosDeviceW(drive, target, (DWORD)(sizeof(target) / sizeof(target[0])));
    if (len > 0 && target[0]) {
      map.push_back(std::make_pair(std::wstring(target), std::wstring(drive)));
    }
  }
  std::sort(map.begin(), map.end(),
            [](const std::pair<std::wstring, std::wstring> &a,
               const std::pair<std::wstring, std::wstring> &b) {
              return a.first.size() > b.first.size();
            });
  return map;
}

static bool StartsWithI(const std::wstring &text, const wchar_t *prefix) {
  if (!prefix)
    return false;
  size_t prefixLen = wcslen(prefix);
  if (text.size() < prefixLen)
    return false;
  return _wcsnicmp(text.c_str(), prefix, prefixLen) == 0;
}

static std::wstring NormalizeFinalPath(
    const std::wstring &path,
    const std::vector<std::pair<std::wstring, std::wstring>> &deviceMap) {
  std::wstring out = path;
  if (StartsWithI(out, L"\\\\?\\UNC\\"))
    out = L"\\\\" + out.substr(8);
  else if (StartsWithI(out, L"\\\\?\\"))
    out = out.substr(4);
  else if (StartsWithI(out, L"\\??\\"))
    out = out.substr(4);

  if (StartsWithI(out, L"\\Device\\")) {
    for (const auto &it : deviceMap) {
      const std::wstring &device = it.first;
      if (out.size() >= device.size() &&
          _wcsnicmp(out.c_str(), device.c_str(), device.size()) == 0) {
        return it.second + out.substr(device.size());
      }
    }
  }
  return out;
}

static std::wstring GetDiskPathFromHandle(
    HANDLE hFile,
    const std::vector<std::pair<std::wstring, std::wstring>> &deviceMap) {
  DWORD cap = 1024;
  std::vector<wchar_t> buf(cap);
  DWORD len = GetFinalPathNameByHandleW(hFile, buf.data(), cap,
                                        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  if (len == 0)
    return L"";
  if (len >= cap) {
    cap = len + 1;
    buf.assign(cap, L'\0');
    len = GetFinalPathNameByHandleW(hFile, buf.data(), cap,
                                    FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (len == 0 || len >= cap)
      return L"";
  }
  return NormalizeFinalPath(std::wstring(buf.data(), len), deviceMap);
}

static bool QuerySystemHandles(std::vector<unsigned char> &buffer,
                               std::wstring &errorText) {
  HMODULE hNtDll = GetModuleHandleW(L"ntdll.dll");
  if (!hNtDll)
    hNtDll = LoadLibraryW(L"ntdll.dll");
  if (!hNtDll) {
    errorText = L"Cannot load ntdll.dll.";
    return false;
  }

  NtQuerySystemInformationFn query =
      (NtQuerySystemInformationFn)GetProcAddress(hNtDll, "NtQuerySystemInformation");
  if (!query) {
    errorText = L"NtQuerySystemInformation is not available.";
    return false;
  }

  ULONG size = 1u << 20;
  for (int i = 0; i < 8; ++i) {
    buffer.assign(size, 0);
    ULONG needed = 0;
    NTSTATUS st = query(kSystemExtendedHandleInformation, buffer.data(), size, &needed);
    if (st >= 0)
      return true;
    if (st == kStatusInfoLengthMismatch || needed > size) {
      size = needed > size ? needed + (1u << 16) : size * 2;
      continue;
    }
    wchar_t msg[96] = {};
    swprintf_s(msg, L"NtQuerySystemInformation failed: 0x%08X", (unsigned)st);
    errorText = msg;
    return false;
  }
  errorText = L"Handle table is too large to query.";
  return false;
}

static bool QueryHandleFileOffset(HANDLE hFile, unsigned long long &outOffset) {
  outOffset = 0;
  HMODULE hNtDll = GetModuleHandleW(L"ntdll.dll");
  if (!hNtDll)
    hNtDll = LoadLibraryW(L"ntdll.dll");
  if (!hNtDll)
    return false;
  NtQueryInformationFileFn query =
      (NtQueryInformationFileFn)GetProcAddress(hNtDll, "NtQueryInformationFile");
  if (!query)
    return false;

  IoStatusBlockLocal ios{};
  FilePositionInfoLocal pos{};
  NTSTATUS st = query(hFile, &ios, &pos, sizeof(pos), kFilePositionInformationClass);
  if (st < 0)
    return false;
  outOffset = (unsigned long long)pos.CurrentByteOffset.QuadPart;
  return true;
}

static bool IsKnownResourceExt(const std::string &ext) {
  static const char *kExts[] = {
      ".spr", ".ini", ".lua", ".txt", ".wav", ".mp3", ".jpg", ".jpeg",
      ".png", ".tga", ".bmp", ".gif", ".ani", ".3de", ".mtl", ".mesh",
      ".map", ".dat", ".res", ".json", ".xml"};
  for (const char *e : kExts) {
    if (_stricmp(ext.c_str(), e) == 0)
      return true;
  }
  return false;
}

static bool IsPathCharA(unsigned char ch) {
  if (ch >= 0x80) return true;
  if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'))
    return true;
  switch (ch) {
  case '\\': case '/': case '.': case '_': case '-': case '+': case '$': case '@': case ':':
    return true;
  default: return false;
  }
}

static bool IsPathCharW(wchar_t ch) {
  if (ch >= 0x80) return true;
  if ((ch >= L'0' && ch <= L'9') || (ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z'))
    return true;
  switch (ch) {
  case L'\\': case L'/': case L'.': case L'_': case L'-': case L'+': case L'$': case L'@': case L':':
    return true;
  default: return false;
  }
}

static bool MatchAsciiExt(const unsigned char *data, size_t len, size_t dot, size_t &outExtLen) {
  static const char *kExts[] = {
      ".spr", ".ini", ".lua", ".txt", ".wav", ".mp3", ".jpg", ".jpeg",
      ".png", ".tga", ".bmp", ".gif", ".ani", ".3de", ".mtl", ".mesh",
      ".map", ".dat", ".res", ".json", ".xml"};
  outExtLen = 0;
  if (!data || dot >= len || data[dot] != '.')
    return false;
  for (const char *ext : kExts) {
    size_t elen = strlen(ext);
    if (dot + elen > len)
      continue;
    if (_strnicmp((const char *)(data + dot), ext, elen) == 0) {
      if (dot + elen < len && IsPathCharA(data[dot + elen]))
        continue;
      outExtLen = elen;
      return true;
    }
  }
  return false;
}

static bool MatchWideExt(const std::vector<wchar_t> &text, size_t dot, size_t &outExtLen) {
  static const wchar_t *kExts[] = {
      L".spr", L".ini", L".lua", L".txt", L".wav", L".mp3", L".jpg",
      L".jpeg", L".png", L".tga", L".bmp", L".gif", L".ani", L".3de",
      L".mtl", L".mesh", L".map", L".dat", L".res", L".json", L".xml"};
  outExtLen = 0;
  if (dot >= text.size() || text[dot] != L'.')
    return false;
  for (const wchar_t *ext : kExts) {
    size_t elen = wcslen(ext);
    if (dot + elen > text.size())
      continue;
    if (_wcsnicmp(text.data() + dot, ext, elen) == 0) {
      if (dot + elen < text.size() && IsPathCharW(text[dot + elen]))
        continue;
      outExtLen = elen;
      return true;
    }
  }
  return false;
}

static bool IsReadableMemoryProtect(DWORD protect) {
  if (protect & PAGE_GUARD)
    return false;
  DWORD baseProtect = protect & 0xff;
  switch (baseProtect) {
  case PAGE_READONLY:
  case PAGE_READWRITE:
  case PAGE_WRITECOPY:
  case PAGE_EXECUTE_READ:
  case PAGE_EXECUTE_READWRITE:
  case PAGE_EXECUTE_WRITECOPY:
    return true;
  default:
    return false;
  }
}

} // namespace

bool EnumerateProcesses(std::vector<ProcessEntry> &outList) {
  outList.clear();
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE)
    return false;

  PROCESSENTRY32W pe{};
  pe.dwSize = sizeof(pe);
  if (Process32FirstW(snap, &pe)) {
    do {
      ProcessEntry e;
      e.pid = pe.th32ProcessID;
      e.name = pe.szExeFile;
      outList.push_back(e);
    } while (Process32NextW(snap, &pe));
  }
  CloseHandle(snap);

  std::sort(outList.begin(), outList.end(),
            [](const ProcessEntry &a, const ProcessEntry &b) {
              int cmp = _wcsicmp(a.name.c_str(), b.name.c_str());
              if (cmp != 0) return cmp < 0;
              return a.pid < b.pid;
            });
  return true;
}

bool SnapshotProcessHandlesAndPaks(DWORD pid,
                                   std::vector<FileEventEntry> &outEntries,
                                   std::set<std::wstring> &knownPaks,
                                   std::wstring &errorText) {
  outEntries.clear();
  if (pid == 0) {
    errorText = L"Select a process first.";
    return false;
  }

  HANDLE hProcess =
      OpenProcess(PROCESS_DUP_HANDLE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!hProcess) {
    DWORD err = GetLastError();
    errorText = L"Cannot open target process. Run as Administrator if needed.";
    std::wstring detail = FormatLastErrorText(err);
    if (!detail.empty())
      errorText += L" " + detail;
    return false;
  }

  std::vector<unsigned char> handleBuffer;
  if (!QuerySystemHandles(handleBuffer, errorText)) {
    CloseHandle(hProcess);
    return false;
  }

  const auto deviceMap = BuildDeviceMap();
  SystemHandleInfoEx *info = (SystemHandleInfoEx *)handleBuffer.data();
  std::map<std::wstring, FileEventEntry> uniqueEntries;

  for (ULONG_PTR i = 0; i < info->NumberOfHandles; ++i) {
    const SystemHandleEntryEx &entry = info->Handles[i];
    if ((DWORD)(ULONG_PTR)entry.UniqueProcessId != pid)
      continue;

    HANDLE hDup = nullptr;
    HANDLE hRemote = (HANDLE)(ULONG_PTR)entry.HandleValue;
    if (!DuplicateHandle(hProcess, hRemote, GetCurrentProcess(), &hDup, 0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
      continue;
    }

    DWORD type = GetFileType(hDup);
    if (type == FILE_TYPE_DISK) {
      std::wstring path = GetDiskPathFromHandle(hDup, deviceMap);
      if (!path.empty()) {
        std::wstring key = ToLowerW(path);
        FileEventEntry openEntry;
        openEntry.eventName = L"Open";
        openEntry.path = path;
        uniqueEntries[L"open|" + key] = openEntry;

        if (IsPakPath(path)) {
          knownPaks.insert(path);
          AddSiblingPakFiles(path, knownPaks);

          unsigned long long offset = 0;
          PakEntryHit hit;
          if (QueryHandleFileOffset(hDup, offset) &&
              ResolvePakEntryByOffset(path, pid, offset, hit)) {
            std::wstring display = path + L" @" + FormatHex64(offset) + L" -> ";
            if (!hit.internalPath.empty()) {
              display += hit.internalPath;
            } else {
              display += L"UID=" + FormatUid(hit.uid);
            }
            display += L" (block " + FormatHex64(hit.offset) + L", size ";
            wchar_t sizeBuf[32] = {};
            swprintf_s(sizeBuf, L"%u", hit.storedSize);
            display += sizeBuf;
            display += L")";

            FileEventEntry pakEntry;
            pakEntry.eventName = L"PakBlock";
            pakEntry.path = display;
            uniqueEntries[L"pak|" + key + L"|" + FormatUid(hit.uid)] = pakEntry;
          }
        }
      }
    }
    CloseHandle(hDup);
  }
  CloseHandle(hProcess);

  for (const auto &it : uniqueEntries)
    outEntries.push_back(it.second);
  return true;
}

bool ScanProcessMemoryStrings(DWORD pid,
                              std::vector<FileEventEntry> &outEntries,
                              std::wstring &errorText,
                              size_t maxRows) {
  outEntries.clear();
  if (pid == 0)
    return true;

  HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
  if (!hProcess) {
    DWORD err = GetLastError();
    errorText = L"Cannot scan target memory for path strings.";
    std::wstring detail = FormatLastErrorText(err);
    if (!detail.empty())
      errorText += L" " + detail;
    return false;
  }

  SYSTEM_INFO si{};
  GetSystemInfo(&si);
  ULONG_PTR minAddr = (ULONG_PTR)si.lpMinimumApplicationAddress;
  ULONG_PTR maxAddr = (ULONG_PTR)si.lpMaximumApplicationAddress;
  ULONG_PTR addr = minAddr;
  const SIZE_T kChunkSize = 256 * 1024;
  const SIZE_T kOverlap = 512;
  const unsigned long long kMaxBytesToScan = 1024ull * 1024ull * 1024ull;
  unsigned long long scannedBytes = 0;
  std::map<std::wstring, FileEventEntry> uniqueEntries;

  while (addr < maxAddr && uniqueEntries.size() < maxRows && scannedBytes < kMaxBytesToScan) {
    MEMORY_BASIC_INFORMATION mbi{};
    SIZE_T q = VirtualQueryEx(hProcess, (LPCVOID)addr, &mbi, sizeof(mbi));
    if (q == 0)
      break;

    ULONG_PTR base = (ULONG_PTR)mbi.BaseAddress;
    ULONG_PTR regionEnd = base + (ULONG_PTR)mbi.RegionSize;
    if (regionEnd <= addr)
      regionEnd = addr + 0x1000;

    if (mbi.State == MEM_COMMIT && IsReadableMemoryProtect(mbi.Protect)) {
      ULONG_PTR chunkAddr = addr > base ? addr : base;
      while (chunkAddr < regionEnd && uniqueEntries.size() < maxRows &&
             scannedBytes < kMaxBytesToScan) {
        SIZE_T want = (SIZE_T)std::min<ULONG_PTR>((ULONG_PTR)kChunkSize, regionEnd - chunkAddr);
        std::vector<unsigned char> buffer(want);
        SIZE_T got = 0;
        if (ReadProcessMemory(hProcess, (LPCVOID)chunkAddr, buffer.data(), want, &got) && got > 0) {
          scannedBytes += got;

          // Scan ANSI
          for (size_t i = 0; i < got && uniqueEntries.size() < maxRows; ++i) {
            if (buffer[i] != '.') continue;
            size_t extLen = 0;
            if (!MatchAsciiExt(buffer.data(), got, i, extLen)) continue;
            size_t start = i;
            while (start > 0 && IsPathCharA(buffer[start - 1]))
              --start;
            size_t end = i + extLen;
            if (end <= start || end - start < 5 || end - start > 520)
              continue;

            std::string raw((const char *)(buffer.data() + start), end - start);
            for (char &c : raw) { if (c == '/') c = '\\'; }
            if (raw.find('\\') == std::string::npos) continue;

            std::wstring wide = GbkToWide(raw);
            if (wide.empty()) wide = CodePageToWide(CP_ACP, raw);
            if (!wide.empty()) {
              if (wide[0] != L'\\' && !(wide.size() >= 2 && wide[1] == L':'))
                wide.insert(wide.begin(), L'\\');

              unsigned long uid = FileNameToPackId(WideToGbk(wide));
              if (uid != 0) RememberRuntimePath(pid, uid, wide);

              FileEventEntry fe;
              fe.eventName = L"PathStr";
              fe.path = wide;
              uniqueEntries[L"pathstr|" + ToLowerW(wide)] = fe;
            }
          }

          // Scan UTF-16
          for (size_t align = 0; align < 2 && uniqueEntries.size() < maxRows; ++align) {
            if (got <= align + 2) continue;
            size_t count = (got - align) / 2;
            std::vector<wchar_t> wide(count);
            for (size_t i = 0; i < count; ++i) {
              unsigned char lo = buffer[align + i * 2];
              unsigned char hi = buffer[align + i * 2 + 1];
              wide[i] = (wchar_t)((unsigned short)lo | ((unsigned short)hi << 8));
            }
            for (size_t i = 0; i < wide.size() && uniqueEntries.size() < maxRows; ++i) {
              if (wide[i] != L'.') continue;
              size_t extLen = 0;
              if (!MatchWideExt(wide, i, extLen)) continue;
              size_t start = i;
              while (start > 0 && IsPathCharW(wide[start - 1]))
                --start;
              size_t end = i + extLen;
              if (end <= start || end - start < 5 || end - start > 520)
                continue;

              std::wstring raw(wide.data() + start, wide.data() + end);
              for (wchar_t &c : raw) { if (c == L'/') c = L'\\'; }
              if (raw.find(L'\\') == std::wstring::npos) continue;

              if (raw[0] != L'\\' && !(raw.size() >= 2 && raw[1] == L':'))
                raw.insert(raw.begin(), L'\\');

              unsigned long uid = FileNameToPackId(WideToGbk(raw));
              if (uid != 0) RememberRuntimePath(pid, uid, raw);

              FileEventEntry fe;
              fe.eventName = L"PathStr";
              fe.path = raw;
              uniqueEntries[L"pathstr|" + ToLowerW(raw)] = fe;
            }
          }
        }
        if (want <= kOverlap)
          break;
        chunkAddr += want - kOverlap;
      }
    }
    addr = regionEnd;
  }

  CloseHandle(hProcess);

  for (const auto &it : uniqueEntries)
    outEntries.push_back(it.second);
  return true;
}

bool ResolveClickTarget(POINT screenPt, DWORD selectedPid, HWND &outHwnd,
                        DWORD &outPid, POINT &outClientPt,
                        std::wstring &outDesc, bool &outMatchesSelected) {
  outHwnd = nullptr;
  outPid = 0;
  outClientPt = POINT{};
  outDesc.clear();
  outMatchesSelected = false;

  HWND pointHwnd = WindowFromPoint(screenPt);
  if (!pointHwnd) {
    outDesc = L"No window under cursor.";
    return false;
  }

  bool foregroundFallback = false;
  HWND candidate = pointHwnd;
  DWORD candidatePid = 0;
  GetWindowThreadProcessId(candidate, &candidatePid);
  HWND root = GetAncestor(pointHwnd, GA_ROOT);
  DWORD rootPid = 0;
  if (root) GetWindowThreadProcessId(root, &rootPid);

  if (selectedPid != 0 && candidatePid != selectedPid && rootPid == selectedPid) {
    candidate = root;
    candidatePid = rootPid;
  }

  if (selectedPid != 0 && candidatePid != selectedPid) {
    HWND foreground = GetForegroundWindow();
    DWORD foregroundPid = 0;
    if (foreground) GetWindowThreadProcessId(foreground, &foregroundPid);
    if (foreground && foregroundPid == selectedPid) {
      candidate = foreground;
      candidatePid = foregroundPid;
      foregroundFallback = true;
    }
  }

  outHwnd = candidate;
  outPid = candidatePid;
  outClientPt = screenPt;
  if (candidate)
    ScreenToClient(candidate, &outClientPt);
  outMatchesSelected = (selectedPid != 0 && candidatePid == selectedPid);

  wchar_t title[256] = {};
  if (candidate) GetWindowTextW(candidate, title, (int)_countof(title));
  wchar_t className[128] = {};
  if (candidate) GetClassNameW(candidate, className, (int)_countof(className));

  wchar_t descBuf[512] = {};
  swprintf_s(descBuf, L"PID %lu  hwnd=0x%p  class=%s  title=\"%s\"%s",
             (unsigned long)candidatePid, (void *)candidate, className, title,
             foregroundFallback ? L" (foreground)" : L"");
  outDesc = descBuf;
  return true;
}
