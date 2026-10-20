# 1.0.0-rc.3 发布验证 · 2026-10-20

验证对象为当前发布源码，基于远程主分支 `e0a4326` 整合。本机为 Apple Silicon macOS；下列检查在发布前实际执行。

## 本次结果

| 检查 | 结果 |
| --- | --- |
| Reporter `python -m unittest discover -v` | 共 85 项：77 通过、8 跳过 |
| LVGL 9.5.0 / SDL2 模拟器 CMake 构建 | 通过 |
| Swift / AppKit 窗口 `swiftc -typecheck`，最低 macOS 13 | 通过 |
| 真实 UI 主机测试 | 通过：开屏消失、页面导航、黑沐与玉米署名、预计天数、50 元选择和关闭充值页 |
| 固件按键回调主机测试 | 通过：普通页、电脑选择、空列表、组合键、充值弹层路由与启动保护 |
| 固件额度渲染主机测试 | 通过：缓存、恢复、缺失、断开、0% / 100%；缓存文字宽度 49 px |
| 设备 / 模拟器 UI 正文同步 | 通过，仅 include 路径不同 |
| 电表时间兼容性 | 通过：无时区日期按 UTC+8 解析，显式时区保持原值，不依赖 Windows 额外时区数据 |
| 改动空白检查 | `git diff --check` 通过 |
| 上传文件检查 | 未发现常见 GitHub / API 密钥或私钥标记；公开楼栋、房间默认 0 |

跳过项：Windows 注册表 2 项、Windows 音频 4 项、缺少 PySide6 的 Windows UI 1 项，以及因运行中的 Reporter 占用 UDP 8766 而跳过的 Mac 完整发现测试 1 项。没有为测试停止用户的现有服务。

首页和关于页已查看实际渲染输出，分别核对预计天数行与双作者署名。截图使用示例数据，不是用户当前余电。

## 可复现入口

准备 Python 依赖、CMake、SDL2、LVGL 9.5.0 后，从源码根目录运行：

```bash
(cd reporter && python -m unittest discover -v)
python3 tests/check_ui_branding.py
python3 tests/check_panel_buttons.py
python3 tests/check_quota_stale.py
git diff --check
```

macOS 的 Swift 类型检查使用 Cocoa / Foundation / ServiceManagement；主机回调测试支持 `CXX` 指定的 C++17 编译器，Windows 仍支持 Visual C++ / `SYNA_VCVARS`。

源码压缩包从最终提交生成，包内文件与提交逐一核对并运行 ZIP 校验，最终提交和包 SHA-256 记录在 Release 附件 `SHA256SUMS.json`。其不作为签名或真实支付证明。

## 历史设备检查与本轮边界

2026-10-10：配套固件完成 16MB 备份、USB 写入哈希校验、启动、Wi-Fi 联网、Reporter 发现及音频 / 唤醒初始化；当时未发现崩溃和看门狗复位。学校 100 元、50 元待支付二维码曾验证获取，未执行真实付款。

本轮是源码整理发布，没有重新刷写设备，也没有重新编译 ESP-IDF 完整固件或 Windows 安装器。公开版取消本机宿舍默认值，与之前私人设置分开。持续电池续航、真实付款到账、Windows 新功能窗口、其他电脑和系统兼容性仍需各自验收。
