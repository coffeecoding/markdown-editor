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

The native menu bar contains File → Open…, Save, Exit and Mode → Edit,
View, followed by a separator and Dark theme. The active mode has a radio check.
Dark theme has an independent check. The title shows the filename (or
Untitled), with `*` for unsaved changes. The document fills the client area.

| Action | Shortcut |
| --- | --- |
| Open | Ctrl+O |
| Save | Ctrl+S |
| Exit | Alt+F4 |
| Edit | Ctrl+1 |
| View | Ctrl+2 |
| Toggle Edit/View | Ctrl+Tab |
| Edit at the corresponding source character | Double-click View, without modifiers |
| Select all / copy | Ctrl+A / Ctrl+C, in either mode |
| Cut / paste | Ctrl+X / Ctrl+V or Shift+Delete / Shift+Insert, in Edit |
| Undo / redo | Ctrl+Z / Ctrl+Y or Ctrl+Shift+Z, in Edit |

Edit is a plain-text Rich Edit control, with horizontal and vertical scrolling.
View is a separate, wrapped, read-only Rich Edit control: mouse selection,
Shift+arrow selection, Select All, and copying are available, with no insertion
caret. Its visible URL destinations can be Ctrl+clicked for HTTP(S) and mailto
links. Other destinations remain readable and copyable. No URLs are opened
automatically. Right-click (or Shift+F10 / the Menu key) opens a native context
menu: Copy, Cut, and Paste in Edit mode, and Copy in View mode. Copy/Cut are
disabled without a selection; Paste is disabled when no compatible text is
available. Pasting inserts plain text and supports undo. Native formatting and
zoom shortcuts remain disabled.

Rendering occurs on opening a document and on entering View after a source
change. The source control stays alive, including its selection, scrolling, and
up to 1,000 undo actions. Each mode retains its own selection and scroll state.
After rerendering, View restores its previous character offsets and approximate
scroll position.

An unmodified left-button double-click in View enters Edit at the corresponding
Markdown source character and scrolls the collapsed caret into view. This also
works in document margins and blank document space. It deliberately replaces
the previous source selection and scroll position; ordinary menu/keyboard mode
changes keep their existing behavior. Source text, undo/redo, and unsaved state
are preserved. Single-click/drag selection, Ctrl+click links, context menus, and
double-click word selection in Edit continue to use the native controls.

Navigation uses 16-byte source-range spans recorded from MD4C's input-buffer
text callbacks, with UTF-16 offsets and one unit per Rich Edit paragraph break.
Directly sourced text maps by character offset, including repeated text,
escapes, code, and Unicode. Hidden Markdown delimiters create source gaps.
Decoded entities are atomic: every displayed UTF-16 unit maps to the start of
its source spelling. Unknown entity names that remain verbatim map directly.

Generated bullets, labels, destinations, breaks, and blank space choose the
nearest mapped span by rendered-character distance, preferring the preceding
span on ties. Approaching from afterward uses that span's source end;
approaching from before uses its source beginning. Documents without mapped
text use zero. Native Rich Edit determines the clicked character. The source
offset is clamped to its bounds and moved backward if needed to avoid splitting
a surrogate pair, using a two-unit native text-range read.

Temporary UTF-8-to-UTF-16 checkpoints occur about every 256 bytes at codepoint
boundaries; one cached resolution avoids rescanning nearby callback prefixes.
There is no per-character table or additional persistent source copy. The
mapping is replaced only after successful preview streaming, including theme
refreshes. Parsing, streaming, and navigation keep the existing rendering
lifecycle and dependencies.

## Light and dark themes

Choose **Mode → Dark theme** to switch immediately; unchecked means Light.
Both document modes share these colors:

| Color | Light | Dark |
| --- | --- | --- |
| Document background | `#F5F3EE` | `#000000` |
| Main text | `#242424` | `#D0D0D0` |
| Links | `#005BAE` | `#79B8FF` |
| Quotes and rules | `#62605C` | `#A0A0A0` |
| Code background | `#E8E5DE` | `#181818` |

The app requests a matching native title bar where Windows supports it. Menus,
context menus, selection highlighting, and common dialogs keep Windows-managed
appearance. Custom document fonts and DPI scaling continue to apply.

The choice is saved immediately beside the executable in `markdown-editor.ini`,
preserving existing font settings:

```ini
[appearance]
theme=dark
```

