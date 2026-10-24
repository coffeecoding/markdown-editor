# Native Markdown Editor — first version

A single-document C++17 / Win32 editor for current x64 Windows. The original
root-level demo is preserved. There is no browser, WebView, .NET runtime,
installer, network fetch at startup, or package manager dependency.

## Run

The ready-built executable is `release\markdown-editor.exe`. Copy the two files
in `release` together to a permanent folder; no installation is required.

From the repository root in PowerShell:

```powershell
& '.\App Demo\release\markdown-editor.exe'
& '.\App Demo\release\markdown-editor.exe' 'C:\Notes\Grüße with spaces.md'
& '.\App Demo\release\markdown-editor.exe' "$PWD\App Demo\sample.md"
```

Without a path, the app starts with an untitled document in Edit mode. An
existing file opens in View mode. One document occupies each window; another
launch creates another independent window. Only the first command-line argument
is used, as a file path. Relative paths are resolved from the launching directory.

## Build

Install Visual Studio / Build Tools with **Desktop development with C++**, an
x64 MSVC compiler, Windows SDK, CMake 3.20+, and Ninja. The Visual Studio CMake
component includes Ninja. All parser sources are vendored; building is offline.

From ordinary PowerShell at the repository root:

```powershell
& '.\App Demo\build.ps1'
```

The script locates Visual Studio with `vswhere`, initializes its x64 environment,
configures CMake, and copies the Release executable and MD4C license to `release`.
An optional `-Configuration Debug` builds into `build\Debug` without replacing
the release. Close running release copies before rebuilding.

Alternatively, from an **x64 Native Tools Command Prompt** with Ninja on PATH:

```bat
cmake -S "App Demo" -B "App Demo\build\Release" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "App Demo\build\Release"
"App Demo\build\Release\markdown-editor.exe" "App Demo\sample.md"
```

The build uses size-oriented `/O1`, link-time optimization, dead-code folding,
and the static MSVC runtime (`/MT`). It embeds its manifest and targets x64.

## Controls

The native menu bar contains exactly File → Open…, Save, Exit and Mode → Edit,
View. The active mode has a radio check. The title shows the filename (or
Untitled), with `*` for unsaved changes. The document fills the client area.

| Action | Shortcut |
| --- | --- |
| Open | Ctrl+O |
| Save | Ctrl+S |
| Exit | Alt+F4 |
| Edit | Ctrl+1 |
| View | Ctrl+2 |
| Toggle Edit/View | Ctrl+Tab |
| Select all / copy | Ctrl+A / Ctrl+C, in either mode |
| Cut / paste | Ctrl+X / Ctrl+V or Shift+Delete / Shift+Insert, in Edit |
| Undo / redo | Ctrl+Z / Ctrl+Y or Ctrl+Shift+Z, in Edit |

Edit is a plain-text Rich Edit control, with horizontal and vertical scrolling.
View is a separate, wrapped, read-only Rich Edit control: mouse selection,
Shift+arrow selection, Select All, and copying are available, with no insertion
caret. Its visible URL destinations can be Ctrl+clicked for HTTP(S) and mailto
links. Other destinations remain readable and copyable. No URLs are opened
automatically. Native formatting and zoom shortcuts and the Rich Edit context
menu are disabled to keep the document controls simple.

Rendering occurs on opening a document and on entering View after a source
change. The source control stays alive, including its selection, scrolling, and
up to 1,000 undo actions. Each mode retains its own selection and scroll state.
After rerendering, View restores its previous character offsets and approximate
scroll position; it does not map positions semantically between source and View.

## Files and unsaved work

Open replaces the current document after Save / Discard / Cancel handling. Save
on an untitled document opens a native dialog, defaults to `.md`, and adds that
extension when omitted. Closing and session shutdown also resolve unsaved work.

The editor always saves Markdown from the source control. It never derives
source from the rendered document. A save failure keeps the document open and
dirty. Writes go to a new file beside the destination, are flushed, and then
replace the destination with Windows file APIs. This requires write access to
the containing directory. Files locked against replacement produce a clear
error and retain the editor's unsaved source.

New files use UTF-8 without BOM and Windows CRLF newlines. Existing UTF-8 BOM,
UTF-16LE BOM, and UTF-16BE BOM are preserved. LF, CRLF, or CR is selected by the
dominant existing newline convention (CRLF wins ties). An unchanged Save leaves
the file untouched; after editing a file with mixed newlines, its newlines are
normalized to the selected convention. Final newlines are retained. Invalid
Unicode, embedded NULs, and files over 32 MiB are rejected with an explanation.
Legacy ANSI encodings require conversion to UTF-8 first.

The unsaved marker is conservative during editing; undo/redo through the
keyboard and the replacement/close prompt compare against the saved source.
There is no auto-save, recovery file, external-change detection, or merge prompt.
Concurrent windows editing the same file use the last successful save.

## CommonMark and fallbacks

