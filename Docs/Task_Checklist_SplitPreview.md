# Task List

## Learning Rules
- [x] Create `.agents/rules/jx1_pak_virtual_filesystem.md`

## Split Preview + UI Composite Renderer
- [x] Create `ui_ini_renderer.h` — structs and API declarations
- [x] Create `ui_ini_renderer.cpp` — INI parser + composite bitmap renderer
- [x] Add `IDC_STATIC_UI_COMPOSITE` to `resource.h`
- [x] Add UI composite Static control to `FindInProcess.rc`
- [x] Add `hUiComposite` HWND + subclass to `main.cpp` AppState
- [x] Modify `LayoutControls` to split preview into top/bottom
- [x] Modify `PreviewSelectedRow` to detect INI → trigger composite render
- [x] Add `ui_ini_renderer.cpp` to `FindInProcess.vcxproj`
- [x] Build x64 Release
- [x] Verify build success — 0 Errors, 0 Warnings
