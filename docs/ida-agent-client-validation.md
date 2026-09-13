# IDA Agent Client 1.0.0 验证记录

验证日期：2026-09-13。代码和脱敏协议说明可复用；实际 URL、凭据、会话/任务 IDs 与原始响应不放入仓库或发布包。

## 已验证

- Python 3.13 / Windows：26 项自动化行为测试通过，包含 URL 解析、同源校验、凭据脱敏、profile/run 隔离、SSE 编码与中断、仅刷新一次 JWT、提交不盲重试、重订阅同一任务、明确取消与状态回查、最终答案按 taskId 匹配。
- Skill 官方 quick_validate 校验通过。
- 实际火山引擎 SaaS：初始化获取 JWT、写入/读回 Windows 凭据管理器、创建会话、发起普通 QA。问答“1+1”自然结束，最终保存答案为 `2`，初始化标记 VERIFIED。
- 独立进程 doctor 从系统凭据库复用配置，无需再次输入凭据。
- 对完成任务执行 SubscribeToTask，重订阅后 GetTask 和 sessionInfo 仍核验为 SUCCESS、答案 `2`。
- 同一会话追问并通过显式超时取消策略调用 CancelTask，GetTask 确认为 CANCELED。历史重建事件仍显示 completed，客户端正确报告冲突与取消状态，未借用上一轮答案。
- 根据本地 run_id 恢复前一轮，仍正确取到前一轮 `2`，不被后续取消轮次覆盖。
- docs 命令输出 9 个官方文档地址及当前 profile 解析的 API 端点。
- 安装目录 7 个源文件与发布包来源逐一比对通过，安装后独立运行 doctor 鉴权通过。发布源码与本次凭据在内存中比对通过，不含测试者的真实凭据、Agent 参数或本机绝对路径。

观察到的权限边界：Windows 沙箱执行上下文不能读取真实用户的凭据/受限数据目录；在获准的真实用户上下文运行后通过。不是通过削弱数据目录权限解决。

## 未覆盖的边界

- macOS Keychain / Linux Secret Service 适配已实现，但本轮没有对应操作系统的实机验证。无桌面凭据库的环境可用宿主环境变量注入模式。
- 当前实机验证为标准 SaaS；私有部署仅显式同源路由覆盖，需部署方验证。
- Skill API 验证不等于此次安装测试的前端 UI 验证，返回 ui_verified=false。此前对接会话的页面显示经用户截图确认，不推及所有新任务。
- finalReport 只验证保存的引用，不下载或验收报告内容；旧会话消息窗口最多 200 条。
- 本版本不处理多模态上传、跨域身份交换、工具审批和业务领域验收。

## 复现

```text
python -X utf8 -m unittest discover -s tests -p test_ida_agent_client.py -v
python -X utf8 tools/package_ida_skill.py
```

实机测试由接收方按 SKILL.md 执行 init，使用自己的 URL 和凭据。打包脚本只纳入 7 个指定源文件，校验 ZIP 目录和内容并输出 SHA-256；不读取本地 profile、凭据或运行记录。
