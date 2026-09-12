[简体中文](platform-acceptance.zh_CN.md) | English

# Board acceptance

Device tests: NOT RUN. The owner flashes the reviewed package and records actual results.

1. Flash the merged image at 0x0 without erase-all; first boot shows applications, with no reboot loop.
2. Open Muyu; tap UP/DOWN and short OK. Hold OK: menu opens with no extra count or sound. Try rapid double presses and repeated holds.
3. Open Device status, then return to Muyu. Verify count is retained across switches and old sound stops. Repeat at least 100 switches; record minimum heap in Diagnostics.
4. Change volume, brightness and screen timeout. Confirm both apps share them, wait two seconds, reboot, and verify persistence. Wait ten seconds in an app, reboot and confirm that app opens.
5. Use phone setup on a 2.4 GHz network. Check IP, switch apps and confirm the connection remains. Disconnect the router, restore it, and verify reconnect without freezing the menu. Try a wrong password and then correct it.
6. Check nearby scan; open setup twice, cancel it, and verify five-minute expiry. Never copy the shown AP password or Wi-Fi credentials into public logs.
7. Toggle BLE beacon at least ten times; scan with a phone BLE scanner for Passport. Test together with Wi-Fi and Muyu; record failures/minimum heap. This is not a pairing test.
8. Let the screen turn off; the first gesture wakes without selecting/striking. Verify backlight never becomes permanently inaccessible.
9. Power-cycle twice within the ten-second startup window to exercise safe menu recovery. Return to an app, wait ten seconds and verify normal recovery. This intentionally tests interrupted starts, not a crash injection.
10. On a later update using another checked full image ending before settings, verify settings and last app survive. Do not claim persistence across firmware flashing until measured.
11. Record display, sound, button response, connection stability, actual free/minimum heap and any serial errors. Hardware timing, microphone sessions, power consumption and endurance are not covered by host tests.
