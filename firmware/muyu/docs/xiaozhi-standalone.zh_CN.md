# 芽芽对话：独立小智应用

[English](xiaozhi-standalone.md) | 简体中文

底座 0.6.0 将独立对话入口命名为 **Yaya Chat**。Yaya Pet 保持离线养成，与聊天不连接存档和模型；两者只复用像素美术。现有云端七喜的人格、声音和记忆配置不变，此前养成数据与语音打通的计划取消。

## 0.6.0：共享外观，应用独立

对话使用既有房间／字体及唯一共享的 96x88 I4 角色缓冲，按语音快照表现倾听、思考、说话，保留激活数字和错误详情。不新增养成模型／存储、动画线程、大图或第二块角色缓冲；应用区为 240x290，无常驻底栏。

本版不改 `pp_voice`、TLS、Opus、40 KiB 工作栈及下述自动对话行为。移除 Bluetooth／NimBLE 和 Storage／About／设备／运行内存／容量页；保留 Wi-Fi、声音／显示设置和熄屏模式，仍输出开发容量报告。新 0.6.0 Device tests 为 NOT RUN，详见[当前归属边界](voice-integration.zh_CN.md)。

## 0.5.3：自动断句与连续半双工对话

Ready 时按一次 OK 开始，说话后的停顿由服务端判断；发送 `listen/start mode:auto`，
每轮不再发送手动 `listen/stop`。收到 `tts:start` 才停止采集、释放编码器；
STT 只作为事件处理，协议没有明确的最终断句标志，不能收到 STT 就停止上传。
收到 `tts:stop` 后，先排空扬声器 DMA、释放解码器，再丢弃麦克风 DMA 中的旧采样，
然后自动开始下一轮。同一时刻只保留一个方向的编解码器；不加入本地 VAD、AEC、
唤醒词模型、额外音频历史缓冲或后台自动重连。

对话中短按 OK 停止；Stopped 时按一次 OK 重连并开始对话。报错重试先回到 Ready，
再按 OK 才开始录音。长按打开菜单、切应用或 Paused 熄屏均撤销会话；Running 熄屏
继续当前会话。单轮监听最多 30 秒，回答阶段 60 秒没有音频进展则报错结束。
取消、断网和超时后不会自动重新录音。保留 40 KiB 工作栈及固定 PCM 自检，继续测量
运行内存余量。

电脑端已在**同一 WSS/session 完成两轮自动问答**：第一轮上传 114 帧、收到 19 帧，
第二轮上传 113 帧、收到 23 帧；两轮均识别合成测试请求并返回预期回答，没有发送
手动停止命令。这证明服务端断句和协议连续性，不证明设备 I2S 交接或声学回声表现。
0.5.3 新固件 Device 为 **NOT RUN**。可通过仓库根 `tools/xiaozhi_auto_voice_probe.py`
在内存中传配置执行有限时探测，离线测试和原手动工具一起进入 CI；凭据、转写、DLL
及音频不提交仓库。

