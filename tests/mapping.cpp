// Include the renderer to inspect temporary checkpoint storage without adding
// instrumentation or a testing interface to the shipped executable.
#include "../src/markdown.cpp"
#include <richedit.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {
void Check(bool condition, const char* description)
{
    if (!condition) throw std::runtime_error(description);
}

struct Preview {
    HMODULE library = LoadLibraryExW(L"Msftedit.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    HWND parent = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 900, 700, nullptr, nullptr, nullptr, nullptr);
    HWND view = CreateWindowExW(0, MSFTEDIT_CLASS, L"", WS_CHILD | WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_READONLY | WS_HSCROLL,
        0, 0, 900, 700, parent, nullptr, nullptr, nullptr);

    Preview() {
        Check(library && parent && view, "create native preview");
        SendMessageW(view, EM_SETTARGETDEVICE, 0, 0);
    }
    ~Preview() { DestroyWindow(parent); FreeLibrary(library); }

    std::wstring Stream(const std::string& rtf)
    {
        struct Input { const std::string& rtf; size_t offset = 0; } input{rtf};
        EDITSTREAM stream{};
        stream.dwCookie = reinterpret_cast<DWORD_PTR>(&input);
        stream.pfnCallback = [](DWORD_PTR cookie, LPBYTE buffer, LONG requested, LONG* supplied) -> DWORD {
            auto& data = *reinterpret_cast<Input*>(cookie);
            const size_t count = std::min<size_t>(requested, data.rtf.size() - data.offset);
            std::memcpy(buffer, data.rtf.data() + data.offset, count);
            data.offset += count;
            *supplied = static_cast<LONG>(count);
            return 0;
        };
        SendMessageW(view, EM_STREAMIN, SF_RTF, reinterpret_cast<LPARAM>(&stream));
        Check(stream.dwError == 0, "native RTF stream succeeds");
        return Raw();
    }

    std::wstring Raw()
    {
        GETTEXTLENGTHEX length{GTL_PRECISE | GTL_NUMCHARS, 1200};
        std::wstring text(static_cast<size_t>(SendMessageW(view, EM_GETTEXTLENGTHEX, reinterpret_cast<WPARAM>(&length), 0)) + 1, 0);
        GETTEXTEX get{static_cast<DWORD>(text.size() * sizeof(wchar_t)), GT_RAWTEXT, 1200, nullptr, nullptr};
        text.resize(static_cast<size_t>(SendMessageW(view, EM_GETTEXTEX, reinterpret_cast<WPARAM>(&get), reinterpret_cast<LPARAM>(text.data()))));
        for (auto& c : text) if (c == L'\r' || c == L'\v') c = L'\n';
        return text;
    }

    POINTL Point(LONG position)
    {
        POINTL point{};
        SendMessageW(view, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&point), position);
        return point;
    }

    CHARFORMAT2W Character(LONG position)
    {
        CHARRANGE selection{position, position + 1};
        SendMessageW(view, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&selection));
        CHARFORMAT2W format{};
        format.cbSize = sizeof(format);
        SendMessageW(view, EM_GETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
        return format;
    }

    WORD Alignment(LONG position)
    {
        CHARRANGE selection{position, position};
        SendMessageW(view, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&selection));
        PARAFORMAT2 format{};
        format.cbSize = sizeof(format);
        SendMessageW(view, EM_GETPARAFORMAT, 0, reinterpret_cast<LPARAM>(&format));
        return format.wAlignment;
    }
};

struct NativeRow {
    TABLEROWPARMS row{};
    TABLECELLPARMS cells[63]{};
    NativeRow(HWND view, LONG position, size_t columns)
    {
        row.cbRow = sizeof(row);
        row.cbCell = sizeof(cells[0]);
        row.cCell = static_cast<BYTE>(columns);
        row.cRow = 1;
        row.cpStartRow = position;
        const auto status = SendMessageW(view, EM_GETTABLEPARMS, reinterpret_cast<WPARAM>(&row), reinterpret_cast<LPARAM>(cells));
        if (status != S_OK) std::fprintf(stderr, "get row position=%ld columns=%zu status=%08llx cbRow=%u cbCell=%u\n", position, columns,
            static_cast<unsigned long long>(status), row.cbRow, row.cbCell);
        Check(status == S_OK, "read native row parameters");
        Check(row.cCell == columns, "native row column count");
    }
};

