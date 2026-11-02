# CHI TIẾT SÂU VỀ HỆ THỐNG SKILL TRÙNG SINH & KỸ NĂNG MÔN PHÁI (VLMP / JX1)

> **Tài liệu:** Cấu trúc chi tiết Kỹ năng Trùng Sinh, Tên Skill, Animation SPR, File cấu hình Settings, Script gắn Skill, và Toàn bộ Danh sách Kỹ năng Mới theo Môn Phái.  
> **Áp dụng:** Võ Lâm Miễn Phí (VLMP) & JX1 Client/Server Structure.

---

## PHẦN 1: CÁC SKILL ĐẶC QUYỀN SAU CÁC LẦN TRÙNG SINH (TS 1 - TS 9)

### 1. Kỹ năng Trùng Sinh 3: **Quan Tinh Luyện Thần**
- **Tên kỹ năng:** Quan Tinh Luyện Thần (Bị động / Tự kích hoạt).
- **Cơ chế hoạt động:**
  - Tăng vĩnh viễn Sinh Lực (Dương) cơ bản (+5.000 đến hàng chục nghìn điểm tùy cấp độ nhân vật).
  - Gia tăng % Công Kích Kỹ Năng (Skill Attack Bonus).
  - Tự động tăng cấp độ kỹ năng theo đẳng cấp của nhân vật.
- **Vị trí icon/spr:**
  - Icon giao diện F5: `\spr\ui3\skillicon\quantinhluyenthan.spr` (hoặc `\spr\ui\skillicon\reborn_skill_03.spr`)
  - SPR Hiệu ứng hào quang (Cast/Buff SPR): `\spr\skill\buff\quantinh_aura.spr`

---

### 2. Kỹ năng Trùng Sinh 4: **Kỹ Năng Trùng Sinh 4 (Bị Động Công Thủ)**
- **Tên kỹ năng:** Kỹ năng Trùng Sinh 4 (Hộ Thể Khí Kình / Kim Cang Bất Hoại).
- **Cơ chế hoạt động:**
  - Tăng % Kháng Tất Cả và tăng Sinh Lực tối đa.
  - Tăng chỉ số Công Kích Kỹ Năng và bỏ qua một phần né tránh của đối phương.
  - Cấp độ kỹ năng tự động thăng tiến tương ứng với cấp bậc của nhân vật.
- **Vị trí icon/spr:**
  - Icon F5: `\spr\ui3\skillicon\reborn_skill_04.spr`
  - SPR Effect quanh thân: `\spr\skill\buff\reborn4_shield.spr`

---

### 3. Kỹ năng Trùng Sinh 5: **Lựa chọn 1 trong 2 Tuyệt Kỹ Đỉnh Cao**
Người chơi khi đạt Trùng Sinh 5 sẽ đến **Bắc Đẩu Lão Nhân** (Minh Nguyệt Trấn) để chọn 1 trong 2 nhánh kỹ năng sau:

#### Nhánh A: **Đại Diệp Khí Tráo** (Thiên về Phòng Thủ / Hấp Thụ)
- **Cơ chế:**
  - Tạo một lớp khiên khí kình hộ thể đặc biệt giúp **hấp thụ một lượng lớn sát thương** trực tiếp (cả sát thương vật lý và sát thương ngũ hành nội công).
  - Khi lớp khí tráo bị đánh vỡ, tự động giảm thời gian trúng độc, thời gian làm chậm, làm choáng.
- **Tài nguyên SPR:**
  - Icon: `\spr\ui3\skillicon\daidiep_khitrao.spr`
  - Cast SPR: `\spr\skill\buff\daidiep_cast.spr`
  - State Loop SPR (Khiên xoay quanh người): `\spr\skill\buff\daidiep_loop.spr`

#### Nhánh B: **Trọng Kích Tuyệt Đối** (Thiên về Tấn Công / Bạo Kích)
- **Cơ chế:**
  - Tạo xác suất xuất hiện đòn đánh **Chí Mạng Tuyệt Đối (True Critical Damage)**, bỏ qua một phần kháng tính và phòng ngự của mục tiêu.
  - Gia tăng tốc độ xuất chiêu và tốc độ di chuyển trong thời gian ngắn sau khi kích hoạt.
- **Tài nguyên SPR:**
  - Icon: `\spr\ui3\skillicon\trongkich_tuyetdoi.spr`
  - Cast SPR: `\spr\skill\effect\trongkich_hit.spr`

---

