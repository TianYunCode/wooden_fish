#pragma once
// 主窗口：分层窗口创建、托盘图标、敲击/拖动/定时器/热键等消息编排

#include "app_context.h"

namespace muyu::ui {

constexpr UINT WM_TRAYICON = WM_APP + 1;
constexpr UINT_PTR kTimerAnim = 1;  // 16ms 动画帧
constexpr UINT_PTR kTimerAuto = 2;  // 自动敲击

// 注册窗口类、创建分层窗口并加托盘图标；失败时 hwnd 保持 nullptr
void Create(AppContext &ctx, HINSTANCE hInst);
void AddTrayIcon(AppContext &ctx, HINSTANCE hInst);
void RemoveTrayIcon(AppContext &ctx);

void DoKnock(AppContext &ctx, double x, double y);  // (x,y) 为基准坐标系
void ApplyScale(AppContext &ctx, double s);
void ApplyAuto(AppContext &ctx);
void ToggleFish(AppContext &ctx);

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

}  // namespace muyu::ui
