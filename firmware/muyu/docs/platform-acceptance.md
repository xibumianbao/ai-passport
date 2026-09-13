[简体中文](platform-acceptance.zh_CN.md) | English

# Board acceptance

New 0.3.0 device tests: NOT RUN. The owner's earlier 0.2.0 functional feedback does not validate this new image. Flash the reviewed package and record actual results.

1. Flash the merged image at 0x0 without erase-all; first boot shows applications, with no reboot loop.
2. Open Muyu; tap UP/DOWN and short OK. Hold OK: menu opens with no extra count or sound. Try rapid double presses and repeated holds.
3. Applications lists only custom apps. Open Settings → Device status, then return to Muyu; count remains and old audio stopped on menu entry. Repeat menu/resume 100 times. With a second catalog app, test switching both ways and record minimum heap.
4. Change volume, brightness, screen timeout and screen mode. Wait two seconds, reboot and verify persistence. Wait ten seconds in an app before testing last-app restore. Upgrade from 0.2.0 without full erase and verify its saved network/settings survive; a saved Device status ID falls back to Applications.
5. Find a 2.4 GHz network and type the password entirely on-device. Test lower/upper case, numbers, symbols, spaces, DEL, GO and cancel; no phone/AP should be required. Test lengths 7, 8, 63, and overflow rejection. Wrong credentials must not replace the previous saved network. Verify the 25-second deadline, editable retry, cancellation and open-network flow.
6. Scan while online and while reconnecting; test 12 results, duplicate SSIDs, empty results, manual/hidden name and unsupported authentication. Confirm app/menu changes retain the system connection. Disconnect/restore the router and verify responsive automatic recovery. Do not publish credentials or private SSIDs in logs/screenshots.
7. Toggle BLE beacon at least ten times; scan with a phone BLE scanner for Passport. Test together with Wi-Fi and Muyu; record failures/minimum heap. This is not a pairing test.
8. Pause mode: first complete gesture only wakes. Running mode: timeout and Screen off (run) keep Muyu key/audio actions running with no backlight; long OK lights the screen and opens the menu without a short action. Check menu timeout always pauses. For a future voice app separately verify recording, upload and playback while dark; measure current draw before claiming savings.
9. Power-cycle twice within the ten-second startup window to exercise safe menu recovery. Return to an app, wait ten seconds and verify normal recovery. This intentionally tests interrupted starts, not a crash injection.
10. On a later update using another checked full image ending before settings, verify settings and last app survive. Do not claim persistence across firmware flashing until measured.
11. Settings → Storage: compare program/app values against capacity-report.json, verify shared code is counted once and unassigned Flash is distinct from program headroom. App NVS entry usage and settings free entries must reflect actual saves. Record display, sound, buttons, connection stability, free/minimum heap and serial errors; hardware timing, microphone sessions, power and endurance require device measurements.
