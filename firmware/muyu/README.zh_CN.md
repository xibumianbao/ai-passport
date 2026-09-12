简体中文 | [English](README.md)

# 木鱼：最小可用应用

运行在 FoloToy AI Passport 上的离线木鱼应用。开机直接进入木鱼界面：按 **上、下或 OK** 任意一个键，播放短促木鱼声、展示木槌敲击动画，并让屏幕上的功德计数加一。

只统计按下瞬间。按住不连续累加，单击、双击、长按识别事件不会重复计数。计数属于本次开机会话，重启清零；无需配网、网络、账号，不后台录音，也不保存计数。

## 构建与检查

目标为 ESP32-C3、8 MB Flash、ESP-IDF **5.5.3**。保留官方 BSP、引脚和身份区分区布局。本地环境参考[环境安装说明](docs/development/engineering/environment-setup.zh_CN.md)，激活后在本目录运行：

```sh
./tools/validate.sh --static
./tools/validate.sh --firmware
```

外层项目的 GitHub Actions 会运行两类检查、用实际 LVGL 界面代码渲染预览、验证 1,000 次快速更新，并上传核验后的固件、分段镜像、PNG 预览和合成 WAV。主机渲染不等于实机验证，产物名包含源码提交号。

## 刷写与验收

刷写前确认自己设备的出厂恢复入口。设备身份和专属恢复参数需保密。使用支持数据传输的 USB-C 线，连接 USB Serial/JTAG 端口。

合并固件为 `FoloToy-AI-Passport-full.bin`。本应用没有增加身份区之后的资源分区，检查脚本要求该 MVP 的合并文件必须在 `0x356000` 之前结束。对于已通过该检查的文件，可使用[官方网页刷写工具](https://ai-passport.folotoy.cn/tools/web-flasher/)，从 **0x0** 写入，不做全片擦除。不要选择 erase-all。在已配置 ESP-IDF 的工程中优先使用分段命令 `idf.py -p PORT flash`。不要把仅含应用的 `FoloToy-AI-Passport.bin` 当成合并固件。

实际设备验收：

1. 开机出现木鱼、零计数和电量，电量不可读时为 `--%`。
2. 三个键分别敲十次，合计准确增加三十次并立即有视觉反馈。
3. 按住一次只加一；快速双击加二。
4. 连续敲一分钟，无崩溃、动画卡住或越来越长的音效积压。
5. 扬声器播放短促木鱼声，检查失真与时延。
6. 重启后计数清零，测试后核对恢复入口可用。

构建通过不能代替上述物理检查。音频初始化或播放失败时显示 `SOUND UNAVAILABLE`，仍可计数；按键初始化失败显示 `BUTTON ERROR`。

## 实现与资源

- `main/muyu_app.c`：BSP 初始化、任务通知与工作任务。按键回调不会等待绘图、I2C、存储或声音播放。
- `main/muyu_ui.c`：固定 LVGL 对象和可重用的绝对位置动画。
- `main/muyu_logic.c`：达到上限后保持不回绕的 32 位计数器，以及最多四路的混音器。
- 合成木鱼声为 192 毫秒、16 kHz、16 位单声道，静态音色数组占 6,144 字节，每个输出块 320 字节。瞬时过多的敲击会替换旧声音尾部，计数仍覆盖收到的全部按下事件。默认音量 65%。
- 固件 LVGL 内存池为 32 KiB，沿用官方 20 行绘制缓冲。主机预览因 64 位指针和整屏输出使用较大内存池，不用于测量设备内存、时延、续航或真实声音。
- 不初始化 Wi-Fi，关闭蓝牙；每次敲击不写 NVS。电量读取沿用官方 CW2017 驱动。

## 来源与许可

完整官方基线来自 [`FoloToy/ai-passport@f75873f1`](https://github.com/FoloToy/ai-passport/tree/f75873f1aab24ac4c0ba9394c131669f66cce650)，保留 [MIT 许可](LICENSE)。原始演示源码仍可参考，本应用通过 CMake 编译木鱼入口，不编译演示菜单。

共振衰减音色思路和 `font_muyu_22.c` 来自 [`demo/coloros-muyu@16df9944`](https://github.com/FoloToy/ai-passport/tree/16df9944d0f6a83b475e05acabbea73c8b49c3e1)。思源黑体 SC 字形子集附有 [SIL Open Font License](assets/fonts/SourceHanSans-OFL.txt)。木鱼图形用 LVGL 基本图形绘制，没有使用示例的背景图片。组件版本继续由 `dependencies.lock` 锁定。
