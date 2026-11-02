# Learning Proposal: TCVN3 Encoding & PAK Performance Caching

## What Was Learned

### Issue 1: Vietnamese Text Encoding (TCVN3)
JX1 VLMP INI files use **mixed encoding**:
- **Section names & Image paths** → GBK (Chinese characters like `\Spr\Ui4\主界面\...`)
- **Text & Tip values** → **TCVN3** (Vietnamese single-byte encoding)

When the entire INI is decoded as GBK, Vietnamese TCVN3 bytes (0x80-0xFF) are misinterpreted as GBK lead bytes, consuming the next ASCII char and producing wrong Chinese characters:
- `Text=Bảng bổng` → garbled as `B溇 b给`
- `Text=Gian Hàng` → garbled as `Gian H脱g`

**Fix**: Parse INI at byte level. Use GBK for structure/paths, TCVN3 lookup table for Text/Tip values.

### Issue 2: Slow PAK Composite Rendering
Each widget's `Image=` path was individually resolved via `TryReadVirtualPathAcrossKnownPakRoots()`, causing redundant PAK file reads when multiple widgets share the same SPR file.

**Fix**: Pre-resolve all unique paths into a `std::map<path, CachedSpr>` before rendering.

## Proposed Rule Updates

### Update: `.agents/rules/jx1_pak_virtual_filesystem.md`

Add TCVN3 encoding section:

```diff
+## Vietnamese Text Encoding (TCVN3)
+JX1 VLMP INI files use MIXED encoding:
+- Section names, key names, and Image= paths: **GBK** (for Chinese characters)
+- Text=, Tip=, Label= values: **TCVN3** (Vietnamese single-byte encoding, 0x80-0xFF)
+
+TCVN3 is a single-byte encoding where bytes 0x80-0xFF map to Vietnamese Unicode characters.
+GBK decoder will incorrectly consume TCVN3 bytes as lead bytes of 2-byte pairs.
+
+**Rule**: Always parse JX1 INI files at the byte level. Use GBK for structure/paths,
+TCVN3 lookup table for text content (Text=, Tip=, Label=).
+
+## Performance: SPR Bitmap Caching
+When rendering UI composites, pre-resolve all unique Image= virtual paths into a cache
+(`std::map<path, bitmap>`) before rendering. This avoids redundant PAK reads for
+shared SPR files and significantly improves rendering speed.
```

## Classification
- **Type:** Rule update (existing `jx1_pak_virtual_filesystem.md`)
- **Scope:** JX1/VLMP INI parsing and rendering
- **Root cause:** Mixed encoding in INI files (GBK structure + TCVN3 text content)
