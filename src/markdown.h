#pragma once
#include <string>
#include <vector>
#include "theme.h"

enum class MarkdownFlavor { CommonMark };
struct RenderOptions {
    MarkdownFlavor flavor = MarkdownFlavor::CommonMark;
    std::wstring bodyFont = L"Segoe UI";
    std::wstring codeFont = L"Consolas";
    ThemePalette palette = Palette(false);
};
// Half-open rendered/source ranges, in UTF-16 code units.
// Each Rich Edit paragraph break occupies one unit. Generated text is a gap.
// Equal lengths map character offsets directly; decoded entities have unequal
// lengths and anchor every displayed unit at sourceBegin.
struct SourceSpan {
    LONG begin, end, sourceBegin, sourceEnd;
};
static_assert(sizeof(SourceSpan) == 16, "Keep source mapping compact");
struct SourceMap {
    std::vector<SourceSpan> spans;
    LONG SourceAt(LONG renderedOffset) const;
};
struct RenderedMarkdown {
    std::string rtf;
    SourceMap mapping;
};
// RTF is only a disposable view; it is never used to obtain Markdown source.
// Source uses normalized (LF) newlines, as returned by App::Source().
bool RenderMarkdown(const std::wstring& source, const RenderOptions& options, RenderedMarkdown& result);
