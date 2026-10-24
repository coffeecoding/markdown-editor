#include "markdown.h"
#include "document.h"
#include "md4c.h"
extern "C" {
#include "entity.h"
}
#include <cstdint>
#include <algorithm>
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

struct Renderer {
    struct List { bool ordered; unsigned next; bool tight; };
    struct Checkpoint { MD_SIZE byte; LONG source; };
    const std::string& input;
    std::string out;
    std::vector<Checkpoint> checkpoints;
    Checkpoint resolved{0, 0};
    SourceMap mapping;
    LONG displayed = 0;
    std::vector<List> lists;
    std::vector<bool> firstItemParagraph;
    unsigned quoteDepth = 0;
    bool paragraphOpen = false;

    explicit Renderer(const std::string& utf8) : input(utf8)
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

    int EnterBlock(MD_BLOCKTYPE type, void* detail)
    {
        switch (type) {
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
    if (options.flavor != MarkdownFlavor::CommonMark) return false;
    const auto utf8 = ToUtf8(source);
    if (!source.empty() && utf8.empty()) return false;
    Renderer renderer(utf8);
    renderer.out = "{\\rtf1\\ansi\\ansicpg1252\\deff0\\uc1{\\fonttbl{\\f0\\fnil ";
    Escape(renderer.out, options.bodyFont);
    renderer.out += ";}{\\f1\\fmodern ";
    Escape(renderer.out, options.codeFont);
    renderer.out += ";}}{\\colortbl;";
    for (COLORREF color : {options.palette.text, options.palette.link, options.palette.muted, options.palette.code})
        renderer.out += "\\red" + std::to_string(GetRValue(color)) + "\\green" +
            std::to_string(GetGValue(color)) + "\\blue" + std::to_string(GetBValue(color)) + ";";
    renderer.out += "}\\viewkind4\\cf1 ";
    MD_PARSER parser{}; // flags == 0 selects MD4C's CommonMark dialect.
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
    return true;
}
