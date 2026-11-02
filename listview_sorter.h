#pragma once

#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <cwctype>
#include <string>
#include <vector>

#ifndef HDF_SORTUP
#define HDF_SORTUP 0x0400
#endif
#ifndef HDF_SORTDOWN
#define HDF_SORTDOWN 0x0200
#endif

struct JxListViewSortState {
  HWND hwnd = nullptr;
  int column = -1;
  bool descending = false;
};

inline std::vector<JxListViewSortState> &JxListViewSortStates() {
  static std::vector<JxListViewSortState> states;
  return states;
}

inline JxListViewSortState &JxListViewSortStateFor(HWND hwnd) {
  std::vector<JxListViewSortState> &states = JxListViewSortStates();
  for (auto &state : states) {
    if (state.hwnd == hwnd)
      return state;
  }
  JxListViewSortState state;
  state.hwnd = hwnd;
  states.push_back(state);
  return states.back();
}

inline bool JxIsReportListView(HWND hwnd) {
  if (!hwnd || !IsWindow(hwnd))
    return false;
  wchar_t cls[64] = {};
  GetClassNameW(hwnd, cls, (int)_countof(cls));
  if (_wcsicmp(cls, WC_LISTVIEWW) != 0 && _wcsicmp(cls, L"SysListView32") != 0)
    return false;
  const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
  if ((style & LVS_OWNERDATA) != 0)
    return false;
  return (style & LVS_TYPEMASK) == LVS_REPORT;
}

inline std::wstring JxListViewGetItemText(HWND list, int item, int subItem) {
  if (!list || item < 0 || subItem < 0)
    return L"";
  int capacity = 256;
  for (int attempt = 0; attempt < 5; ++attempt) {
    std::vector<wchar_t> buf((size_t)capacity, L'\0');
    LVITEMW lvi{};
    lvi.mask = LVIF_TEXT;
    lvi.iItem = item;
    lvi.iSubItem = subItem;
    lvi.pszText = buf.data();
    lvi.cchTextMax = capacity;
    if (!ListView_GetItem(list, &lvi))
      return L"";
    size_t len = wcsnlen_s(buf.data(), (size_t)capacity);
    if (len < (size_t)capacity - 1)
      return std::wstring(buf.data(), len);
    capacity *= 2;
  }
  return L"";
}

inline LPARAM JxListViewGetItemParam(HWND list, int item, LPARAM fallback = 0) {
  if (!list || item < 0)
    return fallback;
  LVITEMW lvi{};
  lvi.mask = LVIF_PARAM;
  lvi.iItem = item;
  return ListView_GetItem(list, &lvi) ? lvi.lParam : fallback;
}

inline int JxCompareDigitRuns(const wchar_t *a, size_t &ia,
                              const wchar_t *b, size_t &ib) {
  const size_t aStart = ia;
  const size_t bStart = ib;
  while (a[ia] == L'0')
    ++ia;
  while (b[ib] == L'0')
    ++ib;
  const size_t aSig = ia;
  const size_t bSig = ib;
  while (iswdigit(a[ia]))
    ++ia;
  while (iswdigit(b[ib]))
    ++ib;
  const size_t aLen = ia - aSig;
  const size_t bLen = ib - bSig;
  if (aLen != bLen)
    return aLen < bLen ? -1 : 1;
  for (size_t i = 0; i < aLen; ++i) {
    if (a[aSig + i] != b[bSig + i])
      return a[aSig + i] < b[bSig + i] ? -1 : 1;
  }
  const size_t aTotal = ia - aStart;
  const size_t bTotal = ib - bStart;
  if (aTotal != bTotal)
    return aTotal < bTotal ? -1 : 1;
  return 0;
}

