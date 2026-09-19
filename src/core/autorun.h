#pragma once
// 开机自启：HKCU\...\Run 键值的存在与否即勾选状态

namespace muyu {

bool AutoRunOn();
void AutoRunSet(bool on);

}  // namespace muyu
