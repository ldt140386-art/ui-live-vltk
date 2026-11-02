# FindInProcess – Báo Cáo Phân Tích Toàn Diện

> **Dự án:** `FindInProcess` — Native Win32 C++ Tool  
> **Stack:** C++17, Win32 API, MSVC v143 / v145, Static Linked (/MT)  
> **Mục đích:** Giám sát thời gian thực, đọc bộ nhớ, giải mã tài nguyên PAK / SPR / Graphic / Media của Client Võ Lâm Truyền Kỳ (JX1 / VLMP).

---

## 1. TỔNG QUAN HỆ THỐNG

`FindInProcess` là công cụ Native Win32 hiệu năng cao được phát triển để:
- **Theo dõi Handles (File/Process):** Bắt trực tiếp các handle file mở qua Windows Native API (`NtQuerySystemInformation`, `NtQueryInformationFile`).
- **Memory String Scanner:** Quét vùng nhớ Commit của game process tìm các chuỗi đường dẫn tài nguyên (ANSI / GBK / UTF-16).
- **PAK Engine:** Đọc trực tiếp cấu trúc file PAK (GUIX Pack format), giải nén UCL / Frame UCL / BZIP2.
- **SPR & Media Preview:** Giải mã ảnh SPR đa tầng (Palette 24-bit, Alpha 5-bit/8-bit, Animation), nạp ảnh WIC / TGA, phát audio WAV/MP3/MID.
- **Click Tracing:** Nhận diện cửa sổ game, tọa độ click và tìm nhanh các tài nguyên vừa được nạp.
- **Unpack Utility:** Trích xuất file trực tiếp từ bộ nhớ hoặc từ gói PAK ra ổ đĩa.

---

## 2. BẢN ĐỒ KIẾN TRÚC MÃ NGUỒN

```
FindInProcess/
├── main.cpp            – Quản lý giao diện Dialog, Event loop, Subclassing, Hover Tooltip, Timer
├── pak_engine.h/.cpp   – Cấu trúc PAK, băm UID (FileNameToPackId), bộ đệm Index, Virtual path resolution
├── process_monitor.h/.cpp – Enum tiến trình, truy vấn Native Handles qua NTDLL, quét chuỗi RAM, phân giải click
├── spr_decoder.h/.cpp  – Parser SPR header, Palette color, Frame decompression, vẽ DIB Section
├── media_preview.h/.cpp – Nạp ảnh qua WIC (PNG/JPG/BMP/GIF), TGA 24/32-bit, MCI Audio player
├── listview_sorter.h   – Sắp xếp đa cột thông minh cho SysListView32
├── resource.h          – Định danh Control IDs & Dialog Resources
├── ucl/                – Thư viện giải nén UCL (NRV2B / NRV2D / NRV2E)
└── Docs/               – Tài liệu phân tích, quy trình phát triển và báo cáo fix bug
```

---

## 3. PHÂN TÍCH CHUYÊN SÂU TỪNG MODULE

### 3.1. Process Monitor (`process_monitor.cpp`)
- Sử dụng `NtQuerySystemInformation(SystemExtendedHandleInformation = 64)` để lấy danh sách handles trong toàn bộ hệ điều hành, lọc theo target `PID`.
- Nhân bản handle từ target process bằng `DuplicateHandle()` với cờ `DUPLICATE_SAME_ACCESS`.
- Dùng `NtQueryInformationFile` truy vấn `FilePositionInformation` để lấy offset con trỏ đọc hiện tại trong file PAK.
- Quét vùng nhớ `VirtualQueryEx` theo từng chunk 256KB có 512-byte overlap để không bỏ sót chuỗi nằm ở ranh giới buffer.

### 3.2. PAK Engine (`pak_engine.cpp`)
- **Thuật toán băm UID chuẩn GUIX:**
  ```cpp
  unsigned long id = 0;
  for (int i = 0; buf[i]; ++i) {
    signed char c = (signed char)buf[i];
    id = (id + (unsigned long)((i + 1) * (int)c)) % 0x8000000bUL * 0xffffffefUL;
  }
  return (id ^ 0x12345678UL);
  ```
- **Hệ thống biến thể đường dẫn (`AddPathStringVariants`):** Sinh tự động hơn 100 dạng prefix chuẩn (`\settings\`, `\ui\`, `\spr\`, `\script\`, ...) giúp tìm kiếm chính xác UID kể cả khi chuỗi bộ nhớ bị cắt ngắn.
- **Cache Index:** Đọc header `GUIXPackFileHeader` và bảng `GUIXPackIndexInfo`, sắp xếp theo offset để hỗ trợ binary search vị trí file.

### 3.3. SPR Decoder (`spr_decoder.cpp`)
- Hỗ trợ đầy đủ các định dạng SPR:
  1. *Uncompressed Standard SPR*
  2. *Frame-compressed SPR* (mỗi frame nén UCL riêng lẻ)
  3. *Fragmented SPR* (nén phân đoạn)
- Xử lý Alpha kênh: Nhận diện chuẩn 5-bit alpha (0-32) và nội suy lên 8-bit (0-255) cho hàm `AlphaBlend` của Windows GDI.

---

## 4. QUY TRÌNH HOẠT ĐỘNG (WORKFLOW)

```
[Chọn Process Game] 
       │
       ▼
[Nhấn Start / Snapshot]
 ├──► Quét File Handles qua NTDLL ──► Xác định Offset trong .PAK ──► Khớp UID/Index
 └──► Quét chuỗi bộ nhớ RAM      ──► Chuẩn hóa Path & Tính UID
       │
       ▼
[Hiển thị lên ListView]
 ├──► Hover chuột / Click dòng   ──► Giải mã Data (SPR/Image/Text/Audio) ──► Hiện Preview
 └──► Chuột phải -> Unpack       ──► Trích xuất ghi file ra ổ đĩa
```
