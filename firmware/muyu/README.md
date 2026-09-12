<p align="right"><a href="README.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Passport platform 0.2.0

A persistent system shell for the FoloToy AI Passport. It contains two registered apps: Muyu and Device status. Normal boot restores the last successfully started app; a first boot, removed app, or repeated failed startup opens the application menu. The historical source directory `firmware/muyu` is retained for build compatibility.

## Controls

- Hold OK for 800 ms: system menu. Inside a menu: back one level.
- UP / DOWN: select. Short OK: confirm on release; a long press never also confirms or strikes.
- Muyu: UP / DOWN strike immediately; short OK strikes on release. Count survives app switching, resets on reboot.
- Settings: Wi-Fi, BLE beacon, volume (0-100), brightness (10-100), screen timeout (off/60/120/300 seconds), diagnostics.
- A sleeping screen consumes the first button gesture to wake. Screen timeout turns off the backlight and pauses the app; this is not deep sleep. Wi-Fi stays managed by the system.
- App identity is saved after 10 seconds of stable operation. Settings save one second after the last adjustment, or when confirmed. Two interrupted startup windows fall back to the menu.

## Shared networking

Wi-Fi is owned by one system worker. Switching apps preserves the connection and saved configuration. Application sessions have generation tokens and cancellation registrations: opening the menu or switching invalidates old tokens and cancels owned resources. Future HTTP/recording adapters must register cancellation and validate tokens on the control task. Shared transport never grants another app the previous app's cloud identity.

In Settings > Wi-Fi > Set up with phone, join the `Passport-…` access point using the random password shown on the device, then open `http://192.168.4.1`. Enter a 2.4 GHz Wi-Fi name and password. The setup AP closes after five minutes or Stop setup; returning to the app preserves setup until then. The form uses a password-protected local AP, limits input length, rejects malformed/duplicate fields and cross-origin requests, and does not echo credentials. This does not provide TLS or encrypted Flash storage. ONLINE means an IP address was acquired, not a verified Internet connection.

A manual scan lists up to four nearby networks; joining uses phone setup. BLE is a non-connectable `Passport` beacon only: no pairing, Bluetooth audio, GATT data service, or BLE provisioning is claimed. The BLE host is initialized on demand and stopped when disabled. Wi-Fi/BLE coexistence and memory remain board acceptance items.

## Architecture and extension

`passport_core` is a host-testable lifecycle, input and cancellation module. `passport_app` owns the app registry and control loop, navigation, persistence and view state. `passport_ui` reuses a single LVGL object tree. `passport_audio` owns codec writes and drains buffered audio before acknowledging cancellation. `passport_radio` owns Wi-Fi, provisioning and reconnect; `passport_ble` owns the NimBLE lifecycle. Radio/audio callbacks never access UI. The control task owns runtime/session access, including resource acquisition/release and callback validation; workers send bounded results back rather than racing that state.

Add an app descriptor to `APPS_LIST`: stable ID, display name, version, author, start/stop/focus/key handlers. Start prepares lightweight state; acquire session resources in focus(true), after activation. Stop must finish application-owned work. Cancellation is idempotent, bounded and must join/acknowledge any asynchronous producer before deleting application state. There is one foreground app and eight bounded cancellation slots. This is cooperative lifecycle management, not process isolation. Dynamic installation, OTA, a voice gateway, microphone sessions, and general network request adapters are future work.

## Persistence and Flash

ESP32-C3, 8 MB, ESP-IDF 5.5.3; the 3 MiB app ceiling and `cardid@0x356000` are unchanged. New `settings` NVS is at `0x310000`, size `0x6000`. The verified merged file must end at or before `0x310000`, preserving both settings and identity on subsequent raw writes. No settings/identity payload is included. Platform keys use namespace `passport`, Wi-Fi uses `pp_wifi`; future app data must use separate namespaces. Wi-Fi credentials are saved as a single bounded blob. Storage initialization never auto-erases partitions on an error.

Use only the checked **FoloToy-AI-Passport-full.bin**, offset **0x0**, with the [official web flasher](https://ai-passport.folotoy.cn/tools/web-flasher/). Never erase the entire chip. The merged image may replace the default `nvs` driver cache below the app; platform settings live outside that range. Do not flash the app-only image at 0x0. This version changes the partition table by adding settings in the previously unused gap; it does not move existing partitions. Board tests are required before merge.

## Validation

Run `./tools/validate.sh --static` and `./tools/validate.sh --firmware`. GitHub Actions uses the pinned IDF container, builds from a clean configuration, checks the dependency lock, validates partitions and hashes, then renders the actual LVGL sources. Core tests include 10,000 clicks, 10,000 lifecycle switches, cancellation capacity, stale tokens, failure recovery, settings validation and 20,000 malformed form inputs with ASan/UBSan. The platform UI runs 5,000 transitions in a 32 KiB LVGL pool. Host heap and simulated network labels are not board RAM or connectivity measurements.

Build and host results belong to the exact CI/source SHA. `build-info.json` records identity, target, version, hash, length and device tests NOT RUN. See [board acceptance](docs/platform-acceptance.md) for the remaining tests. This package is for the first on-device validation, not a claim of hardware acceptance.

## Provenance

Based on FoloToy/ai-passport `f75873f1aab24ac4c0ba9394c131669f66cce650`, the Muyu example `16df9944d0f6a83b475e05acabbea73c8b49c3e1`, and our verified Muyu baseline `16f9a7de434e6918e1a98caeecff28b171bcfea3`. Upstream [MIT license](LICENSE), BSP, engineering references, Muyu logic and font are retained. The system menu follows the requested monochrome list/drawer direction; it uses actual vector widgets, not screenshot assets.