Missing or invalid theme selections default to Light. If the preference cannot
be written, the selected theme stays active for this session and an error
identifies the configuration path. The executable's directory must be writable
to retain the choice across launches.

Both document palettes are also stored in that INI. On startup, the app adds
missing color entries with the defaults below, preserving existing colors,
font settings, theme selection, comments, and unrelated entries. Existing
installations gain these entries on their next launch. Edit the colors and
restart to apply them; switching themes selects the palettes loaded at startup
and saves only `[appearance] theme`.

```ini
[theme.light]
background=#F5F3EE
text=#242424
link=#005BAE
muted=#62605C
code=#E8E5DE

[theme.dark]
background=#000000
text=#D0D0D0
link=#79B8FF
muted=#A0A0A0
code=#181818
```

`background` is the document background; `text` is the main text color; `link`
colors links; `muted` colors quotes and rules (and fallback labels); `code` is
the inline/block code background. Values accept `#RRGGBB`, case-insensitive,
with surrounding whitespace. An invalid or empty value falls back individually
to its compiled default and remains unchanged in the INI for correction. If
adding missing defaults fails, the editor continues with loaded/default colors
and shows one configuration persistence error after creating its window.

These ten settings cover the five document color roles in each theme. Native
title-bar requests, Windows-managed menus, selection highlighting, and dialogs
retain their existing behavior. There is no settings UI, file watcher, or
continuous background work. `markdown-editor.ini.example` includes all keys.

Switching preserves source text, undo/redo, unsaved state, selection, and scroll
position. View regenerates once and restores its selection and approximate
scroll position. In Edit mode, regeneration waits until View is entered. There
is no polling or background theme work; large documents may pause during a
View refresh.

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

## Delivery and verification — 24 October 2026

Built with MSVC 19.51 (Visual Studio 2026), Windows SDK 10.0.26100.0, CMake and
Ninja on Windows x64. Release build completed without warnings.

| Required shipped file | Bytes |
| --- | ---: |
| `markdown-editor.exe` | 329,728 |
| `MD4C-LICENSE.txt` | 1,096 |
| **Total: 2 files** | **330,824 (323.07 KiB)** |

Only Windows DLLs are imported; Rich Edit is loaded from Windows System32.
No VC runtime redistribution is needed. Optional configuration, user fonts,
source, documentation, sample files, and build artifacts are excluded from the
required distribution total. The build remains well below the 2 MB target;
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

The earlier theme update added 2,048 bytes to the executable, with no
bundled dependency added. The extended smoke check passed both palettes for
source, headings/body, links including detected URLs, quotes/rules, and inline
and block code. It checks document backgrounds through the native control API,
custom fonts, 96/144/192 DPI handlers, clean/dirty state, undo/redo across repeated
switches, selections, clipboard copying, and nonzero scrolling in both controls.
It also checks missing/invalid preferences, startup persistence, preservation of
UTF-16 INI font settings/comments, and a locked configuration file producing an
error while keeping the session's theme active.

In a local hidden-window run, an ordinary document's palette checks plus four
switches took 39 ms; one View switch for a 405,000-character document took 45 ms.
These are one-run observations, not benchmark guarantees. Desktop capture in
this execution session returned blank images, so the theme update's visual
appearance, native title bar, physical mixed-monitor behavior, and interactive
keyboard/mouse use still need manual review.

The context-menu update was also checked through native popup menus: Edit's
Copy/Cut/Paste, disabled Copy/Cut without a selection, Unicode clipboard text,
undo after Cut/Paste, and View's Copy-only menu all passed.

The check also confirmed the window's actual PMv2 awareness and exercised the
DPI handler at 96/144/192 DPI via synthetic messages on this 144-DPI session.
Those checks and a visible-window capture verify native rendering and scaling
plumbing; they do not prove physical mixed-monitor behavior. Actual monitor
movement, selection by physical mouse/keyboard, caret visibility while actively
focused, link launching, shutdown prompts, and a font absent from the system
font inventory still need manual review. This execution environment prevented
foreground keyboard automation, so accelerator behavior is implemented and
documented but not claimed as interactively verified. Startup was not timed.

The earlier editable-palette and line-navigation update added 36,352 bytes over
the prior 292,352-byte executable, producing the 328,704-byte baseline. Precise
cursor navigation adds **1,024 bytes (0.31%)**, producing the shipped
**329,728-byte (322 KiB)** executable, with no new dependencies. The extended
native smoke check verifies
all ten defaults are populated, partial and custom palettes in both themes,
case and whitespace handling, invalid/empty entries, preserved settings and
comments, restart application, startup-only color loading, and one startup
error for an unwritable configuration while loaded/default colors remain active.

