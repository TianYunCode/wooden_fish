#pragma once
// 通用小对话框（纯 Win32 内存模板实现）：数字输入框 / 只读文本列表框

#include <windows.h>

#include <string>
#include <utility>
#include <vector>

namespace muyu::ui {

// 弹出标题 title、提示 label 的数字输入框；用户确认且数值合法返回 true 并写 out
bool PromptNumber(HWND parent, const wchar_t *title, const wchar_t *label, int defVal, int minV,
                  int maxV, int &out);

// 只读多行文本框弹窗（关于等共用），DLU 尺寸可调，超出滚动
void ShowTextDialog(HWND parent, const wchar_t *title, const std::wstring &text, int w = 264,
                    int h = 172);

// 功德簿专用弹窗：日期/当日功德两列表格，斑马纹 + 千分位 + 今日标记 + 合计行
// rows 为 (yyyymmdd, 当日功德)，按日期升序
void ShowLedgerDialog(HWND parent,
                      const std::vector<std::pair<DWORD, unsigned long long>> &rows);

}  // namespace muyu::ui
