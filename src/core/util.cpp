#include "core/util.h"

#include <cmath>

namespace muyu {

std::wstring U8(const char *s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    std::wstring w(n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, -1, &w[0], n);
    return w;
}

DWORD TodayYmd() {
    SYSTEMTIME t;
    GetLocalTime(&t);
    return t.wYear * 10000u + t.wMonth * 100u + t.wDay;
}

double EaseOutCubic(double t) { return 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t); }

double EaseInOut(double t) { return t < .5 ? 2 * t * t : 1 - std::pow(-2 * t + 2, 2) / 2; }

}  // namespace muyu