### 4. Kỹ năng Trùng Sinh 6 - 9: **Gia Cố Cảnh Giới & Kháng Trạng Thái**
- **Trùng Sinh 6 - 7:** Tăng cường cảnh giới Bí Kíp 150/180, gia tăng chỉ số **Kháng Đẩy Lùi**, **Kháng Bất Tỉnh**, **Kháng Thọ Thương** lên mức tối đa.
- **Trùng Sinh 8 - 9:** Kích hoạt thuộc tính ẩn **Chân Nguyên Hộ Thể**, tăng hàng nghìn điểm Nội Lực và phản đòn sát thương.

---

## PHẦN 2: CƠ CHẾ GẮN SKILL VÀ VỊ TRÍ FILE TRONG HỆ THỐNG JX1 / CLIENT

### 1. Khai báo chỉ số Skill trong Client & Server (`\settings\`):
- **`\settings\skill.txt`**: Khai báo ID Skill, Tên Skill, Loại kỹ năng (chủ động `1` / bị động `0` / buff `2`), Animation xuất chiêu, thời gian giãn cách (Cooldown).
- **`\settings\skills.txt`**: Cấu hình các công thức tính toán chỉ số cho từng cấp độ (Level 1 -> 20/30/60): `damage_v`, `lifemax_v`, `allres_v`, `state_hurt_rate`.
- **`\settings\skill_tree.txt`**: Gắn kỹ năng vào bảng F5 (cây kỹ năng môn phái / kỹ năng giang hồ / kỹ năng trùng sinh).

### 2. Gắn Animation và SPR Hiệu Ứng (`\spr\`):
- **Icon kỹ năng:** `\spr\ui3\skillicon\*.spr` hoặc `\spr\ui\skillicon\*.spr`.
- **Đạn đạo / Tên bắn / Kiếm khí:** `\spr\skill\missiles\*.spr`.
- **Vết trúng đòn (Explosion / Hit effect):** `\spr\skill\effect\*.spr` hoặc `\spr\skill\hit\*.spr`.
- **Vòng sáng dưới chân / Trên đầu:** `\spr\skill\buff\*.spr` và `\spr\skill\aura\*.spr`.

### 3. Script tiếp nhận & Xử lý Logic Trùng Sinh (`\script\`):
- **`\script\minhnguyettran\bacdaulaonhan.lua`**: Script của NPC Bắc Đẩu Lão Nhân. Khi người chơi đủ điều kiện, hàm `HaveReborn()` sẽ:
  - Tẩy trắng điểm: `ResetBaseAttrib()`, `ResetSkill()`.
  - Set level về cấp quy định: `SetLevel(10)`.
  - Gắn trực tiếp Skill Trùng Sinh vào nhân vật: `AddMagic(nSkillId, nSkillLevel)`.
  - Tăng mốc Trùng Sinh: `SetTask(TASK_REBORN_COUNT, nNewCount)`.

---

## PHẦN 3: CHI TIẾT CÁC KỸ NĂNG MÔN PHÁI (120, 150, 180) ĐƯỢC MỞ KHÓA THEO CÁC MỐC TRÙNG SINH

Sau khi Trùng Sinh, các môn phái đạt cảnh giới cao cấp để học và phát huy 100% công lực của các bộ kỹ năng đỉnh cao:

