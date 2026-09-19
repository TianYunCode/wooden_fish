# 系统架构

> 电子木鱼（木鱼）—— 纯 Win32 API 桌面小工具，单 exe 零依赖。

## 1. 设计目标

| 目标 | 落地方式 |
|---|---|
| 零部署依赖 | 只用 Windows 自带系统 DLL（GDI+ / MF / XAudio2 / Shell），不引入任何框架 |
| 单文件分发 | 图片、音效、图标、版本信息全部以 `RCDATA`/`ICON`/`VERSIONINFO` 嵌入 exe |
| 首击零延迟 | 启动时把 mp3 预解码为 PCM 常驻内存，播放路径上不再有解码开销 |
| 真透明窗口 | 分层窗口 + `UpdateLayeredWindow` 逐像素 alpha，无矩形边框 |
| 可维护性 | 企业式分层：常量配置 / 核心状态 / 媒体 / 渲染 / UI 五层单向依赖 |

## 2. 分层结构

```
┌─────────────────────────────────────────────┐
│  入口  src/main.cpp (wWinMain 生命周期编排)   │
├─────────────────────────────────────────────┤
│  UI 层  src/ui/                             │
│    main_window  窗口/托盘/消息编排/敲击动作    │
│    menu         右键与托盘菜单构建与分发       │
├─────────────────────────────────────────────┤
│  渲染层  src/render/                         │
│    painter   整帧绘制 → DIB → 分层窗口贴回     │
│    skins     GDI+ ColorMatrix 皮肤调色        │
├─────────────────────────────────────────────┤
│  媒体层  src/media/                          │
│    assets        RCDATA → GDI+ Bitmap        │
│    audio_engine  MF 解码 + XAudio2 混音       │
├─────────────────────────────────────────────┤
│  核心层  src/core/                           │
│    app_state  全部可变运行时状态(无 Win32 副作用)│
│    settings   注册表持久化                    │
│    autorun    开机自启(HKCU Run 键)           │
│    util       编码转换/日期/缓动              │
├─────────────────────────────────────────────┤
│  配置层  src/config/layout.h                 │
│    几何/时间轴/档位表/文案表，全部编译期常量     │
└─────────────────────────────────────────────┘
```

**依赖规则（严格单向）**：`ui → render → media → core → config`。
下层不知道上层存在；`config` 不依赖任何自定义模块。跨层传递的唯一聚合对象是 `AppContext`（定义于 `src/app_context.h`），由 `main.cpp` 构造，经 `GWLP_USERDATA` 挂到窗口句柄上供 `WndProc` 取回。

## 3. 模块职责一览

| 文件 | 职责 | 关键点 |
|---|---|---|
| `src/main.cpp` | 进程生命周期 | GDI+/COM/MF 启停顺序、消息循环、退出清理次序严格对称 |
| `src/app_context.h` | 依赖聚合 | `AppState + Assets + AudioEngine + HWND + 托盘数据` |
| `src/config/layout.h` | 静态配置 | 鱼/槌几何、动画时长、尺寸档、音量档、目标档、福语表 |
| `src/core/app_state.h` | 可变状态 | 功德/连击/飘字列表/拖动瞬态，全部字段可序列化或被重置 |
| `src/core/settings.*` | 持久化 | `HKCU\Software\WoodenFish`，merit/daily 用 REG_BINARY，其余 DWORD |
| `src/core/autorun.*` | 开机自启 | `HKCU\...\Run\WoodenFish` 值存在与否即勾选态 |
| `src/core/util.*` | 工具 | UTF-8→wstring、`TodayYmd()` 日切、缓动函数 |
| `src/media/assets.*` | 图像素材 | `FindResource → SHCreateMemStream → Gdiplus::Bitmap::FromStream` |
| `src/media/audio_engine.*` | 音频 | MF SourceReader 解码 16bit PCM；XAudio2 每击一声部、音调随连击升高、按时间回收；禅定音 5 曲目懒解码、仅当前曲目常驻、无限循环 |
| `src/render/skins.*` | 皮肤 | 4 组 ColorMatrix，按 skinIdx 缓存 ImageAttributes |
| `src/render/painter.*` | 渲染 | 32bpp premultiplied DIB 上画鱼身挤压、波纹、挥槌、飘字、功德、目标进度条 |
| `src/ui/main_window.*` | 窗口 | 分层窗口、点击敲击、拖动移位、托盘回调、双定时器、F8 热键 |
| `src/ui/menu.*` | 菜单 | 纯文字 `MF_STRING` + `MF_CHECKED`，命令分发到各子系统 |
| `src/ui/prompt.*` | 输入框 | 内存 DLGTEMPLATE + `DialogBoxIndirectParamW`，数字输入（自定义目标） |

