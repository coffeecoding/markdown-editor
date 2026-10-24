#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <richole.h>
#include <tom.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <gdiplus.h>
#include "document.h"
#include "markdown.h"
#include <algorithm>
#include <cstring>
#include <new>
#include <string>
#include <vector>

namespace {
constexpr wchar_t WindowClass[] = L"NativeMarkdownEditorWindow";
constexpr int OpenId = 100, SaveId = 101, ExitId = 102;
constexpr int EditId = 200, ViewId = 201, ToggleId = 202;
constexpr int DarkThemeId = 203;
constexpr int SourceId = 300, PreviewId = 301;
constexpr LONG TextLimit = 32 * 1024 * 1024;

struct Position {
    CHARRANGE selection{};
    POINT scroll{};
    LONG firstLine = 0;
};

Position Remember(HWND control)
{
    Position p;
    SendMessageW(control, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&p.selection));
    SendMessageW(control, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&p.scroll));
    p.firstLine = static_cast<LONG>(SendMessageW(control, EM_GETFIRSTVISIBLELINE, 0, 0));
    return p;
}

void Restore(HWND control, const Position& p, bool exactPixels = true)
{
    SendMessageW(control, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&p.selection));
    if (exactPixels) SendMessageW(control, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&p.scroll));
    // Also handles Rich Edit's 16-bit pixel scroll range on long documents.
    const LONG current = static_cast<LONG>(SendMessageW(control, EM_GETFIRSTVISIBLELINE, 0, 0));
    if (!exactPixels || std::abs(current - p.firstLine) > 2)
        SendMessageW(control, EM_LINESCROLL, 0, p.firstLine - current);
}

struct App {
    HINSTANCE instance{};
    HWND window{}, source{}, preview{};
    bool editMode = true, dirty = false, renderDirty = true, loading = false;
    bool darkTheme = false;
    UINT dpi = 96;
    UINT richEditBaseDpi = 96;
    std::wstring path, savedSource, privateFontPath, customFamily, iniPath;
    FileFormat format;
    RenderOptions rendering;

    ~App() { if (!privateFontPath.empty()) RemoveFontResourceExW(privateFontPath.c_str(), FR_PRIVATE, nullptr); }

    void Error(const std::wstring& message)
    {
        MessageBoxW(window, message.c_str(), L"Markdown Editor", MB_OK | MB_ICONERROR);
    }

    std::wstring Source() const
    {
        GETTEXTLENGTHEX length{GTL_PRECISE | GTL_NUMCHARS, 1200};
        const auto count = SendMessageW(source, EM_GETTEXTLENGTHEX, reinterpret_cast<WPARAM>(&length), 0);
        std::wstring text(static_cast<size_t>(count) + 1, 0);
        GETTEXTEX get{static_cast<DWORD>(text.size() * sizeof(wchar_t)), GT_DEFAULT, 1200, nullptr, nullptr};
        const auto copied = SendMessageW(source, EM_GETTEXTEX, reinterpret_cast<WPARAM>(&get), reinterpret_cast<LPARAM>(text.data()));
        text.resize(static_cast<size_t>(copied));
        return NormalizeNewlines(text);
    }

    void Title()
    {
        const auto slash = path.find_last_of(L"\\/");
        std::wstring filename = path.empty() ? L"Untitled" : path.substr(slash == std::wstring::npos ? 0 : slash + 1);
        SetWindowTextW(window, ((dirty ? L"*" : L"") + filename + L" — Markdown Editor").c_str());
    }

