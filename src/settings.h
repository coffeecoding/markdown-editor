#pragma once
#include "theme.h"
#include <cstdint>
#include <string>

struct Settings {
    bool dark = false;
    int zoomPercent = 100;
    ThemePalette light = Palette(false), night = Palette(true);
    std::wstring edit = L"Consolas", prose = L"Segoe UI", code = L"Consolas";
    uint32_t missing = 0, invalid = 0;
    const ThemePalette& Colors() const { return dark ? night : light; }
};

bool SamePalette(const ThemePalette& a, const ThemePalette& b);
// False means an I/O failure: retain the current appearance and retry locks.
// Malformed snapshots return true with conspicuous effective error settings.
bool ReadSettings(const std::wstring& path, Settings& settings, DWORD& ioError);
bool RepairSettings(const std::wstring& path, std::wstring& error);
bool SaveZoom(const std::wstring& path, int percent, std::wstring& error);
bool SaveTheme(const std::wstring& path, bool dark, std::wstring& error);

// All operations except Callback run on the window thread. The wait callback
// only posts a message; no worker touches controls or parses settings.
class SettingsWatcher {
public:
    SettingsWatcher() = default;
    SettingsWatcher(const SettingsWatcher&) = delete;
    SettingsWatcher& operator=(const SettingsWatcher&) = delete;
    ~SettingsWatcher() { Stop(); }
    bool Start(HWND window, const std::wstring& path, UINT message, DWORD& error);
    bool Changed(bool& changed, DWORD& error);
    void Stop();
private:
    static void CALLBACK Callback(PTP_CALLBACK_INSTANCE, void*, PTP_WAIT, TP_WAIT_RESULT);
    bool Arm(DWORD& error);
    HWND window_{};
    UINT message_{};
    std::wstring filename_;
    HANDLE directory_ = INVALID_HANDLE_VALUE;
    OVERLAPPED io_{};
    PTP_WAIT wait_{};
    bool pending_ = false;
    DWORD buffer_[2048]{};
};
