[简体中文](README.zh_CN.md) · **English**

# Passport multi-application platform 0.6.0

One persistent system shell contains two independent applications: **Yaya Pet** for offline care and growth, and **Yaya Chat** for Xiaozhi conversation with the same pixel character. Switch between them in Applications. They share artwork, not game state: chatting does not read or change the pet's identity, level, coins, energy or save. The existing cloud agent Qixi keeps its personality, voice and memory configuration unchanged.

## Yaya Chat

Open Applications → Yaya Chat and press OK once when ready. The existing 0.5.3 voice service detects pauses on the server, plays each reply and listens again. Short OK stops the conversation; from Stopped, one OK reconnects and starts. An error retry returns to Ready before recording. Long OK opens the system menu. Wi-Fi, device identity and cloud binding are reused.

The room, character, speech card and listening/thinking/speaking poses replace the former audio orb. Activation codes and failure details remain visible when needed. There is no new voice protocol, codec, wake-word engine or AEC change in 0.6.0. See [voice behavior and acceptance](docs/xiaozhi-standalone.md).

## Yaya Pet

Yaya lives automatically. UP/DOWN selects Feed, Play or Rest; short OK performs one action. Feed combines buying and eating, with free basic food when coins are insufficient. Visible daily activity earns coins/XP, level 3 evolves the character, and two checked saves preserve growth. Menu, screen-off and switching pause growth and invisible purchases; there are no offline penalties or repeated-click minigames.

The [pet guide](docs/pet-v1.md) defines its rules and persistence. The earlier plan to connect pet saves and growth to voice is cancelled. [Application and voice boundaries](docs/voice-integration.md) record the current separation and future extension rules.

## System controls

- Wi-Fi and battery share the top bar. Wi-Fi shows IP acquisition, not measured signal strength or internet reachability. Gray means off, amber unconfigured, pulsing amber connecting, and red error.
- App content is **240×290 at y=30**. The permanent HOLD OK footer is removed, recovering 25 pixels. Artwork stays at native size; no larger framebuffer is added.
- Hold OK for 800 ms to open the menu; hold again to go back. UP/DOWN selects and short OK confirms on release.
- Settings retains Wi-Fi, volume, brightness, screen timeout and screen-off mode. Storage, About, device/runtime diagnostics and capacity subpages are removed. Bluetooth/NimBLE and software Wi-Fi/Bluetooth coexistence are disabled in the build; old saved beacon settings are ignored.
- Settings → Wi-Fi → Find and join network uses the device keyboard. Select a 2.4 GHz network, type its password and select GO; no phone, SoftAP or portal is used.
- The keyboard supports printable ASCII, 8–63-character passwords, open networks, deletion and cancellation. Scan lists up to 12 unique networks; manual names support 32 bytes. Joining has a 25-second deadline, and credentials are saved only after IP acquisition. Failed attempts preserve the previous network.
- Switching applications preserves system Wi-Fi and cancels the previous foreground session.

## Screen-off operation

Pause app is the default; Keep app running preserves an intentional voice conversation while dark. Timeout options remain 0/60/120/300 seconds. The menu's Screen off (run) turns off the backlight immediately; short keys continue reaching the app, while long OK lights the screen and opens the menu. Paused mode consumes the first complete gesture only to wake. Menu timeout always pauses, and both dark modes pause pet growth.

This disables the backlight and skips rendering, not deep sleep. Battery-life improvement still requires measurement.

## Build, capacity and delivery

Use ESP-IDF 5.5.3 for ESP32-C3, 8 MiB Flash and no PSRAM. The build directory remains `firmware/muyu`. `./tools/validate.sh --static` runs source/logic checks; `--preview` runs actual LVGL previews and switching checks; `--firmware` builds and verifies the combined image. `--all` (the default) includes all three stages.

The single 96×88 I4 sprite surface is shared by the two exclusive foreground views. The existing room/font are linked once, with the 32 KiB LVGL pool and 20-row display buffer retained. Exact linked Flash/static RAM figures remain available in `capacity-report.json`, even though device capacity pages are removed. Heap, stacks and DMA require separate device measurements; no new savings figure is claimed before the final build.

Use [the application integration guide](docs/app-integration.md) for the catalog and full-BIN workflow. A community BIN replaces the whole system; it cannot be installed as a plugin.

Flash only the verified `FoloToy-AI-Passport-full.bin` at `0x0`, without full-chip erase. Its end must not exceed `settings@0x310000`; keep the 24 KiB settings partition and `cardid@0x356000` intact. The build retains the 3 MiB program limit and future-app reserve gate. Bind each package to its source SHA, CI, hash and capacity report. **0.6.0 Device tests: NOT RUN** until the owner flashes and checks [the acceptance list](docs/platform-acceptance.md); earlier releases do not validate this new image.

## Provenance

Based on FoloToy/ai-passport `f75873f1aab24ac4c0ba9394c131669f66cce650`. Preserve the [MIT license](LICENSE), upstream BSP, font OFL credits and [voice notice](components/pp_voice/NOTICE). Keyboard interaction was studied in MIT leo-radio. Shipped pixel art is original; no large game images or credentials are included.
