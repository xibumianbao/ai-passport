#!/usr/bin/env python3
"""Build a reproducible Skill ZIP from an explicit source allowlist."""
import hashlib
import json
from pathlib import Path
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SKILL = ROOT / 'skills' / 'ida-agent-client'
FILES = (
    'SKILL.md', 'agents/openai.yaml', 'references/api.md', 'references/usage.md',
    'scripts/ida.py', 'scripts/ida_core.py', 'scripts/ida_credentials.py',
)


def main():
    sys.path.insert(0, str(SKILL / 'scripts'))
    from ida_core import VERSION
    target = ROOT / 'dist' / f'ida-agent-client-{VERSION}.zip'
    target.parent.mkdir(parents=True, exist_ok=True)
    sources = {name: (SKILL / name).read_bytes() for name in FILES}
    for name, content in sources.items():
        content.decode('utf-8-sig')  # Fail on broken source encoding before distributing.
        if (SKILL / name).is_symlink():
            raise SystemExit('Symlink is not allowed in the release allowlist.')
    with zipfile.ZipFile(target, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for name in sorted(sources):
            info = zipfile.ZipInfo('ida-agent-client/' + name)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, sources[name])
    with zipfile.ZipFile(target) as archive:
        assert archive.testzip() is None
        assert set(archive.namelist()) == {'ida-agent-client/' + name for name in FILES}
        for name, content in sources.items():
            assert archive.read('ida-agent-client/' + name) == content
    digest = hashlib.sha256(target.read_bytes()).hexdigest()
    target.with_suffix('.zip.sha256').write_text(digest + '  ' + target.name + '\n', encoding='utf-8')
    print(json.dumps({'package': str(target), 'sha256': digest, 'files': len(FILES)}))


if __name__ == '__main__':
    main()
