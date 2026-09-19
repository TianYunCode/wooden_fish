#pragma once
// 通用小输入框：数值型（今日目标自定义用），纯 Win32 内存对话框模板实现

#include <windows.h>

namespace muyu::ui {

// 弹出标题 title、提示 label 的数字输入框；用户确认且数值合法返回 true 并写 out
bool PromptNumber(HWND parent, const wchar_t *title, const wchar_t *label, int defVal, int minV,
                  int maxV, int &out);

}  // namespace muyu::ui
