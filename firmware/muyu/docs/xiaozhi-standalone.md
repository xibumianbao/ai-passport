# Yaya Chat: standalone Xiaozhi application

English | [简体中文](xiaozhi-standalone.zh_CN.md)

Platform 0.6.0 names the independent conversation entry **Yaya Chat**. Yaya Pet remains an offline game without a model/save connection to chat. Both reuse pixel artwork; the existing Qixi cloud personality, voice and memory configuration is unchanged. The former pet-data/voice integration plan is cancelled.

## 0.6.0: shared appearance, independent application

The view uses the existing room/font and one shared 96x88 I4 sprite. Listening/thinking/speaking follow existing voice snapshots; activation and errors stay visible. No pet model/store, animation worker, large image or second sprite buffer is added. Content is 240x290 without a permanent footer.

`pp_voice`, TLS, Opus, the 40 KiB worker and the automatic conversation behavior below are unchanged. Bluetooth/NimBLE and Storage/About/device/runtime/capacity pages are removed; Wi-Fi, audio/display settings and screen modes remain. Developer capacity reports continue. New 0.6.0 Device tests are NOT RUN. See [current ownership boundaries](voice-integration.md).

## 0.5.3: automatic continuous half-duplex conversation

Press OK once at Ready to begin. Speech pauses are handled by the service;
`listen/start` uses `mode:auto`, and no per-turn `listen/stop` is sent.
`tts:start` ends capture and releases the encoder. STT is informational:
the protocol has no finality flag, so STT never stops capture.
After `tts:stop`, the worker drains speaker DMA, releases the decoder and
discards stale microphone DMA samples before starting the next automatic turn.
One codec direction remains active at a time. No local VAD, AEC, wake-word
model, extra audio-history buffer or background reconnect is introduced.

Short OK stops an active conversation; at Stopped, one OK reconnects and starts
again. Error retry reconnects to Ready and requires OK to begin recording.
Long OK/menu, app switching and Paused
screen-off mode revoke it. Running screen-off mode continues the active session.
An individual listening turn is limited to 30 seconds; a reply with no audio
progress for 60 seconds ends with an error. There is no automatic recording
after cancellation, network failure or timeout. The 40 KiB worker stack and
fixed-PCM diagnostic remain unchanged while runtime reserve is still measured.

A real PC test completed **two automatic turns on the same WSS/session**:
114 uploaded/19 received frames, then 113/23. Both recognized the synthetic
request and returned the expected spoken response. Neither turn sent a manual
stop. This proves service endpointing and protocol continuity, not device I2S
handoff or acoustic echo behavior. New 0.5.3 device acceptance is **NOT RUN**.
Use repository-root `tools/xiaozhi_auto_voice_probe.py` for a bounded in-memory
probe; its offline tests run with the existing manual probe in CI. Credentials,
transcripts, DLLs and audio are excluded from the repository.

