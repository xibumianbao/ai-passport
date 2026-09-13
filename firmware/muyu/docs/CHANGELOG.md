<p align="right">
  <a href="CHANGELOG.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Changelog

## 0.6.1 - Lightweight conversation feedback

- Give Yaya Chat distinct listening, thinking, speaking and idle motions. Keep the existing fixed words and use pixel dots/bars instead of a large transcript font.
- Drive speaking mouth shapes and bars from successfully submitted playback PCM, independently of microphone level; clear stale or cancelled output. Keep the audio buffers, protocol and 40 KiB worker stack.
- Use a compact speech card with no added image, font, canvas or background task. Reset motion after menu/screen-off; preserve activation and error readability and Pet rendering/saves.
- Gate image growth at 8 KiB and linked chat/avatar/voice static RAM growth at 256 B against 0.6.0. Device timing, peak RAM and extended voice stability require separate acceptance.

## 0.6.0 - Independent Yaya care and chat

- Keep Yaya Pet and Yaya Chat as independent menu entries. Share only pixel artwork; chat does not read or change pet saves, and the existing Qixi cloud configuration is unchanged.
- Replace the Xiaozhi orb with the shared room/avatar and state-driven speech card. Reuse one 4,288-byte sprite surface; keep the existing voice service, TLS, Opus and 40 KiB worker unchanged.
- Expand content to 240x290 and remove the permanent bottom hint bar. Preserve activation/errors, keyboard help and button-failure visibility.
- Remove Storage, About, device/runtime/capacity subpages and Bluetooth. Disable BT/NimBLE/coexistence in the build; retain Wi-Fi provisioning, volume, brightness, timeout and screen-off modes.
- Keep developer capacity/link reports and the protected layout/reserve gates. Add the shared `--preview` gate to complete validation. New device acceptance and measured memory savings remain pending.

## 0.5.3 - Automatic Xiaozhi conversation

- Start with one OK press, use service endpointing and listen again after each reply drains.
- Stop capture on TTS start; clear stale microphone samples before the next turn.
- Add a tested turn state machine, bounded cancellation/timeouts and safe transport diagnostics.
- Real host testing passed two automatic turns on one connection. 0.5.2 physical recording/playback passed with stability gaps; new 0.5.3 hardware acceptance remains pending.
- Preserve the 40 KiB worker stack, codec parameters, shared Wi-Fi and protected storage layout.

## 0.5.2 - Xiaozhi codec diagnostics

- Run fixed PCM through the actual encoder and decoder before voice networking or I2S acquisition.
- Measure a 40 KiB worker stack without changing Opus parameters; log first real capture/decode boundaries.
- Add a bounded host voice probe with credential-safe statistics and offline protocol tests.
- 0.5.1 hardware logs confirm a Xiaozhi task stack overflow after encoder creation. This diagnostic build still requires device validation.

## 0.5.1 - Xiaozhi runtime memory

- Address the real C3 Opus encoder allocation failure with upstream Wi-Fi/TLS memory settings.
- Use one codec direction at a time; release codecs while idle, thinking or cancelled.
- Preserve specific initialization errors and report internal heap, largest block and stack headroom.
- Add 10,000-turn codec ownership and failure-injection regression checks. New hardware acceptance is pending.

## 0.5.0 - Standalone Xiaozhi test build

- Add shared-Wi-Fi Xiaozhi voice, protected identity and bounded manual half-duplex sessions.
- Remove Muyu from the linked firmware; keep the pixel pet offline for later voice integration.
- Device testing subsequently found an Opus encoder allocation failure; superseded by 0.5.1.

## 0.4.1 - Compact system status bar

- Place Wi-Fi and BLE icons beside the battery, replacing the separate network-text row.
- Expand the application container from 240×235 to 240×265; adapt pet and Muyu spacing without enlarging image assets.
- Keep detailed radio states in Settings and preview off, online, connecting, unconfigured and error states, including long names and 100% battery.
- Preserve pet save format, game rules, device wiring and protected Flash partitions.

## 0.4.0 - Pet 0.1.0 test build

- Add a low-interaction pixel pet: autonomous daily life, feed/play/rest, levels and one evolution.
- Add two small CRC-checked saves, failure-safe transactions and pause on menu/screen-off/app switch.
- Reuse a 16-color room, one small sprite buffer and subset font; enforce future voice headroom.
- Add independent module ticks and a session-scoped Avatar contract for later Xiaozhi integration.
- Plan IDA as a separate voice-enabled entry; neither network integration is included in this build.
- Add model/persistence/art tests and real LVGL switching previews; hardware acceptance remains pending.

## 0.3.0

- Replace phone provisioning with an on-device Wi-Fi keyboard, bounded attempts and save-on-success.
- Keep only custom apps in Applications; add system Device status, Storage and running screen-off mode.
- Generate app registration from a catalog, provide a scaffold, and embed verified per-app linked capacity.
- Document source-port requirements for Xiaozhi and Spire Expedition; preserve protected Flash layout.


## 2026-09-13 - Passport platform 0.2.0

- Add the persistent app shell, last-app recovery, shared Wi-Fi provisioning, BLE beacon control, shared settings and screen timeout.
- Add generation-scoped cancellation, release-based OK input, and host/UI stress tests.
- Reserve settings NVS at 0x310000; preserve the existing app limit and card identity.
- Hardware acceptance is pending; see [the checklist](platform-acceptance.md).

## Unreleased

- Added the standalone offline Muyu MVP: any button strikes once on press,
  synthesized wood audio, bounded mixing, animated feedback, session counter,
  battery indication, host logic tests and actual-LVGL preview checks.

