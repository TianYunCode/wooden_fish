#pragma once
// 应用上下文：聚合各子系统实例与窗口句柄，贯穿 UI 层传递（经 GWLP_USERDATA 取回）

#include <windows.h>
#include <shellapi.h>

#include "core/app_state.h"
#include "media/assets.h"
#include "media/audio_engine.h"

namespace muyu {

struct AppContext {
    AppState state;
    media::Assets assets;
    media::AudioEngine audio;
    HWND hwnd = nullptr;
    NOTIFYICONDATAW nid = {};
    HICON trayIcon = nullptr;
    bool animOn = false;   // 16ms 动画定时器是否在跑（按需渲染）
    UINT sleepMin = 0;     // 禅定音睡眠定时档位(分钟)，0=关，会话级
    int fadeStep = 0;      // 淡出进度：0..kZenFadeSteps，>0 表示淡出进行中
};

}  // namespace muyu
