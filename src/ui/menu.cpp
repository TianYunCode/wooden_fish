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

// ==================== 命令处理器（Command 模式） ====================
// 每个菜单命令对应一个处理器函数；单选命令忽略 id 参数。
// 新增设置项：加 IDM_* 枚举 → 写处理器 → 在 kHandlers 表注册一行。

void OnShowHide(AppContext &ctx, UINT) { ToggleFish(ctx); }

void OnTopmost(AppContext &ctx, UINT) {
    AppState &st = ctx.state;
    st.topmost = !st.topmost;
    SetWindowPos(ctx.hwnd, st.topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SaveSettings(st, ctx.hwnd);
}

void OnPin(AppContext &ctx, UINT) {
    AppState &st = ctx.state;
    st.pinned = !st.pinned;
    SaveSettings(st, ctx.hwnd);
}

void OnAutoRun(AppContext &, UINT) { AutoRunSet(!AutoRunOn()); }

void OnReset(AppContext &ctx, UINT) {
    AppState &st = ctx.state;
    st.merit = 0;
    st.daily = 0;
    render::Render(ctx.hwnd, st, ctx.assets);
    SaveSettings(st, ctx.hwnd);
}

void OnLedger(AppContext &ctx, UINT) { ShowLedgerDialog(ctx.hwnd, ReadLedger()); }

void OnAbout(AppContext &ctx, UINT) {
    AppState &st = ctx.state;
    std::wstring txt = U8(config::kAppName);
    txt += L"   v" + U8(config::kVersion) + L"\r\n\r\n";
    txt += U8("敲的是赛博木鱼，积的是数字功德。\r\n"
              "纯 Win32 API + GDI+ 绘制，XAudio2 混音，零依赖单 exe，\r\n"
              "图像与音乐素材全部内置。\r\n\r\n");
    wchar_t line[160];
    std::swprintf(line, std::size(line),
                  U8("【今日战绩】总功德 %llu · 今日 %llu · 在册 %d 日\r\n\r\n").c_str(), st.merit,
                  st.daily, static_cast<int>(ReadLedger().size()));
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
    ShowTextDialog(ctx.hwnd, L"关于", txt, 300, 258);
}

void OnSize(AppContext &ctx, UINT id) {
    ApplyScale(ctx, config::kSizeVals[id - IDM_SIZE_BASE]);
    SaveSettings(ctx.state, ctx.hwnd);
}

void OnAuto(AppContext &ctx, UINT id) {
    AppState &st = ctx.state;
    st.autoIdx = static_cast<int>(id - IDM_AUTO_BASE);
    ApplyAuto(ctx);
    SaveSettings(st, ctx.hwnd);
}

void OnVol(AppContext &ctx, UINT id) {
    AppState &st = ctx.state;
    st.volIdx = static_cast<int>(id - IDM_VOL_BASE);
    ctx.audio.SetVolume(st.volIdx);
    SaveSettings(st, ctx.hwnd);
}

void OnWord(AppContext &ctx, UINT id) {
    AppState &st = ctx.state;
    st.wordIdx = static_cast<int>(id - IDM_WORD_BASE);
    SaveSettings(st, ctx.hwnd);
}

void OnGoal(AppContext &ctx, UINT id) {
    AppState &st = ctx.state;
    int idx = static_cast<int>(id - IDM_GOAL_BASE);
    if (idx == 5) {
        int v = 0;
        if (PromptNumber(ctx.hwnd, L"自定义今日目标", L"每日功德目标（1 - 99999）：",
                         st.goalIdx == 5 ? static_cast<int>(st.goalCustom) : 100, 1, 99999, v)) {
            st.goalCustom = static_cast<unsigned>(v);
            st.goalIdx = 5;
        } else {
            return;  // 用户取消，不改档
        }
    } else {
        st.goalIdx = idx;
    }
    render::Render(ctx.hwnd, st, ctx.assets);
    SyncAnim(ctx);  // 改为已达成档需起呼吸动画；改小档静止后自然停
    SaveSettings(st, ctx.hwnd);
}

void OnSkin(AppContext &ctx, UINT id) {
    AppState &st = ctx.state;
    st.skinIdx = static_cast<int>(id - IDM_SKIN_BASE);
    render::Render(ctx.hwnd, st, ctx.assets);
    SaveSettings(st, ctx.hwnd);
}

// "本地音频文件…"：选文件并试播；失败提示并回退原曲
void OnZenCustom(AppContext &ctx) {
    AppState &st = ctx.state;
    wchar_t buf[4096] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = ctx.hwnd;
    ofn.lpstrFilter = L"音频文件\0*.mp3;*.wav;*.m4a;*.aac;*.flac;*.wma\0所有文件\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = static_cast<DWORD>(std::size(buf));
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameW(&ofn))
        return;
    int old = st.zenIdx;
    st.zenFile = buf;
    if (ctx.audio.ApplyZen(config::kZenCustomIdx, st.zenFile)) {
        st.zenIdx = config::kZenCustomIdx;
    } else {
        MessageBoxW(ctx.hwnd, L"无法播放该文件（格式不支持或已损坏）", L"电子木鱼",
                    MB_OK | MB_ICONWARNING);
        ctx.audio.ApplyZen(old, st.zenFile);
    }
    SaveSettings(st, ctx.hwnd);
}

