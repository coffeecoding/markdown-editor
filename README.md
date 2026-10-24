# Native Markdown Editor

A small, portable Markdown editor for Windows x64, built with C++17 and native
Win32 controls. Edit the Markdown source or read a formatted preview, without a
browser engine, extra runtime, installer, or startup network access.

- CommonMark headings, emphasis, lists, quotes, links, code, and Unicode.
- Separate Edit/View controls that preserve undo history, selection, and scrolling.
- Double-click the preview to edit at the corresponding source character.
- Light/dark themes, configurable colors and fonts, and per-monitor DPI scaling.

## Run

Copy `markdown-editor.exe` and `MD4C-LICENSE.txt` from [release/](release/) into
a permanent folder. The required distribution is about **323 KiB**.

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
| Edit / View                         | Ctrl+1 / Ctrl+2                     |
| Toggle Edit/View                    | Ctrl+Tab                            |
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

The renderer checks rebuild Release; `-Benchmark` also reports mapping memory
and render timings. The smoke suite exercises native windows, file handling,
themes, undo/redo, Unicode, DPI handlers, and exact source navigation. Generated
files stay under ignored `build/`. Physical mouse interaction, visible caret
appearance, and real mixed-monitor movement still need manual checks.

## Settings and file handling

**Mode → Dark theme** switches and saves the theme immediately. Colors and an
optional private TTF font are configured in `markdown-editor.ini` beside the
executable; see [the configuration example](markdown-editor.ini.example).
The app fills in missing palette defaults. Restart after changing colors or
fonts. The executable's folder must be writable to persist settings.

Markdown source is always authoritative. Saves preserve supported encodings
(UTF-8 and UTF-16 with BOM) and newline conventions; failed saves retain unsaved
work. New files use UTF-8 without BOM and CRLF. Invalid Unicode, embedded NULs,
and files over 32 MiB are rejected. There is no autosave or recovery file.

Preview uses MD4C CommonMark, with readable image and inert HTML fallbacks.
Images are not downloaded. Tables and other Markdown extensions are currently
unsupported; code is not syntax-highlighted.

## Roadmap

- [ ] Table rendering
- [ ] Emoji support
- [ ] Recent files
- [ ] Status bar
- [ ] Esc to leave Edit mode
- [ ] Application icon
- [ ] Application license
- [ ] Build pipeline and GitHub releases
