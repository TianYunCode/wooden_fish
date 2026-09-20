# 开发指南

## 1. 环境要求

| 项 | 要求 |
|---|---|
| 编译器 | MSVC（Visual Studio 2022，v143 工具集），仅 x64 |
| 构建 | CMake ≥ 3.16 |
| SDK | Windows 10/11 SDK（GDI+、Media Foundation、XAudio2 均为系统组件） |
| 运行时依赖 | 无。目标系统为 Windows 10+ 即可直接运行 exe |

## 2. 构建与运行

```powershell
# 配置（仅首次）
cmake -B build_msvc -G "Visual Studio 17 2022" -A x64
# 编译
cmake --build build_msvc --config Release
# 产物
build\电子木鱼.exe
```

- 输出名通过 `OUTPUT_NAME` + `RUNTIME_OUTPUT_DIRECTORY_<CONFIG>` 固定为 `build/电子木鱼.exe`，`build/` 目录只保留这一个交付物。
- 正在运行时重新链接会报 `LNK1104`（文件被占用），先 `taskkill /IM 电子木鱼.exe /F`。

## 3. 目录结构

```
CMakeLists.txt
README.md
docs/                 本文档目录
resources/            资源
  app.rc              资源脚本（#pragma code_page(65001) 必需；VERSIONINFO 引用 version.h；RT_MANIFEST 引 app.manifest）
  app.manifest        Common Controls v6 依赖声明（功德簿 ListView 表格所需）+ asInvoker
  version.h           全工程唯一版本号（MAJOR/MINOR/PATCH + 字符串），发版只改这里
  resource.h          IDR_FISH=101 IDR_GU=102 IDR_KNOCK=103 IDR_ZEN1=104 IDR_GLOW=105 IDR_ZEN2..6=106..110
  app_icon.ico  images/{muyu,gu,glow}.png  sounds/knock.mp3  audio/zen1..6.mp3
src/
  main.cpp            wWinMain
  app_context.h       依赖聚合根
  config/layout.h     全部静态配置常量
  core/               util / app_state / settings / autorun
  media/              assets / audio_engine
  render/             skins / painter
  ui/                 main_window / menu / prompt
build/                交付目录（只放 电子木鱼.exe）
build_msvc/           CMake 二进制目录（可整体删除重建，勿提交 git）
```

## 4. 编码约定

- **不引入任何第三方框架/DLL**；新能力优先用 Win32/GDI+/系统多媒体栈原生实现。
- 严格分层单向依赖：`ui → render → media → core → config`，禁止反向 include。
- 菜单为 owner-draw：每项 `MF_OWNERDRAW` + `MenuDrawData`（文字/图标 key/是否子菜单）经 `dwItemData` 传给 `menu_icons.cpp` 的 `OnMeasureMenu/OnDrawMenu`；图标为 Lucide（ISC）48px PNG 内嵌 RCDATA；勾选项图标槽改画 ✓。
- 中文字面量：源文件 UTF-8 + 编译选项 `/utf-8`；窄字面量转 `wchar_t` 用 `muyu::U8()`。
- 新增可调参数/文案/档位一律进 `config/layout.h`，不散落魔法数。
- 新增状态字段一律进 `AppState`，并同步 `settings.cpp` 读写与范围校验。

## 5. 数据持久化

### 5.1 应用设置 `HKCU\Software\WoodenFish`

| 值名 | 类型 | 含义 | 校验 |
|---|---|---|---|
| `merit` `daily` | REG_BINARY (u64) | 总功德 / 今日功德 | — |
| `date` | REG_DWORD | yyyymmdd，跨天则 daily 清零 | 读时比对今天 |
| `scale` | REG_DWORD | 缩放×1000 | 300..2000 |
| `topmost` | REG_DWORD | 置顶 | 0/1 |
| `pin` | REG_DWORD | 固定（禁左键拖动） | 0/1 |
| `vol` `auto` `goal` `word` `skin` `zen` | REG_DWORD | 各档位索引 | ≤ 各自上限 |
| `goalX` | REG_DWORD | 自定义目标值 | goal=5 时生效 |
| `zenFile` | REG_SZ | 本地禅定音频路径 | zen=7（本地文件）时生效 |
| `log\yyyymmdd` | REG_BINARY (u64)（子键 log 下每日一条） | 功德簿：当日最终功德，随每次保存刷新 | 读时按值名解析日期 |

