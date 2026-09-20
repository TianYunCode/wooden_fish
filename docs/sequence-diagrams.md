# 时序图

覆盖五条主链路：启动、手动敲击、动画/自动敲击循环、菜单操作、退出清理。

## 1. 启动序列（门控式：一切就绪才显示 UI）

```mermaid
sequenceDiagram
    autonumber
    participant OS as Windows
    participant M as wWinMain (main.cpp)
    participant REG as 注册表
    participant R as exe 内嵌资源
    participant AE as AudioEngine
    participant W as ui::Create / WndProc

    OS->>M: wWinMain(hInst)
    M->>M: GdiplusStartup + 动态加载 SetProcessDpiAwarenessContext(PerMonitorV2)，失败退回 SetProcessDPIAware
    M->>M: state.dpi = GetDpiForSystem()/96
    M->>REG: LoadSettings(state)
    REG-->>M: 功德/设置/自定义目标值 goalX/本地禅曲路径 zenFile（窗口位置不落盘）
    M->>R: Assets.Load()（RCDATA→GDI+ Bitmap ×3）
    R-->>M: fish / gu / glow 位图
    M->>M: CoInitializeEx + MFStartup
    M->>AE: Init()
    AE->>R: MF SourceReader 仅解码 knock.mp3 → PCM（禅曲不预解码，ApplyZen 时整曲解完后才起播）
    AE->>AE: XAudio2Create + MasteringVoice
    AE-->>M: ok
    M->>M: zenIdx==本地文件但文件已丢失 → 视为关
    M->>AE: ApplyZen(state.zenIdx, zenFile)（恢复上次禅定音：解码线程整曲解完后起播）
    M->>W: Create(ctx,hInst)
    W->>OS: RegisterClassExW("WoodenFishWnd")
    W->>OS: CreateWindowExW(LAYERED|TOPMOST|TOOLWINDOW, lpParam=&ctx)
    OS->>W: WM_NCCREATE
    W->>W: GWLP_USERDATA ← &ctx
    W->>W: 位置固定工作区右下角（距边 40px，每次启动如此）
    W->>W: Per-Monitor 校正：GetDpiForWindow 与系统 DPI 不符则重设窗口尺寸
    M->>OS: AddTrayIcon(Shell_NotifyIconW NIM_ADD)
    M->>OS: SyncAnim（动画定时器按需挂载）+ ApplyAuto + RegisterHotKey(F8)
    M->>M: render::Render（首帧）
    M->>OS: ShowWindow / 进入消息循环
```

> 解码先于窗口显示，保证第一次点击必有声音、无预热卡顿。

## 2. 手动敲击（一次点击的完整链路）

```mermaid
sequenceDiagram
    autonumber
    participant U as 用户
    participant OS as Windows
    participant WP as WndProc
    participant K as ui::DoKnock
    participant AE as AudioEngine
    participant P as render::Painter
    participant REG as 注册表

    U->>OS: 左键按下
    OS->>WP: WM_LBUTTONDOWN(x,y)
    WP->>WP: SetCapture · 记录 lastX/Y · dragging=false
    opt 按住期间移动 >4px（"固定"勾选后禁用）
        OS->>WP: WM_MOUSEMOVE → SetWindowPos · dragging=true
    end
    U->>OS: 左键抬起
    OS->>WP: WM_LBUTTONUP(x,y) → ReleaseCapture
    Note over WP: 未拖动过才 DoKnock——抬起才起挥；拖过只算移动窗口
    WP->>K: DoKnock(x/EffScale, y/EffScale)（换算回基准坐标）
    K->>K: 记录 knockAt/impactX/Y · impactPending=true（仅起挥，棒槌开始下挥）
    K->>P: Render（挥槌动画启动）
    Note over K: kSwingMs(90ms) 后由动画定时器回调 Strike（见时序图 3）
    K->>K: Strike：merit++ · 跨天则 daily 归零 · daily++
    K->>K: combo：1.5s 窗口内 +1 否则重置 1
    K->>K: 生成飘字（固定/随机福语，"关闭"档则跳过；10/30/50 连击追加金色"连击 xN"）
    K->>K: 飘字池封顶 24 条
    K->>AE: PlayKnock(volIdx, combo)（槌头触鱼一刻才发声）
    AE->>AE: 回收 endAt 到期声部
    AE->>AE: CreateSourceVoice→SetVolume(档位)→SetFrequencyRatio(音调随连击)→Submit→Start
    K->>P: Render(hwnd, state, assets)
    P->>P: 画入 32bpp DIB：鱼身挤压/达成光晕/音波纹/挥槌/飘字/功德/进度条（挤压与波纹以 impactAt 为时基）
    P->>OS: UpdateLayeredWindow(ULW_ALPHA)
    K->>REG: SaveSettings（功德即时落盘，断电不丢）
```

