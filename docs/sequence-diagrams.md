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
    M->>M: GdiplusStartup + SetProcessDPIAware
    M->>M: state.dpi = GetDpiForSystem()/96
    M->>REG: LoadSettings(state)
    REG-->>M: 功德/设置/上次窗口位置
    M->>R: Assets.Load()（RCDATA→GDI+ Bitmap ×2）
    R-->>M: fish / gu 位图
    M->>M: CoInitializeEx + MFStartup
    M->>AE: Init()
    AE->>R: MF SourceReader 解码 knock.mp3 → PCM
    AE->>R: 解码 zen.mp3 → PCM（可选）
    AE->>AE: XAudio2Create + MasteringVoice
    AE-->>M: ok
    M->>AE: ApplyZen(state.zenIdx)（恢复上次禅定音）
    M->>W: Create(ctx,hInst)
    W->>OS: RegisterClassExW("WoodenFishWnd")
    W->>OS: CreateWindowExW(LAYERED|TOPMOST|TOOLWINDOW, lpParam=&ctx)
    OS->>W: WM_NCCREATE
    W->>W: GWLP_USERDATA ← &ctx
    W->>W: 位置校验（不在任何显示器内则回落到右下角）
    M->>OS: AddTrayIcon(Shell_NotifyIconW NIM_ADD)
    M->>OS: SetTimer(动画16ms) + ApplyAuto + RegisterHotKey(F8)
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
    WP->>WP: SetCapture · 记录 lastX/Y
    WP->>K: DoKnock(x/EffScale, y/EffScale)（换算回基准坐标）
    K->>K: 记录 knockAt/impactX/Y · impactPending=true（仅起挥，棒槌开始下挥）
    K->>P: Render（挥槌动画启动）
    Note over K: kSwingMs(90ms) 后由动画定时器回调 Strike（见时序图 3）
    K->>K: Strike：merit++ · 跨天则 daily 归零 · daily++
    K->>K: combo：1.5s 窗口内 +1 否则重置 1
    K->>K: 生成飘字（固定/随机福语；10/30/50 连击追加金色"连击 xN"）
    K->>K: 飘字池封顶 24 条
    K->>AE: PlayKnock(volIdx, combo)（槌头触鱼一刻才发声）
    AE->>AE: 回收 endAt 到期声部
    AE->>AE: CreateSourceVoice→SetVolume(档位)→SetFrequencyRatio(音调随连击)→Submit→Start
    K->>P: Render(hwnd, state, assets)
    P->>P: 画入 32bpp DIB：鱼身挤压/达成光晕/音波纹/挥槌/飘字/功德/进度条（挤压与波纹以 impactAt 为时基）
    P->>OS: UpdateLayeredWindow(ULW_ALPHA)
    K->>REG: SaveSettings（功德即时落盘，断电不丢）
    U->>OS: 按住拖动 >4px 视为移动窗口
    OS->>WP: WM_MOUSEMOVE → SetWindowPos
    U->>OS: 左键抬起
    OS->>WP: WM_LBUTTONUP → ReleaseCapture ·（拖过则存位置）
```

## 3. 定时器驱动：动画帧 与 自动敲击

```mermaid
sequenceDiagram
    autonumber
    participant T1 as Timer#1 (16ms)
    participant T2 as Timer#2 (400/800/1500ms)
    participant WP as WndProc
    participant K as ui::DoKnock
    participant P as render::Painter

    loop 每 16ms
        T1->>WP: WM_TIMER(1)
        alt impactPending 且 now-knockAt ≥ kSwingMs
            WP->>K: Strike()（触鱼结算：发声+挤压+波纹+飘字+计数+落盘）
        end
        WP->>WP: 清除 born 超过 1s 的飘字
        alt AnimActive（下挥进行中 或 动画未散 或 目标达成光晕呼吸）
            WP->>P: Render（推进 squash/波纹/挥槌/飘字 相位）
        else 静止
            WP->>WP: 跳过重绘（省电）
        end
    end

    opt 自动敲击档位 ≠ 关
        T2->>WP: WM_TIMER(2)
        WP->>K: DoKnock(kHitX+15, kHitY-25)（槌头落点附近）
        Note over K: 与手动敲击同一条链路
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
    MU->>MU: CreatePopupMenu + AddRadio（读当前 state 打勾）
    MU->>U: TrackPopupMenu(TPM_RETURNCMD) 阻塞取选择
    U-->>MU: 菜单项 id（或取消=0）
    alt IDM_SIZE_BASE+n
        MU->>SYS: ApplyScale(kSizeVals[n]) → SetWindowPos+Render
    else IDM_SKIN_BASE+n
        MU->>SYS: state.skinIdx=n → Render（GetSkinAttr 换矩阵）
    else IDM_ZEN_BASE+n
        MU->>SYS: audio.ApplyZen(on) + Render
    else IDM_AUTO_BASE+n
        MU->>SYS: ApplyAuto（KillTimer/SetTimer 2）
    else IDM_VOL/WORD/GOAL
        MU->>SYS: 改 state 字段 → Render
    else IDM_TOPMOST
        MU->>SYS: SetWindowPos TOPMOST/NOTOPMOST
    else IDM_AUTORUN
        MU->>SYS: AutoRunSet(!AutoRunOn())
    else IDM_SHOWHIDE（托盘双击/菜单）
        MU->>SYS: ToggleFish
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
    M->>M: SaveSettings（最后状态与窗口位置落盘）
    M->>OS: Shell_NotifyIconW(NIM_DELETE) + DestroyIcon
    M->>AE: Shutdown()：Zen声部→敲击声部池→Mastering→XAudio2
    M->>OS: MFShutdown + CoUninitialize
    M->>M: Assets.Free()（delete Bitmap）
    M->>OS: GdiplusShutdown → 进程退出
```

---

上一篇：[类图](class-diagram.md) · 下一篇：[开发指南](development-guide.md)
