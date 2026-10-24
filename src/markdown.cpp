#include "markdown.h"
#include "document.h"
#include "md4c.h"
extern "C" {
#include "entity.h"
}
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <richedit.h>
#include <vector>

namespace {
// Only ASCII RTF syntax leaves this boundary; document text is escaped, and
// Unicode is represented as signed UTF-16 \u values. Raw HTML is inert text.
void Escape(std::string& out, const std::wstring& text)
{
    for (wchar_t c : text) {
        switch (c) {
        case L'\\': case L'{': case L'}': out += '\\'; out += static_cast<char>(c); break;
        case L'\n': out += "\\line "; break;
        case L'\r': break;
        case L'\t': out += "\\tab "; break;
        default:
            if (c >= 32 && c < 127) out += static_cast<char>(c);
            else out += "\\u" + std::to_string(static_cast<int16_t>(c)) + "?";
        }
    }
}

std::wstring Codepoint(unsigned value)
{
    if (!value || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return L"\xfffd";
    if (value < 0x10000) return std::wstring(1, static_cast<wchar_t>(value));
    value -= 0x10000;
    return {static_cast<wchar_t>(0xd800 + (value >> 10)), static_cast<wchar_t>(0xdc00 + (value & 0x3ff))};
}

std::wstring DecodeEntity(const char* data, size_t size)
{
    if (size > 3 && data[1] == '#') {
        unsigned value = 0;
        size_t i = 2;
        unsigned base = 10;
        if (data[i] == 'x' || data[i] == 'X') { ++i; base = 16; }
        for (; i + 1 < size; ++i) {
            unsigned digit = data[i] >= '0' && data[i] <= '9' ? data[i] - '0' :
                data[i] >= 'a' && data[i] <= 'f' ? data[i] - 'a' + 10 : data[i] - 'A' + 10;
            if (value > 0x10ffff / base) return L"\xfffd";
            value = value * base + digit;
        }
        // CommonMark's HTML numeric character reference replacements.
        static const unsigned c1[] = {0x20ac,0x81,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,
            0x2c6,0x2030,0x160,0x2039,0x152,0x8d,0x17d,0x8f,0x90,0x2018,0x2019,0x201c,
            0x201d,0x2022,0x2013,0x2014,0x2dc,0x2122,0x161,0x203a,0x153,0x9d,0x17e,0x178};
        if (value >= 0x80 && value <= 0x9f) value = c1[value - 0x80];
        return Codepoint(value);
    }
    if (const ENTITY* entity = entity_lookup(data, size)) {
        auto result = Codepoint(entity->codepoints[0]);
        if (entity->codepoints[1]) result += Codepoint(entity->codepoints[1]);
        return result;
    }
    return FromUtf8(std::string(data, size));
}

std::wstring Attribute(const MD_ATTRIBUTE& attr)
{
    std::wstring result;
    for (unsigned i = 0; attr.size && attr.substr_offsets[i] < attr.size; ++i) {
        const auto begin = attr.substr_offsets[i];
        const auto length = attr.substr_offsets[i + 1] - begin;
        if (attr.substr_types[i] == MD_TEXT_ENTITY) result += DecodeEntity(attr.text + begin, length);
        else if (attr.substr_types[i] == MD_TEXT_NULLCHAR) result += L'\xfffd';
        else result += FromUtf8(std::string(attr.text + begin, length));
    }
    return result;
}

constexpr LONG CellPadding = 80; // Four points on either side.
constexpr LONG MinimumColumn = 360; // Eighteen points, including padding.

std::string RtfHeader(const RenderOptions& options)
{
    std::string out = "{\\rtf1\\ansi\\ansicpg1252\\deff0\\uc1{\\fonttbl{\\f0\\fnil ";
    Escape(out, options.bodyFont);
    out += ";}{\\f1\\fmodern ";
    Escape(out, options.codeFont);
    out += ";}}{\\colortbl;";
    for (COLORREF color : {options.palette.text, options.palette.link, options.palette.muted,
                           options.palette.code, options.palette.tableBorder})
        out += "\\red" + std::to_string(GetRValue(color)) + "\\green" +
            std::to_string(GetGValue(color)) + "\\blue" + std::to_string(GetBValue(color)) + ";";
    return out + "}\\viewkind4\\cf1 ";
}

bool StreamRtf(HWND control, const std::string& rtf)
{
    struct Input { const std::string& text; size_t offset = 0; } input{rtf};
    EDITSTREAM stream{};
    stream.dwCookie = reinterpret_cast<DWORD_PTR>(&input);
    stream.pfnCallback = [](DWORD_PTR cookie, LPBYTE buffer, LONG requested, LONG* supplied) -> DWORD {
        auto& data = *reinterpret_cast<Input*>(cookie);
        const size_t count = std::min<size_t>(requested, data.text.size() - data.offset);
        std::memcpy(buffer, data.text.data() + data.offset, count);
        data.offset += count;
        *supplied = static_cast<LONG>(count);
        return 0;
    };
    SendMessageW(control, EM_STREAMIN, SF_RTF, reinterpret_cast<LPARAM>(&stream));
    return !stream.dwError;
}

// Water-fill the widest columns: short columns retain their natural widths.
// If even the minimums overflow, native horizontal scrolling exposes them.
std::vector<LONG> ColumnWidths(const TableLayout& table, LONG available)
{
    auto widths = table.preferredWidths;
    if (widths.empty()) return widths;
    const LONG budget = std::max<LONG>(MinimumColumn * static_cast<LONG>(widths.size()), available - table.indent);
    const auto sumAt = [&](LONG cap) {
        int64_t sum = 0;
        for (LONG width : widths) sum += std::min(width, cap);
        return sum;
    };
    LONG low = MinimumColumn, high = *std::max_element(widths.begin(), widths.end());
    if (sumAt(high) <= budget) return widths;
    while (low < high) {
        const LONG middle = low + (high - low + 1) / 2;
        if (sumAt(middle) <= budget) low = middle;
        else high = middle - 1;
    }
    LONG remainder = budget - static_cast<LONG>(sumAt(low));
    for (auto& width : widths) {
        const LONG preferred = width;
        width = std::min(width, low);
        if (remainder && preferred > low) { ++width; --remainder; }
    }
    return widths;
}

struct CellMeasurer {
    HMODULE library = nullptr;
    HWND control = nullptr;
    LONG baseDpi = 96;
    ~CellMeasurer() { if (control) DestroyWindow(control); if (library) FreeLibrary(library); }

    LONG Width(const std::string& rtf)
    {
        if (!control) {
            library = LoadLibraryExW(L"Msftedit.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (!library) return -1;
            control = CreateWindowExW(WS_EX_TOOLWINDOW, MSFTEDIT_CLASS, L"",
                WS_POPUP | ES_MULTILINE | ES_AUTOHSCROLL, 0, 0, 1, 1,
                nullptr, nullptr, nullptr, nullptr);
            if (!control) return -1;
            HDC dc = GetDC(control);
            baseDpi = GetDeviceCaps(dc, LOGPIXELSX);
            ReleaseDC(control, dc);
            SendMessageW(control, EM_EXLIMITTEXT, 0, 32 * 1024 * 1024);
            SendMessageW(control, EM_SETUNDOLIMIT, 0, 0);
            // A huge formatting rectangle and native nonwrapping layout allow
            // EM_POSFROMCHAR to measure shaped text, including font fallback.
            RECT rect{0, 0, 0x3fffffff, 1000};
            SendMessageW(control, EM_SETRECTNP, 0, reinterpret_cast<LPARAM>(&rect));
        }
        if (!StreamRtf(control, rtf)) return -1;
        GETTEXTLENGTHEX length{GTL_PRECISE | GTL_NUMCHARS, 1200};
        const LONG end = static_cast<LONG>(SendMessageW(control, EM_GETTEXTLENGTHEX, reinterpret_cast<WPARAM>(&length), 0));
        POINTL first{}, last{};
        SendMessageW(control, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&first), 0);
        SendMessageW(control, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&last), end);
        // Round up and leave one pixel of slack for native pixel rounding.
        return std::max<LONG>(MinimumColumn, MulDiv(std::max<LONG>(0, last.x - first.x) + 1, 1440, baseDpi) + 2 * CellPadding);
    }
};

struct Renderer {
    struct List { bool ordered; unsigned next; bool tight; };
    struct Checkpoint { MD_SIZE byte; LONG source; };
    const std::string& input;
    const RenderOptions options;
    const std::string header;
    std::string out;
    std::vector<Checkpoint> checkpoints;
    Checkpoint resolved{0, 0};
    SourceMap mapping;
    LONG displayed = 0;
    std::vector<List> lists;
    std::vector<bool> firstItemParagraph;
    unsigned quoteDepth = 0;
    bool paragraphOpen = false;
    struct Cell { std::string rtf; };
    std::vector<std::vector<Cell>> tableRows;
    std::vector<TableLayout> tables;
    TableLayout table;
    std::string beforeTable;
    bool nativeTable = false;
    CellMeasurer measurer; // No HWND or library load until the first native table.

