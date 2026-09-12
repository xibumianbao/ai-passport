# 初始化、命令与恢复

所有命令以 `python -X utf8 "<skill>/scripts/ida.py"` 开头，`<skill>` 为安装目录。全局参数必须放在子命令前：`--profile`、`--data-dir`、`--output`、`--events-output`。输出为 UTF-8 逐行 JSON，末行 `event=result` 是核验结果；`docs --format markdown` 还会先输出文档列表。

## 环境与凭据

- Python 3.10+；Windows 使用 Credential Manager，默认不需 pip 安装。
- macOS / Linux 安装 `python -m pip install keyring`；显式使用 macOS Keychain / Linux Secret Service 后端，需要当前会话的系统凭据库可用。没有凭据库的服务器用 `--credential-store env`，由宿主把 `IDA_CLIENT_ID`、`IDA_CLIENT_SECRET`（可选 `IDA_PROXY_USER`）安全注入每次调用进程。不要把真实值写入命令、`.env` 或仓库。
- 系统凭据条目服务名为 `ida-agent-client`，每个 profile 对应独立随机引用；Windows target 为 `ida-agent-client/<uuid>`。可在 OS 凭据管理界面删除，移除后需再次初始化。
- 初始化默认鉴权并短测；只存配置可加 `--skip-smoke`；重置指定 profile 用 `--replace`，其他 profile 不受影响。
- 默认数据目录：Windows `%LOCALAPPDATA%/ida-agent-client`；macOS `~/Library/Application Support/ida-agent-client`；Linux `${XDG_DATA_HOME:-~/.local/share}/ida-agent-client`。可用全局 `--data-dir` 覆盖。配置不含 Client ID / Secret / JWT，但包含页面链接与任务 IDs，属于本机用户。
- 系统凭据不能跨 OS 登录用户复用。复制 Skill 的接收方需自己的首次初始化。

## 命令速查

| 命令 | 用途 | 参数 |
|---|---|---|
| `init` | 解析 URL、鉴权、存凭据、短测 | `--agent-url`，可选 `--region`、`--base-url`、`--jwt-url`、`--a2a-url`、`--replace` |
| `profiles` / `show` | 列配置 / 查看选中配置 | 全局 `--profile` |
| `doctor` / `auth` | 验证配置与鉴权，不创建任务 | 无 |
| `session-create` | 单独创建会话 | `--name` |
| `session-info` | 查看会话历史 | `--session-id` |
| `chat` | 发问并跟踪核验结果 | `--message` 或 UTF-8 `--message-file`，可选 `--session-id`、`--name`、`--mode` |
| `status` | 当前任务快照 | `--task-id`，可选 `--session-id` |
| `result` | 对照任务和保存消息核验答案 | 同上 |
| `subscribe` | 重新订阅已有任务 SSE | 同上及观察参数 |
| `resume` | 从本地记录恢复原任务 | `--run-id` 及观察参数 |
| `cancel` | 取消活动任务并读回状态 | `--task-id`，可选 `--session-id` |
| `docs` | 官方文档链接及当前端点 | `--format json` 或 `markdown` |

观察参数用于 chat / subscribe / resume：`--deadline 300`（0–86400 秒，不含 0）、`--idle-timeout 45`（0–3600 秒，不含 0）、`--max-reconnects 1`（0–3）、`--cancel-on-timeout`、`--show-process-text`。期限用于 SSE 观察，不是进程硬停止时间；鉴权、创建会话、状态/答案回查和取消确认可能另需时间。

`status` 返回 `result_not_checked=true`，只确认任务状态。用户问“真的回复了吗”必须执行 result。初始化 VERIFIED 只证明短问答跑通，不代表所有业务能力或下游工具可用。

## 示例

```text
python -X utf8 "<skill>/scripts/ida.py" --profile finance init --agent-url "https://console.volcengine.com/bi/datawind/data-agent/analytics-agent/pages/agent/home?agentId=101&appId=202&region=cn-shanghai"
python -X utf8 "<skill>/scripts/ida.py" --profile finance doctor
python -X utf8 "<skill>/scripts/ida.py" --profile finance chat --message "你好，请用一句话介绍你能做什么。"
python -X utf8 "<skill>/scripts/ida.py" --profile finance result --task-id <taskId>
python -X utf8 "<skill>/scripts/ida.py" --profile finance resume --run-id <runId> --deadline 600
python -X utf8 "<skill>/scripts/ida.py" --profile finance chat --session-id <sessionId> --message "接着解释第二点。"
python -X utf8 "<skill>/scripts/ida.py" --profile finance --output "<本地报告路径>.json" result --task-id <taskId>
```

101 / 202 是占位值，换成实际完整 URL。凭据在 init 的隐藏提示中输入。`--output` 会覆盖指定报告文件，请选择明确的新路径；`--events-output` 必须是新文件。输出脱敏本次已知凭据和常见 token 字段，但仍有业务内容，不应直接公开。

## 故障处理

| 结果或错误 | 下一步 |
|---|---|
| `INITIALIZATION_REQUIRED` | 收集缺失 URL / Client ID / Secret，init |
| `PROFILE_EXISTS` | doctor 复用；确需换连接再 --replace |
| `AUTH_FAILED` / `AUTH_HTTP_ERROR` | 核对部署、凭据、代理用户权限和服务前缀 |
| `HTTP_ERROR` 403/404 | 检查 `/bi/datawind` 前缀、region 和开放 API 权限 |
| `NETWORK_ERROR` | 请求可能已接收；写入请求先查原会话，不重复发送 |
| `CREDENTIAL_READ_FAILED` | 检查 OS 身份和凭据库，不等于远程凭据无效 |
| `SUBMISSION_UNKNOWN` / `SUBMISSION_UNCONFIRMED` | 保留 run/session，session-info 找到对应 taskId 后 subscribe |
| `PROFILE_CHANGED` | 用原 profile/凭据上下文核查，勿改恢复记录绕过检查 |
| `STREAM_TIMEOUT` / `STREAM_DISCONNECTED` | result 后 resume / subscribe 原 taskId |
| `CANCEL_PENDING` | 取消已受理但未确认，稍后 status |
| `COMPLETED_NO_FINAL_ANSWER` | 再 result；仍空则报告缺少答案，检查 QA 模式和 Agent 配置 |
| `ARTIFACT_REVIEW_REQUIRED` | 核验报告引用及内容是否满足目标 |

会话详情最多取 200 条消息；很旧任务的答案不在窗口时会保守报告未核验，不能据此断言从未回复，更不能误取邻近轮次答案。

退出码：0 为命令成功（问答要求 SUCCESS），1 为配置/协议/传输错误，2 为非成功业务结果或未完成核验，130 为本地中断。`--skip-smoke` 返回 2 表示尚未完成对话验证。程序正常退出或 HTTP 成功不等于业务完成。