    void LoadSettings()
    {
        std::vector<wchar_t> module(32768);
        GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
        std::wstring directory(module.data());
        directory.resize(directory.find_last_of(L"\\/") + 1);
        iniPath = directory + L"markdown-editor.ini";
        wchar_t theme[32]{};
        GetPrivateProfileStringW(L"appearance", L"theme", L"light", theme, ARRAYSIZE(theme), iniPath.c_str());
        darkTheme = _wcsicmp(theme, L"dark") == 0;
        rendering.palette = Palette(darkTheme);
        wchar_t configured[32768]{};
        GetPrivateProfileStringW(L"font", L"path", L"", configured, ARRAYSIZE(configured), iniPath.c_str());
        std::wstring file(configured);
        if (file.empty()) return;
        if (!(file.size() >= 2 && file[1] == L':') && file[0] != L'\\' && file[0] != L'/') file = directory + file;
        const auto dot = file.find_last_of(L'.');
        if (dot == std::wstring::npos || _wcsicmp(file.c_str() + dot, L".ttf") != 0) return;
        Gdiplus::GdiplusStartupInput input;
        ULONG_PTR token = 0;
        if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok) return;
        {
            // Windows reads the font metadata; no application font parser.
            Gdiplus::PrivateFontCollection collection;
            if (collection.AddFontFile(file.c_str()) == Gdiplus::Ok && collection.GetFamilyCount() > 0) {
                Gdiplus::FontFamily family;
                INT found = 0;
                wchar_t name[LF_FACESIZE]{};
                if (collection.GetFamilies(1, &family, &found) == Gdiplus::Ok && found &&
                    family.GetFamilyName(name) == Gdiplus::Ok && AddFontResourceExW(file.c_str(), FR_PRIVATE, nullptr)) {
                    privateFontPath = file;
                    customFamily = name;
                    rendering.bodyFont = rendering.codeFont = customFamily;
                }
            }
        }
        Gdiplus::GdiplusShutdown(token);
    }

    bool ApplyTheme()
    {
        // TOM suspends recording without discarding either undo or redo history.
        IRichEditOle* ole = nullptr;
        ITextDocument* document = nullptr;
        SendMessageW(source, EM_GETOLEINTERFACE, 0, reinterpret_cast<LPARAM>(&ole));
        if (ole) {
            ole->QueryInterface(__uuidof(ITextDocument), reinterpret_cast<void**>(&document));
            ole->Release();
        }
        if (!document) { Error(L"Windows could not change the document theme."); return false; }
        const HRESULT suspended = document->Undo(tomSuspend, nullptr);
        if (FAILED(suspended)) { document->Release(); Error(L"Windows could not preserve undo history while changing the theme."); return false; }
        const auto position = Remember(source);
        const LRESULT modified = SendMessageW(source, EM_GETMODIFY, 0, 0);
        const LRESULT events = SendMessageW(source, EM_SETEVENTMASK, 0, 0);
        // WM_SETREDRAW(TRUE) also sets WS_VISIBLE; never send it to the hidden source.
        if (editMode) SendMessageW(source, WM_SETREDRAW, FALSE, 0);
        rendering.palette = Palette(darkTheme);
        CHARFORMAT2W colors{};
        colors.cbSize = sizeof(colors);
        colors.dwMask = CFM_COLOR;
        colors.crTextColor = rendering.palette.text;
        SendMessageW(source, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&colors));
        SendMessageW(source, EM_SETBKGNDCOLOR, 0, rendering.palette.background);
        SendMessageW(preview, EM_SETBKGNDCOLOR, 0, rendering.palette.background);
        Restore(source, position);
        SendMessageW(source, EM_SETMODIFY, modified, 0);
        SendMessageW(source, EM_SETEVENTMASK, 0, events);
        document->Undo(tomResume, nullptr);
        document->Release();
        if (editMode) SendMessageW(source, WM_SETREDRAW, TRUE, 0);
        // Best effort: older Windows versions may reject either attribute.
        const BOOL dark = darkTheme;
        if (FAILED(DwmSetWindowAttribute(window, 20, &dark, sizeof(dark))))
            DwmSetWindowAttribute(window, 19, &dark, sizeof(dark));
        CheckMenuItem(GetMenu(window), DarkThemeId, MF_BYCOMMAND | (darkTheme ? MF_CHECKED : MF_UNCHECKED));
        renderDirty = true;
        if (!editMode) Render();
        for (HWND child : {source, preview}) InvalidateRect(child, nullptr, TRUE);
        return true;
    }

    void ToggleTheme()
    {
        darkTheme = !darkTheme;
        if (!ApplyTheme()) { darkTheme = !darkTheme; return; }
        if (!WritePrivateProfileStringW(L"appearance", L"theme", darkTheme ? L"dark" : L"light", iniPath.c_str())) {
            const DWORD code = GetLastError();
            Error(L"The theme is active for this session, but could not be saved to:\n" + iniPath +
                  L"\n\n" + SystemError(code));
        }
    }

    void Layout()
    {
        RECT client{};
        GetClientRect(window, &client);
        for (HWND child : {source, preview}) {
            if (!child) continue;
            MoveWindow(child, 0, 0, client.right, client.bottom, TRUE);
            RECT text{MulDiv(18, dpi, 96), MulDiv(14, dpi, 96),
                std::max<LONG>(MulDiv(19, dpi, 96), client.right - MulDiv(18, dpi, 96) - GetSystemMetricsForDpi(SM_CXVSCROLL, dpi)),
                std::max<LONG>(MulDiv(15, dpi, 96), client.bottom - MulDiv(14, dpi, 96))};
            SendMessageW(child, EM_SETRECTNP, 0, reinterpret_cast<LPARAM>(&text));
            InvalidateRect(child, nullptr, TRUE);
        }
    }

    void Scale(UINT newDpi)
    {
        dpi = newDpi;
        // Rich Edit uses the screen DC's logical DPI for twips. Zoom supplies
        // the monitor ratio without rewriting text or touching undo history.
        for (HWND child : {source, preview}) SendMessageW(child, EM_SETZOOM, dpi, richEditBaseDpi);
        Layout();
    }

    bool Render()
    {
        if (!renderDirty) return true;
        std::string rtf;
        if (!RenderMarkdown(Source(), rendering, rtf)) { Error(L"Could not render this document. The Markdown source is still available in Edit mode."); return false; }
        struct Stream { const std::string& text; size_t offset = 0; } input{rtf};
        EDITSTREAM stream{};
        stream.dwCookie = reinterpret_cast<DWORD_PTR>(&input);
        stream.pfnCallback = [](DWORD_PTR cookie, LPBYTE buffer, LONG requested, LONG* supplied) -> DWORD {
            auto& data = *reinterpret_cast<Stream*>(cookie);
            const size_t size = std::min(static_cast<size_t>(requested), data.text.size() - data.offset);
            std::memcpy(buffer, data.text.data() + data.offset, size);
            data.offset += size;
            *supplied = static_cast<LONG>(size);
            return 0;
        };
        const auto position = Remember(preview);
        SendMessageW(preview, EM_STREAMIN, SF_RTF, reinterpret_cast<LPARAM>(&stream));
        if (stream.dwError) { Error(L"Windows could not display the rendered document. The source has been preserved."); return false; }
        SendMessageW(preview, EM_EMPTYUNDOBUFFER, 0, 0);
        SendMessageW(preview, EM_SETMODIFY, FALSE, 0);
        SendMessageW(preview, EM_SETZOOM, dpi, richEditBaseDpi);
        Restore(preview, position);
        renderDirty = false;
        return true;
    }

    bool Mode(bool edit)
    {
        if (!edit && !Render()) return false;
        const auto position = Remember(edit ? source : preview);
        // Stop a typing group at a mode boundary, but retain every undo record.
        if (editMode && !edit) SendMessageW(source, EM_STOPGROUPTYPING, 0, 0);
        editMode = edit;
        ShowWindow(edit ? preview : source, SW_HIDE);
        ShowWindow(edit ? source : preview, SW_SHOW);
        CheckMenuRadioItem(GetSubMenu(GetMenu(window), 1), EditId, ViewId, edit ? EditId : ViewId, MF_BYCOMMAND);
        SetFocus(edit ? source : preview);
        Restore(edit ? source : preview, position);
        if (!edit) HideCaret(preview);
        return true;
    }

    bool ChooseFile(bool save, std::wstring& selected)
    {
        std::vector<wchar_t> buffer(32768, 0);
        if (save) wcscpy_s(buffer.data(), buffer.size(), L"Untitled.md");
        OPENFILENAMEW dialog{sizeof(dialog)};
        dialog.hwndOwner = window;
        dialog.lpstrFilter = L"Markdown files (*.md)\0*.md\0All files (*.*)\0*.*\0\0";
        dialog.lpstrFile = buffer.data();
        dialog.nMaxFile = static_cast<DWORD>(buffer.size());
        dialog.lpstrDefExt = L"md";
        dialog.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_ENABLESIZING |
            (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
        if (!(save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog))) {
            const DWORD code = CommDlgExtendedError();
            if (code) Error(L"The file dialog failed (code " + std::to_wstring(code) + L").");
            return false;
        }
        selected = buffer.data();
        return true;
    }

    bool Save()
    {
        std::wstring destination = path;
        if (destination.empty() && !ChooseFile(true, destination)) return false;
        auto text = Source();
        // Avoid normalizing mixed line endings or replacing a clean file.
        if (!path.empty() && !dirty && text == savedSource) return true;
        std::wstring error;
        if (!WriteDocument(destination, text, format, error)) {
            Error(L"Could not save:\n" + destination + L"\n\n" + error + L"\nYour unsaved source remains in the editor.");
            return false;
        }
        path = std::move(destination);
        savedSource = std::move(text);
        dirty = false;
        SendMessageW(source, EM_SETMODIFY, FALSE, 0);
        Title();
        return true;
    }

    bool ResolveUnsaved()
    {
        if (!dirty) return true;
        // Reconcile a manual edit/undo back to the saved source at this boundary.
        if (Source() == savedSource) { dirty = false; Title(); return true; }
        TASKDIALOG_BUTTON choices[]{{IDYES, L"Save"}, {IDNO, L"Discard"}, {IDCANCEL, L"Cancel"}};
        TASKDIALOGCONFIG dialog{sizeof(dialog)};
        dialog.hwndParent = window;
        dialog.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT;
        dialog.pszWindowTitle = L"Markdown Editor";
        dialog.pszMainIcon = TD_WARNING_ICON;
        dialog.pszMainInstruction = L"Save changes before continuing?";
        const std::wstring description = path.empty() ? L"Untitled document" : path;
        dialog.pszContent = description.c_str();
        dialog.cButtons = ARRAYSIZE(choices);
        dialog.pButtons = choices;
        dialog.nDefaultButton = IDYES;
        int result = IDCANCEL;
        if (FAILED(TaskDialogIndirect(&dialog, &result, nullptr, nullptr))) {
            result = MessageBoxW(window, L"Save changes before continuing?\n\nYes = Save; No = Discard; Cancel = keep editing.",
                                 L"Markdown Editor", MB_YESNOCANCEL | MB_ICONWARNING);
        }
        return result == IDNO || (result == IDYES && Save());
    }

    bool Open(const std::wstring& filename)
    {
        std::wstring text, error;
        FileFormat openedFormat;
        // Read and validate before replacing anything in the current window.
        if (!ReadDocument(filename, text, openedFormat, error)) {
            Error(L"Could not open:\n" + filename + L"\n\n" + error); return false;
        }
        loading = true;
        SETTEXTEX set{ST_DEFAULT, 1200};
        const auto result = SendMessageW(source, EM_SETTEXTEX, reinterpret_cast<WPARAM>(&set), reinterpret_cast<LPARAM>(text.c_str()));
        loading = false;
        if (!result && !text.empty()) { Error(L"Windows could not load the file into the editor."); return false; }
        SendMessageW(source, EM_EMPTYUNDOBUFFER, 0, 0);
        SendMessageW(source, EM_SETMODIFY, FALSE, 0);
        path = filename;
        savedSource = std::move(text);
        format = std::move(openedFormat);
        dirty = false;
        renderDirty = true;
        Restore(source, {});
        Restore(preview, {});
        Title();
        if (!Mode(false)) Mode(true);
        return true;
    }
};

