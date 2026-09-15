
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <algorithm>
#include <cstdint>
#include <vector>
#include <string>
#include <shlobj.h>
#include "resource.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")

constexpr UINT WMAPP_TRAY = WM_APP + 1;

constexpr UINT IDM_TOGGLE = 1001;
constexpr UINT IDM_OPACITY_UP = 1002;
constexpr UINT IDM_OPACITY_DOWN = 1003;
constexpr UINT IDM_EXIT = 1004;
constexpr UINT IDM_INTENSITY = 1005;
constexpr UINT IDM_INTENSITY_SLIDER = 1006;
constexpr UINT IDM_STARTUP = 1007;
static bool g_menuTracking = false;

constexpr int INTENSITY_POPUP_W = 260;
constexpr int INTENSITY_POPUP_H = 86;

static HWND g_intensityPopup = nullptr;
static HWND g_intensitySlider = nullptr;
static HWND g_intensityLabel = nullptr;

constexpr UINT IDM_TINT_NONE = 1100;
constexpr UINT IDM_TINT_FOXED_AMBER = 1101;
constexpr UINT IDM_TINT_BOOKCLOTH = 1102;
constexpr UINT IDM_TINT_RECYCLED_KRAFT = 1103;
constexpr UINT IDM_TINT_PLUM_KOZO = 1104;
constexpr UINT IDM_TINT_ROSE_QUARTZ = 1105;
constexpr UINT IDM_TINT_SAGE_PRESS = 1106;
constexpr UINT IDM_TINT_NORDIC_SKY = 1107;
constexpr UINT IDM_TINT_SOFT_WAVE = 1201;
constexpr UINT IDM_TINT_RICE_PAPER = 1202;
constexpr UINT IDM_TINT_LAID_COTTON = 1203;
constexpr UINT IDM_TINT_NEWSPRINT = 1204;
constexpr UINT IDM_TINT_COLD_PRESS = 1205;
constexpr UINT IDM_TINT_ARTIST_CANVAS = 1206;
constexpr UINT IDM_TINT_FELT_SIDE = 1207;
constexpr UINT IDM_TINT_FROST_GLASSINE = 1208;

static NOTIFYICONDATAW g_nid = {};
static HWND g_hwnd = nullptr;

static HBITMAP g_bitmap = nullptr;
static HDC g_memDC = nullptr;
static void* g_pixels = nullptr;

static int g_x = 0;
static int g_y = 0;
static int g_width = 0;
static int g_height = 0;

static BYTE g_alpha = 34;
static bool g_enabled = true;

// COLORREF is used only as the selected tint definition.
// The actual tint is deliberately mixed very weakly into the paper texture.
static COLORREF g_tint = RGB(255, 255, 255);
static bool g_hasTint = false;

struct Pixel {
    BYTE b, g, r, a;
};

struct TintMenuItem {
    UINT id;
    const wchar_t* name;
    COLORREF color;
};

static const TintMenuItem kTints[] = {
    { IDM_TINT_NONE,          L"No tint",         RGB(255, 255, 255) },
    { IDM_TINT_FOXED_AMBER,   L"Foxed Amber",     RGB(215, 204, 182) },
    { IDM_TINT_BOOKCLOTH,     L"Bookcloth",       RGB(207, 193, 171) },
    { IDM_TINT_RECYCLED_KRAFT,L"Recycled Kraft",  RGB(196, 180, 153) },
    { IDM_TINT_PLUM_KOZO,     L"Plum Kozo",       RGB(204, 194, 201) },
    { IDM_TINT_ROSE_QUARTZ,   L"Rose Quartz",     RGB(215, 199, 198) },
    { IDM_TINT_SAGE_PRESS,    L"Sage Press",      RGB(199, 205, 193) },
    { IDM_TINT_NORDIC_SKY,    L"Nordic Sky",      RGB(196, 204, 208) },

    // Paper palette. Values are approximate RGB samples from the supplied
    // reference image; the actual screen effect remains deliberately subtle.
    { IDM_TINT_SOFT_WAVE,      L"Soft Wave",       RGB(235, 233, 229) },
    { IDM_TINT_RICE_PAPER,     L"Rice Paper",      RGB(241, 239, 232) },
    { IDM_TINT_LAID_COTTON,    L"Laid Cotton",     RGB(232, 229, 223) },
    { IDM_TINT_NEWSPRINT,      L"Newsprint",       RGB(228, 226, 218) },
    { IDM_TINT_COLD_PRESS,     L"Cold Press",      RGB(237, 234, 227) },
    { IDM_TINT_ARTIST_CANVAS,  L"Artist Canvas",   RGB(234, 231, 222) },
    { IDM_TINT_FELT_SIDE,      L"Felt Side",       RGB(226, 223, 215) },
    { IDM_TINT_FROST_GLASSINE, L"Frost Glassine",  RGB(237, 235, 230) }
};