- Added the supplied 80-byte CW2017 profile for the specified 520 mAh cell, including content/update-flag checks, verified writes, the required restart sequence, and bounded SOC-readiness polling.

- Expanded the environment bootstrap document: added Espressif's Git service mirror (`git.espressif.com.cn`) as the preferred mainland-China route for ESP-IDF v5.5.3 and its submodules, documented submodule long-wait/timeout handling, in-place repair, and the pinned-commit shallow fetch for large submodules such as `esp32-wifi-lib`, warned about stale per-repository Jihulab `insteadOf` residue, and added the official offline release archive as a last-resort fallback (learned from `esp-mosaico/esp-mosaico-vibe`).

- Reorganized the documentation by function area with a dual entry point: the root `AGENTS.md` is now a thin router (hard constraints + task routing only) and the detailed AI workflow lives in `docs/development/ai-guide.md`; `agent-guide.md` was folded in. `docs/development/` gained a second level (`engineering/`, `ci/`, `release/`), and the `plays/` application archive and `experiences/` moved into a `docs/reference/` area with a dedicated README. Removed `docs/software-design/` (empty scaffold); folded the three `assets/{fonts,images,music}/README` leaves into the `assets/` README; flattened the six `project-completion` sub-documents into a single file; and unified each directory to a single README, eliminating every `INDEX` file and a duplicated experience index. All cross-references and bibliographic links were updated; no content was dropped.

- Removed the obsolete app/test partition at `0x700000` and its related
  bootloader, validation, and documentation requirements. The fixed protected
  `cardid` partition and its CI checks remain unchanged.
- Documented a release-title convention for multi-app releases: name tags as `v<version>-<app-name>` (e.g. `v0.1.0-voice-keychain`) so the release title carries the version and the app, and confirm the title after the release is published so a release list is scannable by app.
- Added a post-release follow-up workflow: an `issue-suggestions` skill for filing user feedback as issues against the upstream project, an `experience-pr` skill for submitting reusable development experience as a documentation PR, a `docs/experiences/` directory for per-entry experience files, and supporting `project-completion`, `file-issues`, and experience-index documents.
- Simplified the tracked repository root: moved GitHub-recognized community documents into `.github/`, moved the changelog into `docs/`, updated every reference, and added a root-document allowlist to repository checks.
- Repository-wide language policy: every maintained Markdown default `.md` file is English, Simplified Chinese uses a paired `.zh_CN.md`, and both provide language switches. Static checks reject missing peers, missing switches, and Chinese prose in English defaults.
- Phase one of the AI development workflow: streamlined task-based context routing, unified local/CI validation, added PR checks and a template, and committed the dependency lock for reproducible builds.
- PR review fixes: pinned GitHub Actions to full commit SHAs, split build/release jobs by least privilege, disabled persisted sync checkout credentials, added Feature Request and Usage Question forms, clarified private security-report fallback, and corrected stale README, CI-trigger, and branch descriptions.
- Changed commit titles, PR titles, and PR bodies from Chinese-default to English; updated the Chinese punctuation rule so it no longer applies to PR descriptions.
- Reworked `build-firmware.yml` to pass `SDKCONFIG_DEFAULTS=sdkconfig.defaults`, enable `partitions.csv`, preserve the 8 MB image header, merge a flashable `FoloToy-AI-Passport-full.bin`, publish only that artifact, and use Actions cache v5.
- Integrated upstream PR #6 to resolve PR #4 conflicts: Wi-Fi, Bluetooth LE, radio lifecycle, and low-power demos; a 3 MB factory partition; build/menu/configuration updates; hardware-guide coverage; and bilingual capability tables.
- Defined English imperative Conventional Commit formatting for both commits and PR titles.
- Removed stale sync-workflow template comments and generalized an irrelevant Redis TTL rule to cache components.
- Added Chinese punctuation, credential safety, and recoverable file-deletion conventions.
- Expanded source-comment requirements for functions, state, ownership, concurrency, timing, registers, and magic values.
- Removed AI execution instructions from product READMEs so they remain human-facing product and repository overviews.
- Added `docs/development/agent-guide.md` as the focused AI workflow guide.
- Updated `AGENTS.md`, `docs/INDEX.md`, and the development index for the agent guide.
- Documented why the root README path is reserved for fork owners and how GitHub README precedence supports it.
- Created `main-update` from the upstream-aligned baseline and combined the repository-structure, firmware-CI, and upstream-sync work.
- Corrected the merged documentation index, workflow path, project tree, and CI references.
- Moved CI documentation from software design to `docs/development/`.
- Moved fork-only documentation assets from `assets/docs/` to `docs/assets/`.
- Moved the upstream English/Chinese project READMEs under `docs/` and renamed the documentation catalog to `docs/INDEX.md`.
- Initialized `AGENTS.md`, `CLAUDE.md`, and `CHANGELOG.md`.
- Standardized the initial project README language filenames.
- Added the `docs/`, `assets/`, and `skills/` directory structure.
- Moved the upstream hardware guide into `docs/hardware-design/`.
- Standardized subdirectory README capitalization and introduced fork conventions.
- Allowed fork-owned root README and supplemental documentation content on fork `main`.
- Added and documented the fork-only supplemental-document directory.
- Moved the build CI document to its dedicated CI branch before consolidation.
- Documented clean-`main` reasons, the direct-development exception, and Actions enablement for forks.
- Split the original agent rules into contribution, development, and fork documents with a compact root index.
- Updated software-design and project README references for the new documentation structure.
- Added the documentation catalog and task-triggered routing based on the earlier repository model.
- Added bilingual contribution, code-of-conduct, security, and support documents tailored to this ESP-IDF and fork workflow.
