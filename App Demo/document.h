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
bool ReadDocument(const std::wstring& path, std::wstring& text, FileFormat& format, std::wstring& error);
bool WriteDocument(const std::wstring& path, const std::wstring& text, const FileFormat& format,
                   std::wstring& error);
std::wstring SystemError(unsigned long code);
