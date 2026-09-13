# Standalone Xiaozhi test build

English | [简体中文](xiaozhi-standalone.zh_CN.md)

Platform 0.5.0 introduces a standalone Xiaozhi application. The pet remains offline;
personality, pet dialogue and iDA are later stages. Cloud personality and memory
remain controlled by the device's existing Xiaozhi agent configuration.

## Controls and ownership

- Select Xiaozhi in Apps. Configure Wi-Fi in system Settings if required.
- At Ready, press OK, speak, then press OK to send. Recording stops at 30 seconds.
- During a reply, OK stops the session; OK again reconnects. Long OK opens the menu.
- Screen-off Running mode preserves voice; Paused mode revokes the session.
- Opening a menu, switching apps or pausing revokes recording immediately. A worker
  completes bounded I/O and releases codecs/transport before another session starts.
  Workers never access LVGL, so deleting app UI cannot leave a dangling callback.

## Integration and storage

`pp_app_xiaozhi` owns the view. `pp_voice` owns discovery, a TLS WebSocket connection,
Opus and the system audio lease. There is one voice session and one audio owner.
Wi-Fi is shared. UUID is generated once (or imported from upstream `board/uuid`)
and committed to the protected `settings/svc_xiaozhi` namespace. Tokens, raw audio,
transcripts and session IDs stay in RAM and are never written to logs or files.
Discovery uses the physical station MAC with the official OTA discovery endpoint;
firmware update URLs and remote reboot/update commands are ignored.

The active catalog contains Xiaozhi and Yaya Pet. Archived Muyu sources remain for
regression/reference, but its app, font, tone and resident audio worker are absent
from the new firmware link. Pet saves and Wi-Fi configuration retain their namespaces.
No partition moves: program 3 MiB, settings at 0x310000, cardid at 0x356000.
Capacity checks now reserve at least 512 KiB for future app integration, in addition
to the linked voice code. The previous 1.25 MiB reserve was specifically for adding
voice and is consumed by this stage. Per-app archives and shared voice are counted
separately; runtime heap, stacks and DMA require device measurements.

## Protocol and test boundaries

Use the WSS URL/token returned by discovery. A missing WSS configuration produces
an explicit error; MQTT/UDP is not implemented in this test build. Support binary
protocol versions 1/2/3, 16 kHz mono uplink with 60 ms Opus frames, bounded fragmented
messages (8 KiB, JSON depth <=16) and server ping/pong. Playback decodes to 16 kHz at a fixed I2S format.
No wake-word engine, AEC, simultaneous listening/playback or large image assets.

Host tests exercise frame boundaries, continuation frames, oversize rejection,
header injection, URL validation and identity shape. Firmware CI verifies the
8 MiB board image, unchanged protected layout, component lock and final capacity.
Cloud discovery/handshake, host rendering, compilation and physical voice are
separate acceptance items. Never infer microphone/speaker success from a build.

Before device acceptance, test: Wi-Fi loss, retry, 30-second capture limit, quiet
microphone, interrupted reply, rapid menu/app switches, both dark modes, reboot
identity, pet save retention, heap recovery and stack high-water mark. Firmware
logs include counts and memory totals, never tokens, transcripts or MAC addresses.

## Upstream attribution

Protocol and ES8311 behavior reference [FoloToy's Xiaozhi fork](https://github.com/FoloToy/folo-ai-passport-xiaozhi/tree/d24fce080d86d7cc642f71585f6efde40fb99104)
(MIT, Shenzhen Xinzhi Future Technology Co., Ltd. and contributors), based on
upstream 2.4.2. The project LICENSE and [voice attribution notice](../components/pp_voice/NOTICE) preserve the sources.
This is an independent C adapter, not a replacement with upstream's whole firmware.
See [integration research](xiaozhi-research.md) for source comparison and rationale.

Libraries: ESP-IDF 5.5.3 TLS/HTTP/WebSocket transport and Espressif esp_audio_codec
2.5.0, direct Opus APIs only. Component license files ship with managed dependencies.
Build through the project's normal CI and full-BIN delivery pipeline; the Xiaozhi
cloud firmware builder does not include this platform or pet application.
