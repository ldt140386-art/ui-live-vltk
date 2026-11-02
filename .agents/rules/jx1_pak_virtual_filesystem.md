# JX1 PAK Virtual Filesystem & UI INI Format

## Virtual Filesystem
- All `.pak` files in the game's `data/` directory form a unified virtual filesystem.
- Files reference each other via **virtual paths**: `\Spr\Ui4\主界面\登入界面\输入账号界面vn.spr`
- Virtual path → GBK encoding → `FileNameToPackId()` → **UID hash** → search across all PAK files.
- Resolution priority: `update*.pak` > `resource*.pak` > `settings*.pak` > other PAKs.
- Use `TryReadVirtualPathAcrossKnownPakRoots()` to resolve any virtual path to raw bytes.

## UI INI Format
JX1 uses INI-style config files to define complete UI layouts (login screen, chat, inventory, etc.).

### Section Types
- `[Main]` — Root panel: `Left`, `Top`, `Width`, `Height`, `Image` (background SPR), `Moveable`, `StartPos`
- `[ButtonName]` — Button widget: `Left`, `Top`, `Width`, `Height`, `Image` (SPR), `Up`/`Down`/`Over` (frame indices), `Tip`
- `[InputName]` — TextBox: `Left`, `Top`, `Width`, `Height`, `Font`, `Type` (1=password, 2=text), `MaxLen`, `Color`, `BorderColor`
- `[CheckboxName]` — Checkbox: `CheckBox=1`, `Image` (SPR), `Up`/`Down` frame indices
- `[LabelName]` — Static text: `Left`, `Top`, `Width`, `Color`, `Text`
- `[ListName]` — Dropdown/list: `Selable=1`, `MaxMsgCount`, `MsgColor`, `SelColor`, `HighLight`

### Key Properties
| Key | Type | Description |
|-----|------|-------------|
| `Image` | string | Virtual path to `.spr` file (cross-PAK reference) |
| `Left`, `Top` | int | Position relative to parent `[Main]` (also `PosX`, `PosY`) |
| `Width`, `Height` | int | Widget size in pixels |
| `Up`, `Down`, `Over` | int | SPR frame index for button states |
| `Color`, `MsgColor` | R,G,B | Text/foreground color (default warm gold `RGB(255, 226, 70)`) |
| `SelectColor`, `OverColor` | R,G,B | Text color when selected or hovered |
| `BorderColor` | R,G,B | Border/outline color |
| `Font` | int | Font size (12, 14, 16) |
| `LabelXOffset`, `LabelYOffset` | int | Extra offset for text rendered over button |
| `HAlign`, `VAlign` | int | Horizontal (0=left, 1=center, 2=right) & Vertical (0=top, 1=center, 2=bottom) alignment |
| `Trans` | 0/1 | Transparent background |
| `CheckBox` | 0/1 | Marks widget as checkbox or tab button control |
| `Text`, `Label`, `Caption` | string | Static text content (JX1 Vietnamese single-byte encoding) |
| `Tip` | string | Tooltip text |

## 5. Universal UI Composite Multi-Layer Rules
1. **Control Classification**:
   - `[Main]`: Canvas boundary + root background (`Image=` / `BgImage=`).
   - `[Img*]`, `[Bg*]`, `[Pic*]`: Sub-panels and decorative frames.
   - `[Btn*]`, `[Close]`, `[Cancel]`, `[Ok]`: Buttons with vertical + horizontal centered labels.
   - `[Btn*Page]`, `[Tab*]`: Tab pages with duplicate bounding-box overlap filter.
   - `[CheckBox*]`, `StatusImage=`: Status icons (14x14) with label offset `Left + 18px`.
   - `[Txt*]`, `[Title]`, `[*Txt]`: Static single-line text with `DT_NOCLIP` to avoid premature truncation.
   - `[Account]`, `[Edit*]`: Input boxes with `BorderColor`.
   - `[ListBox*]`: List areas with `SelBgColor` and `MsgColor`.
