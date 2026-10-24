#pragma once
#include <string>
#include <vector>
#include "theme.h"

enum class MarkdownFlavor { CommonMark, CommonMarkWithTables };
struct RenderOptions {
    MarkdownFlavor flavor = MarkdownFlavor::CommonMarkWithTables;
    std::wstring bodyFont = L"Segoe UI";
    std::wstring codeFont = L"Consolas";
    ThemePalette palette = Palette(false);
    LONG tableWidth = 9360; // Available width in twips; 6.5 inches for standalone callers.
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
struct TableLayout {
    std::vector<LONG> rows; // Native character positions at each row's start delimiter.
    std::vector<LONG> preferredWidths; // Twips, including cell padding; retained for resize.
    LONG indent = 0;
};
struct RenderedMarkdown {
    std::string rtf;
    SourceMap mapping;
    std::vector<TableLayout> tables;
};
// Change native widths without parsing or measuring again. Does not change selection.
// The caller preserves scrolling around layout changes.
bool ResizeMarkdownTables(HWND preview, const std::vector<TableLayout>& tables, LONG availableWidth);
// RTF is only a disposable view; it is never used to obtain Markdown source.
// Source uses normalized (LF) newlines, as returned by App::Source().
bool RenderMarkdown(const std::wstring& source, const RenderOptions& options, RenderedMarkdown& result);