static uint32_t Hash(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static void SafeDeleteLayerBitmap()
{
    if (g_memDC) {
        DeleteDC(g_memDC);
        g_memDC = nullptr;
    }

    if (g_bitmap) {
        DeleteObject(g_bitmap);
        g_bitmap = nullptr;
    }

    g_pixels = nullptr;
}

static bool CreateLayerBitmap()
{
    SafeDeleteLayerBitmap();

    g_x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    g_y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    g_width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    g_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    if (g_width <= 0 || g_height <= 0)
        return false;

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = g_width;
    bi.bmiHeader.biHeight = -g_height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    HDC screenDC = GetDC(nullptr);
    g_memDC = CreateCompatibleDC(screenDC);

    if (!g_memDC) {
        ReleaseDC(nullptr, screenDC);
        return false;
    }

    g_bitmap = CreateDIBSection(
        screenDC,
        &bi,
        DIB_RGB_COLORS,
        &g_pixels,
        nullptr,
        0);

    ReleaseDC(nullptr, screenDC);

    if (!g_bitmap || !g_pixels) {
        SafeDeleteLayerBitmap();
        return false;
    }

    SelectObject(g_memDC, g_bitmap);
    return true;
}

static BYTE BlendChannel(BYTE gray, BYTE tint, float amount)
{
    const float value =
        static_cast<float>(gray) * (1.0f - amount) +
        static_cast<float>(tint) * amount;

    return static_cast<BYTE>(
        std::clamp(static_cast<int>(value + 0.5f), 0, 255));
}

static void BuildLayer()
{
    if (!g_pixels || g_width <= 0 || g_height <= 0)
        return;

    auto* pixels = static_cast<Pixel*>(g_pixels);
    constexpr int TILE = 1024;

    const BYTE tintR = GetRValue(g_tint);
    const BYTE tintG = GetGValue(g_tint);
    const BYTE tintB = GetBValue(g_tint);

    // This controls how much the selected pastel affects the paper grain.
    // Kept deliberately low so the tint remains subtle.
    constexpr float TINT_MIX = 0.22f;

    for (int y = 0; y < g_height; ++y) {
        for (int x = 0; x < g_width; ++x) {
            Pixel& p = pixels[y * g_width + x];

            if (!g_enabled || g_alpha == 0) {
                p = { 0, 0, 0, 0 };
                continue;
            }

            const int tx = x & (TILE - 1);
            const int ty = y & (TILE - 1);

            const uint32_t h =
                Hash(static_cast<uint32_t>(tx) * 374761393u ^
                     static_cast<uint32_t>(ty) * 668265263u);

            const float noise =
                static_cast<float>(h & 0xffffu) / 65535.0f - 0.5f;

            float value = 0.50f + noise * 0.10f;
            value = std::clamp(value, 0.25f, 0.75f);

            const BYTE gray =
                static_cast<BYTE>(value * 255.0f);

            BYTE r = gray;
            BYTE g = gray;
            BYTE b = gray;

            if (g_hasTint) {
                r = BlendChannel(gray, tintR, TINT_MIX);
                g = BlendChannel(gray, tintG, TINT_MIX);
                b = BlendChannel(gray, tintB, TINT_MIX);
            }

            // Premultiplied alpha required by UpdateLayeredWindow.
            const BYTE a = g_alpha;

            p.r = static_cast<BYTE>(
                (static_cast<unsigned int>(r) * a + 127) / 255);
            p.g = static_cast<BYTE>(
                (static_cast<unsigned int>(g) * a + 127) / 255);
            p.b = static_cast<BYTE>(
                (static_cast<unsigned int>(b) * a + 127) / 255);
            p.a = a;
        }
    }
}

static bool PresentLayer()
{
    if (!g_memDC)
        return false;

    BuildLayer();

    HDC screenDC = GetDC(nullptr);

    POINT dst = { g_x, g_y };
    SIZE size = { g_width, g_height };
    POINT src = { 0, 0 };

    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    const BOOL ok = UpdateLayeredWindow(
        g_hwnd,
        screenDC,
        &dst,
        &size,
        g_memDC,
        &src,
        0,
        &blend,
        ULW_ALPHA);

    ReleaseDC(nullptr, screenDC);
    return ok != FALSE;
}

static void PositionWindow()
{
    SetWindowPos(
        g_hwnd,
        HWND_TOPMOST,
        g_x,
        g_y,
        g_width,
        g_height,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

static void Refresh()
{
    if (!CreateLayerBitmap())
        return;

    PositionWindow();
    PresentLayer();
}

static int AlphaToPercent();
static void PercentToAlpha(int percent);

static std::wstring GetSettingsPath()
{
    PWSTR appData = nullptr;
    std::wstring path;

    if (SUCCEEDED(SHGetKnownFolderPath(
            FOLDERID_RoamingAppData, 0, nullptr, &appData)) && appData) {
        path = appData;
        CoTaskMemFree(appData);

        path += L"\\GrainLayer";
        CreateDirectoryW(path.c_str(), nullptr);
        path += L"\\settings.ini";
    }

    return path;
}

static UINT GetSelectedTintId()
{
    if (!g_hasTint)
        return IDM_TINT_NONE;

    for (const auto& tint : kTints) {
        if (tint.color == g_tint)
            return tint.id;
    }

    return IDM_TINT_NONE;
}

static void SaveSettings()
{
    const std::wstring path = GetSettingsPath();
    if (path.empty())
        return;

    wchar_t buffer[32] = {};
    swprintf_s(buffer, L"%d", AlphaToPercent());
    WritePrivateProfileStringW(L"GrainLayer", L"Intensity", buffer, path.c_str());

    WritePrivateProfileStringW(
        L"GrainLayer", L"Enabled", g_enabled ? L"1" : L"0", path.c_str());

    swprintf_s(buffer, L"%u", GetSelectedTintId());
    WritePrivateProfileStringW(L"GrainLayer", L"Tint", buffer, path.c_str());
}

static void LoadSettings()
{
    const std::wstring path = GetSettingsPath();
    if (path.empty())
        return;

    wchar_t buffer[32] = {};

    GetPrivateProfileStringW(
        L"GrainLayer", L"Intensity", L"25", buffer, _countof(buffer), path.c_str());
    PercentToAlpha(_wtoi(buffer));

    GetPrivateProfileStringW(
        L"GrainLayer", L"Enabled", L"1", buffer, _countof(buffer), path.c_str());
    g_enabled = (_wtoi(buffer) != 0);

    GetPrivateProfileStringW(
        L"GrainLayer", L"Tint", L"1100", buffer, _countof(buffer), path.c_str());
    const UINT tintId = static_cast<UINT>(_wtoi(buffer));

    if (tintId == IDM_TINT_NONE) {
        g_tint = RGB(255, 255, 255);
        g_hasTint = false;
    } else {
        const TintMenuItem* tint = nullptr;
        for (const auto& item : kTints) {
            if (item.id == tintId) {
                tint = &item;
                break;
            }
        }

        if (tint) {
            g_tint = tint->color;
            g_hasTint = true;
        }
    }
}

static bool IsStartupEnabled()
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0,
            KEY_QUERY_VALUE,
            &key) != ERROR_SUCCESS)
        return false;

    wchar_t value[1024] = {};
    DWORD type = 0;
    DWORD size = sizeof(value);
    const LONG result = RegQueryValueExW(
        key, L"GrainLayer", nullptr, &type,
        reinterpret_cast<LPBYTE>(value), &size);
    RegCloseKey(key);

    return result == ERROR_SUCCESS &&
           (type == REG_SZ || type == REG_EXPAND_SZ) &&
           value[0] != L'\0';
}

