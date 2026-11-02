# Giải Mã Bảng Mã Tiếng Việt JX1 VLMP & Tích Hợp UI Composite

## 1. Tổng Quan Vấn Đề
Trước đây, giao diện UI Composite và khung Preview Text của công cụ `FindInProcess` gặp lỗi hiển thị sai tiếng Việt (hiển thị ký tự rác hoặc tiếng Trung GBK như `*M藅 m?c莕 ph秈 c?輙...*`). 
Sau khi trích xuất dữ liệu nhị phân trực tiếp từ `1024.pak` (ví dụ file INI đăng nhập `UID=0x1A71EC0D`), chúng ta đã phân tích và khám phá ra bảng mã ký tự 1 byte đặc thù của JX1 Võ Lâm Miễn Phí (VLMP).

---

## 2. Bảng Mã Ký Tự JX1 VLMP (0x80 - 0xFF)
Các file INI trong client JX1 VLMP sử dụng bảng mã 1-byte mở rộng:
- Các byte `< 0x80`: Ký tự ASCII chuẩn (a-z, A-Z, 0-9, dấu cách, v.v.).
- Các byte `0x80 - 0xFF`: Đại diện trực tiếp cho các nguyên âm có dấu trong tiếng Việt theo bảng ánh xạ:

| Hex | Ký tự | Mã Unicode | Ví dụ từ thực tế trong 1024.pak |
| :---: | :---: | :---: | :--- |
| `0xA7` | **Đ** | `U+0110` | Đăng, Đồng, Đổi, Đóng |
| `0xA8` | **ă** | `U+0103` | Đăng nhập, năng |
| `0xA9` | **â** | `U+00E2` | Trân, Nhân, Thân |
| `0xAA` | **ê** | `U+00EA` | Tên, Thêm, tiêu, lên |
| `0xAB` | **ô** | `U+00F4` | nuôi, Thông |
| `0xAC` | **ơ** | `U+01A1` | (nguyên âm ơ) |
| `0xAD` | **ư** | `U+01B0` | được, thưởng, lượng |
| `0xAE` | **đ** | `U+0111` | được, điểm, hoạt động |
| `0xB5` | **à** | `U+00E0` | Hàng, tài, ngày, bàn, quà |
| `0xB6` | **ả** | `U+1EA3` | khoản, phải, ảo, Sảnh |
| `0xB7` | **ã** | `U+00E3` | Mã |
| `0xB8` | **á** | `U+00E1` | bán, Giá, Các, trái, xác |
| `0xB9` | **ạ** | `U+1EA1` | hoạt, Lại, bạn, trạng, nạm |
| `0xBB` | **ằ** | `U+1EB1` | Bằng Hữu |
| `0xBE` | **ắ** | `U+1EAF` | Bắt đầu |
| `0xC6` | **ặ** | `U+1EB7` | đặt, tặng |
| `0xC7` | **ầ** | `U+1EA7` | cần, đầu, phần |
| `0xC8` | **ẩ** | `U+1EA9` | ẩn, phẩm |
| `0xCA` | **ấ** | `U+1EA5` | nhất, cấp |
| `0xCB` | **ậ** | `U+1EAD` | nhập, nhận, Mật, Vật, Luận |
| `0xCE` | **ẻ** | `U+1EBB` | Thẻ |
| `0xD2` | **ề** | `U+1EC1` | Tiền |
| `0xD3` | **ể** | `U+1EC3` | thể, điểm |
| `0xD5` | **ế** | `U+1EBF` | Chiến, Phiếu, Kiếm, Thiết, Tiết |
| `0xD6` | **ệ** | `U+1EC7` | nghệ, Hiệp, kiệm, hiện, Kiện |
| `0xDD` | **í** | `U+00ED` | ít, phím, Tích |
| `0xDE` | **ị** | `U+1ECB` | Định Giá |
| `0xE1` | **ỏ** | `U+1ECF` | bỏ, nhỏ, Giỏ |
| `0xE2` | **õ** | `U+00F5` | Võ nghệ |
| `0xE3` | **ó** | `U+00F3` | có, Đóng, khóa |
| `0xE4` | **ọ** | `U+1ECD` | Chọn |
| `0xE5` | **ồ** | `U+1ED3` | Đồng ý |
| `0xE6` | **ổ** | `U+1ED5` | Đổi, Tổng |
| `0xE7` | **ỗ** | `U+1ED7` | Mỗi |
| `0xE8` | **ố** | `U+1ED1` | số, muốn |
| `0xE9` | **ộ** | `U+1ED9` | chuột, động |
| `0xEA` | **ờ** | `U+1EDD` | Thời, Lời |
| `0xEB` | **ở** | `U+1EDF` | mở, thưởng |
| `0xED` | **ớ** | `U+1EDB` | mới, nhớ |
| `0xEE` | **ợ** | `U+1EE3` | được, lượng |
| `0xEF` | **ù** | `U+00F9` | dùng |
| `0xF1` | **ủ** | `U+1EE7` | Hủy |
| `0xF2` | **ũ** | `U+0169` | lũy, Mã cũ |
| `0xF3` | **ú** | `U+00FA` | thú nuôi |
| `0xF4` | **ụ** | `U+1EE5` | dụng |
| `0xF5` | **ừ** | `U+1EEB` | từ bàn phím |
| `0xF6` | **ử** | `U+1EED` | Sửa đổi, Sử dụng |
| `0xF7` | **ữ** | `U+1EEF` | chữ số, Hữu |
| `0xF8` | **ứ** | `U+1EE9` | Tin tức |
| `0xF9` | **ự** | `U+1EF1` | Sự kiện, trực, ký tự, Tinh lực |
| `0xFA` | **ỳ** | `U+1EF3` | Kỳ Trân |
| `0xFC` | **ỹ** | `U+1EF9` | Kỹ năng |
| `0xFD` | **ý** | `U+00FD` | Đồng ý, ký tự |

