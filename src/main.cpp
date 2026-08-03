// AdhanBox for Windows — v0.1 هيكل شغّال: مواقيت + عدّاد + أذان تلقائي + تراي
// C++ نيتف خالص (بدون .NET) · التشغيل بـMCI · الإعدادات الكاملة في الجولة الجاية
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <shlobj.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <string>
#include <ctime>
#include "../engine/athan_times.h"
#include "../engine/cities_gen.h"
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")

static const wchar_t* APP_NAME = L"AdhanBox";
static const UINT WM_TRAY = WM_APP + 1;
enum { IDT_TICK = 1, IDM_OPEN = 100, IDM_STOPSND, IDM_ABOUT, IDM_EXIT,
       IDM_MUTE1H, IDM_MUTEDAY, IDM_UNMUTE,
       IDB_TEST = 200, IDB_STOP, IDB_LANG, IDB_ABOUT, IDB_SETTINGS };

// الموقع وطريقة الحساب — من الإعدادات (الافتراضي: الجيزة)
static int  g_cityIdx = 0;
static char g_method[24] = "Egypt";
static int  g_asr = 1;
static int  g_globalTune = 0;   // ترحيل عام بيتضاف لكل الصلوات (فرق المدينة القريبة)
struct PrayCfg { bool on; wchar_t sound[160]; int vol; int tune; int dur; };  // dur=0 يعني الأذان كامل
static PrayCfg g_pc[athan::COUNT];   // خانة الشروق متسابة فاضية

// فرق التوقيت من ساعة الويندوز نفسها (بيراعي الصيفي تلقائيًا) —
// افتراضنا إن المستخدم مختار مدينته اللي هو عايش فيها
static double localTzHours() {
    TIME_ZONE_INFORMATION tzi;
    DWORD r = GetTimeZoneInformation(&tzi);
    LONG bias = tzi.Bias + (r == TIME_ZONE_ID_DAYLIGHT ? tzi.DaylightBias : tzi.StandardBias);
    return -bias / 60.0;
}
static athan::PrayerTimes makePT() {
    const athan::City& c = athan::CITIES[g_cityIdx];
    return athan::PrayerTimes(c.lat, c.lng, localTzHours(), g_method, g_asr);
}
static athan::PrayerTimes g_pt(30.0131, 31.2089, 3.0, "Egypt", 1);
static athan::Times g_times;
static int g_day = -1;                 // يوم-السنة اللي اتحسبت له المواقيت
static int g_firedMin = -1;            // آخر دقيقة اتأذّن فيها (منع التكرار)
static bool g_playing = false;
static int  g_playIdx = -1;            // مين اللي شغّال دلوقتي: رقم الصلاة، أو -2 لزرار التجربة
static bool g_preview = false;         // الصوت الشغّال تجربة من الإعدادات (مش أذان حقيقي)
static time_t g_stopAt = 0;    // وقت الإيقاف التلقائي (مدة التشغيل) — 0 يعني كامل
static bool g_autoShown = false;       // النافذة ظهرت لوحدها وقت الأذان → تتخفي لوحدها لما يخلص
static NOTIFYICONDATAW g_nid = {};
static HFONT g_fBig, g_fMid, g_fSmall, g_fLink, g_fClock;
static RECT g_rSite = {}, g_rGit = {};    // مناطق الضغط على اللينكات
static HWND g_hwnd = nullptr;

// شريط العنوان الغامق (ويندوز 10 1809+) — يطابق ثيم البرنامج
static void darkTitleBar(HWND h) {
    BOOL on = TRUE;
    if (FAILED(DwmSetWindowAttribute(h, 20, &on, sizeof(on))))
        DwmSetWindowAttribute(h, 19, &on, sizeof(on));
}

static void openUrl(const wchar_t* u) {
    ShellExecuteW(nullptr, L"open", u, nullptr, nullptr, SW_SHOWNORMAL);
}

static bool g_ar = true;        // اللغة — محفوظة في الريجستري
static bool g_adhanOn = true;   // الأذان التلقائي
static bool g_preNotify = true; // تنبيه قبل الأذان بـ10 دقايق
static bool g_autoStart = false;
static bool g_showAtAdhan = true; // تظهر الشاشة وقت الأذان وتتخفي لما يخلص
static time_t g_muteUntil = 0;  // كتم مؤقت
static int g_notifiedMin = -1;  // منع تكرار التنبيه
static HWND g_hSet = nullptr;   // نافذة الإعدادات
static const wchar_t* AR_NAMES[athan::COUNT] =
    {L"الفجر", L"الشروق", L"الظهر", L"العصر", L"المغرب", L"العشاء"};
static const wchar_t* EN_NAMES[athan::COUNT] =
    {L"Fajr", L"Sunrise", L"Dhuhr", L"Asr", L"Maghrib", L"Isha"};
static const wchar_t* pname(int i){ return g_ar ? AR_NAMES[i] : EN_NAMES[i]; }

// نصوص الواجهة [عربي، إنجليزي]
struct Str { const wchar_t *ar, *en; const wchar_t* get() const { return g_ar ? ar : en; } };
static const Str S_TITLE   = {L"🕌 AdhanBox — الجيزة، مصر", L"🕌 AdhanBox — Giza, Egypt"};
static const Str S_DONE    = {L"خلصت صلوات النهارده — الفجر بكره إن شاء الله",
                              L"All prayers done today — Fajr tomorrow, insha'Allah"};
static const Str S_PLAYING = {L"🔊 الأذان شغّال دلوقتي", L"🔊 Adhan is playing"};
static const Str S_IDLE    = {L"الأذان هيشتغل لوحده في وقت كل صلاة",
                              L"Adhan plays automatically at each prayer time"};
static const Str S_TEST    = {L"▶ تجربة الأذان", L"▶ Test adhan"};
static const Str S_STOP    = {L"■ إيقاف الصوت", L"■ Stop sound"};
static const Str S_GIT     = {L"⭐ الكود مجاني — نسألكم الدعاء", L"⭐ Free — please keep us in your du’a"};
static const Str S_SITE    = {L"🌐 magicweb.win", L"🌐 magicweb.win"};
static const Str S_OPEN    = {L"فتح AdhanBox", L"Open AdhanBox"};
static const Str S_MSTOP   = {L"إيقاف الصوت", L"Stop sound"};
static const Str S_ABOUT   = {L"عن البرنامج", L"About"};
static const Str S_EXIT    = {L"خروج", L"Exit"};
static const Str S_TIP     = {L"AdhanBox — الأذان في وقته", L"AdhanBox — adhan on time"};

static void saveLang() {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\MagicWeb\\AdhanBox", 0, nullptr,
                        0, KEY_WRITE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
        DWORD v = g_ar ? 1 : 0;
        RegSetValueExW(k, L"lang_ar", 0, REG_DWORD, (BYTE*)&v, 4);
        RegCloseKey(k);
    }
}
static void loadLang() {
    HKEY k; DWORD v = 1, n = 4;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\MagicWeb\\AdhanBox", 0, KEY_READ, &k) == ERROR_SUCCESS) {
        RegQueryValueExW(k, L"lang_ar", nullptr, nullptr, (BYTE*)&v, &n);
        RegCloseKey(k);
    }
    g_ar = (v != 0);
}