void ClipboardMenu(HWND control, bool readOnly, LPARAM location)
{
    constexpr UINT Copy = 1, Cut = 2, Paste = 3;
    CHARRANGE selection{};
    SendMessageW(control, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    const bool selected = selection.cpMin != selection.cpMax;
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING | (selected ? MF_ENABLED : MF_GRAYED), Copy, L"&Copy\tCtrl+C");
    if (!readOnly) {
        AppendMenuW(menu, MF_STRING | (selected ? MF_ENABLED : MF_GRAYED), Cut, L"Cu&t\tCtrl+X");
        const bool canPaste = SendMessageW(control, EM_CANPASTE, CF_UNICODETEXT, 0) != 0;
        AppendMenuW(menu, MF_STRING | (canPaste ? MF_ENABLED : MF_GRAYED), Paste, L"&Paste\tCtrl+V");
    }

    POINT point{GET_X_LPARAM(location), GET_Y_LPARAM(location)};
    if (point.x == -1 && point.y == -1) {
        // Shift+F10 / Menu key: anchor near the selection, within the viewport.
        SendMessageW(control, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&point), selection.cpMax);
        RECT client{};
        GetClientRect(control, &client);
        point.x = std::clamp<LONG>(point.x, 0, std::max<LONG>(0, client.right - 1));
        point.y = std::clamp<LONG>(point.y + MulDiv(20, GetDpiForWindow(control), 96),
                                   0, std::max<LONG>(0, client.bottom - 1));
        ClientToScreen(control, &point);
    }
    SetFocus(control);
    if (readOnly) HideCaret(control);
    const UINT command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
        point.x, point.y, GetAncestor(control, GA_ROOT), nullptr);
    DestroyMenu(menu);
    switch (command) {
    case Copy: SendMessageW(control, WM_COPY, 0, 0); break;
    case Cut: SendMessageW(control, WM_CUT, 0, 0); break;
    case Paste: SendMessageW(control, EM_PASTESPECIAL, CF_UNICODETEXT, 0); break;
    }
    if (readOnly) HideCaret(control);
}

