#include "ui/main_window.h"

#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "config/layout.h"
#include "core/settings.h"
#include "core/util.h"
#include "render/painter.h"
#include "ui/menu.h"
#include "ui/menu_icons.h"

namespace muyu::ui {

namespace {

AppContext *FromHwnd(HWND hwnd) {
    return reinterpret_cast<AppContext *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

}  // namespace

void Create(AppContext &ctx, HINSTANCE hInst) {
    AppState &st = ctx.state;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"WoodenFishWnd";
    if (!RegisterClassExW(&wc))
        return;

    RECT wa{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int w = static_cast<int>(config::kW * st.EffScale() + 0.5);
    int h = static_cast<int>(config::kH * st.EffScale() + 0.5);
    // 每次启动都放屏幕工作区右下角（不记忆位置）
    int px = wa.right - w - 40;
    int py = wa.bottom - h - 40;
    ctx.hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, wc.lpszClassName,
                               U8("电子木鱼").c_str(), WS_POPUP, px, py, w, h, nullptr, nullptr,
                               hInst, &ctx);
    if (!ctx.hwnd)
        return;
    // Per-Monitor v2：初始尺寸按系统 DPI 算的，创建后校正为所在显示器的真实 DPI
    if (HMODULE u = GetModuleHandleW(L"user32.dll")) {
        auto getDpiW = reinterpret_cast<UINT(WINAPI *)(HWND)>(
            GetProcAddress(u, "GetDpiForWindow"));
        UINT d = getDpiW ? getDpiW(ctx.hwnd) : 0;
        double nd = d / 96.0;
        if (nd > 0 && (nd > st.dpi + 0.01 || nd < st.dpi - 0.01)) {
            st.dpi = nd;
            w = static_cast<int>(config::kW * st.EffScale() + 0.5);
            h = static_cast<int>(config::kH * st.EffScale() + 0.5);
            SetWindowPos(ctx.hwnd, nullptr, px, py, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
    if (!st.topmost)
        SetWindowPos(ctx.hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void AddTrayIcon(AppContext &ctx, HINSTANCE hInst) {
    ctx.trayIcon = static_cast<HICON>(
        LoadImageW(hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                   GetSystemMetrics(SM_CYSMICON), 0));
    ctx.nid.cbSize = sizeof(ctx.nid);
    ctx.nid.hWnd = ctx.hwnd;
    ctx.nid.uID = 1;
    ctx.nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    ctx.nid.uCallbackMessage = WM_TRAYICON;
    ctx.nid.hIcon = ctx.trayIcon;
    lstrcpynW(ctx.nid.szTip, U8("电子木鱼").c_str(), 128);
    Shell_NotifyIconW(NIM_ADD, &ctx.nid);
}

void RemoveTrayIcon(AppContext &ctx) {
    Shell_NotifyIconW(NIM_DELETE, &ctx.nid);
    if (ctx.trayIcon) {
        DestroyIcon(ctx.trayIcon);
        ctx.trayIcon = nullptr;
    }
}

// 槌头触鱼：此刻才发声、缩放、波纹、飘字、计数（由起挥后的 kSwingMs 延迟触发）
void Strike(AppContext &ctx) {
    AppState &st = ctx.state;
    st.impactPending = false;
    ULONGLONG now = GetTickCount64();
    st.impactAt = now;
    ++st.merit;
    if (st.dailyDate != TodayYmd()) {
        st.dailyDate = TodayYmd();
        st.daily = 0;
    }
    ++st.daily;
    st.combo = (now - st.comboAt < config::kComboWindowMs) ? st.combo + 1 : 1;
    st.comboAt = now;
    bool crit = st.combo == 10 || st.combo == 30 || st.combo == 50;
    if (st.wordIdx != 2) {  // 飘字档位 2 = 关闭：不产生任何飘字（含连击金字）
        std::wstring word = st.wordIdx == 0 ? U8("功德 +1")
                                            : U8(config::kWords[std::rand() % config::kWordCount]);
        st.floats.push_back({st.impactX, st.impactY, now, word, crit});
        if (crit)
            st.floats.push_back({st.impactX, st.impactY + 26, now,
                                 U8("连击 x") + std::to_wstring(st.combo), true});
    }
    if (st.floats.size() > config::kMaxFloats)
        st.floats.erase(st.floats.begin(),
                        st.floats.begin() + (static_cast<int>(st.floats.size()) - config::kMaxFloats));
    ctx.audio.PlayKnock(st.combo);
    render::Render(ctx.hwnd, st, ctx.assets);
    SaveSettings(st, ctx.hwnd);
}

// 起挥：棒槌先下挥 kSwingMs，触鱼时刻由动画定时器结算给 Strike
void DoKnock(AppContext &ctx, double x, double y) {
    AppState &st = ctx.state;
    if (st.impactPending)
        Strike(ctx);  // 极速连点：先把上一击结算掉
    st.knockAt = GetTickCount64();
    st.impactPending = true;
    st.impactX = x;
    st.impactY = y;
    render::Render(ctx.hwnd, st, ctx.assets);
    SyncAnim(ctx);  // 有活了才挂 16ms 动画定时器
}

void ApplyScale(AppContext &ctx, double s) {
    AppState &st = ctx.state;
    st.scale = s;
    RECT wr;
    GetWindowRect(ctx.hwnd, &wr);
    int nw = static_cast<int>(config::kW * st.EffScale() + 0.5);
    int nh = static_cast<int>(config::kH * st.EffScale() + 0.5);
    SetWindowPos(ctx.hwnd, HWND_TOPMOST, wr.left, wr.bottom - nh, nw, nh, SWP_NOACTIVATE);
    render::Render(ctx.hwnd, st, ctx.assets);
}

void ApplyAuto(AppContext &ctx) {
    KillTimer(ctx.hwnd, kTimerAuto);
    if (ctx.state.autoIdx > 0 && ctx.state.autoIdx <= 3)
        SetTimer(ctx.hwnd, kTimerAuto, config::kAutoMs[ctx.state.autoIdx], nullptr);
}

// 按需渲染：只在动画（挥槌/飘字/达成呼吸）进行中挂 16ms 定时器，静止即摘除
void SyncAnim(AppContext &ctx) {
    bool need = render::AnimActive(ctx.state);
    if (need && !ctx.animOn) {
        SetTimer(ctx.hwnd, kTimerAnim, 16, nullptr);
        ctx.animOn = true;
    } else if (!need && ctx.animOn) {
        KillTimer(ctx.hwnd, kTimerAnim);
        ctx.animOn = false;
    }
}

void ToggleFish(AppContext &ctx) {
    if (IsWindowVisible(ctx.hwnd)) {
        ShowWindow(ctx.hwnd, SW_HIDE);
    } else {
        ShowWindow(ctx.hwnd, SW_SHOWNA);
        render::Render(ctx.hwnd, ctx.state, ctx.assets);
        UpdateWindow(ctx.hwnd);
        SyncAnim(ctx);
    }
}

namespace {

// ---- 定时器分发：一个定时器一个分支，WndProc 只做路由 ----

void OnAutoTimer(AppContext &ctx) {
    if (ctx.state.autoIdx)
        DoKnock(ctx, config::kHitX + 15, config::kHitY - 25);
}

// 睡眠倒计时到点：禅定音开着才开始 5s 淡出
void OnSleepTimer(AppContext &ctx) {
    KillTimer(ctx.hwnd, kTimerSleep);
    if (ctx.state.zenIdx) {
        ctx.fadeStep = 0;
        SetTimer(ctx.hwnd, kTimerFade, 100, nullptr);
    }
}

// 淡出每 100ms 降一档增益，归零即停机并落盘
void OnFadeTimer(AppContext &ctx) {
    AppState &st = ctx.state;
    ++ctx.fadeStep;
    double f = 1.0 - static_cast<double>(ctx.fadeStep) / config::kZenFadeSteps;
    if (f > 0.0) {
        ctx.audio.SetZenFade(f);
        return;
    }
    KillTimer(ctx.hwnd, kTimerFade);
    ctx.fadeStep = 0;
    ctx.sleepMin = 0;
    ctx.audio.ApplyZen(0);
    st.zenIdx = 0;
    SaveSettings(st, ctx.hwnd);
}

// 16ms 动画帧：先结算到点的触鱼，再清理过期飘字，最后渲染或收尾摘表
void OnAnimTimer(AppContext &ctx) {
    AppState &st = ctx.state;
    HWND hwnd = ctx.hwnd;
    if (st.impactPending && GetTickCount64() - st.knockAt >= config::kSwingMs)
        Strike(ctx);  // 槌头落到位：此刻发声并触发鱼身效果
    st.floats.erase(std::remove_if(st.floats.begin(), st.floats.end(),
                                   [&](const FloatText &f) {
                                       return GetTickCount64() - f.born > config::kFloatMs;
                                   }),
                    st.floats.end());
    if (!render::AnimActive(st)) {  // 动画结束：摘除定时器，补画最后一帧收尾
        KillTimer(hwnd, kTimerAnim);
        ctx.animOn = false;
    }
    if (IsWindowVisible(hwnd))
        render::Render(hwnd, st, ctx.assets);
}

LRESULT OnTimer(AppContext &ctx, UINT_PTR id) {
    switch (id) {
    case kTimerAuto:  OnAutoTimer(ctx);  return 0;
    case kTimerSleep: OnSleepTimer(ctx); return 0;
    case kTimerFade:  OnFadeTimer(ctx);  return 0;
    default:          OnAnimTimer(ctx);  return 0;
    }
}

}  // namespace

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto *cs = reinterpret_cast<CREATESTRUCTW *>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    AppContext *pctx = FromHwnd(hwnd);
    if (!pctx)
        return DefWindowProcW(hwnd, msg, wp, lp);
    AppState &st = pctx->state;

    switch (msg) {
    case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        st.captured = true;
        st.dragging = false;
        st.lastX = GET_X_LPARAM(lp);
        st.lastY = GET_Y_LPARAM(lp);
        return 0;
    case WM_MOUSEMOVE:
        if (st.captured && (wp & MK_LBUTTON)) {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            if (!st.dragging && !st.pinned &&
                (std::abs(x - st.lastX) + std::abs(y - st.lastY) > 4))
                st.dragging = true;  // "固定"勾选后禁止拖动，点击仍正常敲击
            if (st.dragging) {
                RECT wr;
                GetWindowRect(hwnd, &wr);
                SetWindowPos(hwnd, nullptr, wr.left + x - st.lastX, wr.top + y - st.lastY, 0, 0,
                             SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
        return 0;
    case WM_LBUTTONUP:
        if (st.captured) {
            ReleaseCapture();
            // 按住期间未拖动才在抬起时起挥；拖过窗口只算移动，不敲
            if (!st.dragging)
                DoKnock(*pctx, GET_X_LPARAM(lp) / st.EffScale(),
                        GET_Y_LPARAM(lp) / st.EffScale());
        }
        st.captured = st.dragging = false;
        return 0;
    case WM_TRAYICON: {
        UINT ev = LOWORD(lp);
        if (ev == WM_LBUTTONUP || ev == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            ShowMenu(*pctx, pt.x, pt.y, true);
        } else if (ev == WM_LBUTTONDBLCLK) {
            ToggleFish(*pctx);
        }
        return 0;
    }
    case WM_RBUTTONUP: {
        POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        ClientToScreen(hwnd, &pt);
        ShowMenu(*pctx, pt.x, pt.y, false);
        return 0;
    }
    case WM_TIMER:
        return OnTimer(*pctx, static_cast<UINT_PTR>(wp));
    case WM_DPICHANGED: {  // 跨显示器/改缩放：按系统建议矩形重设窗口
        st.dpi = HIWORD(wp) / 96.0;
        RECT *sug = reinterpret_cast<RECT *>(lp);
        SetWindowPos(hwnd, nullptr, sug->left, sug->top, sug->right - sug->left,
                     sug->bottom - sug->top, SWP_NOZORDER | SWP_NOACTIVATE);
        render::Render(hwnd, st, pctx->assets);
        return 0;
    }
    case WM_MEASUREITEM:
        if (OnMeasureMenu(reinterpret_cast<LPMEASUREITEMSTRUCT>(lp), st.dpi))
            return TRUE;
        break;
    case WM_DRAWITEM:
        OnDrawMenu(reinterpret_cast<LPDRAWITEMSTRUCT>(lp), st.dpi);
        return TRUE;
    case WM_HOTKEY:
        if (wp == 1)
            DoKnock(*pctx, config::kFishCx, config::kFishCy - 60);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace muyu::ui
