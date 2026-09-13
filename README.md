# AI Passport

基于 FoloToy AI Passport 的独立联网远程语音 Agent 项目。

设备通过 Wi-Fi 或手机热点连接远程 Voice Gateway，使用麦克风、扬声器、屏幕和按键与 Agent 交互。首版采用按键控制的半双工语音，日常使用不依赖电脑常开。

## 当前状态

本仓库处于初始化阶段，目前包含项目说明、开发约定和本地文件忽略规则。已完成硬件与官方源码调研；本仓库尚未交付可运行的设备固件或 Voice Gateway。

开发计划：

1. 核对设备、身份数据保护和恢复方式，建立可构建的固件基线。
2. 验证本机录放音、手机配网与 WSS 连接。
3. 跑通单 Agent 对话、回复播放和取消操作。
4. 增加多 Agent 路由并进行稳定性、续航和恢复验收。

## 开发方式

- 固件、网关、Agent 适配、开发脚本和测试统一在本仓库管理。
- 使用 `codex/*` 分支开发，完成适当验证后通过 Pull Request 合并至 `main`。
- 提交前逐项检查文件，保留上游来源、版本及许可证。
- 配置通过本地环境或脱敏示例提供；凭据、设备身份备份、原始接口响应和个人笔记不进入版本管理。
- 构建、主机测试和实机测试分别报告，只将实际执行并通过的检查标为通过。

更多约定见 [AGENTS.md](AGENTS.md)。

## 官方参考

- [AI Passport 使用与开发指南](https://ai-passport.folotoy.cn/guides/)
- [FoloToy 官方通用开发仓库](https://github.com/FoloToy/ai-passport)
- [FoloToy 官方小智适配](https://github.com/FoloToy/folo-ai-passport-xiaozhi)

本仓库是独立项目，具体固件基线将在恢复方案与资源预算验证后确定。
