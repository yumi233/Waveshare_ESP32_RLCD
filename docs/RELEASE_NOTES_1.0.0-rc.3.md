# 夏柠 1.0.0-rc.3 · 功能整合源码版

发布日期：2026-10-20。仓库：[yumi233/Waveshare_ESP32_RLCD](https://github.com/yumi233/Waveshare_ESP32_RLCD)。

## 功能标记

- **UI-01 / UI-02**：夏柠开屏、角色像素界面和按键说明，三种 Agent 模式及独立的预计天数行。
- **PC-01 / CODEX-01**：电脑采集与 Codex 额度长连接修复，处理原生 CLI 路径、登录、EOF、缓存和重连，macOS 菜单栏后台运行。
- **ELEC-01 / ELEC-03**：五邑大学宿舍余电、累计用电与预计天数，首次至少积累 24 小时记录。
- **ELEC-02**：默认 10 度提醒，100 / 50 元学校待支付二维码，支持结果查询、过期隐藏、稍后处理及房间变更。
- **POWER-01**：降低发现频率，减少重复 SPI 传输并缓存电池采样，实际续航增幅尚未测量。
- **BUILD-01 / RELEASE-01**：固定上游、二维码符号隔离、功能文档、源码包和校验清单。

代码位置见 [功能索引](https://github.com/yumi233/Waveshare_ESP32_RLCD/blob/v1.0.0-rc.3/docs/FEATURES.md)，本次执行的检查见 [验证记录](https://github.com/yumi233/Waveshare_ESP32_RLCD/blob/v1.0.0-rc.3/docs/RELEASE_VALIDATION_2026-10-20.md)。

## 发布文件

| 文件 | 用途 |
| --- | --- |
| `XiaNing-source-1.0.0-rc.3.zip` | 当前标签对应的项目源码、素材、文档、第三方许可 |
| `SHA256SUMS.json` | 源码提交、版本、文件大小与 SHA-256 |

本次仅发布源码，不将之前生成的固件、Mac 应用或 Windows 安装器标为本版新构建。程序内部版本仍为 1.0.0，发布标签区分本轮候选源码。

## 使用限制

充值图形窗口目前在 macOS 实现，Windows 共享后端逻辑，本轮未移植图形充值页或重新验收安装器。二维码需要电脑 Reporter 在线且双方宿舍一致。实际付款由用户扫码，学校到账可能延迟，测试未执行真实支付。

公开源码的宿舍默认未设置，不包含账户、Wi-Fi、配对令牌、用电历史或完整 Flash 备份。源码包不附完整 ESP-IDF / 小智依赖缓存，构建时按锁定版本获取。

## 作者

原项目作者：[黑沐 / heimumumu](https://github.com/heimumumu)，[原项目](https://github.com/heimumumu/Waveshare_ESP32_RLCD)。界面重构与功能定制：[玉米 / yumi233](https://github.com/yumi233)。保留根目录 MIT 和第三方授权，角色相关素材不按 MIT 重新授权。
