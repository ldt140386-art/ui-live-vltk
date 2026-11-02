# TỪ ĐIỂN TRA CỨU SKILL ID & RESOURCE FILE MAPPING (VLMP / JX1)

> **Tài liệu tham khảo chuyên sâu:** Đối chiếu trực tiếp giữa mã ID kỹ năng (`SkillId`), tên chiêu thức tiếng Việt, đường dẫn ảnh icon (`\spr\ui3\skillicon\`), đạn đạo (`\spr\skill\missiles\`), vòng sáng hiệu ứng (`\spr\skill\buff\`) và script xử lý máy chủ.  
> **Áp dụng:** Võ Lâm Miễn Phí (VLMP) & Client/Server JX1.

---

## MỤC 1: BẢNG TRA CỨU KỸ NĂNG TRÙNG SINH (TS 1 - TS 9)

| Skill ID | Tên Kỹ Năng (Tiếng Việt) | Mốc Trùng Sinh | Loại Kỹ Năng | Icon SPR (F5) | Hiệu Ứng SPR (Buff / Missile) | File Cấu Hình Settings |
| :---: | :--- | :---: | :---: | :--- | :--- | :--- |
| **381** | **Quan Tinh Luyện Thần** | TS 3 | Bị động / Tự động | `\spr\ui3\skillicon\quantinhluyenthan.spr` | `\spr\skill\buff\quantinh_aura.spr` | `\settings\skills.txt` |
| **382** | **Hộ Thể Khí Kình (Kim Cang)** | TS 4 | Bị động tăng Kháng/HP | `\spr\ui3\skillicon\reborn_skill_04.spr` | `\spr\skill\buff\reborn4_shield.spr` | `\settings\skills.txt` |
| **385** | **Đại Diệp Khí Tráo** | TS 5 (Nhánh Thủ) | Chủ động Buff Khiên | `\spr\ui3\skillicon\daidiep_khitrao.spr` | `\spr\skill\buff\daidiep_loop.spr` | `\settings\skill.txt` |
| **386** | **Trọng Kích Tuyệt Đối** | TS 5 (Nhánh Công) | Chủ động Bạo Kích | `\spr\ui3\skillicon\trongkich_tuyetdoi.spr` | `\spr\skill\effect\trongkich_hit.spr` | `\settings\skill.txt` |
| **390** | **Chân Nguyên Hộ Thể** | TS 6 - 7 | Bị động Kháng Trạng Thái | `\spr\ui3\skillicon\channguyen_hothe.spr` | `\spr\skill\buff\state_resist.spr` | `\settings\skills.txt` |
| **395** | **Cảnh Giới Tuyệt Tuyệt** | TS 8 - 9 | Hộ Thể Phản Đòn | `\spr\ui3\skillicon\canhgioi_tuyettuyet.spr`| `\spr\skill\aura\reborn_aura_09.spr` | `\settings\skills.txt` |

---

## MỤC 2: BẢNG TRA CỨU SKILL MÔN PHÁI CAO CẤP (120, 150, 180)

### 1. Thiếu Lâm Phái
- **Kỹ năng 120:** `Như Lai Thần Chưởng` (ID: `271`) — Icon: `\spr\ui3\skillicon\thieulam_120.spr` — Buff: `\spr\skill\buff\nhulaithanchuong.spr`
- **Kỹ năng 150:**
  - *Đại Lực Kim Cang Chưởng* (ID: `321` - Chưởng) — Missile: `\spr\skill\missiles\tl_chuong150.spr`
  - *Vi Đà Hiến Xử* (ID: `322` - Đao) — Missile: `\spr\skill\missiles\tl_dao150.spr`
  - *Tam Giới Quy Thiền* (ID: `323` - Côn) — Missile: `\spr\skill\missiles\tl_con150.spr`
- **Kỹ năng 180:** `Phật Quang Phổ Chiếu` (ID: `401`) — Explosion: `\spr\skill\effect\phatquang_hit.spr`

### 2. Thiên Vương Bang
- **Kỹ năng 120:** `Bá Vương Khí Kình` (ID: `272`) — Buff: `\spr\skill\buff\tv_bavuong.spr`
- **Kỹ năng 150:**
  - *Huyết Chiến Bát Phương* (ID: `324` - Thương) — Missile: `\spr\skill\missiles\tv_thuong150.spr`
  - *Thiên Vương Chiến Ý* (ID: `325` - Chùy) — Missile: `\spr\skill\missiles\tv_chuy150.spr`
  - *Phá Thiên Trảm* (ID: `326` - Đao) — Missile: `\spr\skill\missiles\tv_dao150.spr`
- **Kỹ năng 180:** `Bá Khí Trảm Ma` (ID: `402`) — Buff: `\spr\skill\buff\tv_180aura.spr`

### 3. Đường Môn
- **Kỹ năng 120:** `Mê Ảnh Tung` (ID: `273`) — Buff: `\spr\skill\buff\dm_meanh.spr`
- **Kỹ năng 150:**
  - *Bạo Vũ Lê Hoa Châm* (ID: `327` - Châm) — Missile: `\spr\skill\missiles\dm_cham150.spr`
  - *Cửu Cung Phi Tinh* (ID: `328` - Bẫy) — Missile: `\spr\skill\missiles\dm_bay150.spr`
  - *Đoạt Phách Câu Hồn* (ID: `329` - Phi Đao) — Missile: `\spr\skill\missiles\dm_phidao150.spr`
- **Kỹ năng 180:** `Thiên La Địa Võng` (ID: `403`) — Explosion: `\spr\skill\effect\dm_thienla.spr`

### 4. Ngũ Độc Giáo
- **Kỹ năng 120:** `Vạn Cổ Thực Tâm` (ID: `274`) — Buff: `\spr\skill\buff\5d_vanco.spr`
- **Kỹ năng 150:**
  - *Âm Phong Thực Cốt* (ID: `330` - Chưởng) — Missile: `\spr\skill\missiles\5d_chuong150.spr`
  - *U Hồn Phệ Ảnh* (ID: `331` - Đao) — Missile: `\spr\skill\missiles\5d_dao150.spr`
- **Kỹ năng 180:** `Vạn Độc Quy Tông` (ID: `404`) — Explosion: `\spr\skill\effect\5d_vandoc.spr`

### 5. Nga My Phái
- **Kỹ năng 120:** `Phật Tâm Từ Hữu` (ID: `275`) — Buff: `\spr\skill\buff\nm_phattam.spr`
- **Kỹ năng 150:**
  - *Phong Sương Toái Ảnh* (ID: `332` - Chưởng) — Missile: `\spr\skill\missiles\nm_chuong150.spr`
  - *Băng Tâm Tiên Tử* (ID: `333` - Kiếm) — Missile: `\spr\skill\missiles\nm_kiem150.spr`
- **Kỹ năng 180:** `Băng Vũ Thiên Địa` (ID: `405`) — Explosion: `\spr\skill\effect\nm_bangvu.spr`

### 6. Thúy Yên Môn
- **Kỹ năng 120:** `Băng Cốt Tuyết Cơ` (ID: `276`) — Buff: `\spr\skill\buff\ty_bangcot.spr`
- **Kỹ năng 150:**
  - *Băng Tung Vô Ảnh* (ID: `334` - Đao) — Missile: `\spr\skill\missiles\ty_dao150.spr`
  - *Băng Tâm Trái Ảnh* (ID: `335` - Kiếm) — Missile: `\spr\skill\missiles\ty_kiem150.spr`
- **Kỹ năng 180:** `Tuyết Ảnh Vô Ngân` (ID: `406`) — Buff: `\spr\skill\buff\ty_tuyetanh.spr`

### 7. Cái Bang
- **Kỹ năng 120:** `Hỗn Thiên Khí Công` (ID: `277`) — Buff: `\spr\skill\buff\cb_honthein.spr`
- **Kỹ năng 150:**
  - *Phi Long Tại Thiên* (ID: `336` - Rồng/Chưởng) — Missile: `\spr\skill\missiles\cb_rong150.spr`
  - *Thiên Hạ Vô Cẩu* (ID: `337` - Bổng) — Missile: `\spr\skill\missiles\cb_bong150.spr`
- **Kỹ năng 180:** `Thời Thừa Lục Long` (ID: `407`) — Missile: `\spr\skill\missiles\cb_6rong180.spr`

### 8. Thiên Nhẫn Giáo
- **Kỹ năng 120:** `Ma Diễm Thất Sát` (ID: `278`) — Buff: `\spr\skill\buff\tn_madiem.spr`
- **Kỹ năng 150:**
  - *Nhiếp Hồn Loạn Tâm* (ID: `338` - Kích) — Missile: `\spr\skill\missiles\tn_kich150.spr`
  - *Thiên Ngoại Lưu Tinh* (ID: `339` - Đao) — Missile: `\spr\skill\missiles\tn_dao150.spr`
- **Kỹ năng 180:** `Cửu U Ma Diễm` (ID: `408`) — Explosion: `\spr\skill\effect\tn_cuu_u.spr`

### 9. Võ Đang Phái
- **Kỹ năng 120:** `Thái Cực Thần Công` (ID: `279`) — Buff: `\spr\skill\buff\vd_thaicuc.spr`
- **Kỹ năng 150:**
  - *Thiên Địa Vô Cực* (ID: `340` - Kiếm) — Missile: `\spr\skill\missiles\vd_kiem150.spr`
  - *Nhân Kiếm Hợp Nhất* (ID: `341` - Khí) — Missile: `\spr\skill\missiles\vd_khi150.spr`
- **Kỹ năng 180:** `Vạn Kiếm Quy Tông` (ID: `409`) — Missile: `\spr\skill\missiles\vd_vankiem.spr`

### 10. Côn Lôn Phái
- **Kỹ năng 120:** `Lưỡng Nghi Chân Khí` (ID: `280`) — Buff: `\spr\skill\buff\cl_luongnghi.spr`
- **Kỹ năng 150:**
  - *Ngạo Tuyết Tiêu Phong* (ID: `342` - Đao) — Missile: `\spr\skill\missiles\cl_dao150.spr`
  - *Lôi Động Cửu Thiên* (ID: `343` - Kiếm) — Missile: `\spr\skill\missiles\cl_kiem150.spr`
- **Kỹ năng 180:** `Hỗn Nguyên Nhất Khí` (ID: `410`) — Explosion: `\spr\skill\effect\cl_honnguyen.spr`

---

## MỤC 3: TEMPLATE SCRIPT LUA AN TOÀN TRÙNG SINH (`reborn_check.lua`)

```lua
-- File: \script\minhnguyettran\reborn_check.lua
-- Mục đích: Kiểm tra toàn diện trang bị, chiến đội, hành trang trước khi cấp phép Trùng Sinh

Include("\\script\\lib\\task.lua")
Include("\\script\\lib\\award.lua")

TASK_REBORN_COUNT = 1500 -- Task lưu số lần Trùng Sinh

function CheckRebornSafetyConditions()
    -- 1. Kiểm tra đẳng cấp
    if (GetLevel() < 200) then
        Talk(1, "", "Đẳng cấp chưa đạt cấp 200, không thể lĩnh ngộ thuật Trùng Sinh!")
        return 0
    end

    -- 2. Kiểm tra trang bị trên người (phải tháo sạch 10 món chính + ngoại trang)
    for i = 0, 19 do
        local nItemId = GetEquipItem(i)
        if (nItemId and nItemId > 0) then
            Talk(1, "", "Ngươi còn mang trang bị trên người (Vị trí ô trang bị số " .. i .. "). Hãy tháo toàn bộ ra trước khi Trùng Sinh!")
            return 0
        end
    end

    -- 3. Kiểm tra trạng thái tổ đội & chiến đội
    if (GetTeamSize() > 1) then
        Talk(1, "", "Hãy rời khỏi Tổ đội trước khi bắt đầu Trùng Sinh!")
        return 0
    end

    -- 4. Kiểm tra ô trống hành trang (yêu cầu tối thiểu 10 ô)
    if (CalcFreeItemCellCount() < 10) then
        Talk(1, "", "Hành trang cần tối thiểu 10 ô trống để nhận phần thưởng hỗ trợ cảnh giới mới!")
        return 0
    end

    return 1
end

function ExecuteRebornStep(nNextRebornCount, nSkillIdReward)
    if (CheckRebornSafetyConditions() ~= 1) then
        return
    end

    -- Tẩy điểm tiềm năng & kỹ năng
    ResetBaseAttrib()
    ResetSkill()

    -- Set lại cấp độ quy định
    SetLevel(10)

    -- Cập nhật Task Trùng Sinh
    SetTask(TASK_REBORN_COUNT, nNextRebornCount)

    -- Gắn kỹ năng đặc quyền nếu có
    if (nSkillIdReward and nSkillIdReward > 0) then
        AddMagic(nSkillIdReward, 1)
    end

    -- Thông báo toàn server
    Msg2Player("Chúc mừng hiệp khách đã đột phá cảnh giới Trùng Sinh " .. nNextRebornCount .. " thành công!")
end
```
