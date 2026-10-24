# Emoji support implementation path

Status: implementation postponed. Research recorded on 2026-10-24.

The initial scope is attractive, full-color rendering of literal Unicode emoji
in View mode. GitHub-style `:smile:` conversion is a separate feature.

The working target is visible color emoji in the first iteration, followed by
integration and regression fixes over the next few iterations. A few hours is a
plausible target if the native rendering probe succeeds; this is an estimate,
not a measured implementation duration. No color-rendering prototype has yet
been compiled or visually verified for this project.

## Five implementation steps

The following five steps are preserved verbatim from the implementation proposal.

1. **Prove color rendering in a tiny executable under `build/`.** Use the system’s Rich Edit text services with Direct2D and enable color emoji. Render `😀 ❤️ 👍🏽 👩‍💻 👨‍👩‍👧‍👦` alongside formatted text and a table. Check actual colors, joined sequences, and hit-testing. Windows exposes the required drawing interface through [`ITextServices2::TxDrawD2D`](https://learn.microsoft.com/en-us/windows/win32/api/textserv/nf-textserv-itextservices2-txdrawd2d); its behavior with our content is what needs proving.

2. **Put the working renderer behind a small preview control.** Add `src/preview.h/.cpp` to own the native text-services instance and Direct2D resources. Forward the existing Rich Edit messages through it so selection, copying, scrolling, links, and navigation continue using Rich Edit’s implementation. The source editor keeps its existing control and undo history.

3. **Use the same engine for table measurement.** Update `CellMeasurer` alongside View so measured widths match displayed text, including emoji.

4. **Test the features that can regress.** Cover emoji in headings, links, code, and tables; skin tones and joined sequences; Unicode copying; double-click navigation; live font/theme changes; scrolling and DPI. Keep the existing UTF-16 mapping unless a measured difference requires adjustment.

5. **Build and deliver.** Run smoke and renderer/navigation checks with benchmarks, inspect rendered captures, rebuild Release, and report size/performance changes. Use Windows fonts and graphics libraries to keep the distribution small.

## What the existing code already provides

- `src/markdown.cpp` emits Unicode into RTF as signed UTF-16 `\u` escapes.
  Supplementary characters are emitted as their surrogate pairs. Existing
  renderer/navigation and smoke checks already contain emoji such as `😀`.
  The missing feature is improved presentation, especially color and complete
  joined sequences, rather than basic Unicode storage or transport.
- `src/main.cpp` loads the system `Msftedit.dll` and creates both source and
  preview windows with `MSFTEDIT_CLASS`. View currently receives disposable RTF
  through `EM_STREAMIN`.
- `RenderOptions` in `src/markdown.h` already separates prose and code fonts.
  Source mapping uses compact spans in UTF-16 units, with one unit per paragraph
  break. Keeping emoji as text is intended to preserve this representation.
- `CellMeasurer` in `src/markdown.cpp` uses another system Rich Edit window,
  streams formatted cell RTF, and obtains native positions with
  `EM_POSFROMCHAR`. Changing only the visible preview could make its metrics
  disagree with table measurement; both paths need the same rendering/layout
  configuration.
- Layout, selection/scroll restoration, DPI zoom, copying, links, table resizing,
  and double-click source navigation currently communicate with the preview
  through its HWND and Rich Edit messages. The proposed preview host must
  preserve that contract or explicitly adapt the affected calls.

## Researched native Windows path

### Color rendering and the current window control

Microsoft's Rich Edit engineering articles distinguish the traditional
GDI/Uniscribe window-control path from the Direct2D/DirectWrite path that supports
color fonts and emoji. Selecting `Segoe UI Emoji` alone does not establish that
our existing window control can render its color layers.

The articles describe `RichEditD2D` and `RichEditD2DPT` window classes, and
`EM_SWITCHTOD2D` (`WM_USER + 389`), in the Microsoft 365 Rich Edit implementation.
They explicitly distinguish that implementation from the system `Msftedit.dll`.
The Windows 11 Notepad article likewise explains that its Rich Edit build comes
from the Office codebase rather than the system DLL. These articles date from
2020 and 2022; they establish the architectural distinction, not the behavior
of every current Windows build.

Consequently, the plan does not assume that sending `EM_SWITCHTOD2D` to the
current preview is a supported system-DLL solution. Loading a DLL from Office
or a Notepad installation would introduce an installation dependency and is
outside the proposed portable approach.

Sources:

- [RichEditD2D Window Controls](https://devblogs.microsoft.com/math-in-office/richeditd2d-window-controls/)
- [Windows 11 Notepad](https://devblogs.microsoft.com/math-in-office/windows-11-notepad/)

### Windowless Rich Edit with Direct2D

The documented candidate is a native text-services object hosted by our own
Win32 preview window:

- Implement `ITextHost` before calling `CreateTextServices`. The text-services
  object calls the host for drawing invalidation, bounds, device contexts,
  scrollbars, timers, capture, focus, caret services, and notifications.
- Obtain `ITextServices2` and render through `TxDrawD2D`. Microsoft documents
  this method in `Msftedit.dll`, with Windows 8 as the minimum supported client.
  It takes a Direct2D render target, bounds, an optional update rectangle, and
  an active/inactive view identifier.
- Forward applicable Rich Edit messages through `ITextServices::TxSendMessage`.
  Message compatibility with our existing preview operations must be checked
  in the probe and integration tests.
- Microsoft's Rich Edit article gives the windowless color-font opt-in as
  `EM_SETTYPOGRAPHYOPTIONS`, with both `wParam` and `lParam` set to
  `TO_DEFAULTCOLOREMOJI | TO_DISPLAYFONTCOLOR`. It gives the flag values as
  `0x1000` and `0x2000`, respectively. These instructions still need runtime
  verification against the system DLL used by this application.

The local Windows SDK `10.0.26100.0` was inspected. Its `um/TextServ.h` declares
`IID_ITextServices2`, `ITextHost`/`ITextHost2`, and
`ITextServices2::TxDrawD2D`. Header availability and documented API support do
not prove that our proposed host produces correct colored output and metrics.

Sources:

- [ITextHost](https://learn.microsoft.com/en-us/windows/win32/api/textserv/nl-textserv-itexthost)
- [ITextServices2](https://learn.microsoft.com/en-us/windows/win32/api/textserv/nl-textserv-itextservices2)
- [ITextServices2::TxDrawD2D](https://learn.microsoft.com/en-us/windows/win32/api/textserv/nf-textserv-itextservices2-txdrawd2d)
- [Using RichEdit for Text Processing](https://devblogs.microsoft.com/math-in-office/using-richedit-for-text-processing/)
- [RichEditD2D Window Controls](https://devblogs.microsoft.com/math-in-office/richeditd2d-window-controls/)

### Fonts and shaping

Microsoft documents Segoe UI Emoji as a color font and describes Rich Edit font
binding/fallback. DirectWrite/Direct2D provide native color-glyph rendering;
using a color-capable font and using a color-capable rendering path are both
relevant. Direct2D's own `DrawText`/`DrawTextLayout` APIs require
`D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT` to opt into color fonts. That flag is
guidance for those drawing APIs, not a substitute for configuring Rich Edit's
text-services path.

Keep shaping and glyph positioning in the Windows text engine. The proposal
does not require a browser engine, a bundled emoji font/image pack, or custom
glyph layout. The expected small distribution relies on Windows-provided
components; actual executable growth must be measured after integration.

Sources:

- [Color font support](https://learn.microsoft.com/en-us/windows/win32/directwrite/color-fonts)
- [RichEdit Font Binding](https://devblogs.microsoft.com/math-in-office/richedit-font-binding/)

## Probe acceptance and remaining uncertainties

The first step should establish all of the following before adopting the host:

- Actual colored glyphs and correctly shaped supported sequences: simple emoji,
  variation selectors (`♥` versus `♥️`), skin tones, ZWJ sequences, flags, and
  keycaps. Coverage depends on the installed Windows fonts and text engine.
- Correct native positions and hit-testing, Unicode clipboard text, and stable
  UTF-16 offsets around emoji, including in tables and formatted spans.
- Agreement between visible layout and cell measurement, including wrapping,
  row heights, and baselines.
- Correct rendering with both palettes, custom prose/code fonts, and DPI scaling.

No runtime result yet establishes the amount of preview-host adaptation needed,
exact sequence coverage across Windows versions, or the final size/performance
cost. If the proposed system-DLL path fails these checks, record the observed
limitation before choosing another approach. Inline image substitution has not
been prototyped; it would require separate work on Unicode copying, mapping,
scaling, and accessibility and is not the default path in this proposal.

## Verification and delivery requirements

Follow the repository's `AGENTS.md` throughout implementation. Keep Markdown
source authoritative and its control alive; preserve undo/redo, dirty state,
selection, scrolling, Unicode, encoding/BOM, and newline behavior. Retain compact
MD4C-based mapping and defer hidden preview work until View is requested.

Use `& '.\build.ps1' -Configuration Debug` during development. Before delivery,
run `& '.\smoke.ps1'` and `& '.\tests\check.ps1' -Benchmark`, rebuild the Release
executable and license, and update the README and emoji roadmap entry to match
the behavior actually verified. Report executable growth and relevant render,
table measurement/resize, and mapping-memory results. The required distribution
must remain well under 1 MB, with offline builds and no installer or extra runtime.

Inspect rendered captures and perform visible UI checks where possible.
Hidden-window tests alone cannot establish physical mouse behavior, visible
caret/palette appearance, or real mixed-monitor DPI behavior. Record those
verification limits explicitly.
