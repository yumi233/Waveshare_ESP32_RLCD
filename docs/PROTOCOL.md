# 局域网协议与电费命令

协议标记保持 `AI_PANEL_REPORTER_V1`，新增字段供新版设备使用。

## 发现与状态

- UDP 8766 接收 `AI_PANEL_DISCOVER_V1`，返回电脑身份、指纹、性能、Agent、额度、媒体和 `electricity` 快照。
- macOS HTTP 默认 18765，Windows 默认 8765。
- `GET /api/v1/health`：服务状态。
- `GET /api/v1/status`：汇总数据，不包含充值命令令牌。
- `GET /api/v1/pairing`：仅本机可读的旧版配对信息，不应公开。

用电快照包含 `configured`、`available`、`stale`、`remaining_kwh`、`used_kwh`、`daily_kwh`、`estimated_days`、`estimate_state`，以及付款页所需的状态、金额、二维码和过期时间。未形成估算时 `estimated_days=null`。

订单状态为 `idle / creating / ready / expired / paid / failed / error`。订单标识留在电脑后台，设备只接收显示和操作所需的数据。

## 充值命令

所有写接口要求 `Content-Type: application/json`，请求体限制 1024 字节。服务拒绝非允许来源的浏览器 Origin。本机和设备调用的权限不同：

| 路径 | JSON | 范围 |
| --- | --- | --- |
| `POST /api/v1/electricity/recharge` | `{"amount_yuan":100}` 或 `{"amount_yuan":50}` | 请求学校生成待支付码，不执行付款 |
| `POST /api/v1/electricity/check` | `{}` | 查询当前订单 |
| `POST /api/v1/electricity/dismiss` | `{}` | 关闭本次低余电提醒 |
| `POST /api/v1/electricity/config` | `{"enabled":true,"building":1,"room":101,"threshold_kwh":10}` | 仅电脑本机修改配置，房间为示例 |

设备发现响应带有短期 `command_token`，远程 POST 使用 `Authorization: Bearer <token>`。令牌关联来源 IP 与设备发现记录，记录过期后失效。配置写入额外限制为回环地址。设备还检查选定 Reporter 的身份、指纹和当前宿舍匹配关系，避免将另一房间的二维码显示在本机。

该机制针对可信局域网中的误串设备和普通网页误触，不等同于加密通道或完整双向认证。发现消息及学校充值入口沿用现有局域网 / HTTP 路线。不要开放公网端口，也不要发布真实令牌、二维码、订单信息或设备配置。
