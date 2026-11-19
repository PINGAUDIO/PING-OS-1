"""Rasterise the PING Audio mark (its ripples in CMY on black for the FM-1) into RGB565
bitmaps for the FM-1 screen: app/logo.h.

    python tools/make_logo.py [SVG]

Standard library only: a small SVG path parser (M L H V C S Z, absolute and
relative), cubic flattening and a nonzero-winding scanline fill with 4x4
supersampling. The default source is the PING mark from PING Step."""
import math, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(ROOT), 'breakmorph', 'betterpingyel.svg')

svg = open(SRC, encoding='utf-8').read()
vb = [float(v) for v in re.search(r'viewBox="([^"]+)"', svg).group(1).split()]
d = re.search(r'<path[^>]*\sd="([^"]+)"', svg).group(1)
# PING FM-1: the mark's geometry, each ring of it one process colour, on black.
# The path is the frame with the rings cut out of it: subpath 0 is the frame,
# 1..5 the rings from the outside in.
BG = (245, 245, 245)               # the frame and the lines between the rings
CYAN, MAGENTA, YELL, BLACK, WHITE = (0, 174, 239), (236, 0, 140), (255, 236, 0), (0, 0, 0), (245, 245, 245)
RING_INK = {1: BLACK, 2: BLACK, 3: BLACK, 4: BLACK, 5: BLACK}     # white lines on black: swap colours here


def parse(d):
    toks = re.findall(r'[MmLlHhVvCcSsZz]|-?(?:\d+\.?\d*|\.\d+)(?:e-?\d+)?', d)
    i, cmd = 0, None
    x = y = sx = sy = 0.0
    lc = None                       # last cubic control point
    polys, cur = [], []

    def num():
        nonlocal i
        v = float(toks[i]); i += 1; return v

    def cubic(p0, p1, p2, p3):
        for k in range(1, 13):
            t = k / 12; u = 1 - t
            cur.append((u**3 * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t**3 * p3[0],
                        u**3 * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t**3 * p3[1]))
    while i < len(toks):
        if re.match(r'[A-Za-z]', toks[i]):
            cmd = toks[i]; i += 1
            if cmd in 'Zz':
                if cur: polys.append(cur)
                cur = []; x, y = sx, sy; lc = None
                continue
        rel = cmd.islower(); c = cmd.upper()
        if c == 'M':
            if cur: polys.append(cur)
            nx, ny = num(), num()
            if rel: nx += x; ny += y
            x, y = sx, sy = nx, ny; cur = [(x, y)]; cmd = 'l' if rel else 'L'; lc = None
        elif c == 'L':
            nx, ny = num(), num()
            if rel: nx += x; ny += y
            x, y = nx, ny; cur.append((x, y)); lc = None
        elif c == 'H':
            nx = num(); x = nx + x if rel else nx; cur.append((x, y)); lc = None
        elif c == 'V':
            ny = num(); y = ny + y if rel else ny; cur.append((x, y)); lc = None
        elif c in 'CS':
            if c == 'C':
                x1, y1 = num(), num()
                if rel: x1 += x; y1 += y
            else:
                x1, y1 = (2 * x - lc[0], 2 * y - lc[1]) if lc else (x, y)
            x2, y2, nx, ny = num(), num(), num(), num()
            if rel: x2 += x; y2 += y; nx += x; ny += y
            cubic((x, y), (x1, y1), (x2, y2), (nx, ny))
            lc = (x2, y2); x, y = nx, ny
    if cur: polys.append(cur)
    return polys


def coverage(poly, size, ss=4):
    """0..1 per pixel inside one closed subpath"""
    edges = [(poly[k], poly[(k + 1) % len(poly)]) for k in range(len(poly))]
    edges = [(a, b) for a, b in edges if a[1] != b[1]]
    scale = size * ss / vb[2]
    cov = [[0.0] * size for _ in range(size)]
    for sy_ in range(size * ss):
        yy = vb[1] + (sy_ + 0.5) / scale
        xs = sorted(a[0] + (yy - a[1]) / (b[1] - a[1]) * (b[0] - a[0]) for a, b in edges
                    if (a[1] <= yy < b[1]) or (b[1] <= yy < a[1]))
        for j in range(0, len(xs) - 1, 2):
            x0 = (xs[j] - vb[0]) * scale; x1 = (xs[j + 1] - vb[0]) * scale
            for sx_ in range(max(0, int(math.ceil(x0 - 0.5))), min(size * ss, int(math.ceil(x1 - 0.5)))):
                cov[sy_ // ss][sx_ // ss] += 1.0 / (ss * ss)
    return cov


def raster(polys, size):
    img = [[list(BG) for _ in range(size)] for _ in range(size)]
    for k, ink in RING_INK.items():
        cov = coverage(polys[k], size)
        for j in range(size):
            for i in range(size):
                a = min(1.0, cov[j][i])
                if a > 0:
                    for c in range(3): img[j][i][c] = img[j][i][c] * (1 - a) + ink[c] * a
    return img


def rgb565(c):
    r, g, b = (int(round(v)) for v in c)
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def emit(name, size, polys):
    cov = raster(polys, size)
    px = []
    for row in cov:
        for c in row:
            px.append(rgb565(tuple(min(255, v) for v in c[:3])))
    lines = [f'static const uint16_t {name}[{size} * {size}] = {{']
    for k in range(0, len(px), 16):
        lines.append('    ' + ', '.join(f'0x{v:04X}' for v in px[k:k + 16]) + ',')
    lines.append('};')
    return '\n'.join(lines)


polys = parse(d)
out = os.path.join(ROOT, 'app', 'logo.h')
with open(out, 'w', encoding='utf-8') as f:
    f.write('/* generated by tools/make_logo.py from the PING Audio mark: RGB565, row-major */\n')
    f.write('#define LOGO_SMALL 18\n')   # the boot screen draws the mark live (app/ping_logo.h)
    f.write(emit('logo_small', 18, polys) + '\n')
# a preview, to check by eye (PPM)
cov = raster(polys, 96)
with open(os.path.join(ROOT, 'dev', 'out', 'logo_preview.ppm'), 'wb') as f:
    f.write(b'P6 96 96 255\n')
    for row in cov:
        for c in row:
            f.write(bytes(int(min(255, v)) for v in c[:3]))
print('wrote', out)