## 3. 定时器驱动：按需动画 / 自动敲击 / 睡眠淡出

```mermaid
sequenceDiagram
    autonumber
    participant T1 as Timer#1 (16ms 动画,按需挂载)
    participant T2 as Timer#2 (400/800/1500ms 自动敲)
    participant T3 as Timer#3 睡眠倒计时(分钟级)
    participant T4 as Timer#4 淡出(100ms×50)
    participant WP as WndProc
    participant K as ui::DoKnock
    participant P as render::Painter
    participant AE as AudioEngine

    Note over T1: 静止时 Timer#1 不存在（零唤醒、CPU≈0）；DoKnock/达成呼吸等经 SyncAnim 挂载
    loop 动画进行中的每一帧
        T1->>WP: WM_TIMER(1)
        alt impactPending 且 now-knockAt ≥ kSwingMs
            WP->>K: Strike()（触鱼结算：发声+挤压+波纹+飘字+计数+落盘）
        end
        WP->>WP: 清除 born 超过 1s 的飘字
        alt AnimActive（下挥进行中 或 动画未散 或 目标达成光晕呼吸）
            WP->>P: Render（仅窗口可见时；推进 squash/波纹/挥槌/飘字 相位）
        else 静止
            WP->>WP: KillTimer(1) 摘表 + 补画最后一帧收尾
        end
    end

    opt 自动敲击档位 ≠ 关
        T2->>WP: WM_TIMER(2)
        WP->>K: DoKnock(kHitX+15, kHitY-25)（槌头落点附近）
        Note over K: 与手动敲击同一条链路
    end

    opt 睡眠定时已启用（会话级，不落盘）
        T3->>WP: WM_TIMER(3) 到点
        WP->>T4: 挂载淡出定时器（若禅定音开着）
        loop 50 步 × 100ms
            T4->>WP: WM_TIMER(4)
            WP->>AE: SetZenFade(1 - step/50)（增益逐档下降）
        end
        T4->>WP: 最后一步 f≤0
        WP->>AE: ApplyZen(0)（毁声部+释放整曲 PCM）→ zen=0 落盘
        Note over WP: 淡出期间手动切曲/改档即作废（KillTimer+系数复位）
    end

    opt 全局热键 F8（任何前台程序下）
        Note over WP: WM_HOTKEY → DoKnock(鱼面中央)
    end
```

## 4. 菜单操作（右键 / 托盘共用一套）

