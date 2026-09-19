#pragma once
// 应用核心状态：全部可变数据集中于此，由 UI 层持有并传递，各子系统只操作引用

#include <windows.h>
#include <string>
#include <vector>

#include "config/layout.h"

namespace muyu {

// 一条漂浮文字
struct FloatText {
    double x, y;
    ULONGLONG born;
    std::wstring word;
    bool gold = false;
};

struct AppState {
    // 功德
    unsigned long long merit = 0;
    unsigned long long daily = 0;   // 今日功德
    DWORD dailyDate = 0;            // yyyymmdd
    // 用户设置
    double scale = 1.0;             // 启动时被 config 默认档覆盖
    bool topmost = true;            // 默认置顶
    int volIdx = 2;                 // 0静音 1小 2中 3大
    int autoIdx = 0;                // 0关 1慢 2中 3快
    int goalIdx = 0;                // 0不设 1:27 2:54 3:108 4:216 5:自定义
    unsigned goalCustom = 108;      // goalIdx==5 时生效
    int wordIdx = 1;                // 0固定功德+1 1随机福语
    int skinIdx = 0;                // 0原木 1鎏金 2水墨 3霓虹
    int zenIdx = 0;                 // 禅定音 0关 1..5曲目 6:本地文件
    std::wstring zenFile;           // zenIdx==6 时的音频文件路径
    // 动画/交互瞬态
    double dpi = 1.0;
    ULONGLONG knockAt = 0;          // 最近一次起挥（棒槌开始下挥）时刻
    ULONGLONG impactAt = 0;         // 槌头触鱼时刻：声音/缩放/波纹/飘字以此为起点
    bool impactPending = false;     // 下挥进行中，尚未触鱼
    double impactX = 0, impactY = 0;  // 待触鱼这一击的飘字锚点
    int combo = 0;                  // 1.5s 内连击计数
    ULONGLONG comboAt = 0;
    std::vector<FloatText> floats;
    bool captured = false, dragging = false;
    int lastX = 0, lastY = 0;

    double EffScale() const { return scale * dpi; }
    unsigned GoalTotal() const {
        if (goalIdx == 0) return 0;
        return goalIdx <= 4 ? config::kGoals[goalIdx] : goalCustom;
    }
};

}  // namespace muyu
