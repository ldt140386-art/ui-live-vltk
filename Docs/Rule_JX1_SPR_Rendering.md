# JX1 SPR Rendering Rules

## SPR Format Overview
Kingsoft JX1 / Võ Lâm Miễn Phí uses the `.spr` sprite format with:
- `SPRHEAD` header: global `Width`, `Height`, `CenterX`, `CenterY`, `Frames`, `Colors`, `Directions`, `Interval`
- Per-frame data: local `frameW`, `frameH`, `offsetX`, `offsetY` within the global canvas

## Rules

### 1. Canvas Alignment
When rendering SPR frames with `placeOnCanvas = true`:
- **Always** use `SPRHEAD.Width × SPRHEAD.Height` as the canvas dimensions for ALL frames.
- Only fallback to `frameW + offsetX` when header Width/Height is literally 0.
- This ensures all frames share the same canvas size, preventing visual "jumping" during animation.

### 2. Auto Animation
- SPR files with `Frames >= 2` should **automatically start animation** when opened for preview.
- Do NOT require the user to manually tick a checkbox first.
- Animation timer should wrap-around: `nextFrame = (currentFrame + 1) % totalFrames` for infinite loop.

### 3. PAK Event Type
- When loading `.pak` files directly via "Open PAK File..." button, rows are inserted with event type `L"PAK"`.
- `ParsePreviewRow()` must recognize both `"PakBlock"` (from process monitoring) and `"PAK"` (from direct file load).
- For `"PAK"` event rows with format `pakPath -> UID=0x...`, extract `pakPath` before the `->` arrow as `diskPath`, and parse UID from the target.

### 4. Background Color
- SPR preview background should use **light/white** colors (not dark checkerboard) so pixel art with dark outlines is clearly visible.
- Recommended: white `RGB(255,255,255)` + light gray `RGB(240,240,240)` checkerboard pattern.
