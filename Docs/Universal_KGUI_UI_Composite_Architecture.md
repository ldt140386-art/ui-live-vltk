# Universal KGUI & Cross-PAK UI Composite Engine Architecture

## 1. Tổng Quan Hệ Thống Giao Diện JX1 (Kingsoft KGUI)

Hệ thống giao diện người dùng của JX1 Võ Lâm Truyền Kỳ (Kingsoft 2D Engine) được cấu hình thông qua các tệp INI chứa các đối tượng điều khiển `KWnd`. Mỗi section trong file INI tương ứng với một control cụ thể kế thừa từ `KWndWindow`.

```
                  +--------------------------+
                  |    KWndWindow ([Main])   |
                  +--------------------------+
                               |
       +-----------------------+-----------------------+
       |                       |                       |
+--------------+        +--------------+        +--------------+
|  KWndImage   |        |  KWndButton  |        |   KWndText   |
| (Decorations)|        |  (Clickable) |        | (Static Text)|
+--------------+        +--------------+        +--------------+
       |                       |                       |
+--------------+        +--------------+        +--------------+
| KWndCheckBox |        |   KWndPage   |        |   KWndEdit   |
|(StatusImage) |        | (Multi-Tab)  |        | (Input Box)  |
+--------------+        +--------------+        +--------------+
```

---

## 2. Bảng Phân Loại & Quy Tắc Render Tự Động (Auto-Rules)

| Kiểu Control | Tiêu Đề Section | Thuộc Tính Hình Ảnh | Thuộc Tính Văn Bản | Quy Tắc Căn Chỉnh & Dựng Hình |
|---|---|---|---|---|
| **Root Canvas** | `[Main]` | `Image=`, `BgImage=` | N/A | Xác định kích thước canvas và vẽ hình nền toàn khung. |
| **Bảng Nền & Trang Trí** | `[Img*]`, `[Pic*]`, `[Bg*]` | `Image=`, `Trans=1` | N/A | Dựng đồ họa SPR nền phụ với chế độ AlphaBlend trong suốt. |
| **Nút Bấm** | `[Btn*]`, `[Close]`, `[Ok]` | `Image=`, `Up=0` | `Label=`, `Text=`, `Font=12/14` | Căn giữa chữ cả ngang lẫn dọc `(DT_CENTER \| DT_VCENTER)`. Màu vàng `#FFE246`. |
| **Tab Phân Trang** | `[Btn*Page]`, `[Tab*]` | `Image=\spr\Ui4\common\分页.spr` | `Label=`, `SelectColor=` | Lọc trùng vị trí (Bounding-Box Overlap), chỉ vẽ tab active `InitPage=0`. |
| **Ô Tích Checkbox** | `[CheckBox*]` | `StatusImage=`, `ImageWidth=14` | `Label=`, `Font=14` | Vẽ icon 14×14 tại `(Left, Top)`. Nhãn chữ tự động dịch sang phải `Left + 18px`. |
| **Nhãn Văn Bản** | `[Txt*]`, `[Title]` | N/A | `Text=`, `Font=14/16`, `Color=` | Bật **`DT_NOCLIP`** tránh cắt cụt chữ tiếng Việt khi `Width` hẹp. Màu vàng hoàng kim. |
| **Hộp Nhập Liệu** | `[Account]`, `[Edit*]` | N/A | `BorderColor=`, `Type=1/2` | Vẽ khung viền hộp nhập `BorderColor` và căn lề `HAlign=1`. |
| **Vùng Danh Sách** | `[ListBox*]` | N/A | `MsgColor=`, `SelBgColor=` | Hiển thị nền mờ danh sách và các phân cách dòng `RowInterval`. |
| **Thanh Cuộn** | `[ScrollBar*]` | `Image=\Spr\Ui3\...\通用拖动条.spr` | N/A | Vẽ rãnh trượt `SlideBegin..SlideEnd` và nút trượt slider. |
| **Ô Chứa Vật Phẩm**| `[ObjBox*]`, `[ItemBox*]` | N/A | `Width=60, Height=60` | Vẽ khung ô lưới chứa trang bị / item. |

---

## 3. Thứ Tự Phân Tầng Render (Layered Composition Pipeline)

Để đảm bảo các thành phần đồ họa và văn bản hiển thị lớp lang chuẩn xác như trong game:
1. **Lớp 1 (Layer 0 - Root Background):** Vẽ hình nền chính từ section `[Main]`.
2. **Lớp 2 (Layer 1 - Panels & Decors):** Vẽ các bảng nền phụ `[Img*]`, `[Bg*]`, `[Pic*]`.
3. **Lớp 3 (Layer 2 - Functional Elements):** Vẽ các ô vật phẩm `[ObjBox*]`, danh sách `[ListBox*]`, thanh trượt `[ScrollBar*]`.
4. **Lớp 4 (Layer 3 - Interactive Controls):** Vẽ hình ảnh nút bấm `[Btn*]`, tab `[Btn*Page]`, ô tích `[CheckBox*]` (icon `StatusImage`).
5. **Lớp 5 (Layer 4 - Text Labels & Typography):** Vẽ toàn bộ nhãn chữ tiếng Việt, tiêu đề cửa sổ, nhãn nút và chữ checkbox lên lớp trên cùng với `DT_NOCLIP` và font `Segoe UI` sắc nét.
