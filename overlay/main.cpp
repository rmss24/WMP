#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <propidl.h>
#include <gdiplus.h>
#include <stdio.h>
#include <algorithm>
#include <string>
#include <vector>
#include <fstream>
#include "json.hpp"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")

using json = nlohmann::json;

struct Config {
    bool   debugMode            = false;
    bool   launchSettingsOnStart = true;

    struct { bool enabled = true; std::wstring path = L""; int maxW = 260; int maxH = 260; int margin = 28; } character;
    struct { bool enabled = true; int len = 18; int gap = 5; int thick = 2; COLORREF color = RGB(0,255,80); } crosshair;
    struct { bool enabled = true; int thick = 3; COLORREF color = RGB(255,50,50); } border;
    struct { bool enabled = true; int fontSize = 16; } hud;
    struct { bool visible = true; bool clickThrough = true; } overlay;
};

static Config g_cfg;
static std::wstring g_configPath;
static FILETIME g_configLastWrite = {};

static const wchar_t CLASS_NAME[] = L"WMP";
static const COLORREF TRANSPARENT_KEY = RGB(1, 1, 1);
static const UINT_PTR TIMER_GIF    = 10;
static const UINT_PTR TIMER_WATCH  = 11;
static const UINT     WM_RELOAD_CFG = WM_APP + 1;

static HWND  g_hwnd        = NULL;
static bool  g_clickThrough = true;
static bool  g_screenOverlayActive = true;
static ULONG_PTR g_gdiplusToken = 0;
static Gdiplus::Image* g_gif = NULL;
static GUID g_gifFrameDim = {};
static UINT g_gifFrameCount = 0;
static UINT g_gifFrameIndex = 0;
static std::vector<UINT> g_gifDelaysMs;

enum HotkeyID {
    HK_TOGGLE_VISIBLE      = 1,
    HK_TOGGLE_CLICKTHROUGH = 2,
    HK_EXIT                = 3,
    HK_OPEN_SETTINGS       = 4,
};