static DWORD regGet(const wchar_t* name, DWORD defv) {
    HKEY k; DWORD v = defv, n = 4;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\MagicWeb\\AdhanBox", 0, KEY_READ, &k) == ERROR_SUCCESS) {
        RegQueryValueExW(k, name, nullptr, nullptr, (BYTE*)&v, &n);
        RegCloseKey(k);
    }
    return v;
}
static void regSet(const wchar_t* name, DWORD v) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\MagicWeb\\AdhanBox", 0, nullptr,
                        0, KEY_WRITE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(k, name, 0, REG_DWORD, (BYTE*)&v, 4);
        RegCloseKey(k);
    }
}
static void applyAutoStart() {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr,
            0, KEY_WRITE, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    if (g_autoStart) {
        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        // /tray = يقوم مصغّر في شريط المهام من غير ما يفتح شاشة
        std::wstring v = L"\"" + std::wstring(exe) + L"\" /tray";
        RegSetValueExW(k, L"AdhanBox", 0, REG_SZ, (const BYTE*)v.c_str(),
                       (DWORD)((v.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(k, L"AdhanBox");
    }
    RegCloseKey(k);
}
static void loadSettings() {
    g_adhanOn     = regGet(L"adhan_on", 1) != 0;
    g_preNotify   = regGet(L"pre_notify", 1) != 0;
    g_autoStart   = regGet(L"auto_start", 0) != 0;
    g_showAtAdhan = regGet(L"show_at_adhan", 1) != 0;
}
static void saveSettings() {
    regSet(L"adhan_on", g_adhanOn);
    regSet(L"pre_notify", g_preNotify);
    regSet(L"auto_start", g_autoStart);
    regSet(L"show_at_adhan", g_showAtAdhan);
    applyAutoStart();
}

static void regSetStr(const wchar_t* name, const std::wstring& v) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\MagicWeb\\AdhanBox", 0, nullptr,
                        0, KEY_WRITE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(k, name, 0, REG_SZ, (const BYTE*)v.c_str(),
                       (DWORD)((v.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(k);
    }
}
static std::wstring regGetStr(const wchar_t* name, const wchar_t* defv) {
    HKEY k; wchar_t buf[256] = L""; DWORD n = sizeof(buf);
    std::wstring out = defv;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\MagicWeb\\AdhanBox", 0, KEY_READ, &k) == ERROR_SUCCESS) {
        if (RegQueryValueExW(k, name, nullptr, nullptr, (BYTE*)buf, &n) == ERROR_SUCCESS) out = buf;
        RegCloseKey(k);
    }
    return out;
}

static const int PRAYS[5] = {athan::FAJR, athan::DHUHR, athan::ASR, athan::MAGHRIB, athan::ISHA};

static void loadSettings2() {
    // المدينة
    std::wstring city = regGetStr(L"city", L"مصر|الجيزة");
    g_cityIdx = 0;
    for (int i = 0; i < athan::CITIES_N; i++) {
        std::wstring key = std::wstring(athan::CITIES[i].countryAr) + L"|" + athan::CITIES[i].cityAr;
        if (key == city) { g_cityIdx = i; break; }
    }
    std::wstring m = regGetStr(L"method", L"Egypt");
    char mb[24] = {}; wcstombs_s(nullptr, mb, m.c_str(), 23);
    strcpy_s(g_method, mb);
    g_asr = (int)regGet(L"asr", 1) == 2 ? 2 : 1;
    g_globalTune = (int)regGet(L"global_tune", 60) - 60;
    if (g_globalTune < -120) g_globalTune = -120;
    if (g_globalTune >  120) g_globalTune =  120;
    for (int x = 0; x < 5; x++) {
        int i = PRAYS[x];
        wchar_t nm[32];
        swprintf(nm, 32, L"pr%d_on", i);   g_pc[i].on = regGet(nm, 1) != 0;
        swprintf(nm, 32, L"pr%d_vol", i);  g_pc[i].vol = (int)regGet(nm, 90);
        swprintf(nm, 32, L"pr%d_tune", i); g_pc[i].tune = (int)regGet(nm, 60) - 60;
        swprintf(nm, 32, L"pr%d_dur", i);  g_pc[i].dur = (int)regGet(nm, 0);
        swprintf(nm, 32, L"pr%d_snd", i);
        std::wstring sn = regGetStr(nm, L"makkah-adhan.mp3");
        wcscpy_s(g_pc[i].sound, sn.c_str());
    }
}
static void saveSettings2() {
    const athan::City& c = athan::CITIES[g_cityIdx];
    regSetStr(L"city", std::wstring(c.countryAr) + L"|" + c.cityAr);
    wchar_t wm[24]; mbstowcs_s(nullptr, wm, g_method, 23);
    regSetStr(L"method", wm);
    regSet(L"asr", g_asr);
    regSet(L"global_tune", g_globalTune + 60);
    for (int x = 0; x < 5; x++) {
        int i = PRAYS[x];
        wchar_t nm[32];
        swprintf(nm, 32, L"pr%d_on", i);   regSet(nm, g_pc[i].on);
        swprintf(nm, 32, L"pr%d_vol", i);  regSet(nm, g_pc[i].vol);
        swprintf(nm, 32, L"pr%d_tune", i); regSet(nm, g_pc[i].tune + 60);
        swprintf(nm, 32, L"pr%d_dur", i);  regSet(nm, g_pc[i].dur);
        swprintf(nm, 32, L"pr%d_snd", i);  regSetStr(nm, g_pc[i].sound);
    }
}

// الدقيقة بعد «الترحيل العام» + «تعديل (دقيقة)» بتاع الصلاة نفسها
static int prayMin(int i) {
    int m = g_times.minuteOfDay(i);
    if (m < 0) return m;
    if (i == athan::SUNRISE) return ((m + g_globalTune) % 1440 + 1440) % 1440;
    return ((m + g_globalTune + g_pc[i].tune) % 1440 + 1440) % 1440;
}
static std::wstring fmtMin(int m) {
    if (m < 0) return L"--:--";
    wchar_t b[8];
    swprintf(b, 8, L"%02d:%02d", m / 60, m % 60);
    return b;
}

static void balloon(const wchar_t* title, const wchar_t* text) {
    g_nid.uFlags = NIF_INFO;
    wcscpy_s(g_nid.szInfoTitle, title);
    wcscpy_s(g_nid.szInfo, text);
    g_nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
}

static HWND g_hAbout = nullptr;
static RECT g_aSite = {}, g_aMail = {};
static const int ABT_CLOSE = 500;

static LRESULT CALLBACK aboutProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE:
        CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                      176, 284, 148, 34, h, (HMENU)(INT_PTR)ABT_CLOSE, nullptr, nullptr);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_DRAWITEM: {
        const DRAWITEMSTRUCT* di = (const DRAWITEMSTRUCT*)l;
        HBRUSH wb = CreateSolidBrush(RGB(11, 17, 28));
        FillRect(di->hDC, &di->rcItem, wb);
        DeleteObject(wb);
        bool down = (di->itemState & ODS_SELECTED) != 0;
        HBRUSH b = CreateSolidBrush(down ? RGB(35, 52, 80) : RGB(30, 44, 68));
        HPEN pn = CreatePen(PS_SOLID, 1, RGB(80, 104, 140));
        HGDIOBJ ob = SelectObject(di->hDC, b), op = SelectObject(di->hDC, pn);
        RoundRect(di->hDC, di->rcItem.left, di->rcItem.top, di->rcItem.right, di->rcItem.bottom, 10, 10);
        SelectObject(di->hDC, ob); SelectObject(di->hDC, op);
        DeleteObject(b); DeleteObject(pn);
        SetBkMode(di->hDC, TRANSPARENT);
        SetTextColor(di->hDC, RGB(200, 217, 240));
        SelectObject(di->hDC, g_fMid);
        RECT r = di->rcItem;
        DrawTextW(di->hDC, g_ar ? L"\u0625\u063a\u0644\u0627\u0642" : L"Close", -1, &r,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return TRUE;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc0 = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        HDC dc = CreateCompatibleDC(dc0);
        HBITMAP bmp = CreateCompatibleBitmap(dc0, rc.right, rc.bottom);
        HGDIOBJ obmp = SelectObject(dc, bmp);
        HBRUSH bg = CreateSolidBrush(RGB(11, 17, 28));
        FillRect(dc, &rc, bg);
        DeleteObject(bg);
        SetBkMode(dc, TRANSPARENT);

        // أيقونة البرنامج فوق
        HICON ic = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1),
                                     IMAGE_ICON, 48, 48, 0);
        if (ic) { DrawIconEx(dc, (rc.right - 48) / 2, 18, ic, 48, 48, 0, nullptr, DI_NORMAL); DestroyIcon(ic); }

        SelectObject(dc, g_fBig);
        SetTextColor(dc, RGB(233, 239, 248));
        RECT r = {0, 74, rc.right, 106};
        DrawTextW(dc, L"AdhanBox 0.4", -1, &r, DT_CENTER | DT_NOPREFIX);

        SelectObject(dc, g_fSmall);
        SetTextColor(dc, RGB(147, 164, 191));
        r = {24, 110, rc.right - 24, 132};
        DrawTextW(dc, g_ar ? L"\u0645\u0648\u0627\u0642\u064a\u062a \u0627\u0644\u0635\u0644\u0627\u0629 \u0648\u0627\u0644\u0623\u0630\u0627\u0646 \u2014 \u062d\u0633\u0627\u0628 \u0641\u0644\u0643\u064a \u0645\u062d\u0644\u064a \u0628\u062f\u0648\u0646 \u0625\u0646\u062a\u0631\u0646\u062a"
                           : L"Prayer times and adhan \u2014 fully offline calculation",
                  -1, &r, DT_CENTER | DT_NOPREFIX | (g_ar ? DT_RTLREADING : 0));
        SetTextColor(dc, RGB(95, 224, 160));
        r = {24, 134, rc.right - 24, 156};
        DrawTextW(dc, g_ar ? L"\u0627\u0644\u0643\u0648\u062f \u0645\u062c\u0627\u0646\u064a \u0644\u0648\u062c\u0647 \u0627\u0644\u0644\u0647 (\u0631\u062e\u0635\u0629 MIT) \u2014 \u0648\u0646\u0633\u0623\u0644\u0643\u0645 \u0627\u0644\u062f\u0639\u0627\u0621"
                           : L"Free and open source (MIT) \u2014 please keep us in your du\u2019a",
                  -1, &r, DT_CENTER | DT_NOPREFIX | (g_ar ? DT_RTLREADING : 0));

        // فاصل
        HPEN sep = CreatePen(PS_SOLID, 1, RGB(38, 51, 74));
        HGDIOBJ osp = SelectObject(dc, sep);
        MoveToEx(dc, 60, 170, nullptr);
        LineTo(dc, rc.right - 60, 170);
        SelectObject(dc, osp);
        DeleteObject(sep);

        // MagicWeb في سطر لوحدها
        SelectObject(dc, g_fMid);
        SetTextColor(dc, RGB(233, 239, 248));
        r = {0, 182, rc.right, 208};
        DrawTextW(dc, L"MagicWeb", -1, &r, DT_CENTER | DT_NOPREFIX);

        // اللينكات (قابلة للضغط)
        SelectObject(dc, g_fLink);
        SetTextColor(dc, RGB(96, 165, 250));
        SIZE s1, s2;
        const wchar_t* site = L"magicweb.win";
        const wchar_t* mail = L"AdhanBox@magicweb.win";
        GetTextExtentPoint32W(dc, site, (int)wcslen(site), &s1);
        GetTextExtentPoint32W(dc, mail, (int)wcslen(mail), &s2);
        int sx = (rc.right - s1.cx) / 2, sy = 212;
        TextOutW(dc, sx, sy, site, (int)wcslen(site));
        g_aSite = {sx, sy, sx + s1.cx, sy + s1.cy};
        int mx = (rc.right - s2.cx) / 2, my = 236;
        TextOutW(dc, mx, my, mail, (int)wcslen(mail));
        g_aMail = {mx, my, mx + s2.cx, my + s2.cy};

        // جيت هب — آخر سطر، مجرد نص
        SelectObject(dc, g_fSmall);
        SetTextColor(dc, RGB(105, 122, 148));
        r = {0, 258, rc.right, 278};
        DrawTextW(dc, L"github.com/elpasha3000/AdhanBox", -1, &r, DT_CENTER | DT_NOPREFIX);

        BitBlt(dc0, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
        SelectObject(dc, obmp);
        DeleteObject(bmp);
        DeleteDC(dc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_LBUTTONUP: {
        POINT pt = {LOWORD(l), HIWORD(l)};
        if (PtInRect(&g_aSite, pt)) openUrl(L"https://magicweb.win/?src=adhanbox-about");
        if (PtInRect(&g_aMail, pt)) openUrl(L"mailto:AdhanBox@magicweb.win");
        return 0;
    }
    case WM_SETCURSOR: {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(h, &pt);
        if (PtInRect(&g_aSite, pt) || PtInRect(&g_aMail, pt)) {
            SetCursor(LoadCursor(nullptr, IDC_HAND));
            return TRUE;
        }
        break;
    }
    case WM_COMMAND:
        if (LOWORD(w) == ABT_CLOSE) DestroyWindow(h);
        return 0;
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        g_hAbout = nullptr;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static void aboutBox(HWND h) {
    if (g_hAbout) { SetForegroundWindow(g_hAbout); return; }
    static bool reg = false;
    if (!reg) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = aboutProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"AdhanBoxAbout";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
        RegisterClassW(&wc);
        reg = true;
    }
    RECT pr;
    GetWindowRect(h, &pr);
    int wdt = 500, hgt = 336 + GetSystemMetrics(SM_CYCAPTION) + 2 * GetSystemMetrics(SM_CYFIXEDFRAME);
    g_hAbout = CreateWindowExW(WS_EX_TOOLWINDOW, L"AdhanBoxAbout",
                               g_ar ? L"\u0639\u0646 \u0627\u0644\u0628\u0631\u0646\u0627\u0645\u062c" : L"About AdhanBox",
                               WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                               pr.left + 28, pr.top + 40, wdt, hgt,
                               h, nullptr, GetModuleHandleW(nullptr), nullptr);
    darkTitleBar(g_hAbout);
    ShowWindow(g_hAbout, SW_SHOWNORMAL);
}

static std::wstring exeDir() {
    wchar_t p[MAX_PATH];
    GetModuleFileNameW(nullptr, p, MAX_PATH);
    std::wstring s = p;
    return s.substr(0, s.find_last_of(L'\\'));
}

/* أذان الحرم المكي مدمج جوّه الـexe كمورد — بيتكتب في مجلد الأصوات أول تشغيل. */
static void extractAdhan(const std::wstring& path) {
    HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(101), RT_RCDATA);
    if (!r) return;
    HGLOBAL hg = LoadResource(nullptr, r);
    if (!hg) return;
    void* p = LockResource(hg);
    DWORD n = SizeofResource(nullptr, r);
    if (!p || !n) return;
    HANDLE fh = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (fh == INVALID_HANDLE_VALUE) return;
    DWORD w = 0;
    WriteFile(fh, p, n, &w, nullptr);
    CloseHandle(fh);
}
/* مجلد الأصوات في %LOCALAPPDATA% مش جنب الـexe: مجلد تثبيت MSIX للقراءة فقط،
   فالكتابة جنب الـexe بتفشل والأذان مايشتغلش. (نفس درس Screen2ipcam.) */
static std::wstring soundsDir() {
    wchar_t la[MAX_PATH] = L"";
    std::wstring d;
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, la)) && la[0]) {
        d = la;
        d += L"\\MagicWeb"; CreateDirectoryW(d.c_str(), nullptr);
        d += L"\\AdhanBox"; CreateDirectoryW(d.c_str(), nullptr);
        d += L"\\sounds";
    } else {
        d = exeDir() + L"\\sounds";
    }
    CreateDirectoryW(d.c_str(), nullptr);
    WIN32_FIND_DATAW fd;
    HANDLE f = FindFirstFileW((d + L"\\*.mp3").c_str(), &fd);
    if (f == INVALID_HANDLE_VALUE) {
        // ترقية من نسخة قديمة: انقل أي أصوات كان المستخدم ضايفها جنب الـexe
        std::wstring od = exeDir() + L"\\sounds";
        WIN32_FIND_DATAW o;
        HANDLE of = FindFirstFileW((od + L"\\*.*").c_str(), &o);
        if (of != INVALID_HANDLE_VALUE) {
            do {
                if (o.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                CopyFileW((od + L"\\" + o.cFileName).c_str(),
                          (d + L"\\" + o.cFileName).c_str(), TRUE);
            } while (FindNextFileW(of, &o));
            FindClose(of);
        }
        // لسه فاضي؟ استخرج الأذان المدمج
        HANDLE f2 = FindFirstFileW((d + L"\\*.mp3").c_str(), &fd);
        if (f2 == INVALID_HANDLE_VALUE) extractAdhan(d + L"\\makkah-adhan.mp3");
        else FindClose(f2);
    } else FindClose(f);
    return d;
}
static int listSounds(std::wstring* out, int maxn) {
    static const wchar_t* pats[] = {L"\\*.mp3", L"\\*.wav", L"\\*.wma", L"\\*.m4a"};
    std::wstring d = soundsDir();
    int n = 0;
    for (auto* pat : pats) {
        WIN32_FIND_DATAW fd;
        HANDLE f = FindFirstFileW((d + pat).c_str(), &fd);
        if (f == INVALID_HANDLE_VALUE) continue;
        do { if (n < maxn) out[n++] = fd.cFileName; } while (FindNextFileW(f, &fd));
        FindClose(f);
    }
    return n;
}

