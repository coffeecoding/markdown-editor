#pragma once
#include <string>

enum class Encoding { Utf8, Utf8Bom, Utf16Le, Utf16Be };
struct FileFormat {
    Encoding encoding = Encoding::Utf8;
    std::wstring newline = L"\r\n";
};

// Text is canonical LF internally; file format is kept separately.
std::wstring NormalizeNewlines(const std::wstring& text);
std::wstring FromUtf8(const std::string& text);
std::string ToUtf8(const std::wstring& text);
// Settings retain individual line endings; ioError distinguishes locks and
// other I/O failures from decoding errors. Document callers use canonical LF.
bool ReadDocument(const std::wstring& path, std::wstring& text, FileFormat& format, std::wstring& error,
                  bool keepNewlines = false, unsigned long* ioError = nullptr);
bool WriteDocument(const std::wstring& path, const std::wstring& text, const FileFormat& format,
                   std::wstring& error);
std::wstring SystemError(unsigned long code);
