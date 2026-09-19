#pragma once
// 注册表持久化：HKCU\Software\WoodenFish

#include "core/app_state.h"

namespace muyu {

void LoadSettings(AppState &st);
void SaveSettings(const AppState &st, HWND hwnd);

}  // namespace muyu
