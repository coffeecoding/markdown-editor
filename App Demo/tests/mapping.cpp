// Include the renderer to inspect temporary checkpoint storage without adding
// instrumentation or a testing interface to the shipped executable.
#include "../markdown.cpp"
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
    HWND view = CreateWindowExW(0, MSFTEDIT_CLASS, L"", WS_CHILD | ES_MULTILINE | ES_READONLY,
        0, 0, 900, 700, parent, nullptr, nullptr, nullptr);

    Preview() { Check(library && parent && view, "create native preview"); }
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
        GETTEXTLENGTHEX length{GTL_PRECISE | GTL_NUMCHARS, 1200};
        std::wstring text(static_cast<size_t>(SendMessageW(view, EM_GETTEXTLENGTHEX, reinterpret_cast<WPARAM>(&length), 0)) + 1, 0);
        GETTEXTEX get{static_cast<DWORD>(text.size() * sizeof(wchar_t)), GT_DEFAULT, 1200, nullptr, nullptr};
        text.resize(static_cast<size_t>(SendMessageW(view, EM_GETTEXTEX, reinterpret_cast<WPARAM>(&get), reinterpret_cast<LPARAM>(text.data()))));
        for (auto& c : text) if (c == L'\r' || c == L'\v') c = L'\n';
        return text;
    }
};

struct Fixture {
    std::wstring source, text;
    RenderedMarkdown result;
    Fixture(Preview& preview, std::wstring input) : source(std::move(input))
    {
        Check(RenderMarkdown(source, {}, result), "render fixture");
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
    std::puts("Mapping checks passed (native Rich Edit ranges, exact offsets, entities, gaps, checkpoints, replacement).");
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
