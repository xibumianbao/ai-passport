# AI Passport

基于 FoloToy AI Passport 的独立联网远程语音 Agent 项目。

设备通过 Wi-Fi 或手机热点连接远程 Voice Gateway，使用麦克风、扬声器、屏幕和按键与 Agent 交互。首版采用按键控制的半双工语音，日常使用不依赖电脑常开。

## 当前状态

当前迭代 [Passport 多应用底座 0.4.1](firmware/muyu/README.zh_CN.md)：Wi-Fi/BLE 改为电量旁的顶部图标，应用区域扩大到 240×265。保留设备小键盘配网、自定义应用清单、恢复上次应用、两种熄屏和容量查看，以及[芽芽电子宠物 0.1.0](firmware/muyu/docs/pet-v1.zh_CN.md)的自动生活、一键喂食／陪玩／休息、升级进化和小型校验存档。宠物初版已获用户效果认可，新状态栏布局待实机验收。

已完成[小智融合深度研究与实施方案](firmware/muyu/docs/xiaozhi-research.zh_CN.md)：独立“小智”与宠物聊天共用一份语音服务、同一云端角色及记忆配置，IDA 保留后续独立语音入口。控制台已支持 AI Passport 的标准 2.5.0 固件，但组合底座仍由本仓库编译；本次只更新研究与规划，小智／IDA 语音尚未接通。架构入口见[语音接入规划](firmware/muyu/docs/voice-integration.zh_CN.md)。

新增应用按[接入与整包交付指南](firmware/muyu/docs/app-integration.zh_CN.md)执行，第三方玩法先看[小智与尖塔远征评估](firmware/muyu/docs/community-compatibility.zh_CN.md)。所有清单应用与底座一起生成一个经过校验的 full BIN，暂不支持直接安装其他项目的 BIN。

源码位于 `firmware/muyu`，使用 ESP-IDF 5.5.3 与 GitHub Actions 构建。构建、主机测试与实机验收分别记录，首次硬件验收见[测试清单](firmware/muyu/docs/platform-acceptance.zh_CN.md)。当前没有 Voice Gateway 或动态应用安装。

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
