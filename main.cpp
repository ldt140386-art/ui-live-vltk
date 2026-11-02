#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <set>

#include "resource.h"
#include "listview_sorter.h"
#include "pak_engine.h"
#include "spr_decoder.h"
#include "media_preview.h"
#include "process_monitor.h"
#include "ui_ini_renderer.h"

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Msimg32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Ole32.lib")

namespace {

static const UINT_PTR kPollTimerId = 701;
static const UINT_PTR kClickTraceTimerId = 702;
static const UINT_PTR kSprAnimTimerId = 703;
static const UINT kPollIntervalMs = 1000;
static const UINT kClickTraceIntervalMs = 50;
static const UINT kSprAnimIntervalMs = 120;
static const DWORD kStringScanIntervalMs = 5000;

struct EventRowData {
  std::wstring time;
  DWORD pid = 0;
  std::wstring processName;
  std::wstring eventName;
  std::wstring path;
};

struct AppState {
  HWND hDlg = nullptr;
  HWND hComboProcess = nullptr;
  HWND hBtnRefresh = nullptr;
  HWND hBtnStart = nullptr;
  HWND hBtnStop = nullptr;
  HWND hBtnSnapshot = nullptr;
  HWND hBtnClear = nullptr;
  HWND hCheckAutoScroll = nullptr;

  HWND hClickPointEdit = nullptr;
  HWND hClickWindowEdit = nullptr;
  HWND hClickResourceEdit = nullptr;

  // New Search Filter & Batch Controls
  HWND hEditSearch = nullptr;
  HWND hBtnClearSearch = nullptr;
  HWND hBtnUnpackBatch = nullptr;
  HWND hBtnFramePrev = nullptr;
  HWND hBtnFrameNext = nullptr;
  HWND hComboZoom = nullptr;
  HWND hBtnSheetView = nullptr;

  HWND hList = nullptr;
  HWND hGroupPreview = nullptr;
  HWND hCheckAnimation = nullptr;
  HWND hPreviewText = nullptr;
  HWND hPreviewImage = nullptr;
  HWND hUiComposite = nullptr;
  HWND hPreviewInfo = nullptr;
  HWND hBtnPlay = nullptr;
  HWND hBtnStopMedia = nullptr;

  HWND hBtnOpenFolder = nullptr;
  HWND hBtnOpenPak = nullptr;
  HWND hBtnExport = nullptr;
  HWND hStatus = nullptr;

  HFONT hFont = nullptr;
  HBITMAP hPreviewBitmap = nullptr;
  HBITMAP hUiCompositeBitmap = nullptr;
  WNDPROC oldPreviewImageProc = nullptr;
  WNDPROC oldUiCompositeProc = nullptr;

  std::vector<ProcessEntry> processes;
  std::set<std::wstring> seenKeys;
  std::set<std::wstring> knownPaks;
  std::vector<EventRowData> allEvents;
  std::wstring currentFilter;

  std::vector<unsigned char> previewBytes;
  std::string previewExt;
  std::string previewMediaTag;
  std::wstring previewPathHint;
  std::wstring previewSourceInfo;

  bool polling = false;
  bool destroying = false;
  bool previewIsAudio = false;
  bool previewIsSpr = false;
  bool sprSheetMode = false;
  bool previewHasComposite = false;
  int sprCurrentFrame = 0;
  int sprTotalFrames = 0;
  int zoomMode = 1; // 1 = 1x (Auto), 2 = 2x, 4 = 4x
  DWORD lastStringScanTick = 0;
  DWORD selectedPid = 0;