struct Fixture {
    std::wstring source, text;
    RenderedMarkdown result;
    Fixture(Preview& preview, std::wstring input, const RenderOptions& options = {}) : source(std::move(input))
    {
        Check(RenderMarkdown(source, options, result), "render fixture");
        text = preview.Stream(result.rtf);
        LONG end = 0;
        for (const auto& span : result.mapping.spans) {
            Check(span.begin >= end && span.end > span.begin && span.end <= static_cast<LONG>(text.size()), "ordered native display ranges");
            Check(span.sourceBegin >= 0 && span.sourceEnd > span.sourceBegin && span.sourceEnd <= static_cast<LONG>(source.size()), "source range bounds");
            const bool direct = span.end - span.begin == span.sourceEnd - span.sourceBegin;
            if (direct) Check(text.substr(span.begin, span.end - span.begin) == source.substr(span.sourceBegin, span.sourceEnd - span.sourceBegin), "native displayed text matches direct source range");
            for (LONG i = span.begin; i < span.end; ++i)
                Check(result.mapping.SourceAt(i) == span.sourceBegin + (direct ? i - span.begin : 0), "every native character maps to its source offset");
            end = span.end;
        }
    }

    void At(const std::wstring& displayed, const std::wstring& raw, LONG delta = 0, LONG sourceDelta = 0)
    {
        const auto position = text.find(displayed), expected = source.find(raw);
        if (position == std::wstring::npos || expected == std::wstring::npos) {
            std::fprintf(stderr, "missing display '%s' or source '%s'; native text: '%s'\n",
                ToUtf8(displayed).c_str(), ToUtf8(raw).c_str(), ToUtf8(text).c_str());
            throw std::runtime_error("fixture sample exists");
        }
        if (result.mapping.SourceAt(static_cast<LONG>(position) + delta) != static_cast<LONG>(expected) + sourceDelta) {
            std::fprintf(stderr, "sample '%s': expected source %zu + %ld, got %ld at display %zu + %ld\n", ToUtf8(displayed).c_str(), expected, sourceDelta,
                result.mapping.SourceAt(static_cast<LONG>(position) + delta), position, delta);
            throw std::runtime_error("exact source offset");
        }
    }
};