static bool FileExists(const std::wstring& path)
{
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static std::wstring DirName(const std::wstring& path)
{
    size_t s = path.find_last_of(L"\\/");
    return s == std::wstring::npos ? L"." : path.substr(0, s);
}

static COLORREF HexToColorref(const std::string& hex)
{
    if (hex.size() < 7) return RGB(255, 255, 255);
    try {
        int r = std::stoi(hex.substr(1, 2), nullptr, 16);
        int g = std::stoi(hex.substr(3, 2), nullptr, 16);
        int b = std::stoi(hex.substr(5, 2), nullptr, 16);
        return RGB(r, g, b);
    } catch (...) { return RGB(255, 255, 255); }
}

static std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
    std::wstring w(n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}

static std::wstring FindConfigPath()
{
    wchar_t mod[MAX_PATH] = {};
    GetModuleFileNameW(NULL, mod, MAX_PATH);
    std::wstring moduleDir = DirName(mod);

    wchar_t cwd[MAX_PATH] = {};
    GetCurrentDirectoryW(MAX_PATH, cwd);

    std::vector<std::wstring> candidates = {
        std::wstring(cwd)    + L"\\config.json",
        moduleDir            + L"\\config.json",
        moduleDir            + L"\\..\\config.json",
        moduleDir            + L"\\..\\..\\config.json",
    };
    for (const auto& c : candidates)
        if (FileExists(c)) return c;
    return moduleDir + L"\\config.json";
}

static void ApplyConfig();

static void LoadConfig()
{
    if (g_configPath.empty()) g_configPath = FindConfigPath();

    std::ifstream f(g_configPath.c_str());
    if (!f.is_open()) return;

    try {
        json j; f >> j;

        auto getb = [&](const json& obj, const char* k, bool def) {
            return obj.contains(k) && obj[k].is_boolean() ? obj[k].get<bool>() : def;
        };
        auto geti = [&](const json& obj, const char* k, int def) {
            return obj.contains(k) && obj[k].is_number() ? obj[k].get<int>() : def;
        };
        auto gets = [&](const json& obj, const char* k, std::string def) {
            return obj.contains(k) && obj[k].is_string() ? obj[k].get<std::string>() : def;
        };

        g_cfg.debugMode             = getb(j, "debugMode", false);
        g_cfg.launchSettingsOnStart = getb(j, "launchSettingsOnStart", true);

        if (j.contains("character") && j["character"].is_object()) {
            auto& c = j["character"];
            g_cfg.character.enabled = getb(c, "enabled", true);
            g_cfg.character.path    = Utf8ToWide(gets(c, "path", ""));
            g_cfg.character.maxW    = geti(c, "maxWidth",  260);
            g_cfg.character.maxH    = geti(c, "maxHeight", 260);
            g_cfg.character.margin  = geti(c, "margin",    28);
        }
        if (j.contains("crosshair") && j["crosshair"].is_object()) {
            auto& c = j["crosshair"];
            g_cfg.crosshair.enabled = getb(c, "enabled",   true);
            g_cfg.crosshair.len     = geti(c, "length",    18);
            g_cfg.crosshair.gap     = geti(c, "gap",        5);
            g_cfg.crosshair.thick   = geti(c, "thickness",  2);
            g_cfg.crosshair.color   = HexToColorref(gets(c, "color", "#00FF50"));
        }
        if (j.contains("border") && j["border"].is_object()) {
            auto& b = j["border"];
            g_cfg.border.enabled = getb(b, "enabled",   true);
            g_cfg.border.thick   = geti(b, "thickness",  3);
            g_cfg.border.color   = HexToColorref(gets(b, "color", "#FF3232"));
        }
        if (j.contains("hud") && j["hud"].is_object()) {
            auto& h = j["hud"];
            g_cfg.hud.enabled  = getb(h, "enabled",  true);
            g_cfg.hud.fontSize = geti(h, "fontSize", 16);
        }
        if (j.contains("overlay") && j["overlay"].is_object()) {
            auto& o = j["overlay"];
            g_cfg.overlay.visible     = getb(o, "visible",     true);
            g_cfg.overlay.clickThrough = getb(o, "clickThrough", true);
        }
    } catch (...) {}
}

static void SaveOverlayState()
{
    if (g_configPath.empty()) return;
    try {
        json j;
        {
            std::ifstream f(g_configPath.c_str());
            if (f.is_open()) f >> j;
        }
        j["overlay"]["visible"]     = g_screenOverlayActive;
        j["overlay"]["clickThrough"] = g_clickThrough;
        std::ofstream f(g_configPath.c_str());
        if (f.is_open()) f << j.dump(2);
    } catch (...) {}
}

static std::wstring FindGifPath()
{
    if (!g_cfg.character.path.empty()) {
        if (FileExists(g_cfg.character.path)) return g_cfg.character.path;
        std::wstring base = DirName(g_configPath) + L"\\" + g_cfg.character.path;
        if (FileExists(base)) return base;
    }
    wchar_t mod[MAX_PATH] = {}; GetModuleFileNameW(NULL, mod, MAX_PATH);
    std::wstring moduleDir = DirName(mod);
    const wchar_t* rel = L"characters\\frieren\\frieren.gif";
    for (const auto& base : { DirName(g_configPath), moduleDir, moduleDir + L"\\.." }) {
        std::wstring p = base + L"\\" + rel;
        if (FileExists(p)) return p;
    }
    return L"";
}

static void LoadGif()
{
    delete g_gif; g_gif = NULL;
    g_gifFrameCount = 0; g_gifFrameIndex = 0; g_gifDelaysMs.clear();
    if (!g_cfg.character.enabled) return;

    std::wstring path = FindGifPath();
    if (path.empty()) return;

    g_gif = Gdiplus::Image::FromFile(path.c_str(), FALSE);
    if (!g_gif || g_gif->GetLastStatus() != Gdiplus::Ok) { delete g_gif; g_gif = NULL; return; }

    UINT dimCount = g_gif->GetFrameDimensionsCount();
    if (!dimCount) return;
    std::vector<GUID> dims(dimCount);
    g_gif->GetFrameDimensionsList(dims.data(), dimCount);
    g_gifFrameDim   = dims[0];
    g_gifFrameCount = g_gif->GetFrameCount(&g_gifFrameDim);
    g_gifDelaysMs.assign(g_gifFrameCount, 100);

    UINT delaySize = g_gif->GetPropertyItemSize(PropertyTagFrameDelay);
    if (delaySize > 0) {
        std::vector<BYTE> buf(delaySize);
        Gdiplus::PropertyItem* item = (Gdiplus::PropertyItem*)buf.data();
        if (g_gif->GetPropertyItem(PropertyTagFrameDelay, delaySize, item) == Gdiplus::Ok) {
            UINT cnt = item->length / sizeof(UINT);
            UINT* delays = (UINT*)item->value;
            for (UINT i = 0; i < g_gifFrameCount && i < cnt; ++i)
                g_gifDelaysMs[i] = std::max(20u, delays[i] * 10u);
        }
    }
}

static void ApplyClickThrough(bool enable)
{
    LONG_PTR ex = GetWindowLongPtr(g_hwnd, GWL_EXSTYLE);
    if (enable) ex |=  WS_EX_TRANSPARENT;
    else        ex &= ~WS_EX_TRANSPARENT;
    SetWindowLongPtr(g_hwnd, GWL_EXSTYLE, ex);
    SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    g_clickThrough = enable;
}

static void ApplyConfig()
{
    g_screenOverlayActive = g_cfg.overlay.visible;
    ApplyClickThrough(g_cfg.overlay.clickThrough);
    LoadGif();
    if (g_hwnd) {
        if (g_gif && g_gifFrameCount > 1)
            SetTimer(g_hwnd, TIMER_GIF, g_gifDelaysMs[0], NULL);
        else
            KillTimer(g_hwnd, TIMER_GIF);
        InvalidateRect(g_hwnd, NULL, FALSE);
    }
}

static void LaunchSettingsUI()
{
    std::wstring configDir = DirName(g_configPath);

    std::vector<std::wstring> candidates = {
        configDir + L"\\settings-ui.exe",
        configDir + L"\\launch-settings.bat",
    };
    for (const auto& p : candidates) {
        if (FileExists(p)) {
            ShellExecuteW(NULL, L"open", p.c_str(), NULL, configDir.c_str(), SW_SHOW);
            return;
        }
    }
    std::wstring bat = configDir + L"\\launch-settings.bat";
    ShellExecuteW(NULL, L"open", bat.c_str(), NULL, configDir.c_str(), SW_SHOW);
}

static void DrawGif(HDC hdc, const RECT& rc)
{
    if (!g_gif || !g_cfg.character.enabled) return;
    Gdiplus::Graphics g(hdc);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    UINT sw = g_gif->GetWidth(), sh = g_gif->GetHeight();
    if (!sw || !sh) return;
    double scale = std::min((double)g_cfg.character.maxW / sw, (double)g_cfg.character.maxH / sh);
    int dw = std::max(1, (int)(sw * scale));
    int dh = std::max(1, (int)(sh * scale));
    int x  = rc.right  - dw - g_cfg.character.margin;
    int y  = rc.bottom - dh - g_cfg.character.margin;
    g_gif->SelectActiveFrame(&g_gifFrameDim, g_gifFrameIndex);
    g.DrawImage(g_gif, x, y, dw, dh);
}

static void DrawCrosshair(HDC hdc, int cx, int cy)
{
    const auto& ch = g_cfg.crosshair;
    HPEN p1 = CreatePen(PS_SOLID, ch.thick + 2, RGB(0, 40, 0));
    HPEN old = (HPEN)SelectObject(hdc, p1);
    MoveToEx(hdc, cx - ch.len - ch.gap, cy, NULL); LineTo(hdc, cx - ch.gap, cy);
    MoveToEx(hdc, cx + ch.gap,          cy, NULL); LineTo(hdc, cx + ch.len + ch.gap, cy);
    MoveToEx(hdc, cx, cy - ch.len - ch.gap, NULL); LineTo(hdc, cx, cy - ch.gap);
    MoveToEx(hdc, cx, cy + ch.gap,          NULL); LineTo(hdc, cx, cy + ch.len + ch.gap);
    SelectObject(hdc, old); DeleteObject(p1);
    HPEN p2 = CreatePen(PS_SOLID, ch.thick, ch.color);
    old = (HPEN)SelectObject(hdc, p2);
    MoveToEx(hdc, cx - ch.len - ch.gap, cy, NULL); LineTo(hdc, cx - ch.gap, cy);
    MoveToEx(hdc, cx + ch.gap,          cy, NULL); LineTo(hdc, cx + ch.len + ch.gap, cy);
    MoveToEx(hdc, cx, cy - ch.len - ch.gap, NULL); LineTo(hdc, cx, cy - ch.gap);
    MoveToEx(hdc, cx, cy + ch.gap,          NULL); LineTo(hdc, cx, cy + ch.len + ch.gap);
    SelectObject(hdc, old); DeleteObject(p2);
}

static void DrawBorder(HDC hdc, RECT* rc)
{
    const auto& bd = g_cfg.border;
    HPEN pen = CreatePen(PS_SOLID, bd.thick, bd.color);
    HPEN old = (HPEN)SelectObject(hdc, pen);
    HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH);
    HBRUSH ob = (HBRUSH)SelectObject(hdc, nb);
    Rectangle(hdc, rc->left, rc->top, rc->right, rc->bottom);

    HBRUSH accent = CreateSolidBrush(bd.color);
    SelectObject(hdc, accent);
    int t = bd.thick, c = 20;
    RECT corners[] = {
        {0, 0, c, t}, {0, 0, t, c},
        {rc->right-c, 0, rc->right, t}, {rc->right-t, 0, rc->right, c},
        {0, rc->bottom-t, c, rc->bottom}, {0, rc->bottom-c, t, rc->bottom},
        {rc->right-c, rc->bottom-t, rc->right, rc->bottom}, {rc->right-t, rc->bottom-c, rc->right, rc->bottom},
    };
    for (auto& cr : corners) FillRect(hdc, &cr, accent);

    SelectObject(hdc, ob); SelectObject(hdc, old);
    DeleteObject(pen); DeleteObject(accent);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);

        HDC     mdc = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HBITMAP old = (HBITMAP)SelectObject(mdc, bmp);

        HBRUSH tb = CreateSolidBrush(TRANSPARENT_KEY);
        FillRect(mdc, &rc, tb); DeleteObject(tb);

        if (g_screenOverlayActive) {
            int cx = rc.right / 2, cy = rc.bottom / 2;
            if (g_cfg.crosshair.enabled) DrawCrosshair(mdc, cx, cy);
            if (g_cfg.border.enabled)    DrawBorder(mdc, &rc);
        }

        if (g_screenOverlayActive && g_cfg.hud.enabled) {
            SetBkMode(mdc, TRANSPARENT);
            HFONT font = CreateFontW(g_cfg.hud.fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
            HFONT oldf = (HFONT)SelectObject(mdc, font);
            SetTextColor(mdc, RGB(200, 200, 200));
            wchar_t buf[512];
            if (g_cfg.debugMode) {
                swprintf_s(buf,
                    L"  [DEBUG] CT:%s  OVR:%s  CFG:%s",
                    g_clickThrough ? L"ON" : L"OFF",
                    g_screenOverlayActive ? L"ON" : L"OFF",
                    g_configPath.c_str());
            } else {
                swprintf_s(buf,
                    L"  WMP  |  Ctrl+Shift+T: Click-Through [%s]"
                    L"  |  Ctrl+Shift+H: Screen [%s]"
                    L"  |  Ctrl+Shift+S: Settings"
                    L"  |  Ctrl+Shift+Q: Quit",
                    g_clickThrough ? L"ON" : L"OFF",
                    g_screenOverlayActive ? L"ON" : L"OFF");
            }
            TextOutW(mdc, 6, 6, buf, (int)wcslen(buf));
            SelectObject(mdc, oldf); DeleteObject(font);
        }

        DrawGif(mdc, rc);

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, mdc, 0, 0, SRCCOPY);
        SelectObject(mdc, old); DeleteObject(bmp); DeleteDC(mdc);
        EndPaint(hwnd, &ps);
        break;
    }

    case WM_HOTKEY:
        switch ((int)wParam) {
        case HK_TOGGLE_VISIBLE:
            g_screenOverlayActive = !g_screenOverlayActive;
            InvalidateRect(hwnd, NULL, FALSE);
            SaveOverlayState();
            break;
        case HK_TOGGLE_CLICKTHROUGH:
            ApplyClickThrough(!g_clickThrough);
            InvalidateRect(hwnd, NULL, FALSE);
            SaveOverlayState();
            break;
        case HK_OPEN_SETTINGS:
            LaunchSettingsUI();
            break;
        case HK_EXIT:
            PostQuitMessage(0);
            break;
        }
        break;

    case WM_TIMER:
        if (wParam == TIMER_GIF && g_gif && g_gifFrameCount > 1) {
            g_gifFrameIndex = (g_gifFrameIndex + 1) % g_gifFrameCount;
            SetTimer(hwnd, TIMER_GIF, g_gifDelaysMs[g_gifFrameIndex], NULL);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        if (wParam == TIMER_WATCH) {
            SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        if (wParam == TIMER_WATCH && !g_configPath.empty()) {
            HANDLE h = CreateFileW(g_configPath.c_str(), GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
            if (h != INVALID_HANDLE_VALUE) {
                FILETIME ft = {};
                GetFileTime(h, NULL, NULL, &ft);
                CloseHandle(h);
                if (CompareFileTime(&ft, &g_configLastWrite) != 0) {
                    g_configLastWrite = ft;
                    LoadConfig();
                    ApplyConfig();
                }
            }
        }
        break;

    case WM_RELOAD_CFG:
        LoadConfig();
        ApplyConfig();
        break;

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_GIF);
        KillTimer(hwnd, TIMER_WATCH);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}


int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR lpCmdLine, int)
{
    bool noUI = false;
    if (lpCmdLine) {
        std::wstring cmd(lpCmdLine);
        noUI = cmd.find(L"--no-ui") != std::wstring::npos;
    }

    Gdiplus::GdiplusStartupInput gsi;
    if (Gdiplus::GdiplusStartup(&g_gdiplusToken, &gsi, NULL) != Gdiplus::Ok) return 0;

    HANDLE mutex = CreateMutexW(NULL, TRUE, L"WMP_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        return 0;
    }

    LoadConfig();

    WNDCLASSEX wc   = {};
    wc.cbSize       = sizeof(wc);
    wc.lpfnWndProc  = WndProc;
    wc.hInstance    = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassExW(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    g_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        CLASS_NAME, L"Overlay", WS_POPUP,
        0, 0, sw, sh, NULL, NULL, hInst, NULL);

    SetLayeredWindowAttributes(g_hwnd, TRANSPARENT_KEY, 0, LWA_COLORKEY);

    ApplyConfig();

    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(g_hwnd);
    SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    SetTimer(g_hwnd, TIMER_WATCH, 1000, NULL);

    RegisterHotKey(g_hwnd, HK_TOGGLE_VISIBLE,      MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'H');
    RegisterHotKey(g_hwnd, HK_TOGGLE_CLICKTHROUGH, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'T');
    RegisterHotKey(g_hwnd, HK_EXIT,                MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'Q');
    RegisterHotKey(g_hwnd, HK_OPEN_SETTINGS,       MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'S');

    if (!noUI && g_cfg.launchSettingsOnStart)
        LaunchSettingsUI();

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterHotKey(g_hwnd, HK_TOGGLE_VISIBLE);
    UnregisterHotKey(g_hwnd, HK_TOGGLE_CLICKTHROUGH);
    UnregisterHotKey(g_hwnd, HK_EXIT);
    UnregisterHotKey(g_hwnd, HK_OPEN_SETTINGS);
    delete g_gif; g_gif = NULL;
    CloseHandle(mutex);
    Gdiplus::GdiplusShutdown(g_gdiplusToken);
    return (int)msg.wParam;
}
