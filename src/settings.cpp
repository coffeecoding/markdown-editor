#include "settings.h"
#include "document.h"
#include <algorithm>
#include <array>
#include <cwctype>
#include <string_view>
#include <vector>

namespace {
constexpr unsigned FieldCount = 17;
constexpr uint32_t AllFields = (1u << FieldCount) - 1;
constexpr COLORREF BadBackground = RGB(139, 0, 0), BadForeground = RGB(255, 105, 180);
const wchar_t* Sections[] = {L"appearance", L"font", L"font", L"font",
    L"theme.light", L"theme.light", L"theme.light", L"theme.light", L"theme.light", L"theme.light",
    L"theme.dark", L"theme.dark", L"theme.dark", L"theme.dark", L"theme.dark", L"theme.dark", L"appearance"};
const wchar_t* Keys[] = {L"theme", L"edit", L"prose", L"code", L"background", L"text", L"link",
    L"muted", L"code", L"table_border", L"background", L"text", L"link", L"muted", L"code", L"table_border", L"zoom"};
COLORREF ThemePalette::* const Colors[] = {&ThemePalette::background, &ThemePalette::text,
    &ThemePalette::link, &ThemePalette::muted, &ThemePalette::code, &ThemePalette::tableBorder};

std::wstring_view Trim(std::wstring_view text)
{
    while (!text.empty() && iswspace(text.front())) text.remove_prefix(1);
    while (!text.empty() && iswspace(text.back())) text.remove_suffix(1);
    return text;
}

bool Equal(std::wstring_view a, std::wstring_view b)
{
    return a.size() == b.size() && CompareStringOrdinal(a.data(), static_cast<int>(a.size()),
        b.data(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}

bool Color(std::wstring_view text, COLORREF& result)
{
    if (text.size() != 7 || text.front() != L'#') return false;
    unsigned value = 0;
    for (size_t i = 1; i < text.size(); ++i) {
        const wchar_t c = towlower(text[i]);
        if (c >= L'0' && c <= L'9') value = value * 16 + c - L'0';
        else if (c >= L'a' && c <= L'f') value = value * 16 + c - L'a' + 10;
        else return false;
    }
    result = RGB((value >> 16) & 255, (value >> 8) & 255, value & 255);
    return true;
}

bool InstalledFont(std::wstring_view name)
{
    if (name.empty() || name.size() >= LF_FACESIZE) return false;
    LOGFONTW query{};
    query.lfCharSet = DEFAULT_CHARSET;
    std::copy(name.begin(), name.end(), query.lfFaceName);
    struct Match { std::wstring_view name; bool found = false; } match{name};
    HDC dc = GetDC(nullptr);
    if (!dc) return false;
    EnumFontFamiliesExW(dc, &query, [](const LOGFONTW* font, const TEXTMETRICW*, DWORD, LPARAM data) -> int {
        auto& match = *reinterpret_cast<Match*>(data);
        match.found = Equal(font->lfFaceName, match.name);
        return match.found ? 0 : 1;
    }, reinterpret_cast<LPARAM>(&match), 0);
    ReleaseDC(nullptr, dc);
    return match.found;
}

struct Location { size_t begin = 0, end = 0; };
struct Snapshot {
    Settings settings;
    std::wstring text;
    FileFormat format;
    std::array<Location, FieldCount> locations{};
    bool malformed = false;
};

void BadSnapshot(Snapshot& snapshot)
{
    snapshot.malformed = true;
    snapshot.settings = {};
    snapshot.settings.invalid = AllFields;
    snapshot.settings.edit = snapshot.settings.prose = snapshot.settings.code = L"Consolas";
    snapshot.settings.light = snapshot.settings.night = {BadBackground, BadForeground, BadForeground,
        BadForeground, BadBackground, BadForeground};
}

bool Load(const std::wstring& path, Snapshot& snapshot, DWORD& ioError)
{
    std::wstring error;
    if (!ReadDocument(path, snapshot.text, snapshot.format, error, true, &ioError)) {
        if (ioError == ERROR_FILE_NOT_FOUND || ioError == ERROR_PATH_NOT_FOUND) {
            snapshot.settings.missing = AllFields;
            return true;
        }
        if (ioError) return false;
        BadSnapshot(snapshot);
        return true;
    }
    snapshot.settings.missing = AllFields;
    std::wstring section;
    std::array<std::wstring, FieldCount> values;
    for (size_t offset = 0; offset < snapshot.text.size();) {
        size_t end = snapshot.text.find_first_of(L"\r\n", offset);
        if (end == std::wstring::npos) end = snapshot.text.size();
        const auto line = Trim(std::wstring_view(snapshot.text).substr(offset, end - offset));
        if (!line.empty() && line.front() != L';') {
            if (line.front() == L'[') {
                if (line.back() != L']' || line.size() < 3 || line.find(L']') != line.size() - 1) {
                    BadSnapshot(snapshot); return true;
                }
                section = Trim(line.substr(1, line.size() - 2));
                if (section.empty()) { BadSnapshot(snapshot); return true; }
            } else {
                const auto equals = line.find(L'=');
                if (section.empty() || equals == std::wstring_view::npos || Trim(line.substr(0, equals)).empty()) {
                    BadSnapshot(snapshot); return true;
                }
                const auto key = Trim(line.substr(0, equals));
                const auto value = Trim(line.substr(equals + 1));
                for (unsigned i = 0; i < FieldCount; ++i) {
                    if (!Equal(section, Sections[i]) || !Equal(key, Keys[i])) continue;
                    snapshot.settings.missing &= ~(1u << i);
                    values[i] = value;
                    // Replace the trimmed value only, retaining key spelling and whitespace.
                    snapshot.locations[i] = {static_cast<size_t>(value.data() - snapshot.text.data()),
                        static_cast<size_t>(value.data() - snapshot.text.data() + value.size())};
                    break;
                }
            }
        }
        offset = end;
        if (offset < snapshot.text.size() && snapshot.text[offset++] == L'\r' &&
            offset < snapshot.text.size() && snapshot.text[offset] == L'\n') ++offset;
    }
    for (unsigned i = 0; i < FieldCount; ++i) {
        if (snapshot.settings.missing & (1u << i)) continue;
        const auto& value = values[i];
        bool valid = true;
        if (i == 16) {
            int percent = 0;
            valid = !value.empty() && value.size() <= 3;
            if (valid) for (wchar_t c : value) {
                if (c < L'0' || c > L'9') { valid = false; break; }
                percent = percent * 10 + c - L'0';
            }
            valid = valid && percent >= 50 && percent <= 300;
            snapshot.settings.zoomPercent = valid ? percent : 100;
        } else if (i == 0) {
            valid = Equal(value, L"light") || Equal(value, L"dark");
            snapshot.settings.dark = Equal(value, L"dark");
        } else if (i < 4) {
            valid = InstalledFont(value);
            auto& font = i == 1 ? snapshot.settings.edit : i == 2 ? snapshot.settings.prose : snapshot.settings.code;
            font = valid ? value : L"Consolas";
        } else {
            const unsigned color = (i - 4) % 6;
            auto& palette = i < 10 ? snapshot.settings.light : snapshot.settings.night;
            valid = Color(value, palette.*Colors[color]);
            if (!valid) palette.*Colors[color] = color == 0 || color == 4 ? BadBackground : BadForeground;
        }
        if (!valid) snapshot.settings.invalid |= 1u << i;
    }
    return true;
}

std::wstring Default(unsigned field)
{
    if (field == 16) return L"100";
    if (field == 0) return L"light";
    if (field < 4) return field == 2 ? L"Segoe UI" : L"Consolas";
    const COLORREF value = Palette(field >= 10).*Colors[(field - 4) % 6];
    wchar_t text[8]{};
    swprintf_s(text, L"#%02X%02X%02X", GetRValue(value), GetGValue(value), GetBValue(value));
    return text;
}

bool Write(const std::wstring& path, Snapshot& snapshot, uint32_t fields, bool dark, bool preferenceOnly, std::wstring& error, int zoom = 100)
{
    struct Replacement { Location location; std::wstring value; };
    std::vector<Replacement> replacements;
    std::wstring appended, section;
    for (unsigned i = 0; i < FieldCount; ++i) {
        if (!(fields & (1u << i))) continue;
        const std::wstring value = preferenceOnly ? (i == 16 ? std::to_wstring(zoom) : (dark ? L"dark" : L"light")) : Default(i);
        if (snapshot.settings.missing & (1u << i)) {
            if (section != Sections[i]) {
                section = Sections[i];
                appended += L"[" + section + L"]" + snapshot.format.newline;
            }
            appended += std::wstring(Keys[i]) + L"=" + value + snapshot.format.newline;
        } else replacements.push_back({snapshot.locations[i], value});
    }
    std::sort(replacements.begin(), replacements.end(), [](const Replacement& a, const Replacement& b) {
        return a.location.begin > b.location.begin;
    });
    for (const auto& r : replacements) snapshot.text.replace(r.location.begin, r.location.end - r.location.begin, r.value);
    if (!appended.empty()) {
        if (!snapshot.text.empty() && snapshot.text.back() != L'\n' && snapshot.text.back() != L'\r')
            snapshot.text += snapshot.format.newline;
        snapshot.text += appended;
    }
    // Existing line endings stay byte-for-byte, including mixed newlines.
    snapshot.format.newline = L"\n";
    return WriteDocument(path, snapshot.text, snapshot.format, error);
}
}

bool SamePalette(const ThemePalette& a, const ThemePalette& b)
{
    for (auto field : Colors) if (a.*field != b.*field) return false;
    return true;
}

bool ReadSettings(const std::wstring& path, Settings& settings, DWORD& ioError)
{
    Snapshot snapshot;
    if (!Load(path, snapshot, ioError)) return false;
    settings = std::move(snapshot.settings);
    return true;
}

bool RepairSettings(const std::wstring& path, std::wstring& error)
{
    Snapshot snapshot;
    DWORD code = 0;
    if (!Load(path, snapshot, code)) { error = SystemError(code); return false; }
    if (snapshot.malformed) {
        // Never overwrite an unreadable configuration without preserving its bytes.
        bool backedUp = false;
        for (unsigned n = 0; n < 1000; ++n) {
            const auto backup = path + L".invalid-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(n) + L".bak";
            if (CopyFileW(path.c_str(), backup.c_str(), TRUE)) { backedUp = true; break; }
            const DWORD failure = GetLastError();
            if (failure != ERROR_FILE_EXISTS && failure != ERROR_ALREADY_EXISTS) { error = SystemError(failure); return false; }
        }
        if (!backedUp) { error = L"Could not create a unique settings backup."; return false; }
        snapshot.text.clear();
        snapshot.settings.missing = AllFields;
    }
    const uint32_t fields = snapshot.settings.missing | snapshot.settings.invalid;
    return !fields || Write(path, snapshot, fields, false, false, error);
}

bool SaveTheme(const std::wstring& path, bool dark, std::wstring& error)
{
    Snapshot snapshot;
    DWORD code = 0;
    if (!Load(path, snapshot, code)) { error = SystemError(code); return false; }
    if (snapshot.malformed) { error = L"The INI cannot be parsed. Correct it or close the editor to repair it."; return false; }
    if (!(snapshot.settings.missing & 1) && !(snapshot.settings.invalid & 1) && snapshot.settings.dark == dark) return true;
    return Write(path, snapshot, 1, dark, true, error);
}

bool SaveZoom(const std::wstring& path, int percent, std::wstring& error)
{
    if (percent < 50 || percent > 300) { error = L"Invalid zoom percentage."; return false; }
    Snapshot snapshot;
    DWORD code = 0;
    if (!Load(path, snapshot, code)) { error = SystemError(code); return false; }
    if (snapshot.malformed) { error = L"The INI cannot be parsed. Correct it or close the editor to repair it."; return false; }
    constexpr uint32_t field = 1u << 16;
    if (!((snapshot.settings.missing | snapshot.settings.invalid) & field) && snapshot.settings.zoomPercent == percent) return true;
    return Write(path, snapshot, field, false, true, error, percent);
}

void CALLBACK SettingsWatcher::Callback(PTP_CALLBACK_INSTANCE, void* context, PTP_WAIT, TP_WAIT_RESULT)
{
    const auto& watcher = *static_cast<SettingsWatcher*>(context);
    PostMessageW(watcher.window_, watcher.message_, 0, 0);
}

bool SettingsWatcher::Arm(DWORD& error)
{
    ResetEvent(io_.hEvent);
    if (!ReadDirectoryChangesW(directory_, buffer_, sizeof(buffer_), FALSE,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
        nullptr, &io_, nullptr)) { error = GetLastError(); return false; }
    pending_ = true;
    SetThreadpoolWait(wait_, io_.hEvent, nullptr);
    return true;
}

bool SettingsWatcher::Start(HWND window, const std::wstring& path, UINT message, DWORD& error)
{
    Stop();
    window_ = window;
    message_ = message;
    const auto slash = path.find_last_of(L"\\/");
    filename_ = path.substr(slash + 1);
    directory_ = CreateFileW(path.substr(0, slash + 1).c_str(), FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
    if (directory_ == INVALID_HANDLE_VALUE) { error = GetLastError(); return false; }
    io_.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!io_.hEvent) { error = GetLastError(); Stop(); return false; }
    wait_ = CreateThreadpoolWait(Callback, this, nullptr);
    if (!wait_) { error = GetLastError(); Stop(); return false; }
    if (!Arm(error)) { Stop(); return false; }
    return true;
}

bool SettingsWatcher::Changed(bool& changed, DWORD& error)
{
    changed = false;
    if (!pending_) return true;
    // Finish the one-shot callback before reusing its event and OVERLAPPED.
    WaitForThreadpoolWaitCallbacks(wait_, FALSE);
    DWORD bytes = 0;
    const BOOL okay = GetOverlappedResult(directory_, &io_, &bytes, FALSE);
    const DWORD code = okay ? ERROR_SUCCESS : GetLastError();
    pending_ = false;
    if (!okay && code != ERROR_NOTIFY_ENUM_DIR) { error = code; Stop(); return false; }
    changed = bytes == 0 || code == ERROR_NOTIFY_ENUM_DIR;
    for (DWORD offset = 0; bytes && offset < bytes;) {
        const auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(reinterpret_cast<const BYTE*>(buffer_) + offset);
        if (Equal(std::wstring_view(info->FileName, info->FileNameLength / sizeof(wchar_t)), filename_)) changed = true;
        if (!info->NextEntryOffset) break;
        offset += info->NextEntryOffset;
    }
    if (!Arm(error)) { Stop(); return false; }
    return true;
}

void SettingsWatcher::Stop()
{
    if (wait_) SetThreadpoolWait(wait_, nullptr, nullptr);
    if (pending_) {
        CancelIoEx(directory_, &io_);
        DWORD ignored = 0;
        GetOverlappedResult(directory_, &io_, &ignored, TRUE);
        pending_ = false;
    }
    if (wait_) { WaitForThreadpoolWaitCallbacks(wait_, TRUE); CloseThreadpoolWait(wait_); wait_ = nullptr; }
    if (io_.hEvent) CloseHandle(io_.hEvent);
    io_ = {};
    if (directory_ != INVALID_HANDLE_VALUE) CloseHandle(directory_);
    directory_ = INVALID_HANDLE_VALUE;
}
