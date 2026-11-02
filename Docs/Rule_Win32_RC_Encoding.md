# Win32 RC File Encoding Rule

## Problem
Windows Resource Compiler (`rc.exe`) processes `.rc` files using the system's ANSI Code Page (typically CP1252 on Western systems).
Vietnamese UTF-8 characters (ă, ơ, ữ, ấ, etc.) placed directly in `.rc` files will be corrupted into Mojibake (e.g., "Mở File PAK..." → "Má»Ÿ File PAK...").

## Rule
When displaying Vietnamese or any non-ASCII Unicode text on Win32 controls (Button, Label, ComboBox, etc.):

1. **In `.rc` files:** Use only pure ASCII text as placeholder labels (e.g., `"Open PAK File..."` instead of `"Mở File PAK..."`).
2. **In `WM_INITDIALOG`:** Call `SetWindowTextW()` to set the correct Unicode (UTF-16) text at runtime.
3. **Ensure** the `.cpp` source files are compiled with the `/utf-8` flag so that `L"..."` wide string literals encode Vietnamese correctly.

## Example
```rc
// FindInProcess.rc — ASCII only
PUSHBUTTON  "Open PAK File...", IDC_BTN_OPEN_PAK, 74, 454, 72, 14
```

```cpp
// main.cpp — WM_INITDIALOG
SetWindowTextW(st->hBtnOpenPak, L"Mở File PAK...");
SetWindowTextW(st->hBtnExport, L"Lấy Danh Sách Path");
```
