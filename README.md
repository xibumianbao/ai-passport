# AI Passport

基于 FoloToy AI Passport 的独立联网远程语音 Agent 项目。

设备通过 Wi-Fi 或手机热点连接远程 Voice Gateway，使用麦克风、扬声器、屏幕和按键与 Agent 交互。首版采用按键控制的半双工语音，日常使用不依赖电脑常开。

## 当前状态

当前迭代 [Passport 多应用底座 0.4.0](firmware/muyu/README.zh_CN.md)：保留设备小键盘 Wi-Fi 配网、自定义应用清单、恢复上次应用、两种熄屏和容量查看，新增[芽芽电子宠物 0.1.0 测试版](firmware/muyu/docs/pet-v1.zh_CN.md)。宠物自己生活，一键喂食／陪玩／休息，自动赚币、升级、进化，使用小型像素素材与双份校验存档；新版本待实机验收。

后续按[语音接入规划](firmware/muyu/docs/voice-integration.zh_CN.md)，宠物与小智融合，IDA 保留独立语音应用入口，可评估共用小智语音服务。本次只实现离线宠物，不接入小智或 IDA 语音。

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
