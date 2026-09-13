"""Use verified firmware values in the host storage screenshot."""
import json
from pathlib import Path
import sys

report=json.loads(Path(sys.argv[1]).read_text())
assert report['program_limit']==0x300000
assert report['program_free']==report['program_limit']-report['image_bytes']
text=('Flash total: 8192 KiB\nAllocated: 3176 KiB\nUnassigned: 5016 KiB\n\n'
      f"Program: {report['image_bytes']//1024} / 3072 KiB\n"
      f"Can grow: {report['program_free']//1024} KiB\n"
      f"System/shared: {report['shared_and_image_overhead_bytes']//1024} KiB\n\n"
      'Unassigned is not an\napp install partition.')
Path(sys.argv[2]).write_text('#define PP_PREVIEW_CAPACITY '+json.dumps(text)+'\n')