```mermaid
sequenceDiagram
    autonumber
    participant U as 用户
    participant WP as WndProc
    participant MU as ui::ShowMenu
    participant SYS as 各子系统

    U->>WP: WM_RBUTTONUP（鱼身上）或 托盘图标单击
    WP->>MU: ShowMenu(ctx, 屏幕坐标, fromTray)
    MU->>MU: CreatePopupMenu + AddMI/AddRadio（owner-draw，MenuDrawData 存 dwItemData，读当前 state 打勾）
    MU->>U: TrackPopupMenu(TPM_RETURNCMD) 阻塞取选择
    Note over U,WP: 弹出/悬停期间系统回调 WM_MEASUREITEM / WM_DRAWITEM → ui::OnMeasureMenu / OnDrawMenu（图标列 + 文字 + ✓）
    U-->>MU: 菜单项 id（或取消=0）
    alt IDM_SIZE_BASE+n
        MU->>SYS: ApplyScale(kSizeVals[n]) → SetWindowPos+Render
    else IDM_SKIN_BASE+n
        MU->>SYS: state.skinIdx=n → Render（GetSkinAttr 换矩阵）
    else IDM_GOAL_BASE+n（n=5 "自定义…"）
        MU->>SYS: PromptNumber 数字输入框（内存 DLGTEMPLATE）→ 设定 goalCustom 且 goalIdx=5 → Render
    else IDM_GOAL_BASE+n（预设档）
        MU->>SYS: state.goalIdx=n → Render → SyncAnim（改为已达成档要起呼吸动画）
    else IDM_ZEN_BASE+n（n=kZenCustomIdx "本地音频文件…"）
        MU->>SYS: GetOpenFileNameW 选文件 → ApplyZen(kZenCustomIdx, 路径)；打开失败 MessageBox 并回退原曲（手动切曲同时作废睡眠淡出）
    else IDM_ZEN_BASE+n（其余）
        MU->>SYS: audio.ApplyZen(n) 切曲（先毁旧曲并释放其整曲 PCM，再后台解码新曲）
    else IDM_SLEEP_BASE+n（睡眠定时 关/15/30/45/60/90 分钟）
        MU->>SYS: ctx.sleepMin=kSleepMin[n]（会话级）→ 重挂/摘 kTimerSleep，作废进行中淡出
    else IDM_AUTO_BASE+n
        MU->>SYS: ApplyAuto（KillTimer/SetTimer 2）
    else IDM_VOL/WORD
        MU->>SYS: 改 state 字段 → Render
    else IDM_TOPMOST
        MU->>SYS: SetWindowPos TOPMOST/NOTOPMOST
    else IDM_PIN
        MU->>SYS: state.pinned 取反（WM_MOUSEMOVE 拖动闸门）
    else IDM_AUTORUN
        MU->>SYS: AutoRunSet(!AutoRunOn())
    else IDM_SHOWHIDE（托盘双击/菜单）
        MU->>SYS: ToggleFish
    else IDM_LEDGER
        MU->>SYS: ReadLedger（注册表 log 子键）→ ShowLedgerDialog（ListView 表格：日期/当日功德，斑马纹+千分位+今日标记+合计行）
    else IDM_ABOUT
        MU->>SYS: 组装版本/战绩/致谢文本（config::kVersion ← version.h）→ ShowTextDialog
    else IDM_RESET
        MU->>SYS: merit=daily=0 → Render
    else IDM_QUIT
        MU->>SYS: DestroyWindow → WM_DESTROY
    end
    MU->>MU: SaveSettings（一切改设置即持久化）
```

## 5. 退出与资源清理（与启动严格逆序）

```mermaid
sequenceDiagram
    autonumber
    participant WP as WndProc
    participant M as wWinMain
    participant AE as AudioEngine
    participant OS as Windows

    WP->>OS: WM_DESTROY → PostQuitMessage(0)
    OS-->>M: GetMessage 返回 0，消息循环退出
    M->>OS: KillTimer(1) KillTimer(2) UnregisterHotKey(F8)
    M->>M: SaveSettings（功德与全部设置落盘；窗口位置不存）
    M->>OS: Shell_NotifyIconW(NIM_DELETE) + DestroyIcon
    M->>AE: Shutdown()：Zen声部→敲击声部池→Mastering→XAudio2
    M->>OS: MFShutdown + CoUninitialize
    M->>M: Assets.Free()（delete Bitmap）
    M->>OS: GdiplusShutdown → 进程退出
```

---

上一篇：[类图](class-diagram.md) · 下一篇：[开发指南](development-guide.md)