## 4. 关键技术选型与理由

### 4.1 窗口：`UpdateLayeredWindow` 而非 `SetLayeredWindowAttributes`
逐像素 alpha 才能做出完全贴合素材轮廓的不规则透明；每帧画进 top-down 32bpp DIB（预乘 alpha），一次性贴出，无闪烁。
窗口样式：`WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW` + `WS_POPUP`（不出现在任务栏，工具窗口样式）。

### 4.2 音频：Media Foundation + XAudio2（弃用 MCI/winmm）
- 本机环境的 MCI 设备链不可靠，`PlaySound`/`mciSendString` 均有延迟或失败问题。
- MF `IMFSourceReader` 一次解码 mp3 → PCM 字节流常驻内存。
- XAudio2 原生多声部：快速连击时每次 `CreateSourceVoice` 提交独立缓冲，重叠播放自然形成"连打"效果；`SetFrequencyRatio` 让音调随连击升高（+0.4%/击，封顶 50）。
- 声部回收按 `播放时刻 + 样本时长 + 300ms` 推算，不依赖 `IsStopped` 轮询。
- 禅定背景音：5 首内嵌 mp3（IDR_ZEN1..5）+ 本地文件曲（`MFCreateSourceReaderFromURL`，超长截断 10 分钟并代码内淡入淡出）。`ApplyZen(trackIdx, file)` 选 0=关；选中曲目时才 MF 解码并常驻（换曲释放旧曲 PCM），单独一个 SourceVoice，`XAUDIO2_LOOP_INFINITE`，增益 0.45 × 音量档系数。

### 4.3 皮肤：ColorMatrix 调色而非多套素材
同一张位图用 `ImageAttributes + ColorMatrix` 实时变换（sepia 鎏金 / 去色偏冷水墨 / 通道重排霓虹），零额外体积。

### 4.4 文本渲染：GDI+ + Microsoft YaHei UI
与图形共用同一 Graphics 上下文，随 DIB 一起获得抗锯齿与 alpha 合成；小窗口时字号按"最小物理像素"反向放大（功德 ≥13px、目标 ≥11px 物理高），防止缩到不可读。

### 4.5 构建：CMake + MSVC
`/utf-8` 统一源字符集与执行字符集；`resources/app.rc` 首部 `#pragma code_page(65001)`。
注意：**不能定义 `WIN32_LEAN_AND_MEAN`**——MSVC 的 `gdiplus.h` 依赖完整 `windows.h` 引入的 COM/属性系统头（`IStream`、`PROPID`）。

## 5. 数据流总览

```mermaid
flowchart LR
    R[RCDATA 资源\nexe 内嵌] -->|启动加载| A[Assets\nGDI+ Bitmap]
    R -->|MF 预解码| AE[AudioEngine\nPCM 常驻]
    UI[WndProc / Menu\n用户输入] -->|DoKnock 起挥 → 定时器 Strike 触鱼结算| S[AppState\n功德/连击/飘字]
    S -->|读状态| P[render::Painter]
    A --> P
    S -->|PlayKnock| AE
    P -->|DIB + UpdateLayeredWindow| W[分层窗口\n屏幕]
    S -->|SaveSettings| REG[(注册表\nHKCU\Software\WoodenFish)]
    REG -->|LoadSettings| S
```

## 6. 线程模型

单 UI 线程。音频解码、位图加载全部在 `wWinMain` 显示窗口**之前**同步完成（门控），运行期不做任何磁盘/注册表扫描（仅敲击/改设置时一次性注册表写）。16ms 定时器承担动画帧驱动，无动画需求时跳过重绘以省 CPU。

---

下一篇：[类图](class-diagram.md) · [时序图](sequence-diagrams.md) · [开发指南](development-guide.md)
