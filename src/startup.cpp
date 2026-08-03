/* AdhanBoxStartup.exe — مشغّل صغير للتشغيل مع بدء ويندوز في حزمة MSIX.
   نفس نمط s2c_start.exe بتاع Screen2ipcam: StartupTask بتشغّل الملف ده،
   وهو يشغّل AdhanBox.exe بعلم /tray (مصغّر في شريط المهام) وبعدين يقفل.
   السبب: StartupTask مابتقدرش تمرّر معاملات، والبرنامج لازم يبدأ مخفي. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    wchar_t p[MAX_PATH] = L"";
    GetModuleFileNameW(nullptr, p, MAX_PATH);
    std::wstring dir(p);
    size_t s = dir.find_last_of(L'\\');
    if (s == std::wstring::npos) return 1;
    dir.resize(s);

    std::wstring exe = dir + L"\\AdhanBox.exe";
    std::wstring cmd = L"\"" + exe + L"\" /tray";

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (CreateProcessW(exe.c_str(), &cmd[0], nullptr, nullptr, FALSE,
                       0, nullptr, dir.c_str(), &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 0;
    }
    return 1;
}
