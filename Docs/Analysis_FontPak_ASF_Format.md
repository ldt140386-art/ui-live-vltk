# Phân Tích font.pak & ASF Bitmap Font Format

## Kết Quả Nghiên Cứu

### font.pak chứa 6 file ASF (All bitmap fonts, KHÔNG phải TTF)

| # | UID | Size (decompressed) | Font W×H | Char Count | Loại |
|---|-----|------|----------|------|------|
| 0 | `0x0C4A456C` | 27,747 B | **16×16** | 512 | ASCII (chỉ 0x00-0x1FF) |
| 1 | `0x0F1695A7` | 2,813,148 B | **16×16** | 23,940 | Full (CJK + VN) |
| 2 | `0x310F02B4` | 19,483 B | **12×12** | 512 | ASCII |
| 3 | `0x31A50593` | 1,547,703 B | **12×12** | 23,940 | Full (CJK + VN) |
| 4 | `0x326E5D9D` | 2,175,896 B | **14×14** | 23,940 | Full (CJK + VN) |
| 5 | `0x32E8E588` | 22,905 B | **14×14** | 512 | ASCII |

### ASF Header Structure (reverse-engineered)

```
Offset  Size  Description
0x00    4     Signature: "ASF\0" (0x41, 0x53, 0x46, 0x00)
0x04    4     Total data size (bitmap data after header)
0x08    4     Number of characters (glyph entries)
0x0C    2     Cell width (W: 12, 14, or 16 pixels)
0x0E    2     Cell height (H: 12, 14, or 16 pixels)
0x10    ...   Glyph offset table: uint32[numChars] offsets into bitmap data
              (Only present when numChars > 512, i.e., Full fonts)
              For ASCII-only fonts (512 chars), offsets are all 0 → fixed grid
After offsets: Raw bitmap data (1-bit per pixel, row-major)
```

### 3 Font Sizes Available
- **12px** — Small text (chat, tooltips, tips)
- **14px** — Medium text (UI labels, button text)
- **16px** — Large text (titles, headings)

### How Game Engine Uses Fonts
1. INI file specifies `Font=14` → engine selects ASF font size 14
2. `Text=Gian Hàng` text → TCVN3 bytes → mapped to glyph index in ASF
3. Each glyph is rendered as a bitmap stamp onto the UI surface

---

## Đề Xuất Tích Hợp vào UI Composite

> [!IMPORTANT]
> **Khuyến nghị KHÔNG sử dụng ASF font trong UI Composite**. Lý do:
>
> 1. ASF là bitmap font 1-bit (đen trắng), kích thước cố định 12/14/16px — rất thô ở resolution hiện đại
> 2. Cần reverse-engineer bảng mapping TCVN3/GBK → glyph index (phức tạp)
> 3. **Tahoma/Segoe UI** Unicode font đã render Vietnamese text **đẹp hơn** và **đúng hơn** ở mọi kích thước
> 4. Mục đích UI Composite là **preview layout**, không phải pixel-perfect game emulation

> [!TIP]
> **Giải pháp thay thế**: Giữ nguyên Tahoma font nhưng **match font size** từ INI:
> - `Font=12` → `CreateFontW(-12, ...)`
> - `Font=14` → `CreateFontW(-14, ...)`
> - `Font=16` → `CreateFontW(-16, ...)`
> Điều này đã được implement trong `RenderUiComposite()` hiện tại.

## Open Question

> [!IMPORTANT]
> Bạn có muốn:
> - **A)** Giữ Tahoma Unicode (đã implement, text đẹp, đúng chính tả) ✅ Khuyến nghị
> - **B)** Tích hợp ASF bitmap font (pixel-perfect giống game, nhưng text thô, cần nhiều work)
> - **C)** Kết hợp: dùng Tahoma mặc định, nhưng thêm option "Game Font" button để toggle sang ASF
