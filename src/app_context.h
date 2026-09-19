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
};

}  // namespace muyu