static bool SetStartupEnabled(bool enabled)
{
    HKEY key = nullptr;
    const LONG openResult = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        KEY_SET_VALUE,
        &key);

    if (openResult != ERROR_SUCCESS)
        return false;

    LONG result = ERROR_SUCCESS;

    if (enabled) {
        wchar_t exePath[MAX_PATH] = {};
        if (GetModuleFileNameW(nullptr, exePath, _countof(exePath)) == 0) {
            result = GetLastError();
        } else {
            std::wstring command = L"\"";
            command += exePath;
            command += L"\"";

            result = RegSetValueExW(
                key,
                L"GrainLayer",
                0,
                REG_SZ,
                reinterpret_cast<const BYTE*>(command.c_str()),
                static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
        }
    } else {
        result = RegDeleteValueW(key, L"GrainLayer");
        if (result == ERROR_FILE_NOT_FOUND)
            result = ERROR_SUCCESS;
    }

    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

static void ToggleOverlay()
{
    g_enabled = !g_enabled;
    PresentLayer();
    SaveSettings();
}

static void ChangeOpacity(int delta)
{
    g_alpha = static_cast<BYTE>(
        std::clamp(static_cast<int>(g_alpha) + delta, 0, 100));
    PresentLayer();
    SaveSettings();
}

static void SetTint(COLORREF color, bool enabled)
{
    g_tint = color;
    g_hasTint = enabled;
    PresentLayer();
    SaveSettings();
}

static void AddTrayIcon()
{
    ZeroMemory(&g_nid, sizeof(g_nid));

    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = WMAPP_TRAY;
    g_nid.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON));

    wcscpy_s(g_nid.szTip, L"GrainLayer");

    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

