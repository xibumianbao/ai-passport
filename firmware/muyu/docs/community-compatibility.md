[简体中文](community-compatibility.zh_CN.md) · **English**

# Community application compatibility assessment

Reviewed on 2026-09-13. This is a source/layout assessment, not a completed port or hardware acceptance. Both community pages explicitly describe replacement firmware. Installing either published full BIN replaces this platform.

| Reference | Assessment | Main work |
| --- | --- | --- |
| [72: Xiaozhi](https://ai-passport.folotoy.cn/plays/72/) | Feasible as an adapted source module; substantial audio/protocol lifecycle work | Reuse system Wi-Fi/BSP, provide cancellable voice tasks, reserve global long OK, fit assets and protected layout |
| [190: Spire Expedition](https://ai-passport.folotoy.cn/plays/190/) | Hardware is suitable; port requires a license decision and resource budget validation | Replace device drivers, adapt lifecycle/input/save namespace and optimize images/audio/fonts |

## Xiaozhi

Source snapshot: [FoloToy/folo-ai-passport-xiaozhi @ d24fce0](https://github.com/FoloToy/folo-ai-passport-xiaozhi/tree/d24fce080d86d7cc642f71585f6efde40fb99104). MIT with NOTICE; based on upstream Xiaozhi 2.4.2. Its README recommends IDF 6.0.2 and reports a 5.5.3 build. This platform remains pinned to 5.5.3 pending an actual port build.

The board matches ESP32-C3, 8 MiB, no PSRAM, 240×320 and ES8311. Its selected `partitions/v2/8m.csv` has `ota_0@0x20000` and two `0x2f0000` application slots, plus 2 MiB assets at `0x600000`. The second OTA slot spans our settings and protected identity addresses. Its `app_main` owns NVS recovery, initialization and an infinite application loop. Importing that entry point or its partition table would bypass platform ownership.

Recommended first port: button-driven half-duplex voice with WebSocket, shared Wi-Fi, a small text UI and screen-off operation. Keep activation/binding instructions visible until completed. Stop upload, playback and sockets with acknowledgements when leaving; inspect `AudioService::Stop` and thread completion together rather than assuming a stop flag joins workers. Start without local wake-word models, extra transports or OTA; add each only after measured RAM/Flash and cancellation tests. Model/asset requirements may need a separate resource-delivery design. No provider credentials are included in the platform.

## Spire Expedition

Source snapshot: [PhoenixZHC/Spire-Expedition @ 060953d](https://github.com/PhoenixZHC/Spire-Expedition/tree/060953df6d65914c63b5b88bd06a7e4ab2e07b84). README describes IDF 5.5.4, LVGL 9.5.0 and the same board. It explicitly states there is no unified source-code license; assets have individual credits. Resolve source redistribution authorization before copying it into this public repository; this assessment copies no game source or assets.

Its partition table gives the factory app `0x7f0000` bytes from `0x10000`, spanning our protected regions. The full release is padded to 8,388,608 bytes. Local parsing of v1.4 finds an actual application image of **2,398,000 bytes**, with valid esptool checksum/hash; the 8 MiB download is not its live application footprint. This does not prove that the game plus our shell fits: shared libraries deduplicate, but the final combined link must stay below 3 MiB.

Port `game_core`, state/content and rendering behind an app module; substitute platform display, buttons, audio and battery instead of initializing the `device_*` drivers. Reserve long OK for the system menu and retain the game's explicit End Turn item. Save only in `app_spire` with versioned migration; preserve the current platform's settings. Validate animation/timer teardown, mid-battle save/resume and repeated switches.

Optimize the large read-only resources first: remove promo/debug assets from firmware, keep only needed font glyphs, compress music/effects or synthesize simple effects, then measure final linked Flash and maximum free RAM. External resources above identity require segmented flashing, as described in [the integration guide](app-integration.md). The platform does not support installing the original game BIN alongside itself.
