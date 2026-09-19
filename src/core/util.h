#pragma once
// 通用工具：编码转换、日期、缓动函数

#include <windows.h>
#include <string>

namespace muyu {

constexpr double kPi = 3.14159265358979;

std::wstring U8(const char *s);
DWORD TodayYmd();
double EaseOutCubic(double t);
double EaseInOut(double t);

}  // namespace muyu