void VerifyTables(Preview& preview)
{
    Fixture table(preview, L"| Left | Center | Right |\n| :--- | :---: | ---: |\n"
        L"| **bold** *italic* `code` | [label](url) | 😀 &amp;&NotEqualTilde;&#x1F600; |\n"
        L"| repeated | repeated | escaped\\|pipe |\n| | missing |\n|\n\nAfter table **target**.");
    table.At(L"bold", L"bold", 2, 2);
    table.At(L"target", L"target", 2, 2);
    table.At(L"escaped|pipe", L"escaped", 7, 8);
    table.At(L"label", L"label", 2, 2);
    table.At(L"≂̸", L"&NotEqualTilde;", 1);
    table.At(L"😀", L"😀", 1, 1);
    const auto lastRepeated = table.text.rfind(L"repeated");
    Check(table.result.mapping.SourceAt(static_cast<LONG>(lastRepeated) + 3) == static_cast<LONG>(table.source.rfind(L"repeated")) + 3,
        "repeated table cells map from MD4C locations");
    Check(table.result.tables.size() == 1, "native table metadata");
    const auto& layout = table.result.tables[0];
    Check(layout.rows.size() == 5 && layout.preferredWidths.size() == 3, "empty and missing cells retain rectangular rows");
    Check(preview.Alignment(static_cast<LONG>(table.text.find(L"Left"))) == PFA_LEFT &&
          preview.Alignment(static_cast<LONG>(table.text.find(L"Center"))) == PFA_CENTER &&
          preview.Alignment(static_cast<LONG>(table.text.find(L"Right"))) == PFA_RIGHT, "native Markdown column alignment");
    Check((preview.Character(static_cast<LONG>(table.text.find(L"Left"))).dwEffects & CFE_BOLD) &&
          (preview.Character(static_cast<LONG>(table.text.find(L"bold"))).dwEffects & CFE_BOLD) &&
          (preview.Character(static_cast<LONG>(table.text.find(L"italic"))).dwEffects & CFE_ITALIC), "bold headers and inline formatting");
    Check(std::wstring(preview.Character(static_cast<LONG>(table.text.find(L"code"))).szFaceName) == L"Consolas" &&
          preview.Character(static_cast<LONG>(table.text.find(L"label"))).crTextColor == Palette(false).link, "code font and link formatting in cells");
    for (LONG position : layout.rows) {
        Check(table.text[position] == 0xfff9 && table.text[position + 1] == L'\n', "native row start markers occupy two generated units");
        const auto end = table.text.find(static_cast<wchar_t>(0xfffb), position);
        Check(end != std::wstring::npos && table.text[end + 1] == L'\n' &&
              std::count(table.text.begin() + position, table.text.begin() + end, L'\x7') == 3, "native cell terminators and two row end markers");
        NativeRow native(preview.view, position, 3);
        for (size_t i = 0; i < 3; ++i) {
            const auto& cell = native.cells[i];
            Check(cell.dxBrdrLeft == 10 && cell.dyBrdrTop == 10 && cell.dxBrdrRight == 10 && cell.dyBrdrBottom == 10,
                "uniform native half-point grid");
            Check(cell.crBrdrLeft == Palette(false).tableBorder && cell.crBrdrTop == Palette(false).tableBorder &&
                  cell.crBrdrRight == Palette(false).tableBorder && cell.crBrdrBottom == Palette(false).tableBorder,
                "fixed light grid color on all four sides");
        }
    }

    // These fixtures validate every directly mapped native UTF-16 unit, as well
    // as source bounds and entity anchors, including table/prose transitions.
    Fixture adjacent(preview, L"a | b\n---|---\none | two\n\nx | y\n---|---\nthree | four\n\nlast");
    Check(adjacent.result.tables.size() == 2, "adjacent tables retain independent metadata");
    adjacent.At(L"three", L"three", 3, 3);
    adjacent.At(L"last", L"last", 2, 2);
    Fixture nested(preview, L"> - lead\n>\n>   | Quoted | Table |\n>   | --- | --- |\n>   | Ωbody | nested |\n>\n>   after\n\noutside");
    Check(nested.result.tables.size() == 1 && nested.result.tables[0].indent == 660, "list and quote indentation retained");
    NativeRow nestedRow(preview.view, nested.result.tables[0].rows[0], 2);
    Check(nestedRow.row.dxIndent == 660, "native row indentation");
    nested.At(L"Ωbody", L"Ωbody", 3, 3);
    nested.At(L"outside", L"outside", 4, 4);
    Fixture headerOnly(preview, L"header | only\n---|---\n\nfollowing");
    Check(headerOnly.result.tables[0].rows.size() == 1, "header-only table");
    Fixture escaped(preview, L"a | b\n---|---\n`a\\|b` | &vert; and \\|\n\nafter");
    escaped.At(L"a\\|b", L"a\\|b", 2, 2); // MD4C retains code-span backslashes.
    escaped.At(L"| and", L"&vert;");

    RenderOptions strict;
    strict.flavor = MarkdownFlavor::CommonMark;
    Fixture commonmark(preview, table.source, strict);
    Check(commonmark.result.tables.empty() && commonmark.text.find(L"| :--- |") != std::wstring::npos,
        "strict CommonMark remains available");
    Fixture extensions(preview, L"~~plain~~ [ ] task www.example.com");
    Check(extensions.text.find(L"~~plain~~ [ ] task www.example.com") != std::wstring::npos, "only the table extension is enabled");

    for (int columns : {63, 64, 70}) {
        std::wstring header, separator, body;
        for (int i = 0; i < columns; ++i) {
            header += L"| H" + std::to_wstring(i);
            separator += L"| ---";
            body += L"| value" + std::to_wstring(i) + L"😀";
        }
        Fixture wide(preview, header + L"|\n" + separator + L"|\n" + body + L"|\n\nAfter wide");
        Check(wide.result.tables.size() == (columns == 63 ? 1u : 0u), "63-column native limit uses text fallback above limit");
        for (int i = 0; i < columns; ++i) wide.At(L"value" + std::to_wstring(i) + L"😀", L"value" + std::to_wstring(i) + L"😀", 2, 2);
        wide.At(L"After wide", L"After wide", 3, 3);
    }

    for (bool dark : {false, true}) {
        RenderOptions custom;
        custom.palette = Palette(dark);
        custom.palette.text = RGB(12, 34, 56);
        custom.palette.background = RGB(91, 72, 43);
        Fixture defaults(preview, L"a|b\n---|---\nx|y", custom);
        NativeRow defaultRow(preview.view, defaults.result.tables[0].rows[0], 2);
        Check(defaultRow.cells[0].crBrdrLeft == Palette(dark).tableBorder, "text and background do not recompute table borders");
        custom.palette.tableBorder = RGB(10, 20, 30);
        Fixture colors(preview, L"a|b\n---|---\nx|y", custom);
        NativeRow colorRow(preview.view, colors.result.tables[0].rows[0], 2);
        Check(colorRow.cells[1].crBrdrBottom == custom.palette.tableBorder, "custom border RGB independent of palette");
    }

    RenderOptions natural;
    natural.tableWidth = 20000;
    const std::wstring prose = L"Surrounding prose should wrap to the viewport even when a wide table needs horizontal scrolling. ";
    Fixture sizes(preview, L"| i | A much wider bold heading | description |\n|---|---|---|\n| x | words | "
        L"A long description with many ordinary words that will wrap as the table is narrowed. |\n\n" + prose + prose, natural);
    const auto& sized = sizes.result.tables[0];
    const auto preferred = sized.preferredWidths;
    Check(preferred[0] < preferred[1] && preferred[1] < preferred[2], "column widths reflect content, including bold headers");
    NativeRow naturalRow(preview.view, sized.rows[0], 3);
    LONG total = 0;
    for (size_t i = 0; i < 3; ++i) {
        Check(naturalRow.cells[i].dxWidth == preferred[i], "compact tables use natural widths without stretching");
        total += preferred[i];
    }
    Check(total < natural.tableWidth, "compact table leaves unused space");
    Check(naturalRow.row.dxCellMargin == CellPadding, "native modest cell padding");
    const auto begin = static_cast<LONG>(sizes.text.find(L"A long description"));
    const auto tail = static_cast<LONG>(sizes.text.find(L"narrowed."));
    Check(preview.Point(tail).y == preview.Point(begin).y, "content measured without wrapping");
    const CHARRANGE chosen{begin + 2, begin + 8};
    SendMessageW(preview.view, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&chosen));
    Check(ResizeMarkdownTables(preview.view, sizes.result.tables, 2700), "native resize succeeds without reparse");
    CHARRANGE retained{};
    SendMessageW(preview.view, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&retained));
    Check(retained.cpMin == chosen.cpMin && retained.cpMax == chosen.cpMax && preview.Raw() == sizes.text &&
          sized.preferredWidths == preferred, "native resizing preserves selection, text, offsets and preferred widths");
    Check(preview.Point(tail).y > preview.Point(begin).y, "long cell wraps after shrinking");
    NativeRow narrowRow(preview.view, sized.rows[1], 3);
    Check(narrowRow.cells[0].dxWidth == preferred[0] && narrowRow.cells[2].dxWidth < preferred[2] &&
          narrowRow.cells[0].dxWidth + narrowRow.cells[1].dxWidth + narrowRow.cells[2].dxWidth == 2700,
        "progressive shrinking retains short columns and fits available width");
    for (int zoom : {96, 144, 192}) {
        SendMessageW(preview.view, EM_SETZOOM, zoom, 96);
        Check(ResizeMarkdownTables(preview.view, sizes.result.tables, 500), "native resize under DPI zoom");
        NativeRow minimum(preview.view, sized.rows[0], 3);
        for (int i = 0; i < 3; ++i) Check(minimum.cells[i].dxWidth == MinimumColumn && minimum.cells[i].crBrdrLeft == Palette(false).tableBorder,
            "minimum column width and borders survive DPI relayout");
    }
    SendMessageW(preview.view, EM_SETZOOM, 0, 0);
    Check(ResizeMarkdownTables(preview.view, sizes.result.tables, natural.tableWidth), "restore natural widths after resize");
    NativeRow restored(preview.view, sized.rows[0], 3);
    for (size_t i = 0; i < 3; ++i) Check(restored.cells[i].dxWidth == preferred[i], "growing restores preferred widths");
    MoveWindow(preview.view, 0, 0, 70, 700, FALSE);
    RECT rect{0, 0, 60, 700};
    SendMessageW(preview.view, EM_SETRECTNP, 0, reinterpret_cast<LPARAM>(&rect));
    SendMessageW(preview.view, EM_SETTARGETDEVICE, 0, 0);
    Check(ResizeMarkdownTables(preview.view, sizes.result.tables, 900), "table minimums overflow a narrow viewport");
    const LONG proseStart = static_cast<LONG>(sizes.text.find(L"Surrounding"));
    CHARRANGE proseSelection{proseStart, proseStart};
    SendMessageW(preview.view, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&proseSelection));
    SendMessageW(preview.view, EM_SCROLLCARET, 0, 0);
    Check(preview.Point(proseStart + 40).y > preview.Point(proseStart).y, "surrounding prose stays wrapped with horizontal overflow");
    SCROLLINFO scroll{sizeof(scroll), SIF_RANGE | SIF_PAGE};
    GetScrollInfo(preview.view, SB_HORZ, &scroll);
    Check(scroll.nMax > static_cast<int>(scroll.nPage), "overflow table enables horizontal scrolling");
    MoveWindow(preview.view, 0, 0, 900, 700, FALSE);
    rect = {0, 0, 900, 700};
    SendMessageW(preview.view, EM_SETRECTNP, 0, reinterpret_cast<LPARAM>(&rect));

    CellMeasurer measurement;
    const auto prefix = RtfHeader({});
    const LONG normalWidth = measurement.Width(prefix + "\\pard\\plain\\f0\\fs24 MMMMMMMM}");
    const LONG boldWidth = measurement.Width(prefix + "\\pard\\plain\\f0\\fs24\\b MMMMMMMM}");
    Check(boldWidth > normalWidth && normalWidth > 1000, "native measurement includes bold shaping");
    const LONG rtlWidth = measurement.Width(prefix + "\\pard\\plain\\f0\\fs24 " + [] {
        std::string text; Escape(text, L"שלום עולם שלום עולם"); return text;
    }() + "}");
    Check(rtlWidth > 1500, "native measurement includes right-to-left text extent");
    const auto beforeWindows = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
    { const auto input = ToUtf8(L"plain"); Renderer scratch(input); Check(!scratch.measurer.control && !scratch.measurer.library, "plain documents create no measurement control"); }
    Check(GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS) == beforeWindows, "measurement HWND is lazy for plain documents");
}

