#include "core/settings.h"

#include "config/layout.h"
#include "core/util.h"

namespace muyu {

namespace {
const wchar_t *kRegPath = L"Software\\WoodenFish";
}

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
    u32(L"vol", v);
    if (v <= 3) st.volIdx = static_cast<int>(v);
    u32(L"auto", v);
    if (v <= 3) st.autoIdx = static_cast<int>(v);
    u32(L"goal", v);
    if (v <= 4) st.goalIdx = static_cast<int>(v);
    u32(L"word", v);
    if (v <= 1) st.wordIdx = static_cast<int>(v);
    u32(L"skin", v);
    if (v <= 3) st.skinIdx = static_cast<int>(v);
    u32(L"zen", v);
    if (v <= 1) st.zenIdx = static_cast<int>(v);
    DWORD px = 0xFFFFFFFF, py = 0xFFFFFFFF;
    u32(L"x", px);
    u32(L"y", py);
    if (px != 0xFFFFFFFF && py != 0xFFFFFFFF) {
        st.startX = static_cast<int>(px);
        st.startY = static_cast<int>(py);
    }
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
    w32(L"vol", st.volIdx);
    w32(L"auto", st.autoIdx);
    w32(L"goal", st.goalIdx);
    w32(L"word", st.wordIdx);
    w32(L"skin", st.skinIdx);
    w32(L"zen", st.zenIdx);
    if (hwnd) {
        RECT wr;
        GetWindowRect(hwnd, &wr);
        w32(L"x", static_cast<DWORD>(wr.left));
        w32(L"y", static_cast<DWORD>(wr.top));
    }
    RegCloseKey(k);
}

}  // namespace muyu