    explicit Renderer(const std::string& utf8, const RenderOptions& settings = {})
        : input(utf8), options(settings), header(RtfHeader(options)), out(header)
    {
        checkpoints.push_back({0, 0});
        LONG sourceOffset = 0;
        for (size_t i = 0; i < input.size(); ++i) {
            const auto c = static_cast<unsigned char>(input[i]);
            // UTF-8 was validated by ToUtf8. Count only leading bytes, with
            // supplementary characters taking two UTF-16 units.
            if ((c & 0xc0) != 0x80) {
                if (i - checkpoints.back().byte >= 256)
                    checkpoints.push_back({static_cast<MD_SIZE>(i), sourceOffset});
                sourceOffset += c >= 0xf0 ? 2 : 1;
            }
        }
    }

    LONG SourceAt(size_t offset)
    {
        // Nearby callbacks normally advance through the same checkpoint. Reuse
        // the last resolution rather than repeatedly scanning its UTF-8 prefix.
        // Backward or distant offsets always start at a sparse checkpoint.
        if (offset < resolved.byte || offset - resolved.byte > 256)
            resolved = *(std::upper_bound(checkpoints.begin(), checkpoints.end(), offset,
                [](size_t at, const Checkpoint& candidate) { return at < candidate.byte; }) - 1);
        LONG position = resolved.source;
        for (size_t i = resolved.byte; i < offset; ++i) {
            const auto c = static_cast<unsigned char>(input[i]);
            if ((c & 0xc0) != 0x80) position += c >= 0xf0 ? 2 : 1;
        }
        resolved = {static_cast<MD_SIZE>(offset), position};
        return position;
    }

