"""MVP-specific delivery checks in addition to the upstream layout verifier."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import re
from verify_firmware import parse_partition_table, Partition

folder = Path(sys.argv[1] if len(sys.argv) > 1 else 'build').resolve()
image = folder / 'FoloToy-AI-Passport-full.bin'
data = image.read_bytes()
assert 0x10000 < len(data) <= 0x310000, 'Merged image must end before persistent settings and protected cardid'
partitions, md5_ok = parse_partition_table(data[0x8000:0x8C00])
assert md5_ok
assert Partition(1, 2, 0x310000, 0x6000, 'settings') in partitions
config = (folder / 'sdkconfig').read_text(encoding='utf-8')
assert 'CONFIG_IDF_TARGET="esp32c3"' in config
assert 'CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y' in config
assert '# CONFIG_BT_ENABLED is not set' in config, 'Bluetooth must stay disabled in this firmware'
for option in ('CONFIG_BT_ENABLED', 'CONFIG_BT_NIMBLE_ENABLED', 'CONFIG_ESP_COEX_SW_COEXIST_ENABLE'):
    assert option+'=y' not in config, 'Unexpected Bluetooth/coexistence runtime: '+option
root=Path(__file__).resolve().parent.parent
version=re.search(r'#define PP_VERSION "([^"]+)"',(root/'main/passport_core.h').read_text()).group(1)
assert 'set(PROJECT_VER "'+version+'")' in (root/'CMakeLists.txt').read_text()
assert 'CONFIG_LV_BIN_DECODER_RAM_LOAD=y' not in config
# These options are part of the C3 voice RAM contract. Check the generated
# sdkconfig, so a dependency/default change cannot silently drop the fix.
for option in ('CONFIG_MBEDTLS_DYNAMIC_BUFFER', 'CONFIG_MBEDTLS_DYNAMIC_FREE_CONFIG_DATA'):
    assert option+'=y' in config, 'Missing voice memory setting: '+option
for option in ('CONFIG_ESP_WIFI_IRAM_OPT', 'CONFIG_ESP_WIFI_RX_IRAM_OPT',
               'CONFIG_MBEDTLS_SSL_KEEP_PEER_CERTIFICATE', 'CONFIG_MBEDTLS_SSL_RENEGOTIATION'):
    assert option+'=y' not in config, 'Unexpected resident voice RAM cost: '+option
assert 'CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN=16384' in config, 'Preserve standard TLS receive record size'
info = {
    'application': 'passport-platform', 'version': version,
    'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
    'target': 'esp32c3', 'esp_idf': '5.5.3', 'flash_size_bytes': 8 * 1024 * 1024,
    'merged_file': image.name, 'merged_offset': '0x0', 'merged_bytes': len(data),
    'merged_sha256': hashlib.sha256(data).hexdigest(),
    'protected_cardid_offset': '0x356000', 'settings_offset': '0x310000', 'device_tests': 'NOT RUN'
}
(folder / 'build-info.json').write_text(json.dumps(info, indent=2) + '\n', encoding='utf-8')
print(json.dumps(info, indent=2))