void Verify()
{
    const SourceMap gaps{{{2, 5, 10, 13}, {8, 10, 20, 22}}};
    for (const auto& sample : {std::pair<LONG, LONG>{LONG_MIN, 10}, {2, 10}, {4, 12},
        {5, 13}, {6, 13}, {7, 20}, {8, 20}, {10, 22}, {LONG_MAX, 22}})
        Check(gaps.SourceAt(sample.first) == sample.second, "nearest character distance and preceding tie");
    Check(SourceMap{}.SourceAt(100) == 0, "empty map uses zero");
    const SourceMap atomic{{{0, 2, 0, 15}, {5, 6, 20, 21}}};
    Check(atomic.SourceAt(1) == 0 && atomic.SourceAt(2) == 15 && atomic.SourceAt(3) == 15 && atomic.SourceAt(4) == 20,
        "entity anchor inside, real source end afterward");

    Preview preview;
    Fixture formatted(preview, L"# Heading 😀\n\nBefore **bold** and *italic*; \\*escape\\*. [label](url) then **again**.");
    formatted.At(L"bold", L"bold", 2, 2);
    formatted.At(L"italic", L"italic", 4, 4);
    formatted.At(L"*escape*", L"*escape", 0, 0);
    formatted.At(L"label", L"label", 3, 3);
    formatted.At(L" <url>", L"label", 0, 5);
    formatted.At(L" <url>", L" then", 5, 0);
    formatted.At(L"again", L"again", 3, 3);
    formatted.At(L"Heading", L"Heading");
    Fixture unicode(preview, L"日本語 😀 **Ωbold😀** &amp;&NotEqualTilde;&#x1F600; &unknownEntity; end");
    unicode.At(L"Ωbold😀", L"Ωbold😀", 4, 4);
    unicode.At(L"&≂̸😀", L"&amp;");
    unicode.At(L"≂̸", L"&NotEqualTilde;", 1);
    unicode.At(L"😀 &unknown", L"&#x1F600;", 1);
    unicode.At(L"&unknownEntity;", L"&unknownEntity;", 6, 6);
    Fixture code(preview, L"`multi\nline code`\n\n~~~\n\tcode😀\nnext\n~~~\n\n    indented\n");
    code.At(L"multi line code", L"multi", 5, 5);
    code.At(L"line code", L"line code", 4, 4);
    // MD4C expands code indentation tabs into generated spaces.
    code.At(L"    code😀", L"line code", 0, 9);
    code.At(L"    code😀", L"code😀", 3);
    code.At(L"code😀", L"code😀", 3, 3);
    code.At(L"next", L"next", 2, 2);
    code.At(L"indented", L"indented", 4, 4);
    Fixture breaks(preview, L"soft\nnext  \nhard\\\nbreak\n\n- item\n\n![alt](image.png)\n\n<div>html</div>\n");
    breaks.At(L"soft next", L"soft", 4, 4);
    breaks.At(L"next\nhard", L"next", 4, 4);
    breaks.At(L"hard\nbreak", L"hard", 4, 4);
    breaks.At(L"•\titem", L"break", 0, 5);
    breaks.At(L"•\titem", L"item", 1);
    breaks.At(L"[Image: ", L"item", 0, 4);
    breaks.At(L"[Image: ", L"alt", 7);
    breaks.At(L"image.png", L"alt", 0, 3);
    breaks.At(L"[HTML source]", L"<div>");
    Fixture tab(preview, L"before\ttext");
    tab.At(L"before\ttext", L"before\ttext", 6, 6);
    Fixture finalEntity(preview, L"&amp;&NotEqualTilde;&#x1F600;");
    finalEntity.At(L"😀", L"&#x1F600;", 1);
    Check(finalEntity.result.mapping.spans.size() == 3 && finalEntity.result.mapping.SourceAt(100) == static_cast<LONG>(finalEntity.source.size()),
        "adjacent entities retain separate anchors and final source extent");
    Fixture repeated(preview, L"same **same** same\n\nsame **same** same");
    const auto lastSame = repeated.text.rfind(L"same");
    Check(repeated.result.mapping.SourceAt(static_cast<LONG>(lastSame) + 2) == static_cast<LONG>(repeated.source.rfind(L"same")) + 2, "repeated words use callback pointers");
    Fixture plain(preview, L"ordinary plain text");
    Check(plain.result.mapping.spans.size() == 1, "contiguous direct callbacks stay compact");
    Check(plain.result.mapping.SourceAt(10000) == static_cast<LONG>(plain.source.size()), "document end");
    for (const auto& source : {L"", L"\n\n", L"---", L"![](image.png)"}) {
        Fixture empty(preview, source);
        Check(empty.result.mapping.spans.empty() && empty.result.mapping.SourceAt(0) == 0, "empty and generated-only documents");
    }

    // Exercise each codepoint boundary against an independent UTF-16 counter,
    // including 2/3/4-byte sequences crossing sparse-checkpoint boundaries.
    std::wstring longLine;
    for (int i = 0; i < 2000; ++i) longLine += L"aΩ日本😀**bold**\\* ";
    const auto utf8 = ToUtf8(longLine);
    Renderer scratch(utf8);
    LONG sourceOffset = 0;
    for (size_t i = 0; i < utf8.size(); ++i) {
        const auto c = static_cast<unsigned char>(utf8[i]);
        if ((c & 0xc0) != 0x80) {
            Check(scratch.SourceAt(i) == sourceOffset, "UTF-8 checkpoint resolution at every codepoint boundary");
            sourceOffset += c >= 0xf0 ? 2 : 1;
        }
    }
    Check(scratch.SourceAt(utf8.size()) == sourceOffset, "UTF-8 endpoint");
    for (size_t i = utf8.size(); i > 0; --i) {
        const auto c = static_cast<unsigned char>(utf8[i - 1]);
        if ((c & 0xc0) != 0x80) {
            sourceOffset -= c >= 0xf0 ? 2 : 1;
            Check(scratch.SourceAt(i - 1) == sourceOffset, "checkpoint lookup also handles backward offsets");
        }
    }
    for (size_t i = 1; i < scratch.checkpoints.size(); ++i) {
        const auto byte = scratch.checkpoints[i].byte;
        Check(byte - scratch.checkpoints[i - 1].byte >= 256 && byte - scratch.checkpoints[i - 1].byte <= 259 &&
            (static_cast<unsigned char>(utf8[byte]) & 0xc0) != 0x80, "sparse checkpoints stay aligned and bounded");
    }
    Fixture dense(preview, longLine);
    Check(dense.result.mapping.SourceAt(static_cast<LONG>(dense.text.rfind(L"bold")) + 3) == static_cast<LONG>(longLine.rfind(L"bold")) + 3,
        "dense formatting near end of long Unicode line");

    RenderedMarkdown retained;
    Check(RenderMarkdown(L"**old**", {}, retained), "initial mapping");
    const auto oldRtf = retained.rtf;
    Check(!RenderMarkdown(std::wstring(1, 0xd800), {}, retained) && retained.rtf == oldRtf && retained.mapping.SourceAt(0) == 2,
        "failed rendering preserves previous result");
    Check(RenderMarkdown(L"*new*", {}, retained) && retained.mapping.SourceAt(0) == 1, "successful rendering replaces mapping");
    VerifyTables(preview);
    std::puts("Mapping and table checks passed (native raw ranges, offsets, formatting, grids, widths, resize, DPI, fallback).");
}