    void Emit(const std::wstring& text, LONG sourceBegin = -1, LONG sourceEnd = -1)
    {
        const LONG begin = displayed;
        Escape(out, text);
        displayed += static_cast<LONG>(text.size() - std::count(text.begin(), text.end(), L'\r'));
        if (sourceBegin < 0 || begin == displayed) return;
        auto& spans = mapping.spans;
        // Only merge direct mappings with contiguous source and display ranges.
        // Entity spans stay separate, retaining their individual source anchors.
        if (!spans.empty() && spans.back().end == begin && spans.back().sourceEnd == sourceBegin &&
            spans.back().end - spans.back().begin == spans.back().sourceEnd - spans.back().sourceBegin &&
            displayed - begin == sourceEnd - sourceBegin) {
            spans.back().end = displayed;
            spans.back().sourceEnd = sourceEnd;
        } else spans.push_back({begin, displayed, sourceBegin, sourceEnd});
    }

    void EndParagraph()
    {
        if (paragraphOpen) { out += "\\par\n"; ++displayed; paragraphOpen = false; }
    }

    void BeginParagraph(const std::string& extra = {})
    {
        EndParagraph();
        const unsigned indent = static_cast<unsigned>(lists.size()) * 360 + quoteDepth * 300;
        const bool tight = !lists.empty() && lists.back().tight;
        out += "\\pard\\plain\\f0\\fs24\\cf1\\sa" + std::to_string(tight ? 50 : 150)
            + "\\sl300\\slmult1\\li" + std::to_string(indent) + "\\ri0 ";
        if (quoteDepth) out += "\\cf3 ";
        out += extra;
        paragraphOpen = true;
    }

