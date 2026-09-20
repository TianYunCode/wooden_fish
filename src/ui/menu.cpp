#include "ui/menu.h"

#include <windows.h>
#include <commdlg.h>
#include <cstdio>
#include <cwchar>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "config/layout.h"
#include "core/autorun.h"
#include "core/settings.h"
#include "core/util.h"
#include "render/painter.h"
#include "ui/main_window.h"
#include "ui/menu_icons.h"
#include "ui/prompt.h"

namespace muyu::ui {

namespace {

// owner-draw 项的绘制数据（文字 + 图标 key + 是否子菜单）集中存放在这里，
// 指针写进 MENUITEMINFO.dwItemData；ShowMenu 每次开头 clear，模态 TrackPopupMenu 期间保持有效
std::vector<std::unique_ptr<MenuDrawData>> &DrawStore() {
    static std::vector<std::unique_ptr<MenuDrawData>> store;
    return store;
}

void AddMI(HMENU m, UINT_PTR id, bool popup, const wchar_t *text, const wchar_t *icon,
           bool checked = false) {
    AppendMenuW(m, (popup ? MF_POPUP : MF_STRING) | MF_OWNERDRAW | (checked ? MF_CHECKED : 0), id,
                text);
    auto d = std::make_unique<MenuDrawData>();
    d->text = text;
    d->icon = icon ? icon : L"";
    d->sub = popup;
    MENUITEMINFOW mii{};
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_DATA;
    mii.dwItemData = reinterpret_cast<ULONG_PTR>(d.get());
    SetMenuItemInfoW(m, GetMenuItemCount(m) - 1, MF_BYPOSITION, &mii);
    DrawStore().push_back(std::move(d));
}

void AddRadio(HMENU m, int base, const char *const *names, const wchar_t *const *icons, int n,
              int cur) {
    for (int i = 0; i < n; ++i) {
        std::wstring t = U8(names[i]);
        AddMI(m, base + i, false, t.c_str(), icons[i], i == cur);
    }
}

}  // namespace

void ShowMenu(AppContext &ctx, int x, int y, bool fromTray) {
    HWND hwnd = ctx.hwnd;
    AppState &st = ctx.state;

    const char *sizeNames[] = {"小", "中", "大"};
    const wchar_t *sizeIcons[] = {L"shrink", L"expand", L"maximize"};
    const char *autoNames[] = {"关", "慢", "中", "快"};
    const wchar_t *autoIcons[] = {L"ban", L"turtle", L"footprints", L"rabbit"};
    const char *volNames[] = {"静音", "小", "中", "大"};
    const wchar_t *volIcons[] = {L"volume-x", L"volume-1", L"volume", L"volume-2"};
    const char *goalNames[] = {"不设", "27", "54", "108", "216", "自定义…"};
    const wchar_t *goalIcons[] = {L"circle-slash", L"target", L"target",
                                  L"target",       L"target", L"pencil-line"};
    const char *wordNames[] = {"固定 功德+1", "随机福语", "关闭"};
    const wchar_t *wordIcons[] = {L"type", L"shuffle", L"circle-slash"};
    const char *skinNames[] = {"原木", "鎏金", "水墨", "霓虹"};
    const wchar_t *skinIcons[] = {L"tree-deciduous", L"crown", L"brush", L"zap"};
    const wchar_t *zenIcons[] = {L"ban",       L"piano",   L"music-2", L"hand-metal",
                                 L"wind",      L"flower-2", L"scroll-text", L"file-audio"};
    const char *sleepNames[] = {"关", "15 分钟", "30 分钟", "45 分钟", "60 分钟", "90 分钟"};
    const wchar_t *sleepIcons[] = {L"ban", L"timer", L"timer", L"timer", L"timer", L"timer"};

    DrawStore().clear();

    HMENU menu = CreatePopupMenu();
    if (fromTray) {
        bool vis = IsWindowVisible(hwnd) != FALSE;
        AddMI(menu, IDM_SHOWHIDE, false, U8(vis ? "隐藏木鱼" : "显示木鱼").c_str(),
              vis ? L"eye-off" : L"eye");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    }
    AddMI(menu, IDM_TOPMOST, false, L"置顶", L"pin", st.topmost);
    AddMI(menu, IDM_PIN, false, L"固定", L"lock", st.pinned);
    AddMI(menu, IDM_AUTORUN, false, L"开机自启", L"calendar-clock", AutoRunOn());
    {
        HMENU s = CreatePopupMenu();
        AddRadio(s, IDM_SIZE_BASE, sizeNames, sizeIcons, 3,
                 st.scale < config::kSizeBig * 0.375 ? 0 : st.scale < config::kSizeBig * 0.75 ? 1 : 2);
        AddMI(menu, reinterpret_cast<UINT_PTR>(s), true, L"调节大小", L"scaling");
    }
    {
        HMENU s = CreatePopupMenu();
        AddRadio(s, IDM_AUTO_BASE, autoNames, autoIcons, 4, st.autoIdx);
        AddMI(menu, reinterpret_cast<UINT_PTR>(s), true, L"自动敲击", L"bot");
    }
    {
        HMENU s = CreatePopupMenu();
        AddRadio(s, IDM_VOL_BASE, volNames, volIcons, 4, st.volIdx);
        AddMI(menu, reinterpret_cast<UINT_PTR>(s), true, L"音量", L"volume-2");
    }
    {
        HMENU s = CreatePopupMenu();
        AddRadio(s, IDM_WORD_BASE, wordNames, wordIcons, 3, st.wordIdx);
        AddMI(menu, reinterpret_cast<UINT_PTR>(s), true, L"飘字", L"type");
    }
    {
        HMENU s = CreatePopupMenu();
        AddRadio(s, IDM_GOAL_BASE, goalNames, goalIcons, 6, st.goalIdx);
        AddMI(menu, reinterpret_cast<UINT_PTR>(s), true, L"今日目标", L"target");
    }
    {
        HMENU s = CreatePopupMenu();
        AddRadio(s, IDM_SKIN_BASE, skinNames, skinIcons, 4, st.skinIdx);
        AddMI(menu, reinterpret_cast<UINT_PTR>(s), true, L"皮肤", L"palette");
    }
    {
        HMENU s = CreatePopupMenu();
        AddRadio(s, IDM_ZEN_BASE, config::kZenNames, zenIcons, config::kZenMenuCount, st.zenIdx);
        AddMI(menu, reinterpret_cast<UINT_PTR>(s), true, L"禅定音", L"music");
    }
    {
        HMENU s = CreatePopupMenu();
        int cur = 0;
        for (int i = 0; i < 6; ++i)
            if (ctx.sleepMin == config::kSleepMin[i])
                cur = i;
        AddRadio(s, IDM_SLEEP_BASE, sleepNames, sleepIcons, 6, cur);
        AddMI(menu, reinterpret_cast<UINT_PTR>(s), true, L"睡眠定时(禅定音)", L"moon-star");
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AddMI(menu, IDM_LEDGER, false, L"功德簿", L"book-open");
    AddMI(menu, IDM_RESET, false, L"重置功德", L"rotate-ccw");
    AddMI(menu, IDM_ABOUT, false, L"关于", L"info");
    AddMI(menu, IDM_QUIT, false, L"退出", L"log-out");

    SetForegroundWindow(hwnd);
    UINT id = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, x, y, 0, hwnd,
                             nullptr);
    DestroyMenu(menu);
    if (fromTray)
        PostMessageW(hwnd, WM_NULL, 0, 0);  // 修复托盘菜单点击别处不消失

    if (id == IDM_SHOWHIDE) {
        ToggleFish(ctx);
    } else if (id == IDM_LEDGER) {
        ShowLedgerDialog(hwnd, ReadLedger());
    } else if (id == IDM_ABOUT) {
        std::wstring txt = U8(config::kAppName);
        txt += L"   v" + U8(config::kVersion) + L"\r\n\r\n";
        txt += U8("敲的是赛博木鱼，积的是数字功德。\r\n"
                  "纯 Win32 API + GDI+ 绘制，XAudio2 混音，零依赖单 exe，\r\n"
                  "图像与音乐素材全部内置。\r\n\r\n");
        wchar_t line[160];
        std::swprintf(line, std::size(line),
                      U8("【今日战绩】总功德 %llu · 今日 %llu · 在册 %d 日\r\n\r\n")
                          .c_str(),
                      st.merit, st.daily, static_cast<int>(ReadLedger().size()));
        txt += line;
        txt += U8("【操作提示】\r\n"
                  "木鱼上左键按下再抬起 = 敲一记；按住拖动 = 移动窗口\r\n"
                  "F8 隔空敲击 · 托盘双击显隐 · 右键唤出全部设置\r\n\r\n");
        txt += U8("【致谢与授权】\r\n"
                  "禅定音乐：Kevin MacLeod (incompetech.com), CC-BY 4.0\r\n"
                  "大悲咒（印能法师版）：佛音网 (foyinwang.com) 免费流通版本\r\n"
                  "木鱼/木槌图像：开源微信小程序「电子木鱼」(mp-muyu)\r\n"
                  "佛光图片：本地 AI 生成，无第三方版权\r\n"
                  "菜单图标：Lucide (lucide.dev)，ISC 许可\r\n"
                  "代码：免费软件，拟以 MIT 许可证发布\r\n\r\n");
        txt += U8("© 2026 TianYunCode\r\n"
                  "github.com/TianYunCode/wooden_fish\r\n\r\n"
                  "功德无价，本软件亦免费。");
        ShowTextDialog(hwnd, L"关于", txt, 300, 258);
    } else if (id == IDM_TOPMOST) {
        st.topmost = !st.topmost;
        SetWindowPos(hwnd, st.topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        SaveSettings(st, hwnd);
    } else if (id == IDM_PIN) {
        st.pinned = !st.pinned;
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
        ctx.audio.SetVolume(st.volIdx);
        SaveSettings(st, hwnd);
    } else if (id >= IDM_WORD_BASE && id < IDM_WORD_BASE + 3) {
        st.wordIdx = static_cast<int>(id - IDM_WORD_BASE);
        SaveSettings(st, hwnd);
    } else if (id >= IDM_GOAL_BASE && id < IDM_GOAL_BASE + 6) {
        int idx = static_cast<int>(id - IDM_GOAL_BASE);
        if (idx == 5) {
            int v = 0;
            if (PromptNumber(hwnd, L"自定义今日目标", L"每日功德目标（1 - 99999）：",
                             st.goalIdx == 5 ? static_cast<int>(st.goalCustom) : 100, 1, 99999, v)) {
                st.goalCustom = static_cast<unsigned>(v);
                st.goalIdx = 5;
                render::Render(hwnd, st, ctx.assets);
                SyncAnim(ctx);
                SaveSettings(st, hwnd);
            }
        } else {
            st.goalIdx = idx;
            render::Render(hwnd, st, ctx.assets);
            SyncAnim(ctx);  // 改为已达成档需起呼吸动画；改小档静止后自然停
            SaveSettings(st, hwnd);
        }
    } else if (id >= IDM_SKIN_BASE && id < IDM_SKIN_BASE + 4) {
        st.skinIdx = static_cast<int>(id - IDM_SKIN_BASE);
        render::Render(hwnd, st, ctx.assets);
        SaveSettings(st, hwnd);
    } else if (id >= IDM_ZEN_BASE && id < IDM_ZEN_BASE + config::kZenMenuCount) {
        int idx = static_cast<int>(id - IDM_ZEN_BASE);
        KillTimer(hwnd, kTimerFade);  // 手动切曲作废进行中的睡眠淡出（ApplyZen 内复位增益）
        ctx.fadeStep = 0;
        if (idx == config::kZenCustomIdx) {
            wchar_t buf[4096] = L"";
            OPENFILENAMEW ofn{};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter =
                L"音频文件\0*.mp3;*.wav;*.m4a;*.aac;*.flac;*.wma\0所有文件\0*.*\0";
            ofn.lpstrFile = buf;
            ofn.nMaxFile = static_cast<DWORD>(std::size(buf));
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
            if (GetOpenFileNameW(&ofn)) {
                int old = st.zenIdx;
                st.zenFile = buf;
                if (ctx.audio.ApplyZen(config::kZenCustomIdx, st.zenFile)) {
                    st.zenIdx = config::kZenCustomIdx;
                } else {
                    MessageBoxW(hwnd, L"无法播放该文件（格式不支持或已损坏）", L"电子木鱼",
                                MB_OK | MB_ICONWARNING);
                    ctx.audio.ApplyZen(old, st.zenFile);
                }
                SaveSettings(st, hwnd);
            }
        } else {
            st.zenIdx = idx;
            ctx.audio.ApplyZen(st.zenIdx, st.zenFile);
            SaveSettings(st, hwnd);
        }
    } else if (id >= IDM_SLEEP_BASE && id < IDM_SLEEP_BASE + 6) {
        ctx.sleepMin = config::kSleepMin[id - IDM_SLEEP_BASE];
        KillTimer(hwnd, kTimerSleep);
        KillTimer(hwnd, kTimerFade);
        ctx.fadeStep = 0;
        ctx.audio.SetZenFade(1.0);  // 若淡出进行中，回到正常音量
        if (ctx.sleepMin)
            SetTimer(hwnd, kTimerSleep, ctx.sleepMin * 60000u, nullptr);
    } else if (id == IDM_QUIT) {
        DestroyWindow(hwnd);
    }
}

}  // namespace muyu::ui