// مجلد التنزيلات — الفولدر الافتراضي اللي بيفتح عليه اختيار الأصوات
static std::wstring downloadsDir() {
    wchar_t* p = nullptr;
    std::wstring d;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &p)) && p) {
        d = p;
        CoTaskMemFree(p);
    }
    if (d.empty()) {
        wchar_t up[MAX_PATH] = L"";
        if (GetEnvironmentVariableW(L"USERPROFILE", up, MAX_PATH)) d = std::wstring(up) + L"\\Downloads";
    }
    return d;
}

// اختيار ملفات صوت من أي مكان ونسخها جوّه مجلد الأصوات
static int addSoundFiles(HWND owner) {
    static wchar_t buf[16384];
    buf[0] = 0;
    std::wstring init = downloadsDir();
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = g_ar ? L"ملفات صوت\0*.mp3;*.wav;*.wma;*.m4a\0كل الملفات\0*.*\0\0"
                           : L"Audio files\0*.mp3;*.wav;*.wma;*.m4a\0All files\0*.*\0\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = 16384;
    ofn.lpstrTitle = g_ar ? L"اختار ملفات الأذان اللي عايز تضيفها" : L"Pick adhan files to add";
    if (!init.empty()) ofn.lpstrInitialDir = init.c_str();
    ofn.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST |
                OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return 0;

    std::wstring dst = soundsDir();
    int n = 0;
    std::wstring first = buf;
    const wchar_t* second = buf + first.size() + 1;
    if (!*second) {                       // ملف واحد → المسار كامل
        const wchar_t* nm = wcsrchr(first.c_str(), L'\\');
        nm = nm ? nm + 1 : first.c_str();
        if (CopyFileW(first.c_str(), (dst + L"\\" + nm).c_str(), FALSE)) n++;
    } else {                              // أكتر من ملف → المجلد الأول وبعده الأسماء
        const wchar_t* p = second;
        while (*p) {
            std::wstring src = first + L"\\" + p;
            if (CopyFileW(src.c_str(), (dst + L"\\" + p).c_str(), FALSE)) n++;
            p += wcslen(p) + 1;
        }
    }
    return n;
}

static void stopSound() {
    mciSendStringW(L"close abx", nullptr, 0, nullptr);
    g_playing = false;
    g_playIdx = -1;
    g_preview = false;
    g_stopAt = 0;
    if (g_hSet) InvalidateRect(g_hSet, nullptr, TRUE);
}

// الملف خلص لوحده؟ (MCI بترجّع stopped بعد النهاية)
static bool mciStillPlaying() {
    wchar_t st[32] = L"";
    if (mciSendStringW(L"status abx mode", st, 32, nullptr) != 0) return false;
    return wcscmp(st, L"playing") == 0;
}


static void playFile(const wchar_t* fname, int vol, int dur = 0) {
    stopSound();
    g_stopAt = dur > 0 ? time(nullptr) + dur : 0;
    std::wstring path = soundsDir() + L"\\" + ((fname && *fname) ? fname : L"makkah-adhan.mp3");
    std::wstring cmd = L"open \"" + path + L"\" type mpegvideo alias abx";
    if (mciSendStringW(cmd.c_str(), nullptr, 0, nullptr) == 0) {
        wchar_t vc[64];
        int v = vol < 0 ? 0 : (vol > 100 ? 100 : vol);
        swprintf(vc, 64, L"setaudio abx volume to %d", v * 10);
        mciSendStringW(vc, nullptr, 0, nullptr);
        mciSendStringW(L"play abx", nullptr, 0, nullptr);
        g_playing = true;
    }
}
static void playAdhan() {   // للتجربة العامة — بصوت ودرجة الظهر
    playFile(g_pc[athan::DHUHR].sound, g_pc[athan::DHUHR].vol, g_pc[athan::DHUHR].dur);
    g_playIdx = -2;
}
// زرار ▶/■ توجل: نفس الزرار بيشغّل ويوقّف
static void togglePlay(int i) {
    if (g_playing && g_playIdx == i) { stopSound(); return; }
    playFile(g_pc[i].sound, g_pc[i].vol, g_pc[i].dur);
    g_playIdx = i;
    g_preview = true;
    if (g_hSet) InvalidateRect(g_hSet, nullptr, TRUE);
}

