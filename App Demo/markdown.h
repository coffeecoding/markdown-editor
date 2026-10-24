#pragma once
#include <string>

enum class MarkdownFlavor { CommonMark };
struct RenderOptions {
    MarkdownFlavor flavor = MarkdownFlavor::CommonMark;
    std::wstring bodyFont = L"Segoe UI";
    std::wstring codeFont = L"Consolas";
};
// RTF is only a disposable view; it is never used to obtain Markdown source.
bool RenderMarkdown(const std::wstring& source, const RenderOptions& options, std::string& rtf);
