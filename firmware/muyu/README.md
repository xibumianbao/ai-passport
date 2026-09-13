[简体中文](README.zh_CN.md) · **English**

# Passport multi-application platform 0.5.1

A persistent system shell for FoloToy AI Passport. Applications contains standalone Xiaozhi and Yaya Pet. Muyu is retired from the firmware. Device status and capacity belong to Settings. The last stable application resumes after restart, with first/failed startup falling back to the application list.

## Standalone Xiaozhi test application

0.5.1 corrects the 0.5.0 on-device Opus allocation failure with C3 Wi-Fi/TLS
memory settings and one active codec direction. New hardware acceptance is pending.

Open Applications -> Xiaozhi. At Ready, short OK starts recording; short OK again
sends it (30-second limit). OK during an answer stops the connection; OK again
reconnects. Long OK always opens the system menu. Existing system Wi-Fi and cloud
binding are reused. See the [voice guide](docs/xiaozhi-standalone.md) for activation,
protocol, memory, cancellation and device acceptance. No wake word or AEC yet.

## Yaya Pet test application

Open Applications → Yaya Pet. Yaya lives automatically; up/down select Feed, Play
or Rest, and short OK performs one action. Feed combines buying and eating;
insufficient coins provide free basic food. Daily activity earns coins/XP, level
3 evolves the same character, and two tiny checked saves preserve growth.
Menu, screen-off and app switching pause growth and invisible purchases.
There are no offline penalties or repeated-click minigames.

The [pet guide and hardware checklist](docs/pet-v1.md) explains the exact rules,
small pixel resources and tests. [Future voice integration](docs/voice-integration.md)
keeps the pet with Xiaozhi and IDA as a separate voice-enabled entry. Pet conversation and IDA speech
remain future work; this test build implements standalone Xiaozhi first. Use the capacity report
from the exact delivered BIN; hardware acceptance remains pending.

## Controls and Wi-Fi

- Wi-Fi and BLE icons share the top row with battery percentage. Dark means connected/advertising, gray off, amber not configured, pulsing amber connecting, and red error; Settings retains text details. The Wi-Fi icon indicates IP acquisition, not measured signal strength or internet reachability. BLE on means beacon advertising, not pairing.
- The application area is now 240×265, recovering the former 30-pixel network row. Pet art stays at native pixel resolution; no additional image resources are needed.
- Hold OK for 800 ms to open the system drawer; hold again to go back. UP/DOWN selects; short OK confirms on release.
- Settings → Wi-Fi → Find and join network: select a 2.4 GHz network, type the password on the device keyboard, select GO. No phone, SoftAP or web portal is required or started.
- Keyboard pages: abc / ABC / 123 / #+=, with SP, DEL, GO and BACK. All printable ASCII password characters are available. UP/DOWN moves between keys; OK types. Hold OK always cancels/back, never submits.
- Scan shows up to 12 unique visible networks. Hidden/manual name supports up to 32 bytes; passwords support 8–63 ASCII characters. Open networks connect without a password. WEP and enterprise authentication are not supported. Network names are retained byte-for-byte; non-Latin names may lack display glyphs.
- Connecting has a 25-second deadline. Credentials are saved only after obtaining an IP; failed attempts preserve the previous saved network. Retry edits remain masked; cancel/success clears the input. One network is remembered across restarts. ONLINE means an IP was obtained, not a separate internet reachability test.
- Wi-Fi belongs to the system. Switching apps preserves the connection and cancels old app sessions. BLE remains an optional non-connectable Passport beacon, without pairing or Bluetooth audio.

## Screen-off operation

Screen-off mode has Pause app (default, compatible with 0.2.0) and Keep app running. It controls automatic timeout while an app is open. Screen timeout remains 0/60/120/300 seconds. Menu timeout always pauses.

The system drawer also has Screen off (run), which immediately resumes the active app with its backlight off. Short keys continue reaching the app and do not light the screen. Long OK lights it and opens the menu, cancelling the app's foreground session. In paused mode the first complete key gesture only wakes the display. This turns off the backlight and skips app rendering; it is not deep sleep or a measured battery-life guarantee. Xiaozhi can keep audio/network running while dark in Running mode. Pause mode cancels its voice session.

## Capacity and application development

Settings → Storage shows physical Flash, allocated/unassigned space, the 3 MiB program budget and growth headroom, per-app linked Flash/static RAM, app NVS entries and shared settings free entries. Shared libraries count once; dynamic RAM and unassigned Flash are not falsely described as installable application space.

Use [the application integration guide](docs/app-integration.md) for the catalog, scaffold command, lifecycle, build pipeline and next-session checklist. [Community compatibility](docs/community-compatibility.md) assesses Xiaozhi and Spire Expedition; their original BINs replace this platform and cannot be installed as plugins.

## Build and flash

ESP32-C3, 8 MiB, no PSRAM; ESP-IDF 5.5.3. Keep the historical directory firmware/muyu as the build entry. Run ./tools/validate.sh --static and ./tools/validate.sh --firmware. The latter builds all catalog apps with the platform, measures linked usage, relinks fixed-size capacity data and rejects mismatches before merging.

Use only the verified FoloToy-AI-Passport-full.bin at offset 0x0 in the [official web flasher](https://ai-passport.folotoy.cn/tools/web-flasher/). Never full-chip erase. The image must end at or before settings@0x310000, leaving settings (24 KiB) and cardid@0x356000 untouched. Existing 0.2.0 platform settings and saved network retain their namespace/blob format; new screen mode defaults to Pause. A previously selected Device status now falls back to Applications.

Build, host tests and device results are separate. Every release binds source SHA, CI, image hash and capacity-report.json. See [on-device acceptance](docs/platform-acceptance.md); newly built firmware has Device tests NOT RUN until the owner flashes and validates it.

## Provenance

Based on FoloToy/ai-passport f75873f1aab24ac4c0ba9394c131669f66cce650 and the verified Muyu/platform work in this repository. Preserve the [MIT license](LICENSE), upstream BSP and font OFL credits. Keyboard interaction was studied in MIT leo-radio; the implementation is independent. Xiaozhi protocol attribution is preserved in [the voice component notice](components/pp_voice/NOTICE). No credentials are committed.