---

## 3. Kết Quả Giải Mã Thực Tế
- **Chuỗi Tip Đăng Nhập (`0x1A71EC0D`):**
  - *Raw*: `2A 4D CB 74 20 6D B7 20 63 C7 6E 20 70 68 B6 69 20 63 E3 20 DD 74 20 6E 68 CA 74 20 33 20 6B FD 20 74 F9 20 AE AD EE 63 20 6E 68 CB 70 20 74 F5 20 62 B5 6E 20 70 68 DD 6D 20 B6 6F 2A`
  - *Giải mã*: `*Mật mã cần phải có ít nhất 3 ký tự được nhập từ bàn phím ảo*`
- **Đoạn văn hoạt động (`0x223DA71C`):**
  - *Giải mã*: `Mỗi ngày đăng nhập tham gia hoạt động có thể nhận được thưởng điểm danh, Kỳ Trân Các có bán Thẻ Sự Kiện, sau khi dùng có thể mở khóa phần thưởng thẻ quà tặng.`

---

## 4. Tích Hợp Vào Source Code
1. `ui_ini_renderer.cpp`:
   - Thay thế toàn bộ bảng mã cũ bằng `kJx1VnTable[128]`.
   - Cung cấp `DecodeVnSingleByteString` và `DecodeIniTextWithVietnamese`.
   - Cập nhật `ParseUiIniLayout`: Tự động nhận diện đầy đủ các key chứa text tiếng Việt: `Text=`, `Label=`, `Caption=`, `Title=`, `String=`, `Name=`, `Tip=`.
   - Hỗ trợ đầy đủ các thuộc tính toạ độ và căn chỉnh nâng cao: `LabelXOffset`, `LabelYOffset`, `HAlign` (Trái/Giữa/Phải), `VAlign` (Trên/Giữa/Dưới).
   - Hỗ trợ nút tab đa tầng (kết hợp nền SPR `分页.spr` với nhãn text tiếng Việt `Thuộc tính`, `Trang bị`, `Đánh giá`, `Danh hiệu`, `Kinh mạch`).
   - **Chuẩn hóa màu chữ giao diện**: Chuyển toàn bộ chữ sang sắc độ **vàng hoàng kim ấm áp** `RGB(255, 226, 70)` (`#FFE246`) hoặc màu `Color=`, `SelectColor=` chỉ định trong file INI.
   - **Loại bỏ bóng đổ 1px**: Nét chữ thanh thoát, tự nhiên và sắc nét trên nền tối.
   - **Kích thước Font chính xác**: Tự động nhận diện và áp dụng chính xác cỡ font từ INI (`Font=12`, `Font=14`, `Font=16`) theo quy chuẩn `font.pak`.
2. `media_preview.cpp`:
   - `DecodeTextBytesForPreview`: Tự động nhận diện file UI INI và áp dụng `DecodeIniTextWithVietnamese`, giúp khung hiển thị Text bên trái đọc được toàn bộ văn bản tiếng Việt chuẩn xác thay vì ký tự Trung Quốc rác.
3. Binary Build:
   - Đã biên dịch hoàn tất bản **x64 Release** tại `E:\Jx1\Source_unpak UI_Live_VLMP\bin\x64\Release\FindInProcess.exe`.
