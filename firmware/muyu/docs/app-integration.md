[简体中文](app-integration.zh_CN.md) · **English**

# Application integration and firmware delivery

Version 0.3.0 uses one statically linked firmware, one foreground custom app and shared system services. A community full BIN is a replacement operating image, not a plugin. Do not concatenate BINs, create an app partition per menu entry, or call another project's `app_main`.

## Repeatable development path

1. Read the current platform guide, `apps/catalog.json`, `passport_apps.h`, `passport_core.h` and the Muyu component. Start from the latest tested platform commit, not the old standalone Muyu branch. Preserve unrelated work; use a `codex/*` branch.
2. From `firmware/muyu`, run `python tools/new_app.py my_app --name "My App"`. It creates `components/pp_app_my_app/` and updates the catalog, refusing existing names. The generated counter is an integration scaffold, not a production app. Up to 16 apps are supported.
3. Implement the module callbacks. The catalog owns ID, display name, version, author, component, exported module symbol and NVS namespace. CMake generates the runtime registry from this same catalog. No edits to the shell's application menu are needed.
4. `start` initializes lightweight app state; `focus(true)` acquires resources after activation; `key` receives semantic short presses. `focus(false)` suspends work; `stop` must cancel and join workers before UI destruction. The shell reserves long OK. It supplies a 240×235 content container below the status bar; UI creation/rendering run under its LVGL lock. Apps do not initialize the display, buttons, network stack or codec again.
5. Use `pp_app_runtime()` only on the control task. Register bounded, idempotent cancellation callbacks with `pp_resource_acquire`; old generation results must be discarded. Network workers return bounded messages to the control task. A stop flag alone is not a joined producer. The current audio service supports the Muyu knock; microphone/streaming adapters still need implementation and resource ownership tests.
6. Save into the `settings` partition using the catalog's `app_*` namespace. Add versioned serialization and migration tests. Never erase shared NVS or the whole Flash. Do not persist pointers, plaintext provider keys or logs containing user credentials.
7. Run focused app tests, then `./tools/validate.sh --static` and `./tools/validate.sh --firmware` in ESP-IDF 5.5.3. Review the final `capacity-report.json`, actual LVGL previews and app switching/cancellation tests. Test both directions of switching, Wi-Fi reuse, dark operation, wake, restart and save migration on hardware.
8. Commit the explicitly reviewed app/catalog/tests/docs files, push and update the project PR. Bind the delivered BIN, source SHA, CI run and SHA-256; record Build / Host tests / Device tests separately. Preserve the previous tested package until the user accepts the new one.

## One build produces the whole system

`catalog → generated registry + component list → link platform and all apps → measure linked archives → embed fixed-size metrics → relink → verify metrics unchanged → merge and validate → full BIN + capacity report + checksum`

The capacity tool uses the official ESP-IDF size map. Each app owns one archive. Linked code, constants and initialized data count toward its Flash contribution; BSS does not. Static RAM is reported separately. Shared SDKs, the UI shell, audio worker and image padding count once as system/shared overhead. Per-app values do not claim to equal the Flash reclaimed by uninstalling that app. Dynamic heap/stacks/DMA cannot be attributed by this report.

The two-pass build refuses a stale embedded measurement. Direct `idf.py build` may contain missing/stale metrics and is not a delivery command. Use the complete gate for every distributed BIN.

## Storage boundaries

- Physical Flash: 8 MiB; executable partition: 3 MiB at `0x10000`.
- Persistent settings: `0x310000`, 24 KiB. Device identity: `0x356000`, 16 KiB. Do not move or overwrite either.
- Firmware growth headroom equals the executable partition limit minus the final application image length. Other unassigned Flash is not automatically an installable app area.
- Settings free capacity is shown in NVS entries, not guessed byte capacity. App details show used entries in that app's namespace. Variable-length blobs use metadata/chunks.
- Current resource assets are linked into each app archive. No external asset partition is included by this version. If assets exceed the 3 MiB budget, use compression/subsets or design and review a separate resource layout.

Adding resources above `cardid` changes delivery: a raw merged BIN from `0x0` would write its padding across settings/identity. Such a package must use a reviewed segmented manifest/flasher preserving those ranges. Do not silently enlarge the current full BIN. Dynamic app installation, multi-boot and OTA are separate future designs, not supported by this release.

## Reference projects

Device keyboard interaction was studied in [leo-radio](https://github.com/leo0183/leo-radio/tree/e28b8adafb2ca95e386e8b6e7db0db042cf09523), under MIT. This platform implements its own bounded keyboard/state machine and station-only connection service; it does not copy the radio's player, artwork or portal. See [compatibility assessment](community-compatibility.md) for Xiaozhi and Spire Expedition.