static void RemoveTrayIcon()
{
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

static const TintMenuItem* FindTint(UINT id)
{
    for (const auto& tint : kTints) {
        if (tint.id == id)
            return &tint;
    }
    return nullptr;
}

static void AppendTintItem(HMENU menu, const TintMenuItem& tint)
{
    MENUITEMINFOW mii = {};
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_ID | MIIM_FTYPE | MIIM_DATA;
    mii.wID = tint.id;
    mii.fType = MFT_OWNERDRAW;
    mii.dwItemData =
        reinterpret_cast<ULONG_PTR>(&tint);

    InsertMenuItemW(
        menu,
        GetMenuItemCount(menu),
        TRUE,
        &mii);
}



static int AlphaToPercent()
{
    return (static_cast<int>(g_alpha) * 100 + 127) / 255;
}

static void PercentToAlpha(int percent)
{
    percent = std::clamp(percent, 0, 100);
    g_alpha = static_cast<BYTE>((percent * 255 + 50) / 100);
}

static void SetIntensityFromCursor(HMENU menu)
{
    if (!menu)
        return;

    RECT itemRect = {};
    if (!GetMenuItemRect(g_hwnd, menu, IDM_INTENSITY_SLIDER, &itemRect))
        return;

    POINT pt = {};
    GetCursorPos(&pt);

    const int left = itemRect.left + 28;
    const int right = itemRect.right - 18;

    if (right <= left)
        return;

    const int mouseX = static_cast<int>(pt.x);
    const int clampedX =
        mouseX < left ? left :
        (mouseX > right ? right : mouseX);

    const int percent =
        ((clampedX - left) * 100 + (right - left) / 2) /
        (right - left);

    const BYTE oldAlpha = g_alpha;
    PercentToAlpha(percent);

    if (oldAlpha != g_alpha)
        PresentLayer();
}

static void AppendIntensitySlider(HMENU menu)
{
    MENUITEMINFOW mii = {};
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_ID | MIIM_FTYPE | MIIM_DATA;
    mii.wID = IDM_INTENSITY_SLIDER;
    mii.fType = MFT_OWNERDRAW;
    mii.dwItemData = 0;
    InsertMenuItemW(
        menu,
        GetMenuItemCount(menu),
        TRUE,
        &mii);
}

static void DrawIntensitySlider(const DRAWITEMSTRUCT* dis)
{
    HDC dc = dis->hDC;
    RECT rc = dis->rcItem;

    const COLORREF bg = GetSysColor(COLOR_MENU);
    const COLORREF text = GetSysColor(COLOR_MENUTEXT);
    const COLORREF face = GetSysColor(COLOR_3DFACE);
    const COLORREF shadow = GetSysColor(COLOR_3DSHADOW);

    HBRUSH bgBrush = CreateSolidBrush(bg);
    FillRect(dc, &rc, bgBrush);
    DeleteObject(bgBrush);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, text);

    RECT label = rc;
    label.left += 8;
    label.top += 3;
    label.right -= 8;
    label.bottom = label.top + 18;

    wchar_t textBuffer[64] = {};
    swprintf_s(
        textBuffer,
        L"Intensity                         %d%%",
        AlphaToPercent());

    DrawTextW(
        dc,
        textBuffer,
        -1,
        &label,
        DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_NOPREFIX);

    const int trackLeft = rc.left + 12;
    const int trackRight = rc.right - 12;
    const int trackY = rc.bottom - 14;
    const int trackHeight = 4;

    RECT track = {
        trackLeft,
        trackY - trackHeight / 2,
        trackRight,
        trackY + (trackHeight + 1) / 2
    };

    HBRUSH trackBrush = CreateSolidBrush(shadow);
    FillRect(dc, &track, trackBrush);
    DeleteObject(trackBrush);

    const int percent = AlphaToPercent();
    const int knobX =
        trackLeft +
        ((trackRight - trackLeft) * percent) / 100;

    RECT filled = track;
    filled.right = knobX;

    HBRUSH fillBrush = CreateSolidBrush(
        GetSysColor(COLOR_HIGHLIGHT));
    FillRect(dc, &filled, fillBrush);
    DeleteObject(fillBrush);

    const int radius = 7;
    RECT knob = {
        knobX - radius,
        trackY - radius,
        knobX + radius + 1,
        trackY + radius + 1
    };

    HBRUSH knobBrush = CreateSolidBrush(face);
    FillRect(dc, &knob, knobBrush);
    DeleteObject(knobBrush);

    FrameRect(
        dc,
        &knob,
        static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
}

static void MeasureIntensitySlider(MEASUREITEMSTRUCT* mis)
{
    mis->itemHeight = 48;
    mis->itemWidth = 250;
}

static HMENU g_activeMenu = nullptr;
static HHOOK g_menuHook = nullptr;
static bool g_sliderDragging = false;

static bool GetIntensityItemRect(HMENU menu, RECT* itemRect)
{
    if (!menu || !itemRect)
        return false;

    const int count = GetMenuItemCount(menu);

    for (int position = 0; position < count; ++position) {
        MENUITEMINFOW mii = {};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_ID;

        if (GetMenuItemInfoW(
                menu,
                static_cast<UINT>(position),
                TRUE,
                &mii) &&
            mii.wID == IDM_INTENSITY_SLIDER) {

            return GetMenuItemRect(
                g_hwnd,
                menu,
                static_cast<UINT>(position),
                itemRect) != FALSE;
        }
    }

    return false;
}

static HWND FindPopupMenuWindow()
{
    // Standard Win32 popup menus are implemented by the internal #32768
    // window class. This is used only to force an immediate repaint of the
    // owner-drawn slider while TrackPopupMenu is running.
    return FindWindowExW(
        nullptr,
        nullptr,
        L"#32768",
        nullptr);
}

static void RefreshPopupMenu()
{
    HWND menuWindow = FindPopupMenuWindow();
    if (menuWindow) {
        InvalidateRect(menuWindow, nullptr, FALSE);
        UpdateWindow(menuWindow);
    }
}

static void SetIntensityFromPoint(HMENU menu, POINT pt)
{
    if (!menu)
        return;

    RECT itemRect = {};
    if (!GetIntensityItemRect(menu, &itemRect))
        return;

    const int left = itemRect.left + 12;
    const int right = itemRect.right - 12;

    if (right <= left)
        return;

    const int mouseX = static_cast<int>(pt.x);

    const int clampedX =
        mouseX < left ? left :
        (mouseX > right ? right : mouseX);

    const int percent =
        ((clampedX - left) * 100 + (right - left) / 2) /
        (right - left);

    const BYTE oldAlpha = g_alpha;
    PercentToAlpha(percent);

    if (oldAlpha != g_alpha) {
        PresentLayer();
        RefreshPopupMenu();
        SaveSettings();
    }
}

static bool IsPointOverSlider(HMENU menu, POINT pt)
{
    if (!menu)
        return false;

    RECT itemRect = {};
    if (!GetIntensityItemRect(menu, &itemRect))
        return false;

    return PtInRect(&itemRect, pt) != FALSE;
}

static LRESULT CALLBACK MenuMessageFilter(
    int code,
    WPARAM wParam,
    LPARAM lParam)
{
    if (code == MSGF_MENU && g_activeMenu) {
        const MSG* msg =
            reinterpret_cast<const MSG*>(lParam);

        if (msg) {
            switch (msg->message) {
            case WM_LBUTTONDOWN:
                if (IsPointOverSlider(g_activeMenu, msg->pt)) {
                    g_sliderDragging = true;

                    // Keep receiving mouse movement even when the pointer
                    // leaves the slider while the button is held.
                    HWND menuWindow = FindPopupMenuWindow();
                    if (menuWindow)
                        SetCapture(menuWindow);

                    SetIntensityFromPoint(g_activeMenu, msg->pt);

                    // Consume the click so the menu stays open.
                    return 1;
                }
                break;

            case WM_MOUSEMOVE:
                // A plain mouseover must not change the intensity.
                // Only a left-button drag updates the slider.
                if (g_sliderDragging) {
                    SetIntensityFromPoint(g_activeMenu, msg->pt);
                    return 1;
                }
                break;

            case WM_LBUTTONUP:
                if (g_sliderDragging) {
                    SetIntensityFromPoint(g_activeMenu, msg->pt);
                    g_sliderDragging = false;

                    if (GetCapture() == FindPopupMenuWindow())
                        ReleaseCapture();

                    // Consume the release so the menu stays open.
                    return 1;
                }
                break;
            }
        }
    }

    return CallNextHookEx(g_menuHook, code, wParam, lParam);
}

static void ShowTrayMenu()
{
    HMENU menu = CreatePopupMenu();
    if (!menu)
        return;

    AppendMenuW(
        menu,
        MF_STRING,
        IDM_TOGGLE,
        g_enabled ? L"Toggle overlay (F6)" : L"Toggle overlay (F6)");

    AppendMenuW(
        menu,
        MF_STRING,
        IDM_OPACITY_UP,
        L"Increase intensity (F7)");

    AppendMenuW(
        menu,
        MF_STRING,
        IDM_OPACITY_DOWN,
        L"Decrease intensity (F8)");

    AppendIntensitySlider(menu);

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(menu, MF_STRING, IDM_TINT_NONE, L"No tint");

    HMENU warmMenu = CreatePopupMenu();
    if (warmMenu) {
        AppendTintItem(warmMenu, kTints[1]);
        AppendTintItem(warmMenu, kTints[2]);
        AppendTintItem(warmMenu, kTints[3]);
        AppendTintItem(warmMenu, kTints[4]);
        AppendTintItem(warmMenu, kTints[5]);
        AppendTintItem(warmMenu, kTints[6]);
        AppendTintItem(warmMenu, kTints[7]);

        AppendMenuW(
            menu,
            MF_POPUP,
            reinterpret_cast<UINT_PTR>(warmMenu),
            L"Warm & tinted");
    }

    HMENU paperMenu = CreatePopupMenu();
    if (paperMenu) {
        AppendTintItem(paperMenu, kTints[8]);
        AppendTintItem(paperMenu, kTints[9]);
        AppendTintItem(paperMenu, kTints[10]);
        AppendTintItem(paperMenu, kTints[11]);
        AppendTintItem(paperMenu, kTints[12]);
        AppendTintItem(paperMenu, kTints[13]);
        AppendTintItem(paperMenu, kTints[14]);
        AppendTintItem(paperMenu, kTints[15]);

        AppendMenuW(
            menu,
            MF_POPUP,
            reinterpret_cast<UINT_PTR>(paperMenu),
            L"Paper");
    }

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(
        menu,
        MF_STRING | (IsStartupEnabled() ? MF_CHECKED : 0),
        IDM_STARTUP,
        L"Open at startup");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(
        menu,
        MF_STRING,
        IDM_EXIT,
        L"Exit (Ctrl+Alt+Shift+Q)");

    POINT pt = {};
    GetCursorPos(&pt);

    SetForegroundWindow(g_hwnd);

    g_activeMenu = menu;
    g_menuTracking = true;
    g_sliderDragging = false;

    g_menuHook = SetWindowsHookExW(
        WH_MSGFILTER,
        MenuMessageFilter,
        nullptr,
        GetCurrentThreadId());

    TrackPopupMenu(
        menu,
        TPM_RIGHTBUTTON |
        TPM_BOTTOMALIGN |
        TPM_LEFTALIGN,
        pt.x,
        pt.y,
        0,
        g_hwnd,
        nullptr);

    if (g_menuHook) {
        UnhookWindowsHookEx(g_menuHook);
        g_menuHook = nullptr;
    }

    g_sliderDragging = false;
    g_menuTracking = false;
    g_activeMenu = nullptr;

    DestroyMenu(menu);
}

static void DrawTintSwatch(
    HDC dc,
    const RECT& rc,
    COLORREF color)
{
    RECT swatch = rc;

    swatch.left += 8;
    swatch.top += 4;
    swatch.right = swatch.left + 16;
    swatch.bottom = swatch.top + 16;

    HBRUSH fill = CreateSolidBrush(color);
    FillRect(dc, &swatch, fill);
    DeleteObject(fill);

    FrameRect(
        dc,
        &swatch,
        static_cast<HBRUSH>(
            GetStockObject(GRAY_BRUSH)));
}

static void DrawTintMenuItem(const DRAWITEMSTRUCT* dis)
{
    const auto* tint =
        reinterpret_cast<const TintMenuItem*>(
            dis->itemData);

    if (!tint)
        return;

    HDC dc = dis->hDC;
    RECT rc = dis->rcItem;

    const bool selected =
        (dis->itemState & ODS_SELECTED) != 0;

    const COLORREF background =
        selected
            ? GetSysColor(COLOR_HIGHLIGHT)
            : GetSysColor(COLOR_MENU);

    const COLORREF text =
        selected
            ? GetSysColor(COLOR_HIGHLIGHTTEXT)
            : GetSysColor(COLOR_MENUTEXT);

    HBRUSH bg = CreateSolidBrush(background);
    FillRect(dc, &rc, bg);
    DeleteObject(bg);

    DrawTintSwatch(dc, rc, tint->color);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, text);

    RECT textRc = rc;
    textRc.left += 34;
    textRc.right -= 8;

    DrawTextW(
        dc,
        tint->name,
        -1,
        &textRc,
        DT_SINGLELINE |
        DT_VCENTER |
        DT_LEFT |
        DT_NOPREFIX);
}

