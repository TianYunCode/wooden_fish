#pragma once
// 渲染层：把整帧画到 32bpp DIB 再 UpdateLayeredWindow 贴到分层窗口

#include <windows.h>
#include "core/app_state.h"
#include "media/assets.h"

namespace muyu::render {

void Render(HWND hwnd, const AppState &st, media::Assets &assets);
bool AnimActive(const AppState &st);

}  // namespace muyu::render
