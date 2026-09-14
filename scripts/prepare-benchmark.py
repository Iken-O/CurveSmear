"""Create deterministic raster fixtures for the AE host benchmark, using only the Python stdlib."""
import pathlib
import struct
import zlib

root = pathlib.Path(__file__).resolve().parent.parent
folder = root / 'test-output' / 'ae-benchmark'
folder.mkdir(parents=True, exist_ok=True)
w, h = 1920, 1080

def png(name, pixel):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            raw.extend(pixel(x, y))
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data))
    content = b'\x89PNG\r\n\x1a\n'
    content += chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
    content += chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')
    (folder / name).write_bytes(content)

png('source.png', lambda x, y: (x % 256, y % 256, 64, 255))
png('matte.png', lambda x, y: ((255 if (x // 120) % 2 else 0),) * 3 + (255,))
print(folder)