    bool FinishTable()
    {
        // A table alone is buffered, so all rows share content-based widths.
        // Fragments contain the exact character formatting used in the view.
        if (nativeTable) {
            for (const auto& row : tableRows) {
                for (size_t i = 0; i < row.size(); ++i) {
                    const LONG width = measurer.Width(header + "\\pard " + row[i].rtf + "}");
                    if (width < 0) return false;
                    table.preferredWidths[i] = std::max(table.preferredWidths[i], width);
                }
            }
        }
        out = std::move(beforeTable);
        const auto widths = ColumnWidths(table, options.tableWidth);
        for (const auto& row : tableRows) {
            if (nativeTable) {
                out += "\\trowd\\trql\\trgaph" + std::to_string(CellPadding) + "\\trleft" + std::to_string(table.indent);
                LONG right = table.indent;
                for (LONG width : widths) {
                    right += width;
                    for (const char* side : {"l", "t", "r", "b"})
                        out += std::string("\\clbrdr") + side + "\\brdrs\\brdrw10\\brdrcf5";
                    out += "\\cellx" + std::to_string(right);
                }
                out += '\n';
            }
            for (size_t i = 0; i < row.size(); ++i) {
                if (nativeTable) out += "\\pard\\intbl";
                else if (!i) out += "\\pard\\li" + std::to_string(table.indent);
                out += row[i].rtf;
                out += nativeTable ? "\\cell " : (i + 1 < row.size() ? "\\tab " : "");
            }
            out += nativeTable ? "\\row\n" : "\\par\n";
        }
        if (nativeTable) {
            tables.push_back(std::move(table));
            // Separate adjacent tables and reset native table paragraph state.
            out += "\\pard\\plain\\f0\\fs24\\cf1\\sa0\\par\n";
            ++displayed;
        }
        tableRows.clear();
        return true;
    }

    int EnterBlock(MD_BLOCKTYPE type, void* detail)
    {
        switch (type) {
        case MD_BLOCK_TABLE: {
            EndParagraph();
            const unsigned columns = static_cast<MD_BLOCK_TABLE_DETAIL*>(detail)->col_count;
            nativeTable = columns <= 63;
            table = {};
            table.indent = static_cast<LONG>(lists.size()) * 360 + quoteDepth * 300;
            if (nativeTable) table.preferredWidths.assign(columns, MinimumColumn);
            beforeTable = std::move(out);
            out.clear();
            break;
        }
        case MD_BLOCK_TR:
            tableRows.emplace_back();
            if (nativeTable) { table.rows.push_back(displayed); displayed += 2; }
            break;
        case MD_BLOCK_TH: case MD_BLOCK_TD: {
            if (!nativeTable && !tableRows.back().empty()) ++displayed; // Text fallback's tab separator.
            const auto align = static_cast<MD_BLOCK_TD_DETAIL*>(detail)->align;
            out = "\\plain\\f0\\fs24\\cf" + std::string(quoteDepth ? "3" : "1") +
                "\\sa40\\sb40\\sl300\\slmult1";
            out += nativeTable && align == MD_ALIGN_CENTER ? "\\qc" : nativeTable && align == MD_ALIGN_RIGHT ? "\\qr" : "\\ql";
            if (type == MD_BLOCK_TH) out += "\\b";
            out += ' ';
            break;
        }
        case MD_BLOCK_QUOTE: EndParagraph(); ++quoteDepth; break;
        case MD_BLOCK_UL: EndParagraph(); lists.push_back({false, 0, static_cast<MD_BLOCK_UL_DETAIL*>(detail)->is_tight != 0}); break;
        case MD_BLOCK_OL: {
            EndParagraph(); auto* list = static_cast<MD_BLOCK_OL_DETAIL*>(detail);
            lists.push_back({true, list->start, list->is_tight != 0}); break;
        }
        case MD_BLOCK_LI: {
            BeginParagraph("\\fi-260 ");
            if (!lists.empty() && lists.back().ordered) Emit(std::to_wstring(lists.back().next++) + L".\t");
            else Emit(L"\x2022\t");
            firstItemParagraph.push_back(true);
            break;
        }
        case MD_BLOCK_P:
            if (!firstItemParagraph.empty() && firstItemParagraph.back() && paragraphOpen) firstItemParagraph.back() = false;
            else BeginParagraph();
            break;
        case MD_BLOCK_H: {
            static const unsigned sizes[] = {44, 36, 30, 27, 25, 24};
            unsigned level = static_cast<MD_BLOCK_H_DETAIL*>(detail)->level;
            BeginParagraph("\\b\\sb160\\sa140\\fs" + std::to_string(sizes[level - 1]) + " "); break;
        }
        case MD_BLOCK_CODE: BeginParagraph("\\f1\\fs22\\sa140\\highlight4 "); break;
        case MD_BLOCK_HTML: BeginParagraph("\\f1\\fs22\\cf3 "); Emit(L"[HTML source]\n"); break;
        // Windows Rich Edit does not draw RTF paragraph borders. Use a visible
        // text rule so separators survive both native display and copying.
        case MD_BLOCK_HR: BeginParagraph("\\cf3\\sa180 "); Emit(std::wstring(24, L'\x2500')); EndParagraph(); break;
        default: break;
        }
        return 0;
    }

