#pragma once
// 注册表持久化：HKCU\Software\WoodenFish（功德簿在其 log 子键）

#include "core/app_state.h"

#include <utility>
#include <vector>

namespace muyu {

void LoadSettings(AppState &st);
void SaveSettings(const AppState &st, HWND hwnd);

// 功德簿：每日最终功德，(yyyymmdd, 当日功德)，按日期升序返回
std::vector<std::pair<DWORD, unsigned long long>> ReadLedger();

}  // namespace muyu
