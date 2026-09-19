#include "ui/menu.h"

#include <windows.h>
#include <string>

#include "config/layout.h"
#include "core/autorun.h"
#include "core/settings.h"
#include "core/util.h"
#include "render/painter.h"
#include "ui/main_window.h"

namespace muyu::ui {

namespace {

void AddRadio(HMENU m, int base, const char *const *names, int n, int cur) {
    for (int i = 0; i < n; ++i) {
        std::wstring t = U8(names[i]);
        AppendMenuW(m, MF_STRING | (i == cur ? MF_CHECKED : 0), base + i, t.c_str());
    }
}

}  // namespace

void ShowMenu(AppContext &ctx, int x, int y, bool fromTray) {
    HWND hwnd = ctx.hwnd;
    AppState &st = ctx.state;

    const char *sizeNames[] = {"小", "中", "大"};
    const char *autoNames[] = {"关", "慢", "中", "快"};
    const char *volNames[] = {"静音", "小", "中", "大"};
    const char *goalNames[] = {"不设", "27", "54", "108", "216"};
    const char *wordNames[] = {"固定 功德+1", "随机福语"};
    const char *skinNames[] = {"原木", "鎏金", "水墨", "霓虹"};
    const char *zenNames[] = {"关", "开"};

    HMENU menu = CreatePopupMenu();
    if (fromTray) {
        std::wstring shTxt = U8(IsWindowVisible(hwnd) ? "隐藏木鱼" : "显示木鱼");
        AppendMenuW(menu, MF_STRING, IDM_SHOWHIDE, shTxt.c_str());
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    }
    AppendMenuW(menu, MF_STRING | (st.topmost ? MF_CHECKED : 0), IDM_TOPMOST, L"置顶");
    AppendMenuW(menu, MF_STRING | (AutoRunOn() ? MF_CHECKED : 0), IDM_AUTORUN, L"开机自启");
    {
        HMENU s = CreatePopupMenu();
        AddRadio(s, IDM_SIZE_BASE, sizeNames, 3,
                 st.scale < config::kSizeBig * 0.375 ? 0 : st.scale < config::kSizeBig * 0.75 ? 1 : 2);
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(s), L"调节大小");
    }
    {
        HMENU s = CreatePopupMenu();
        AddRadio(s, IDM_AUTO_BASE, autoNames, 4, st.autoIdx);
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(s), L"自动敲击");
    }
    {
        HMENU s = CreatePopupMenu();
        AddRadio(s, IDM_VOL_BASE, volNames, 4, st.volIdx);
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(s), L"音量");
    }
    {
        HMENU s = CreatePopupMenu();
        AddRadio(s, IDM_WORD_BASE, wordNames, 2, st.wordIdx);
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(s), L"飘字");
    }
    {
        HMENU s = CreatePopupMenu();
        AddRadio(s, IDM_GOAL_BASE, goalNames, 5, st.goalIdx);
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(s), L"今日目标");
    }
    {
        HMENU s = CreatePopupMenu();
        AddRadio(s, IDM_SKIN_BASE, skinNames, 4, st.skinIdx);
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(s), L"皮肤");
    }
    {
        HMENU s = CreatePopupMenu();
        AddRadio(s, IDM_ZEN_BASE, zenNames, 2, st.zenIdx);
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(s), L"禅定音");
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_RESET, L"重置功德");
    AppendMenuW(menu, MF_STRING, IDM_QUIT, L"退出");

    SetForegroundWindow(hwnd);
    UINT id = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, x, y, 0, hwnd,
                             nullptr);
    DestroyMenu(menu);
    if (fromTray)
        PostMessageW(hwnd, WM_NULL, 0, 0);  // 修复托盘菜单点击别处不消失

    if (id == IDM_SHOWHIDE) {
        ToggleFish(ctx);
    } else if (id == IDM_TOPMOST) {
        st.topmost = !st.topmost;
        SetWindowPos(hwnd, st.topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        SaveSettings(st, hwnd);
    } else if (id == IDM_AUTORUN) {
        AutoRunSet(!AutoRunOn());
    } else if (id == IDM_RESET) {
        st.merit = 0;
        st.daily = 0;
        render::Render(hwnd, st, ctx.assets);
        SaveSettings(st, hwnd);
    } else if (id >= IDM_SIZE_BASE && id < IDM_SIZE_BASE + 3) {
        ApplyScale(ctx, config::kSizeVals[id - IDM_SIZE_BASE]);
        SaveSettings(st, hwnd);
    } else if (id >= IDM_AUTO_BASE && id < IDM_AUTO_BASE + 4) {
        st.autoIdx = static_cast<int>(id - IDM_AUTO_BASE);
        ApplyAuto(ctx);
        SaveSettings(st, hwnd);
    } else if (id >= IDM_VOL_BASE && id < IDM_VOL_BASE + 4) {
        st.volIdx = static_cast<int>(id - IDM_VOL_BASE);
        SaveSettings(st, hwnd);
    } else if (id >= IDM_WORD_BASE && id < IDM_WORD_BASE + 2) {
        st.wordIdx = static_cast<int>(id - IDM_WORD_BASE);
        SaveSettings(st, hwnd);
    } else if (id >= IDM_GOAL_BASE && id < IDM_GOAL_BASE + 5) {
        st.goalIdx = static_cast<int>(id - IDM_GOAL_BASE);
        render::Render(hwnd, st, ctx.assets);
        SaveSettings(st, hwnd);
    } else if (id >= IDM_SKIN_BASE && id < IDM_SKIN_BASE + 4) {
        st.skinIdx = static_cast<int>(id - IDM_SKIN_BASE);
        render::Render(hwnd, st, ctx.assets);
        SaveSettings(st, hwnd);
    } else if (id >= IDM_ZEN_BASE && id < IDM_ZEN_BASE + 2) {
        st.zenIdx = static_cast<int>(id - IDM_ZEN_BASE);
        ctx.audio.ApplyZen(st.zenIdx != 0);
        SaveSettings(st, hwnd);
    } else if (id == IDM_QUIT) {
        DestroyWindow(hwnd);
    }
}

}  // namespace muyu::ui
