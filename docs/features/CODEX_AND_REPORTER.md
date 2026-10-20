# PC-01 / CODEX-01 · Reporter 与额度恢复

## 数据链路

Reporter 采集性能、Codex 状态和账户额度，通过局域网 UDP 发现响应发给开发板。音乐信息与歌词链路保留。macOS 应用默认驻留菜单栏，关闭窗口仍采集，退出应用会停止由它启动的后台服务。

## 本版额度修复

图形应用启动时的 PATH 可能没有 Node，直接运行 npm JavaScript 启动脚本会失败。本版优先寻找桌面应用和 npm 平台包内的原生 Codex，再尝试其他候选路径。

- `platform_support.py`：发现候选文件，支持 `SYNA_CODEX_EXECUTABLE` 指定路径。
- `codex_rpc.py`：独立 stdio JSONL 会话，处理初始化、EOF、超时、固定错误分类及进程回收。
- `reporter.py`：调用 `account/read` 与 `account/rateLimits/read`，约每 30 秒轮询，每 5 分钟重建连接以加载新的登录状态和安装路径。

按返回的窗口长度区分短周期和周额度，`剩余百分比 = 100 - usedPercent`。没有可靠数据的窗口保持空值，界面显示 `--`。未登录时清空旧额度并显示登录提示。

临时失败最多保留 120 秒缓存并标记，缓存超时或窗口已到期则清除。只记录退出码和固定错误类别，不把原始认证输出写入日志。

## 检查方法

1. 在 Codex 客户端确认账户已登录。
2. 核对 Reporter 周额度；短周期为 `--` 可能是账户没有返回该窗口。
3. 只读诊断：macOS `http://127.0.0.1:18765/api/v1/status`，Windows `http://127.0.0.1:8765/api/v1/status`。
4. 路径检测失败时，用 `SYNA_CODEX_EXECUTABLE` 指定原生 CLI，再重启 Reporter。

服务不会读取提示词、代码或认证令牌，也不根据本地 token 消耗推测余额。App Server 协议随客户端更新可能需要适配。

测试入口：`reporter/test_codex_quota.py`、`test_reporter_recovery.py`、`test_macos.py`。平台使用细节见 [macOS](../../reporter/MACOS.md) 和 [Reporter 总览](../../reporter/README.md)。