LRESULT CALLBACK ControlProc(HWND control, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR data)
{
    auto& app = *reinterpret_cast<App*>(data);
    const bool view = control == app.preview;
    if (message == WM_KEYDOWN) {
        const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (ctrl && wparam == 'A') {
            CHARRANGE all{0, -1};
            SendMessageW(control, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&all));
            if (view) HideCaret(control);
            return 0;
        }
        if (!view && ctrl && (wparam == 'Z' || wparam == 'Y')) {
            SendMessageW(control, wparam == 'Y' || shift ? EM_REDO : EM_UNDO, 0, 0);
            app.dirty = app.Source() != app.savedSource;
            app.Title();
            return 0;
        }
        // Pasting through Rich Edit's text-only path never inserts OLE objects.
        if (!view && ((ctrl && wparam == 'V') || (shift && wparam == VK_INSERT))) {
            SendMessageW(control, EM_PASTESPECIAL, CF_UNICODETEXT, 0);
            return 0;
        }
        if (ctrl && (wparam == 'B' || wparam == 'I' || wparam == 'U' || wparam == 'E' ||
                     wparam == 'L' || wparam == 'R' || wparam == 'J' || wparam == 'K' ||
                     wparam == 'T' || wparam == VK_OEM_PLUS || wparam == VK_OEM_MINUS ||
                     (shift && (wparam == VK_OEM_COMMA || wparam == VK_OEM_PERIOD)))) return 0;
    }
    if (message == WM_CONTEXTMENU) {
        ClipboardMenu(control, view, lparam);
        return 0;
    }
    // Suppress Rich Edit's built-in formatting and zoom shortcuts.
    if (message == WM_MOUSEWHEEL && (GET_KEYSTATE_WPARAM(wparam) & MK_CONTROL)) return 0;
    if (message == WM_CHAR && (wparam == 1 || wparam == 22 || wparam == 25 || wparam == 26)) return 0;
    if (message == WM_CHAR && (GetKeyState(VK_CONTROL) & 0x8000) &&
        (wparam == 2 || wparam == 5 || wparam == 9 || wparam == 10 || wparam == 11 ||
         wparam == 12 || wparam == 18 || wparam == 20 || wparam == 21)) return 0;
    const LRESULT result = DefSubclassProc(control, message, wparam, lparam);
    if (view && message != WM_NCDESTROY && GetFocus() == control) HideCaret(control);
    if (message == WM_NCDESTROY) RemoveWindowSubclass(control, ControlProc, 1);
    return result;
}

