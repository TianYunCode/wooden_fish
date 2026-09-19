#include "core/autorun.h"

#include <windows.h>
#include <string>

namespace muyu {

namespace {
const wchar_t *kRunPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t *kRunValue = L"WoodenFish";
}  // namespace

bool AutoRunOn() {
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunPath, 0, KEY_QUERY_VALUE, &k) != ERROR_SUCCESS)
        return false;
    DWORD cb = 0;
    bool exists = RegQueryValueExW(k, kRunValue, nullptr, nullptr, nullptr, &cb) == ERROR_SUCCESS;
    RegCloseKey(k);
    return exists;
}

void AutoRunSet(bool on) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k,
                        nullptr) != ERROR_SUCCESS)
        return;
    if (on) {
        wchar_t path[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring cmd = L"\"" + std::wstring(path) + L"\"";
        RegSetValueExW(k, kRunValue, 0, REG_SZ, reinterpret_cast<const BYTE *>(cmd.c_str()),
                       static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(k, kRunValue);
    }
    RegCloseKey(k);
}

}  // namespace muyu
