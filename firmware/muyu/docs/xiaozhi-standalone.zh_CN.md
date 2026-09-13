# 小智独立应用测试版

[English](xiaozhi-standalone.md) | 简体中文

底座 0.5.0 增加独立小智应用。宠物暂时保持离线；性格设置、宠物对话与 iDA 后续再接。
人格、声音和记忆继续使用设备在小智平台上绑定的智能体配置。

## 0.5.1：C3 运行内存修正

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
NOT RUN；需验证 Ready、真实上行帧、可听见的回答、多轮、菜单取消、熄屏与断网重试，
并确认同阶段堆稳定后验收。

## 操作与资源归属

- Apps 选择 Xiaozhi；未联网先到系统 Settings 配置 Wi-Fi。
- Ready 时按 OK 开始说话，再按 OK 发送；最多录音 30 秒。
- 回答时按 OK 结束会话，再按 OK 重新连接。长按 OK 打开系统菜单。
- 熄屏 Running 模式继续语音；Paused 模式撤销会话。
- 打开菜单、切应用或暂停立即撤销录音权限。工作线程完成有超时的 I/O 后释放资源，
  然后才允许启动下一会话。线程不操作 LVGL，应用界面删除后没有悬空 UI 回调。

## 接入与存储

`pp_app_xiaozhi` 负责界面，`pp_voice` 负责设备发现、TLS WebSocket、Opus 与系统音频租约。
同一时刻只有一个语音会话和音频使用者，Wi-Fi 复用底座。UUID 首次生成，或从上游
`board/uuid` 导入，写入受保护 `settings/svc_xiaozhi`。令牌、音频、转写和会话 ID
只放内存，不落盘、不写日志。使用设备真实 station MAC 请求官方设备发现接口，
忽略返回的固件更新地址与远程重启/升级命令。

实际应用目录只有小智与宠物。历史木鱼源码保留作回归与参考；新固件不链接其应用、
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