  bool clickLeftWasDown = false;
  unsigned int clickSeq = 0;
};

static AppState *g_app = nullptr;
static void PreviewSelectedRow(AppState *st);
static void LayoutControls(AppState *st);

static void SetText(HWND hwnd, const std::wstring &text) {
  if (hwnd)
    SetWindowTextW(hwnd, text.c_str());
}

static std::wstring FormatTimeNow() {
  SYSTEMTIME st{};
  GetLocalTime(&st);
  wchar_t buf[32] = {};
  swprintf_s(buf, L"%02u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
  return buf;
}

static DWORD GetSelectedPid(AppState *st) {
  if (!st || !st->hComboProcess)
    return 0;
  int sel = (int)SendMessageW(st->hComboProcess, CB_GETCURSEL, 0, 0);
  if (sel < 0)
    return 0;
  return (DWORD)(ULONG_PTR)SendMessageW(st->hComboProcess, CB_GETITEMDATA, sel, 0);
}

static std::wstring GetSelectedProcessName(AppState *st, DWORD pid) {
  if (!st)
    return L"";
  for (const auto &it : st->processes) {
    if (it.pid == pid)
      return it.name;
  }
  return L"";
}

static void RefreshProcessList(AppState *st) {
  if (!st || !st->hComboProcess)
    return;
  DWORD keepPid = st->selectedPid ? st->selectedPid : GetSelectedPid(st);
  st->processes.clear();
  SendMessageW(st->hComboProcess, CB_RESETCONTENT, 0, 0);

  if (!EnumerateProcesses(st->processes)) {
    SetText(st->hStatus, L"Cannot enumerate processes.");
    return;
  }

  int keepIndex = -1;
  for (size_t i = 0; i < st->processes.size(); ++i) {
    const ProcessEntry &e = st->processes[i];
    wchar_t label[512] = {};
    swprintf_s(label, L"%lu - %s", (unsigned long)e.pid, e.name.c_str());
    int idx = (int)SendMessageW(st->hComboProcess, CB_ADDSTRING, 0, (LPARAM)label);
    SendMessageW(st->hComboProcess, CB_SETITEMDATA, idx, (LPARAM)(ULONG_PTR)e.pid);
    if (e.pid == keepPid)
      keepIndex = idx;
  }

  if (keepIndex < 0 && SendMessageW(st->hComboProcess, CB_GETCOUNT, 0, 0) > 0)
    keepIndex = 0;
  if (keepIndex >= 0)
    SendMessageW(st->hComboProcess, CB_SETCURSEL, keepIndex, 0);
  st->selectedPid = GetSelectedPid(st);
  SetText(st->hStatus, L"Ready. Select a process, then Snapshot or Start.");
}

static void UpdateButtons(AppState *st) {
  if (!st)
    return;
  EnableWindow(st->hBtnStart, st->polling ? FALSE : TRUE);
  EnableWindow(st->hBtnStop, st->polling ? TRUE : FALSE);
  EnableWindow(st->hBtnSnapshot, st->polling ? FALSE : TRUE);
}

static bool MatchesFilter(const EventRowData &item, const std::wstring &filterLower) {
  if (filterLower.empty())
    return true;
  if (ToLowerW(item.path).find(filterLower) != std::wstring::npos)
    return true;
  if (ToLowerW(item.eventName).find(filterLower) != std::wstring::npos)
    return true;
  if (ToLowerW(item.processName).find(filterLower) != std::wstring::npos)
    return true;
  wchar_t pidBuf[32] = {};
  swprintf_s(pidBuf, L"%lu", (unsigned long)item.pid);
  if (std::wstring(pidBuf).find(filterLower) != std::wstring::npos)
    return true;
  return false;
}

static void InsertListRowDirect(HWND hList, const EventRowData &data, bool ensureVisible) {
  if (!hList)
    return;
  int row = ListView_GetItemCount(hList);
  LVITEMW item{};
  item.mask = LVIF_TEXT;
  item.iItem = row;
  item.iSubItem = 0;
  item.pszText = const_cast<wchar_t *>(data.time.c_str());
  ListView_InsertItem(hList, &item);

  wchar_t pidBuf[32] = {};
  swprintf_s(pidBuf, L"%lu", (unsigned long)data.pid);
  ListView_SetItemText(hList, row, 1, pidBuf);
  ListView_SetItemText(hList, row, 2, const_cast<wchar_t *>(data.processName.c_str()));
  ListView_SetItemText(hList, row, 3, const_cast<wchar_t *>(data.eventName.c_str()));
  ListView_SetItemText(hList, row, 4, const_cast<wchar_t *>(data.path.c_str()));
  if (ensureVisible) {
    ListView_EnsureVisible(hList, row, FALSE);
  }
}

static void ApplyListFilter(AppState *st) {
  if (!st || !st->hList)
    return;
  SendMessageW(st->hList, WM_SETREDRAW, FALSE, 0);
  ListView_DeleteAllItems(st->hList);

  std::wstring filterLower = ToLowerW(st->currentFilter);
  bool autoScroll = st->hCheckAutoScroll &&
                    SendMessageW(st->hCheckAutoScroll, BM_GETCHECK, 0, 0) == BST_CHECKED;

  for (const auto &item : st->allEvents) {
    if (MatchesFilter(item, filterLower)) {
      InsertListRowDirect(st->hList, item, false);
    }
  }

  if (autoScroll && ListView_GetItemCount(st->hList) > 0) {
    ListView_EnsureVisible(st->hList, ListView_GetItemCount(st->hList) - 1, FALSE);
  }
  SendMessageW(st->hList, WM_SETREDRAW, TRUE, 0);
  InvalidateRect(st->hList, nullptr, TRUE);
}

static void InsertListRow(AppState *st, const std::wstring &time,
                          DWORD pid, const std::wstring &processName,
                          const std::wstring &eventName,
                          const std::wstring &path) {
  if (!st || !st->hList)
    return;

  EventRowData data{time, pid, processName, eventName, path};
  st->allEvents.push_back(data);

  std::wstring filterLower = ToLowerW(st->currentFilter);
  if (MatchesFilter(data, filterLower)) {
    bool autoScroll = st->hCheckAutoScroll &&
                      SendMessageW(st->hCheckAutoScroll, BM_GETCHECK, 0, 0) == BST_CHECKED;
    InsertListRowDirect(st->hList, data, autoScroll);
  }
}

static int ScanSelectedProcessCollect(AppState *st, bool manual,
                                      std::vector<std::wstring> *outAddedPaths) {
  if (!st)
    return 0;
  if (outAddedPaths)
    outAddedPaths->clear();
  DWORD pid = GetSelectedPid(st);
  st->selectedPid = pid;
  if (pid == 0) {
    SetText(st->hStatus, L"Select a process first.");
    return 0;
  }

  std::vector<FileEventEntry> entries;
  std::vector<FileEventEntry> stringEntries;
  std::vector<FileEventEntry> handleEntries;
  std::wstring error;
  std::wstring procName = GetSelectedProcessName(st, pid);
  DWORD nowTick = GetTickCount();
  bool scanStrings = manual || st->lastStringScanTick == 0 ||
                     (nowTick - st->lastStringScanTick) >= kStringScanIntervalMs;

  if (scanStrings) {
    std::wstring stringError;
    size_t maxRows = manual ? 1000 : 300;
    if (ScanProcessMemoryStrings(pid, stringEntries, stringError, maxRows)) {
      st->lastStringScanTick = nowTick;
      entries.insert(entries.end(), stringEntries.begin(), stringEntries.end());
    }
  }

  if (!SnapshotProcessHandlesAndPaks(pid, handleEntries, st->knownPaks, error)) {
    SetText(st->hStatus, error.empty() ? L"Snapshot failed." : error);
    return -1;
  }
  entries.insert(entries.end(), handleEntries.begin(), handleEntries.end());

  int added = 0;
  std::wstring now = FormatTimeNow();
  for (const FileEventEntry &entry : entries) {
    std::wstring key = ToLowerW(entry.eventName + L"|" + entry.path);
    if (st->seenKeys.insert(key).second) {
      InsertListRow(st, now, pid, procName, entry.eventName, entry.path);
      if (outAddedPaths)
        outAddedPaths->push_back(entry.path);
      ++added;
    }
  }

  wchar_t status[256] = {};
  if (added > 0) {
    swprintf_s(status, L"Added %d event(s). Strings: %u, handles/PAK: %u. Total: %d.",
               added, (unsigned)stringEntries.size(), (unsigned)handleEntries.size(),
               ListView_GetItemCount(st->hList));
  } else {
    swprintf_s(status, manual ? L"No new events. Strings: %u, handles/PAK: %u."
                              : L"Watching... (handles/PAK: %u, strings: %u, total items: %d)",
               (unsigned)handleEntries.size(), (unsigned)stringEntries.size(),
               ListView_GetItemCount(st->hList));
  }
  SetText(st->hStatus, status);
  return added;
}

static void PollClickTrace(AppState *st) {
  if (!st || !st->hDlg)
    return;

  bool leftDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
  if (!leftDown) {
    st->clickLeftWasDown = false;
    return;
  }
  if (st->clickLeftWasDown)
    return;
  st->clickLeftWasDown = true;

  POINT screenPt{};
  if (!GetCursorPos(&screenPt))
    return;

  DWORD selectedPid = GetSelectedPid(st);
  HWND targetHwnd = nullptr;
  DWORD targetPid = 0;
  POINT clientPt{};
  std::wstring windowDesc;
  bool matchesSelected = false;
  ResolveClickTarget(screenPt, selectedPid, targetHwnd, targetPid, clientPt,
                     windowDesc, matchesSelected);

  ++st->clickSeq;
  wchar_t pointText[128] = {};
  swprintf_s(pointText, L"#%u  Screen:%ld,%ld  Client:%ld,%ld",
             st->clickSeq, screenPt.x, screenPt.y, clientPt.x, clientPt.y);
  SetText(st->hClickPointEdit, pointText);
  SetText(st->hClickWindowEdit, windowDesc);

  if (selectedPid == 0) {
    SetText(st->hClickResourceEdit, L"Select a process first.");
    return;
  }
  if (!matchesSelected) {
    std::wstring msg = L"Ignored: selected PID " + std::to_wstring((unsigned long)selectedPid) +
                       L", clicked PID " + std::to_wstring((unsigned long)targetPid);
    SetText(st->hClickResourceEdit, msg);
    return;
  }

  std::vector<std::wstring> addedPaths;
  int added = ScanSelectedProcessCollect(st, true, &addedPaths);
  if (added > 0 && !addedPaths.empty()) {
    std::wstring sum = L"+" + std::to_wstring((unsigned long)addedPaths.size()) + L" " + addedPaths[0];
    if (addedPaths.size() > 1) sum += L" ...";
    SetText(st->hClickResourceEdit, sum);
  } else {
    SetText(st->hClickResourceEdit, L"No new resource accessed upon click.");
  }
}

static void StopSprPreviewAnimation(AppState *st) {
  if (st && st->hDlg)
    KillTimer(st->hDlg, kSprAnimTimerId);
}

static void ClearPreviewState(AppState *st) {
  if (!st)
    return;
  StopPreviewMedia();
  StopSprPreviewAnimation(st);
  st->previewBytes.clear();
  st->previewExt.clear();
  st->previewMediaTag.clear();
  st->previewPathHint.clear();
  st->previewSourceInfo.clear();
  st->previewIsAudio = false;
  st->previewIsSpr = false;
  st->sprCurrentFrame = 0;
  st->sprTotalFrames = 0;

  if (st->hPreviewBitmap) {
    DeleteObject(st->hPreviewBitmap);
    st->hPreviewBitmap = nullptr;
  }
  if (st->hPreviewImage)
    InvalidateRect(st->hPreviewImage, nullptr, TRUE);
  if (st->hPreviewText)
    SetText(st->hPreviewText, L"");
  if (st->hPreviewInfo)
    SetText(st->hPreviewInfo, L"Select a row to preview.");
  if (st->hBtnPlay)
    EnableWindow(st->hBtnPlay, FALSE);
  if (st->hBtnStopMedia)
    EnableWindow(st->hBtnStopMedia, FALSE);
  if (st->hCheckAnimation) {
    SendMessageW(st->hCheckAnimation, BM_SETCHECK, BST_UNCHECKED, 0);
    EnableWindow(st->hCheckAnimation, FALSE);
  }
  if (st->hBtnFramePrev)
    EnableWindow(st->hBtnFramePrev, FALSE);
  if (st->hBtnFrameNext)
    EnableWindow(st->hBtnFrameNext, FALSE);
  if (st->hPreviewImage)
    ShowWindow(st->hPreviewImage, SW_HIDE);
  if (st->hPreviewText)
    ShowWindow(st->hPreviewText, SW_SHOW);

  // Clear UI composite panel
  st->previewHasComposite = false;
  if (st->hUiCompositeBitmap) {
    DeleteObject(st->hUiCompositeBitmap);
    st->hUiCompositeBitmap = nullptr;
  }
  if (st->hUiComposite) {
    ShowWindow(st->hUiComposite, SW_HIDE);
    InvalidateRect(st->hUiComposite, nullptr, TRUE);
  }
}

static std::wstring GetSelectedListPath(AppState *st) {
  if (!st || !st->hList)
    return L"";
  int row = ListView_GetNextItem(st->hList, -1, LVNI_SELECTED);
  if (row < 0)
    return L"";
  wchar_t buf[4096] = {};
  ListView_GetItemText(st->hList, row, 4, buf, (int)_countof(buf));
  return buf;
}

static bool ParsePreviewRow(AppState *st, int row, std::wstring &outEvent,
                            std::wstring &outDisplayText, std::wstring &outDiskPath,
                            unsigned long &outUid, std::wstring &outVirtualPath) {
  outEvent.clear();
  outDisplayText.clear();
  outDiskPath.clear();
  outUid = 0;
  outVirtualPath.clear();
  if (!st || !st->hList || row < 0)
    return false;

  wchar_t bufEvent[64] = {};
  ListView_GetItemText(st->hList, row, 3, bufEvent, (int)_countof(bufEvent));
  outEvent = bufEvent;

  wchar_t bufPath[4096] = {};
  ListView_GetItemText(st->hList, row, 4, bufPath, (int)_countof(bufPath));
  outDisplayText = bufPath;

  if (outEvent == L"Open") {
    outDiskPath = outDisplayText;
    return !outDiskPath.empty();
  }
  if (outEvent == L"PathStr") {
    outVirtualPath = outDisplayText;
    return !outVirtualPath.empty();
  }
  if (outEvent == L"PakBlock" || outEvent == L"PAK") {
    size_t atPos = outDisplayText.find(L" @");
    if (atPos != std::wstring::npos)
      outDiskPath = outDisplayText.substr(0, atPos);
    else
      outDiskPath = outDisplayText;

    size_t arrow = outDisplayText.find(L" -> ");
    if (arrow != std::wstring::npos) {
      outDiskPath = outDisplayText.substr(0, arrow);
      size_t tail = outDisplayText.find(L" (block ", arrow + 4);
      if (tail == std::wstring::npos) tail = outDisplayText.size();
      std::wstring target = outDisplayText.substr(arrow + 4, tail - (arrow + 4));
      if (target.find(L"UID=0x") == 0) {
        outUid = wcstoul(target.c_str() + 6, nullptr, 16);
      } else {
        outVirtualPath = target;
      }
    } else {
      outVirtualPath = outDisplayText;
    }
    return !outDiskPath.empty() || !outVirtualPath.empty() || outUid != 0;
  }
  return false;
}

static void ParseAllUidsFromRowText(const std::wstring &text, std::vector<unsigned long> &uids) {
  size_t search = 0;
  while (search < text.size()) {
    size_t pos = text.find(L"0x", search);
    if (pos == std::wstring::npos)
      break;
    size_t start = pos + 2;
    size_t end = start;
    while (end < text.size()) {
      wchar_t ch = text[end];
      bool hex = (ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'f') || (ch >= L'A' && ch <= L'F');
      if (!hex) break;
      ++end;
    }
    if (end > start) {
      unsigned long u = wcstoul(text.substr(start, end - start).c_str(), nullptr, 16);
      if (u != 0) {
        bool exists = false;
        for (unsigned long e : uids) { if (e == u) { exists = true; break; } }
        if (!exists) uids.push_back(u);
      }
    }
    search = end + 1;
  }
}

static bool ReadPreviewBytesForRow(AppState *st, int row,
                                   std::vector<unsigned char> &outBytes,
                                   std::wstring &outPathHint,
                                   std::string &outExt,
                                   std::wstring &outSourceInfo) {
  outBytes.clear();
  outPathHint.clear();
  outExt.clear();
  outSourceInfo.clear();

  std::wstring eventName, rowText, diskPath, virtualPath;
  unsigned long uid = 0;
  if (!ParsePreviewRow(st, row, eventName, rowText, diskPath, uid, virtualPath))
    return false;

  if (st->knownPaks.empty() && st->selectedPid != 0) {
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, st->selectedPid);
    if (hProc) {
      wchar_t exePath[MAX_PATH * 2] = {};
      DWORD sz = (DWORD)_countof(exePath);
      if (QueryFullProcessImageNameW(hProc, 0, exePath, &sz)) {
        std::wstring exeDir = ParentDirOfPath(exePath);
        AddSiblingPakFiles(exeDir + L"\\dummy.pak", st->knownPaks);
        AddSiblingPakFiles(exeDir + L"\\data\\dummy.pak", st->knownPaks);
      }
      CloseHandle(hProc);
    }
  }

  outPathHint = !virtualPath.empty() ? virtualPath : diskPath;
  outExt = GetLowerExtFromPath(outPathHint);

  std::vector<unsigned long> candidateUids;
  if (uid != 0) candidateUids.push_back(uid);
  ParseAllUidsFromRowText(rowText, candidateUids);

  if (!virtualPath.empty()) {
    std::string gbk = WideToGbk(virtualPath);
    if (!gbk.empty()) {
      unsigned long u1 = FileNameToPackId(gbk);
      if (u1 != 0) candidateUids.push_back(u1);
      if (gbk[0] == '\\') {
        unsigned long u2 = FileNameToPackId(gbk.substr(1));
        if (u2 != 0) candidateUids.push_back(u2);
      } else {
        unsigned long u3 = FileNameToPackId("\\" + gbk);
        if (u3 != 0) candidateUids.push_back(u3);
      }
    }
  }

  // 1. Try reading disk file
  if (!diskPath.empty() && !IsPakPath(diskPath)) {
    DWORD attr = GetFileAttributesW(diskPath.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
      HANDLE h = CreateFileW(diskPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (h != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER li{};
        if (GetFileSizeEx(h, &li) && li.QuadPart > 0 && li.QuadPart <= 64ll * 1024ll * 1024ll) {
          outBytes.resize((size_t)li.QuadPart);
          DWORD rd = 0;
          BOOL ok = ReadFile(h, outBytes.data(), (DWORD)outBytes.size(), &rd, nullptr);
          CloseHandle(h);
          if (ok && rd == (DWORD)outBytes.size()) {
            outSourceInfo = L"Disk File";
            return true;
          }
          outBytes.clear();
        } else {
          CloseHandle(h);
        }
      }
    }
  }

  // 2. Try reading from known PAK directly with candidate UIDs
  if (!diskPath.empty() && IsPakPath(diskPath)) {
    for (unsigned long u : candidateUids) {
      if (ReadDecodedBlockFromPakByUid(diskPath, u, outBytes)) {
        outSourceInfo = L"PAK " + FileNameOfPath(diskPath);
        return true;
      }
    }
  }

  // 3. Search across all known PAKs
  for (unsigned long u : candidateUids) {
    std::wstring foundPak;
    if (FindKnownPakForUid(st->knownPaks, u, foundPak)) {
      if (ReadDecodedBlockFromPakByUid(foundPak, u, outBytes)) {
        outSourceInfo = L"PAK " + FileNameOfPath(foundPak);
        return true;
      }
    }
  }

  // 4. Try virtual path lookup across known PAK roots
  if (!virtualPath.empty()) {
    if (TryReadVirtualPathAcrossKnownPakRoots(st->knownPaks, virtualPath, outBytes,
                                              outPathHint, outExt, outSourceInfo)) {
      return true;
    }
  }

  return false;
}

static void PaintPreviewImageControl(HWND hwnd, HDC hdc, AppState *st) {
  RECT rc{};
  GetClientRect(hwnd, &rc);
  const int availW = rc.right - rc.left;
  const int availH = rc.bottom - rc.top;

  // Draw light checkerboard background for clean SPR pixel art preview
  const int tileSize = 16;
  HBRUSH br1 = CreateSolidBrush(RGB(255, 255, 255));
  HBRUSH br2 = CreateSolidBrush(RGB(240, 240, 240));
  for (int y = 0; y < availH; y += tileSize) {
    for (int x = 0; x < availW; x += tileSize) {
      RECT tr{x, y, std::min(x + tileSize, availW), std::min(y + tileSize, availH)};
      FillRect(hdc, &tr, ((x / tileSize + y / tileSize) % 2 == 0) ? br1 : br2);
    }
  }
  DeleteObject(br1);
  DeleteObject(br2);

  if (!st || !st->hPreviewBitmap)
    return;

  BITMAP bm{};
  if (!GetObjectW(st->hPreviewBitmap, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight == 0)
    return;

  const int srcW = bm.bmWidth;
  const int srcH = bm.bmHeight < 0 ? -bm.bmHeight : bm.bmHeight;

  int drawW = srcW;
  int drawH = srcH;

  if (st->zoomMode == 2) {
    drawW = srcW * 2;
    drawH = srcH * 2;
  } else if (st->zoomMode == 4) {
    drawW = srcW * 4;
    drawH = srcH * 4;
  } else {
    // 1x (Auto-fit if too large, 1:1 if fits)
    if (srcW > availW - 8 || srcH > availH - 8) {
      double scale = std::min((double)(availW - 8) / srcW, (double)(availH - 8) / srcH);
      drawW = std::max(1, (int)(srcW * scale));
      drawH = std::max(1, (int)(srcH * scale));
    }
  }

  const int x = (availW - drawW) / 2;
  const int y = (availH - drawH) / 2;

  HDC mem = CreateCompatibleDC(hdc);
  if (!mem)
    return;
  HBITMAP old = (HBITMAP)SelectObject(mem, st->hPreviewBitmap);

  BLENDFUNCTION blend{};
  blend.BlendOp = AC_SRC_OVER;
  blend.SourceConstantAlpha = 255;
  blend.AlphaFormat = AC_SRC_ALPHA;

  // Use COLORONCOLOR for integer pixel zoom (2x, 4x) to preserve pixel sharpness
  if (st->zoomMode > 1) {
    SetStretchBltMode(hdc, COLORONCOLOR);
  } else {
    SetStretchBltMode(hdc, HALFTONE);
    SetBrushOrgEx(hdc, 0, 0, nullptr);
  }

  if (!AlphaBlend(hdc, x, y, drawW, drawH, mem, 0, 0, srcW, srcH, blend)) {
    StretchBlt(hdc, x, y, drawW, drawH, mem, 0, 0, srcW, srcH, SRCCOPY);
  }

  SelectObject(mem, old);
  DeleteDC(mem);
}

static LRESULT CALLBACK PreviewImageSubclassProc(HWND hwnd, UINT msg,
                                                 WPARAM wParam, LPARAM lParam) {
  AppState *st = reinterpret_cast<AppState *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  WNDPROC oldProc = st ? st->oldPreviewImageProc : nullptr;

  switch (msg) {
  case WM_ERASEBKGND:
    return 1;
  case WM_PAINT: {
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(hwnd, &ps);
    PaintPreviewImageControl(hwnd, hdc, st);
    EndPaint(hwnd, &ps);
    return 0;
  }
  default:
    break;
  }
  return oldProc ? CallWindowProcW(oldProc, hwnd, msg, wParam, lParam)
                 : DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------
// UI Composite Panel (bottom half) painting
// ---------------------------------------------------------
static void PaintUiCompositeControl(HWND hwnd, HDC hdc, AppState *st) {
  RECT rc{};
  GetClientRect(hwnd, &rc);
  const int availW = rc.right - rc.left;
  const int availH = rc.bottom - rc.top;

  // Light checkerboard background
  const int tileSize = 16;
  HBRUSH br1 = CreateSolidBrush(RGB(245, 245, 250));
  HBRUSH br2 = CreateSolidBrush(RGB(230, 230, 238));
  for (int y = 0; y < availH; y += tileSize) {
    for (int x = 0; x < availW; x += tileSize) {
      RECT tr{x, y, std::min(x + tileSize, availW), std::min(y + tileSize, availH)};
      FillRect(hdc, &tr, ((x / tileSize + y / tileSize) % 2 == 0) ? br1 : br2);
    }
  }
  DeleteObject(br1);
  DeleteObject(br2);

  if (!st || !st->hUiCompositeBitmap) return;

  BITMAP bm{};
  if (!GetObjectW(st->hUiCompositeBitmap, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight == 0)
    return;

  const int srcW = bm.bmWidth;
  const int srcH = bm.bmHeight < 0 ? -bm.bmHeight : bm.bmHeight;

  // Fit to panel
  double scale = std::min((double)(availW - 4) / srcW, (double)(availH - 4) / srcH);
  if (scale > 1.0) scale = 1.0; // don't upscale
  int drawW = std::max(1, (int)(srcW * scale));
  int drawH = std::max(1, (int)(srcH * scale));
  int x = (availW - drawW) / 2;
  int y = (availH - drawH) / 2;

  HDC mem = CreateCompatibleDC(hdc);
  if (!mem) return;
  HBITMAP old = (HBITMAP)SelectObject(mem, st->hUiCompositeBitmap);

  BLENDFUNCTION blend{};
  blend.BlendOp = AC_SRC_OVER;
  blend.SourceConstantAlpha = 255;
  blend.AlphaFormat = AC_SRC_ALPHA;
  SetStretchBltMode(hdc, HALFTONE);
  SetBrushOrgEx(hdc, 0, 0, nullptr);

  if (!AlphaBlend(hdc, x, y, drawW, drawH, mem, 0, 0, srcW, srcH, blend)) {
    StretchBlt(hdc, x, y, drawW, drawH, mem, 0, 0, srcW, srcH, SRCCOPY);
  }

  SelectObject(mem, old);
  DeleteDC(mem);
}

static LRESULT CALLBACK UiCompositeSubclassProc(HWND hwnd, UINT msg,
                                                 WPARAM wParam, LPARAM lParam) {
  AppState *st = reinterpret_cast<AppState *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  WNDPROC oldProc = st ? st->oldUiCompositeProc : nullptr;

  switch (msg) {
  case WM_ERASEBKGND:
    return 1;
  case WM_PAINT: {
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(hwnd, &ps);
    PaintUiCompositeControl(hwnd, hdc, st);
    EndPaint(hwnd, &ps);
    return 0;
  }
  default:
    break;
  }
  return oldProc ? CallWindowProcW(oldProc, hwnd, msg, wParam, lParam)
                 : DefWindowProcW(hwnd, msg, wParam, lParam);
}

static bool RenderSprPreviewFrame(AppState *st, int frameIndex) {
  if (!st || !st->previewIsSpr || st->previewBytes.empty())
    return false;
  if (st->sprTotalFrames <= 0)
    st->sprTotalFrames = GetSprFrameCountFromBytes(st->previewBytes);
  if (st->sprTotalFrames <= 0)
    return false;

  int fi = frameIndex;
  if (fi < 0 || fi >= st->sprTotalFrames)
    fi = 0;

  std::string infoA;
  HBITMAP hbmp = CreateBitmapFromSprBytes(st->previewBytes.data(), st->previewBytes.size(),
                                          fi, infoA, false);
  if (!hbmp)
    return false;

  st->sprCurrentFrame = fi;
  if (st->hPreviewBitmap) {
    DeleteObject(st->hPreviewBitmap);
    st->hPreviewBitmap = nullptr;
  }
  st->hPreviewBitmap = hbmp;

  wchar_t frameText[128] = {};
  swprintf_s(frameText, L"Frame %d/%d - %s", fi + 1, st->sprTotalFrames,
             Utf8ToWide(infoA).c_str());
  SetText(st->hPreviewInfo, frameText);

  ShowWindow(st->hPreviewText, SW_HIDE);
  ShowWindow(st->hPreviewImage, SW_SHOW);
  if (st->hPreviewImage)
    InvalidateRect(st->hPreviewImage, nullptr, TRUE);
  return true;
}

static void StartSprPreviewAnimation(AppState *st) {
  if (!st || !st->hDlg || !st->previewIsSpr || st->sprTotalFrames <= 1) {
    StopSprPreviewAnimation(st);
    return;
  }
  st->sprSheetMode = false;
  KillTimer(st->hDlg, kSprAnimTimerId);
  SetTimer(st->hDlg, kSprAnimTimerId, kSprAnimIntervalMs, nullptr);
}

static void BuildSpriteSheetBitmap(AppState *st) {
  if (!st || !st->previewIsSpr || st->previewBytes.empty() || st->sprTotalFrames <= 0)
    return;

  int totalFrames = st->sprTotalFrames;
  int cols = (int)ceil(sqrt((double)totalFrames));
  if (cols < 1) cols = 1;
  int rows = (totalFrames + cols - 1) / cols;

  // Get canvas size from first frame render
  std::string dummyInfo;
  HBITMAP hTest = CreateBitmapFromSprBytes(st->previewBytes.data(), st->previewBytes.size(), 0, dummyInfo, false);
  if (!hTest) return;
  BITMAP bmTest{};
  GetObjectW(hTest, sizeof(bmTest), &bmTest);
  int cellW = bmTest.bmWidth;
  int cellH = bmTest.bmHeight < 0 ? -bmTest.bmHeight : bmTest.bmHeight;
  DeleteObject(hTest);
  if (cellW <= 0 || cellH <= 0) return;

  int gridLineW = 1;
  int sheetW = cols * cellW + (cols + 1) * gridLineW;
  int sheetH = rows * cellH + (rows + 1) * gridLineW;
  if (sheetW > 8192 || sheetH > 8192) return;

  HDC screenDC = GetDC(nullptr);
  HDC memDC = CreateCompatibleDC(screenDC);

  BITMAPINFO bmi{};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = sheetW;
  bmi.bmiHeader.biHeight = -sheetH;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  void *sheetBits = nullptr;
  HBITMAP hSheet = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &sheetBits, nullptr, 0);
  if (!hSheet || !sheetBits) {
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
    return;
  }

  // Fill with white background
  DWORD *pixels = (DWORD *)sheetBits;
  int totalPixels = sheetW * sheetH;
  for (int i = 0; i < totalPixels; ++i)
    pixels[i] = 0xFFFFFFFF; // opaque white (ARGB premul)

  // Draw grid lines in light gray
  for (int gy = 0; gy <= rows; ++gy) {
    int py = gy * (cellH + gridLineW);
    for (int px = 0; px < sheetW; ++px) {
      if (py < sheetH) pixels[py * sheetW + px] = 0xFFC0C0C0;
    }
  }
  for (int gx = 0; gx <= cols; ++gx) {
    int px = gx * (cellW + gridLineW);
    for (int py = 0; py < sheetH; ++py) {
      if (px < sheetW) pixels[py * sheetW + px] = 0xFFC0C0C0;
    }
  }

  // Render each frame into its cell
  HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hSheet);
  for (int fi = 0; fi < totalFrames; ++fi) {
    int col = fi % cols;
    int row = fi / cols;
    int cellX = gridLineW + col * (cellW + gridLineW);
    int cellY = gridLineW + row * (cellH + gridLineW);

    std::string frameInfo;
    HBITMAP hFrame = CreateBitmapFromSprBytes(st->previewBytes.data(), st->previewBytes.size(), fi, frameInfo, false);
    if (!hFrame) continue;

    HDC frameDC = CreateCompatibleDC(screenDC);
    HBITMAP oldFrame = (HBITMAP)SelectObject(frameDC, hFrame);

    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    AlphaBlend(memDC, cellX, cellY, cellW, cellH, frameDC, 0, 0, cellW, cellH, blend);

    SelectObject(frameDC, oldFrame);
    DeleteDC(frameDC);
    DeleteObject(hFrame);
  }
  SelectObject(memDC, oldBmp);
  DeleteDC(memDC);
  ReleaseDC(nullptr, screenDC);

  // Set as preview bitmap
  StopSprPreviewAnimation(st);
  if (st->hCheckAnimation)
    SendMessageW(st->hCheckAnimation, BM_SETCHECK, BST_UNCHECKED, 0);
  st->sprSheetMode = true;

  if (st->hPreviewBitmap) DeleteObject(st->hPreviewBitmap);
  st->hPreviewBitmap = hSheet;

  wchar_t info[128] = {};
  swprintf_s(info, L"Sprite Sheet %dx%d (%d frames, cell %dx%d)",
             sheetW, sheetH, totalFrames, cellW, cellH);
  SetText(st->hPreviewInfo, info);

  ShowWindow(st->hPreviewText, SW_HIDE);
  ShowWindow(st->hPreviewImage, SW_SHOW);
  if (st->hPreviewImage) InvalidateRect(st->hPreviewImage, nullptr, TRUE);
}

// ---------------------------------------------------------
// Floating Hover Preview Tooltip Implementation
// ---------------------------------------------------------
static HWND g_hHoverPopup = nullptr;
static HBITMAP g_hHoverBmp = nullptr;
static std::wstring g_hoverTitle;
static std::wstring g_hoverSubtitle;
static std::wstring g_hoverTextContent;
static bool g_hoverIsImage = false;
static int g_lastHoverRow = -1;
static WNDPROC g_oldListProc = nullptr;

static void HideHoverPopup() {
  if (g_hHoverPopup && IsWindowVisible(g_hHoverPopup)) {
    ShowWindow(g_hHoverPopup, SW_HIDE);
  }
  if (g_hHoverBmp) {
    DeleteObject(g_hHoverBmp);
    g_hHoverBmp = nullptr;
  }
  g_hoverIsImage = false;
  g_hoverTextContent.clear();
  g_hoverTitle.clear();
  g_hoverSubtitle.clear();
}

static LRESULT CALLBACK HoverPopupWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
  case WM_ERASEBKGND:
    return 1;
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    // Dark background
    HBRUSH bgBrush = CreateSolidBrush(RGB(28, 28, 30));
    FillRect(memDC, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Border
    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(75, 75, 82));
    HPEN oldPen = (HPEN)SelectObject(memDC, borderPen);
    HBRUSH nullBr = (HBRUSH)GetStockObject(NULL_BRUSH);
    HBRUSH oldBr = (HBRUSH)SelectObject(memDC, nullBr);
    Rectangle(memDC, 0, 0, width, height);
    SelectObject(memDC, oldPen);
    DeleteObject(borderPen);
    SelectObject(memDC, oldBr);

    // Header (Title & Subtitle)
    SetBkMode(memDC, TRANSPARENT);
    HFONT hTitleFont = CreateFontW(-12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT hSubFont = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT hTextFont = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");

    HFONT oldFont = (HFONT)SelectObject(memDC, hTitleFont);
    SetTextColor(memDC, RGB(235, 235, 240));
    RECT rcTitle = {8, 6, width - 8, 22};
    DrawTextW(memDC, g_hoverTitle.c_str(), -1, &rcTitle, DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_END_ELLIPSIS);

    SelectObject(memDC, hSubFont);
    SetTextColor(memDC, RGB(130, 180, 230));
    RECT rcSub = {8, 22, width - 8, 36};
    DrawTextW(memDC, g_hoverSubtitle.c_str(), -1, &rcSub, DT_LEFT | DT_NOPREFIX | DT_SINGLELINE | DT_END_ELLIPSIS);

    // Divider
    HPEN divPen = CreatePen(PS_SOLID, 1, RGB(55, 55, 60));
    SelectObject(memDC, divPen);
    MoveToEx(memDC, 6, 38, nullptr);
    LineTo(memDC, width - 6, 38);
    DeleteObject(divPen);

    RECT rcContent = {6, 42, width - 6, height - 6};

    if (g_hoverIsImage && g_hHoverBmp) {
      // Checkerboard
      const int tileSize = 12;
      HBRUSH cbr1 = CreateSolidBrush(RGB(20, 20, 22));
      HBRUSH cbr2 = CreateSolidBrush(RGB(34, 34, 38));
      for (int cy = rcContent.top; cy < rcContent.bottom; cy += tileSize) {
        for (int cx = rcContent.left; cx < rcContent.right; cx += tileSize) {
          RECT tr = {cx, cy, std::min(cx + tileSize, (int)rcContent.right), std::min(cy + tileSize, (int)rcContent.bottom)};
          FillRect(memDC, &tr, ((cx / tileSize + cy / tileSize) % 2 == 0) ? cbr1 : cbr2);
        }
      }
      DeleteObject(cbr1);
      DeleteObject(cbr2);

      BITMAP bm{};
      if (GetObjectW(g_hHoverBmp, sizeof(bm), &bm) && bm.bmWidth > 0 && bm.bmHeight != 0) {
        int srcW = bm.bmWidth;
        int srcH = bm.bmHeight < 0 ? -bm.bmHeight : bm.bmHeight;
        int maxW = rcContent.right - rcContent.left;
        int maxH = rcContent.bottom - rcContent.top;

        int drawW = srcW;
        int drawH = srcH;
        if (srcW > maxW || srcH > maxH) {
          double scale = std::min((double)maxW / srcW, (double)maxH / srcH);
          drawW = std::max(1, (int)(srcW * scale));
          drawH = std::max(1, (int)(srcH * scale));
        }

        int dx = rcContent.left + (maxW - drawW) / 2;
        int dy = rcContent.top + (maxH - drawH) / 2;

        HDC imgDC = CreateCompatibleDC(memDC);
        HBITMAP oldImgBmp = (HBITMAP)SelectObject(imgDC, g_hHoverBmp);

        BLENDFUNCTION blend{};
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = AC_SRC_ALPHA;

        SetStretchBltMode(memDC, HALFTONE);
        SetBrushOrgEx(memDC, 0, 0, nullptr);
        if (!AlphaBlend(memDC, dx, dy, drawW, drawH, imgDC, 0, 0, srcW, srcH, blend)) {
          StretchBlt(memDC, dx, dy, drawW, drawH, imgDC, 0, 0, srcW, srcH, SRCCOPY);
        }
        SelectObject(imgDC, oldImgBmp);
        DeleteDC(imgDC);
      }
    } else {
      // Monospace text box
      HBRUSH textBg = CreateSolidBrush(RGB(20, 20, 22));
      FillRect(memDC, &rcContent, textBg);
      DeleteObject(textBg);

      SelectObject(memDC, hTextFont);
      SetTextColor(memDC, RGB(220, 220, 225));
      RECT rcTextPad = {rcContent.left + 6, rcContent.top + 4, rcContent.right - 6, rcContent.bottom - 4};
      DrawTextW(memDC, g_hoverTextContent.c_str(), -1, &rcTextPad, DT_LEFT | DT_NOPREFIX | DT_WORDBREAK | DT_EXPANDTABS);
    }

    SelectObject(memDC, oldFont);
    DeleteObject(hTitleFont);
    DeleteObject(hSubFont);
    DeleteObject(hTextFont);

    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);

    EndPaint(hwnd, &ps);
    return 0;
  }
  default:
    return DefWindowProcW(hwnd, msg, wParam, lParam);
  }
}

