# Antigravity Project Configuration — UI_LIVE_VLTK

## 🔗 Repository Information
- **GitHub Remote:** [https://github.com/ldt140386-art/UI_LIVE_VLTK](https://github.com/ldt140386-art/UI_LIVE_VLTK)
- **Primary Branch:** `main`
- **Project Name:** UI_LIVE_VLTK (FindInProcess)
- **Local Path:** `E:\Jx1\Source_unpak UI_Live_VLMP`

---

## 🛠 Build & Environment Settings
- **Solution File:** `FindInProcess.sln`
- **Target Platform:** `x64`
- **Configuration:** `Release`
- **Build Command:**
  ```powershell
  & "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\amd64\MSBuild.exe" "FindInProcess.sln" /p:Configuration=Release /p:Platform=x64 /v:minimal
  ```
- **Output Executable:** `bin\x64\Release\FindInProcess.exe`
- **Git Binary:** `C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe`

---

## 🔄 Git Synchronization Protocol
1. **Periodic Milestone Commits**:
   - Every major feature, bugfix, encoding update, or UI rendering enhancement must be committed with structured conventional commit messages (`feat:`, `fix:`, `docs:`, `refactor:`).
2. **Ignored Artifacts**:
   - Build output directories (`bin/`, `build/`, `x64/`, `.vs/`) and intermediate binary objects (`*.obj`, `*.pdb`, `*.ilk`) are excluded via `.gitignore`.
3. **Documentation Sync**:
   - All technical analysis, encoding discoveries, and walkthroughs must be saved to `Docs/` and committed to git alongside code changes.
4. **Push Protocol**:
   - Synchronize local commits to `origin/main` at [ldt140386-art/UI_LIVE_VLTK](https://github.com/ldt140386-art/UI_LIVE_VLTK).

---

## 📚 Core Documentation Index
- [`Docs/Giai_Ma_Encoding_Tieng_Viet_JX1_VLMP.md`](file:///e:/Jx1/Source_unpak%20UI_Live_VLMP/Docs/Giai_Ma_Encoding_Tieng_Viet_JX1_VLMP.md) — Bảng mã tiếng Việt 1-byte JX1 VLMP (`kJx1VnTable[128]`)
- [`Docs/Analysis_FontPak_ASF_Format.md`](file:///e:/Jx1/Source_unpak%20UI_Live_VLMP/Docs/Analysis_FontPak_ASF_Format.md) — Phân tích định dạng font bitmap ASF từ `font.pak`
- [`Docs/Walkthrough_CrossPAK_UI_Composite.md`](file:///e:/Jx1/Source_unpak%20UI_Live_VLMP/Docs/Walkthrough_CrossPAK_UI_Composite.md) — Kiến trúc hệ thống file ảo Cross-PAK UI Composite
- [`Docs/Universal_KGUI_UI_Composite_Architecture.md`](file:///e:/Jx1/Source_unpak%20UI_Live_VLMP/Docs/Universal_KGUI_UI_Composite_Architecture.md) — Kiến trúc Universal KGUI Auto-Layout UI Composite Engine
- [`.agents/rules/jx1_pak_virtual_filesystem.md`](file:///e:/Jx1/Source_unpak%20UI_Live_VLMP/.agents/rules/jx1_pak_virtual_filesystem.md) — Quy tắc nạp PAK, giải mã SPR, và cấu trúc đa tầng UI INI
