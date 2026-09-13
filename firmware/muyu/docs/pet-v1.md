[简体中文](pet-v1.zh_CN.md) · **English**

# Yaya Pet test version

Platform 0.6.0 keeps Yaya Pet 0.1.0 as an independent offline application. Open **Applications → Yaya Pet**. Yaya Chat is a separate menu entry sharing artwork only; chatting never loads or changes the pet save. New device acceptance remains pending.

Platform 0.6.0 keeps Wi-Fi beside the battery, removes Bluetooth and the permanent footer, and expands content to 240x290. The original scene stays native-sized; bottom actions follow container height. Game rules and save format are unchanged.

## Play with few buttons

Yaya lives in a warm pixel room, looks around, reads, helps with a plant and walks. One minute of visible, focused daily activity gives 4 star coins and 3 XP automatically. Eating, playing, resting, growth celebrations and time outside the foreground pause that minute. Low fullness pauses earning; low energy starts a nap. There is no death, level loss, offline decay, daily streak or login requirement.

- Up/down select **Feed / Play / Rest**. Short OK executes once; long OK always opens the system menu.
- Feed combines purchase and eating: 5 coins, +35 fullness, +2 XP. At fullness above 60 no money is spent. With fewer than 5 coins the same action gives free basic food without XP. Price/free status is visible before execution.
- Play is a ten-second automatic ball animation. It needs no repeated input and does not farm coins or XP.
- Rest restores two energy points per second, up to 60 in 30 seconds, capped at 100. Short OK wakes Yaya; it never also triggers another action.
- Short OK during eating or a celebration skips only its animation. Repeated input cannot charge for the same meal twice.
- Levels 1–5 use cumulative XP 0/6/16/32/56. Level 3 evolves the same pet and raises daily earnings to 5 coins. Level 5 remains playable; current XP caps at 56.

The initial values are 10 coins, fullness 50 and energy 70. Recommendations do not move while the child is selecting. There is one food action, no shop/inventory, no grinding minigame and no music/audio assets in this version. The original sprout creature and test name can be refined after hardware feedback.

## Leaving and saving

The `settings` partition contains namespace `app_pet`. Two 48-byte records (`save_a`, `save_b`) carry a format/version, stable pet ID, sequence, reward-cycle number, partial cycle time, coins, XP, fullness, energy, rest flag and CRC32. Explicit little-endian serialization avoids compiler padding and pointers. The newest valid record is loaded; a broken record falls back to the other. Two invalid existing records cause a safe launch failure, never an automatic reset or erasure.

Meals, rewards, level changes and starting/ending rest write a complete record before confirming the transaction. Passive progress is checkpointed on menu, screen-off and app exit, not every animation frame. Unplugging during ordinary activity may lose progress since the last successful checkpoint; already saved purchases/rewards survive. A failed write freezes further growth/spending and offers an explicit OK retry. Retry does not also purchase food.

A saved nap resumes with its saved energy; switching or rebooting never fills energy instantly. This deliberately replaces the earlier draft's optional "wake rested" shortcut, which would otherwise reward repeated restarts. No wall clock, Wi-Fi or offline wage calculation is needed.

Both screen-off modes pause the pet's economics and ignore invisible purchases. Pause mode retains the shell's first-key-wakes behavior; keep-running mode retains long OK to light the screen and open the menu. The system timeout still applies. To observe a complete 90-second test without touching buttons, set **Settings → Screen timeout** to 120/300 seconds or off. Partial progress survives a normal automatic screen-off.

## Resource and integration design

- An original 240×148, 16-color I4 room costs **17,824 Flash bytes**, including its palette. `pet_art_export.c` regenerates it from palette-colored pixel spans; the static gate compares every byte.
- A single 96×88 I4 sprite surface shared exclusively with Yaya Chat costs **4,288 static RAM bytes**. Faces, leaf ears, book, food, quilt and ball are drawn into it; no animation frame collection is stored. The same drawing code handles both forms and simulated dialogue states.
- LVGL uses its existing 32 KiB pool and the board's 20-row RGB565 display buffer. `LV_BIN_DECODER_RAM_LOAD=0` row-decodes indexed pictures instead of expanding the room into a 142,080-byte ARGB8888 buffer. There is no full-screen application framebuffer.
- The 14-pixel, 2-bpp Source Han Sans SC Medium subset contains ASCII plus only the UI glyphs. Its OFL license is retained. Regenerate with `python tools/generate_pet_font.py /path/to/SourceHanSansSC-Medium.otf`.
- The firmware gate limits pet-owned linked Flash to 128 KiB, shared Avatar code to 48 KiB, and leaves at least **512 KiB** in the existing 3 MiB program partition. These are enforced reservations after adding voice, not a guarantee about future applications. Consult the delivered `capacity-report.json` for actual numbers; shared Avatar bytes count once.
- The optional module `tick(ctx, elapsed_ms, showing)` runs on the control task outside the LVGL lock. It owns game timing and storage; `render_ui` only paints snapshots under the existing lock. No new RTOS task, LVGL timer, driver, Wi-Fi stack or audio service is created by the pet.

User art direction references the detail and palette discipline of Dave the Diver. All shipped art is original pixel construction; no game screenshots, characters, large photographs or third-party game textures are embedded.

The current direction is **independent Yaya Pet and Yaya Chat**, sharing appearance without connecting game state. The earlier pet-data/voice plan is cancelled; existing cloud Qixi configuration is unchanged. See [ownership boundaries](voice-integration.md). The pet has no microphone, network or IDA call; its dialogue fixtures test artwork only.

## Validation and hardware checklist

The static gate runs economy rules, bounds, atomic transaction failure injection, recovery, 384 single-bit corruptions, 100,000 mixed actions/reloads, stale dialogue-event rejection, pixel buffer guards and exact art regeneration. The real LVGL preview adds 2,000 pet/Muyu switches, dark-input and pause/resume scenarios, and nine visual fixtures. The existing 5,000 shell transitions and 2,000 generated-app switches still run. CI builds ESP32-C3 with ESP-IDF 5.5.3 and verifies the protected image and capacity after the second link.

Before accepting the BIN on hardware:

1. Flash the checked full BIN at `0x0`, with whole-chip erase disabled. Confirm Wi-Fi/settings and device identity remain intact.
2. Open Yaya Pet, observe look/read/help/walk, and allow a full active minute. Check one reward, not repeated rewards.
3. Feed once, skip the animation and press again while full. Confirm the price, remaining coins and free-food path.
4. Play and rest; wake once, switch to Yaya Chat and return, then reboot. Check the same pet ID/values, no duplicate rewards or free energy.
5. Reach level 3 across short sessions. Check evolved appearance, level-5 cap and higher earnings.
6. Test both dark modes and system menu. No hidden spending, automatic growth or leftover conversation audio while the pet is open.
7. Check real LCD colors, text clarity, animation pace, button responsiveness, repeated switching, runtime free/minimum heap and extended stability. Report these measurements separately from host previews.

Keep the previously accepted package for recovery. The new image still ends before settings at `0x310000`; protected `cardid@0x356000` and all partitions are unchanged.
