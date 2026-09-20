#include "ui/menu.h"

#include <windows.h>
#include <commdlg.h>
#include <cstdio>
#include <cwchar>
#include <iterator>
#include <string>
#include <vector>

#include "config/layout.h"
#include "core/autorun.h"
#include "core/settings.h"
#include "core/util.h"
#include "render/painter.h"
#include "ui/main_window.h"
#include "ui/prompt.h"

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
    const char *goalNames[] = {"不设", "27", "54", "108", "216", "自定义…"};
    const char *wordNames[] = {"固定 功德+1", "随机福语", "关闭"};
    const char *skinNames[] = {"原木", "鎏金", "水墨", "霓虹"};

    HMENU menu = CreatePopupMenu();
    if (fromTray) {
        std::wstring shTxt = U8(IsWindowVisible(hwnd) ? "隐藏木鱼" : "显示木鱼");
        AppendMenuW(menu, MF_STRING, IDM_SHOWHIDE, shTxt.c_str());
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    }
    AppendMenuW(menu, MF_STRING | (st.topmost ? MF_CHECKED : 0), IDM_TOPMOST, L"置顶");
    AppendMenuW(menu, MF_STRING | (st.pinned ? MF_CHECKED : 0), IDM_PIN, L"固定");
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
        AddRadio(s, IDM_WORD_BASE, wordNames, 3, st.wordIdx);
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(s), L"飘字");
    }
    {
        HMENU s = CreatePopupMenu();
        AddRadio(s, IDM_GOAL_BASE, goalNames, 6, st.goalIdx);
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(s), L"今日目标");
    }
    {
        HMENU s = CreatePopupMenu();
        AddRadio(s, IDM_SKIN_BASE, skinNames, 4, st.skinIdx);
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(s), L"皮肤");
    }
    {
        HMENU s = CreatePopupMenu();
        AddRadio(s, IDM_ZEN_BASE, config::kZenNames, config::kZenMenuCount, st.zenIdx);
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(s), L"禅定音");
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_LEDGER, L"功德簿");
    AppendMenuW(menu, MF_STRING, IDM_RESET, L"重置功德");
    AppendMenuW(menu, MF_STRING, IDM_ABOUT, L"关于");
    AppendMenuW(menu, MF_STRING, IDM_QUIT, L"退出");

    SetForegroundWindow(hwnd);
    UINT id = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, x, y, 0, hwnd,
                             nullptr);
    DestroyMenu(menu);
    if (fromTray)
        PostMessageW(hwnd, WM_NULL, 0, 0);  // 修复托盘菜单点击别处不消失

    if (id == IDM_SHOWHIDE) {
        ToggleFish(ctx);
    } else if (id == IDM_LEDGER) {
        auto rows = ReadLedger();
        std::wstring txt;
        wchar_t line[96];
        unsigned long long sum = 0;
        for (auto &r : rows)
            sum += r.second;
        if (rows.empty()) {
            txt = U8("尚无记录。\n开始敲击，功德簿会自动记下每天最终的功德。");
        } else {
            txt = U8("日期          当日功德\n");
            for (auto it = rows.rbegin(); it != rows.rend(); ++it) {  // 最近一天在最上
                DWORD d = it->first;
                std::swprintf(line, std::size(line), L"%04u-%02u-%02u    %llu\r\n",
                              d / 10000, d / 100 % 100, d % 100, it->second);
                txt += line;
            }
            std::swprintf(line, std::size(line), L"\r\n在册 %d 日，累计 %llu 功德",
                          static_cast<int>(rows.size()), sum);
            txt += line;
        }
        ShowTextDialog(hwnd, L"功德簿", txt);
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
                SaveSettings(st, hwnd);
            }
        } else {
            st.goalIdx = idx;
            render::Render(hwnd, st, ctx.assets);
            SaveSettings(st, hwnd);
        }
    } else if (id >= IDM_SKIN_BASE && id < IDM_SKIN_BASE + 4) {
        st.skinIdx = static_cast<int>(id - IDM_SKIN_BASE);
        render::Render(hwnd, st, ctx.assets);
        SaveSettings(st, hwnd);
    } else if (id >= IDM_ZEN_BASE && id < IDM_ZEN_BASE + config::kZenMenuCount) {
        int idx = static_cast<int>(id - IDM_ZEN_BASE);
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
    } else if (id == IDM_QUIT) {
        DestroyWindow(hwnd);
    }
}

}  // namespace muyu::ui
