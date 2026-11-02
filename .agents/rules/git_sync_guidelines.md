# Git Backup & Synchronization Guidelines

## 1. Remote Repository
- **URL:** `https://github.com/ldt140386-art/UI_LIVE_VLTK.git`
- **Main Branch:** `main`

## 2. Commit Requirements
- Perform commits on each major milestone or task completion.
- Keep commits focused and provide clear commit messages (e.g. `feat(ui): ...`, `fix(encoding): ...`, `docs: ...`).
- Always stage all updated files in `Docs/`, `ui_ini_renderer.*`, `pak_engine.*`, `media_preview.*`, and `.agents/`.

## 3. Git Push Workflow
- When pushing to GitHub:
  ```powershell
  & "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" push -u origin main
  ```
- Windows Credential Manager will securely store authentication credentials for `https://github.com/ldt140386-art/UI_LIVE_VLTK.git`.
