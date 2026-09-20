// 电子木鱼 —— 纯 Win32 API + GDI+ 实现，无第三方框架
// 分层：config(常量) / core(状态·设置·自启) / media(素材·音频) / render(绘制) / ui(窗口·菜单)
// 素材与音效以 RCDATA 嵌入 exe，启动时解码常驻内存

#include <windows.h>
#include <commctrl.h>
#include <objidl.h>
#include <gdiplus.h>
#include <mfapi.h>
#include <cstdio>
#include <cstdlib>

#include "app_context.h"
#include "core/settings.h"
#include "render/painter.h"
#include "ui/main_window.h"
#include "ui/menu_icons.h"


int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    using namespace muyu;
    namespace ui = muyu::ui;

    Gdiplus::GdiplusStartupInput gi;
    ULONG_PTR gdiToken = 0;
    Gdiplus::GdiplusStartup(&gdiToken, &gi, nullptr);
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&icc);  // 功德簿的报表视图列表控件
    // 优先 Per-Monitor v2（Win10 1703+，动态加载防旧系统缺导出），失败退回系统级感知
    if (HMODULE u = GetModuleHandleW(L"user32.dll")) {
        auto setCtx = reinterpret_cast<BOOL(WINAPI *)(HANDLE)>(
            GetProcAddress(u, "SetProcessDpiAwarenessContext"));
        if (!setCtx || !setCtx(reinterpret_cast<HANDLE>(-4)))  // -4 = PER_MONITOR_AWARE_V2
            SetProcessDPIAware();
    } else {
        SetProcessDPIAware();
    }

    AppContext ctx;
    ctx.state.dpi = GetDpiForSystem() / 96.0;
    LoadSettings(ctx.state);

    if (!ctx.assets.Load()) {
        Gdiplus::GdiplusShutdown(gdiToken);
        return 1;
    }

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION);
    ctx.audio.Init();                       // 解码+预热完成后才显示 UI，杜绝首击延迟
    ctx.audio.SetVolume(ctx.state.volIdx);  // 恢复音量档（敲击+禅定共用）
    if (ctx.state.zenIdx == config::kZenCustomIdx &&
        GetFileAttributesW(ctx.state.zenFile.c_str()) == INVALID_FILE_ATTRIBUTES)
        ctx.state.zenIdx = 0;  // 自定义文件已丢失则视为关
    ctx.audio.ApplyZen(ctx.state.zenIdx, ctx.state.zenFile);  // 恢复上次会话的禅定音

    ui::Create(ctx, hInst);
    if (!ctx.hwnd) {
        ctx.audio.Shutdown();
        MFShutdown();
        CoUninitialize();
        ctx.assets.Free();
        Gdiplus::GdiplusShutdown(gdiToken);
        return 1;
    }
    ui::AddTrayIcon(ctx, hInst);

    std::srand(static_cast<unsigned>(GetTickCount()));
    ui::ApplyAuto(ctx);                                // 恢复自动敲击
    RegisterHotKey(ctx.hwnd, 1, MOD_NOREPEAT, VK_F8);  // 全局热键：隔空敲一记
    render::Render(ctx.hwnd, ctx.state, ctx.assets);
    ui::SyncAnim(ctx);  // 动画定时器按需挂载（静止时不烧 CPU）
    ShowWindow(ctx.hwnd, SW_SHOW);
    UpdateWindow(ctx.hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    KillTimer(ctx.hwnd, ui::kTimerAnim);
    KillTimer(ctx.hwnd, ui::kTimerAuto);
    KillTimer(ctx.hwnd, ui::kTimerSleep);
    KillTimer(ctx.hwnd, ui::kTimerFade);
    UnregisterHotKey(ctx.hwnd, 1);
    SaveSettings(ctx.state, ctx.hwnd);
    ui::RemoveTrayIcon(ctx);
    ui::FreeMenuIcons();
    ctx.audio.Shutdown();
    MFShutdown();
    CoUninitialize();
    ctx.assets.Free();
    Gdiplus::GdiplusShutdown(gdiToken);
    return 0;
}
