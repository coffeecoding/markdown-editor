# Agent Instructions

- Maintain and incrementally improve this small native Windows Markdown editor. Keep it fast and portable on x64, with offline builds and required distribution **well under 1 MB**; no installer or extra runtime.
- Application code lives in `src/`; the root CMake project builds the editor. Read the root `README.md` for behavior and limitations.
- Use C++17, Win32, Rich Edit, and vendored MD4C. Prefer existing dependencies and native APIs; avoid frameworks, browser engines, and unnecessary background work.
- Code map within `src/`: `main.cpp` = UI/settings; `document.cpp` = encoding/safe file I/O; `markdown.cpp` = parsing/RTF/source mapping; `theme.h` = palettes. Tests and vendored dependencies live in root `tests/` and `third_party/`.
- Markdown source is authoritative; RTF is disposable. Keep the source control alive; preserve undo/redo, dirty state, selections, and scrolling across mode, render, theme, and DPI changes.
- Preserve Unicode, encoding/BOM, and newline conventions. Internal source uses LF; mapping uses UTF-16 units and one unit per paragraph break. Never split surrogate pairs.
- Use MD4C callback locations and the existing render lifecycle. Keep mapping compact with spans and sparse checkpoints; avoid text searches, per-character tables, and custom glyph layout.
- Build from root PowerShell: `& '.\build.ps1'` (MSVC/Windows SDK/CMake/Ninja). `-Configuration Debug` leaves the shipped Release executable untouched.
- Verify application changes with `& '.\smoke.ps1'`; renderer/navigation changes also need `& '.\tests\check.ps1'` (`-Benchmark` for memory/timings). Put generated output and scratch files under ignored `build/`.
- Deliver application changes with rebuilt `release/markdown-editor.exe`, its license, and updated README. Report executable growth and relevant performance deltas.
- Hidden-window tests do not verify physical mouse interaction, visible caret/palette appearance, or real mixed-monitor DPI movement. State manual verification limits accurately.
