# 功能索引 · 1.0.0-rc.3

板级代码目录简称 `board/`，完整路径为 `xiaozhi/overlay/main/boards/waveshare/esp32-s3-rlcd-4.2/`。

| 标记 | 功能 | 主要代码 | 文档 / 验证 |
| --- | --- | --- | --- |
| UI-01 | 开屏、首页、性能、对话、用电、电脑选择、关于页 | `simulator/src/ui/ui.c`、`board/ui.c` | [页面与按键](features/UI_AND_BUTTONS.md) |
| UI-02 | 首页三种 Agent 模式、语音名称和作者署名 | `board/settings_portal_service.cc`、`board/config.json` | [界面设计](UI_REDESIGN_EDITORIAL.md) |
| PC-01 | 性能、网易云、稳定身份和电脑发现 | `reporter/reporter.py`、`reporter/media_monitor.py`、`board/reporter_service.cc` | [Reporter](../reporter/README.md) |
| CODEX-01 | 原生文件发现、App Server、额度缓存 | `reporter/platform_support.py`、`reporter/codex_rpc.py`、`reporter/reporter.py` | [额度与恢复](features/CODEX_AND_REPORTER.md)、`reporter/test_codex_quota.py` |
| ELEC-01 | 余电和累计用电查询 | `reporter/electricity_monitor.py`、`board/electricity_service.cc` | [用电说明](ELECTRICITY_RECHARGE.md) |
| ELEC-02 | 100 / 50 元待支付码、10 度提醒 | `reporter/native_ui.swift`、`reporter/electricity_monitor.py`、`board/ui.c` | `reporter/test_electricity.py` |
| ELEC-03 | 日均耗电、预计天数、私有历史 | `reporter/electricity_history.py`、`board/electricity_estimate.h` | [预计天数](ELECTRICITY_RECHARGE.md#预计可用天数) |
| POWER-01 | 5 秒发现、相同帧跳过 SPI、15 秒 ADC 缓存 | `board/reporter_service.cc`、`board/custom_lcd_display.cc`、`board/waveshare-s3-rlcd-4.2.cc` | [待机优化](features/POWER.md) |
| BUILD-01 | 固定上游覆盖、二维码符号隔离 | `xiaozhi/upstream.lock`、`xiaozhi/apply_ui_overlay.py` | [构建](BUILD.md) |
| RELEASE-01 | 按提交打包源码、SHA-256 校验 | `release/package_source.py` | [打包与发布](PUBLISHING.md) |

## 平台范围

| 能力 | ESP32 RLCD | macOS Reporter | Windows Reporter |
| --- | --- | --- | --- |
| 状态、性能、额度与媒体 | 显示 | 采集与原生窗口 | 原有采集与窗口，本轮未在 Windows 重测 |
| 宿舍余电 / 预计天数 | 独立查询，可接收电脑估算 | 窗口配置与历史统计 | 共用后端具备逻辑，图形设置入口未集成 |
| 充值提醒 | 同步电脑二维码和支付状态 | 设置、生成、查询、自动弹窗 | 后端接口可用，本版未提供图形充值页或安装包验收 |
| 待机优化 | 减少后台操作 | 长连接与限频查询 | 共用后端，实际兼容性待 Windows 验证 |

“已实现”表示代码已整合。本次运行过的验证和历史设备检查分别记录在 [验证记录](RELEASE_VALIDATION_2026-10-20.md)。
