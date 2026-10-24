#include <windows.h>
#include "document.h"
#include <algorithm>
#include <climits>
#include <vector>

namespace {
constexpr DWORD MaxFileBytes = 32 * 1024 * 1024;
struct Handle {
    HANDLE value;
    ~Handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};

bool ValidUtf16(const std::wstring& text)
{
    for (size_t i = 0; i < text.size(); ++i) {
        const auto c = text[i];
        if (c >= 0xd800 && c <= 0xdbff) {
            if (++i == text.size() || text[i] < 0xdc00 || text[i] > 0xdfff) return false;
        } else if (c >= 0xdc00 && c <= 0xdfff) return false;
    }
    return true;
}
}

std::wstring SystemError(unsigned long code)
{
    wchar_t* message = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, reinterpret_cast<wchar_t*>(&message), 0, nullptr);
    std::wstring result = message ? message : L"Windows error " + std::to_wstring(code);
    LocalFree(message);
    return result;
}

std::wstring NormalizeNewlines(const std::wstring& text)
{
    std::wstring result;
    result.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\r') {
            if (i + 1 < text.size() && text[i + 1] == L'\n') ++i;
            result += L'\n';
        } else result += text[i];
    }
    return result;
}

std::wstring FromUtf8(const std::string& text)
{
    if (text.empty()) return {};
    if (text.size() > INT_MAX) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(size, 0);
    if (size) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

std::string ToUtf8(const std::wstring& text)
{
    if (text.empty() || text.size() > INT_MAX) return {};
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    if (size) WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

bool ReadDocument(const std::wstring& path, std::wstring& text, FileFormat& format, std::wstring& error)
{
    Handle file{CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if (file.value == INVALID_HANDLE_VALUE) { error = SystemError(GetLastError()); return false; }
    LARGE_INTEGER length{};
    if (!GetFileSizeEx(file.value, &length)) { error = SystemError(GetLastError()); return false; }
    if (length.QuadPart > MaxFileBytes) { error = L"This first version opens files up to 32 MiB."; return false; }
    std::string bytes(static_cast<size_t>(length.QuadPart), 0);
    DWORD read = 0;
    if (!ReadFile(file.value, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) || read != bytes.size()) {
        error = L"Could not read the complete file. " + SystemError(GetLastError()); return false;
    }
    format = {};
    if (bytes.size() >= 2 && ((static_cast<unsigned char>(bytes[0]) == 0xff && static_cast<unsigned char>(bytes[1]) == 0xfe) ||
                             (static_cast<unsigned char>(bytes[0]) == 0xfe && static_cast<unsigned char>(bytes[1]) == 0xff))) {
        const bool little = static_cast<unsigned char>(bytes[0]) == 0xff;
        format.encoding = little ? Encoding::Utf16Le : Encoding::Utf16Be;
        if (bytes.size() % 2) { error = L"The UTF-16 file has an incomplete character."; return false; }
        text.clear();
        for (size_t i = 2; i < bytes.size(); i += 2) {
            const auto a = static_cast<unsigned char>(bytes[i]);
            const auto b = static_cast<unsigned char>(bytes[i + 1]);
            text += static_cast<wchar_t>(little ? a | (b << 8) : (a << 8) | b);
        }
        if (!ValidUtf16(text)) { error = L"The file contains invalid UTF-16 characters."; return false; }
    } else {
        if (bytes.size() >= 3 && bytes.compare(0, 3, "\xef\xbb\xbf") == 0) {
            format.encoding = Encoding::Utf8Bom;
            bytes.erase(0, 3);
        }
        text = FromUtf8(bytes);
        if (!bytes.empty() && text.empty()) {
            error = L"The file is not valid UTF-8. Convert it to UTF-8 or BOM-marked UTF-16 before opening."; return false;
        }
    }
    if (text.find(L'\0') != std::wstring::npos) { error = L"The file contains NUL characters and cannot be edited safely as text."; return false; }
    size_t crlf = 0, lf = 0, cr = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\r') {
            if (i + 1 < text.size() && text[i + 1] == L'\n') { ++crlf; ++i; } else ++cr;
        } else if (text[i] == L'\n') ++lf;
    }
    if (lf > crlf && lf >= cr) format.newline = L"\n";
    else if (cr > crlf && cr > lf) format.newline = L"\r";
    text = NormalizeNewlines(text);
    return true;
}

bool WriteDocument(const std::wstring& path, const std::wstring& text, const FileFormat& format, std::wstring& error)
{
    if (!ValidUtf16(text)) { error = L"The source contains an incomplete Unicode character. Saving was cancelled."; return false; }
    std::wstring expanded;
    expanded.reserve(text.size() + text.size() / 20);
    for (wchar_t c : text) { if (c == L'\n') expanded += format.newline; else expanded += c; }
    std::string bytes;
    if (format.encoding == Encoding::Utf16Le || format.encoding == Encoding::Utf16Be) {
        const bool little = format.encoding == Encoding::Utf16Le;
        bytes = little ? "\xff\xfe" : "\xfe\xff";
        for (wchar_t c : expanded) {
            bytes += static_cast<char>(little ? c & 0xff : c >> 8);
            bytes += static_cast<char>(little ? c >> 8 : c & 0xff);
        }
    } else {
        if (format.encoding == Encoding::Utf8Bom) bytes = "\xef\xbb\xbf";
        bytes += ToUtf8(expanded);
    }
    // Write beside the destination, flush, then atomically replace it. A failed
    // write never truncates the user's existing file or clears the editor.
    std::wstring temporary;
    HANDLE raw = INVALID_HANDLE_VALUE;
    for (unsigned attempt = 0; attempt < 100; ++attempt) {
        temporary = path + L".native-md-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(attempt) + L".tmp";
        raw = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (raw != INVALID_HANDLE_VALUE) break;
        if (GetLastError() != ERROR_FILE_EXISTS) { error = SystemError(GetLastError()); return false; }
    }
    if (raw == INVALID_HANDLE_VALUE) { error = L"Could not create a temporary file beside the destination."; return false; }
    DWORD written = 0;
    const bool okay = bytes.size() <= MAXDWORD && WriteFile(raw, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr)
        && written == bytes.size() && FlushFileBuffers(raw);
    const DWORD writeError = okay ? ERROR_SUCCESS : GetLastError();
    CloseHandle(raw);
    if (!okay) {
        DeleteFileW(temporary.c_str());
        error = L"Could not write the complete file. " + SystemError(writeError); return false;
    }
    bool replaced = false;
    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
        replaced = ReplaceFileW(path.c_str(), temporary.c_str(), nullptr, 0, nullptr, nullptr) != FALSE;
    } else {
        replaced = MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE;
    }
    if (!replaced) {
        const DWORD code = GetLastError();
        DeleteFileW(temporary.c_str());
        error = SystemError(code); return false;
    }
    return true;
}