static HBRUSH g_hHoverBgBrush = nullptr;

static void EnsureHoverPopupCreated(HWND hParent) {
  if (g_hHoverPopup && IsWindow(g_hHoverPopup)) return;

  static bool s_classRegistered = false;
  if (!s_classRegistered) {
    if (!g_hHoverBgBrush) {
      g_hHoverBgBrush = CreateSolidBrush(RGB(28, 28, 30));
    }
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DROPSHADOW | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = HoverPopupWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = g_hHoverBgBrush;
    wc.lpszClassName = L"FindInProcess_HoverPopup";
    if (RegisterClassExW(&wc) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
      s_classRegistered = true;
    }
  }

  g_hHoverPopup = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
      L"FindInProcess_HoverPopup", L"",
      WS_POPUP | WS_BORDER,
      0, 0, 10, 10,
      hParent, nullptr, GetModuleHandleW(nullptr), nullptr);
}

static void ShowHoverPopupForRow(AppState *st, int row, POINT ptCursor) {
  if (!st || !st->hList || row < 0 || row >= ListView_GetItemCount(st->hList)) {
    HideHoverPopup();
    return;
  }

  try {
    EnsureHoverPopupCreated(st->hDlg);

    std::vector<unsigned char> bytes;
    std::wstring pathHint;
    std::string ext;
    std::wstring sourceInfo;

    if (!ReadPreviewBytesForRow(st, row, bytes, pathHint, ext, sourceInfo) || bytes.empty()) {
      HideHoverPopup();
      return;
    }

    if (g_hHoverBmp) {
      DeleteObject(g_hHoverBmp);
      g_hHoverBmp = nullptr;
    }

    g_hoverTitle = FileNameOfPath(pathHint);
    if (g_hoverTitle.empty()) g_hoverTitle = pathHint;
    g_hoverSubtitle = sourceInfo + L"  (" + std::to_wstring(bytes.size()) + L" bytes)";

    int popW = 380;
    int popH = 260;
    g_hoverIsImage = false;

    if (ext == ".spr" || IsLikelySprBufferAt(bytes.data(), bytes.size())) {
      std::string sprInfo;
      g_hHoverBmp = CreateBitmapFromSprBytes(bytes.data(), bytes.size(), 0, sprInfo, false);
      if (g_hHoverBmp) {
        g_hoverIsImage = true;
        BITMAP bm{};
        GetObjectW(g_hHoverBmp, sizeof(bm), &bm);
        int w = bm.bmWidth, h = bm.bmHeight < 0 ? -bm.bmHeight : bm.bmHeight;
        int frameCnt = GetSprFrameCountFromBytes(bytes);
        g_hoverSubtitle = L"SPR " + std::to_wstring(w) + L"x" +
                          std::to_wstring(h) + L" (" +
                          std::to_wstring(frameCnt) + L" frames) - " + sourceInfo;
        popW = std::max(280, std::min(520, w + 40));
        popH = std::max(220, std::min(440, h + 70));
      }
    } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif") {
      HBITMAP hbmp = nullptr;
      std::wstring imgInfo;
      if (LoadWicBitmapFromBytes(bytes, hbmp, imgInfo)) {
        g_hHoverBmp = hbmp;
        g_hoverIsImage = true;
        BITMAP bm{};
        GetObjectW(g_hHoverBmp, sizeof(bm), &bm);
        int w = bm.bmWidth, h = bm.bmHeight < 0 ? -bm.bmHeight : bm.bmHeight;
        g_hoverSubtitle = imgInfo + L" - " + sourceInfo;
        popW = std::max(280, std::min(520, w + 40));
        popH = std::max(220, std::min(440, h + 70));
      }
    } else if (ext == ".tga") {
      HBITMAP hbmp = nullptr;
      std::wstring imgInfo;
      if (LoadTgaBitmapFromBytes(bytes, hbmp, imgInfo)) {
        g_hHoverBmp = hbmp;
        g_hoverIsImage = true;
        BITMAP bm{};
        GetObjectW(g_hHoverBmp, sizeof(bm), &bm);
        int w = bm.bmWidth, h = bm.bmHeight < 0 ? -bm.bmHeight : bm.bmHeight;
        g_hoverSubtitle = imgInfo + L" - " + sourceInfo;
        popW = std::max(280, std::min(520, w + 40));
        popH = std::max(220, std::min(440, h + 70));
      }
    }

    if (!g_hoverIsImage) {
      if (ext == ".txt" || ext == ".ini" || ext == ".lua" || ext == ".cfg" ||
          ext == ".log" || ext == ".xml" || ext == ".json" || ext == ".dat" ||
          LooksLikeTextBytes(bytes)) {
        std::wstring fullText = DecodeTextBytesForPreview(bytes);
        size_t lineCount = 0;
        size_t cutPos = 0;
        for (size_t i = 0; i < fullText.size() && lineCount < 15 && i < 1200; ++i) {
          if (fullText[i] == L'\n') lineCount++;
          cutPos = i + 1;
        }
        g_hoverTextContent = fullText.substr(0, cutPos);
        if (cutPos < fullText.size()) g_hoverTextContent += L"\r\n...";
        popW = 440;
        popH = 260;
      } else if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".mid") {
        g_hoverTextContent = L"[ Audio File / Stream ]\r\n\r\nFormat: " + Utf8ToWide(ext) +
                             L"\r\nSize: " + std::to_wstring(bytes.size()) + L" bytes\r\n\r\nClick row in list to Play sound.";
        popW = 320;
        popH = 180;
      } else {
        g_hoverTextContent = HexDumpPreview(bytes);
        popW = 420;
        popH = 220;
      }
    }

    HMONITOR hMon = MonitorFromPoint(ptCursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(hMon, &mi);

    int px = ptCursor.x + 16;
    int py = ptCursor.y + 16;
    if (px + popW > mi.rcWork.right) px = ptCursor.x - popW - 10;
    if (py + popH > mi.rcWork.bottom) py = ptCursor.y - popH - 10;
    if (px < mi.rcWork.left) px = mi.rcWork.left + 4;
    if (py < mi.rcWork.top) py = mi.rcWork.top + 4;

    SetWindowPos(g_hHoverPopup, HWND_TOPMOST, px, py, popW, popH, SWP_SHOWWINDOW | SWP_NOACTIVATE);
    InvalidateRect(g_hHoverPopup, nullptr, TRUE);
    UpdateWindow(g_hHoverPopup);
  } catch (...) {
    HideHoverPopup();
  }
}

