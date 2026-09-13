"""Fail if generated pixel art drifts or small-asset limits regress."""
from pathlib import Path
import re
import sys
root=Path(__file__).resolve().parent.parent
asset=root/'assets/images/pp_pet_room_i4.c'
def pixels(p): return bytes(int(x,16) for x in re.findall(r'0x([0-9a-fA-F]{2})',p.read_text(encoding='utf-8-sig')))
data=pixels(asset)
assert len(data)==17824 and data==pixels(Path(sys.argv[1]))
assert set(data[3:64:4])=={0,255}
font=(root/'main/font_pet_14.c').read_text(encoding='utf-8')
assert 'Bpp: 2' in font and 'Size: 14 px' in font
assert len(font.encode())<150000, 'Inspect font growth before adding new glyphs'
assert '#define LV_BIN_DECODER_RAM_LOAD 0' in (root/'tools/preview/lv_conf.h').read_text()
assert '# CONFIG_LV_BIN_DECODER_RAM_LOAD is not set' in (root/'sdkconfig.defaults').read_text(encoding='utf-8')
print('Pet assets: PASS (17824-byte I4 room, reproducible original art, small font, row decode)')
