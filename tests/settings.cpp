#include "../src/settings.h"
#include "../src/document.h"
#include <cstdio>
#include <stdexcept>

namespace {
void Check(bool condition, const char* description)
{
    if (!condition) throw std::runtime_error(description);
}

std::wstring path;
void Put(const std::wstring& text, Encoding encoding = Encoding::Utf8)
{
    FileFormat format;
    format.encoding = encoding;
    format.newline = L"\n";
    std::wstring error;
    Check(WriteDocument(path, text, format, error), "write fixture");
}

std::wstring Text(FileFormat* format = nullptr)
{
    FileFormat readFormat;
    std::wstring text, error;
    Check(ReadDocument(path, text, readFormat, error, true), "read fixture");
    if (format) *format = readFormat;
    return text;
}

Settings Read()
{
    Settings settings;
    DWORD code = 0;
    Check(ReadSettings(path, settings, code), "read settings snapshot");
    return settings;
}

void Repair()
{
    std::wstring error;
    Check(RepairSettings(path, error), "repair settings");
}

void ParserAndRepair()
{
    DeleteFileW(path.c_str());
    auto settings = Read();
    Check(settings.missing == 0xffff && !settings.invalid && settings.prose == L"Segoe UI", "missing uses normal defaults");
    Check(GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES, "read never populates missing file");
    Repair();
    settings = Read();
    Check(!settings.missing && !settings.invalid, "complete defaults on close");
    WIN32_FILE_ATTRIBUTE_DATA before{}, after{};
    Check(GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &before), "stat complete settings");
    Repair();
    Check(GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &after), "stat after no-op repair");
    Check(CompareFileTime(&before.ftLastWriteTime, &after.ftLastWriteTime) == 0, "valid repair skips writes");

    for (Encoding encoding : {Encoding::Utf8, Encoding::Utf8Bom, Encoding::Utf16Le, Encoding::Utf16Be}) {
        const std::wstring original = L"; Notes Ω 日本語\r\n [APPEARANCE] \n theme = DARK \r"
            L"[unknown]\nother = valid=value\r\n[font]\nedit=Arial\nprose=\ncode=No such font Ω\n"
            L"[theme.light]\ntext=#123456\n TEXT = #GG0000 \t\nbackground=\nlink=wrong\nmuted=#123\ncode=#11223344\ntable_border=no\n"
            L"[THEME.DARK]\r\n BACKGROUND = #aBcDeF \t\r\n";
        Put(original, encoding);
        settings = Read();
        Check(settings.dark && settings.edit == L"Arial" && settings.prose == L"Consolas" && settings.code == L"Consolas", "independent fonts and empty font error");
        Check(settings.light.background == RGB(139, 0, 0) && settings.light.code == RGB(139, 0, 0), "invalid background errors");
        Check(settings.light.text == RGB(255, 105, 180) && settings.light.link == RGB(255, 105, 180) &&
            settings.light.muted == RGB(255, 105, 180) && settings.light.tableBorder == RGB(255, 105, 180), "last occurrence wins and foreground errors");
        Check(settings.night.background == RGB(171, 205, 239) && settings.night.text == Palette(true).text, "case, whitespace, missing fields");
        Check(Text() == original, "loading leaves invalid entries untouched");
        Repair();
        FileFormat format;
        const auto repaired = Text(&format);
        Check(format.encoding == encoding, "repair preserves encoding and BOM");
        Check(repaired.find(L"; Notes Ω 日本語\r\n [APPEARANCE] \n theme = DARK \r[unknown]\nother = valid=value\r\n") == 0, "repair preserves comments, unknown entries and mixed newlines");
        Check(repaired.find(L" BACKGROUND = #aBcDeF \t\r\n") != std::wstring::npos &&
            repaired.find(L"text=#123456\n TEXT = #242424 \t\n") != std::wstring::npos, "repair edits only last invalid occurrence");
        settings = Read();
        Check(!settings.invalid && !settings.missing && settings.dark && settings.edit == L"Arial" && settings.prose == L"Segoe UI", "repair uses normal defaults and keeps choices");
    }

    Put(L"; keep\n[appearance]\ntheme=\n[font]\nedit=Consolas\n");
    settings = Read();
    Check(!settings.dark && (settings.invalid & 1), "empty theme invalid, Light effective");
    std::wstring error;
    Check(SaveTheme(path, true, error), "save theme immediately");
    Check(Text() == L"; keep\n[appearance]\ntheme=dark\n[font]\nedit=Consolas\n", "theme save changes only preference");
    for (unsigned i = 0; i < 20; ++i)
        Check(SaveTheme(path, i % 2 != 0, error), "rapid theme persistence");

    HANDLE locked = CreateFileW(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(locked != INVALID_HANDLE_VALUE, "lock settings");
    DWORD code = 0;
    Check(!ReadSettings(path, settings, code) && code == ERROR_SHARING_VIOLATION, "lock distinct from invalid data");
    Check(!RepairSettings(path, error), "locked repair fails safely");
    CloseHandle(locked);
    Check(Read().dark, "lock failure leaves original intact");

    // Exercise an installed non-ASCII family when this Windows image has one.
    std::wstring unicodeFamily;
    LOGFONTW query{};
    query.lfCharSet = DEFAULT_CHARSET;
    HDC dc = GetDC(nullptr);
    EnumFontFamiliesExW(dc, &query, [](const LOGFONTW* font, const TEXTMETRICW*, DWORD, LPARAM data) -> int {
        if (font->lfFaceName[0] == L'@') return 1;
        for (const wchar_t* c = font->lfFaceName; *c; ++c) {
            if (*c > 127) { *reinterpret_cast<std::wstring*>(data) = font->lfFaceName; return 0; }
        }
        return 1;
    }, reinterpret_cast<LPARAM>(&unicodeFamily), 0);
    ReleaseDC(nullptr, dc);
    if (!unicodeFamily.empty()) {
        Put(L"[font]\nedit=" + unicodeFamily + L"\nprose=" + unicodeFamily + L"\ncode=" + unicodeFamily + L"\n");
        settings = Read();
        Check(!settings.invalid && settings.edit == unicodeFamily && settings.prose == unicodeFamily &&
            settings.code == unicodeFamily, "installed Unicode font family accepted in each role");
        printf("Installed Unicode font verified: %s\n", ToUtf8(unicodeFamily).c_str());
    } else puts("No installed non-ASCII family: Unicode font acceptance needs another Windows image.");

    for (const auto& malformed : {L"[broken\ntext=#000000\n", L"[font]\nnot a setting\n", L"outside=section\n", L"[ ]\n"}) {
        Put(malformed, Encoding::Utf16Be);
        settings = Read();
        Check(settings.invalid == 0xffff && settings.prose == L"Consolas" && settings.light.text == RGB(255, 105, 180), "malformed snapshot has global errors");
        Repair();
        Check(!Read().invalid && !Read().missing, "malformed repair generates complete defaults");
        WIN32_FIND_DATAW found{};
        HANDLE search = FindFirstFileW((path + L".invalid-*.bak").c_str(), &found);
        Check(search != INVALID_HANDLE_VALUE, "malformed original backed up");
        FindClose(search);
        const auto backup = path.substr(0, path.find_last_of(L"\\/") + 1) + found.cFileName;
        std::wstring bytes, message;
        FileFormat format;
        Check(ReadDocument(backup, bytes, format, message, true) && bytes == malformed && format.encoding == Encoding::Utf16Be, "backup preserves original encoding and content");
        DeleteFileW(backup.c_str());
    }
}

