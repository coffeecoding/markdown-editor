# Native Markdown Editor

<img src="src/app-icon.svg" width="64" height="64" alt="An ivory quill on deep teal">

A small, portable Markdown editor for Windows x64, built with C++17 and native
Win32 controls. Edit the Markdown source or read a formatted preview, without a
browser engine, extra runtime, installer, or startup network access.

- CommonMark headings, emphasis, lists, quotes, links, code, Unicode, and pipe tables.
- Separate Edit/View controls that preserve undo history, selection, and scrolling.
- Double-click the preview to edit at the corresponding source character.
- Light/dark themes, configurable colors and fonts, and per-monitor DPI scaling.

## Run

Copy `markdown-editor.exe` and `MD4C-LICENSE.txt` from [release/](release/) into
a permanent folder. The required distribution is about **394 KiB**.

The ivory quill icon is embedded in the executable for Explorer, the taskbar,
Alt+Tab, and the title bar. Ten sizes from 16 to 256 pixels keep it clear at
different display scales; window icons reload at the monitor's DPI.

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
| Zoom / reset zoom                   | Ctrl+wheel / Ctrl+0                 |
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

The original icon artwork is [an editable SVG](src/app-icon.svg). After editing
it, run `& '.\tools\make-icon.ps1'` to regenerate `src/app.ico`, then rebuild.
This optional tool uses PowerShell and Windows System.Drawing, without packages;
normal builds use the checked-in ICO and require no artwork tools.

## Verify

```powershell
& '.\tests\check.ps1' -Benchmark
& '.\smoke.ps1'
```

The renderer checks rebuild Release; `-Benchmark` also reports mapping and table
layout memory, render timings, native cell measurement, and cached table resizing.
The native settings checks cover parsing, repair, encodings, file locks, directory
notification overflow, and shutdown. The smoke suite exercises native windows,
file handling, live settings, undo/redo, Unicode, tables, DPI handlers, and exact
source navigation. Generated
files stay under ignored `build/`. Icon extraction and DPI-specific window icon
sizes are checked too. Physical mouse interaction, visible caret and shell icon
appearance, visible fallback colors/font reflow, and real mixed-monitor movement
still need manual checks. Acceptance of non-ASCII family names is checked when
such a font is installed on the test machine.
Viewport checks also cover outer padding when scrolling tables, resizing, and
showing or hiding the horizontal scrollbar.

## Settings and file handling

**Mode → Dark theme** switches and saves the theme immediately. Edit
`markdown-editor.ini` beside the executable to change colors, installed Windows
font families, or the selected theme; see [the configuration example](markdown-editor.ini.example).
Saved changes apply live after a 150 ms debounce, including replacement saves
and changes made while a native dialog is open. Each window watches the same
file without idle polling. Temporary sharing locks retain the current appearance
and retry with a 100 ms to one-second backoff.

Ctrl+wheel scales all document text proportionally in 10 percentage point steps,
from 50–300%; Ctrl+0 restores 100% while retaining Windows DPI scaling. Edit and
View share one zoom level per window, including when opening another document.
The latest zoom is saved after 300 ms to `[appearance] zoom=100` for new windows
and future launches. Existing windows keep their own zoom during settings reloads.
Invalid or missing zoom values use 100%. Failed saves retain the active zoom and
report once until a save succeeds. Confirmed close flushes pending zoom changes.

The independent font settings are `[font] edit=Consolas`, `prose=Segoe UI`, and
`code=Consolas`. Font sizes remain fixed. Family names are checked with Windows
font enumeration; there is no private font loading. Both palettes keep their
own `background`, `text`, `link`, `muted`, `code`, and `table_border` RGB settings.
Table borders default to `#C1BFB9` in Light and `#484848` in Dark, independently
of the other colors.

Missing settings use their normal defaults, with Light as the default theme.
Present-but-empty or invalid background/code colors show dark red (`#8B0000`);
invalid text/link/muted/border colors show hot pink (`#FF69B4`). Invalid font
families use Consolas; an invalid theme uses Light. A structurally unparseable
INI uses all error colors and Consolas. Correcting the file restores the
configured appearance automatically. Font changes preserve visible text anchors
across reflow; color changes retain pixel scrolling. Hidden preview rendering
waits until View is requested.

Settings accept UTF-8 (with or without BOM) and BOM-marked UTF-16 LE/BE,
case-insensitive section/key names, surrounding whitespace, and semicolon comment
lines. Duplicate keys use the last occurrence. Non-comment lines must be a
`[section]` or a nonempty `key=value` inside a section. Values have no quoting
or inline-comment syntax.

Startup and reload never populate or repair the INI. After a confirmed close,
the app rereads the latest file and atomically repairs missing or invalid theme,
font, zoom, and palette settings to normal defaults, preserving valid choices,
comments, unrelated entries, encoding/BOM, and existing line endings. A malformed
file is first copied to a unique `markdown-editor.ini.invalid-*.bak` beside it,
then replaced with complete defaults. Canceling close performs no repair. A
failed repair reports once, retains the original, and allows closing. The
executable's folder must be writable to persist settings or create backups.

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
bottom while scrolling. Zoom, resizing, and DPI changes reuse cached row positions and
preferred widths without reparsing or measuring again. Double-click navigation
uses native cell and hidden row offsets; copying produces ordinary cell text.
Tables beyond Rich Edit's [63-column limit](https://learn.microsoft.com/en-us/windows/win32/controls/em-settableparms)
fall back to readable tab-separated rows with every parsed cell retained.

Images use a readable fallback and are not downloaded; HTML is shown as inert
source. Other Markdown extensions are unsupported; code is not syntax-highlighted.

On the development machine, the existing large/dense renderer benchmarks remain
about 8/14 ms with unchanged mapping memory. The table workload (80 tables,
720 rows, 2,880 cells) takes about 166 ms to render, dominated by native formatted
cell measurement, and 9 ms to resize from cached widths. Its retained table
metadata occupies 8,640 bytes (9,424 allocated), in addition to source mapping.
Zoom measured about 2–4 ms for both a 405,000-character document and an 80-table
document, using cached layout without parsing or cell measurement. Renderer
benchmarks remain about 8/14 ms with unchanged mapping and table metadata memory.
These timings depend on fonts and hardware; run the benchmark for local results.

## Roadmap

- [x] Table rendering
- [ ] Recent files
- [x] Scroll-wheel zoom
- [x] Application icon
- [ ] Application license
- [ ] Build pipeline and GitHub releases
- [ ] Configurable line height in edit-mode

### Further down the road

- [!] Emoji support -> Support path doc added
- [ ] Status bar