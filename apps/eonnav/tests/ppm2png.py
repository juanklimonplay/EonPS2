"""Convert EonNav's binary PPM screenshot to PNG (standard library only).

Fails if the image is missing or a single flat colour, i.e. nothing was drawn.
"""
import struct
import sys
import zlib

src, dst = sys.argv[1], sys.argv[2]
with open(src, "rb") as f:
    data = f.read()
parts = data.split(b"\n", 3)
if parts[0] != b"P6":
    sys.exit("not a P6 PPM")
w, h = map(int, parts[1].split())
pixels = parts[3]
if len(pixels) != w * h * 3:
    sys.exit("truncated PPM")
if len(set(pixels[i:i + 3] for i in range(0, len(pixels), 3 * 97))) < 3:
    sys.exit("screenshot is blank")
raw = b"".join(b"\0" + pixels[y * w * 3:(y + 1) * w * 3] for y in range(h))


def chunk(tag, body):
    return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF)


with open(dst, "wb") as f:
    f.write(b"\x89PNG\r\n\x1a\n")
    f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)))
    f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
    f.write(chunk(b"IEND", b""))
