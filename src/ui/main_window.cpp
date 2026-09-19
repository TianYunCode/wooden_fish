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

// 槌头触鱼：此刻才发声、缩放、波纹、飘字、计数（由点击后的 kSwingMs 延迟触发）
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
    std::wstring word =
        st.wordIdx == 0 ? U8("功德 +1") : U8(config::kWords[std::rand() % config::kWordCount]);
    bool crit = st.combo == 10 || st.combo == 30 || st.combo == 50;
    st.floats.push_back({st.impactX, st.impactY, now, word, crit});
    if (crit)
        st.floats.push_back({st.impactX, st.impactY + 26, now, U8("连击 x") + std::to_wstring(st.combo), true});
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

void ToggleFish(AppContext &ctx) {
    if (IsWindowVisible(ctx.hwnd)) {
        ShowWindow(ctx.hwnd, SW_HIDE);
    } else {
        ShowWindow(ctx.hwnd, SW_SHOWNA);
        render::Render(ctx.hwnd, ctx.state, ctx.assets);
        UpdateWindow(ctx.hwnd);
    }
}

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
        DoKnock(*pctx, st.lastX / st.EffScale(), st.lastY / st.EffScale());
        return 0;
    case WM_MOUSEMOVE:
        if (st.captured && (wp & MK_LBUTTON)) {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            if (!st.dragging && (std::abs(x - st.lastX) + std::abs(y - st.lastY) > 4))
                st.dragging = true;
            if (st.dragging) {
                RECT wr;
                GetWindowRect(hwnd, &wr);
                SetWindowPos(hwnd, nullptr, wr.left + x - st.lastX, wr.top + y - st.lastY, 0, 0,
                             SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
        return 0;
    case WM_LBUTTONUP:
        if (st.captured)
            ReleaseCapture();
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
        if (wp == kTimerAuto) {  // 自动敲击
            if (st.autoIdx)
                DoKnock(*pctx, config::kHitX + 15, config::kHitY - 25);
            return 0;
        }
        if (st.impactPending && GetTickCount64() - st.knockAt >= config::kSwingMs)
            Strike(*pctx);  // 槌头落到位：此刻发声并触发鱼身效果
        st.floats.erase(std::remove_if(st.floats.begin(), st.floats.end(),
                                       [&](const FloatText &f) {
                                           return GetTickCount64() - f.born > config::kFloatMs;
                                       }),
                        st.floats.end());
        if (render::AnimActive(st))
            render::Render(hwnd, st, pctx->assets);
        return 0;
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
