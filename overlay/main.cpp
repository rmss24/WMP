#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <propidl.h>
#include <gdiplus.h>
#include <stdio.h>
#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include "json.hpp"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")

using json = nlohmann::json;

struct Config {
    bool debugMode             = false;
    bool launchSettingsOnStart = true;

    struct {
        bool         enabled  = true;
        std::wstring path     = L"";
        int          maxW     = 260;
        int          maxH     = 260;
        int          margin   = 28;
        std::string  anchorX  = "right";   // "left" | "center" | "right"
        std::string  anchorY  = "bottom";  // "top"  | "center" | "bottom"
        int          offsetX  = 0;
        int          offsetY  = 0;
    } character;

    struct {
        bool enabled        = false;
        int  walkFrequency  = 30;       // seconds between walks
        std::string walkDirection = "both"; // "left" | "right" | "both"
        int  walkSpeed      = 150;      // pixels per second
    } animation;

    struct { bool enabled = true; int len = 18; int gap = 5; int thick = 2; COLORREF color = RGB(0,255,80); } crosshair;
    struct { bool enabled = true; int thick = 3; COLORREF color = RGB(255,50,50); } border;
    struct { bool enabled = true; int fontSize = 16; } hud;
    struct { bool visible = true; bool clickThrough = true; } overlay;
};

static Config g_cfg;
static std::wstring g_configPath;
static FILETIME g_configLastWrite = {};
// charPath (as stored in config) → walk GIF path
static std::map<std::wstring, std::wstring> g_walkPaths;

static const wchar_t CLASS_NAME[]    = L"WMP";
static const COLORREF TRANSPARENT_KEY = RGB(1, 1, 1);
static const UINT_PTR TIMER_GIF          = 10;
static const UINT_PTR TIMER_WATCH        = 11;
static const UINT_PTR TIMER_GIF_WALK     = 12;
static const UINT_PTR TIMER_WALK_MOVE    = 13;
static const UINT_PTR TIMER_WALK_TRIGGER = 14;
static const UINT     WM_RELOAD_CFG      = WM_APP + 1;

static HWND  g_hwnd              = NULL;
static bool  g_clickThrough      = true;
static bool  g_screenOverlayActive = true;
static ULONG_PTR g_gdiplusToken  = 0;

// Static (idle) GIF
static Gdiplus::Image* g_gif          = NULL;
static GUID  g_gifFrameDim            = {};
static UINT  g_gifFrameCount          = 0;
static UINT  g_gifFrameIndex          = 0;
static std::vector<UINT> g_gifDelaysMs;

// Walk GIF
static Gdiplus::Image* g_gifWalk          = NULL;
static GUID  g_gifWalkFrameDim            = {};
static UINT  g_gifWalkFrameCount          = 0;
static UINT  g_gifWalkFrameIndex          = 0;
static std::vector<UINT> g_gifWalkDelaysMs;

// Character position & walk state
static float g_charX        = -1.f;  // <0 = recompute from anchor
static float g_charY        = -1.f;
static float g_anchorX_px   = 0.f;
static float g_anchorY_px   = 0.f;
static int   g_walkPhase    = 0;     // 0=idle, 1=walk_out, 2=walk_back
static int   g_walkDir      = -1;   // +1=right, -1=left
static int   g_walkAlternate = 0;   // tracks direction alternation for "both"

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

static std::string WideToUtf8(const std::wstring& w)
{
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, NULL, 0, NULL, NULL);
    std::string s(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, NULL, NULL);
    return s;
}