    int LeaveBlock(MD_BLOCKTYPE type)
    {
        switch (type) {
        case MD_BLOCK_TABLE: return FinishTable() ? 0 : 1;
        case MD_BLOCK_TR: displayed += nativeTable ? 2 : 1; break;
        case MD_BLOCK_TH: case MD_BLOCK_TD:
            tableRows.back().push_back({std::move(out)});
            out.clear();
            // Rich Edit stores a terminator for every cell, including the last.
            if (nativeTable) ++displayed;
            break;
        case MD_BLOCK_QUOTE: EndParagraph(); --quoteDepth; break;
        case MD_BLOCK_UL: case MD_BLOCK_OL: EndParagraph(); lists.pop_back(); break;
        case MD_BLOCK_LI: EndParagraph(); firstItemParagraph.pop_back(); break;
        case MD_BLOCK_P: case MD_BLOCK_H: case MD_BLOCK_CODE: case MD_BLOCK_HTML: EndParagraph(); break;
        default: break;
        }
        return 0;
    }

    int EnterSpan(MD_SPANTYPE type)
    {
        switch (type) {
        case MD_SPAN_EM: out += "{\\i "; break;
        case MD_SPAN_STRONG: out += "{\\b "; break;
        case MD_SPAN_CODE: out += "{\\f1\\fs22\\highlight4 "; break;
        case MD_SPAN_A: out += "{\\cf2\\ul "; break;
        case MD_SPAN_IMG: out += "{\\cf3 "; Emit(L"[Image: "); break;
        default: out += '{'; break;
        }
        return 0;
    }

    int LeaveSpan(MD_SPANTYPE type, void* detail)
    {
        // Visible destinations retain meaning when copying rendered text and
        // let Rich Edit detect ordinary URLs without executable RTF fields.
        if (type == MD_SPAN_A) {
            Emit(L" <" + Attribute(static_cast<MD_SPAN_A_DETAIL*>(detail)->href) + L">");
        } else if (type == MD_SPAN_IMG) {
            Emit(L" — " + Attribute(static_cast<MD_SPAN_IMG_DETAIL*>(detail)->src) + L"]");
        }
        out += '}';
        return 0;
    }

    int Text(MD_TEXTTYPE type, const char* text, MD_SIZE size)
    {
        // MD4C also emits static strings (breaks, code indentation, etc.).
        // Only pointers into this parse's input establish a source location;
        // repeated text is never searched for or guessed.
        const auto address = reinterpret_cast<uintptr_t>(text);
        const auto base = reinterpret_cast<uintptr_t>(input.data());
        const bool sourced = address >= base && address - base < input.size() &&
            size <= input.size() - (address - base);
        if (type == MD_TEXT_SOFTBR) Emit(L" ");
        else if (type == MD_TEXT_BR) Emit(L"\n");
        else if (type == MD_TEXT_NULLCHAR) Emit(L"\xfffd");
        else if (sourced) {
            const LONG begin = SourceAt(address - base);
            const auto value = type == MD_TEXT_ENTITY ? DecodeEntity(text, size) : FromUtf8(std::string(text, size));
            // Entity spelling is ASCII; decoded entities are always shorter.
            // Unrecognized entity names remain direct, verbatim source text.
            Emit(value, begin, begin + static_cast<LONG>(type == MD_TEXT_ENTITY ? size : value.size()));
        } else if (type == MD_TEXT_ENTITY) Emit(DecodeEntity(text, size));
        else Emit(FromUtf8(std::string(text, size)));
        return 0;
    }
};
}