static void recompute() {
    time_t now = time(nullptr);
    tm lt;
    localtime_s(&lt, &now);
    g_pt = makePT();
    g_times = g_pt.compute(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
    g_day = lt.tm_yday;
}

static int nowMinute() {
    time_t now = time(nullptr);
    tm lt;
    localtime_s(&lt, &now);
    return lt.tm_hour * 60 + lt.tm_min;
}

// الصلاة الجاية النهارده (بدون الشروق) — -1 لو خلصوا
static int nextPrayerIdx(int nowMin) {
    for (int x = 0; x < 5; x++) {
        int i = PRAYS[x];
        if (!g_pc[i].on) continue;
        int m = prayMin(i);
        if (m > nowMin) return i;
    }
    return -1;
}

// الشاشة اللي ظهرت لوحدها وقت الأذان ترجع تختفي — سواء الأذان خلص أو المستخدم وقّفه
static void hideIfAutoShown(HWND hwnd) {
    if (!g_autoShown) return;
    g_autoShown = false;
    ShowWindow(hwnd, SW_HIDE);
}

static void tick(HWND hwnd) {
    time_t now = time(nullptr);
    tm lt;
    localtime_s(&lt, &now);
    if (lt.tm_yday != g_day) { recompute(); g_firedMin = -1; }

    // الصوت خلص لوحده؟ نصحّح الحالة، ولو الشاشة كانت ظهرت للأذان نخفيها
    if (g_playing && (!mciStillPlaying() || (g_stopAt && time(nullptr) >= g_stopAt))) {
        stopSound();               // خلص لوحده أو عدّى مدة التشغيل المحددة
        hideIfAutoShown(hwnd);
    }

    int nm = nowMinute();
    bool muted = (g_muteUntil > time(nullptr));
    for (int x = 0; x < 5; x++) {
        int i = PRAYS[x];
        if (!g_pc[i].on) continue;
        int pm = prayMin(i);
        if (pm < 0) continue;
        if (g_preNotify && g_adhanOn && !muted && pm - nm == 10 && g_notifiedMin != pm) {
            g_notifiedMin = pm;
            wchar_t msg[128];
            swprintf(msg, 128, g_ar ? L"%s %s — باقي 10 دقايق" : L"%s at %s — in 10 minutes",
                     pname(i), fmtMin(pm).c_str());
            balloon(g_ar ? L"🕌 استعد للصلاة" : L"🕌 Get ready", msg);
        }
        if (pm == nm && g_firedMin != nm) {
            g_firedMin = nm;
            if (g_adhanOn && !muted) {
                playFile(g_pc[i].sound, g_pc[i].vol, g_pc[i].dur);
                g_playIdx = i;
                // تظهر الشاشة وقت الأذان وتفضل لحد ما يخلص وبعدين تتخفي لوحدها
                if (g_showAtAdhan) {
                    // مخفية في التراي **أو مصغّرة** = إحنا اللي طلّعناها، فنخفيها بعد ما يخلص
                    if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) g_autoShown = true;
                    ShowWindow(hwnd, IsIconic(hwnd) ? SW_RESTORE : SW_SHOWNORMAL);
                    // ويندوز بيمنع برنامج في الخلفية إنه ياخد الواجهة بـSetForegroundWindow لوحدها،
                    // فبنرفعها فوق الكل بـtopmost لحظيًا وبعدين نرجّعها عادية
                    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
                    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                                 SWP_NOMOVE | SWP_NOSIZE);
                    SetForegroundWindow(hwnd);
                    FlashWindow(hwnd, TRUE);   // احتياطي لو ويندوز رفض التقديم
                }
            }
        }
    }
    // كل ثانية: كارت الرأس بس (الساعة والعدّاد) — الشاشة كلها عند تغيّر الدقيقة
    static int lastMin = -1;
    if (nm != lastMin) {
        lastMin = nm;
        InvalidateRect(hwnd, nullptr, TRUE);
    } else {
        RECT rc;
        GetClientRect(hwnd, &rc);
        RECT hd = {14, 12, rc.right - 14, 150};
        InvalidateRect(hwnd, &hd, TRUE);
    }
}

static void roundCard(HDC dc, RECT r, COLORREF fill, COLORREF border) {
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, 14, 14);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b); DeleteObject(p);
}

static void paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc0 = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);

    // رسم مزدوج — منع الوميض
    HDC dc = CreateCompatibleDC(dc0);
    HBITMAP bmp = CreateCompatibleBitmap(dc0, rc.right, rc.bottom);
    HGDIOBJ obmp = SelectObject(dc, bmp);

    HBRUSH bg = CreateSolidBrush(RGB(11, 17, 28));
    FillRect(dc, &rc, bg);
    DeleteObject(bg);
    SetBkMode(dc, TRANSPARENT);

    int nm = nowMinute();
    int nxt = nextPrayerIdx(nm);
    wchar_t line[128];

    // ── كارت الرأس: عنوان + العدّاد ──
    RECT hd = {14, 12, rc.right - 14, 150};
    roundCard(dc, hd, RGB(19, 32, 52), RGB(38, 51, 74));
    SelectObject(dc, g_fBig);
    SetTextColor(dc, RGB(233, 239, 248));
    RECT r = {hd.left, 42, hd.right, 74};
    {
        const athan::City& cc = athan::CITIES[g_cityIdx];
        wchar_t tt[128];
        swprintf(tt, 128, g_ar ? L"🕌 AdhanBox — %s، %s"
                              : L"🕌 AdhanBox — %s, %s",
                 g_ar ? cc.cityAr : cc.cityEn, g_ar ? cc.countryAr : cc.countryEn);
        DrawTextW(dc, tt, -1, &r, DT_CENTER | DT_NOPREFIX | (g_ar ? DT_RTLREADING : 0));
    }
    {   // ── ساعة حيّة بالثواني تحت اسم البرنامج ──
        time_t tn = time(nullptr);
        tm ln;
        localtime_s(&ln, &tn);
        wchar_t ck[32];
        swprintf(ck, 32, L"%02d:%02d:%02d", ln.tm_hour, ln.tm_min, ln.tm_sec);
        SelectObject(dc, g_fClock);
        SetTextColor(dc, RGB(255, 255, 255));
        RECT rk = {hd.left, 76, hd.right, 114};
        DrawTextW(dc, ck, -1, &rk, DT_CENTER);
    }
    SelectObject(dc, g_fMid);
    SetTextColor(dc, RGB(95, 224, 160));
    if (nxt >= 0) {
        int left = prayMin(nxt) - nm;
        swprintf(line, 128,
                 g_ar ? L"⏳ %s %s — باقي %d:%02d ساعة" : L"⏳ %s %s — in %d:%02d",
                 pname(nxt), fmtMin(prayMin(nxt)).c_str(), left / 60, left % 60);
    } else {
        // خلصت صلوات النهارده → عدّاد لفجر بكره (عبر منتصف الليل)
        time_t t2 = time(nullptr) + 24 * 3600;
        tm l2;
        localtime_s(&l2, &t2);
        athan::Times tt2 = g_pt.compute(l2.tm_year + 1900, l2.tm_mon + 1, l2.tm_mday);
        int fm = tt2.minuteOfDay(athan::FAJR);
        if (fm >= 0) {
            fm = ((fm + g_globalTune + g_pc[athan::FAJR].tune) % 1440 + 1440) % 1440;
            int left = fm + 1440 - nm;
            swprintf(line, 128,
                     g_ar ? L"⏳ الفجر بكره %s — باقي %d:%02d ساعة"
                          : L"⏳ Fajr tomorrow %s — in %d:%02d",
                     fmtMin(fm).c_str(), left / 60, left % 60);
        } else {
            swprintf(line, 128, L"%s", S_DONE.get());
        }
    }
    r = {hd.left, 114, hd.right, 144};
    {   // السطر الطويل (زي «خلصت صلوات النهارده») يتصغّر بدل ما يتقص
        SIZE ls;
        GetTextExtentPoint32W(dc, line, (int)wcslen(line), &ls);
        if (ls.cx > hd.right - hd.left - 16) SelectObject(dc, g_fSmall);
    }
    DrawTextW(dc, line, -1, &r, DT_CENTER | DT_NOPREFIX | (g_ar ? DT_RTLREADING : 0));

    // ── كروت المواقيت: شبكة 3×2 ──
    int cw = (rc.right - 28 - 2 * 10) / 3, ch = 66;
    int x0 = 14, y0 = 162;
    for (int i = 0; i < athan::COUNT; i++) {
        // ترتيب من اليمين لليسار: الفجر أول كارت يمين
        int col = g_ar ? 2 - (i % 3) : (i % 3), row = i / 3;
        RECT cr = {x0 + col * (cw + 10), y0 + row * (ch + 10),
                   x0 + col * (cw + 10) + cw, y0 + row * (ch + 10) + ch};
        bool isNext = (i == nxt);
        bool off = (i != athan::SUNRISE && !g_pc[i].on);
        roundCard(dc, cr,
                  isNext ? RGB(16, 61, 40) : RGB(21, 29, 44),
                  isNext ? RGB(47, 191, 113) : RGB(38, 51, 74));
        SelectObject(dc, g_fSmall);
        SetTextColor(dc, isNext ? RGB(125, 227, 171)
                                : (i == athan::SUNRISE ? RGB(120, 138, 166) : RGB(147, 164, 191)));
        RECT rn = {cr.left, cr.top + 8, cr.right, cr.top + 28};
        DrawTextW(dc, pname(i), -1, &rn, DT_CENTER | (g_ar ? DT_RTLREADING : 0));
        SelectObject(dc, g_fMid);
        SetTextColor(dc, off ? RGB(80, 95, 120) : (isNext ? RGB(95, 224, 160) : RGB(233, 239, 248)));
        RECT rt = {cr.left, cr.top + 30, cr.right, cr.bottom - 4};
        DrawTextW(dc, fmtMin(prayMin(i)).c_str(), -1, &rt, DT_CENTER);
    }

    // ── حالة التشغيل ──
    SelectObject(dc, g_fSmall);
    r = {0, 318, rc.right, 340};
    {
        bool muted = (g_muteUntil > time(nullptr));
        const wchar_t* st = g_playing ? S_PLAYING.get()
            : (!g_adhanOn ? (g_ar ? L"⏸ الأذان التلقائي مقفول من الإعدادات" : L"⏸ Auto-adhan is OFF (settings)")
            : (muted ? (g_ar ? L"🔕 مكتوم مؤقتًا — من قايمة التراي ترجّعه" : L"🔕 Temporarily muted — unmute from tray")
                     : S_IDLE.get()));
        SetTextColor(dc, g_playing ? RGB(240, 169, 46)
                                   : ((!g_adhanOn || muted) ? RGB(240, 169, 46) : RGB(91, 107, 134)));
        DrawTextW(dc, st, -1, &r, DT_CENTER | DT_NOPREFIX | (g_ar ? DT_RTLREADING : 0));
    }

    // ── اللينكات ──
    SelectObject(dc, g_fLink);
    SetTextColor(dc, RGB(96, 165, 250));
    const wchar_t* site = S_SITE.get();
    const wchar_t* git  = S_GIT.get();
    SIZE s1, s2;
    GetTextExtentPoint32W(dc, site, (int)wcslen(site), &s1);
    GetTextExtentPoint32W(dc, git, (int)wcslen(git), &s2);
    int gap = 46, totalW = s1.cx + s2.cx + gap;
    int lx = (rc.right - totalW) / 2, ly = rc.bottom - 34;
    g_rGit  = {lx, ly, lx + s2.cx, ly + s2.cy};
    TextOutW(dc, lx, ly, git, (int)wcslen(git));
    g_rSite = {lx + s2.cx + gap, ly, lx + s2.cx + gap + s1.cx, ly + s1.cy};
    TextOutW(dc, g_rSite.left, ly, site, (int)wcslen(site));

    BitBlt(dc0, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, obmp);
    DeleteObject(bmp);
    DeleteDC(dc);
    EndPaint(hwnd, &ps);
}

