"""Encode the actual LVGL framebuffer and C-generated PCM; no mock UI."""
from pathlib import Path
import struct
import wave
import zlib
import argparse

parser=argparse.ArgumentParser()
parser.add_argument('--images-only',action='store_true')
args=parser.parse_args()


def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))


for raw in list(Path('.').glob('muyu-*.rgb')) + list(Path('.').glob('passport-*.rgb')):
    pixels = raw.read_bytes()
    assert len(pixels) == 240 * 320 * 3
    rows = b''.join(b'\0' + pixels[y * 720:(y + 1) * 720] for y in range(320))
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 240, 320, 8, 2, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b'')
    raw.with_suffix('.png').write_bytes(png)
if not args.images_only:
    with wave.open('muyu-knock.wav', 'wb') as output:
        output.setparams((1, 2, 16000, 0, 'NONE', 'not compressed'))
        output.writeframes(Path('muyu-knock.pcm').read_bytes())