void Benchmark()
{
    for (bool dense : {false, true}) {
        std::wstring source;
        const std::wstring unit = dense ? L"**bold** *em* `code` [label](url) &amp; \\* 😀 " :
            L"## Heading\n\nParagraph **bold** with [link](https://example.com) and `code`.\n\n";
        for (int i = 0; i < (dense ? 10000 : 5000); ++i) source += unit;
        const auto utf8 = ToUtf8(source);
        Renderer scratch(utf8);
        RenderedMarkdown result;
        std::vector<double> timings;
        for (int i = 0; i < 12; ++i) {
            const auto start = std::chrono::steady_clock::now();
            Check(RenderMarkdown(source, {}, result), "benchmark render");
            const auto stop = std::chrono::steady_clock::now();
            if (i >= 3) timings.push_back(std::chrono::duration<double, std::milli>(stop - start).count());
        }
        std::sort(timings.begin(), timings.end());
        const auto& spans = result.mapping.spans;
        std::printf("%s: source_utf16=%zu utf8_bytes=%zu span_bytes=%zu spans=%zu live_map_bytes=%zu allocated_map_bytes=%zu temporary_live_bytes=%zu temporary_allocated_bytes=%zu render_median_ms=%.3f\n",
            dense ? "dense" : "large", source.size(), utf8.size(), sizeof(spans[0]), spans.size(),
            spans.size() * sizeof(spans[0]), spans.capacity() * sizeof(spans[0]),
            scratch.checkpoints.size() * sizeof(scratch.checkpoints[0]), scratch.checkpoints.capacity() * sizeof(scratch.checkpoints[0]), timings[4]);
    }

    // Keep actual buffered cell RTF from one parse, so the isolated measurement
    // benchmark uses exactly the fragments and fonts sent to the preview.
    std::wstring unit = L"| Name | State | Description | Amount |\n| :--- | :---: | --- | ---: |\n";
    for (int row = 0; row < 8; ++row) unit += L"| **bold** Ω | ok 😀 | `code` with words &amp; more | 42 |\n";
    unit += L"\n";
    const auto sampleInput = ToUtf8(unit);
    Renderer buffered(sampleInput);
    MD_PARSER parser{};
    parser.flags = MD_FLAG_TABLES;
    parser.enter_block = [](MD_BLOCKTYPE type, void* detail, void* data) { return static_cast<Renderer*>(data)->EnterBlock(type, detail); };
    parser.leave_block = [](MD_BLOCKTYPE type, void*, void* data) { return type == MD_BLOCK_TABLE ? 0 : static_cast<Renderer*>(data)->LeaveBlock(type); };
    parser.enter_span = [](MD_SPANTYPE type, void*, void* data) { return static_cast<Renderer*>(data)->EnterSpan(type); };
    parser.leave_span = [](MD_SPANTYPE type, void* detail, void* data) { return static_cast<Renderer*>(data)->LeaveSpan(type, detail); };
    parser.text = [](MD_TEXTTYPE type, const char* text, MD_SIZE size, void* data) { return static_cast<Renderer*>(data)->Text(type, text, size); };
    Check(md_parse(sampleInput.data(), static_cast<MD_SIZE>(sampleInput.size()), &parser, &buffered) == 0, "prepare measurement benchmark");
    std::vector<std::string> cells;
    for (const auto& row : buffered.tableRows)
        for (const auto& cell : row) cells.push_back(buffered.header + "\\pard " + cell.rtf + "}");
    constexpr int TableCount = 80;
    std::wstring source;
    for (int i = 0; i < TableCount; ++i) source += unit;
    RenderedMarkdown result;
    std::vector<double> renderTimes, measureTimes, layoutTimes;
    const auto milliseconds = [](auto start) {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    };
    for (int trial = 0; trial < 7; ++trial) {
        auto start = std::chrono::steady_clock::now();
        Check(RenderMarkdown(source, {}, result), "table-heavy benchmark render");
        const double renderTime = milliseconds(start);
        CellMeasurer measurement;
        start = std::chrono::steady_clock::now();
        for (int table = 0; table < TableCount; ++table)
            for (const auto& cell : cells) Check(measurement.Width(cell) >= MinimumColumn, "table-heavy native measurement");
        const double measureTime = milliseconds(start);
        if (trial >= 2) { renderTimes.push_back(renderTime); measureTimes.push_back(measureTime); }
    }
    Preview preview;
    preview.Stream(result.rtf);
    for (int trial = 0; trial < 7; ++trial) {
        const auto start = std::chrono::steady_clock::now();
        Check(ResizeMarkdownTables(preview.view, result.tables, trial % 2 ? 3600 : 9360), "table-heavy resize benchmark");
        if (trial >= 2) layoutTimes.push_back(milliseconds(start));
    }
    const auto median = [](std::vector<double>& values) { std::sort(values.begin(), values.end()); return values[values.size() / 2]; };
    size_t live = result.tables.size() * sizeof(TableLayout), allocated = result.tables.capacity() * sizeof(TableLayout), rows = 0;
    for (const auto& table : result.tables) {
        rows += table.rows.size();
        live += (table.rows.size() + table.preferredWidths.size()) * sizeof(LONG);
        allocated += (table.rows.capacity() + table.preferredWidths.capacity()) * sizeof(LONG);
    }
    std::printf("tables: source_utf16=%zu tables=%zu rows=%zu cells=%zu live_map_bytes=%zu live_table_bytes=%zu allocated_table_bytes=%zu render_median_ms=%.3f native_measure_median_ms=%.3f relayout_median_ms=%.3f\n",
        source.size(), result.tables.size(), rows, cells.size() * TableCount, result.mapping.spans.size() * sizeof(SourceSpan), live, allocated,
        median(renderTimes), median(measureTimes), median(layoutTimes));
}
}

int main(int argc, char** argv)
{
    try {
        if (argc > 1 && std::strcmp(argv[1], "--benchmark") == 0) Benchmark();
        else Verify();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
