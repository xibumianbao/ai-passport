[简体中文](platform-acceptance.zh_CN.md) · **English**

# Board acceptance: 0.6.0

New 0.6.0 Device tests: **NOT RUN**. Earlier voice/pet feedback does not validate this combined image. Record the tested BIN hash, source version and actual results separately from build and host checks.

1. Flash the verified merged image at `0x0` without whole-chip erase. Confirm settings and device identity survive, startup is responsive, and the last stable app returns after reboot.
2. Applications contains Yaya Pet and Yaya Chat. Storage, About, device/runtime/capacity pages and Bluetooth are absent. Check the Wi-Fi/battery top bar and full 240×290 app area without the old footer.
3. Verify pet feed/play/rest, normal rewards and saved evolution. Switch to chat and return: chatting must not change the pet ID, coins, XP, energy or save. Check the shared sprite appears correctly after repeated switches.
4. In chat, press OK once when ready; speak, pause, hear the reply and continue without another send press. Test short OK stopping, Stopped restart, errors/retry, activation digits and long OK during capture/playback. See [voice acceptance boundaries](xiaozhi-standalone.md).
5. Change volume, brightness, timeout and screen-off mode; wait two seconds and reboot to verify persistence. Existing saved Wi-Fi and pet records must survive the upgrade. A former BLE preference must not start a beacon.
6. Join a 2.4 GHz network using only the on-device keyboard. Test case, digits, symbols, spaces, DEL, GO, cancel, open networks and 7/8/63-character bounds. Wrong credentials must preserve the prior network; check the 25-second deadline and editable retry.
7. Scan while online/reconnecting, including duplicate SSIDs, 12 results, no results and manual/hidden names. Switch apps without reprovisioning. Disconnect/restore the router and check responsive recovery; keep private network details out of shared logs.
8. Paused screen-off: the first complete gesture only wakes. Running screen-off: intentional chat audio continues with no backlight; long OK lights the screen and opens the menu. Both modes must pause pet economics and invisible purchases. Menu timeout always pauses.
9. Repeat menu/resume and two-way switching at least 100 times. Verify no residual voice plays over the pet. Record same-stage free heap, largest block and stack minimum, first error, and a longer conversation. A hidden screen or successful build does not prove power savings or stable device timing.
10. Exercise the existing startup-interruption recovery and perform a later verified update without erase-all. Recheck identity, settings and saves. Retain the previously accepted package until these checks pass.

Developers compare the final `capacity-report.json` and linked build reports outside the device. No device capacity screen is expected. Current firmware disables Bluetooth/NimBLE; restoring it later requires a new build and acceptance plan.