| Môn Phái | Kỹ Năng 120 (Trấn Phái / Buff) | Kỹ Năng 150 (Chủ Động Tối Thượng) | Kỹ Năng 180 (Cảnh Giới Tuyệt Tuyệt) |
| :--- | :--- | :--- | :--- |
| **Thiếu Lâm** | *Như Lai Thần Chưởng* / *Dịch Cân Kinh* (Tăng phản đòn, hồi phục, hộ thể) | *Đại Lực Kim Cang Chưởng* (Chưởng), *Vi Đà Hiến Xử* (Đao), *Tam Giới Quy Thiền* (Côn) | *Phật Quang Phổ Chiếu* (Tăng phạm vi nổ sát thương diện rộng và định thân) |
| **Thiên Vương** | *Bá Vương Khí Kình* / *Đoạt Mệnh Tam Thương* (Bất tử ngắn, tăng tốc độ đánh) | *Huyết Chiến Bát Phương* (Thương), *Thiên Vương Chiến Ý* (Chùy), *Phá Thiên Trảm* (Đao) | *Bá Khí Trảm Ma* (Tăng cực hạn xuyên giáp và kháng hiệu ứng) |
| **Đường Môn** | *Mê Ảnh Tung* / *Tâm Ma Tạp Niệm* (Tăng né tránh, tàng hình ngắn, tăng tốc độ) | *Bạo Vũ Lê Hoa Châm* (Châm), *Cửu Cung Phi Tinh* (Bẫy), *Đoạt Phách Câu Hồn* (Phi Đao) | *Thiên La Địa Võng* (Tỏa bẫy liên hoàn và sát thương chí mạng từ xa) |
| **Ngũ Độc** | *Vạn Cổ Thực Tâm* (Trừ kháng độc, tăng sát thương độc rút) | *Âm Phong Thực Cốt* (Chưởng), *U Hồn Phệ Ảnh* (Đao) | *Vạn Độc Quy Tông* (Tạo màn sương độc ăn mòn sinh lực toàn màn hình) |
| **Nga My** | *Phật Tâm Từ Hữu* / *Thanh Âm Phạn Xướng* (Tăng máu toàn đội, thanh tẩy hiệu ứng xấu) | *Phong Sương Toái Ảnh* (Chưởng), *Băng Tâm Tiên Tử* (Kiếm) | *Băng Vũ Thiên Địa* (Đóng băng diện rộng và phong tỏa hồi máu) |
| **Thúy Yên** | *Băng Cốt Tuyết Cơ* / *Phù Vân Tạ Tuyết* (Tăng né tránh, làm chậm cực đại) | *Băng Tung Vô Ảnh* (Đao), *Băng Tâm Trái Ảnh* (Kiếm) | *Tuyết Ảnh Vô Ngân* (Tàng hình liên kích và gây bất tỉnh mục tiêu) |
| **Cái Bang** | *Hỗn Thiên Khí Công* / *Túy Điệp Cuồng Vũ* (Tăng tốc độ xuất chiêu, kháng hỏa) | *Phi Long Tại Thiên* (Rồng), *Thiên Hạ Vô Cẩu* (Bổng) | *Thời Thừa Lục Long* (Triệu hồi 6 rồng lửa đuổi theo mục tiêu) |
| **Thiên Nhẫn** | *Ma Diễm Thất Sát* / *Bi Tô Thanh Phong* (Hút máu, thiêu đốt nội lực) | *Nhiếp Hồn Loạn Tâm* (Kích), *Thiên Ngoại Lưu Tinh* (Đao) | *Cửu U Ma Diễm* (Thiêu đốt mặt đất khiến địch thủ không thể tiếp cận) |
| **Võ Đang** | *Thái Cực Thần Công* / *Tọa Vọng Vô Vi* (Dùng nội lực hóa giải sát thương) | *Thiên Địa Vô Cực* (Kiếm), *Nhân Kiếm Hợp Nhất* (Khí) | *Vạn Kiếm Quy Tông* (Mưa kiếm khí làm choáng liên tục) |
| **Côn Lôn** | *Lưỡng Nghi Chân Khí* / *Sương Ngạo Côn Lôn* (Tăng né tránh, giảm sát thương nhận vào) | *Ngạo Tuyết Tiêu Phong* (Đao), *Lôi Động Cửu Thiên* (Kiếm) | *Hỗn Nguyên Nhất Khí* (Tạo lốc xoáy lôi điện áp chế toàn bộ chiến trường) |

---

## PHẦN 4: HƯỚNG DẪN TÌM & PREVIEW TÀI NGUYÊN SKILL TRÊN TOOL `FindInProcess`

Khi bật công cụ `FindInProcess` và gắn vào Client Võ Lâm (Game.exe):
1. **Tìm file cấu hình Skill:**
   - Trong ô tìm kiếm hoặc bộ nhớ quét chuỗi (PathStr), tìm: `\settings\skill.txt` hoặc `\settings\skills.txt`.
   - Double-click để nạp và preview nội dung file cấu hình bảng skill.
2. **Preview SPR Kỹ Năng Trùng Sinh:**
   - Quét tìm đường dẫn: `\spr\ui3\skillicon\*.spr` hoặc `\spr\skill\buff\*.spr`.
   - Nhấn chọn dòng để xem ảnh SPR hoặc tích vào ô **Animation** để chạy khung hình động của chiêu thức.
3. **Trích xuất (Unpack):**
   - Click chuột phải vào file SPR chiêu thức -> Chọn **Unpak Resource** để trích xuất file `.spr` nguyên bản ra thư mục làm việc.