static LRESULT CALLBACK ListSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  AppState *st = (AppState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

  switch (msg) {
  case WM_MOUSEMOVE: {
    LVHITTESTINFO hti{};
    hti.pt.x = GET_X_LPARAM(lParam);
    hti.pt.y = GET_Y_LPARAM(lParam);
    int hitRow = ListView_SubItemHitTest(hwnd, &hti);
    if (hitRow >= 0 && hitRow != g_lastHoverRow) {
      g_lastHoverRow = hitRow;
      TRACKMOUSEEVENT tme{};
      tme.cbSize = sizeof(tme);
      tme.dwFlags = TME_HOVER | TME_LEAVE;
      tme.hwndTrack = hwnd;
      tme.dwHoverTime = 250; // 250ms hover trigger
      TrackMouseEvent(&tme);
    } else if (hitRow < 0) {
      g_lastHoverRow = -1;
      HideHoverPopup();
    }
    break;
  }
  case WM_MOUSEHOVER: {
    if (g_lastHoverRow >= 0 && st) {
      POINT pt{};
      GetCursorPos(&pt);
      ShowHoverPopupForRow(st, g_lastHoverRow, pt);
    }
    break;
  }
  case WM_MOUSELEAVE: {
    g_lastHoverRow = -1;
    HideHoverPopup();
    break;
  }
  case WM_LBUTTONDOWN:
  case WM_RBUTTONDOWN:
  case WM_MBUTTONDOWN:
  case WM_VSCROLL:
  case WM_HSCROLL:
  case WM_MOUSEWHEEL:
  case WM_KEYDOWN:
    g_lastHoverRow = -1;
    HideHoverPopup();
    break;
  }

  return g_oldListProc ? CallWindowProcW(g_oldListProc, hwnd, msg, wParam, lParam)
                       : DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------
// Subclass and Hover Extraction for Edit Preview Text Box
// ---------------------------------------------------------
static WNDPROC g_oldPreviewTextProc = nullptr;
static std::wstring g_lastHoverTextPath;

static void ShowHoverPopupForVirtualPath(AppState *st, const std::wstring &virtualPath, POINT ptCursor) {
  if (!st || virtualPath.empty()) {
    HideHoverPopup();
    return;
  }

  try {
    EnsureHoverPopupCreated(st->hDlg);

    std::vector<unsigned char> bytes;
    std::wstring pathHint = virtualPath;
    std::string ext = GetLowerExtFromPath(virtualPath);
    std::wstring sourceInfo;

    if (st->knownPaks.empty() && st->selectedPid != 0) {
      HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, st->selectedPid);
      if (hProc) {
        wchar_t exePath[MAX_PATH * 2] = {};
        DWORD sz = (DWORD)_countof(exePath);
        if (QueryFullProcessImageNameW(hProc, 0, exePath, &sz)) {
          std::wstring exeDir = ParentDirOfPath(exePath);
          AddSiblingPakFiles(exeDir + L"\\dummy.pak", st->knownPaks);
          AddSiblingPakFiles(exeDir + L"\\data\\dummy.pak", st->knownPaks);
        }
        CloseHandle(hProc);
      }
    }

    if (!TryReadVirtualPathAcrossKnownPakRoots(st->knownPaks, virtualPath, bytes, pathHint, ext, sourceInfo) || bytes.empty()) {
      HideHoverPopup();
      return;
    }

    if (g_hHoverBmp) {
      DeleteObject(g_hHoverBmp);
      g_hHoverBmp = nullptr;
    }

    g_hoverTitle = FileNameOfPath(pathHint);
    if (g_hoverTitle.empty()) g_hoverTitle = pathHint;
    g_hoverSubtitle = sourceInfo + L"  (" + std::to_wstring(bytes.size()) + L" bytes)";

    int popW = 380;
    int popH = 260;
    g_hoverIsImage = false;

    if (ext == ".spr" || IsLikelySprBufferAt(bytes.data(), bytes.size())) {
      std::string sprInfo;
      g_hHoverBmp = CreateBitmapFromSprBytes(bytes.data(), bytes.size(), 0, sprInfo, false);
      if (g_hHoverBmp) {
        g_hoverIsImage = true;
        BITMAP bm{};
        GetObjectW(g_hHoverBmp, sizeof(bm), &bm);
        int w = bm.bmWidth, h = bm.bmHeight < 0 ? -bm.bmHeight : bm.bmHeight;
        int frameCnt = GetSprFrameCountFromBytes(bytes);
        g_hoverSubtitle = L"SPR " + std::to_wstring(w) + L"x" +
                          std::to_wstring(h) + L" (" +
                          std::to_wstring(frameCnt) + L" frames) - " + sourceInfo;
        popW = std::max(280, std::min(520, w + 40));
        popH = std::max(220, std::min(440, h + 70));
      }
    } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif") {
      HBITMAP hbmp = nullptr;
      std::wstring imgInfo;
      if (LoadWicBitmapFromBytes(bytes, hbmp, imgInfo)) {
        g_hHoverBmp = hbmp;
        g_hoverIsImage = true;
        BITMAP bm{};
        GetObjectW(g_hHoverBmp, sizeof(bm), &bm);
        int w = bm.bmWidth, h = bm.bmHeight < 0 ? -bm.bmHeight : bm.bmHeight;
        g_hoverSubtitle = imgInfo + L" - " + sourceInfo;
        popW = std::max(280, std::min(520, w + 40));
        popH = std::max(220, std::min(440, h + 70));
      }
    } else if (ext == ".tga") {
      HBITMAP hbmp = nullptr;
      std::wstring imgInfo;
      if (LoadTgaBitmapFromBytes(bytes, hbmp, imgInfo)) {
        g_hHoverBmp = hbmp;
        g_hoverIsImage = true;
        BITMAP bm{};
        GetObjectW(g_hHoverBmp, sizeof(bm), &bm);
        int w = bm.bmWidth, h = bm.bmHeight < 0 ? -bm.bmHeight : bm.bmHeight;
        g_hoverSubtitle = imgInfo + L" - " + sourceInfo;
        popW = std::max(280, std::min(520, w + 40));
        popH = std::max(220, std::min(440, h + 70));
      }
    }

    if (!g_hoverIsImage) {
      if (ext == ".txt" || ext == ".ini" || ext == ".lua" || ext == ".cfg" ||
          ext == ".log" || ext == ".xml" || ext == ".json" || ext == ".dat" ||
          LooksLikeTextBytes(bytes)) {
        std::wstring fullText = DecodeTextBytesForPreview(bytes);
        size_t lineCount = 0;
        size_t cutPos = 0;
        for (size_t i = 0; i < fullText.size() && lineCount < 15 && i < 1200; ++i) {
          if (fullText[i] == L'\n') lineCount++;
          cutPos = i + 1;
        }
        g_hoverTextContent = fullText.substr(0, cutPos);
        if (cutPos < fullText.size()) g_hoverTextContent += L"\r\n...";
        popW = 440;
        popH = 260;
      } else if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".mid") {
        g_hoverTextContent = L"[ Audio File / Stream ]\r\n\r\nFormat: " + Utf8ToWide(ext) +
                             L"\r\nSize: " + std::to_wstring(bytes.size()) + L" bytes\r\n\r\nClick row in list to Play sound.";
        popW = 320;
        popH = 180;
      } else {
        g_hoverTextContent = HexDumpPreview(bytes);
        popW = 420;
        popH = 220;
      }
    }

    HMONITOR hMon = MonitorFromPoint(ptCursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(hMon, &mi);

    int px = ptCursor.x + 16;
    int py = ptCursor.y + 16;
    if (px + popW > mi.rcWork.right) px = ptCursor.x - popW - 10;
    if (py + popH > mi.rcWork.bottom) py = ptCursor.y - popH - 10;
    if (px < mi.rcWork.left) px = mi.rcWork.left + 4;
    if (py < mi.rcWork.top) py = mi.rcWork.top + 4;

    SetWindowPos(g_hHoverPopup, HWND_TOPMOST, px, py, popW, popH, SWP_SHOWWINDOW | SWP_NOACTIVATE);
    InvalidateRect(g_hHoverPopup, nullptr, TRUE);
    UpdateWindow(g_hHoverPopup);
  } catch (...) {
    HideHoverPopup();
  }
}

