# LVGL 桌面可视化模拟器

该目录用于在 Windows 和 VS Code 中预览 ESP32-S3-RLCD-4.2 的 400×300 黑白界面，不需要连接或烧录开发板。

## 当前页面

- 首页：波奇酱头像、时间、温湿度、Agent 状态、可切换的任务和额度，以及媒体信息。
- 性能：资源占用、温度、网络延迟与流量走势。
- 夏柠：最近对话、待办和 API 余额。
- 电脑列表：发现、选择和连接 Reporter 电脑。
- 关于：原项目作者黑沐、界面重构者玉米及开源许可说明。

五页采用 400×300 单色编辑式排版。前三区使用实时 LVGL 文本、细分隔线和留白；底图资源保留供旧版布局参考，不参与当前页面绘制。

## 运行

在 VS Code 中执行任务 `UI Simulator: Run`，或者在终端运行：

```powershell
& .\simulator\scripts\run.ps1
```

模拟器以 2 倍比例显示，逻辑分辨率始终保持 400×300。

当前 Mac 的 LVGL 9.5.0 依赖保存在 1T 盘的 `toolchains/lvgl-9.5.0/`，`simulator/vendor/lvgl-9.5.0` 指向该目录。在项目根目录可运行：

```sh
cmake -S simulator -B simulator/build-mac -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/sdl2-compat
cmake --build simulator/build-mac -j 6
simulator/build-mac/bin/ai_panel_simulator
```

截图时可设置 `AI_PANEL_SCREENSHOT_PATH`、`AI_PANEL_SCREENSHOT_DELAY_MS=4800` 和 `AI_PANEL_AUTOCLOSE_MS=6500`。使用 `AI_PANEL_START_PAGE=dashboard|performance|syna|computers|about` 逐页预览；`AI_PANEL_AGENT_MODE=quotas` 或 `task` 预览首页另外两种 Agent 模式；默认是当前任务加每周额度。

首次构建会编译 LVGL，后续只会增量编译发生变化的文件。需要清空 CMake 配置缓存时可执行：

```powershell
& .\simulator\scripts\configure.ps1 -Fresh
```

## 键盘操作

| 按键 | 功能 |
| --- | --- |
| `Space` | 首页、性能、夏柠、关于之间循环 |
| `C` | 打开电脑列表 |
| `↑` / `↓` | 选择电脑 |
| `Enter` | 确认当前电脑并返回首页 |
| `Esc` | 返回首页 |
| `R` | 循环模拟 Working/Waiting/Done 状态 |

电脑列表也支持鼠标单击进行高亮选择。硬件移植时，这些操作将映射到开发板的 KEY 按键。

## 工程边界

- `src/ui/`：可移植的 LVGL UI 代码，后续由模拟器和 ESP32 固件共享。
- `src/main.c`：仅用于 Windows 的 SDL2 启动和键盘模拟。
- `vendor/lvgl-9.5.0/`：锁定的 LVGL 依赖，不直接修改。
- 构建产物输出到 `C:\ai-panel-build\simulator`，避免 Windows 工具链受到中文构建路径影响。

动态数字使用 LVGL 内置英文字体；当前首页中文已经转换为 1-bit 嵌入式位图，不依赖运行电脑的系统字体。后续需要大量动态中文时，再生成精简中文字库以控制 Flash 占用。

## 单色素材

- `assets/source/music_ui_icons/`：当前页面的时钟、温湿度、Agent、播放、歌曲、歌手、网络、电脑和电池图片生成素材。
- `assets/source/icons/` 与角色图片：上一版页面的备用素材，当前首页不引用。
- `assets/generate_assets.ps1`：摆放图片生成素材，与中文和布局线条合成为严格 400×300 的 1-bit 底图，再生成 LVGL C 数组；脚本不使用图形代码重画图标。
- `src/assets/`：可直接链接到模拟器、后续也可移植到 ESP32 固件的单色资源。

修改图标或静态布局后，在 VS Code 执行 `UI Assets: Regenerate`，再运行模拟器。

## 音乐数据范围

当前 UI 使用《夜的第七章》作为网易云音乐适配预览数据，并只显示一行歌词。电脑端 Reporter 后续通过 Windows 系统媒体会话取得标题、歌手、播放状态和进度；第一版歌词适配仅面向网易云音乐，其他播放器暂不提供歌词。