The application uses the small MIT-licensed
[MD4C 0.5.2 parser](https://github.com/mity/md4c/tree/release-0.5.2), with flags set
to zero for its CommonMark dialect. The parser is unmodified in
`third_party\md4c`, along with its named-entity table and license. No HTML engine
or Markdown regular-expression parser is involved.

View supports headings, paragraphs, nested emphasis and strong text, inline
code, indented and fenced code blocks, ordered and unordered nested lists,
block quotes, horizontal rules, links, hard/soft line breaks, character
references, and Unicode. Headings use larger text, code has a shaded background,
and list numbering honors its starting number. Link labels include the visible
destination so copied text retains context.

This first renderer does **not** claim complete CommonMark presentation
conformance. Images are `[Image: alt text — destination]`; nothing is downloaded.
HTML blocks are labeled `[HTML source]` and shown verbatim; inline HTML tags
also remain visible and inert. Code languages are not highlighted. Table,
task-list, strikethrough, footnote, math, and other extension syntax is not
enabled and is displayed as ordinary CommonMark text. List and quote spacing is
an approximation, with no browser layout, CSS, or image layout.

`RenderOptions::flavor` / `MarkdownFlavor` in `markdown.h` is the small internal
flavor boundary. Only CommonMark is implemented; there is no visible setting or
plugin system.

## Optional custom TTF

Copy `markdown-editor.ini.example` beside the executable as
`markdown-editor.ini`, then set a file path and restart:

```ini
[font]
path=fonts\MyFont.ttf
```

Absolute paths also work. Relative paths are resolved beside the executable,
not from the working directory. Do not quote the value. This uses Windows INI
reading: ASCII is sufficient for ordinary paths; save the INI as **UTF-16LE with
BOM** for non-ASCII paths. For example, Windows PowerShell `Set-Content -Encoding
Unicode` can create that format.

Windows reads the TTF's family name; the app loads it with `FR_PRIVATE`, without
system installation. The same family applies to source, rendered body/headings,
and code. Bold/italic may be synthesized if the TTF has only a regular face.
Normal Windows glyph fallback supplies characters the font lacks. Missing,
unreadable, invalid, or non-TTF files fall back silently. Remove the setting or
INI and restart to restore Consolas for source/code and Segoe UI for the body.
Native menus, title bars, and dialogs use Windows-managed fonts.

The embedded manifest declares **Per-Monitor V2** DPI awareness. On a DPI change,
the app accepts the suggested window rectangle, resizes the controls, scales
document margins and Rich Edit text, and restores selection and approximate
scroll position without rebuilding source or its undo buffer. DPI text scaling
is relative to the screen DC's actual logical DPI, including systems whose
primary display starts above 100%.

## Windows “Open with” and defaults

Place `release` in its permanent location first. In File Explorer, right-click
a `.md` file → **Open with** → **Choose another app**. Choose **Choose an app on
your PC** / **Look for another app on this PC** (wording depends on Windows),
then select `markdown-editor.exe`. Choose **Just once** for a single opening or
**Always** / **Always use this app** to make it the `.md` default.

You can also use **Settings → Apps → Default apps**, search for `.md`, and select
the editor. See Microsoft's [default-app instructions](https://support.microsoft.com/en-us/windows/apps/change-default-apps-in-windows).
The app does not edit registry associations or change defaults itself.

## Delivery and verification — 23 October 2026

Built with MSVC 19.51 (Visual Studio 2026), Windows SDK 10.0.26100.0, CMake and
Ninja on Windows x64. Release build completed without warnings.

| Required shipped file | Bytes |
| --- | ---: |
| `markdown-editor.exe` | 289,280 |
| `MD4C-LICENSE.txt` | 1,096 |
| **Total: 2 files** | **290,376 (283.57 KiB)** |

Only Windows DLLs are imported; Rich Edit is loaded from Windows System32.
No VC runtime redistribution is needed. Optional configuration, user fonts,
source, documentation, sample files, and build artifacts are excluded from the
required distribution total. The first build is well below the 2 MB target;
no iterative size tuning or startup benchmarking was done.

Run the brief native integration smoke check after building:

```powershell
& '.\App Demo\smoke.ps1'
```

It uses temporary documents and executable copies under `build\smoke` and
Windows APIs, with no test-framework installation. It checks quoted Unicode
paths, raw source versus rendered text, entities/Unicode, fallback content,
exact menus and mode checks, Select All/copy, read-only View, edits and dirty
titles, rerendering, undo/redo across modes, native Open/Save dialogs, extension
completion, Cancel/Discard, failure-safe saving against a locked file, UTF-8 and
both UTF-16 BOM encodings, and LF/CRLF retention. It checks a configured Arial
TTF in both controls and graceful fallback for a missing file. The prior
clipboard text is restored when present.

The check also confirmed the window's actual PMv2 awareness and exercised the
DPI handler at 96/144/192 DPI via synthetic messages on this 144-DPI session.
Those checks and a visible-window capture verify native rendering and scaling
plumbing; they do not prove physical mixed-monitor behavior. Actual monitor
movement, selection by physical mouse/keyboard, caret visibility while actively
focused, link launching, shutdown prompts, and a font absent from the system
font inventory still need manual review. This execution environment prevented
foreground keyboard automation, so accelerator behavior is implemented and
documented but not claimed as interactively verified. Startup was not timed.
