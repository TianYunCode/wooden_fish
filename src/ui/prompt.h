#pragma once
// 通用小对话框（纯 Win32 内存模板实现）：数字输入框 / 只读文本列表框

#include <windows.h>

#include <string>

namespace muyu::ui {

// 弹出标题 title、提示 label 的数字输入框；用户确认且数值合法返回 true 并写 out
bool PromptNumber(HWND parent, const wchar_t *title, const wchar_t *label, int defVal, int minV,
                  int maxV, int &out);

// 弹出只读多行文本框（可滚动），点"关闭"或 Esc 退出
void ShowTextDialog(HWND parent, const wchar_t *title, const std::wstring &text);

}  // namespace muyu::ui
