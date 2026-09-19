#include "core/settings.h"

#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <iterator>

#include "config/layout.h"
#include "core/util.h"

namespace muyu {

namespace {
const wchar_t *kRegPath = L"Software\\WoodenFish";

void w64At(HKEY h, const wchar_t *n, unsigned long long v) {
    RegSetValueExW(h, n, 0, REG_BINARY, reinterpret_cast<const BYTE *>(&v), sizeof(v));
}
}  // namespace

void LoadSettings(AppState &st) {
    st.scale = config::kScaleDefault;
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegPath, 0, KEY_READ, &k) != ERROR_SUCCESS)
        return;
    auto u64 = [&](const wchar_t *n, unsigned long long &out) {
        DWORD s = sizeof(out);
        RegQueryValueExW(k, n, nullptr, nullptr, reinterpret_cast<BYTE *>(&out), &s);
    };
    auto u32 = [&](const wchar_t *n, DWORD &out) {
        DWORD s = sizeof(out);
        RegQueryValueExW(k, n, nullptr, nullptr, reinterpret_cast<BYTE *>(&out), &s);
    };
    DWORD v = 0;
    u64(L"merit", st.merit);
    u64(L"daily", st.daily);
    u32(L"date", v);
    if (v != TodayYmd()) {
        st.daily = 0;
        v = TodayYmd();
    }
    st.dailyDate = v;
    u32(L"scale", v);
    if (v >= 300 && v <= 2000)
        st.scale = v / 1000.0;
    u32(L"topmost", v);
    st.topmost = v != 0;
    u32(L"pin", v);
    st.pinned = v != 0;
    u32(L"vol", v);
    if (v <= 3) st.volIdx = static_cast<int>(v);
    u32(L"auto", v);
    if (v <= 3) st.autoIdx = static_cast<int>(v);
    u32(L"goal", v);
    if (v <= 5) st.goalIdx = static_cast<int>(v);
    u32(L"goalX", v);
    if (v >= 1 && v <= 99999) st.goalCustom = v;
    u32(L"word", v);
    if (v <= 2) st.wordIdx = static_cast<int>(v);
    u32(L"skin", v);
    if (v <= 3) st.skinIdx = static_cast<int>(v);
    u32(L"zen", v);
    if (v <= static_cast<DWORD>(config::kZenCustomIdx)) st.zenIdx = static_cast<int>(v);
    wchar_t path[512] = L"";
    DWORD sz = sizeof(path);
    RegQueryValueExW(k, L"zenFile", nullptr, nullptr, reinterpret_cast<BYTE *>(path), &sz);
    st.zenFile = path;
    RegCloseKey(k);
}

void SaveSettings(const AppState &st, HWND hwnd) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegPath, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) !=
        ERROR_SUCCESS)
        return;
    auto w64 = [&](const wchar_t *n, unsigned long long v) {
        RegSetValueExW(k, n, 0, REG_BINARY, reinterpret_cast<const BYTE *>(&v), sizeof(v));
    };
    auto w32 = [&](const wchar_t *n, DWORD v) {
        RegSetValueExW(k, n, 0, REG_DWORD, reinterpret_cast<const BYTE *>(&v), sizeof(v));
    };
    w64(L"merit", st.merit);
    w64(L"daily", st.daily);
    w32(L"date", st.dailyDate);
    w32(L"scale", static_cast<DWORD>(st.scale * 1000 + 0.5));
    w32(L"topmost", st.topmost ? 1 : 0);
    w32(L"pin", st.pinned ? 1 : 0);
    w32(L"vol", st.volIdx);
    w32(L"auto", st.autoIdx);
    w32(L"goal", st.goalIdx);
    w32(L"goalX", st.goalCustom);
    w32(L"word", st.wordIdx);
    w32(L"skin", st.skinIdx);
    w32(L"zen", st.zenIdx);
    if (!st.zenFile.empty())
        RegSetValueExW(k, L"zenFile", 0, REG_SZ,
                       reinterpret_cast<const BYTE *>(st.zenFile.c_str()),
                       static_cast<DWORD>((st.zenFile.size() + 1) * sizeof(wchar_t)));
    // 功德簿：每次保存都刷新"当日"条目，一天结束时其值即为当日最终功德
    HKEY log;
    if (st.dailyDate &&
        RegCreateKeyExW(k, L"log", 0, nullptr, 0, KEY_WRITE, nullptr, &log, nullptr) == ERROR_SUCCESS) {
        wchar_t day[16];
        std::swprintf(day, std::size(day), L"%08u", st.dailyDate);
        w64At(log, day, st.daily);
        RegCloseKey(log);
    }
    (void)hwnd;  // 位置不再持久化：每次启动固定屏幕右下角
    RegCloseKey(k);
}

std::vector<std::pair<DWORD, unsigned long long>> ReadLedger() {
    std::vector<std::pair<DWORD, unsigned long long>> rows;
    HKEY k, log;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegPath, 0, KEY_READ, &k) != ERROR_SUCCESS)
        return rows;
    if (RegOpenKeyExW(k, L"log", 0, KEY_READ, &log) == ERROR_SUCCESS) {
        wchar_t name[32];
        for (DWORD i = 0;; ++i) {
            DWORD nlen = std::size(name), t = 0;
            unsigned long long val = 0;
            DWORD cb = sizeof(val);
            if (RegEnumValueW(log, i, name, &nlen, nullptr, &t, reinterpret_cast<BYTE *>(&val), &cb) !=
                ERROR_SUCCESS)
                break;
            if (t != REG_BINARY || nlen != 8 || cb != sizeof(val))
                continue;
            DWORD ymd = std::wcstoul(name, nullptr, 10);
            if (ymd >= 19700101 && ymd <= 99999999)
                rows.push_back({ymd, val});
        }
        RegCloseKey(log);
    }
    RegCloseKey(k);
    std::sort(rows.begin(), rows.end());
    return rows;
}

}  // namespace muyu
