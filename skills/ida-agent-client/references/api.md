# IDA v2 A2A 接口与证据规则

协议核对日期：2026-09-13。以下是火山引擎 SaaS 实测路由，不声称适用所有私有部署。其他 SDK/标准 A2A 版本不可直接替换这些方法名与字段。

## 官方文档

| 功能 | 文档 |
|---|---|
| 概览/鉴权入口 | [2649370](https://docs.volcengine.com/docs/85637/2649370?lang=zh) |
| 获取 JWT | [2649373](https://docs.volcengine.com/docs/85637/2649373?lang=zh) |
| 域名说明 | [2649374](https://docs.volcengine.com/docs/85637/2649374?lang=zh) |
| 创建会话 | [2649393](https://docs.volcengine.com/docs/85637/2649393?lang=zh) |
| 会话详情 | [2649388](https://docs.volcengine.com/docs/85637/2649388?lang=zh) |
| 流式对话 | [2649378](https://docs.volcengine.com/docs/85637/2649378?lang=zh) |
| 订阅/恢复 | [2649386](https://docs.volcengine.com/docs/85637/2649386?lang=zh) |
| 查询任务 | [2649379](https://docs.volcengine.com/docs/85637/2649379?lang=zh) |
| 取消任务 | [2649383](https://docs.volcengine.com/docs/85637/2649383?lang=zh) |

## 路由与字段

SaaS base：`https://console.volcengine.com/bi/datawind`。appId 从前端 URL 保存作配置辨识，不额外塞进不需要它的 API body。Agent ID 由 createSession 绑定到会话，A2A 通过 sessionId/contextId 指向会话。

1. **JWT**：`POST <base>/aeolus/api/v3/openapi/jwtToken`。JSON：`{"metadata":{"clientId":"<hidden>","clientSecret":"<hidden>","expire":3600}}`，可选 metadata.proxyUser。要求 `aeolus/ok` 和非空 data.jwtToken。JWT 只在内存，不打印。
2. **创建会话**：`POST <base>/dataAgent/llm/openApi/v2/signed/agent/createSession`。JSON：`{"agentId":101,"name":"Example","source":"openapi","version":"v5"}`。要求 `llm/ok`，读 data.sessionInfo.id。
3. **会话详情**：`GET <base>/dataAgent/llm/openApi/v2/signed/agent/sessionInfo?sessionId=<id>&needMsg=true&maxMsgCount=200`。
4. **A2A**：`POST <base>/dataAgent/llm/openApi/v2/a2a/`，JSON-RPC 2.0；id 为客户端 UUID。业务 headers：`Authorization: Bearer <JWT>`、`x-vedi-region: <region>`、`Content-Type: application/json`；SSE 加 `Accept: text/event-stream`、`A2A-Version: 1.0`。记录 request-id 供定位。

普通问答：

```json
{
  "jsonrpc": "2.0",
  "id": "<request-uuid>",
  "method": "SendStreamingMessage",
  "params": {
    "message": {
      "role": "ROLE_USER",
      "parts": [{"text": "1+1 等于几？只回复数字答案，然后结束。"}],
      "metadata": {
        "sessionId": "<sessionId>",
        "version": "v5",
        "enableKnowledgeQA": true,
        "enableFastMode": true
      }
    }
  }
}
```

深度研究模式设 enableKnowledgeQA=false，不设置 fast mode。会话外层 sessionType / isKnowledgeQA 可能与消息内模式不同，不可据此推断实际发送模式。

GetTask / SubscribeToTask / CancelTask 共用 JSON-RPC 信封，method 为对应方法名，params 为 `{"id":"<taskId>","contextId":"<sessionId>"}`。GetTask 返回 result.task；SubscribeToTask 返回 SSE；CancelTask 的 result.success=true 只代表受理，需要 GetTask 确认 canceled 或已经自然结束的终态。

## SSE 与最终答案

result.task 给初始快照和 taskId；result.statusUpdate 给状态；result.artifactUpdate 给片段。按 UTF-8、空行分帧、多行 data、注释心跳、EOF、[DONE] 解析。重订阅可能重放内容，不能把多条订阅的文本拼成答案。

artifact.metadata 中的过程步骤、deep_research_running_step / executor_think 文本不是最终回复；lastChunk 只结束当前 artifact。完成事件后再 GetTask 和 sessionInfo，用当前 taskId 核验：

```text
data.messageList[].content.content.deepResearchContent.taskId
data.messageList[].content.content.deepResearchContent.finalAnswer
data.messageList[].content.content.deepResearchContent.finalReport
```

已观察到：取消任务的重建历史事件仍可能 completed；成功任务的 summary.hasFinalArtifact / hasFinalMessage 可能 false。权威快照与按 taskId 保存的 finalAnswer 优先。报告引用单独列为待内容验收，不能只凭存在就宣称业务成功。

客户端要求 HTTPS 同源端点且不自动重定向。私有部署仅支持显式同源 base/jwt/a2a 覆盖；跨域身份交换、多模态上传、工具执行审批、报告文件下载不在本版本通用客户端范围内。
