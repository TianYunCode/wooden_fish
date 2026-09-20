#pragma once
// 菜单 owner-draw 基建：从 RCDATA("ic_<key>") 加载 Lucide 线性图标，
// 处理 WM_MEASUREITEM / WM_DRAWITEM，实现"图标 + 文字 + ✓"的现代风格菜单
// 图标来源：Lucide 图标库（ISC 许可），经 Iconify API 以 #303030 渲染为 48px PNG

#include <windows.h>
#include <string>

namespace muyu::ui {

// 每个菜单项的绘制数据，指针存进 MENUITEMINFO.dwItemData；
// 由 ShowMenu 持有一个容器贯穿菜单生命周期（TrackPopupMenu 是模态的，返回即销毁）
struct MenuDrawData {
    std::wstring text;  // 显示文字
    std::wstring icon;  // 图标 key（对应 RCDATA "ic_<key>"），空则不画图标
    bool sub = false;   // 是否弹出式父项（右侧为系统绘制的箭头预留宽度）
};

// 返回 true 表示已按"图标列 + 文字 + 子菜单箭头"量好尺寸
bool OnMeasureMenu(LPMEASUREITEMSTRUCT ms, double dpi);
void OnDrawMenu(LPDRAWITEMSTRUCT dis, double dpi);
void FreeMenuIcons();

}  // namespace muyu::ui
