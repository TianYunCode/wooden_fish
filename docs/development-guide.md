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
  app.rc              资源脚本（#pragma code_page(65001) 必需）
  resource.h          IDR_FISH/GU/KNOCK/ZEN = 101..104
  app_icon.ico  images/*.png  sounds/*.mp3  zen.mp3
src/
  main.cpp            wWinMain
  app_context.h       依赖聚合根
  config/layout.h     全部静态配置常量
  core/               util / app_state / settings / autorun
  media/              assets / audio_engine
  render/             skins / painter
  ui/                 main_window / menu
build/                交付目录（只放 电子木鱼.exe）
build_msvc/           CMake 二进制目录（可整体删除重建，勿提交 git）
```

## 4. 编码约定

- **不引入任何第三方框架/DLL**；新能力优先用 Win32/GDI+/系统多媒体栈原生实现。
- 严格分层单向依赖：`ui → render → media → core → config`，禁止反向 include。
- 菜单一律纯文字（`MF_STRING` + `MF_CHECKED`），不加图标。
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
| `vol` `auto` `goal` `word` `skin` `zen` | REG_DWORD | 各档位索引 | ≤ 各自上限 |
| `goalX` | REG_DWORD | 自定义目标值 | goal=5 时生效 |
| `zenFile` | REG_SZ | 本地禅定音频路径 | zen=6 时生效 |

旧枚举值兼容：升级/重构不改键名与格式，MinGW 时代存档可直接读取。

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

## 7. 如何扩展

- **加一档皮肤**：`config::` 加名字 → `render/skins.cpp` 的 `GetSkinAttr` 加分支（ColorMatrix）→ `ui/menu.cpp` skinNames 数组补名并在处理分支放宽计数。
- **加一条福语**：只改 `config/layout.h::kWords` 与 `kWordCount`。
- **加一个设置项**：`AppState` 加字段 → `LoadSettings/SaveSettings` 加键 → 菜单 `IDM_*_BASE` 加组 → `ShowMenu` 建子菜单+分发分支。
- **换背景乐**：替换 `resources/zen.mp3`（保持 IDR_ZEN 与 ≤ 数百 MB 内存可接受），重新构建即可，代码零改动。

## 8. 验证清单（改完跑一遍）

1. 启动：无窗口前无报错，右下角出现木鱼，托盘有图标。
2. 点击：有声音、飘字、按压动画；快速连点音调升高，10/30/50 连击出金字。
3. F8 全局热键敲击；托盘双击隐藏/恢复。
4. 菜单逐项：尺寸三档、音量四档、自动敲击四档、目标、皮肤、禅定音、置顶、开机自启、重置。
5. 退出重开：功德/位置/全部设置复原（注册表兼容性）。
6. `build/` 目录有且仅有一个 `电子木鱼.exe`。

---

返回：[架构](architecture.md) · [类图](class-diagram.md) · [时序图](sequence-diagrams.md)
