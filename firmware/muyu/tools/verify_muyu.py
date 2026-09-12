"""MVP-specific delivery checks in addition to the upstream layout verifier."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

folder = Path(sys.argv[1] if len(sys.argv) > 1 else 'build').resolve()
image = folder / 'FoloToy-AI-Passport-full.bin'
data = image.read_bytes()
assert 0x10000 < len(data) <= 0x310000, 'Merged image must end before persistent settings and protected cardid'
config = (folder / 'sdkconfig').read_text(encoding='utf-8')
assert 'CONFIG_IDF_TARGET="esp32c3"' in config
assert 'CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y' in config
assert 'CONFIG_BT_ENABLED=y' in config
assert 'CONFIG_BT_NIMBLE_ENABLED=y' in config
info = {
    'application': 'passport-platform', 'version': '0.2.0',
    'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
    'target': 'esp32c3', 'esp_idf': '5.5.3', 'flash_size_bytes': 8 * 1024 * 1024,
    'merged_file': image.name, 'merged_offset': '0x0', 'merged_bytes': len(data),
    'merged_sha256': hashlib.sha256(data).hexdigest(),
    'protected_cardid_offset': '0x356000', 'settings_offset': '0x310000', 'device_tests': 'NOT RUN'
}
(folder / 'build-info.json').write_text(json.dumps(info, indent=2) + '\n', encoding='utf-8')
print(json.dumps(info, indent=2))