void OnZen(AppContext &ctx, UINT id) {
    AppState &st = ctx.state;
    int idx = static_cast<int>(id - IDM_ZEN_BASE);
    KillTimer(ctx.hwnd, kTimerFade);  // 手动切曲作废进行中的睡眠淡出（ApplyZen 内复位增益）
    ctx.fadeStep = 0;
    if (idx == config::kZenCustomIdx) {
        OnZenCustom(ctx);
    } else {
        st.zenIdx = idx;
        ctx.audio.ApplyZen(st.zenIdx, st.zenFile);
        SaveSettings(st, ctx.hwnd);
    }
}

void OnSleep(AppContext &ctx, UINT id) {
    ctx.sleepMin = config::kSleepMin[id - IDM_SLEEP_BASE];
    KillTimer(ctx.hwnd, kTimerSleep);
    KillTimer(ctx.hwnd, kTimerFade);
    ctx.fadeStep = 0;
    ctx.audio.SetZenFade(1.0);  // 若淡出进行中，回到正常音量
    if (ctx.sleepMin)
        SetTimer(ctx.hwnd, kTimerSleep, ctx.sleepMin * 60000u, nullptr);
}

void OnQuit(AppContext &ctx, UINT) { DestroyWindow(ctx.hwnd); }

// 命令表：ID 闭区间 → 处理器（区段命令注册整段，处理器内自行减 base 取档位）
struct MenuHandler {
    UINT_PTR first, last;
    void (*fn)(AppContext &, UINT id);
};

const MenuHandler kHandlers[] = {
    {IDM_SHOWHIDE, IDM_SHOWHIDE, OnShowHide},                                  //
    {IDM_LEDGER, IDM_LEDGER, OnLedger},                                        //
    {IDM_ABOUT, IDM_ABOUT, OnAbout},                                           //
    {IDM_TOPMOST, IDM_TOPMOST, OnTopmost},                                     //
    {IDM_PIN, IDM_PIN, OnPin},                                                 //
    {IDM_AUTORUN, IDM_AUTORUN, OnAutoRun},                                     //
    {IDM_RESET, IDM_RESET, OnReset},                                           //
    {IDM_SIZE_BASE, IDM_SIZE_BASE + 2, OnSize},                                //
    {IDM_AUTO_BASE, IDM_AUTO_BASE + 3, OnAuto},                                //
    {IDM_VOL_BASE, IDM_VOL_BASE + 3, OnVol},                                   //
    {IDM_WORD_BASE, IDM_WORD_BASE + 2, OnWord},                                //
    {IDM_GOAL_BASE, IDM_GOAL_BASE + 5, OnGoal},                                //
    {IDM_SKIN_BASE, IDM_SKIN_BASE + 3, OnSkin},                                //
    {IDM_ZEN_BASE, IDM_ZEN_BASE + config::kZenMenuCount - 1, OnZen},           //
    {IDM_SLEEP_BASE, IDM_SLEEP_BASE + 5, OnSleep},                             //
    {IDM_QUIT, IDM_QUIT, OnQuit},                                              //
};

void DispatchMenu(AppContext &ctx, UINT id) {
    for (const auto &h : kHandlers) {
        if (id >= h.first && id <= h.last) {
            h.fn(ctx, id);
            return;
        }
    }
}

// ==================== 菜单构建 ====================

HMENU BuildSizeMenu(const AppState &st) {
    const char *names[] = {"小", "中", "大"};
    const wchar_t *icons[] = {L"shrink", L"expand", L"maximize"};
    HMENU s = CreatePopupMenu();
    AddRadio(s, IDM_SIZE_BASE, names, icons, 3,
             st.scale < config::kSizeBig * 0.375 ? 0 : st.scale < config::kSizeBig * 0.75 ? 1 : 2);
    return s;
}