生产 `pp_voice_turn` 状态机覆盖多轮、STT 先到、重复事件、超时、会话不匹配和取消；
实际阻塞的编解码、I2S 与传输调用仍需集成验收。见[脱敏验证记录](xiaozhi-voice-validation.json)。
行为参考固定版本的[原生应用](https://github.com/FoloToy/folo-ai-passport-xiaozhi/blob/d24fce080d86d7cc642f71585f6efde40fb99104/main/application.cc)
和[自动监听命令](https://github.com/FoloToy/folo-ai-passport-xiaozhi/blob/d24fce080d86d7cc642f71585f6efde40fb99104/main/protocols/protocol.cc)。

## 0.5.2：实机录放通过，扩展稳定性待测

用户已烧录交付的诊断包，并反馈重连后实机聊天顺畅。日志捕获三次固定 PCM 自检通过，
以及真实麦克风编码、云端音频解码和扬声器写入；最低观察栈余量 16,192 字节，
该窗口没有 panic。诊断特征与 0.5.2 一致，但本次未捕获开机版本／ELF 头。
最低空闲堆 17,360 字节，最大连续块最低 8,192 字节，低于项目 20 KiB 观察目标。
其中一次连接结束并正常清理，原因尚未定位。因此为 **PARTIAL PASS**，不能据此宣布
长会话与完整生命周期验收通过。以下保留诊断设计和当时的手动交互记录。

0.5.1 的新串口记录已经核对固件版本及 ELF 摘要：编码器和解码器均能创建，
开始录音后出现 `A stack overflow in task xiaozhi has been detected`，随即重启。
这与 0.5.0 的编码器分配失败是两个不同阶段的问题。栈保护器检测到破坏，但还需
通过固定 PCM 测试区分实际栈深度和其他越界写；不能只凭该报错排除后一种可能。

本版保留 VOIP、16 kHz、60 ms、24 kbps、complexity 0 等原参数，将工作栈由
24 KiB 调整到 40 KiB 作为测量起点。乐鑫 [2.5.0 文档](https://components.espressif.com/components/espressif/esp_audio_codec/versions/2.5.0/readme)
说明其内存性能表只统计 heap、支持全部编码器需约 40K 栈；这不是 Opus/C3 的精确
最低要求。栈新增 16 KiB 会挤占 heap，必须同时观察 TLS 活跃时的最低堆和最大连续块。
单纯改为 size 编译优化不会重新编译 SDK 预编译的 Opus 库。

进入小智时，在设备发现、TLS 和 I2S 前执行固定 PCM 的真实编解码诊断，覆盖静音、
波形、确定性噪声与近削顶输入，复用短缓冲，不录环境声音、不上传、不保存音频。
通过后才继续连接云端；串口记录每阶段 SDK 结果、长度、耗时及栈/堆余量，并标出
首次真实 I2S 采集、录音编码和回复解码。暂以至少 4 KiB 栈余量作为项目诊断门槛，
TLS 活跃时最大空闲块约 20 KiB 为观察目标，后者不是 SDK 硬性需求或自动通过标准。
这一步未修改编解码参数、BSP、任务退出回收次序、Wi-Fi/TLS 配置或宠物逻辑。
脱敏观测见 [验证记录](xiaozhi-voice-validation.json)。

电脑端已完成真实小智音频往返：6.28 秒合成语音、105 帧上行、23 帧下行，服务端
24 kHz Opus 可解码为 16 kHz；识别出测试请求并返回预期回答，输出 1.38 秒非静音
音频；网络时限保护调整后的第二轮为 105 帧上行、19 帧下行，语义同样通过。
使用已授权连接设备的标识和临时主机 Client UUID，令牌和转写仅在内存中。
没有重新激活、修改智能体配置或调用工具；主机测试不证明固件持久身份、麦克风、
扬声器、ESP SDK 栈和多应用启停已通过。

复现工具位于仓库根 `tools/xiaozhi_voice_probe.py`，由可信本地包装程序在内存中传入
`ConnectionConfig`，输入 16 kHz 单声道 PCM16 WAV（最长 30 秒）；返回统计与仅存在
内存中的 STT/TTS 文本供语义核对，回复最多等 90 秒。依赖 websocket-client 与 Xiph
libopus；DLL/音频/设备标识/发现返回值不提交。离线测试使用
`python -m unittest discover -s tests -p test_xiaozhi_voice_probe.py -v`，CI 自动执行。

验证顺序：烧录 0.5.2 后先进入小智观察本机诊断；通过后按 OK 录音，再按 OK 发送，
检查可听见的回答和日志；随后分别验收多轮、录音/回答时长按退出、重进、断网、
两种熄屏模式。快速重进时旧线程栈可能尚待 IDLE 回收，以及 BSP 初始化失败回滚
不完整，属于独立生命周期风险；本单变量诊断不宣称已修复它们。出现失败时保留
第一处错误、固件版本、阶段及同阶段内存统计，按证据再做下一步，不继续盲目加栈。

## 0.5.1：C3 运行内存修正（历史实现）

0.5.0 实机在录音前失败：SDK 日志为
`ESP_OPUS_ENC: Opus encoder init failed. ret:-7.`，屏幕提示
`Not enough memory for Opus`。[Opus API](https://github.com/xiph/opus/blob/v1.5.2/include/opus_defines.h)
中 -7 表示分配失败。这里是运行 SRAM，不是该镜像剩余约 1.07 MiB 的程序 Flash。
日志 61,416 字节空闲堆是在连接清理后、线程栈和上下文仍占用时记录，不能当作失败瞬间的
空闲量，也不能据此认定泄漏；旧版没有记录最大连续空闲块。

0.5.1 对齐[上游内存配置](https://github.com/FoloToy/folo-ai-passport-xiaozhi/blob/d24fce080d86d7cc642f71585f6efde40fb99104/sdkconfig.defaults)：
可选 Wi-Fi 指令路径放在 Flash，释放 C3 共用的指令／数据 SRAM；TLS 缓冲按需分配并
在使用后释放，验证后不常驻保存对端证书。
[IDF 5.5.3 配置](https://github.com/espressif/esp-idf/blob/v5.5.3/components/mbedtls/Kconfig)
继续保留 16 KiB TLS 接收记录与正常证书／域名校验。握手后释放 TLS 配置数据、关闭
重协商，每次重连新建 TLS 对象。Wi-Fi 可选指令移出 SRAM 后的实际吞吐仍需设备验证。

`pp_voice_codec` 单一句柄同一时刻只拥有编码器或解码器。先准备音频硬件／DMA，再
顺序检查两个编解码器并释放，之后才允许 Ready。录音只持有编码器，发送 listen-stop
前释放；收到第一帧回答音频时才创建解码器，TTS-stop 或取消时释放。空闲和等待回答
期间两者都不持有。暂保留 24 KiB 线程栈、32 KiB LVGL 池，后续依据实际栈与界面余量
优化。不增加图片，不改变分区。

错误明确区分编码／解码方向与 SDK 返回码，外层通用连接错误不覆盖首次错误。串口在
分配／释放阶段记录内部空闲堆、最大连续块、历史最低值和栈余量。最后清理日志明确
说明线程栈尚未回收。比较多轮内存必须选同一阶段；首次硬件初始化会保留系统 DMA
句柄，历史最低值本身不会回升。

主机回归使用生产代码执行 10,000 次编解码切换，以及分配失败、部分分配后失败、空句柄、
帧查询失败与尺寸边界注入；测试分配器的模拟额度不是乐鑫 Opus 实测占用。固件门禁
核对最终生成的内存配置。0.5.0 Device：编码器初始化 FAIL。0.5.1 Device：烧录前
NOT RUN；后续实机验证为 FAIL（录音阶段任务栈溢出），见上方 0.5.2 诊断记录。

## 操作与资源归属

- Apps 选择 Yaya Chat；未联网先到系统 Settings 配置 Wi-Fi。
- Ready 时按一次 OK 开始，停顿后自动回复，回答播完自动继续听；单轮监听最多 30 秒。
- 对话中短按 OK 停止会话；长按 OK 打开系统菜单。
- 熄屏 Running 模式继续语音；Paused 模式撤销会话。
- 打开菜单、切应用或暂停立即撤销录音权限。工作线程完成有超时的 I/O 后释放资源，
  然后才允许启动下一会话。线程不操作 LVGL，应用界面删除后没有悬空 UI 回调。

## 接入与存储

`pp_app_xiaozhi` 负责界面，`pp_voice` 负责设备发现、TLS WebSocket、Opus 与系统音频租约。
同一时刻只有一个语音会话和音频使用者，Wi-Fi 复用底座。UUID 首次生成，或从上游
`board/uuid` 导入，写入受保护 `settings/svc_xiaozhi`。令牌、音频、转写和会话 ID
只放内存，不落盘、不写日志。使用设备真实 station MAC 请求官方设备发现接口，
忽略返回的固件更新地址与远程重启/升级命令。

实际应用目录只有 Yaya Chat（稳定 ID `xiaozhi`）与 Yaya Pet。历史木鱼源码保留作回归与参考；新固件不链接其应用、
字体、音效和常驻音频任务。宠物存档与 Wi-Fi 配置的命名空间不变。分区不变：程序
3 MiB、settings 位于 0x310000、cardid 位于 0x356000。加入语音后仍强制预留至少
512 KiB 给后续应用；此前专为接语音保留的 1.25 MiB 在本阶段使用。容量报告分别
列出应用专属代码和共享语音组件；实际堆、线程栈和 DMA 另做设备测量。

## 协议与验证边界

使用发现接口返回的 WSS 地址与令牌。缺少 WSS 配置明确报错，本测试版不实现 MQTT/UDP。
支持二进制协议 1/2/3、16 kHz 单声道 60 ms Opus 上行、分片重组、消息长度与 JSON 嵌套深度限制和
ping/pong。播放直接解码为 16 kHz，保持 I2S 格式固定。暂不加入唤醒词、AEC、边听边说
或大图资源。

主机测试检查帧边界、续帧、超长拒绝、头部注入、URL 校验和身份格式。固件 CI 检查
8 MiB 目标、保护分区、依赖锁与最终容量。云端发现/握手、主机渲染、编译和硬件语音
分别记录，编译通过不等于麦克风与扬声器验证通过。

设备验收包括断网重试、30 秒限时、静音输入、打断回答、快速切应用与菜单、两种熄屏
模式、重启身份保持、宠物存档、堆回收和栈余量。日志只记录帧计数与内存，不记录令牌、
转写或 MAC。

## 来源

协议与 ES8311 行为参考 [FoloToy 小智分支](https://github.com/FoloToy/folo-ai-passport-xiaozhi/tree/d24fce080d86d7cc642f71585f6efde40fb99104)
（MIT，Shenzhen Xinzhi Future Technology Co., Ltd. 及贡献者），基于上游 2.4.2。
本实现是独立 C 适配层，保留项目既有许可证与第三方声明。完整判断见[接入研究](xiaozhi-research.zh_CN.md)。

依赖 ESP-IDF 5.5.3 的 TLS/HTTP/WebSocket 与乐鑫 esp_audio_codec 2.5.0，仅使用 Opus
直接接口；组件许可证随依赖保留。沿用项目 CI 整包编译，小智云端固件编译不包含本底座和宠物。
