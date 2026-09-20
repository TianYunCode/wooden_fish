#pragma once
// 右键/托盘菜单：构建与响应，纯文字无图标

#include "app_context.h"

namespace muyu::ui {

enum MenuId : WORD {
    IDM_RESET = 101,
    IDM_QUIT = 103,
    IDM_SHOWHIDE = 104,
    IDM_TOPMOST = 105,
    IDM_AUTORUN = 106,
    IDM_LEDGER = 107,  // 功德簿（查看每日最终功德）
    IDM_PIN = 108,     // 固定（勾选后左键拖动无效，防误碰移位）
    IDM_ABOUT = 109,   // 关于（版本/致谢/战绩）
    IDM_SIZE_BASE = 201,  // 小/中/大
    IDM_AUTO_BASE = 211,  // 关/慢/中/快
    IDM_VOL_BASE = 221,   // 静音/小/中/大
    IDM_GOAL_BASE = 231,  // 不设/27/54/108/216
    IDM_WORD_BASE = 241,  // 固定/随机
    IDM_SKIN_BASE = 251,  // 原木/鎏金/水墨/霓虹
    IDM_ZEN_BASE = 261,   // 关/开
    IDM_SLEEP_BASE = 271, // 睡眠定时：关/15/30/45/60/90 分钟
};

// 在屏幕坐标 (x,y) 弹出菜单并处理选择；fromTray=true 表示由托盘图标唤起
void ShowMenu(AppContext &ctx, int x, int y, bool fromTray);

}  // namespace muyu::ui