// زراير مرسومة: أخضر للتجربة وأحمر خافت للإيقاف
static void drawButton(const DRAWITEMSTRUCT* di) {
    // خلفية الزرار نفسها بلون النافذة — من غيرها بتظهر أركان بيضا حوالين الدائري
    {
        HBRUSH wb = CreateSolidBrush(RGB(11, 17, 28));
        FillRect(di->hDC, &di->rcItem, wb);
        DeleteObject(wb);
    }
    if (di->CtlID == IDB_LANG || di->CtlID == IDB_ABOUT || di->CtlID == IDB_SETTINGS) {
        HDC dc = di->hDC;
        bool down = (di->itemState & ODS_SELECTED) != 0;
        HBRUSH b = CreateSolidBrush(down ? RGB(35, 52, 80) : RGB(24, 36, 58));
        HPEN pn = CreatePen(PS_SOLID, 1, RGB(60, 80, 110));
        HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, pn);
        RoundRect(dc, di->rcItem.left, di->rcItem.top, di->rcItem.right, di->rcItem.bottom, 9, 9);
        SelectObject(dc, ob); SelectObject(dc, op);
        DeleteObject(b); DeleteObject(pn);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(147, 180, 220));
        SelectObject(dc, g_fSmall);
        RECT r = di->rcItem;
        DrawTextW(dc, di->CtlID == IDB_LANG ? (g_ar ? L"EN" : L"عربي") : (di->CtlID == IDB_ABOUT ? L"ℹ" : L"⚙"),
                  -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    bool test = (di->CtlID == IDB_TEST);
    bool down = (di->itemState & ODS_SELECTED) != 0;
    COLORREF fill = test ? (down ? RGB(31, 154, 88) : RGB(47, 191, 113))
                         : (down ? RGB(84, 26, 32) : RGB(61, 18, 22));
    COLORREF border = test ? RGB(47, 191, 113) : RGB(160, 60, 70);
    COLORREF txt = test ? RGB(6, 33, 15) : RGB(255, 139, 150);
    HDC dc = di->hDC;
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    RoundRect(dc, di->rcItem.left, di->rcItem.top, di->rcItem.right, di->rcItem.bottom, 12, 12);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b); DeleteObject(p);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, txt);
    SelectObject(dc, g_fMid);
    wchar_t t[64];
    GetWindowTextW(di->hwndItem, t, 64);
    RECT r = di->rcItem;
    DrawTextW(dc, t, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// ═══ نافذة الإعدادات الكاملة — زي صفحة الراسبيري ═══
struct SetRow { bool* val; const wchar_t* ar; const wchar_t* en; };
static SetRow g_rows[] = {
    {&g_adhanOn,     L"الأذان التلقائي في وقت كل صلاة", L"Automatic adhan at prayer times"},
    {&g_preNotify,   L"تنبيه قبل الأذان بـ10 دقايق",     L"Notify 10 minutes before adhan"},
    {&g_autoStart,   L"تشغيل البرنامج مع بدء ويندوز (مصغّر)", L"Start with Windows (minimized)"},
    {&g_showAtAdhan, L"إظهار الشاشة وقت الأذان وإخفاؤها بعده", L"Show window during adhan, hide after"},
};
enum { CB_COUNTRY = 340, CB_CITY, CB_METHOD, CB_ASR, ED_GTUNE,
       BT_GMINUS = 345, BT_GPLUS, BT_GAPPLY,
       BT_FOLDER = 350, BT_ADDSND, BT_CLOSE,
       CB_SND0 = 360, ED_VOL0 = 370, ED_TUNE0 = 380, BT_PLAY0 = 390,
       BT_VMINUS0 = 400, BT_VPLUS0 = 410, CB_DUR0 = 420 };
static const int DURS[] = {0, 15, 30, 45, 60, 90, 120, 180, 300};
static const int DURN = (int)(sizeof(DURS) / sizeof(DURS[0]));
static HBRUSH g_hbDark = nullptr;
static const int PROW_Y0 = 250, PROW_H = 44, GROW_Y0 = 478, GROW_H = 42, GROW_N = 4;
static const int BTNROW_Y = GROW_Y0 + GROW_N * GROW_H + 8;   // صف زراير مجلد الأصوات
static const int CLOSE_Y  = BTNROW_Y + 44;                    // زرار الإغلاق

static void drawSwitch(HDC dc, RECT r, bool on) {
    HBRUSH b = CreateSolidBrush(on ? RGB(47, 191, 113) : RGB(57, 70, 92));
    HPEN pn = CreatePen(PS_SOLID, 1, on ? RGB(47, 191, 113) : RGB(70, 86, 110));
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, pn);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, r.bottom - r.top, r.bottom - r.top);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b); DeleteObject(pn);
    int d = (r.bottom - r.top) - 6;
    int x = on ? r.right - d - 3 : r.left + 3;
    HBRUSH kb = CreateSolidBrush(RGB(255, 255, 255));
    HGDIOBJ ok = SelectObject(dc, kb);
    Ellipse(dc, x, r.top + 3, x + d, r.top + 3 + d);
    SelectObject(dc, ok);
    DeleteObject(kb);
}

static void fillCitiesCombo(HWND h) {
    HWND cb = GetDlgItem(h, CB_CITY);
    SendMessageW(cb, CB_RESETCONTENT, 0, 0);
    const wchar_t* country = athan::CITIES[g_cityIdx].countryAr;
    int sel = 0;
    for (int i = 0; i < athan::CITIES_N; i++) {
        if (wcscmp(athan::CITIES[i].countryAr, country)) continue;
        int idx = (int)SendMessageW(cb, CB_ADDSTRING, 0,
                    (LPARAM)(g_ar ? athan::CITIES[i].cityAr : athan::CITIES[i].cityEn));
        SendMessageW(cb, CB_SETITEMDATA, idx, i);
        if (i == g_cityIdx) sel = idx;
    }
    SendMessageW(cb, CB_SETCURSEL, sel, 0);
}
static void fillCombos(HWND h) {
    HWND cb = GetDlgItem(h, CB_COUNTRY);
    SendMessageW(cb, CB_RESETCONTENT, 0, 0);
    const wchar_t* cur = athan::CITIES[g_cityIdx].countryAr;
    const wchar_t* last = L"";
    int sel = 0;
    for (int i = 0; i < athan::CITIES_N; i++) {
        if (!wcscmp(athan::CITIES[i].countryAr, last)) continue;
        last = athan::CITIES[i].countryAr;
        int idx = (int)SendMessageW(cb, CB_ADDSTRING, 0,
                    (LPARAM)(g_ar ? athan::CITIES[i].countryAr : athan::CITIES[i].countryEn));
        SendMessageW(cb, CB_SETITEMDATA, idx, i);
        if (!wcscmp(last, cur)) sel = idx;
    }
    SendMessageW(cb, CB_SETCURSEL, sel, 0);
    fillCitiesCombo(h);

    cb = GetDlgItem(h, CB_METHOD);
    SendMessageW(cb, CB_RESETCONTENT, 0, 0);
    int i2 = 0, msel = 0;
    for (auto& kv : athan::methods()) {
        SendMessageW(cb, CB_ADDSTRING, 0,
                     (LPARAM)(g_ar ? kv.second.labelAr : kv.second.labelEn));
        if (kv.first == g_method) msel = i2;
        i2++;
    }
    SendMessageW(cb, CB_SETCURSEL, msel, 0);

    cb = GetDlgItem(h, CB_ASR);
    SendMessageW(cb, CB_RESETCONTENT, 0, 0);
    SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)(g_ar ? L"الجمهور" : L"Standard"));
    SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)(g_ar ? L"الحنفي" : L"Hanafi"));
    SendMessageW(cb, CB_SETCURSEL, g_asr == 2 ? 1 : 0, 0);

    wchar_t gb[16];
    swprintf(gb, 16, L"%d", g_globalTune);
    SetWindowTextW(GetDlgItem(h, ED_GTUNE), gb);

    std::wstring snd[64];
    int ns = listSounds(snd, 64);
    for (int x = 0; x < 5; x++) {
        int i = PRAYS[x];
        HWND sc = GetDlgItem(h, (int)(CB_SND0 + i));
        SendMessageW(sc, CB_RESETCONTENT, 0, 0);
        int ssel = 0;
        for (int j = 0; j < ns; j++) {
            SendMessageW(sc, CB_ADDSTRING, 0, (LPARAM)snd[j].c_str());
            if (snd[j] == g_pc[i].sound) ssel = j;
        }
        SendMessageW(sc, CB_SETCURSEL, ssel, 0);
        HWND dcb = GetDlgItem(h, (int)(CB_DUR0 + i));
        SendMessageW(dcb, CB_RESETCONTENT, 0, 0);
        int dsel = 0;
        for (int j = 0; j < DURN; j++) {
            wchar_t db[32];
            if (DURS[j] == 0) wcscpy_s(db, g_ar ? L"كامل" : L"Full");
            else swprintf(db, 32, g_ar ? L"%d ثانية" : L"%d sec", DURS[j]);
            SendMessageW(dcb, CB_ADDSTRING, 0, (LPARAM)db);
            if (DURS[j] == g_pc[i].dur) dsel = j;
        }
        SendMessageW(dcb, CB_SETCURSEL, dsel, 0);
        wchar_t b[16];
        swprintf(b, 16, L"%d", g_pc[i].vol);
        SetWindowTextW(GetDlgItem(h, (int)(ED_VOL0 + i)), b);
        swprintf(b, 16, L"%d", g_pc[i].tune);
        SetWindowTextW(GetDlgItem(h, (int)(ED_TUNE0 + i)), b);
    }
}

