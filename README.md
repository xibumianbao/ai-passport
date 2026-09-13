# AI Passport

基于 FoloToy AI Passport 的独立联网远程语音 Agent 项目。

设备通过 Wi-Fi 或手机热点独立联网，使用麦克风、扬声器、屏幕和按键与 Agent 交互。当前复用小智服务，采用自动半双工语音，日常使用不依赖电脑常开。

## 当前状态

当前迭代 [Passport 多应用底座 0.6.0](firmware/muyu/README.zh_CN.md)：[芽芽对话 Yaya Chat](firmware/muyu/docs/xiaozhi-standalone.zh_CN.md)使用小智语音和像素形象，[芽芽养成 Yaya Pet](firmware/muyu/docs/pet-v1.zh_CN.md)保持离线。两个应用通过菜单切换，只共享美术和一块绘图缓冲，不打通养成数据。

小智复用系统 Wi-Fi 与受保护的设备身份；按一次 OK 开始对话，服务端自动断句，回答播完后恢复监听。短按停止、长按菜单，云端七喜配置保持。0.6.0 沿用 0.5.3 语音实现，构建、主机测试与设备验收分别记录，新包仍待用户烧录验收。IDA 保留后续独立入口；当前应用边界见[接入规划](firmware/muyu/docs/voice-integration.zh_CN.md)。

移除底部常驻提示，应用内容增加到 240×290；顶部保留 Wi-Fi 与电量。蓝牙、Storage、About 和设备／运行信息页已从当前固件移除，保留设备键盘配网、音量、亮度、最近应用恢复与两种熄屏模式。容量检查继续在编译报告中执行，木鱼不再链接。

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

本仓库是独立项目，当前固件基线为 ESP32-C3、8 MiB Flash、ESP-IDF 5.5.3，保留官方硬件保护分区。