旧枚举值兼容：升级/重构不改键名与格式，MinGW 时代存档可直接读取。

功德簿语义：`SaveSettings` 每次刷新当日条目，故"当日最终功德"即当天最后一次保存时的 daily；"重置功德"会把当日条目一并拉低（历史记录不动），属预期行为。

### 5.2 开机自启
`HKCU\Software\Microsoft\Windows\CurrentVersion\Run\WoodenFish = "<exe 全路径>"`；值的存在与否即菜单勾选态。

## 6. 常见坑（踩过的）

| 症状 | 原因 / 解决 |
|---|---|
| `gdiplus.h` 报 `IStream/PROPID 未声明` | 定义了 `WIN32_LEAN_AND_MEAN`。MSVC 的 GDI+ 头需要完整 windows.h 的 COM 链；去掉该宏（或在 gdiplus 前 `#include <objidl.h>` 并补 propidl） |
| rc.exe 中文资源乱码/编译失败 | `app.rc` 首行必须 `#pragma code_page(65001)` |
| 链接 LNK1104 打不开 exe | 旧实例还在运行，先 taskkill |
| 首击无声 | 忘记把音频解码放在窗口显示之前（启动门控），或 MF 未 `MFStartup` |
| 连击音高一直不变 | `PlayKnock` 需把 `combo` 传入并作用于 `SetFrequencyRatio`，注意先自增再播 |
| 小窗口下文字消失 | 字体按逻辑像素随窗口缩小；painter 中已按"最小物理像素"反算字号（功德 13px / 目标 11px），新文字元素同理 |
| Git Bash 下跑 PowerShell 检查窗口 | `$` 被 bash 展开/中文乱码；用 `-EncodedCommand`（UTF-16LE→base64）或按 ASCII 类名 `WoodenFishWnd` 匹配 |
| `taskkill` 参数被 MSYS 改写 | 前缀 `MSYS_NO_PATHCONV=1` |
| 内存 DLGTEMPLATE 对话框"点了没反应" | `DialogBoxIndirectParamW` 失败且静默。DLGTEMPLATE 字段顺序是 style→exstyle→**cdit**→x,y,cx,cy→menu→class→title→[字号+字体名]；cdit 必须紧跟 exstyle，多写/错位一个 WORD 全盘错。字体大小字段按**整点**存（9=9pt）。可把 prompt.cpp 单独编成小测无头验证（找 #32770/填值/PostMessage IDOK） |
| DLGITEMTEMPLATE 里类名写成 `0xFFFF`+字符串 | 0xFFFF 前缀表示"序数 atom"，其后应是 WORD 而非字符串；内联类名字符串**不加** 0xFFFF 前缀。写错时 DialogBoxIndirectParamW 返回 -1 且 err=0，WM_INITDIALOG 根本不触发；可用 CreateDialogIndirectParamW（非阻塞、逐项返回句柄）做模板变体探针定位 |
| app.rc 里嵌 RT_MANIFEST 后链接报 CVT1100 资源重复 | link.exe 默认也嵌入自身 manifest；加 `/MANIFEST:NO` 让 RC 里的那份生效 |
| 公共控件（SysListView32）弹窗前 | 必须先 `InitCommonControlsEx(ICC_LISTVIEW_CLASSES)`，否则控件类未注册、对话框创建失败；且需 exe 内 manifest 声明 comctl32 v6，否则得到 Windows2000 风格灰皮控件 |
| `FindResourceW` 找不到 rc 里的字符串名资源 | rc.exe 把带引号的字符串资源名**连引号一起、且转大写**存入 exe（`"ic_pin"` → `"IC_PIN"`）；查找串须原样带引号与大写。另：`MEASUREITEMSTRUCT` 没有 hwndItem，owner-draw 菜单量尺寸时拿不到菜单句柄，文字等元数据只能走 `itemData` |
| 裁出来的 mp3 是纯静音 | 素材整段落在源曲目空白处；`-ss` 快进定位也可能落到错误偏移。交付前必查 `ffmpeg -af volumedetect`（mean 应远大于 -91dB） |
| mp3 时长/码率头显示异常（如 210s 报 54s） | `-ss` 放在 `-i` 前的快裁导致 Xing 头损坏；改输出端精确定位，仍不对就 wav 往返重编并 `-write_xing 1` |
| ffmpeg geq 表达式报"Undefined constant"或"A luminance or RGB expression is mandatory" | geq 坐标变量是大写 `X,Y`；对 gbrap 只写 a 表达式不行，r/g/b 必须给恒等式 `p(X,Y)` |

