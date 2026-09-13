---
name: ida-agent-client
description: 初始化并远程调用火山引擎 IDA 智能体。首次收集完整 Agent 前端 URL、Client ID 和 Secret，完成鉴权、会话创建、流式问答、任务状态和最终结果核验；支持继续会话、断线重订阅、任务恢复、取消与接口文档输出。用户要求调用 IDA、测试 IDA 接口、查看远程任务是否真正回答、排查空答案或复用 IDA 对接时使用。
---

# IDA 远程智能体

使用自带 `scripts/ida.py` 执行请求，不再临时重写鉴权和 SSE 解析代码。先找到本 Skill 的绝对目录，以下命令的 `<skill>` 替换为该目录。需要 Python 3.10+。Windows 直接使用系统凭据管理器，无第三方 Python 依赖；其他平台见 [初始化与命令](references/usage.md)。

## 首次使用

1. 运行 `python -X utf8 "<skill>/scripts/ida.py" profiles`。已有合适 profile 时先 `--profile <name> doctor`，无需重新收集凭据。多个 profile 无法从上下文确定时再让用户选择。
2. 缺少配置时，一次性收集 **Agent 前端对话页面的完整 URL、Client ID、Secret**。当前会话已提供的值直接复用；优先让用户在终端隐藏输入凭据。URL 应包含 `agentId`、`appId`（若有）、`region`。
3. 初始化会解析 URL、鉴权、把凭据写入当前用户系统凭据库，保存无密钥 profile，并发起一次“1+1”短问答，检查最终保存的答案是否为 `2`：

   ```text
   python -X utf8 "<skill>/scripts/ida.py" --profile default init --agent-url "<完整前端 URL>"
   ```

   交互式终端依次隐藏输入 Client ID、Secret。工具已持有凭据时，可加 `--credentials-prompt-json`，通过 **PTY 隐藏输入**传入 JSON 对象，字段为 `clientId`、`clientSecret`、可选 `proxyUser`。不要把密钥插入命令参数、脚本、临时文件或普通回显 stdin。无交互环境使用宿主安全注入的 `IDA_CLIENT_ID` / `IDA_CLIENT_SECRET`，加 `--credential-store env`。
4. 只有 `initialization=VERIFIED` 才说明初始化问答通过。鉴权成功、创建会话成功或 `HTTP 200` 均不足。失败后保留 profile 与 run，先检查原任务，不重新发问题。
5. 非标准 SaaS 前端 URL 需要部署方正式 API base URL，通过 `--base-url` 等显式配置，不猜域名或路由。前端和 API 必须同源。缺少 region 时补充 `--region`。

初始化会创建一个云端测试会话。用户只要求配置、暂不测试时用 `--skip-smoke`，明确结果是 `AUTHENTICATED_CHAT_UNVERIFIED`。已有 profile 默认不覆盖；用户要求换连接时用 `--replace`，旧 run 与新凭据隔离。

## 问答和结果检查

默认创建新会话，普通问答使用 `knowledge-qa`。只有用户明确要深度探索/研究时选择 `--mode deep-research`。

```text
python -X utf8 "<skill>/scripts/ida.py" --profile default chat --message "<用户的问题>"
python -X utf8 "<skill>/scripts/ida.py" --profile default chat --session-id <sessionId> --message "<追问>"
python -X utf8 "<skill>/scripts/ida.py" --profile default result --task-id <taskId>
```

输出逐行 JSON，包括 `run_id`、`session_id`、`task_id`、状态变化和最终 `event=result`。原样保留这些 ID 用于恢复和回查。对话模式不是工具权限边界；提交问题前遵守用户授权范围，不得借此委托未获授权的外部消息或操作。

验收规则：

- `SUCCESS`：GetTask 快照为 `TASK_STATE_COMPLETED`，且会话中 **同一 taskId** 的 `finalAnswer` 已保存。展示 `answer` 原文，再说明任务状态。
- `IN_PROGRESS`：观察结束，云任务仍运行。提供原任务 ID 和恢复方式，不宣称失败或重发问题。
- `COMPLETED_NO_FINAL_ANSWER`：任务结束但未核验到最终答案。流式过程文本、`lastChunk`、`final=true`、页面“已完成”均不足。必要时再次 `result`。
- `ARTIFACT_REVIEW_REQUIRED`：保存了 `finalReport` 引用。展示引用并说明尚未核验文件内容，不声称报告已成功下载或完成业务验收。
- `CANCELED` / `FAILED` / `REJECTED`：按权威任务快照报告。历史重建事件里的 completed 不得覆盖 canceled。
- `INPUT_REQUIRED`：需要用户输入；汇报需要补充的内容后再继续原会话。

`--show-process-text` 仅显示标为 `is_final_answer=false` 的过程文本，不能作为答案交付。`summary.hasFinalArtifact` / `hasFinalMessage` 不是唯一判据。脚本验证 API 结果，`ui_verified=false` 是正常边界；只有实际观察页面或用户提供截图才能说前端显示已验证。

## 恢复、取消和诊断

```text
python -X utf8 "<skill>/scripts/ida.py" --profile default status --task-id <taskId>
python -X utf8 "<skill>/scripts/ida.py" --profile default resume --run-id <runId>
python -X utf8 "<skill>/scripts/ida.py" --profile default subscribe --task-id <taskId>
python -X utf8 "<skill>/scripts/ida.py" --profile default cancel --task-id <taskId>
python -X utf8 "<skill>/scripts/ida.py" --profile default session-info --session-id <sessionId>
python -X utf8 "<skill>/scripts/ida.py" --profile default docs
```

- 默认观察 300 秒、空闲 45 秒，最多重订阅一次；用 `--deadline`、`--idle-timeout`、`--max-reconnects` 调整。超时/本地中断默认保留云任务。只有用户要求超时停止或初始化测试才加 `--cancel-on-timeout`。取消后回查 GetTask。
- 断线只重订阅已有任务，绝不自动重复 SendStreamingMessage。没有 taskId 的 `SUBMISSION_UNKNOWN` / `SUBMISSION_UNCONFIRMED` 必须先按 sessionId 查历史，确认对应任务后 subscribe，避免重复执行。
- 身份、Agent、会话不匹配立即停止；不要尝试另一用户或 profile 绕过。鉴权 401/明确 tokenExpired 最多刷新一次 JWT，网络错误、5xx 或业务错误不重放提交。
- 若主机网络沙箱或 OS 登录上下文导致失败，先按宿主权限机制使用获准的真实用户执行上下文，不要将其报告为凭据失效。
- 详细操作与错误处理读 [初始化与命令](references/usage.md)；协议字段和完整文档地址读 [API 参考](references/api.md) 或运行 `docs`。

## 保存、共享和安装

凭据保存在当前 OS 用户的系统凭据库；JWT 只在进程内存。profile 和恢复记录在用户数据目录；恢复记录只存 IDs、状态、问题摘要哈希，不存原问题或答案。仅显式指定 `--output <path>` / `--events-output <new-path>` 才保存回答报告/脱敏事件；它们仍可能含业务内容，不加入共享包。

分享整个 `ida-agent-client` 文件夹或 ZIP。接收方解压到自己的 `$CODEX_HOME/skills/ida-agent-client`（默认 `~/.codex/skills/ida-agent-client`），在新 Codex 任务中输入 `$ida-agent-client` 或“用 IDA 回答……”。每个接收方首次提供自己的 URL 和凭据，之后复用本地 profile。不要复制发送方用户数据目录、凭据库或测试记录。
