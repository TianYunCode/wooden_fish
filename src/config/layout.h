#pragma once
// 静态配置：布局几何、动画时间轴、内容表（档位/文案/目标）。全部为编译期常量。

#include <windows.h>
#include <gdiplus.h>

namespace muyu::config {

// ---- 基准几何：按 mp-muyu-master 的 rpx 布局换算(鱼图像460rpx→330px，1rpx=0.7174px)
// 摆放按参考(静止姿态不变)，敲击动作绕柄端挥动，见 kPivot ----
inline constexpr double kW = 545, kH = 455;  // 棒槌右移100后加宽窗口防裁剪
inline constexpr double kFishLeft = 25, kFishTop = 85, kFishW = 330, kFishH = 330;
inline constexpr double kFishCx = 190, kFishCy = 250;
inline constexpr double kHitX = 252, kHitY = 187;  // 波纹中心(用户微调)
inline constexpr double kStickSize = 258.3;        // 360rpx
inline constexpr double kStickX = 268.5, kStickY = 42;  // 用户指定: 右移100px
// 敲击绕柄端(gu.png 约(0.857,0.857)处)挥动：槌头划大弧真实砸向鱼面
inline constexpr double kPivotX = kStickX + 0.857 * kStickSize;
inline constexpr double kPivotY = kStickY + 0.857 * kStickSize;
inline constexpr double kRestDeg = 0, kHitDeg = -35;

// ---- 动画时间轴(ms)：下挥90(急)→回弹220(缓)；下挥尽头即触鱼时刻，声音/受压/波纹此刻才起 ----
inline constexpr ULONGLONG kSwingMs = 90, kBackMs = 310, kSquashMs = 220, kRippleMs = 420,
                           kFloatMs = 1000;

// ---- 尺寸档：大=鱼素材原始 480px 满幅(基准绘制宽 330px)，中=一半，小=一半的一半 ----
inline constexpr double kSizeBig = 480.0 / 330.0;
inline constexpr double kSizeVals[3] = {kSizeBig / 4.0, kSizeBig / 2.0, kSizeBig};
inline constexpr double kScaleDefault = kSizeBig / 2.0;  // 默认“中”

// ---- 内容表 ----
inline constexpr float kVolLv[4] = {0.0f, 0.35f, 0.7f, 1.0f};  // 静音/小/中/大
inline constexpr UINT kAutoMs[4] = {0, 1500, 800, 400};        // 自动敲击间隔：关/慢/中/快
inline constexpr unsigned kGoals[5] = {0, 27, 54, 108, 216};   // 每日目标(佛教数)
inline constexpr int kGoalMenuCount = 6;  // 5 档预设 + 自定义…
inline constexpr int kWordCount = 10;
inline constexpr const char *kWords[kWordCount] = {
    "功德 +1", "佛系 +1", "智慧 +1", "平静 +1", "好运 +1",
    "慈悲 +1", "欢喜 +1", "自在 +1", "清净 +1", "正能量 +1"};

// 禅定音：0=关，1..kZenTrackCount 选曲（全部 Kevin MacLeod, CC-BY 4.0），kZenTrackCount+1=本地文件
inline constexpr int kZenTrackCount = 5;
inline constexpr int kZenCustomIdx = kZenTrackCount + 1;
inline constexpr int kZenMenuCount = kZenTrackCount + 2;  // 含"关"与"本地文件…"
inline constexpr const char *kZenNames[kZenMenuCount] = {
    "关", "禅意即兴·钢琴弦乐", "清新空气·钢琴独奏", "卡林巴·拇指琴",
    "溪流竹笛·流水衬底", "白莲·梵呗唱钵", "本地音频文件…"};
inline constexpr ULONGLONG kZenMaxMs = 10 * 60 * 1000;  // 本地文件超长截断，控内存

inline constexpr ULONGLONG kComboWindowMs = 1500;  // 连击判定窗口
inline constexpr int kCritCombos[3] = {10, 30, 50};
inline constexpr int kMaxFloats = 24;

}  // namespace muyu::config