inline int JxNaturalCompareNoCase(const std::wstring &a,
                                  const std::wstring &b) {
  size_t ia = 0;
  size_t ib = 0;
  while (ia < a.size() && ib < b.size()) {
    if (iswdigit(a[ia]) && iswdigit(b[ib])) {
      int cmp = JxCompareDigitRuns(a.c_str(), ia, b.c_str(), ib);
      if (cmp != 0)
        return cmp;
      continue;
    }
    wchar_t ca = (wchar_t)towlower(a[ia]);
    wchar_t cb = (wchar_t)towlower(b[ib]);
    if (ca != cb)
      return ca < cb ? -1 : 1;
    ++ia;
    ++ib;
  }
  if (ia == a.size() && ib == b.size())
    return 0;
  return ia == a.size() ? -1 : 1;
}

struct JxListViewSortContext {
  HWND list = nullptr;
  int column = 0;
  bool descending = false;
};

inline int CALLBACK JxListViewCompareItems(LPARAM left, LPARAM right,
                                           LPARAM data) {
  const JxListViewSortContext *ctx =
      reinterpret_cast<const JxListViewSortContext *>(data);
  if (!ctx || !ctx->list)
    return 0;

  const int lhs = (int)left;
  const int rhs = (int)right;
  int cmp = JxNaturalCompareNoCase(
      JxListViewGetItemText(ctx->list, lhs, ctx->column),
      JxListViewGetItemText(ctx->list, rhs, ctx->column));
  if (ctx->descending)
    cmp = -cmp;
  if (cmp != 0)
    return cmp;

  if (ctx->column != 0) {
    cmp = JxNaturalCompareNoCase(JxListViewGetItemText(ctx->list, lhs, 0),
                                 JxListViewGetItemText(ctx->list, rhs, 0));
    if (cmp != 0)
      return cmp;
  }

  LPARAM lp = JxListViewGetItemParam(ctx->list, lhs, 0);
  LPARAM rp = JxListViewGetItemParam(ctx->list, rhs, 0);
  if (lp == rp)
    return lhs < rhs ? -1 : (lhs > rhs ? 1 : 0);
  return lp < rp ? -1 : 1;
}

inline void JxListViewUpdateSortHeader(HWND list, int sortColumn,
                                       bool descending) {
  HWND header = list ? ListView_GetHeader(list) : nullptr;
  if (!header)
    return;
  const int count = Header_GetItemCount(header);
  for (int i = 0; i < count; ++i) {
    HDITEMW item{};
    item.mask = HDI_FORMAT;
    if (!Header_GetItem(header, i, &item))
      continue;
    item.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
    if (i == sortColumn)
      item.fmt |= descending ? HDF_SORTDOWN : HDF_SORTUP;
    Header_SetItem(header, i, &item);
  }
}

inline bool JxTrySortListViewOnColumnClick(NMHDR *hdr) {
  if (!hdr || hdr->code != LVN_COLUMNCLICK || !JxIsReportListView(hdr->hwndFrom))
    return false;

  HWND list = hdr->hwndFrom;
  HWND header = ListView_GetHeader(list);
  const int columnCount = header ? Header_GetItemCount(header) : 0;
  NMLISTVIEW *lv = reinterpret_cast<NMLISTVIEW *>(hdr);
  const int column = lv ? lv->iSubItem : 0;
  if (column < 0 || column >= columnCount)
    return true;

  JxListViewSortState &state = JxListViewSortStateFor(list);
  if (state.column == column) {
    state.descending = !state.descending;
  } else {
    state.column = column;
    state.descending = false;
  }

  JxListViewSortContext ctx;
  ctx.list = list;
  ctx.column = column;
  ctx.descending = state.descending;
  SendMessageW(list, WM_SETREDRAW, FALSE, 0);
  ListView_SortItemsEx(list, JxListViewCompareItems, (LPARAM)&ctx);
  SendMessageW(list, WM_SETREDRAW, TRUE, 0);
  JxListViewUpdateSortHeader(list, state.column, state.descending);
  RedrawWindow(list, nullptr, nullptr,
               RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
  return true;
}
