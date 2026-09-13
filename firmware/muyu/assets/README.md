<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, and sound effects, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

Store reusable font files and generated font sources in `fonts/`.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

## Images

The original pet room is `images/pp_pet_room_i4.c`: 240 by 148 pixels, 16-color I4,
17,824 bytes including palette, linked by `pp_app_pet`. Its editable source is
`components/pp_avatar/pp_pixel_art.c` (repository MIT license); regenerate with
`tools/preview/pet_art_export.c`. No third-party game artwork is used.
The pet's `main/font_pet_14.c` is an ASCII/UI subset of Source Han Sans SC Medium,
14 pixels and 2 bpp, under [OFL](fonts/SourceHanSans-OFL.txt). Regeneration uses
`tools/generate_pet_font.py` and an external full OTF; no full font is shipped.

Store reusable source images and generated display assets in `images/`.

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

## Music and sound effects

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.
