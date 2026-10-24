# Native Markdown Editor

A small, portable Markdown editor for Windows x64, built with C++17 and native
Win32 controls. Edit the Markdown source or read a formatted preview, without a
browser engine, extra runtime, installer, or startup network access.

- CommonMark headings, emphasis, lists, quotes, links, code, Unicode, and pipe tables.
- Separate Edit/View controls that preserve undo history, selection, and scrolling.
- Double-click the preview to edit at the corresponding source character.
- Light/dark themes, configurable colors and fonts, and per-monitor DPI scaling.

## Run

Copy `markdown-editor.exe` and `MD4C-LICENSE.txt` from [release/](release/) into
a permanent folder. The required distribution is about **337 KiB**.

From repository-root PowerShell:

```powershell
& '.\release\markdown-editor.exe'
& '.\release\markdown-editor.exe' '.\sample.md'
```

An empty launch starts in Edit; opening a file starts in View. Each window holds
one document. To make it your `.md` default, use Windows **Open with** and select
the executable; the application does not change file associations itself.

## Controls

| Action                              | Shortcut                            |
| ----------------------------------- | ----------------------------------- |
| Open / Save                         | Ctrl+O / Ctrl+S                     |
| Toggle Edit/View                    | Ctrl+T                              |
| Leave Edit mode                     | Esc                                 |
| Edit at the clicked source position | Double-click View without modifiers |
| Undo / Redo in Edit                 | Ctrl+Z / Ctrl+Y or Ctrl+Shift+Z     |
| Select all / Copy                   | Ctrl+A / Ctrl+C                     |
| Open an HTTP(S) or mailto link      | Ctrl+click in View                  |

Edit supports native cut/paste and a Copy/Cut/Paste context menu; View is
read-only with selection, copying, and a Copy context menu. Mode switches keep
source text and undo/redo intact. `*` in the title marks unsaved changes.

## Build

Requires Visual Studio or Build Tools with **Desktop development with C++**,
an x64 MSVC toolchain, Windows SDK, CMake 3.20+, and Ninja. From ordinary
PowerShell, the script locates Visual Studio and initializes its environment:

```powershell
& '.\build.ps1'
& '.\build.ps1' -Configuration Debug
```

Release builds into `build/Release/` and updates the executable and license in
`release/`. Debug builds into `build/Debug/` and leaves the packaged Release
untouched. Close running packaged copies before rebuilding. Builds are offline;
the static MSVC runtime means no VC runtime installation is needed.

Alternatively, from an **x64 Native Tools Command Prompt** with Ninja on PATH:

```bat
cmake -S . -B build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release
```

Application code is in `src/`, native checks in `tests/`, and the vendored
[MD4C parser](third_party/md4c/LICENSE.md) in `third_party/md4c/`.

## Verify

```powershell
& '.\tests\check.ps1' -Benchmark
& '.\smoke.ps1'
```

The renderer checks rebuild Release; `-Benchmark` also reports mapping and table
layout memory, render timings, native cell measurement, and cached table resizing.
The smoke suite exercises native windows, file handling, themes, undo/redo,
Unicode, tables, DPI handlers, and exact source navigation. Generated
files stay under ignored `build/`. Physical mouse interaction, visible caret
appearance, and real mixed-monitor movement still need manual checks.
Viewport checks also cover outer padding when scrolling tables, resizing, and
showing or hiding the horizontal scrollbar.

## Settings and file handling

**Mode → Dark theme** switches and saves the theme immediately. Colors and an
optional private TTF font are configured in `markdown-editor.ini` beside the
executable; see [the configuration example](markdown-editor.ini.example).
The app fills in missing palette defaults. Restart after changing colors or
fonts. The executable's folder must be writable to persist settings.
Table borders have an independent `table_border` RGB setting, defaulting to
`#C1BFB9` in Light and `#484848` in Dark. Changing text or background colors does
not recompute this setting; invalid entries use the corresponding default.

Markdown source is always authoritative. Saves preserve supported encodings
(UTF-8 and UTF-16 with BOM) and newline conventions; failed saves retain unsaved
work. New files use UTF-8 without BOM and CRLF. Invalid Unicode, embedded NULs,
and files over 32 MiB are rejected. There is no autosave or recovery file.

Preview uses MD4C CommonMark with only its pipe-table extension enabled (the
renderer API also supports explicit strict `CommonMark`). Tables use native
Rich Edit cells with a thin 0.5-point full grid, bold headers without added fill,
and left/center/right alignment from the Markdown separator row. Inline styles,
links, entities, Unicode, and list/quote indentation are retained.

Columns use the largest formatted cell width, including bold headers and modest
padding. Compact tables keep their natural width; wider tables shrink their
widest columns first and wrap cell text. Columns have an 18-point minimum. If
those minimums cannot fit, the preview scrolls horizontally while surrounding
prose stays wrapped. Partial rows are clipped at the viewport edges; vertical
padding sits outside the native text window to avoid stale table text at the
bottom while scrolling. Resizing and DPI changes reuse cached row positions and
preferred widths without reparsing or measuring again. Double-click navigation
uses native cell and hidden row offsets; copying produces ordinary cell text.
Tables beyond Rich Edit's [63-column limit](https://learn.microsoft.com/en-us/windows/win32/controls/em-settableparms)
fall back to readable tab-separated rows with every parsed cell retained.

Images use a readable fallback and are not downloaded; HTML is shown as inert
source. Other Markdown extensions are unsupported; code is not syntax-highlighted.

On the development machine, the existing large/dense renderer benchmarks remain
about 8/14 ms with unchanged mapping memory. The table workload (80 tables,
720 rows, 2,880 cells) takes about 162 ms to render, dominated by native formatted
cell measurement, and 9 ms to resize from cached widths. Its retained table
metadata occupies 8,640 bytes (9,424 allocated), in addition to source mapping.
These timings depend on fonts and hardware; run the benchmark for local results.

## Roadmap

- [x] Table rendering
- [ ] Emoji support
- [ ] Recent files
- [ ] Status bar
- [ ] Application icon
- [ ] Application license
- [ ] Build pipeline and GitHub releases
- [ ] Configurable line height in edit-mode