static void applyFromControls(HWND h) {
    HWND cb = GetDlgItem(h, CB_CITY);
    int idx = (int)SendMessageW(cb, CB_GETCURSEL, 0, 0);
    if (idx >= 0) g_cityIdx = (int)SendMessageW(cb, CB_GETITEMDATA, idx, 0);
    int mi = (int)SendMessageW(GetDlgItem(h, CB_METHOD), CB_GETCURSEL, 0, 0);
    int i2 = 0;
    for (auto& kv : athan::methods()) {
        if (i2 == mi) { strcpy_s(g_method, kv.first.c_str()); break; }
        i2++;
    }
    g_asr = SendMessageW(GetDlgItem(h, CB_ASR), CB_GETCURSEL, 0, 0) == 1 ? 2 : 1;
    {
        wchar_t gb[16] = L"";
        GetWindowTextW(GetDlgItem(h, ED_GTUNE), gb, 16);
        int g = _wtoi(gb);
        g_globalTune = g < -120 ? -120 : (g > 120 ? 120 : g);
    }
    for (int x = 0; x < 5; x++) {
        int i = PRAYS[x];
        wchar_t b[160];
        HWND sc = GetDlgItem(h, (int)(CB_SND0 + i));
        int si = (int)SendMessageW(sc, CB_GETCURSEL, 0, 0);
        if (si >= 0) {
            SendMessageW(sc, CB_GETLBTEXT, si, (LPARAM)b);
            wcscpy_s(g_pc[i].sound, b);
        }
        GetWindowTextW(GetDlgItem(h, (int)(ED_VOL0 + i)), b, 16);
        int v = _wtoi(b);
        g_pc[i].vol = v < 0 ? 0 : (v > 100 ? 100 : v);
        GetWindowTextW(GetDlgItem(h, (int)(ED_TUNE0 + i)), b, 16);
        v = _wtoi(b);
        g_pc[i].tune = v < -60 ? -60 : (v > 60 ? 60 : v);
        int di = (int)SendMessageW(GetDlgItem(h, (int)(CB_DUR0 + i)), CB_GETCURSEL, 0, 0);
        if (di >= 0 && di < DURN) g_pc[i].dur = DURS[di];
    }
    saveSettings2();
    saveSettings();
    recompute();
    if (g_hwnd) InvalidateRect(g_hwnd, nullptr, TRUE);
}