static std::wstring FindConfigPath()
{
    wchar_t mod[MAX_PATH] = {};
    GetModuleFileNameW(NULL, mod, MAX_PATH);
    std::wstring moduleDir = DirName(mod);

    wchar_t cwd[MAX_PATH] = {};
    GetCurrentDirectoryW(MAX_PATH, cwd);

    std::vector<std::wstring> candidates = {
        std::wstring(cwd) + L"\\config.json",
        moduleDir         + L"\\config.json",
        moduleDir         + L"\\..\\config.json",
        moduleDir         + L"\\..\\..\\config.json",
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
        auto gets = [&](const json& obj, const char* k, std::string def) -> std::string {
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
            g_cfg.character.anchorX = gets(c, "anchorX", "right");
            g_cfg.character.anchorY = gets(c, "anchorY", "bottom");
            g_cfg.character.offsetX = geti(c, "offsetX", 0);
            g_cfg.character.offsetY = geti(c, "offsetY", 0);
        }

        if (j.contains("walkPaths") && j["walkPaths"].is_object()) {
            g_walkPaths.clear();
            for (auto& [k, v] : j["walkPaths"].items())
                if (v.is_string()) g_walkPaths[Utf8ToWide(k)] = Utf8ToWide(v.get<std::string>());
        }

        if (j.contains("animation") && j["animation"].is_object()) {
            auto& a = j["animation"];
            g_cfg.animation.enabled       = getb(a, "enabled",       false);
            g_cfg.animation.walkFrequency = geti(a, "walkFrequency", 30);
            g_cfg.animation.walkDirection = gets(a, "walkDirection", "both");
            g_cfg.animation.walkSpeed     = geti(a, "walkSpeed",     150);
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
            g_cfg.overlay.visible      = getb(o, "visible",      true);
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
        j["overlay"]["visible"]      = g_screenOverlayActive;
        j["overlay"]["clickThrough"] = g_clickThrough;
        std::ofstream f(g_configPath.c_str());
        if (f.is_open()) f << j.dump(2);
    } catch (...) {}
}

// ── GIF loading ─────────────────────────────────────────────────────────────

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

static std::wstring FindGifWalkPath()
{
    auto it = g_walkPaths.find(g_cfg.character.path);
    if (it == g_walkPaths.end()) return L"";
    const std::wstring& wp = it->second;
    if (wp.empty()) return L"";
    if (FileExists(wp)) return wp;
    std::wstring base = DirName(g_configPath) + L"\\" + wp;
    if (FileExists(base)) return base;
    return L"";
}

static void LoadFrameDelays(Gdiplus::Image* img, UINT frameCount, std::vector<UINT>& delays)
{
    delays.assign(frameCount, 100);
    UINT sz = img->GetPropertyItemSize(PropertyTagFrameDelay);
    if (!sz) return;
    std::vector<BYTE> buf(sz);
    auto* item = (Gdiplus::PropertyItem*)buf.data();
    if (img->GetPropertyItem(PropertyTagFrameDelay, sz, item) != Gdiplus::Ok) return;
    UINT cnt = item->length / sizeof(UINT);
    UINT* raw = (UINT*)item->value;
    for (UINT i = 0; i < frameCount && i < cnt; ++i)
        delays[i] = std::max(20u, raw[i] * 10u);
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
    LoadFrameDelays(g_gif, g_gifFrameCount, g_gifDelaysMs);
}

static void LoadGifWalk()
{
    delete g_gifWalk; g_gifWalk = NULL;
    g_gifWalkFrameCount = 0; g_gifWalkFrameIndex = 0; g_gifWalkDelaysMs.clear();

    std::wstring path = FindGifWalkPath();
    if (path.empty()) return;

    g_gifWalk = Gdiplus::Image::FromFile(path.c_str(), FALSE);
    if (!g_gifWalk || g_gifWalk->GetLastStatus() != Gdiplus::Ok) { delete g_gifWalk; g_gifWalk = NULL; return; }

    UINT dimCount = g_gifWalk->GetFrameDimensionsCount();
    if (!dimCount) return;
    std::vector<GUID> dims(dimCount);
    g_gifWalk->GetFrameDimensionsList(dims.data(), dimCount);
    g_gifWalkFrameDim   = dims[0];
    g_gifWalkFrameCount = g_gifWalk->GetFrameCount(&g_gifWalkFrameDim);
    LoadFrameDelays(g_gifWalk, g_gifWalkFrameCount, g_gifWalkDelaysMs);
}

// ── Position & animation ─────────────────────────────────────────────────────

static bool ComputeCharSize(int& dw, int& dh)
{
    Gdiplus::Image* img = (g_walkPhase > 0 && g_gifWalk) ? g_gifWalk : g_gif;
    if (!img) return false;
    UINT sw = img->GetWidth(), sh = img->GetHeight();
    if (!sw || !sh) return false;
    double scale = std::min((double)g_cfg.character.maxW / sw, (double)g_cfg.character.maxH / sh);
    dw = std::max(1, (int)(sw * scale));
    dh = std::max(1, (int)(sh * scale));
    return true;
}

static void ComputeAnchorPos(int sw, int sh, int dw, int dh)
{
    int m = g_cfg.character.margin;
    if      (g_cfg.character.anchorX == "left")   g_anchorX_px = (float)(m + g_cfg.character.offsetX);
    else if (g_cfg.character.anchorX == "center")  g_anchorX_px = (float)((sw - dw) / 2 + g_cfg.character.offsetX);
    else                                            g_anchorX_px = (float)(sw - dw - m + g_cfg.character.offsetX);

    if      (g_cfg.character.anchorY == "top")    g_anchorY_px = (float)(m + g_cfg.character.offsetY);
    else if (g_cfg.character.anchorY == "center")  g_anchorY_px = (float)((sh - dh) / 2 + g_cfg.character.offsetY);
    else                                            g_anchorY_px = (float)(sh - dh - m + g_cfg.character.offsetY);

    if (g_charX < 0) g_charX = g_anchorX_px;
    if (g_charY < 0) g_charY = g_anchorY_px;
}

static void StopWalk()
{
    g_walkPhase = 0;
    KillTimer(g_hwnd, TIMER_GIF_WALK);
    KillTimer(g_hwnd, TIMER_WALK_MOVE);
    g_gifWalkFrameIndex = 0;
    // Snap back to anchor
    g_charX = g_anchorX_px;
    g_charY = g_anchorY_px;
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void StartWalk()
{
    if (!g_cfg.animation.enabled || !g_hwnd || g_walkPhase != 0) return;

    if      (g_cfg.animation.walkDirection == "left")  g_walkDir = -1;
    else if (g_cfg.animation.walkDirection == "right") g_walkDir = +1;
    else {
        g_walkDir = ((g_walkAlternate & 1) == 0) ? -1 : +1;
        g_walkAlternate++;
    }

    g_walkPhase = 1;
    g_gifWalkFrameIndex = 0;

    if (g_gifWalk && g_gifWalkFrameCount > 1)
        SetTimer(g_hwnd, TIMER_GIF_WALK, g_gifWalkDelaysMs[0], NULL);

    SetTimer(g_hwnd, TIMER_WALK_MOVE, 16, NULL);
}

// ── Drawing ──────────────────────────────────────────────────────────────────

static void DrawGif(HDC hdc, const RECT& rc)
{
    if (!g_cfg.character.enabled) return;

    int dw, dh;
    if (!ComputeCharSize(dw, dh)) return;

    if (g_charX < 0 || g_charY < 0)
        ComputeAnchorPos(rc.right, rc.bottom, dw, dh);

    Gdiplus::Image* img = (g_walkPhase > 0 && g_gifWalk) ? g_gifWalk : g_gif;
    if (!img) return;

    if (g_walkPhase > 0 && g_gifWalk)
        img->SelectActiveFrame(&g_gifWalkFrameDim, g_gifWalkFrameIndex);
    else if (g_gif)
        g_gif->SelectActiveFrame(&g_gifFrameDim, g_gifFrameIndex);

    Gdiplus::Graphics g(hdc);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

    float fx = g_charX, fy = g_charY;
    if (g_walkPhase > 0 && g_walkDir < 0) {
        // Mirror horizontally when walking left
        g.DrawImage(img, Gdiplus::RectF(fx + dw, fy, (float)-dw, (float)dh));
    } else {
        g.DrawImage(img, Gdiplus::RectF(fx, fy, (float)dw, (float)dh));
    }
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

// ── Window proc ──────────────────────────────────────────────────────────────

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
    ShellExecuteW(NULL, L"open", (configDir + L"\\launch-settings.bat").c_str(),
                  NULL, configDir.c_str(), SW_SHOW);
}

static void ApplyConfig()
{
    g_screenOverlayActive = g_cfg.overlay.visible;
    ApplyClickThrough(g_cfg.overlay.clickThrough);
    LoadGif();
    LoadGifWalk();

    // Reset position so anchor gets recomputed
    g_charX = -1.f;
    g_charY = -1.f;

    if (g_hwnd) {
        KillTimer(g_hwnd, TIMER_GIF_WALK);
        KillTimer(g_hwnd, TIMER_WALK_MOVE);
        KillTimer(g_hwnd, TIMER_WALK_TRIGGER);
        g_walkPhase = 0;

        if (g_gif && g_gifFrameCount > 1)
            SetTimer(g_hwnd, TIMER_GIF, g_gifDelaysMs[0], NULL);
        else
            KillTimer(g_hwnd, TIMER_GIF);

        if (g_cfg.animation.enabled && g_cfg.animation.walkFrequency > 0)
            SetTimer(g_hwnd, TIMER_WALK_TRIGGER,
                     (UINT)(g_cfg.animation.walkFrequency * 1000), NULL);

        InvalidateRect(g_hwnd, NULL, FALSE);
    }
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
        // Static GIF frame advance
        if (wParam == TIMER_GIF && g_gif && g_gifFrameCount > 1) {
            g_gifFrameIndex = (g_gifFrameIndex + 1) % g_gifFrameCount;
            SetTimer(hwnd, TIMER_GIF, g_gifDelaysMs[g_gifFrameIndex], NULL);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        // Walk GIF frame advance
        if (wParam == TIMER_GIF_WALK && g_gifWalk && g_gifWalkFrameCount > 1) {
            g_gifWalkFrameIndex = (g_gifWalkFrameIndex + 1) % g_gifWalkFrameCount;
            SetTimer(hwnd, TIMER_GIF_WALK, g_gifWalkDelaysMs[g_gifWalkFrameIndex], NULL);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        // Walk movement (16ms ~60fps)
        if (wParam == TIMER_WALK_MOVE) {
            int dw, dh;
            if (ComputeCharSize(dw, dh)) {
                int sw = GetSystemMetrics(SM_CXSCREEN);
                int m  = g_cfg.character.margin;
                float speed = (float)g_cfg.animation.walkSpeed * 16.f / 1000.f;
                g_charX += (float)g_walkDir * speed;
                float leftEdge  = (float)m;
                float rightEdge = (float)(sw - dw - m);
                if (g_walkPhase == 1) {
                    bool hitEdge = (g_walkDir > 0 && g_charX >= rightEdge) ||
                                   (g_walkDir < 0 && g_charX <= leftEdge);
                    if (hitEdge) {
                        g_charX     = (g_walkDir > 0) ? rightEdge : leftEdge;
                        g_walkDir   = -g_walkDir;
                        g_walkPhase = 2;
                    }
                } else if (g_walkPhase == 2) {
                    bool reachedAnchor =
                        (g_walkDir > 0 && g_charX >= g_anchorX_px) ||
                        (g_walkDir < 0 && g_charX <= g_anchorX_px);
                    if (reachedAnchor) { StopWalk(); break; }
                }
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        // Walk trigger: start a new walk cycle
        if (wParam == TIMER_WALK_TRIGGER && g_walkPhase == 0)
            StartWalk();
        // Keep window on top + check config changes
        if (wParam == TIMER_WATCH) {
            SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            if (!g_configPath.empty()) {
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
        }
        break;

    case WM_RELOAD_CFG:
        LoadConfig();
        ApplyConfig();
        break;

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_GIF);
        KillTimer(hwnd, TIMER_WATCH);
        KillTimer(hwnd, TIMER_GIF_WALK);
        KillTimer(hwnd, TIMER_WALK_MOVE);
        KillTimer(hwnd, TIMER_WALK_TRIGGER);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// ── Entry point ───────────────────────────────────────────────────────────────

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

    WNDCLASSEX wc    = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
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

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }

    delete g_gif;
    delete g_gifWalk;
    Gdiplus::GdiplusShutdown(g_gdiplusToken);
    CloseHandle(mutex);
    return 0;
}
