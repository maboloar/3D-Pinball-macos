"""Draws an original app icon for the public release build: a glossy steel-blue pinball with soft studio highlights on a
transparent background. It is rendered procedurally (lighting model + highlights) and uses no game artwork.
usage: python3 -I make_release_icon.py <out.png>   (writes a 512x512 RGBA PNG)"""
import math
import struct
import sys
import zlib

N = 512
SS = 3                      # supersampling per axis for smooth edges
CX = CY = N / 2.0
R = 232.0                   # sphere radius


def png(w, h, rgba):
    raw = b''.join(b'\0' + bytes(rgba[y * w * 4:(y + 1) * w * 4]) for y in range(h))

    def ch(t, x):
        c = struct.pack(">I", len(x)) + t + x
        return c + struct.pack(">I", zlib.crc32(t + x))

    return (b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
            + ch(b'IDAT', zlib.compress(raw, 9)) + ch(b'IEND', b''))


def clamp(v, lo=0.0, hi=1.0):
    return max(lo, min(hi, v))


def shade(x, y):
    """Colour (r,g,b) in 0..1 of the sphere at pixel (x,y), or None outside the sphere."""
    nx, ny = (x - CX) / R, (y - CY) / R
    d2 = nx * nx + ny * ny
    if d2 >= 1.0:
        return None
    nz = math.sqrt(1.0 - d2)
    # key light from the upper left, soft fill from the lower right
    lx, ly, lz = -0.45, -0.55, 0.70
    ln = math.sqrt(lx * lx + ly * ly + lz * lz)
    diff = clamp((nx * lx + ny * ly + nz * lz) / ln)
    fill = clamp((nx * 0.6 + ny * 0.5 + nz * 0.62) / 1.0)
    base = (0.10 + 0.52 * diff ** 0.9 + 0.10 * fill)
    r, g, b = base * 0.52, base * 0.62, base * 0.98          # steel blue
    # darker rim (like glass/chrome edge) with a faint cool rim light on the lit side
    rim = clamp((math.sqrt(d2) - 0.78) / 0.22)
    r, g, b = r * (1 - 0.55 * rim), g * (1 - 0.55 * rim), b * (1 - 0.40 * rim)
    # reflected "sky" band across the lower half
    band = math.exp(-(((ny - 0.50) / 0.13) ** 2)) * clamp(1.0 - abs(nx) * 0.9)
    r, g, b = r + 0.22 * band, g + 0.28 * band, b + 0.34 * band
    # large soft highlight and a smaller, brighter one (studio window reflections)
    for hx, hy, ax, ay, strength in ((-0.36, -0.40, 0.30, 0.20, 0.95), (-0.18, -0.12, 0.12, 0.08, 0.80)):
        e = ((nx - hx) / ax) ** 2 + ((ny - hy) / ay) ** 2
        h = math.exp(-e * 1.4) * strength
        r, g, b = r + h, g + h, b + h
    # small specular glint on the lower right edge
    e = ((nx - 0.55) / 0.10) ** 2 + ((ny - 0.42) / 0.05) ** 2
    s = math.exp(-e) * 0.55
    r, g, b = r + s, g + s, b + s
    return clamp(r), clamp(g), clamp(b)


buf = bytearray()
for py in range(N):
    for px in range(N):
        acc_r = acc_g = acc_b = 0.0
        cov = 0
        for sy in range(SS):
            for sx in range(SS):
                c = shade(px + (sx + 0.5) / SS, py + (sy + 0.5) / SS)
                if c:
                    acc_r += c[0]
                    acc_g += c[1]
                    acc_b += c[2]
                    cov += 1
        if cov == 0:
            buf += b'\0\0\0\0'
        else:
            buf += bytes((int(acc_r / cov * 255), int(acc_g / cov * 255), int(acc_b / cov * 255),
                          int(255 * cov / (SS * SS))))
open(sys.argv[1], "wb").write(png(N, N, buf))
print("wrote", sys.argv[1])