LONG SourceMap::SourceAt(LONG renderedOffset) const
{
    if (spans.empty()) return 0;
    const auto next = std::upper_bound(spans.begin(), spans.end(), renderedOffset,
        [](LONG at, const SourceSpan& span) { return at < span.begin; });
    if (next == spans.begin()) return next->sourceBegin;
    const auto previous = next - 1;
    if (renderedOffset < previous->end) {
        if (previous->end - previous->begin == previous->sourceEnd - previous->sourceBegin)
            return previous->sourceBegin + renderedOffset - previous->begin;
        return previous->sourceBegin; // Decoded entity: atomic source anchor.
    }
    if (next == spans.end()) return previous->sourceEnd;
    // Distance to the closest displayed character; equal distances use the
    // preceding span. Approaching a span from a gap selects its source edge.
    return renderedOffset - (previous->end - 1) <= next->begin - renderedOffset ? previous->sourceEnd : next->sourceBegin;
}

bool RenderMarkdown(const std::wstring& source, const RenderOptions& options, RenderedMarkdown& result)
{
    if (options.flavor != MarkdownFlavor::CommonMark && options.flavor != MarkdownFlavor::CommonMarkWithTables) return false;
    const auto utf8 = ToUtf8(source);
    if (!source.empty() && utf8.empty()) return false;
    Renderer renderer(utf8, options);
    MD_PARSER parser{}; // Only enable the table extension; strict CommonMark remains available.
    parser.flags = options.flavor == MarkdownFlavor::CommonMarkWithTables ? MD_FLAG_TABLES : 0;
    parser.enter_block = [](MD_BLOCKTYPE type, void* detail, void* data) { return static_cast<Renderer*>(data)->EnterBlock(type, detail); };
    parser.leave_block = [](MD_BLOCKTYPE type, void*, void* data) { return static_cast<Renderer*>(data)->LeaveBlock(type); };
    parser.enter_span = [](MD_SPANTYPE type, void*, void* data) { return static_cast<Renderer*>(data)->EnterSpan(type); };
    parser.leave_span = [](MD_SPANTYPE type, void* detail, void* data) { return static_cast<Renderer*>(data)->LeaveSpan(type, detail); };
    parser.text = [](MD_TEXTTYPE type, const char* text, MD_SIZE size, void* data) { return static_cast<Renderer*>(data)->Text(type, text, size); };
    if (md_parse(utf8.data(), static_cast<MD_SIZE>(utf8.size()), &parser, &renderer) != 0) return false;
    renderer.EndParagraph();
    renderer.out += '}';
    result.rtf = std::move(renderer.out);
    result.mapping = std::move(renderer.mapping);
    result.tables = std::move(renderer.tables);
    return true;
}

bool ResizeMarkdownTables(HWND preview, const std::vector<TableLayout>& tables, LONG availableWidth)
{
    for (const auto& table : tables) {
        const auto widths = ColumnWidths(table, availableWidth);
        for (LONG position : table.rows) {
            TABLEROWPARMS row{};
            row.cbRow = sizeof(row);
            row.cbCell = sizeof(TABLECELLPARMS);
            row.cCell = static_cast<BYTE>(widths.size());
            row.cRow = 1;
            row.cpStartRow = position;
            TABLECELLPARMS cells[63]{};
            if (SendMessageW(preview, EM_GETTABLEPARMS, reinterpret_cast<WPARAM>(&row), reinterpret_cast<LPARAM>(cells)) != S_OK)
                return false;
            // Retain native borders, shading and row attributes verbatim.
            for (size_t i = 0; i < widths.size(); ++i) cells[i].dxWidth = widths[i];
            row.cRow = 1;
            row.cpStartRow = position;
            if (SendMessageW(preview, EM_SETTABLEPARMS, reinterpret_cast<WPARAM>(&row), reinterpret_cast<LPARAM>(cells)) != S_OK)
                return false;
        }
    }
    return true;
}
