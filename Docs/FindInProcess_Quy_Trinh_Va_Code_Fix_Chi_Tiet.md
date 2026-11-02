# FindInProcess – Chi Tiết Quá Trình Fix Bug, Mã Nguồn Thay Đổi & Quy Trình Thực Hiện

> **Thời gian thực hiện:** 2026-09-01  
> **Dự án:** `FindInProcess` (Win32 C++ Native)  
> **Mục tiêu:** Khắc phục triệt để các bug nghiêm trọng (GDI leak, COM apartment leak, Partial I/O read/write) và cảnh báo đồng bộ đa luồng mà không làm phát sinh lỗi mới.

---

## 1. QUY TRÌNH THỰC HIỆN (STEP-BY-STEP WORKFLOW)

1. **Khảo sát & Phân tích nguyên nhân gốc rễ:**
   - Soát xét từng luồng gọi hàm từ tương tác UI (`main.cpp`), nạp PAK (`pak_engine.cpp`), quét tiến trình (`process_monitor.cpp`) đến bộ giải mã (`spr_decoder.cpp`, `media_preview.cpp`).
   - Khoanh vùng các điểm thiếu kiểm tra dung lượng I/O và rò rỉ Handle hệ thống.

2. **Thiết kế giải pháp an toàn (Zero-Side-Effects Patching):**
   - Đảm bảo tính đóng gói và cân bằng tài nguyên (Symmetric resource allocation & deallocation).
   - Bảo toàn luồng xử lý và dữ liệu trả về của các API sẵn có.

3. **Thực thi và Kiểm tra đối chiếu (Code Review & Audit):**
   - Kiểm tra logic từng nhánh mã nguồn (success path, error path, early return).
   - Bảo đảm an toàn luồng dữ liệu (Thread-Safety).

---

## 2. DANH SÁCH BUG VÀ MÃ NGUỒN FIX CHI TIẾT

### 🔴 BUG 1: Rò rỉ GDI Handle Brush và Đăng ký Window Class lặp lại
- **File:** `main.cpp`
- **Nguyên nhân:** Hàm `EnsureHoverPopupCreated` gọi `CreateSolidBrush(RGB(28, 28, 30))` mỗi lần rê chuột mà không lưu handle lại để giải phóng, gây cạn kiệt bảng GDI Handle của Windows khi dùng lâu dài. Đồng thời gọi `RegisterClassExW` lặp lại không cần thiết.
- **Mã nguồn đã sửa:**
```cpp
// main.cpp: Khai báo brush tĩnh dùng chung
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

// main.cpp: Giải phóng GDI Brush trong sự kiện WM_CLOSE
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
  // ...
```

---

### 🔴 BUG 2: Đọc file đĩa không kiểm tra dung lượng thực đọc (`ReadFile` Partial Read)
- **File:** `main.cpp`
- **Nguyên nhân:** Khi đọc file từ đĩa trong `ReadPreviewBytesForRow`, code gọi `ReadFile` nhưng không kiểm tra trạng thái trả về và số byte thực đọc `rd == outBytes.size()`.
- **Mã nguồn đã sửa:**
```cpp
// main.cpp: ReadPreviewBytesForRow()
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
```

---

### 🔴 BUG 3: Rò rỉ COM Apartment trong `PromptForOutputFolder`
- **File:** `main.cpp`
- **Nguyên nhân:** Khi `SHGetPathFromIDListW` trả về lỗi hoặc chuỗi đường dẫn rỗng, hàm return ngay mà không gọi `CoUninitialize()`.
- **Mã nguồn đã sửa:**
```cpp
// main.cpp: PromptForOutputFolder()
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
  if (SUCCEEDED(coHr)) CoUninitialize(); // Luôn giải phóng COM trước khi kiểm tra thoát
  if (!ok || !path[0])
    return false;
  outFolder = path;
  return true;
}
```

---

### 🔴 BUG 4: Thiếu kiểm tra toàn vẹn khi trích xuất file (`WriteFile` Verification)
- **File:** `main.cpp`
- **Nguyên nhân:** Hàm `UnpackSelectedRow` gọi `WriteFile` mà không kiểm tra xem toàn bộ buffer có được ghi thành công xuống đĩa hay không.
- **Mã nguồn đã sửa:**
```cpp
// main.cpp: UnpackSelectedRow()
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
```

---

### 🟡 BUG 5 / CẢNH BÁO: Đồng bộ hóa an toàn luồng cho Cache đường dẫn động
- **File:** `pak_engine.cpp`
- **Nguyên nhân:** `g_runtimePathMaps` được gọi từ nhiều nhánh (Memory scan, Handle event) nhưng chưa có mutex bảo vệ.
- **Mã nguồn đã sửa:**
```cpp
// pak_engine.cpp: Bảo vệ g_runtimePathMaps bằng g_runtimePathMutex
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
```

---

## 3. BÀI HỌC KINH NGHIỆM ĐỂ TRÁNH LỖI PHÁT SINH (/LEARN)

1. **Quản lý tài nguyên GDI và COM:** Luôn tuân thủ cơ chế RAII hoặc đối xứng chặt chẽ giữa hàm tạo (`Create*`, `CoInitialize*`) và hàm hủy (`DeleteObject`, `CoUninitialize`) trên mọi nhánh return.
2. **Xác thực dữ liệu I/O nhị phân:** Luôn so khớp `BytesRead == ExpectedBytes` và `BytesWritten == TotalBytes` để ngăn ngừa lỗi đọc/ghi dữ liệu bị cắt ngắn (partial transfer).
3. **Đồng bộ hóa Concurrency:** Mọi cấu trúc dữ liệu lưu cache dùng chung (global / static container) đều phải được bảo vệ bởi khóa `std::mutex` hoặc `std::shared_mutex` để tránh crash do xung đột dữ liệu đa luồng.