static void MeasureTintMenuItem(MEASUREITEMSTRUCT* mis)
{
    const auto* tint =
        reinterpret_cast<const TintMenuItem*>(
            mis->itemData);

    if (!tint)
        return;

    mis->itemHeight = 24;

    HDC dc = GetDC(g_hwnd);
    HFONT font = static_cast<HFONT>(
        GetStockObject(DEFAULT_GUI_FONT));

    HFONT old = static_cast<HFONT>(
        SelectObject(dc, font));

    SIZE size = {};
    GetTextExtentPoint32W(
        dc,
        tint->name,
        lstrlenW(tint->name),
        &size);

    SelectObject(dc, old);
    ReleaseDC(g_hwnd, dc);

    mis->itemWidth =
        static_cast<UINT>(size.cx + 50);
}

static LRESULT CALLBACK WndProc(
    HWND h,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (msg) {
    case WMAPP_TRAY:
        if (lParam == WM_RBUTTONUP)
            ShowTrayMenu();
        else if (lParam == WM_LBUTTONDBLCLK)
            ToggleOverlay();
        return 0;

    case WM_MEASUREITEM:
        if (wParam == 0) {
            const auto* mis =
                reinterpret_cast<const MEASUREITEMSTRUCT*>(lParam);

            if (mis && mis->CtlType == ODT_MENU) {
                auto* mutableMis =
                    const_cast<MEASUREITEMSTRUCT*>(mis);

                if (mis->itemID == IDM_INTENSITY_SLIDER) {
                    MeasureIntensitySlider(mutableMis);
                    return TRUE;
                }

                if (FindTint(mis->itemID)) {
                    MeasureTintMenuItem(mutableMis);
                    return TRUE;
                }
            }
        }
        break;

    case WM_DRAWITEM:
        if (wParam == 0) {
            const auto* dis =
                reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);

            if (dis && dis->CtlType == ODT_MENU) {
                if (dis->itemID == IDM_INTENSITY_SLIDER) {
                    DrawIntensitySlider(dis);
                    return TRUE;
                }

                if (FindTint(dis->itemID)) {
                    DrawTintMenuItem(dis);
                    return TRUE;
                }
            }
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_TOGGLE:
            ToggleOverlay();
            return 0;

        case IDM_OPACITY_UP:
            ChangeOpacity(+5);
            return 0;

        case IDM_OPACITY_DOWN:
            ChangeOpacity(-5);
            return 0;

        case IDM_STARTUP:
            SetStartupEnabled(!IsStartupEnabled());
            return 0;

        case IDM_EXIT:
            DestroyWindow(h);
            return 0;

        case IDM_TINT_NONE:
            SetTint(RGB(255, 255, 255), false);
            return 0;

        default:
        {
            const TintMenuItem* tint =
                FindTint(LOWORD(wParam));

            if (tint) {
                SetTint(tint->color, true);
                return 0;
            }
        }
        break;
        }
        break;

    case WM_NCHITTEST:
        return HTTRANSPARENT;

    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;

    case WM_DISPLAYCHANGE:
        Refresh();
        return 0;

    case WM_HOTKEY:
        switch (wParam) {
        case 1:
            ToggleOverlay();
            break;

        case 2:
            ChangeOpacity(+5);
            break;

        case 3:
            ChangeOpacity(-5);
            break;

        case 4:
            DestroyWindow(h);
            break;
        }
        return 0;

    case WM_DESTROY:
        SaveSettings();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(h, msg, wParam, lParam);
}

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int)
{
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = L"GrainLayerOverlay";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hIconSm = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));

    if (!RegisterClassExW(&wc))
        return 1;

    g_x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    g_y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    g_width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    g_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    const DWORD exStyle =
        WS_EX_LAYERED |
        WS_EX_TRANSPARENT |
        WS_EX_NOACTIVATE |
        WS_EX_TOOLWINDOW |
        WS_EX_TOPMOST;

    g_hwnd = CreateWindowExW(
        exStyle,
        wc.lpszClassName,
        L"GrainLayer",
        WS_POPUP,
        g_x,
        g_y,
        g_width,
        g_height,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!g_hwnd)
        return 2;

    LoadSettings();

    if (!CreateLayerBitmap())
        return 3;

    AddTrayIcon();

    RegisterHotKey(
        g_hwnd, 1, MOD_NOREPEAT, VK_F6);

    RegisterHotKey(
        g_hwnd, 2, MOD_NOREPEAT, VK_F7);

    RegisterHotKey(
        g_hwnd, 3, MOD_NOREPEAT, VK_F8);

    RegisterHotKey(
        g_hwnd,
        4,
        MOD_CONTROL |
        MOD_ALT |
        MOD_SHIFT |
        MOD_NOREPEAT,
        'Q');

    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
    PositionWindow();
    PresentLayer();

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnregisterHotKey(g_hwnd, 1);
    UnregisterHotKey(g_hwnd, 2);
    UnregisterHotKey(g_hwnd, 3);
    UnregisterHotKey(g_hwnd, 4);

    RemoveTrayIcon();
    SafeDeleteLayerBitmap();

    return 0;
}
