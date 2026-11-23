#include <windows.h>
#include <stdio.h>
#include <string>
#include <random>

static const wchar_t* SEEDDIR = L"C:\\ProgramData\\spoof";
static const wchar_t* DST = L"C:\\Windows\\System32\\drivers\\spoof.sys";

static void Run(const std::wstring& c) { _wsystem(c.c_str()); }

static bool WriteSeed(unsigned long long s)
{
    std::wstring p = std::wstring(SEEDDIR) + L"\\seed.bin";
    HANDLE h; DWORD w; HKEY k; DWORD d;
    CreateDirectoryW(SEEDDIR, NULL);
    h = CreateFileW(p.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) { WriteFile(h, &s, 8, &w, NULL); CloseHandle(h); }
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE,
            L"SYSTEM\\CurrentControlSet\\Services\\spoof\\Parameters", 0, NULL, 0,
            KEY_WRITE, NULL, &k, &d) == ERROR_SUCCESS) {
        RegSetValueExW(k, L"Seed", 0, REG_BINARY, (BYTE*)&s, 8);
        RegCloseKey(k);
    }
    return true;
}

static unsigned long long NewSeed()
{
    std::random_device rd; unsigned long long s = 0;
    while (!s) s = ((unsigned long long)rd() << 32) ^ rd();
    return s;
}

static bool IsAdmin()
{
    HANDLE t; TOKEN_ELEVATION e = {}; DWORD n; bool ok = false;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t)) {
        if (GetTokenInformation(t, TokenElevation, &e, sizeof(e), &n)) ok = e.TokenIsElevated != 0;
        CloseHandle(t);
    }
    return ok;
}

int wmain(int argc, wchar_t** argv)
{
    if (!IsAdmin()) { wprintf(L"Run this as Administrator.\n"); return 1; }

    if (argc > 1) {
        std::wstring a = argv[1];
        if (a == L"install") {
            if (!CopyFileW(L"spoof.sys", DST, FALSE)) { wprintf(L"Cannot copy driver.\n"); return 1; }
            WriteSeed(NewSeed());
            Run(L"sc stop spoof >nul 2>&1");
            Run(L"sc delete spoof >nul 2>&1");
            Run(L"sc create spoof type= kernel start= demand binPath= C:\\Windows\\System32\\drivers\\spoof.sys");
            Run(L"sc start spoof");
            wprintf(L"Loaded.\n"); return 0;
        }
        if (a == L"reroll") {
            Run(L"sc stop spoof >nul 2>&1");
            WriteSeed(NewSeed());
            Run(L"sc start spoof");
            wprintf(L"Rerolled and reloaded.\n"); return 0;
        }
        if (a == L"unload") { Run(L"sc stop spoof"); wprintf(L"Unloaded. Machine is back to normal.\n"); return 0; }
        if (a == L"remove") {
            Run(L"sc stop spoof >nul 2>&1"); Run(L"sc delete spoof >nul 2>&1");
            DeleteFileW(DST); wprintf(L"Removed.\n"); return 0;
        }
    }

    wprintf(L"  1  load\n  2  reroll (new identity)\n  3  unload\n  4  remove\n  0  exit\n");
    for (;;) {
        wchar_t c[8]; wprintf(L"> "); if (!wscanf_s(L"%7s", c, 7)) break;
        if (c[0] == L'0') break;
        if (c[0] == L'1') { wchar_t* av[] = { L"", L"install" }; wmain(2, av); }
        if (c[0] == L'2') { wchar_t* av[] = { L"", L"reroll" }; wmain(2, av); }
        if (c[0] == L'3') { wchar_t* av[] = { L"", L"unload" }; wmain(2, av); }
        if (c[0] == L'4') { wchar_t* av[] = { L"", L"remove" }; wmain(2, av); }
    }
    return 0;
}