void Watcher()
{
    constexpr UINT Notification = WM_APP + 10;
    HWND window = CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
    Check(window != nullptr, "create watcher message window");
    SettingsWatcher watcher;
    DWORD code = 0;
    Check(watcher.Start(window, path, Notification, code), "start asynchronous watcher");
    const auto wait = [&](bool wanted = true) {
        const ULONGLONG deadline = GetTickCount64() + 4000;
        MSG message{};
        while (GetTickCount64() < deadline) {
            if (PeekMessageW(&message, window, Notification, Notification, PM_REMOVE)) {
                bool changed = false;
                Check(watcher.Changed(changed, code), "complete and rearm watcher");
                Check(wanted || !changed, "unrelated filename filtered out");
                if (changed == wanted) return;
            }
            MsgWaitForMultipleObjectsEx(0, nullptr, 20, QS_POSTMESSAGE, MWMO_INPUTAVAILABLE);
        }
        Check(false, "watcher notification timeout");
    };
    const auto other = path + L".unrelated";
    HANDLE unrelated = CreateFileW(other.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    Check(unrelated != INVALID_HANDLE_VALUE, "create unrelated notification");
    CloseHandle(unrelated);
    wait(false);
    DeleteFileW(other.c_str());
    Put(L"[font]\nedit=Arial\n");
    wait(); // Atomic replacement with a differently named temporary file.
    Check(DeleteFileW(path.c_str()), "delete INI");
    wait();
    Put(L"[font]\nedit=Consolas\n");
    wait();
    // Leave completions unconsumed while filling the native notification
    // buffer. The next arm must deliver overflow as a request to rescan.
    for (unsigned i = 0; i < 400; ++i) {
        HANDLE file = CreateFileW(other.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
        Check(file != INVALID_HANDLE_VALUE, "create overflow fixture");
        CloseHandle(file);
        DeleteFileW(other.c_str());
    }
    wait();
    watcher.Stop();
    Check(!watcher.Start(window, path + L"\\missing\\config.ini", Notification, code), "watcher start failure reported");
    Check(watcher.Start(window, path, Notification, code), "restart watcher");
    Put(L"[appearance]\ntheme=dark\n");
    watcher.Stop(); // Drain callbacks with completion potentially queued.
    DestroyWindow(window);
}
}

int wmain()
{
    wchar_t directory[32768]{};
    GetCurrentDirectoryW(32768, directory);
    path = std::wstring(directory) + L"\\settings-check.ini";
    try {
        ParserAndRepair();
        Watcher();
        DeleteFileW(path.c_str());
        puts("Settings parser, repair, encodings, locks and asynchronous watcher checks passed.");
        return 0;
    } catch (const std::exception& error) {
        fprintf(stderr, "Settings check failed: %s\n", error.what());
        return 1;
    }
}
