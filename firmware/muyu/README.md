[简体中文](README.zh_CN.md) | English

# Muyu: wooden-fish MVP

An offline wooden-fish app for FoloToy AI Passport. It boots directly into the
app: press **UP, DOWN or OK** to strike, hear a short wooden percussion tone,
animate the mallet and increase the on-screen merit counter by one.

Only the initial button-down event counts. Holding a key does not auto-repeat,
and click/double-click/long-press notifications do not add duplicate counts.
The counter belongs to the current boot session and resets on reboot. There is
no provisioning, network dependency, account, background recording or saved count.

## Build and inspect

The target is ESP32-C3 with 8 MB Flash and ESP-IDF **5.5.3**. The project retains
the upstream BSP, pins and protected partition layout. See
[environment setup](docs/development/engineering/environment-setup.md) for a
local toolchain, then run from this directory:

```sh
./tools/validate.sh --static
./tools/validate.sh --firmware
```

The project workflow in the outer repository runs both gates, renders the same
LVGL UI on the host, exercises 1,000 rapid UI updates and uploads the checked
firmware, Flash segments, PNG previews and generated WAV. Host rendering is not
device validation. The artifact name includes the source commit.

## Flash and test

Before flashing, confirm the factory recovery entry for your own device.
Keep the device identity and its recovery parameters private. Use a data-capable
USB-C cable and the device's USB Serial/JTAG connection.

The checked artifact is `FoloToy-AI-Passport-full.bin`. This application adds no
resource partition after `cardid`; the verification gate requires the merged
file to end before `0x356000` for this MVP. With that verified file, the
[official browser flasher](https://ai-passport.folotoy.cn/tools/web-flasher/)
can write at **0x0** without erasing the entire chip. Never select erase-all.
For a configured ESP-IDF checkout, prefer segmented `idf.py -p PORT flash`.
Do not confuse the app-only `FoloToy-AI-Passport.bin` with the merged image.

Acceptance on the actual board:

1. Boot shows the wooden fish, zero count and battery level (or `--%`).
2. Press each key ten times: exactly thirty counts and immediate visible feedback.
3. Hold one key: exactly one count until release; double-tap: two counts.
4. Repeatedly press for one minute: no crash, stuck animation or growing sound queue.
5. The speaker emits a short wood-like tone; listen for distortion and latency.
6. Restart: the count returns to zero. Check recovery access after the test.

Build results cannot establish these physical checks. A codec or audio-worker
failure shows `SOUND UNAVAILABLE` while counting remains usable. A button-init
failure shows `BUTTON ERROR`.

## Implementation and resources

- `main/muyu_app.c`: BSP initialization and notification-driven workers. The
  button callback never blocks on rendering, I2C, storage or sound.
- `main/muyu_ui.c`: fixed LVGL objects and reusable absolute-position animations.
- `main/muyu_logic.c`: saturating 32-bit counter and bounded four-voice mixer.
- The synthesized tone is 192 ms, 16 kHz, signed 16-bit mono: 6,144 static bytes.
  Output chunks are 320 bytes. Excess simultaneous hits replace old tails; all
  accepted presses still count. Default speaker volume is 65 percent.
- Firmware LVGL pool: 32 KiB; upstream 20-row display buffer is unchanged.
  The host preview uses a larger pool for 64-bit pointers and full-frame output;
  it does not measure device RAM, latency, battery life or acoustic quality.
- Wi-Fi is not initialized and Bluetooth is disabled. The counter never writes
  NVS on a strike. Battery access uses the unchanged upstream CW2017 driver.

## Source and licenses

The complete upstream baseline was imported from
[`FoloToy/ai-passport@f75873f1`](https://github.com/FoloToy/ai-passport/tree/f75873f1aab24ac4c0ba9394c131669f66cce650).
It remains under the included [MIT license](LICENSE). The original demo sources
remain available for reference; this application's CMake target builds the Muyu
entry point instead of the demo menu.

The modal synthesis idea and `font_muyu_22.c` come from the
[`demo/coloros-muyu` example at 16df9944](https://github.com/FoloToy/ai-passport/tree/16df9944d0f6a83b475e05acabbea73c8b49c3e1).
The Source Han Sans SC subset retains its
[SIL Open Font License](assets/fonts/SourceHanSans-OFL.txt).
The wooden-fish graphic is drawn with LVGL primitives; no background image from
that example is used. Dependency versions remain pinned in `dependencies.lock`.