static std::wstring ExtractResourcePathFromCharPos(HWND hEdit, int charPos) {
  if (!hEdit || charPos < 0) return L"";
  int lineNo = (int)SendMessageW(hEdit, EM_LINEFROMCHAR, charPos, 0);
  int lineIndex = (int)SendMessageW(hEdit, EM_LINEINDEX, lineNo, 0);
  int lineLen = (int)SendMessageW(hEdit, EM_LINELENGTH, lineIndex, 0);
  if (lineLen <= 0 || lineLen > 4096) return L"";

  std::vector<wchar_t> lineBuf(lineLen + 4, 0);
  *(WORD*)lineBuf.data() = (WORD)(lineLen + 2);
  int copied = (int)SendMessageW(hEdit, EM_GETLINE, lineNo, (LPARAM)lineBuf.data());
  if (copied <= 0) return L"";
  lineBuf[copied] = L'\0';
  std::wstring line = lineBuf.data();

  int col = charPos - lineIndex;
  if (col < 0) col = 0;
  if (col > (int)line.size()) col = (int)line.size();

  auto isDelim = [](wchar_t c) {
    return c == L'=' || c == L'"' || c == L'\'' || c == L'\t' || c == L' ' ||
           c == L',' || c == L';' || c == L'(' || c == L')' || c == L'[' ||
           c == L']' || c == L'{' || c == L'}' || c == L'<' || c == L'>' ||
           c == L'\r' || c == L'\n';
  };

  auto isKnownPath = [](const std::wstring &s) {
    if (s.empty()) return false;
    std::string ext = GetLowerExtFromPath(s);
    if (ext == ".spr" || ext == ".png" || ext == ".tga" || ext == ".bmp" ||
        ext == ".jpg" || ext == ".jpeg" || ext == ".wav" || ext == ".mp3" ||
        ext == ".ogg" || ext == ".mid" || ext == ".ini" || ext == ".lua" ||
        ext == ".txt" || ext == ".dat" || ext == ".xml" || ext == ".json" ||
        ext == ".ani" || ext == ".3de" || ext == ".mesh" || ext == ".mtl" ||
        ext == ".res" || ext == ".cfg")
      return true;
    if (s.find(L"\\spr\\") != std::wstring::npos || s.find(L"\\ui\\") != std::wstring::npos ||
        s.find(L"\\sound\\") != std::wstring::npos || s.find(L"\\settings\\") != std::wstring::npos ||
        s.find(L"\\image\\") != std::wstring::npos || s.find(L"\\script\\") != std::wstring::npos)
      return true;
    return false;
  };

  // 1. Try word around col
  int start = col;
  while (start > 0 && !isDelim(line[start - 1])) start--;
  int end = col;
  while (end < (int)line.size() && !isDelim(line[end])) end++;
  if (end > start) {
    std::wstring token = line.substr(start, end - start);
    while (!token.empty() && (token.front() == L' ' || token.front() == L'=' || token.front() == L'"' || token.front() == L'\''))
      token.erase(token.begin());
    while (!token.empty() && (token.back() == L' ' || token.back() == L'"' || token.back() == L'\''))
      token.pop_back();
    if (isKnownPath(token))
      return token;
  }

  // 2. Try value after '=' in line
  size_t eqPos = line.find(L'=');
  if (eqPos != std::wstring::npos) {
    std::wstring val = line.substr(eqPos + 1);
    while (!val.empty() && (val.front() == L' ' || val.front() == L'\t' || val.front() == L'"' || val.front() == L'\''))
      val.erase(val.begin());
    while (!val.empty() && (val.back() == L' ' || val.back() == L'\t' || val.back() == L'\r' || val.back() == L'\n' || val.back() == L'"' || val.back() == L'\''))
      val.pop_back();
    if (isKnownPath(val))
      return val;
  }

  // 3. Scan whole line for any path pattern
  for (size_t i = 0; i < line.size(); ++i) {
    if (line[i] == L'\\' || line[i] == L'/') {
      size_t pEnd = i;
      while (pEnd < line.size() && !isDelim(line[pEnd])) pEnd++;
      std::wstring sub = line.substr(i, pEnd - i);
      if (isKnownPath(sub))
        return sub;
    }
  }

  return L"";
}