## 7. 如何扩展

- **加一档皮肤**：`config::` 加名字 → `render/skins.cpp` 的 `GetSkinAttr` 加分支（ColorMatrix）→ `ui/menu.cpp` skinNames 数组补名并在处理分支放宽计数。
- **加一条福语**：只改 `config/layout.h::kWords` 与 `kWordCount`。
- **加一个设置项**：`AppState` 加字段 → `LoadSettings/SaveSettings` 加键 → 菜单 `IDM_*_BASE` 加组 → `ShowMenu` 建子菜单+分发分支。
- **换/加内置禅曲**：替换或追加 `resources/audio/zenN.mp3`（对应 IDR_ZENN），并同步 `config::kZenNames` 与 `kZenTrackCount`。素材须按同一管线离线处理：两遍 loudnorm 归一 -23 LUFS / TP -2.1 → 3s 淡入淡出 → 44.1k 立体声 80k CBR。**一律用完整曲目、不裁切**（禅定音整曲解码进内存后才起播，曲长与常驻内存成正比，约 176KB/秒）。**编码后必须 `ffmpeg -af volumedetect` 验非静音、完整解码验时长**（注意：CBR mp3 的容器头时长可能虚短，以 `ffmpeg -i x -f null -` 解出的 time 为准，勿信 ffprobe duration）。用户也可在菜单"本地音频文件…"自选，无需重编。
- **改敲击时序**：鼠标路径为**左键抬起才起挥**（`WM_LBUTTONUP` 且按住期间未拖动；拖过只移动窗口不敲）；起挥在 `ui::DoKnock`（记 `knockAt`），触鱼结算在 `ui::Strike`（由 16ms 动画定时器在 `kSwingMs` 后驱动）；声音/挤压/波纹一律以 `impactAt` 为时基，勿再挂到按下时刻。
- **加新的常驻/持续动画**：动画 16ms 定时器**按需挂载**（`ui::SyncAnim` 依 `render::AnimActive` 起停，静止即摘表省 CPU）。新增状态型动画（类似"达成呼吸"）必须并入 `AnimActive` 判定，否则定时器不会为它启动；菜单里改动能引起动画起停的（如目标档），Render 之后记得补 `SyncAnim(ctx)`。
- **睡眠定时（禅定音）**：`ctx.sleepMin` 会话级不落盘；`kTimerSleep` 到点挂 `kTimerFade`（100ms×50 步 `SetZenFade` 降增益），走完 `ApplyZen(0)` 并把 zen=0 落盘；任何手动切曲/改档都要先 `KillTimer(kTimerFade)` 并复位系数。
- **升版本号**：只改 `resources/version.h`（三数字 + 字符串两处），exe 文件属性（app.rc VERSIONINFO）与"关于"页（`config::kVersion`）自动同步；发版说明见 README Releases。

## 8. 验证清单（改完跑一遍）

1. 启动：无窗口前无报错，右下角出现木鱼（工作区右下角、距边 40px），托盘有图标。
2. 点击：左键抬起棒槌才起挥、触鱼一刻才响/挤压/波纹/飘字；按住拖动只移窗不敲；快速连点音调升高，10/30/50 连击出金字。
3. F8 全局热键敲击；托盘双击隐藏/恢复。
4. 菜单逐项：尺寸三档、音量四档、自动敲击四档、目标（含"自定义…"弹窗）、皮肤、禅定音（6 曲+本地文件）、置顶、固定、开机自启、功德簿、重置、关于（版本与文件名属性一致）。
5. 退出重开：功德/全部设置复原（注册表兼容性）；窗口仍固定右下角（位置不记忆）。
6. `build/` 目录有且仅有一个 `电子木鱼.exe`。

---

返回：[架构](architecture.md) · [类图](class-diagram.md) · [时序图](sequence-diagrams.md)