HMENU BuildAutoMenu(const AppState &st) {
    const char *names[] = {"关", "慢", "中", "快"};
    const wchar_t *icons[] = {L"ban", L"turtle", L"footprints", L"rabbit"};
    HMENU s = CreatePopupMenu();
    AddRadio(s, IDM_AUTO_BASE, names, icons, 4, st.autoIdx);
    return s;
}

HMENU BuildVolMenu(const AppState &st) {
    const char *names[] = {"静音", "小", "中", "大"};
    const wchar_t *icons[] = {L"volume-x", L"volume-1", L"volume", L"volume-2"};
    HMENU s = CreatePopupMenu();
    AddRadio(s, IDM_VOL_BASE, names, icons, 4, st.volIdx);
    return s;
}

HMENU BuildWordMenu(const AppState &st) {
    const char *names[] = {"固定 功德+1", "随机福语", "关闭"};
    const wchar_t *icons[] = {L"type", L"shuffle", L"circle-slash"};
    HMENU s = CreatePopupMenu();
    AddRadio(s, IDM_WORD_BASE, names, icons, 3, st.wordIdx);
    return s;
}

HMENU BuildGoalMenu(const AppState &st) {
    const char *names[] = {"不设", "27", "54", "108", "216", "自定义…"};
    const wchar_t *icons[] = {L"circle-slash", L"target", L"target",
                              L"target",           L"target", L"pencil-line"};
    HMENU s = CreatePopupMenu();
    AddRadio(s, IDM_GOAL_BASE, names, icons, 6, st.goalIdx);
    return s;
}

HMENU BuildSkinMenu(const AppState &st) {
    const char *names[] = {"原木", "鎏金", "水墨", "霓虹"};
    const wchar_t *icons[] = {L"tree-deciduous", L"crown", L"brush", L"zap"};
    HMENU s = CreatePopupMenu();
    AddRadio(s, IDM_SKIN_BASE, names, icons, 4, st.skinIdx);
    return s;
}

HMENU BuildZenMenu(const AppState &st) {
    const wchar_t *icons[] = {L"ban",       L"piano",   L"music-2", L"hand-metal",
                              L"wind",      L"flower-2", L"scroll-text", L"file-audio"};
    HMENU s = CreatePopupMenu();
    AddRadio(s, IDM_ZEN_BASE, config::kZenNames, icons, config::kZenMenuCount, st.zenIdx);
    return s;
}

HMENU BuildSleepMenu(const AppContext &ctx) {
    const char *names[] = {"关", "15 分钟", "30 分钟", "45 分钟", "60 分钟", "90 分钟"};
    const wchar_t *icons[] = {L"ban", L"timer", L"timer", L"timer", L"timer", L"timer"};
    int cur = 0;
    for (int i = 0; i < 6; ++i)
        if (ctx.sleepMin == config::kSleepMin[i])
            cur = i;
    HMENU s = CreatePopupMenu();
    AddRadio(s, IDM_SLEEP_BASE, names, icons, 6, cur);
    return s;
}

}  // namespace

void ShowMenu(AppContext &ctx, int x, int y, bool fromTray) {
    HWND hwnd = ctx.hwnd;
    AppState &st = ctx.state;

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
    AddMI(menu, reinterpret_cast<UINT_PTR>(BuildSizeMenu(st)), true, L"调节大小", L"scaling");
    AddMI(menu, reinterpret_cast<UINT_PTR>(BuildAutoMenu(st)), true, L"自动敲击", L"bot");
    AddMI(menu, reinterpret_cast<UINT_PTR>(BuildVolMenu(st)), true, L"音量", L"volume-2");
    AddMI(menu, reinterpret_cast<UINT_PTR>(BuildWordMenu(st)), true, L"飘字", L"type");
    AddMI(menu, reinterpret_cast<UINT_PTR>(BuildGoalMenu(st)), true, L"今日目标", L"target");
    AddMI(menu, reinterpret_cast<UINT_PTR>(BuildSkinMenu(st)), true, L"皮肤", L"palette");
    AddMI(menu, reinterpret_cast<UINT_PTR>(BuildZenMenu(st)), true, L"禅定音", L"music");
    AddMI(menu, reinterpret_cast<UINT_PTR>(BuildSleepMenu(ctx)), true, L"睡眠定时(禅定音)",
          L"moon-star");
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

    DispatchMenu(ctx, id);
}

}  // namespace muyu::ui