HMENU Menu()
{
    HMENU bar = CreateMenu(), file = CreatePopupMenu(), mode = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, OpenId, L"&Open…\tCtrl+O");
    AppendMenuW(file, MF_STRING, SaveId, L"&Save\tCtrl+S");
    AppendMenuW(file, MF_STRING, ExitId, L"E&xit\tAlt+F4");
    AppendMenuW(mode, MF_STRING | MF_CHECKED, EditId, L"&Edit\tCtrl+1");
    AppendMenuW(mode, MF_STRING, ViewId, L"&View\tCtrl+2");
    AppendMenuW(mode, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(mode, MF_STRING, DarkThemeId, L"&Dark theme");
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"&File");
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(mode), L"&Mode");
    return bar;
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        app = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        app->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (!app) return DefWindowProcW(window, message, wparam, lparam);
    switch (message) {
    case WM_CREATE: {
        app->dpi = GetDpiForWindow(window);
        HDC dc = GetDC(window);
        app->richEditBaseDpi = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(window, dc);
        const DWORD common = WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_NOHIDESEL | WS_TABSTOP | ES_NOOLEDRAGDROP;
        app->source = CreateWindowExW(0, MSFTEDIT_CLASS, L"", common | WS_VISIBLE | WS_HSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
            0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(SourceId)), app->instance, nullptr);
        app->preview = CreateWindowExW(0, MSFTEDIT_CLASS, L"", common | ES_READONLY,
            0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(PreviewId)), app->instance, nullptr);
        if (!app->source || !app->preview) return -1;
        SendMessageW(app->source, EM_SETTEXTMODE, TM_PLAINTEXT | TM_MULTILEVELUNDO, 0);
        SendMessageW(app->source, EM_SETUNDOLIMIT, 1000, 0);
        SendMessageW(app->source, EM_SETEVENTMASK, 0, ENM_CHANGE);
        for (HWND child : {app->source, app->preview}) {
            SendMessageW(child, EM_EXLIMITTEXT, 0, TextLimit);
            // Plain-text paste and no rich objects, drag/drop, or embedded links
            // that could fetch content while a file is merely being opened.
            SetWindowSubclass(child, ControlProc, 1, reinterpret_cast<DWORD_PTR>(app));
        }
        CHARFORMAT2W font{};
        font.cbSize = sizeof(font);
        font.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR | CFM_CHARSET;
        font.yHeight = 12 * 20;
        font.crTextColor = app->rendering.palette.text;
        font.bCharSet = DEFAULT_CHARSET;
        wcscpy_s(font.szFaceName, app->customFamily.empty() ? L"Consolas" : app->customFamily.c_str());
        SendMessageW(app->source, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&font));
        SendMessageW(app->preview, EM_AUTOURLDETECT, AURL_ENABLEURL | AURL_ENABLEEMAILADDR, 0);
        SendMessageW(app->preview, EM_SETEVENTMASK, 0, ENM_LINK);
        if (!app->ApplyTheme()) return -1;
        app->Scale(app->dpi);
        return 0;
    }
    case WM_SIZE: app->Layout(); return 0;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
        info->ptMinTrackSize = {MulDiv(280, app->dpi, 96), MulDiv(180, app->dpi, 96)};
        return 0;
    }
    case WM_DPICHANGED: {
        auto sourcePosition = Remember(app->source), viewPosition = Remember(app->preview);
        const RECT r = *reinterpret_cast<RECT*>(lparam);
        app->dpi = HIWORD(wparam);
        SetWindowPos(window, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE);
        app->Scale(app->dpi);
        Restore(app->source, sourcePosition, false);
        Restore(app->preview, viewPosition, false);
        return 0;
    }
    case WM_SETFOCUS: SetFocus(app->editMode ? app->source : app->preview); return 0;
    case WM_COMMAND:
        if (reinterpret_cast<HWND>(lparam) == app->source && HIWORD(wparam) == EN_CHANGE) {
            if (!app->loading) { app->dirty = true; app->renderDirty = true; app->Title(); }
            return 0;
        }
        switch (LOWORD(wparam)) {
        case OpenId: {
            std::wstring filename;
            if (app->ResolveUnsaved() && app->ChooseFile(false, filename)) app->Open(filename);
            return 0;
        }
        case SaveId: app->Save(); return 0;
        case ExitId: SendMessageW(window, WM_CLOSE, 0, 0); return 0;
        case EditId: app->Mode(true); return 0;
        case ViewId: app->Mode(false); return 0;
        case ToggleId: app->Mode(!app->editMode); return 0;
        case DarkThemeId: app->ToggleTheme(); return 0;
        }
        break;
    case WM_NOTIFY: {
        const auto* header = reinterpret_cast<NMHDR*>(lparam);
        if (header->hwndFrom == app->preview && header->code == EN_LINK) {
            const auto* link = reinterpret_cast<ENLINK*>(lparam);
            if (link->msg == WM_LBUTTONUP && (GetKeyState(VK_CONTROL) & 0x8000)) {
                std::wstring url(static_cast<size_t>(link->chrg.cpMax - link->chrg.cpMin) + 1, 0);
                TEXTRANGEW range{link->chrg, url.data()};
                SendMessageW(app->preview, EM_GETTEXTRANGE, 0, reinterpret_cast<LPARAM>(&range));
                url.resize(wcslen(url.c_str()));
                if (url.compare(0, 7, L"http://") == 0 || url.compare(0, 8, L"https://") == 0 || url.compare(0, 7, L"mailto:") == 0)
                    ShellExecuteW(window, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
            return 0;
        }
        break;
    }
    case WM_CLOSE: if (app->ResolveUnsaved()) DestroyWindow(window); return 0;
    case WM_QUERYENDSESSION: return app->ResolveUnsaved() ? TRUE : FALSE;
    case WM_ENDSESSION: if (wparam) DestroyWindow(window); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show)
{
    // The manifest establishes PMv2 before any HWNDs or common dialogs exist.
    HMODULE richEdit = LoadLibraryExW(L"Msftedit.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!richEdit) { MessageBoxW(nullptr, L"Windows Rich Edit could not be loaded.", L"Markdown Editor", MB_OK | MB_ICONERROR); return 1; }
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);
    App app;
    app.instance = instance;
    app.LoadSettings();
    WNDCLASSW cls{};
    cls.lpfnWndProc = WindowProc;
    cls.hInstance = instance;
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cls.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    cls.lpszClassName = WindowClass;
    RegisterClassW(&cls);
    const UINT dpi = GetDpiForSystem();
    HWND window = CreateWindowExW(0, WindowClass, L"Untitled — Markdown Editor", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, MulDiv(900, dpi, 96), MulDiv(700, dpi, 96), nullptr, Menu(), instance, &app);
    if (!window) { MessageBoxW(nullptr, L"The editor window could not be created.", L"Markdown Editor", MB_OK | MB_ICONERROR); return 1; }
    ACCEL keys[]{{FVIRTKEY | FCONTROL, 'O', OpenId}, {FVIRTKEY | FCONTROL, 'S', SaveId},
                 {FVIRTKEY | FCONTROL, '1', EditId}, {FVIRTKEY | FCONTROL, '2', ViewId},
                 {FVIRTKEY | FCONTROL, VK_TAB, ToggleId}};
    HACCEL accelerators = CreateAcceleratorTableW(keys, ARRAYSIZE(keys));
    ShowWindow(window, show);
    UpdateWindow(window);
    int count = 0;
    wchar_t** args = CommandLineToArgvW(GetCommandLineW(), &count);
    if (args && count > 1) app.Open(args[1]);
    LocalFree(args);
    SetFocus(app.editMode ? app.source : app.preview);
    MSG message{};
    int result;
    while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        if (!TranslateAcceleratorW(window, accelerators, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    DestroyAcceleratorTable(accelerators);
    FreeLibrary(richEdit);
    return result < 0 ? 1 : static_cast<int>(message.wParam);
}