static LRESULT CALLBACK setProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        if (!g_hbDark) g_hbDark = CreateSolidBrush(RGB(15, 23, 37));
        DWORD cbs = WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL;
        CreateWindowW(L"COMBOBOX", nullptr, cbs, 308, 76, 264, 320, h, (HMENU)CB_COUNTRY, nullptr, nullptr);
        CreateWindowW(L"COMBOBOX", nullptr, cbs, 24, 76, 264, 360, h, (HMENU)CB_CITY, nullptr, nullptr);
        CreateWindowW(L"COMBOBOX", nullptr, cbs, 192, 132, 380, 380, h, (HMENU)CB_METHOD, nullptr, nullptr);
        CreateWindowW(L"COMBOBOX", nullptr, cbs, 24, 132, 158, 120, h, (HMENU)CB_ASR, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                      24, 190, 26, 26, h, (HMENU)BT_GMINUS, nullptr, nullptr);
        CreateWindowW(L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | ES_CENTER | WS_BORDER,
                      54, 190, 74, 26, h, (HMENU)ED_GTUNE, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                      132, 190, 26, 26, h, (HMENU)BT_GPLUS, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                      164, 190, 94, 26, h, (HMENU)BT_GAPPLY, nullptr, nullptr);
        for (int x = 0; x < 5; x++) {
            int i = PRAYS[x], y = PROW_Y0 + x * PROW_H;
            CreateWindowW(L"COMBOBOX", nullptr, cbs, 288, y + 6, 164, 300, h, (HMENU)(INT_PTR)(CB_SND0 + i), nullptr, nullptr);
            CreateWindowW(L"COMBOBOX", nullptr, cbs, 192, y + 6, 90, 260, h, (HMENU)(INT_PTR)(CB_DUR0 + i), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                          164, y + 7, 22, 24, h, (HMENU)(INT_PTR)(BT_VPLUS0 + i), nullptr, nullptr);
            CreateWindowW(L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_CENTER | WS_BORDER,
                          124, y + 7, 38, 24, h, (HMENU)(INT_PTR)(ED_VOL0 + i), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                          100, y + 7, 22, 24, h, (HMENU)(INT_PTR)(BT_VMINUS0 + i), nullptr, nullptr);
            CreateWindowW(L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | ES_CENTER | WS_BORDER,
                          56, y + 7, 38, 24, h, (HMENU)(INT_PTR)(ED_TUNE0 + i), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"\u25B6", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                          24, y + 6, 28, 26, h, (HMENU)(INT_PTR)(BT_PLAY0 + i), nullptr, nullptr);
        }
        CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                      24, BTNROW_Y, 266, 34, h, (HMENU)BT_FOLDER, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                      302, BTNROW_Y, 266, 34, h, (HMENU)BT_ADDSND, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                      192, CLOSE_Y, 200, 36, h, (HMENU)BT_CLOSE, nullptr, nullptr);
        fillCombos(h);
        return 0;
    }
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)w;
        SetTextColor(dc, RGB(233, 239, 248));
        SetBkColor(dc, RGB(15, 23, 37));
        return (LRESULT)g_hbDark;
    }
    case WM_DRAWITEM: {
        const DRAWITEMSTRUCT* di = (const DRAWITEMSTRUCT*)l;
        HBRUSH wb = CreateSolidBrush(RGB(11, 17, 28));
        FillRect(di->hDC, &di->rcItem, wb);
        DeleteObject(wb);
        bool down = (di->itemState & ODS_SELECTED) != 0;
        int  id = (int)di->CtlID;
        bool wide = (id == BT_FOLDER || id == BT_ADDSND || id == BT_CLOSE || id == BT_GAPPLY);
        bool closeb = (id == BT_CLOSE);
        // زراير ناقص/زايد الصغيرة (الصوت والترحيل) — شكل محايد رمادي
        bool step = (id == BT_GMINUS || id == BT_GPLUS ||
                     (id >= BT_VMINUS0 && id < BT_VMINUS0 + athan::COUNT) ||
                     (id >= BT_VPLUS0 && id < BT_VPLUS0 + athan::COUNT));
        if (step) {
            bool plus = (id == BT_GPLUS || (id >= BT_VPLUS0 && id < BT_VPLUS0 + athan::COUNT));
            HBRUSH sb = CreateSolidBrush(down ? RGB(45, 62, 92) : RGB(28, 40, 62));
            HPEN sp = CreatePen(PS_SOLID, 1, RGB(64, 84, 116));
            HGDIOBJ o1 = SelectObject(di->hDC, sb), o2 = SelectObject(di->hDC, sp);
            RoundRect(di->hDC, di->rcItem.left, di->rcItem.top, di->rcItem.right, di->rcItem.bottom, 8, 8);
            SelectObject(di->hDC, o1); SelectObject(di->hDC, o2);
            DeleteObject(sb); DeleteObject(sp);
            SetBkMode(di->hDC, TRANSPARENT);
            SetTextColor(di->hDC, RGB(190, 210, 236));
            SelectObject(di->hDC, g_fMid);
            RECT sr = di->rcItem;
            DrawTextW(di->hDC, plus ? L"+" : L"−", -1, &sr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        // زرار التجربة توجل: ▶ أخضر وهو واقف، ■ أحمر وهو شغّال
        bool stopMode = (!wide && g_playing && g_playIdx == id - BT_PLAY0);
        COLORREF fill = down ? RGB(35, 52, 80)
                      : (closeb ? RGB(30, 44, 68)
                      : (wide ? RGB(24, 36, 58)
                      : (stopMode ? RGB(61, 18, 22) : RGB(16, 61, 40))));
        COLORREF edge = closeb ? RGB(80, 104, 140)
                      : (wide ? RGB(60, 80, 110)
                      : (stopMode ? RGB(160, 60, 70) : RGB(47, 191, 113)));
        COLORREF ink  = closeb ? RGB(200, 217, 240)
                      : (wide ? RGB(147, 180, 220)
                      : (stopMode ? RGB(255, 139, 150) : RGB(125, 227, 171)));
        HBRUSH b = CreateSolidBrush(fill);
        HPEN pn = CreatePen(PS_SOLID, 1, edge);
        HGDIOBJ ob = SelectObject(di->hDC, b), op = SelectObject(di->hDC, pn);
        RoundRect(di->hDC, di->rcItem.left, di->rcItem.top, di->rcItem.right, di->rcItem.bottom, 10, 10);
        SelectObject(di->hDC, ob); SelectObject(di->hDC, op);
        DeleteObject(b); DeleteObject(pn);
        SetBkMode(di->hDC, TRANSPARENT);
        SetTextColor(di->hDC, ink);
        SelectObject(di->hDC, closeb ? g_fMid : g_fSmall);
        const wchar_t* cap;
        if (id == BT_GAPPLY)
            cap = g_ar ? L"✔ تطبيق" : L"✔ Apply";
        else if (id == BT_FOLDER)
            cap = g_ar ? L"\U0001F4C2 فتح مجلد الأصوات"
                       : L"\U0001F4C2 Open sounds folder";
        else if (id == BT_ADDSND)
            cap = g_ar ? L"\u2795 أضف ملفات أذان"
                       : L"\u2795 Add adhan files";
        else if (closeb)
            cap = g_ar ? L"إغلاق" : L"Close";
        else
            cap = stopMode ? L"\u25A0" : L"\u25B6";
        RECT r = di->rcItem;
        DrawTextW(di->hDC, cap, -1, &r,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | ((g_ar && wide) ? DT_RTLREADING : 0));
        return TRUE;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc0 = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        HDC dc = CreateCompatibleDC(dc0);
        HBITMAP bmp = CreateCompatibleBitmap(dc0, rc.right, rc.bottom);
        HGDIOBJ obmp = SelectObject(dc, bmp);
        HBRUSH bg = CreateSolidBrush(RGB(11, 17, 28));
        FillRect(dc, &rc, bg); DeleteObject(bg);
        SetBkMode(dc, TRANSPARENT);
        SelectObject(dc, g_fMid);
        SetTextColor(dc, RGB(233, 239, 248));
        RECT rt = {0, 14, rc.right, 42};
        DrawTextW(dc, g_ar ? L"\u2699 الإعدادات" : L"\u2699 Settings", -1, &rt,
                  DT_CENTER | (g_ar ? DT_RTLREADING : 0));
        SelectObject(dc, g_fSmall);
        SetTextColor(dc, RGB(147, 164, 191));
        RECT r1 = {308, 56, 572, 74};
        DrawTextW(dc, g_ar ? L"الدولة" : L"Country", -1, &r1, (g_ar ? DT_RIGHT | DT_RTLREADING : DT_LEFT));
        RECT r2 = {24, 56, 288, 74};
        DrawTextW(dc, g_ar ? L"المدينة" : L"City", -1, &r2, (g_ar ? DT_RIGHT | DT_RTLREADING : DT_LEFT));
        RECT r3 = {192, 112, 572, 130};
        DrawTextW(dc, g_ar ? L"طريقة الحساب" : L"Calculation method", -1, &r3, (g_ar ? DT_RIGHT | DT_RTLREADING : DT_LEFT));
        RECT r4 = {24, 112, 182, 130};
        DrawTextW(dc, g_ar ? L"مذهب العصر" : L"Asr", -1, &r4, (g_ar ? DT_RIGHT | DT_RTLREADING : DT_LEFT));
        // ── الترحيل العام لكل المواقيت ──
        RECT r5 = {24, 168, 258, 188};
        DrawTextW(dc, g_ar ? L"ترحيل عام لكل المواقيت (± دقيقة)"
                           : L"Global shift for all times (± min)", -1, &r5,
                  (g_ar ? DT_RIGHT | DT_RTLREADING : DT_LEFT));
        SetTextColor(dc, RGB(120, 138, 166));
        RECT r6 = {286, 162, 572, 220};
        DrawTextW(dc, g_ar ? L"بيتضاف لكل مواقيت الصلاة. غيّر الرقم بزرار الناقص أو الزايد، وبعدين دوس «تطبيق»"
                           : L"Added to all prayer times. Use the minus or plus button, then press Apply",
                  -1, &r6, DT_WORDBREAK | (g_ar ? DT_RIGHT | DT_RTLREADING : DT_LEFT));
        RECT hh0 = {192, PROW_Y0 - 20, 282, PROW_Y0 - 2};
        DrawTextW(dc, g_ar ? L"مدة التشغيل" : L"Duration", -1, &hh0,
                  DT_CENTER | (g_ar ? DT_RTLREADING : 0));
        RECT hh1 = {288, PROW_Y0 - 20, 452, PROW_Y0 - 2};
        DrawTextW(dc, g_ar ? L"صوت الأذان" : L"Adhan sound", -1, &hh1,
                  DT_CENTER | (g_ar ? DT_RTLREADING : 0));
        RECT hh2 = {100, PROW_Y0 - 20, 186, PROW_Y0 - 2};
        DrawTextW(dc, g_ar ? L"درجة الصوت" : L"Volume", -1, &hh2,
                  DT_CENTER | (g_ar ? DT_RTLREADING : 0));
        RECT hh3 = {50, PROW_Y0 - 20, 100, PROW_Y0 - 2};
        DrawTextW(dc, g_ar ? L"± دقيقة" : L"± min", -1, &hh3, DT_CENTER);
        for (int x = 0; x < 5; x++) {
            int i = PRAYS[x], y = PROW_Y0 + x * PROW_H;
            RECT card = {14, y, rc.right - 14, y + PROW_H - 6};
            roundCard(dc, card, RGB(21, 29, 44), RGB(38, 51, 74));
            SetTextColor(dc, g_pc[i].on ? RGB(219, 231, 251) : RGB(80, 95, 120));
            RECT nm = {card.right - 110, y + 8, card.right - 60, y + 32};
            DrawTextW(dc, pname(i), -1, &nm, DT_CENTER | DT_VCENTER | DT_SINGLELINE | (g_ar ? DT_RTLREADING : 0));
            RECT sw = {card.right - 54, y + 9, card.right - 12, y + 30};
            drawSwitch(dc, sw, g_pc[i].on);
        }
        for (int i = 0; i < GROW_N; i++) {
            int y = GROW_Y0 + i * GROW_H;
            RECT card = {14, y, rc.right - 14, y + GROW_H - 8};
            roundCard(dc, card, RGB(21, 29, 44), RGB(38, 51, 74));
            SetTextColor(dc, RGB(219, 231, 251));
            RECT lb, sw;
            if (g_ar) { lb = {card.left + 70, y + 4, card.right - 16, y + GROW_H - 12};
                        sw = {card.left + 14, y + 8, card.left + 58, y + 30}; }
            else      { lb = {card.left + 16, y + 4, card.right - 70, y + GROW_H - 12};
                        sw = {card.right - 58, y + 8, card.right - 14, y + 30}; }
            DrawTextW(dc, g_ar ? g_rows[i].ar : g_rows[i].en, -1, &lb,
                      (g_ar ? DT_RIGHT | DT_RTLREADING : DT_LEFT) | DT_VCENTER | DT_SINGLELINE);
            drawSwitch(dc, sw, *g_rows[i].val);
        }
        BitBlt(dc0, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
        SelectObject(dc, obmp); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_LBUTTONUP: {
        int x = LOWORD(l), y = HIWORD(l);
        RECT rc; GetClientRect(h, &rc);
        if (y >= PROW_Y0 && y < PROW_Y0 + 5 * PROW_H) {
            int row = (y - PROW_Y0) / PROW_H;
            int cy = PROW_Y0 + row * PROW_H;
            if (y >= cy + 6 && y <= cy + 33 && x >= rc.right - 68 && x <= rc.right - 22) {
                int i = PRAYS[row];
                g_pc[i].on = !g_pc[i].on;
                saveSettings2(); recompute();
                InvalidateRect(h, nullptr, TRUE);
                if (g_hwnd) InvalidateRect(g_hwnd, nullptr, TRUE);
            }
            return 0;
        }
        if (y >= GROW_Y0 && y < GROW_Y0 + GROW_N * GROW_H) {
            int i = (y - GROW_Y0) / GROW_H;
            *g_rows[i].val = !*g_rows[i].val;
            saveSettings();
            InvalidateRect(h, nullptr, TRUE);
            if (g_hwnd) InvalidateRect(g_hwnd, nullptr, TRUE);
            return 0;
        }
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(w), code = HIWORD(w);
        if (id == CB_COUNTRY && code == CBN_SELCHANGE) {
            HWND cb = GetDlgItem(h, CB_COUNTRY);
            int idx = (int)SendMessageW(cb, CB_GETCURSEL, 0, 0);
            g_cityIdx = (int)SendMessageW(cb, CB_GETITEMDATA, idx, 0);
            fillCitiesCombo(h);
            applyFromControls(h);
        } else if ((id == CB_CITY || id == CB_METHOD || id == CB_ASR ||
                    (id >= CB_SND0 && id < CB_SND0 + athan::COUNT) ||
                    (id >= CB_DUR0 && id < CB_DUR0 + athan::COUNT)) && code == CBN_SELCHANGE) {
            applyFromControls(h);
        } else if (((id >= ED_VOL0 && id < ED_TUNE0 + athan::COUNT) || id == ED_GTUNE)
                   && code == EN_KILLFOCUS) {
            applyFromControls(h);
            InvalidateRect(h, nullptr, TRUE);
        } else if (id >= BT_PLAY0 && id < BT_PLAY0 + athan::COUNT) {
            applyFromControls(h);
            togglePlay(id - BT_PLAY0);
            InvalidateRect(h, nullptr, TRUE);
        } else if (id == BT_GMINUS || id == BT_GPLUS) {
            applyFromControls(h);
            g_globalTune += (id == BT_GPLUS) ? 1 : -1;
            if (g_globalTune < -60) g_globalTune = -60;
            if (g_globalTune >  60) g_globalTune =  60;
            saveSettings2(); recompute(); fillCombos(h);
            if (g_hwnd) InvalidateRect(g_hwnd, nullptr, TRUE);
        } else if (id == BT_GAPPLY) {
            // «تطبيق»: الترحيل العام يتوزّع على خانة ±دقيقة بتاعة كل صلاة ويرجع صفر
            applyFromControls(h);
            if (g_globalTune != 0) {
                for (int x = 0; x < 5; x++) {
                    int i = PRAYS[x];
                    int t = g_pc[i].tune + g_globalTune;
                    g_pc[i].tune = t < -60 ? -60 : (t > 60 ? 60 : t);
                }
                g_globalTune = 0;
                saveSettings2(); recompute(); fillCombos(h);
                InvalidateRect(h, nullptr, TRUE);
                if (g_hwnd) InvalidateRect(g_hwnd, nullptr, TRUE);
            }
        } else if ((id >= BT_VMINUS0 && id < BT_VMINUS0 + athan::COUNT) ||
                   (id >= BT_VPLUS0 && id < BT_VPLUS0 + athan::COUNT)) {
            bool plus = (id >= BT_VPLUS0);
            int i = plus ? id - BT_VPLUS0 : id - BT_VMINUS0;
            applyFromControls(h);
            int v = g_pc[i].vol + (plus ? 5 : -5);
            g_pc[i].vol = v < 0 ? 0 : (v > 100 ? 100 : v);
            saveSettings2();
            wchar_t vb[16];
            swprintf(vb, 16, L"%d", g_pc[i].vol);
            SetWindowTextW(GetDlgItem(h, (int)(ED_VOL0 + i)), vb);
            // الصوت شغّال؟ درجة الصوت تتغيّر فورًا وانت سامع
            if (g_playing && g_playIdx == i) {
                wchar_t vc[64];
                swprintf(vc, 64, L"setaudio abx volume to %d", g_pc[i].vol * 10);
                mciSendStringW(vc, nullptr, 0, nullptr);
            }
        } else if (id == BT_FOLDER) {
            ShellExecuteW(nullptr, L"open", soundsDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        } else if (id == BT_ADDSND) {
            int n = addSoundFiles(h);
            if (n > 0) {
                fillCombos(h);          // القوايم تتحدّث فورًا بالملفات الجديدة
                wchar_t msg[128];
                swprintf(msg, 128, g_ar ? L"اتضاف %d ملف لمجلد الأصوات." : L"%d file(s) added to the sounds folder.", n);
                MessageBoxW(h, msg, g_ar ? L"تمام" : L"Done", MB_OK | MB_ICONINFORMATION);
            }
        } else if (id == BT_CLOSE) {
            SendMessageW(h, WM_CLOSE, 0, 0);
        }
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_CLOSE:
        applyFromControls(h);
        if (g_preview) stopSound();     // تجربة صوت شغّالة؟ تقف مع الإغلاق (الأذان الحقيقي بيكمّل)
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        g_hSet = nullptr;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static void openSettings(HWND parent) {
    if (g_hSet) { SetForegroundWindow(g_hSet); return; }
    static bool reg = false;
    if (!reg) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = setProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"AdhanBoxSet";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
        RegisterClassW(&wc);
        reg = true;
    }
    RECT pr; GetWindowRect(parent, &pr);
    int hgt = CLOSE_Y + 36 + 12 + GetSystemMetrics(SM_CYCAPTION) + 2 * GetSystemMetrics(SM_CYFIXEDFRAME);
    int top = 40;
    int scr = GetSystemMetrics(SM_CYSCREEN);
    if (top + hgt > scr - 40) top = (scr - hgt) / 2 > 0 ? (scr - hgt) / 2 : 0;
    int left = pr.left - 110;                       // تفضل جوّه الشاشة مهما كان مكان النافذة الرئيسية
    int scrW = GetSystemMetrics(SM_CXSCREEN);
    if (left < 8) left = 8;
    if (left + 600 > scrW - 8) left = scrW - 608;
    g_hSet = CreateWindowExW(WS_EX_TOOLWINDOW, L"AdhanBoxSet",
                             g_ar ? L"الإعدادات" : L"Settings",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                             left, top, 600, hgt,
                             parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    darkTitleBar(g_hSet);
    ShowWindow(g_hSet, SW_SHOWNORMAL);
}

static LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        CreateWindowW(L"BUTTON", S_TEST.get(), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                      252, 348, 158, 38, h, (HMENU)IDB_TEST, nullptr, nullptr);
        CreateWindowW(L"BUTTON", S_STOP.get(), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                      76, 348, 158, 38, h, (HMENU)IDB_STOP, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"EN", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                      18, 18, 44, 26, h, (HMENU)IDB_LANG, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"ℹ", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                      66, 18, 30, 26, h, (HMENU)IDB_ABOUT, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"⚙", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                      100, 18, 30, 26, h, (HMENU)IDB_SETTINGS, nullptr, nullptr);
        SetTimer(h, IDT_TICK, 1000, nullptr);    // كل ثانية — الساعة الحيّة + رصد نهاية الأذان
        return 0;
    }
    case WM_DRAWITEM:
        drawButton((const DRAWITEMSTRUCT*)l);
        return TRUE;
    case WM_LBUTTONUP: {
        POINT pt = {LOWORD(l), HIWORD(l)};
        if (PtInRect(&g_rSite, pt)) openUrl(L"https://magicweb.win/?src=adhanbox-win");
        if (PtInRect(&g_rGit, pt))  openUrl(L"https://github.com/elpasha3000/AdhanBox");
        return 0;
    }
    case WM_SETCURSOR: {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(h, &pt);
        if (PtInRect(&g_rSite, pt) || PtInRect(&g_rGit, pt)) {
            SetCursor(LoadCursor(nullptr, IDC_HAND));
            return TRUE;
        }
        break;
    }
    case WM_ERASEBKGND:
        return 1;    // الرسم المزدوج بيغطي كل حاجة — منع الوميض
    case WM_COMMAND:
        if (LOWORD(w) == IDB_TEST) { playAdhan(); InvalidateRect(h, nullptr, TRUE); }
        if (LOWORD(w) == IDB_STOP) { stopSound(); hideIfAutoShown(h); InvalidateRect(h, nullptr, TRUE); }
        if (LOWORD(w) == IDB_LANG) {
            g_ar = !g_ar; saveLang();
            SetWindowTextW(GetDlgItem(h, IDB_TEST), S_TEST.get());
            SetWindowTextW(GetDlgItem(h, IDB_STOP), S_STOP.get());
            wcscpy_s(g_nid.szTip, S_TIP.get());
            Shell_NotifyIconW(NIM_MODIFY, &g_nid);
            InvalidateRect(h, nullptr, TRUE);
        }
        if (LOWORD(w) == IDB_ABOUT || LOWORD(w) == IDM_ABOUT) aboutBox(h);
        if (LOWORD(w) == IDM_MUTE1H) { g_muteUntil = time(nullptr) + 3600; stopSound(); hideIfAutoShown(h); InvalidateRect(h, nullptr, TRUE); }
        if (LOWORD(w) == IDM_MUTEDAY) { g_muteUntil = time(nullptr) + 24 * 3600; stopSound(); hideIfAutoShown(h); InvalidateRect(h, nullptr, TRUE); }
        if (LOWORD(w) == IDM_UNMUTE) { g_muteUntil = 0; InvalidateRect(h, nullptr, TRUE); }
        if (LOWORD(w) == IDB_SETTINGS) openSettings(h);
        if (LOWORD(w) == IDM_OPEN) ShowWindow(h, SW_SHOWNORMAL), SetForegroundWindow(h);
        if (LOWORD(w) == IDM_STOPSND) { stopSound(); hideIfAutoShown(h); }
        if (LOWORD(w) == IDM_EXIT) DestroyWindow(h);
        return 0;
    case WM_TIMER:
        tick(h);
        return 0;
    case WM_PAINT:
        paint(h);
        return 0;
    case WM_TRAY:
        if (l == WM_LBUTTONUP) { ShowWindow(h, SW_SHOWNORMAL); SetForegroundWindow(h); }
        if (l == WM_RBUTTONUP) {
            HMENU mnu = CreatePopupMenu();
            AppendMenuW(mnu, MF_STRING, IDM_OPEN, S_OPEN.get());
            AppendMenuW(mnu, MF_STRING, IDM_STOPSND, S_MSTOP.get());
            bool muted = (g_muteUntil > time(nullptr));
            if (muted)
                AppendMenuW(mnu, MF_STRING, IDM_UNMUTE, g_ar ? L"🔔 إلغاء الكتم" : L"🔔 Unmute");
            else {
                AppendMenuW(mnu, MF_STRING, IDM_MUTE1H, g_ar ? L"🔕 إيقاف الصوت لمدة ساعة" : L"🔕 Mute for 1 hour");
                AppendMenuW(mnu, MF_STRING, IDM_MUTEDAY, g_ar ? L"🔕 إيقاف الصوت لمدة 24 ساعة" : L"🔕 Mute for 24 hours");
            }
            AppendMenuW(mnu, MF_STRING, IDM_ABOUT, S_ABOUT.get());
            AppendMenuW(mnu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(mnu, MF_STRING, IDM_EXIT, S_EXIT.get());
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(h);
            TrackPopupMenu(mnu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, h, nullptr);
            DestroyMenu(mnu);
        }
        return 0;
    case WM_CLOSE:
        ShowWindow(h, SW_HIDE);           // إغلاق = تصغير للتراي، الأذان شغّال
        return 0;
    case WM_DESTROY:
        stopSound();
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int) {
    loadLang();
    loadSettings();
    loadSettings2();
    recompute();
    g_fBig   = CreateFontW(-24, 0, 0, 0, FW_BOLD,   0, 0, 0, DEFAULT_CHARSET, 0, 0,
                           CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fMid   = CreateFontW(-19, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                           CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fSmall = CreateFontW(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                           CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fClock = CreateFontW(-32, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                           CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fLink  = CreateFontW(-15, 0, 0, 0, FW_SEMIBOLD, 0, TRUE, 0, DEFAULT_CHARSET, 0, 0,
                           CLEARTYPE_QUALITY, 0, L"Segoe UI");   // مسطّر — شكل اللينك

    WNDCLASSW wc = {};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hi;
    wc.lpszClassName = L"AdhanBoxWnd";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(hi, MAKEINTRESOURCEW(1));
    RegisterClassW(&wc);

    HWND h = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, APP_NAME,
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                             CW_USEDEFAULT, CW_USEDEFAULT, 496, 484,
                             nullptr, nullptr, hi, nullptr);

    g_hwnd = h;
    darkTitleBar(h);
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = h;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                    GetSystemMetrics(SM_CXSMICON),
                                    GetSystemMetrics(SM_CYSMICON), 0);
    if (!g_nid.hIcon) g_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, S_TIP.get());
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    // مع بدء ويندوز بيتفتح مصغّر في التراي (/tray) — والباقي عادي
    bool startHidden = false;
    {
        const wchar_t* cl = GetCommandLineW();
        if (cl && (wcsstr(cl, L"/tray") || wcsstr(cl, L"-tray"))) startHidden = true;
    }
    ShowWindow(h, startHidden ? SW_HIDE : SW_SHOWNORMAL);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
