[English](app-integration.md) · **简体中文**

# 新应用接入与整包烧录链路

0.6.0 采用一个静态链接的系统固件、一个前台自定义应用和公共系统服务。社区完整 BIN 是整机固件，不是插件；不要拼接多个 BIN、为菜单每一项分配固件分区，或直接调用外部项目的 `app_main`。

0.4.0 沿用该模式，增加[芽芽宠物](pet-v1.zh_CN.md)。模块可选提供
`tick(ctx, elapsed_ms, showing)`，由控制任务约每 20 ms 在 LVGL 锁外调用，
`showing` 仅在亮屏、应用页面且有焦点时成立。处理可见性变化，恢复时不补算离开时间，
该回调不能访问 UI。模块使用指定字段初始化，后续新增可选回调默认为空。
[当前语音边界](voice-integration.zh_CN.md)记录 Yaya Pet／Yaya Chat 独立，只共享美术、不连接养成存档、不改云端角色；IDA 保留后续独立工作。

## 下次直接照做

1. 先读当前底座说明、`apps/catalog.json`、`passport_apps.h`、`passport_core.h` 和木鱼组件。基于最新测试通过的底座提交开发，不从旧独立木鱼分支重新开始。保留其他改动，使用 `codex/*` 分支。
2. 在 `firmware/muyu` 执行 `python tools/new_app.py my_app --name "My App"`。生成 `components/pp_app_my_app/` 并更新清单，已有名称拒绝覆盖。生成的计数器只是接入骨架，最多 16 个应用。
3. 实现模块回调。清单统一登记 ID、名称、版本、作者、组件、导出符号和 NVS 命名空间。CMake 从同一清单生成注册表；添加应用不需要再修改系统菜单。
4. `start` 准备轻量状态；`focus(true)` 在激活后申请资源；`key` 接收短按语义。失焦暂停任务，`stop` 必须取消并等待工作任务退出，之后系统才能删除应用 UI。长按 OK 由系统保留。0.6.0 的 Wi-Fi 与电量共用 30 像素顶部栏，应用区 240x290、屏幕 y=30，无常驻底栏。蓝牙、Storage、About、设备／运行内存／容量页移除；保留配网、音量、亮度、超时及熄屏模式。读取父容器尺寸并据此定位底部按钮，保持原生像素素材；详细联网文字留在 Settings，不在应用中重复占行。创建/渲染由底座持有 LVGL 锁。不要重复初始化屏幕、按键、网络栈或 codec。
5. `pp_app_runtime()` 仅限控制任务访问。通过 `pp_resource_acquire` 登记有超时、可重复执行的取消动作；旧代次结果必须丢弃。网络工作任务通过有界消息回到控制任务。只设置停止标志不等于任务已退出。现有 `pp_voice` 负责录放和系统音频租约，应复用归属边界而非另建音频栈；Yaya Pet 不调用语音。
6. 使用 `settings` 分区和清单中的独立 `app_*` 命名空间，保存格式带版本，测试迁移。禁止清空共享 NVS 或整片 Flash。不要保存指针、供应商密钥或带凭据的日志。
7. 先跑应用专项测试，再在 ESP-IDF 5.5.3 下运行 `./tools/validate.sh --static`、`./tools/validate.sh --preview` 和 `./tools/validate.sh --firmware`。查看最终 `capacity-report.json`、真实 LVGL 预览和切换/取消测试。实机检查双向切换、共享联网、黑屏运行、唤醒、重启和存档迁移。
8. 明确暂存本轮应用/清单/测试/文档，提交、推送并更新 PR。交付绑定源码 SHA、CI、BIN 和 SHA-256；分别记录构建、主机与实机结果。用户接受前保留上一份已测试包。

## 系统与所有应用一起生成 BIN

`应用清单 → 注册表和组件列表 → 链接底座及全部应用 → 统计实际链接归档 → 写入构建测量头文件 → 再链接 → 核对最终统计未变化 → 合并校验 → 完整 BIN、容量报告、校验和`

容量工具读取官方 ESP-IDF size 结果。每个应用独占一个组件归档，统计实际链接的代码、常量及有初值数据，BSS 不算入 Flash。静态 RAM 单列；共享 SDK、系统 UI、音频工作任务和镜像填充只记一次系统开销。应用数字不等于删除它一定能回收的空间，也不含动态堆、任务栈和 DMA。

两次最终尺寸统计不一致就拒绝交付。测量头文件是构建产物，不再要求设备容量页及其链接指标常量存在。直接 `idf.py build` 未完成报告／合并校验，只供迭代；发布 BIN 必须使用完整链路。

从成功 CI 下载 `passport-firmware-<SHA>` 到 `<delivery>/firmware`、`passport-preview-<SHA>` 到 `<delivery>/preview`。把 `gh run view <run> --json conclusion,headSha,url` 保存为 `<delivery>/ci-result.json`，把 `gh run view <run> --log` 保存为 `<delivery>/ci-log.txt`。在对应的干净源码提交上执行 `python tools/package_release.py <delivery> --output <dist>`。脚本核验 CI/提交、分区、镜像哈希、测试证据和容量报告，再生成一个易辨认的完整 BIN 与校验后的 ZIP；不会烧录或发布 Release。

主机测试还会实际生成并编译独立脚手架与注册表，在真实 LVGL 下与木鱼样例切换 2,000 次。新应用自己的行为仍应补主机适配与专项测试。C++ 模块要以 C 链接方式导出清单符号，并显式桥接 C 平台函数。

## 容量与保护边界

- 物理 Flash 8 MiB，程序分区 3 MiB，起点 `0x10000`。
- 设置区 `0x310000`、24 KiB；身份区 `0x356000`、16 KiB，均不得移动或覆盖。
- 程序可增长量 = 3 MiB 减去最终应用镜像长度。其他未分配 Flash 不能直接解释为可安装应用空间。
- 设备容量页已移除。开发工具排查 NVS 应统计条目、元信息和变长数据分块，不虚构剩余字节。
- 共享美术／字体通过 `pp_avatar` 只链接一次，专属资源仍归各应用，没有外置资源分区。超过 3 MiB 时先压缩/裁剪资源，或另行设计并审查资源分区。

若未来在身份区之后放资源，从 `0x0` 连续写入的合并 BIN 会把中间填充也写过设置/身份区。届时必须提供经过审查的分段镜像和支持分段的刷机清单，不能悄悄放大当前 full.bin。动态安装、多固件启动与 OTA 是后续独立设计，本版不支持。

## 参考

设备键盘交互参考 MIT 项目 [leo-radio](https://github.com/leo0183/leo-radio/tree/e28b8adafb2ca95e386e8b6e7db0db042cf09523)。本底座独立实现有界键盘与连接状态机，没有复制电台播放器、素材或配网门户。小智与尖塔远征的接入判断见[兼容性评估](community-compatibility.zh_CN.md)。
