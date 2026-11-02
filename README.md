# UI_LIVE_VLTK (FindInProcess)

Công cụ trích xuất, giám sát tiến trình và render giao diện trực tiếp (**Live UI Composite Preview**) cho dữ liệu client Võ Lâm Truyền Kỳ / Võ Lâm Miễn Phí (JX1).

---

## 🌟 Tính Năng Nổi Bật

1. **Live Process Monitor & Hooking**:
   - Tự động phát hiện và hook tiến trình game `game.exe`, quét các gói file `.pak` đang nạp trong bộ nhớ.
   - Hiển thị danh sách file đang được đọc theo thời gian thực (Live Access Log).

2. **Cross-PAK UI Composite Preview**:
   - Phân tích cú pháp các file cấu hình giao diện `*.ini` trong client game (như màn hình đăng nhập, hành trang, trạng thái nhân vật, v.v.).
   - Tự động phân giải các file ảnh `.spr` tham chiếu chéo giữa nhiều gói archive `.pak` khác nhau (`update*.pak`, `1024.pak`, `resource*.pak`).
   - Kết hợp đa tầng (Multi-layer Compositing): Nền `[Main]`, các nút bấm `[Button]`, ô nhập liệu `[Input]`, ô tích `[CheckBox]`, và các nút tab phân trang `[BtnAttribPage]`.

3. **Giải Mã Bảng Mã Tiếng Việt Độc Quyền JX1 VLMP**:
   - Tích hợp bảng ánh xạ 1-byte JX1 (`kJx1VnTable[128]`) tương thích 100% với các nguyên âm có dấu tiếng Việt trong client VLMP.
   - Hỗ trợ giải mã chính xác các thuộc tính text: `Text=`, `Label=`, `Caption=`, `Title=`, `String=`, `Name=`, `Tip=`.

4. **Nghệ Thuật Hiển Thị Kiểu Chữ & Màu Sắc Chuẩn Game**:
   - Tone màu chữ vàng hoàng kim ấm áp (`#FFE246` / `RGB(255, 226, 70)`).
   - Tự động áp dụng kích cỡ font chuẩn (`Font=12`, `Font=14`, `Font=16`) theo quy chuẩn `font.pak`.
   - Căn lề tự động đa chiều (`HAlign`, `VAlign`) kết hợp offset chữ trên nút bấm (`LabelXOffset`, `LabelYOffset`).

---

## 🛠 Hướng Dẫn Biên Dịch (Build)

### Yêu Cầu Môi Trường
- **Hệ Điều Hành:** Windows 10 / 11 (x64)
- **Công Cụ:** Visual Studio 2022 (MSVC v143 toolset)
- **Windows SDK:** Windows 10/11 SDK

### Lệnh Biên Dịch Nhanh (MSBuild)
```powershell
MSBuild.exe FindInProcess.sln /p:Configuration=Release /p:Platform=x64 /v:minimal
```
File thực thi sau khi biên dịch sẽ nằm tại: `bin\x64\Release\FindInProcess.exe`.

---

## 📁 Cấu Trúc Dự Án

```
├── .agents/                    # Cấu hình & quy tắc Antigravity AI Assistant
│   └── rules/                  # Quy tắc phân tích định dạng JX1 PAK & UI INI
├── Docs/                       # Tài liệu nghiên cứu & giải mã kỹ thuật
│   ├── Giai_Ma_Encoding_Tieng_Viet_JX1_VLMP.md
│   ├── Analysis_FontPak_ASF_Format.md
│   └── Walkthrough_CrossPAK_UI_Composite.md
├── ucl/                        # Thư viện giải nén UCL / NRV algorithm
├── main.cpp                    # Khung cửa sổ Win32, ListView & vòng lặp sự kiện
├── pak_engine.h/.cpp           # Engine đọc file PAK, hash UID FileNameToPackId
├── spr_decoder.h/.cpp          # Bộ giải mã định dạng ảnh SPR sang HBITMAP
├── ui_ini_renderer.h/.cpp      # Bộ parser INI đa tầng & vẽ UI Composite
├── media_preview.h/.cpp        # Xem trước nội dung văn bản, âm thanh, ảnh
└── process_monitor.h/.cpp      # Giám sát I/O tiến trình game
```

---

## 📜 Giấy Phép & Tác Quyền
Dự án được phát triển phục vụ mục đích nghiên cứu cấu trúc dữ liệu và hỗ trợ cộng đồng làm UI cho game JX1.
Repository: [ldt140386-art/UI_LIVE_VLTK](https://github.com/ldt140386-art/UI_LIVE_VLTK)