static LRESULT CALLBACK PreviewTextSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  AppState *st = (AppState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

  switch (msg) {
  case WM_MOUSEMOVE: {
    POINT ptClient = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    LRESULT cp = SendMessageW(hwnd, EM_CHARFROMPOS, 0, MAKELPARAM(ptClient.x, ptClient.y));
    int charPos = (int)(cp & 0xFFFF);
    std::wstring path = ExtractResourcePathFromCharPos(hwnd, charPos);
    if (!path.empty()) {
      if (path != g_lastHoverTextPath) {
        g_lastHoverTextPath = path;
        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_HOVER | TME_LEAVE;
        tme.hwndTrack = hwnd;
        tme.dwHoverTime = 250; // 250ms hover trigger
        TrackMouseEvent(&tme);
      }
    } else {
      g_lastHoverTextPath.clear();
      HideHoverPopup();
    }
    break;
  }
  case WM_MOUSEHOVER: {
    if (!g_lastHoverTextPath.empty() && st) {
      POINT pt{};
      GetCursorPos(&pt);
      ShowHoverPopupForVirtualPath(st, g_lastHoverTextPath, pt);
    }
    break;
  }
  case WM_MOUSELEAVE: {
    g_lastHoverTextPath.clear();
    HideHoverPopup();
    break;
  }
  case WM_LBUTTONDOWN:
  case WM_RBUTTONDOWN:
  case WM_VSCROLL:
  case WM_HSCROLL:
  case WM_MOUSEWHEEL:
  case WM_KEYDOWN:
    g_lastHoverTextPath.clear();
    HideHoverPopup();
    break;
  case WM_LBUTTONDBLCLK: {
    POINT ptClient = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    LRESULT cp = SendMessageW(hwnd, EM_CHARFROMPOS, 0, MAKELPARAM(ptClient.x, ptClient.y));
    int charPos = (int)(cp & 0xFFFF);
    std::wstring path = ExtractResourcePathFromCharPos(hwnd, charPos);
    if (!path.empty() && st) {
      InsertListRow(st, FormatTimeNow(), st->selectedPid, GetSelectedProcessName(st, st->selectedPid), L"SubPath", path);
      int lastIdx = ListView_GetItemCount(st->hList) - 1;
      if (lastIdx >= 0) {
        ListView_SetItemState(st->hList, lastIdx, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(st->hList, lastIdx, FALSE);
        PreviewSelectedRow(st);
      }
      return 0;
    }
    break;
  }
  }

  return g_oldPreviewTextProc ? CallWindowProcW(g_oldPreviewTextProc, hwnd, msg, wParam, lParam)
                              : DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void PreviewSelectedRow(AppState *st) {
  if (!st || !st->hList)
    return;
  try {
    int row = ListView_GetNextItem(st->hList, -1, LVNI_SELECTED);
    ClearPreviewState(st);
    if (row < 0)
      return;

    std::vector<unsigned char> bytes;
    std::wstring pathHint, sourceInfo;
    std::string ext;
    if (!ReadPreviewBytesForRow(st, row, bytes, pathHint, ext, sourceInfo)) {
      SetText(st->hPreviewText, L"Cannot read data for preview.\r\nIf this is a PathStr, wait for process PAK reads.");
      SetText(st->hPreviewInfo, L"Preview unavailable");
      return;
    }

    st->previewBytes = bytes;
    st->previewExt = ext;
    st->previewPathHint = pathHint;
    st->previewSourceInfo = sourceInfo;

    if (ext == ".wav" || ext == ".mp3" || ext == ".mid" || ext == ".ogg") {
      st->previewIsAudio = true;
      st->previewMediaTag = ext == ".mp3" ? "MP3" : (ext == ".mid" ? "MID" : "WAV");
      std::wstring text = L"Audio resource ready: " + pathHint + L"\r\nSource: " + sourceInfo +
                          L"\r\nSize: " + std::to_wstring(bytes.size()) + L" bytes\r\n\r\nClick Play below to listen.";
      SetText(st->hPreviewText, text);
      SetText(st->hPreviewInfo, L"Audio - " + std::to_wstring(bytes.size()) + L" bytes");
      EnableWindow(st->hBtnPlay, TRUE);
      EnableWindow(st->hBtnStopMedia, TRUE);
      return;
    }

    if (ext == ".spr" || IsLikelySprBufferAt(bytes.data(), bytes.size())) {
      st->previewIsSpr = true;
      st->sprCurrentFrame = 0;
      st->sprTotalFrames = GetSprFrameCountFromBytes(bytes);
      if (st->hCheckAnimation) {
        EnableWindow(st->hCheckAnimation, st->sprTotalFrames > 1 ? TRUE : FALSE);
        if (st->sprTotalFrames <= 1)
          SendMessageW(st->hCheckAnimation, BM_SETCHECK, BST_UNCHECKED, 0);
      }
      if (st->hBtnFramePrev) EnableWindow(st->hBtnFramePrev, st->sprTotalFrames > 1 ? TRUE : FALSE);
      if (st->hBtnFrameNext) EnableWindow(st->hBtnFrameNext, st->sprTotalFrames > 1 ? TRUE : FALSE);

      if (RenderSprPreviewFrame(st, 0)) {
        // Auto-play animation for multi-frame SPR (loop)
        if (st->sprTotalFrames > 1 && st->hCheckAnimation) {
          SendMessageW(st->hCheckAnimation, BM_SETCHECK, BST_CHECKED, 0);
          StartSprPreviewAnimation(st);
        }
        return;
      }
    }

    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif") {
      HBITMAP hbmp = nullptr;
      std::wstring imgInfo;
      if (LoadWicBitmapFromBytes(bytes, hbmp, imgInfo)) {
        st->hPreviewBitmap = hbmp;
        ShowWindow(st->hPreviewText, SW_HIDE);
        ShowWindow(st->hPreviewImage, SW_SHOW);
        InvalidateRect(st->hPreviewImage, nullptr, TRUE);
        SetText(st->hPreviewInfo, imgInfo + L" - " + sourceInfo);
        return;
      }
    }

    if (ext == ".tga") {
      HBITMAP hbmp = nullptr;
      std::wstring imgInfo;
      if (LoadTgaBitmapFromBytes(bytes, hbmp, imgInfo)) {
        st->hPreviewBitmap = hbmp;
        ShowWindow(st->hPreviewText, SW_HIDE);
        ShowWindow(st->hPreviewImage, SW_SHOW);
        InvalidateRect(st->hPreviewImage, nullptr, TRUE);
        SetText(st->hPreviewInfo, imgInfo + L" - " + sourceInfo);
        return;
      }
    }

    if (ext == ".txt" || ext == ".ini" || ext == ".lua" || ext == ".cfg" ||
        ext == ".log" || ext == ".xml" || ext == ".json" || ext == ".dat" ||
        LooksLikeTextBytes(bytes)) {
      std::wstring text = DecodeTextBytesForPreview(bytes);
      if (text.empty()) text = L"(empty text)";
      SetText(st->hPreviewText, text);
      SetText(st->hPreviewInfo, L"Text - " + std::to_wstring(bytes.size()) + L" bytes - " + sourceInfo);

      // Check if this is a JX1 UI INI with Image= references
      if (LooksLikeUiIniContent(bytes)) {
        UiIniLayout layout;
        if (ParseUiIniLayout(bytes, layout)) {
          std::wstring compositeInfo;
          HBITMAP hComposite = RenderUiComposite(layout, st->knownPaks, compositeInfo);
          if (hComposite) {
            st->previewHasComposite = true;
            if (st->hUiCompositeBitmap) DeleteObject(st->hUiCompositeBitmap);
            st->hUiCompositeBitmap = hComposite;

            // Show both panels
            ShowWindow(st->hPreviewText, SW_SHOW);
            ShowWindow(st->hUiComposite, SW_SHOW);

            // Update info with composite details
            SetText(st->hPreviewInfo, compositeInfo + L" - " + sourceInfo);

            // Re-layout to split the preview area
            LayoutControls(st);
            InvalidateRect(st->hUiComposite, nullptr, TRUE);
          }
        }
      }
      return;
    }

    SetText(st->hPreviewText, HexDumpPreview(bytes));
    SetText(st->hPreviewInfo, L"Binary - " + std::to_wstring(bytes.size()) + L" bytes - " + sourceInfo);
  } catch (const std::exception &ex) {
    if (st && st->hPreviewText) {
      SetText(st->hPreviewText, L"Lỗi đọc dữ liệu xem trước: " + Utf8ToWide(ex.what()));
      SetText(st->hPreviewInfo, L"Preview Error");
    }
  } catch (...) {
    if (st && st->hPreviewText) {
      SetText(st->hPreviewText, L"Lỗi đọc dữ liệu: Tệp tin không đúng định dạng hoặc bị hỏng.");
      SetText(st->hPreviewInfo, L"Preview Error");
    }
  }
}


static bool PromptForOutputFolder(HWND owner, std::wstring &outFolder) {
  outFolder.clear();
  HRESULT coHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  BROWSEINFOW bi{};
  bi.hwndOwner = owner;
  bi.lpszTitle = L"Chọn thư mục lưu file trích xuất (Unpack):";
  bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
  LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
  if (!pidl) {
    if (SUCCEEDED(coHr)) CoUninitialize();
    return false;
  }
  wchar_t path[MAX_PATH] = {};
  BOOL ok = SHGetPathFromIDListW(pidl, path);
  CoTaskMemFree(pidl);
  if (SUCCEEDED(coHr)) CoUninitialize();
  if (!ok || !path[0])
    return false;
  outFolder = path;
  return true;
}

static bool EnsureParentDirs(const std::wstring &filePath) {
  size_t slash = filePath.find_last_of(L"\\/");
  if (slash == std::wstring::npos) return true;
  std::wstring dir = filePath.substr(0, slash);
  if (dir.empty()) return true;

  std::wstring cur;
  size_t pos = 0;
  if (dir.size() >= 2 && dir[1] == L':') {
    cur = dir.substr(0, 2);
    pos = 2;
  }
  while (pos <= dir.size()) {
    size_t next = dir.find_first_of(L"\\/", pos);
    std::wstring part = dir.substr(pos, next == std::wstring::npos ? std::wstring::npos : next - pos);
    if (!part.empty()) {
      if (!cur.empty() && cur.back() != L'\\') cur += L"\\";
      cur += part;
      DWORD attr = GetFileAttributesW(cur.c_str());
      if (attr == INVALID_FILE_ATTRIBUTES) {
        if (!CreateDirectoryW(cur.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
          return false;
      }
    }
    if (next == std::wstring::npos) break;
    pos = next + 1;
  }
  return true;
}

static void BatchUnpackRows(AppState *st, bool onlySelected) {
  if (!st || !st->hList)
    return;
  int totalCount = ListView_GetItemCount(st->hList);
  if (totalCount <= 0) {
    SetText(st->hStatus, L"Không có file nào trong danh sách để trích xuất.");
    return;
  }

  std::vector<int> targetRows;
  if (onlySelected) {
    int row = -1;
    while ((row = ListView_GetNextItem(st->hList, row, LVNI_SELECTED)) >= 0) {
      targetRows.push_back(row);
    }
    if (targetRows.empty()) {
      SetText(st->hStatus, L"Hãy chọn ít nhất 1 dòng để trích xuất (Batch).");
      return;
    }
  } else {
    for (int i = 0; i < totalCount; ++i) {
      targetRows.push_back(i);
    }
  }

  std::wstring folder;
  if (!PromptForOutputFolder(st->hDlg, folder)) {
    SetText(st->hStatus, L"Đã hủy trích xuất hàng loạt.");
    return;
  }
  while (!folder.empty() && (folder.back() == L'\\' || folder.back() == L'/'))
    folder.pop_back();

  int successCount = 0;
  int failCount = 0;
  size_t totalBytesWritten = 0;

  for (size_t idx = 0; idx < targetRows.size(); ++idx) {
    int r = targetRows[idx];
    std::vector<unsigned char> bytes;
    std::wstring pathHint, sourceInfo;
    std::string ext;

    if (!ReadPreviewBytesForRow(st, r, bytes, pathHint, ext, sourceInfo) || bytes.empty()) {
      failCount++;
      continue;
    }

    std::wstring rel = pathHint;
    while (!rel.empty() && (rel[0] == L'\\' || rel[0] == L'/'))
      rel.erase(rel.begin());
    if (rel.empty()) {
      wchar_t dummyName[64] = {};
      swprintf_s(dummyName, L"file_%zu%s", idx + 1, ext.empty() ? L".bin" : Utf8ToWide(ext).c_str());
      rel = dummyName;
    }

    std::wstring outPath = folder + L"\\" + rel;
    if (!EnsureParentDirs(outPath)) {
      failCount++;
      continue;
    }

    HANDLE h = CreateFileW(outPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
      failCount++;
      continue;
    }
    DWORD written = 0;
    BOOL writeOk = WriteFile(h, bytes.data(), (DWORD)bytes.size(), &written, nullptr);
    CloseHandle(h);

    if (writeOk && written == (DWORD)bytes.size()) {
      successCount++;
      totalBytesWritten += bytes.size();
    } else {
      failCount++;
    }
  }

  wchar_t msg[512] = {};
  swprintf_s(msg, L"Trích xuất hoàn tất: Thành công %d/%zu file (%zu KB). Thất bại: %d.",
             successCount, targetRows.size(), totalBytesWritten / 1024, failCount);
  SetText(st->hStatus, msg);
}

static void UnpackSelectedRow(AppState *st) {
  if (!st || !st->hList)
    return;
  int selCount = ListView_GetSelectedCount(st->hList);
  if (selCount > 1) {
    BatchUnpackRows(st, true);
    return;
  }
  int row = ListView_GetNextItem(st->hList, -1, LVNI_SELECTED);
  if (row < 0) {
    SetText(st->hStatus, L"Chọn một dòng để Unpack.");
    return;
  }

  std::vector<unsigned char> bytes;
  std::wstring pathHint, sourceInfo;
  std::string ext;
  if (!ReadPreviewBytesForRow(st, row, bytes, pathHint, ext, sourceInfo) || bytes.empty()) {
    SetText(st->hStatus, L"Không đọc được nội dung của file đã chọn để trích xuất.");
    return;
  }

  std::wstring folder;
  if (!PromptForOutputFolder(st->hDlg, folder)) {
    SetText(st->hStatus, L"Đã hủy Unpack.");
    return;
  }
  while (!folder.empty() && (folder.back() == L'\\' || folder.back() == L'/'))
    folder.pop_back();

  std::wstring rel = pathHint;
  while (!rel.empty() && (rel[0] == L'\\' || rel[0] == L'/'))
    rel.erase(rel.begin());
  if (rel.empty())
    rel = L"extracted_file" + (ext.empty() ? L".bin" : Utf8ToWide(ext));

  std::wstring outPath = folder + L"\\" + rel;
  if (!EnsureParentDirs(outPath)) {
    SetText(st->hStatus, L"Không tạo được thư mục đích.");
    return;
  }

  HANDLE h = CreateFileW(outPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    SetText(st->hStatus, L"Không thể tạo file để ghi.");
    return;
  }
  DWORD written = 0;
  BOOL writeOk = WriteFile(h, bytes.data(), (DWORD)bytes.size(), &written, nullptr);
  CloseHandle(h);

  if (!writeOk || written != (DWORD)bytes.size()) {
    SetText(st->hStatus, L"Lỗi: Không thể ghi toàn bộ nội dung file ra ổ đĩa.");
    return;
  }

  wchar_t msg[512] = {};
  swprintf_s(msg, L"Đã trích xuất thành công: %s (%zu bytes)", rel.c_str(), bytes.size());
  SetText(st->hStatus, msg);
}

static void ExportResourcePaths(AppState *st) {
  if (!st || !st->hList)
    return;

  ScanSelectedProcessCollect(st, true, nullptr);

  std::set<std::wstring> seen;
  std::vector<std::wstring> paths;
  int count = ListView_GetItemCount(st->hList);
  for (int i = 0; i < count; ++i) {
    wchar_t buf[4096] = {};
    ListView_GetItemText(st->hList, i, 4, buf, (int)_countof(buf));
    std::wstring line = buf;
    size_t arrow = line.find(L" -> ");
    if (arrow != std::wstring::npos)
      line = line.substr(arrow + 4);
    size_t block = line.find(L" (block ");
    if (block != std::wstring::npos)
      line = line.substr(0, block);

    if (line.find(L"UID=") == 0)
      continue;
    if (seen.insert(ToLowerW(line)).second)
      paths.push_back(line);
  }

  std::sort(paths.begin(), paths.end(),
            [](const std::wstring &a, const std::wstring &b) {
              return _wcsicmp(a.c_str(), b.c_str()) < 0;
            });

  wchar_t exePath[MAX_PATH] = {};
  GetModuleFileNameW(nullptr, exePath, MAX_PATH);
  std::wstring exeDir = ParentDirOfPath(exePath);
  std::wstring outFile = exeDir + L"\\ProcessFilePath.txt";

  FILE *fp = nullptr;
  _wfopen_s(&fp, outFile.c_str(), L"wb");
  if (!fp) {
    SetText(st->hStatus, L"Cannot write ProcessFilePath.txt.");
    return;
  }
  unsigned char bom[] = {0xEF, 0xBB, 0xBF};
  fwrite(bom, 1, 3, fp);
  for (const std::wstring &p : paths) {
    std::string u8 = WideToUtf8(p) + "\r\n";
    fwrite(u8.data(), 1, u8.size(), fp);
  }
  fclose(fp);

  wchar_t status[256] = {};
  swprintf_s(status, L"Exported %zu paths to ProcessFilePath.txt", paths.size());
  SetText(st->hStatus, status);
}

static void LoadPakFileDirectly(AppState *st, const std::wstring &pakPath) {
  if (!st || pakPath.empty())
    return;

  st->knownPaks.insert(pakPath);
  AddSiblingPakFiles(pakPath, st->knownPaks);

  GUIXPackIndexInfo dummy{};
  std::vector<GUIXPackIndexInfo> indexList;

  HANDLE h = CreateFileW(pakPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    SetText(st->hStatus, L"Lỗi: Không thể mở file PAK được chỉ định.");
    return;
  }

  GUIXPackFileHeader header{};
  DWORD read = 0;
  if (!ReadFile(h, &header, sizeof(header), &read, nullptr) || read != sizeof(header) ||
      std::memcmp(header.cSignature, "PACK", 4) != 0) {
    CloseHandle(h);
    SetText(st->hStatus, L"Lỗi: File không đúng cấu trúc GUIX PAK hợp lệ.");
    return;
  }

  LARGE_INTEGER li{};
  li.QuadPart = header.uIndexTableOffset;
  if (!SetFilePointerEx(h, li, nullptr, FILE_BEGIN)) {
    CloseHandle(h);
    SetText(st->hStatus, L"Lỗi: Không tìm thấy bảng Index của file PAK.");
    return;
  }

  indexList.resize(header.uCount);
  DWORD indexBytes = (DWORD)(header.uCount * sizeof(GUIXPackIndexInfo));
  if (!ReadFile(h, indexList.data(), indexBytes, &read, nullptr) || read != indexBytes) {
    CloseHandle(h);
    SetText(st->hStatus, L"Lỗi: Không đọc được đầy đủ bảng Index của file PAK.");
    return;
  }
  CloseHandle(h);

  std::wstring pakName = FileNameOfPath(pakPath);
  std::wstring now = FormatTimeNow();
  int addedCount = 0;

  for (const auto &idxInfo : indexList) {
    std::wstring virtualPath;
    if (!LookupRuntimePath(0, idxInfo.uId, virtualPath) || virtualPath.empty()) {
      // Check if pak has internal path mapping
      std::wstring foundInternal;
      PakEntryHit hit{};
      if (ResolvePakEntryByOffset(pakPath, 0, idxInfo.uOffset, hit) && !hit.internalPath.empty()) {
        virtualPath = hit.internalPath;
      }
    }

    std::wstring displayPath;
    if (!virtualPath.empty()) {
      displayPath = virtualPath;
    } else {
      wchar_t uidBuf[64] = {};
      swprintf_s(uidBuf, L"UID=0x%08lX (block %lu)", idxInfo.uId, idxInfo.uOffset);
      displayPath = pakPath + L" -> " + uidBuf;
    }

    std::wstring key = ToLowerW(L"pak|" + displayPath);
    if (st->seenKeys.insert(key).second) {
      InsertListRow(st, now, 0, pakName, L"PAK", displayPath);
      addedCount++;
    }
  }

  wchar_t status[256] = {};
  swprintf_s(status, L"Đã nạp file PAK thành công: %s (%lu files, %d mục mới).",
             pakName.c_str(), header.uCount, addedCount);
  SetText(st->hStatus, status);
}

static void OpenPakFileDialogAndLoad(AppState *st) {
  if (!st)
    return;

  wchar_t fileBuf[MAX_PATH * 4] = {};
  OPENFILENAMEW ofn{};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = st->hDlg;
  ofn.lpstrFilter = L"Kingsoft PAK Files (*.pak)\0*.pak\0All Files (*.*)\0*.*\0";
  ofn.lpstrFile = fileBuf;
  ofn.nMaxFile = (DWORD)_countof(fileBuf);
  ofn.lpstrTitle = L"Chọn file PAK của Võ Lâm Miễn Phí (VLMP) để mở:";
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;

  if (GetOpenFileNameW(&ofn)) {
    LoadPakFileDirectly(st, fileBuf);
  }
}

static void LayoutControls(AppState *st) {
  if (!st || !st->hDlg)
    return;
  RECT rc{};
  GetClientRect(st->hDlg, &rc);
  int width = rc.right - rc.left;
  int height = rc.bottom - rc.top;

  int margin = 8;
  int top = 8;
  int btnH = 22;
  int row2Top = top + btnH + 4;
  int row3Top = row2Top + btnH + 4; // Search row
  int listTop = row3Top + btnH + 6;
  int bottomH = 28;
  int listH = height - listTop - bottomH - margin;
  if (listH < 80) listH = 80;

  int splitX = (width * 55) / 100;
  if (splitX < 440) splitX = 440;
  if (splitX > width - 260) splitX = width - 260;

  // Row 1
  SetWindowPos(st->hComboProcess, nullptr, 44, top, 250, 160, SWP_NOZORDER);
  SetWindowPos(st->hBtnRefresh, nullptr, 300, top, 56, btnH, SWP_NOZORDER);
  SetWindowPos(st->hBtnStart, nullptr, 362, top, 48, btnH, SWP_NOZORDER);
  SetWindowPos(st->hBtnStop, nullptr, 416, top, 48, btnH, SWP_NOZORDER);
  SetWindowPos(st->hBtnSnapshot, nullptr, 470, top, 64, btnH, SWP_NOZORDER);
  SetWindowPos(st->hBtnClear, nullptr, 540, top, 48, btnH, SWP_NOZORDER);
  SetWindowPos(st->hCheckAutoScroll, nullptr, 594, top + 2, 74, btnH, SWP_NOZORDER);

  int clickEditW = width - 718 - margin;
  if (clickEditW < 120) clickEditW = 120;
  SetWindowPos(GetDlgItem(st->hDlg, IDC_STATIC_CLICK_LABEL), nullptr, 674, top + 4, 36, 16, SWP_NOZORDER);
  SetWindowPos(st->hClickPointEdit, nullptr, 716, top, clickEditW, btnH, SWP_NOZORDER);

  // Row 2
  SetWindowPos(GetDlgItem(st->hDlg, IDC_STATIC_WINDOW_LABEL), nullptr, margin, row2Top + 4, 44, 16, SWP_NOZORDER);
  SetWindowPos(st->hClickWindowEdit, nullptr, 44, row2Top, 250, btnH, SWP_NOZORDER);
  SetWindowPos(GetDlgItem(st->hDlg, IDC_STATIC_RESOURCE_LABEL), nullptr, 300, row2Top + 4, 56, 16, SWP_NOZORDER);
  int resEditW = width - 362 - margin;
  if (resEditW < 160) resEditW = 160;
  SetWindowPos(st->hClickResourceEdit, nullptr, 362, row2Top, resEditW, btnH, SWP_NOZORDER);

  // Row 3: Search / Filter & Batch Unpack
  int listW = splitX - margin * 2;
  SetWindowPos(GetDlgItem(st->hDlg, IDC_STATIC_SEARCH_LABEL), nullptr, margin, row3Top + 3, 76, 16, SWP_NOZORDER);
  int searchEditW = listW - 78 - 24 - 96;
  if (searchEditW < 80) searchEditW = 80;
  SetWindowPos(st->hEditSearch, nullptr, margin + 78, row3Top, searchEditW, btnH, SWP_NOZORDER);
  SetWindowPos(st->hBtnClearSearch, nullptr, margin + 78 + searchEditW + 2, row3Top, 20, btnH, SWP_NOZORDER);
  SetWindowPos(st->hBtnUnpackBatch, nullptr, margin + 78 + searchEditW + 24, row3Top, 92, btnH, SWP_NOZORDER);

  // List & Preview
  SetWindowPos(st->hList, nullptr, margin, listTop, listW, listH, SWP_NOZORDER);

  int prevX = splitX + 4;
  int prevW = width - prevX - margin;
  SetWindowPos(st->hGroupPreview, nullptr, prevX, row3Top, prevW, listH + btnH + 6, SWP_NOZORDER);

  // Preview header controls: Animation, Frame Nav, Zoom
  SetWindowPos(st->hCheckAnimation, nullptr, prevX + 60, row3Top - 2, 70, btnH, SWP_NOZORDER);
  SetWindowPos(st->hBtnFramePrev, nullptr, prevX + 132, row3Top - 1, 26, 18, SWP_NOZORDER);
  SetWindowPos(st->hBtnFrameNext, nullptr, prevX + 160, row3Top - 1, 26, 18, SWP_NOZORDER);
  SetWindowPos(st->hComboZoom, nullptr, prevX + prevW - 114, row3Top - 2, 58, 120, SWP_NOZORDER);
  SetWindowPos(st->hBtnSheetView, nullptr, prevX + prevW - 52, row3Top - 1, 46, 18, SWP_NOZORDER);

  int prevInnerX = prevX + 8;
  int prevInnerY = row3Top + 20;
  int prevInnerW = prevW - 16;
  int prevInfoH = 34;
  int totalInnerH = (listH + btnH + 6) - prevInfoH - 28;
  if (totalInnerH < 120) totalInnerH = 120;

  if (st->previewHasComposite) {
    // Split: 40% top (text/image), 60% bottom (UI composite)
    int topH = (int)(totalInnerH * 0.4);
    if (topH < 60) topH = 60;
    int gap = 4;
    int bottomH = totalInnerH - topH - gap;
    if (bottomH < 60) bottomH = 60;

    SetWindowPos(st->hPreviewText, nullptr, prevInnerX, prevInnerY, prevInnerW, topH, SWP_NOZORDER);
    SetWindowPos(st->hPreviewImage, nullptr, prevInnerX, prevInnerY, prevInnerW, topH, SWP_NOZORDER);
    SetWindowPos(st->hUiComposite, nullptr, prevInnerX, prevInnerY + topH + gap, prevInnerW, bottomH, SWP_NOZORDER);
  } else {
    // Full height for top panel, composite hidden
    SetWindowPos(st->hPreviewText, nullptr, prevInnerX, prevInnerY, prevInnerW, totalInnerH, SWP_NOZORDER);
    SetWindowPos(st->hPreviewImage, nullptr, prevInnerX, prevInnerY, prevInnerW, totalInnerH, SWP_NOZORDER);
    SetWindowPos(st->hUiComposite, nullptr, prevInnerX, prevInnerY + totalInnerH + 4, prevInnerW, 0, SWP_NOZORDER);
  }

  int prevBottomY = prevInnerY + totalInnerH + 6;
  SetWindowPos(st->hPreviewInfo, nullptr, prevInnerX, prevBottomY, prevInnerW - 116, prevInfoH, SWP_NOZORDER);
  SetWindowPos(st->hBtnPlay, nullptr, prevInnerX + prevInnerW - 110, prevBottomY + 2, 50, btnH, SWP_NOZORDER);
  SetWindowPos(st->hBtnStopMedia, nullptr, prevInnerX + prevInnerW - 54, prevBottomY + 2, 50, btnH, SWP_NOZORDER);

  // Bottom row
  int bottomY = listTop + listH + 6;
  SetWindowPos(st->hBtnOpenFolder, nullptr, margin, bottomY, 78, btnH, SWP_NOZORDER);
  SetWindowPos(st->hBtnOpenPak, nullptr, margin + 82, bottomY, 88, btnH, SWP_NOZORDER);
  SetWindowPos(st->hBtnExport, nullptr, margin + 174, bottomY, 116, btnH, SWP_NOZORDER);
  SetWindowPos(st->hStatus, nullptr, margin + 296, bottomY + 4, width - margin * 2 - 296, 16, SWP_NOZORDER);

  if (st->hList) {
    int pathColW = listW - 74 - 58 - 138 - 82 - 20;
    if (pathColW < 220) pathColW = 220;
    ListView_SetColumnWidth(st->hList, 0, 74);
    ListView_SetColumnWidth(st->hList, 1, 58);
    ListView_SetColumnWidth(st->hList, 2, 138);
    ListView_SetColumnWidth(st->hList, 3, 82);
    ListView_SetColumnWidth(st->hList, 4, pathColW);
  }
}

static BOOL CALLBACK SetChildFontProc(HWND hwnd, LPARAM lParam) {
  SendMessageW(hwnd, WM_SETFONT, (WPARAM)lParam, TRUE);
  return TRUE;
}

static void ShowContextMenu(AppState *st, POINT pt) {
  if (!st || !st->hList)
    return;

  int row = ListView_GetNextItem(st->hList, -1, LVNI_SELECTED);
  if (row < 0)
    return;

  int selCount = ListView_GetSelectedCount(st->hList);
  int totalCount = ListView_GetItemCount(st->hList);

  HMENU hMenu = CreatePopupMenu();
  AppendMenuW(hMenu, MF_STRING, ID_MENU_COPY_PATH, L"Copy Path");
  AppendMenuW(hMenu, MF_STRING, ID_MENU_COPY_UID, L"Copy UID");
  AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(hMenu, MF_STRING, ID_MENU_UNPACK, L"Unpak Resource (Trích xuất file này)...");

  wchar_t batchSelText[128] = {};
  swprintf_s(batchSelText, L"Unpak Các File Đang Chọn (%d file)...", selCount > 0 ? selCount : 1);
  AppendMenuW(hMenu, MF_STRING, ID_MENU_UNPACK_SELECTED, batchSelText);

  wchar_t batchAllText[128] = {};
  swprintf_s(batchAllText, L"Unpak Toàn Bộ Danh Sách Đang Lọc (%d file)...", totalCount);
  AppendMenuW(hMenu, MF_STRING, ID_MENU_UNPACK_ALL_FILTERED, batchAllText);

  AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(hMenu, MF_STRING, ID_MENU_OPEN_CONTAINING_FOLDER, L"Open Containing Folder");

  UINT cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, st->hDlg, nullptr);
  DestroyMenu(hMenu);

  if (cmd == ID_MENU_COPY_PATH) {
    std::wstring path = GetSelectedListPath(st);
    if (!path.empty() && OpenClipboard(st->hDlg)) {
      EmptyClipboard();
      HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (path.size() + 1) * sizeof(wchar_t));
      if (hMem) {
        void *dst = GlobalLock(hMem);
        memcpy(dst, path.c_str(), (path.size() + 1) * sizeof(wchar_t));
        GlobalUnlock(hMem);
        SetClipboardData(CF_UNICODETEXT, hMem);
      }
      CloseClipboard();
      SetText(st->hStatus, L"Copied path to clipboard.");
    }
  } else if (cmd == ID_MENU_COPY_UID) {
    std::wstring path = GetSelectedListPath(st);
    unsigned long uid = FileNameToPackId(WideToGbk(path));
    if (uid != 0 && OpenClipboard(st->hDlg)) {
      EmptyClipboard();
      wchar_t buf[32] = {};
      swprintf_s(buf, L"0x%08lX", uid);
      HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (wcslen(buf) + 1) * sizeof(wchar_t));
      if (hMem) {
        void *dst = GlobalLock(hMem);
        memcpy(dst, buf, (wcslen(buf) + 1) * sizeof(wchar_t));
        GlobalUnlock(hMem);
        SetClipboardData(CF_UNICODETEXT, hMem);
      }
      CloseClipboard();
      SetText(st->hStatus, L"Copied UID to clipboard.");
    }
  } else if (cmd == ID_MENU_UNPACK) {
    UnpackSelectedRow(st);
  } else if (cmd == ID_MENU_OPEN_CONTAINING_FOLDER) {
    std::wstring path = GetSelectedListPath(st);
    size_t atPos = path.find(L" @");
    if (atPos != std::wstring::npos) path = path.substr(0, atPos);
    std::wstring dir = ParentDirOfPath(path);
    if (!dir.empty())
      ShellExecuteW(st->hDlg, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
  }
}

static INT_PTR CALLBACK MainDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  AppState *st = g_app;

  switch (msg) {
  case WM_INITDIALOG: {
    st = new AppState();
    st->hDlg = hwnd;
    g_app = st;

    HICON hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON));
    if (hIcon) {
      SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
      SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }

    st->hComboProcess = GetDlgItem(hwnd, IDC_COMBO_PROCESS);
    st->hBtnRefresh = GetDlgItem(hwnd, IDC_BTN_REFRESH_PROCESS);
    st->hBtnStart = GetDlgItem(hwnd, IDC_BTN_START);
    st->hBtnStop = GetDlgItem(hwnd, IDC_BTN_STOP);
    st->hBtnSnapshot = GetDlgItem(hwnd, IDC_BTN_SNAPSHOT);
    st->hBtnClear = GetDlgItem(hwnd, IDC_BTN_CLEAR);
    st->hCheckAutoScroll = GetDlgItem(hwnd, IDC_CHECK_AUTOSCROLL);
    if (st->hCheckAutoScroll) {
      SendMessageW(st->hCheckAutoScroll, BM_SETCHECK, BST_UNCHECKED, 0);
    }

    st->hClickPointEdit = GetDlgItem(hwnd, IDC_EDIT_CLICK_POINT);
    st->hClickWindowEdit = GetDlgItem(hwnd, IDC_EDIT_CLICK_WINDOW);
    st->hClickResourceEdit = GetDlgItem(hwnd, IDC_EDIT_CLICK_RESOURCE);

    st->hEditSearch = GetDlgItem(hwnd, IDC_EDIT_SEARCH_FILTER);
    st->hBtnClearSearch = GetDlgItem(hwnd, IDC_BTN_CLEAR_SEARCH);
    st->hBtnUnpackBatch = GetDlgItem(hwnd, IDC_BTN_UNPACK_BATCH);
    st->hBtnFramePrev = GetDlgItem(hwnd, IDC_BTN_FRAME_PREV);
    st->hBtnFrameNext = GetDlgItem(hwnd, IDC_BTN_FRAME_NEXT);
    st->hComboZoom = GetDlgItem(hwnd, IDC_COMBO_ZOOM);
    st->hBtnSheetView = GetDlgItem(hwnd, IDC_BTN_SHEET_VIEW);

    if (st->hComboZoom) {
      SendMessageW(st->hComboZoom, CB_ADDSTRING, 0, (LPARAM)L"100%");
      SendMessageW(st->hComboZoom, CB_ADDSTRING, 0, (LPARAM)L"200%");
      SendMessageW(st->hComboZoom, CB_ADDSTRING, 0, (LPARAM)L"400%");
      SendMessageW(st->hComboZoom, CB_SETCURSEL, 0, 0); // Default 100%
    }

    if (st->hEditSearch) {
      SendMessageW(st->hEditSearch, EM_SETCUEBANNER, TRUE, (LPARAM)L"Filter by keyword (.spr, .txt, monk, skill, script)...");
    }

    st->hList = GetDlgItem(hwnd, IDC_LIST_FILES);
    st->hGroupPreview = GetDlgItem(hwnd, IDC_GROUP_PREVIEW);
    st->hCheckAnimation = GetDlgItem(hwnd, IDC_CHECK_ANIMATION);
    st->hPreviewText = GetDlgItem(hwnd, IDC_EDIT_PREVIEW_TEXT);
    st->hPreviewImage = GetDlgItem(hwnd, IDC_STATIC_PREVIEW_IMAGE);
    st->hPreviewInfo = GetDlgItem(hwnd, IDC_STATIC_PREVIEW_INFO);
    st->hBtnPlay = GetDlgItem(hwnd, IDC_BTN_PREVIEW_PLAY);
    st->hBtnStopMedia = GetDlgItem(hwnd, IDC_BTN_PREVIEW_STOP);

    st->hBtnOpenFolder = GetDlgItem(hwnd, IDC_BTN_OPEN_FOLDER);
    st->hBtnOpenPak = GetDlgItem(hwnd, IDC_BTN_OPEN_PAK);
    st->hBtnExport = GetDlgItem(hwnd, IDC_BTN_EXPORT_PATHS);
    st->hStatus = GetDlgItem(hwnd, IDC_STATIC_STATUS);

    if (st->hPreviewImage) {
      SetWindowLongPtrW(st->hPreviewImage, GWLP_USERDATA, (LONG_PTR)st);
      st->oldPreviewImageProc = (WNDPROC)SetWindowLongPtrW(
          st->hPreviewImage, GWLP_WNDPROC, (LONG_PTR)PreviewImageSubclassProc);
    }

    if (st->hPreviewText) {
      SetWindowLongPtrW(st->hPreviewText, GWLP_USERDATA, (LONG_PTR)st);
      g_oldPreviewTextProc = (WNDPROC)SetWindowLongPtrW(
          st->hPreviewText, GWLP_WNDPROC, (LONG_PTR)PreviewTextSubclassProc);
    }

    st->hUiComposite = GetDlgItem(hwnd, IDC_STATIC_UI_COMPOSITE);
    if (st->hUiComposite) {
      SetWindowLongPtrW(st->hUiComposite, GWLP_USERDATA, (LONG_PTR)st);
      st->oldUiCompositeProc = (WNDPROC)SetWindowLongPtrW(
          st->hUiComposite, GWLP_WNDPROC, (LONG_PTR)UiCompositeSubclassProc);
      ShowWindow(st->hUiComposite, SW_HIDE);
    }

    st->hFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    EnumChildWindows(hwnd, SetChildFontProc, (LPARAM)st->hFont);

    if (st->hList) {
      ListView_SetExtendedListViewStyle(st->hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_LABELTIP);
      LVCOLUMNW col{};
      col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
      col.iSubItem = 0; col.cx = 74; col.pszText = const_cast<wchar_t *>(L"Time");
      ListView_InsertColumn(st->hList, 0, &col);
      col.iSubItem = 1; col.cx = 58; col.pszText = const_cast<wchar_t *>(L"PID");
      ListView_InsertColumn(st->hList, 1, &col);
      col.iSubItem = 2; col.cx = 138; col.pszText = const_cast<wchar_t *>(L"Process");
      ListView_InsertColumn(st->hList, 2, &col);
      col.iSubItem = 3; col.cx = 82; col.pszText = const_cast<wchar_t *>(L"Event");
      ListView_InsertColumn(st->hList, 3, &col);
      col.iSubItem = 4; col.cx = 400; col.pszText = const_cast<wchar_t *>(L"File path");
      ListView_InsertColumn(st->hList, 4, &col);
    }

    ShowWindow(st->hPreviewImage, SW_HIDE);
    ShowWindow(st->hPreviewText, SW_SHOW);
    EnableWindow(st->hBtnPlay, FALSE);
    EnableWindow(st->hBtnStopMedia, FALSE);
    EnableWindow(st->hCheckAnimation, FALSE);
    EnableWindow(st->hBtnFramePrev, FALSE);
    EnableWindow(st->hBtnFrameNext, FALSE);

    SetWindowTextW(st->hBtnOpenFolder, L"Open Folder");
    SetWindowTextW(st->hBtnOpenPak, L"Mở File PAK...");
    SetWindowTextW(st->hBtnExport, L"Lấy Danh Sách Path");
    SetWindowTextW(st->hBtnUnpackBatch, L"Batch Unpack...");

    RefreshProcessList(st);
    UpdateButtons(st);
    LayoutControls(st);
    return TRUE;
  }

  case WM_SIZE:
    if (st && !st->destroying)
      LayoutControls(st);
    return TRUE;

  case WM_TIMER:
    if (!st || st->destroying) break;
    if (wParam == kPollTimerId) {
      ScanSelectedProcessCollect(st, false, nullptr);
      return TRUE;
    }
    if (wParam == kClickTraceTimerId) {
      PollClickTrace(st);
      return TRUE;
    }
    if (wParam == kSprAnimTimerId) {
      if (!st->previewIsSpr || st->sprTotalFrames <= 1 || st->previewBytes.empty()) {
        StopSprPreviewAnimation(st);
        return TRUE;
      }
      int nextFrame = (st->sprCurrentFrame + 1) % st->sprTotalFrames;
      if (!RenderSprPreviewFrame(st, nextFrame))
        StopSprPreviewAnimation(st);
      return TRUE;
    }
    break;

  case WM_COMMAND: {
    if (!st || st->destroying) break;
    int id = LOWORD(wParam);
    int code = HIWORD(wParam);

    if (id == IDC_EDIT_SEARCH_FILTER && code == EN_CHANGE) {
      wchar_t filterBuf[512] = {};
      GetWindowTextW(st->hEditSearch, filterBuf, (int)_countof(filterBuf));
      st->currentFilter = filterBuf;
      ApplyListFilter(st);
      return TRUE;
    }
    if (id == IDC_BTN_CLEAR_SEARCH && code == BN_CLICKED) {
      if (st->hEditSearch) {
        SetWindowTextW(st->hEditSearch, L"");
      }
      return TRUE;
    }
    if (id == IDC_BTN_UNPACK_BATCH && code == BN_CLICKED) {
      int selCount = ListView_GetSelectedCount(st->hList);
      BatchUnpackRows(st, selCount > 0);
      return TRUE;
    }
    if (id == IDC_COMBO_ZOOM && code == CBN_SELCHANGE) {
      int sel = (int)SendMessageW(st->hComboZoom, CB_GETCURSEL, 0, 0);
      if (sel == 1) st->zoomMode = 2;      // 200%
      else if (sel == 2) st->zoomMode = 4; // 400%
      else st->zoomMode = 1;              // 100%
      if (st->hPreviewImage && IsWindowVisible(st->hPreviewImage)) {
        InvalidateRect(st->hPreviewImage, nullptr, TRUE);
      }
      return TRUE;
    }
    if (id == IDC_BTN_FRAME_PREV && code == BN_CLICKED) {
      if (st->previewIsSpr && st->sprTotalFrames > 1) {
        StopSprPreviewAnimation(st);
        if (st->hCheckAnimation) SendMessageW(st->hCheckAnimation, BM_SETCHECK, BST_UNCHECKED, 0);
        int prevFrame = (st->sprCurrentFrame - 1 + st->sprTotalFrames) % st->sprTotalFrames;
        RenderSprPreviewFrame(st, prevFrame);
      }
      return TRUE;
    }
    if (id == IDC_BTN_FRAME_NEXT && code == BN_CLICKED) {
      if (st->previewIsSpr && st->sprTotalFrames > 1) {
        StopSprPreviewAnimation(st);
        if (st->hCheckAnimation) SendMessageW(st->hCheckAnimation, BM_SETCHECK, BST_UNCHECKED, 0);
        int nextFrame = (st->sprCurrentFrame + 1) % st->sprTotalFrames;
        RenderSprPreviewFrame(st, nextFrame);
      }
      return TRUE;
    }
    if (id == IDC_BTN_SHEET_VIEW && code == BN_CLICKED) {
      if (st->previewIsSpr && st->sprTotalFrames > 0) {
        if (st->sprSheetMode) {
          // Toggle back to single-frame animation mode
          st->sprSheetMode = false;
          RenderSprPreviewFrame(st, 0);
          if (st->sprTotalFrames > 1 && st->hCheckAnimation) {
            SendMessageW(st->hCheckAnimation, BM_SETCHECK, BST_CHECKED, 0);
            StartSprPreviewAnimation(st);
          }
        } else {
          BuildSpriteSheetBitmap(st);
        }
      }
      return TRUE;
    }
    if (id == ID_MENU_UNPACK_SELECTED) {
      BatchUnpackRows(st, true);
      return TRUE;
    }
    if (id == ID_MENU_UNPACK_ALL_FILTERED) {
      BatchUnpackRows(st, false);
      return TRUE;
    }

    if (id == IDC_COMBO_PROCESS && code == CBN_SELCHANGE) {
      st->selectedPid = GetSelectedPid(st);
      st->lastStringScanTick = 0;
      st->seenKeys.clear();
      st->knownPaks.clear();
      st->allEvents.clear();
      ListView_DeleteAllItems(st->hList);
      ClearPreviewState(st);
      SetText(st->hStatus, L"Process changed. Snapshot or Start to monitor.");
      return TRUE;
    }
    if (id == IDC_BTN_REFRESH_PROCESS && code == BN_CLICKED) {
      RefreshProcessList(st);
      return TRUE;
    }
    if (id == IDC_BTN_START && code == BN_CLICKED) {
      st->selectedPid = GetSelectedPid(st);
      if (!st->selectedPid) {
        SetText(st->hStatus, L"Select a process first.");
        return TRUE;
      }
      st->polling = true;
      st->clickLeftWasDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
      SetTimer(hwnd, kPollTimerId, kPollIntervalMs, nullptr);
      SetTimer(hwnd, kClickTraceTimerId, kClickTraceIntervalMs, nullptr);
      ScanSelectedProcessCollect(st, true, nullptr);
      SetText(st->hStatus, L"Watching process handles, PAK offsets, runtime paths & game clicks...");
      UpdateButtons(st);
      return TRUE;
    }
    if (id == IDC_BTN_STOP && code == BN_CLICKED) {
      KillTimer(hwnd, kPollTimerId);
      KillTimer(hwnd, kClickTraceTimerId);
      st->polling = false;
      SetText(st->hStatus, L"Stopped.");
      UpdateButtons(st);
      return TRUE;
    }
    if (id == IDC_BTN_SNAPSHOT && code == BN_CLICKED) {
      ScanSelectedProcessCollect(st, true, nullptr);
      return TRUE;
    }
    if (id == IDC_BTN_CLEAR && code == BN_CLICKED) {
      st->seenKeys.clear();
      st->knownPaks.clear();
      st->allEvents.clear();
      if (st->selectedPid) ClearRuntimePathsForPid(st->selectedPid);
      ListView_DeleteAllItems(st->hList);
      ClearPreviewState(st);
      SetText(st->hStatus, L"Cleared monitor list.");
      return TRUE;
    }
    if (id == IDC_BTN_OPEN_FOLDER && code == BN_CLICKED) {
      std::wstring path = GetSelectedListPath(st);
      size_t atPos = path.find(L" @");
      if (atPos != std::wstring::npos) path = path.substr(0, atPos);
      std::wstring dir = ParentDirOfPath(path);
      if (!dir.empty()) {
        ShellExecuteW(hwnd, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        SetText(st->hStatus, L"Opened containing folder.");
      }
      return TRUE;
    }
    if (id == IDC_BTN_OPEN_PAK && code == BN_CLICKED) {
      OpenPakFileDialogAndLoad(st);
      return TRUE;
    }
    if (id == IDC_BTN_EXPORT_PATHS && code == BN_CLICKED) {
      ExportResourcePaths(st);
      return TRUE;
    }
    if (id == IDC_CHECK_ANIMATION && code == BN_CLICKED) {
      if (st->previewIsSpr && SendMessageW(st->hCheckAnimation, BM_GETCHECK, 0, 0) == BST_CHECKED)
        StartSprPreviewAnimation(st);
      else
        StopSprPreviewAnimation(st);
      return TRUE;
    }
    if (id == IDC_BTN_PREVIEW_PLAY && code == BN_CLICKED) {
      if (st->previewIsAudio && !st->previewBytes.empty()) {
        PreviewPlayMedia(st->previewBytes.data(), st->previewBytes.size(),
                         st->previewMediaTag.c_str(), false, st->previewExt.c_str());
        SetText(st->hPreviewInfo, L"Playing audio preview...");
      }
      return TRUE;
    }
    if (id == IDC_BTN_PREVIEW_STOP && code == BN_CLICKED) {
      StopPreviewMedia();
      SetText(st->hPreviewInfo, L"Audio preview stopped.");
      return TRUE;
    }
    break;
  }

  case WM_NOTIFY: {
    if (!st || st->destroying) break;
    LPNMHDR hdr = (LPNMHDR)lParam;
    if (JxTrySortListViewOnColumnClick(hdr))
      return TRUE;
    if (hdr && hdr->idFrom == IDC_LIST_FILES && hdr->code == LVN_ITEMCHANGED) {
      LPNMLISTVIEW lv = (LPNMLISTVIEW)lParam;
      if (lv && (lv->uChanged & LVIF_STATE) && (lv->uNewState & LVIS_SELECTED) && !(lv->uOldState & LVIS_SELECTED)) {
        PreviewSelectedRow(st);
        return TRUE;
      }
    }
    break;
  }

  case WM_CONTEXTMENU:
    if (st && (HWND)wParam == st->hList) {
      POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      if (pt.x == -1 && pt.y == -1) {
        int row = ListView_GetNextItem(st->hList, -1, LVNI_SELECTED);
        RECT rcItem{};
        if (row >= 0 && ListView_GetItemRect(st->hList, row, &rcItem, LVIR_BOUNDS)) {
          pt.x = rcItem.left + 12;
          pt.y = rcItem.top + 12;
          ClientToScreen(st->hList, &pt);
        } else {
          GetCursorPos(&pt);
        }
      }
      ShowContextMenu(st, pt);
      return TRUE;
    }
    break;

  case WM_CLOSE:
    HideHoverPopup();
    if (g_hHoverPopup) {
      DestroyWindow(g_hHoverPopup);
      g_hHoverPopup = nullptr;
    }
    if (g_hHoverBgBrush) {
      DeleteObject(g_hHoverBgBrush);
      g_hHoverBgBrush = nullptr;
    }
    if (st) {
      st->destroying = true;
      KillTimer(hwnd, kPollTimerId);
      KillTimer(hwnd, kClickTraceTimerId);
      KillTimer(hwnd, kSprAnimTimerId);
      StopPreviewMedia();
      if (st->hFont) DeleteObject(st->hFont);
      if (st->hPreviewBitmap) DeleteObject(st->hPreviewBitmap);
      delete st;
      g_app = nullptr;
    }
    EndDialog(hwnd, 0);
    return TRUE;
  }

  return FALSE;
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
  INITCOMMONCONTROLSEX icc{};
  icc.dwSize = sizeof(icc);
  icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&icc);

  DialogBoxW(hInstance, MAKEINTRESOURCEW(IDD_MAIN_DIALOG), nullptr, MainDlgProc);
  return 0;
}