Navigation checks assert exact UTF-16 source offsets within formatted words,
links, escapes, and repeated text. They cover headings, paragraphs and
continuations, nested and ordered lists, quotes, fenced/indented and multiline
inline code, tabs, links,
images/HTML fallbacks, generated text, nearest-span ties, repeated text,
supplementary Unicode and entities, document margins, blank space, empty and
generated-only documents, and positions beyond 65,535 rendered characters.
They repeat after edits, wrapping at a narrow width, scrolling, theme refreshes,
and synthetic 96/144/192 DPI changes, and confirm source/dirty state, undo/redo
availability and operation, collapsed selections, and scrolling the source caret
into view. Ordinary mode
changes, native Edit double-click selection, modified View double-clicks,
single-click caret positioning, and nonclient scrollbar messages are checked.

Additional renderer checks compare every direct span against actual native Rich
Edit text, verify every mapped UTF-16 unit, adjacent/multi-unit/supplementary
entities, unknown entity names, generated gaps and ties, final source extents,
empty results, failed-render preservation, and sparse checkpoint lookups in
both directions across long Unicode lines. Run them from ordinary PowerShell:

```powershell
& '.\App Demo\tests\check.ps1' -Benchmark
& '.\App Demo\smoke.ps1'
```

`check.ps1` rebuilds Release and builds the separate native check executable
under `build\mapping-tests`; no check code is shipped. `-Benchmark` also prints
mapping vector sizes/capacities, temporary checkpoint storage, and renderer
timings for the existing large sample and a densely formatted single line.
Both the renderer checks and the extended integration smoke suite passed.

The before/after comparison below uses identical MSVC `/O1` + LTCG builds. The
large sample is the smoke suite's 5,000 repetitions (405,000 file UTF-16 units
with CRLF; 385,000 normalized source units). The dense sample repeats this unit
plus a trailing space 10,000 times on one line (460,000 UTF-16 units; 480,000
UTF-8 bytes):

```text
**bold** *em* `code` [label](url) &amp; \* 😀
```

Sizes describe one result's vector payload; allocator overhead,
vector objects, the eight-byte cached resolution, existing source/RTF buffers,
and the previous preview mapping retained during refresh are excluded.

| Measurement | Large before | Large after | Dense before | Dense after |
| --- | ---: | ---: | ---: | ---: |
| Persistent span count | 15,000 | 40,000 | 10,001 | 110,000 |
| Live span bytes | 180,000 | 640,000 | 120,012 | 1,760,000 |
| Span vector capacity, bytes | 218,484 | 655,440 | 145,656 | 2,212,080 |
| Temporary lookup live bytes | 320,016 | 12,032 | 16 | 15,000 |
| Temporary lookup capacity, bytes | 436,960 | 12,792 | 16 | 19,184 |
| Renderer median, ms | 9.27 | 8.00 | 12.32 | 13.52 |
| Native View theme refresh median, ms | 45.886 | 44.444 | 360.674 | 360.525 |

Retained mapping capacity grew by 436,956 bytes for the large document and
2,066,424 bytes for the dense one. Temporary lookup capacity changed by
−424,168 and +19,168 bytes respectively. The large sample's temporary lookup
storage shrinks by replacing its line table with sparse checkpoints. The dense
sample's larger map is expected: precise offsets require separate spans at
hidden delimiters and entity boundaries, while line-only navigation could
collapse them. Its 110,000 spans each remain exactly 16 bytes. No per-character
mapping table was introduced.

Renderer figures are medians of three runs, each with three warmups and nine
timed renders: −1.27 ms for the large sample and +1.20 ms for the dense sample.
Native figures include parsing, RTF streaming, and restoring View, using three
warmups and seven timed theme refreshes per sample/version; their differences
are −1.442 ms and −0.149 ms. These local hidden-window measurements have normal
timing noise and are observations, not performance guarantees. An initial
dense-render slowdown was traced to rescanning checkpoint prefixes for nearby
callbacks and removed by caching one recent resolution.

These checks send native messages to hidden windows. Physical double-click and
drag selection, Ctrl+click launching, visible caret and palette appearance,
native title bars, and actual movement between monitors still need manual
review. Synthetic drag messages retain View, but this session cannot verify
selection driven by a physically held mouse button.
