#include <windows.h>
#include <dwmapi.h>
#include <uxtheme.h>

namespace {

constexpr int ToggleButtonId = 1;
bool dark_mode = false;
HWND theme_label = nullptr;
HWND toggle_button = nullptr;
HBRUSH background_brush = nullptr;

void ApplyTheme(HWND window)
{
    const COLORREF background = dark_mode ? RGB(32, 32, 32) : RGB(255, 255, 255);
    DeleteObject(background_brush);
    background_brush = CreateSolidBrush(background);

    SetWindowTextW(theme_label, dark_mode ? L"Current theme: Dark" : L"Current theme: Light");
    SetWindowTheme(toggle_button, dark_mode ? L"DarkMode_Explorer" : L"Explorer", nullptr);

    const BOOL use_dark_title_bar = dark_mode;
    DwmSetWindowAttribute(window, 20, &use_dark_title_bar, sizeof(use_dark_title_bar));

    InvalidateRect(window, nullptr, TRUE);
    InvalidateRect(toggle_button, nullptr, TRUE);
}

} // namespace

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE: {
        HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(window, GWLP_HINSTANCE));

        theme_label = CreateWindowW(
            L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            24, 24, 220, 24, window, nullptr, instance, nullptr);

        toggle_button = CreateWindowW(
            L"BUTTON", L"Toggle theme", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            24, 58, 130, 32, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ToggleButtonId)), instance, nullptr);

        HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        SendMessageW(theme_label, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(toggle_button, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

        ApplyTheme(window);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == ToggleButtonId && HIWORD(wparam) == BN_CLICKED) {
            dark_mode = !dark_mode;
            ApplyTheme(window);
            return 0;
        }
        break;
    case WM_CTLCOLORSTATIC: {
        HDC device_context = reinterpret_cast<HDC>(wparam);
        SetTextColor(device_context, dark_mode ? RGB(240, 240, 240) : RGB(0, 0, 0));
        SetBkColor(device_context, dark_mode ? RGB(32, 32, 32) : RGB(255, 255, 255));
        return reinterpret_cast<LRESULT>(background_brush);
    }
    case WM_ERASEBKGND: {
        RECT client_area{};
        GetClientRect(window, &client_area);
        FillRect(reinterpret_cast<HDC>(wparam), &client_area, background_brush);
        return 1;
    }
    case WM_DESTROY:
        DeleteObject(background_brush);
        background_brush = nullptr;
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(window, message, wparam, lparam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command)
{
    const wchar_t class_name[] = L"Win32AppDemoWindow";

    WNDCLASSW window_class{};
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.lpszClassName = class_name;

    RegisterClassW(&window_class);

    HWND window = CreateWindowExW(
        0,
        class_name,
        L"Win32 App Demo",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 640, 480,
        nullptr, nullptr, instance, nullptr);

    if (window == nullptr) {
        return 1;
    }

    ShowWindow(window, show_command);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}