The production `pp_voice_turn` state machine has host coverage for repeated
turns, STT-before-TTS, duplicate events, timeout, session mismatch and
cancellation. Integration checks must still cover the actual blocking codec,
I2S and transport calls. Read the [sanitized observations](xiaozhi-voice-validation.json).
Behavior follows the pinned [native application](https://github.com/FoloToy/folo-ai-passport-xiaozhi/blob/d24fce080d86d7cc642f71585f6efde40fb99104/main/application.cc)
and [automatic listen command](https://github.com/FoloToy/folo-ai-passport-xiaozhi/blob/d24fce080d86d7cc642f71585f6efde40fb99104/main/protocols/protocol.cc).

## 0.5.2: physical recording/playback passed; extended stability pending

The user flashed the delivered diagnostic image and confirmed smooth physical
conversation after reconnecting. Captured logs show three successful fixed-PCM
selftests, real microphone encoding, cloud audio decoding and speaker writes;
minimum observed remaining stack is 16,192 bytes, with no panic in that window.
The diagnostic signature matches 0.5.2, but this capture did not contain the boot
version/ELF header. Minimum free heap is 17,360 bytes and largest block 8,192
bytes, below the project's 20 KiB observation target. One connection ended and
cleaned up normally; its cause remains unknown. This is **PARTIAL PASS**, not
long-session or complete lifecycle acceptance. The following explains the
diagnostic design and historical manual controls.

New 0.5.1 serial evidence matches the firmware version and ELF digest: encoder
and decoder creation succeeds, but starting recording produces
`A stack overflow in task xiaozhi has been detected`, followed by reboot.
This differs from the 0.5.0 encoder allocation failure. The stack guard detects
corruption; fixed PCM tests must still distinguish actual depth from other writes.

This build preserves VOIP, 16 kHz, 60 ms, 24 kbps and complexity 0, increasing
the worker from 24 to 40 KiB as an initial measurement budget. The Espressif
[2.5.0 documentation](https://components.espressif.com/components/espressif/esp_audio_codec/versions/2.5.0/readme)
excludes stack from its heap tables and describes roughly 40K stack for all
encoders; it does not establish an exact Opus/C3 minimum. The extra 16 KiB
comes from heap, so TLS-active minimum heap and largest block must be measured.
Changing project optimization cannot rebuild the SDK's precompiled Opus library.

Opening Xiaozhi now processes fixed PCM before device discovery, TLS or I2S.
Silence, waveform, deterministic noise and near-clipping inputs exercise actual
encoding/decoding with short reused buffers; no ambient recording, upload or
audio persistence occurs. Only a successful probe proceeds to the cloud.
Serial diagnostics report SDK results, lengths, duration and stack/heap, and mark
first real I2S capture, recording encode and reply decode boundaries. A 4 KiB
remaining-stack floor is a project diagnostic threshold. A TLS-active largest
block near 20 KiB is an observation target, not an SDK guarantee or automatic pass.
Codec parameters, BSP, task-reclamation order, Wi-Fi/TLS and pet logic are unchanged.
See the sanitized [validation observations](xiaozhi-voice-validation.json).

A real host voice round trip has passed: 6.28 seconds of synthetic speech,
105 uploaded frames and 23 reply frames; server 24 kHz Opus decoded at 16 kHz.
STT matched the test request and TTS matched the expected reply, producing
1.38 seconds of non-silent audio. A second turn after transport-deadline changes
also passed semantics with 105 uploaded and 19 received frames.
It used the authorized connected device identity
and a temporary host Client UUID. Tokens and transcripts stayed in RAM; no
activation, agent-configuration change or tool execution occurred. This does not
validate firmware persistent identity, microphone, speaker, ESP codec stack or
application lifecycle.

The repository-root `tools/xiaozhi_voice_probe.py` accepts an in-memory
`ConnectionConfig` from a trusted local wrapper and a 16 kHz mono PCM16 WAV up to
30 seconds. It returns safe statistics plus RAM-only STT/TTS for semantic checking,
with a 90-second response bound. Dependencies are websocket-client and Xiph
libopus; DLLs, audio, device identifiers and discovery responses stay outside Git.
Run offline tests with
`python -m unittest discover -s tests -p test_xiaozhi_voice_probe.py -v`; CI also runs them.

Device sequence: flash 0.5.2, open Xiaozhi and observe the local diagnostic; after
Ready, press OK to record and OK to send, then check audible output and logs.
Follow with repeated turns, menu cancellation during recording/reply, re-entry,
network loss and both screen-off modes. Old self-deleting worker stacks may await
IDLE reclamation during rapid re-entry, and BSP initialization rollback is incomplete;
these separate lifecycle risks are not fixed by this single-variable diagnostic.
Retain the first error, firmware version, stage and comparable memory readings
before choosing the next change; do not keep increasing stack without evidence.

## 0.5.1: C3 runtime-memory correction (historical implementation)

The 0.5.0 device test failed before recording: the SDK logged
`ESP_OPUS_ENC: Opus encoder init failed. ret:-7.` and the screen showed
`Not enough memory for Opus`. In the [Opus API](https://github.com/xiph/opus/blob/v1.5.2/include/opus_defines.h),
-7 means allocation failure. This is runtime SRAM, not the 1.07 MiB program-Flash
headroom reported for that image. The observed 61,416-byte heap was logged AFTER
transport cleanup, with worker/context still allocated; it is not the free heap
at the failed allocation, nor proof of a leak. Largest free block was not recorded.

0.5.1 follows the [upstream memory settings](https://github.com/FoloToy/folo-ai-passport-xiaozhi/blob/d24fce080d86d7cc642f71585f6efde40fb99104/sdkconfig.defaults):
optional Wi-Fi instruction paths run from Flash, freeing shared C3 instruction/data
SRAM; TLS buffers are dynamic and peer certificates are released after verification.
The [IDF 5.5.3 settings](https://github.com/espressif/esp-idf/blob/v5.5.3/components/mbedtls/Kconfig)
retain 16 KiB incoming TLS records and normal certificate/hostname validation.
TLS configuration data is released after the handshake, renegotiation is disabled,
and every reconnect creates a new TLS object. Network throughput must be checked
on the board after moving the optional Wi-Fi paths out of SRAM.

One `pp_voice_codec` handle owns either the encoder OR decoder. Hardware/DMA is
prepared first; startup checks both codecs sequentially and releases them before
Ready. Recording allocates only the encoder, listen-stop frees it before sending
the control message, and the first TTS audio packet allocates only the decoder.
TTS-stop and cancellation release the active codec. Idle/thinking retain neither.
The 24 KiB worker stack and 32 KiB LVGL pool are preserved until actual stack and
UI headroom measurements justify changes. No new assets or partition changes.

Errors now identify the direction and SDK return code; generic connection errors
cannot overwrite the first failure. Serial diagnostics record free internal heap,
largest block, historical minimum and remaining stack at allocation/release stages.
The final cleanup log explicitly says the worker stack is still allocated.
Compare repeated runs at the SAME stage; first hardware initialization retains
system DMA handles, and the historical minimum never increases.

Host regression runs 10,000 codec direction changes and injected allocation,
partial-allocation, null-handle and frame-query failures against the production
owner. Its synthetic allocator budget is not a measurement of Espressif Opus RAM.
The firmware gate checks resolved memory settings. 0.5.0 Device: FAIL at encoder
initialization. 0.5.1 was NOT RUN at delivery; subsequent device testing is FAIL
with a recording-stage task stack overflow. See the 0.5.2 diagnostic above.

## Controls and ownership

- Select Yaya Chat in Apps. Configure Wi-Fi in system Settings if required.
- At Ready, press OK once; pauses send automatically and listening resumes after each reply.
- During conversation, OK stops the session. Long OK opens the menu. Each listening turn is bounded to 30 seconds.
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

The active catalog contains Yaya Chat (stable ID `xiaozhi`) and Yaya Pet. Archived Muyu sources remain for
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