2. **Layered Composition Order**:
   - Layer 0: Root background `[Main]`.
   - Layer 1: Sub-panels `[Img*]`.
   - Layer 2: Functional boxes `[ObjBox*]`, `[ListBox*]`, `[ScrollBar*]`.
   - Layer 3: Interactive controls `[Btn*]`, `[CheckBox*]`.
   - Layer 4: Text labels on top with `Segoe UI`, `FW_NORMAL` (400) and warm gold `#FFE246`.

## Cross-PAK Dependency
When previewing an INI from one PAK file, its `Image=` paths typically point to SPR files
stored in **completely different PAK archives**. Always search ALL known/loaded PAK files
when resolving `Image=` virtual paths. The `knownPaks` set must include sibling PAKs.

## Text Encoding — JX1 Custom Vietnamese Single-Byte Encoding (Critical)
JX1 VLMP INI files use **MIXED encoding** in the same file:

### GBK (Code Page 936) — for structure & SPR paths
- Section names: `[Main]`, `[LoginTip]`
- Key names: `Image`, `Left`, `Width`
- `Image=` virtual paths: `\Spr\Ui4\主界面\登入界面\输入账号界面vn.spr`
- UID generation: Always try GBK → ACP → UTF-8 when generating candidate UIDs

### JX1 Single-Byte Table (`kJx1VnTable`) — for Vietnamese text content
- `Text=`, `Label=`, `Caption=`, `Title=`, `String=`, `Name=`, `Tip=` values.
- Single-byte encoding where bytes `0x80 - 0xFF` map directly to Vietnamese Unicode characters.
- If decoded with GBK, these bytes are misinterpreted as Chinese lead bytes and corrupt the text.

### Rule: Byte-Level Parsing Required
**ALWAYS parse JX1 INI files at the byte level:**
1. Split into lines at byte level (scan for `\n`)
2. Extract section headers and key names as raw bytes → decode with GBK
3. For `Text=`, `Tip=`, `Label=` values → decode with TCVN3 lookup table (128 entries, 0x80-0xFF)
4. For `Image=` paths → decode with GBK
5. For numeric values (`Left=`, `Width=`, etc.) → parse as ASCII digits

**NEVER decode the entire INI as GBK first** — this irreversibly corrupts Vietnamese text.

## Performance: SPR Bitmap Caching
When rendering UI composites with multiple widgets referencing SPR files:
- **Pre-resolve** all unique `Image=` virtual paths into a `std::map<path, CachedSpr>` cache
- Only call `TryReadVirtualPathAcrossKnownPakRoots()` + `CreateBitmapFromSprBytes()` once per unique path
- Reuse cached bitmaps for all widgets sharing the same SPR file
- This avoids redundant PAK file I/O and decompression, significantly improving render speed

## JX1 ASF Bitmap Font Format
`font.pak` contains 6 ASF (custom bitmap font) files, NOT TrueType/OpenType.

### ASF Header (16 bytes minimum)
| Offset | Size | Field |
|--------|------|-------|
| 0x00 | 4 | Signature: `ASF\0` (0x41534600) |
| 0x04 | 4 | Total bitmap data size |
| 0x08 | 4 | Number of glyphs |
| 0x0C | 2 | Cell width (12, 14, or 16 pixels) |
| 0x0E | 2 | Cell height (12, 14, or 16 pixels) |

### Available Font Sizes
| Size | ASCII (512 glyphs) | Full CJK+VN (23,940 glyphs) |
|------|-------|------|
| 12×12 | 19 KB | 1.5 MB |
| 14×14 | 23 KB | 2.1 MB |
| 16×16 | 28 KB | 2.8 MB |

### UI Composite Rendering
For preview purposes, use system Unicode fonts (Tahoma) matching the INI `Font=` size
rather than ASF bitmap fonts. ASF fonts are 1-bit bitmap and appear rough at modern
resolutions. The `Font=14` INI key maps to `CreateFontW(-14, ...)` with Tahoma.
