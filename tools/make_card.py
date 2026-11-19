"""The Firmware Hub card picture (fmhub-card.png): the PING mark, the name and the main features, beside a
mosaic of the firmware's own screens. Square, 1024 x 1024, a PNG under the hub's 300 KB.
    python tools/make_card.py SHOTS_DIR [OUT.png]     (SHOTS_DIR: the simulator's captures, 240 x 240 .bmp)"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
FONT = ROOT / 'app' / 'assets' / 'fonts' / 'Jost[wght].ttf'
MARK = ROOT / 'app' / 'web' / 'brand' / 'ping-mark.svg'
ORANGE, WHITE, GRAY = (255, 132, 36), (242, 242, 240), (150, 150, 146)
# the screens on the right, at their own size: COLS across, four down (python tools/make_card.py DIR OUT [COLS])
SCREENS = {1: ['tracks_play', 'scope', 'line', 'fx_20'], 2: ['tracks_play', 'scope', 'line', 'step', 'edit_synth', 'fx_20', 'layout_mixer', 'mod']}
# the page's own list (app/web/home.html), each line cut to what fits
ROWS = [('SOUND', '69 models, sampler, granular'), ('EFFECTS', '23, each drawn live'),
        ('STEPS', 'Parameter locks, ratchets, chance'), ('GENERATE', 'Line generator, autopilot'),
        ('KEY', 'Key lock across all tracks'), ('MOVE', '4 modulators a track, 4 macros'),
        ('SONG', '16 patterns, song mode'), ('LIVE', 'Performance mode, stutter'),
        ('SEE', '3D visualizer, theme colour'), ('DAW', 'VST3 plugin, same engine')]

def font(px, wght):
    f = ImageFont.truetype(str(FONT), px)
    f.set_variation_by_axes([wght])
    return f

def mark(size):
    """The PING mark (a square with arcs from its left edge), drawn here as the SVG has it: white lines on black."""
    s = 4; n = size * s
    im = Image.new('RGB', (n, n), 'black'); d = ImageDraw.Draw(im)
    w = max(2, n // 40)
    for r in (0.22, 0.44, 0.66, 0.88, 1.10):
        R = r * n
        d.ellipse((-R, n / 2 - R, R, n / 2 + R), outline=WHITE, width=w)
    d.rectangle((0, 0, n - 1, n - 1), outline=WHITE, width=w)
    return im.resize((size, size), Image.LANCZOS)

def main():
    shots = Path(sys.argv[1]); out = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / 'fmhub-card.png'
    N, T, G = 1024, 240, 4                       # the picture, a tile (the screen as it is), the gap
    im = Image.new('RGB', (N, N), 'black'); d = ImageDraw.Draw(im)
    cols = int(sys.argv[3]) if len(sys.argv) > 3 else 2
    x0, y0 = N - cols * (T + G) - (4 if cols > 1 else 18), (N - 4 * T - 3 * G) // 2
    for k, name in enumerate(SCREENS[cols]):
        t = Image.open(shots / (name + '.bmp')).convert('RGB')
        im.paste(t, (x0 + (k % cols) * (T + G), y0 + (k // cols) * (T + G)))
    cx = x0 // 2
    m = mark(140); im.paste(m, (cx - 70, 44))
    def text(y, s, f, fill):
        d.text((cx, y), s, font=f, fill=fill, anchor='mt')
    text(204, 'PING OS-1', font(88, 700), WHITE)
    text(306, 'RHYTHM SYNTHESIS ENGINE', font(31, 800), ORANGE)
    text(350, 'by PING AUDIO', font(24, 600), GRAY)
    # the list: a label column and a text column, as on the page; the type as large as the longest line allows
    lx, tx, right = (22, 150, x0 - 14) if cols > 1 else (40, 215, x0 - 24)
    px = 40
    while max(d.textlength(t, font=font(px, 600)) for _, t in ROWS) > right - tx: px -= 1
    fl, ft = font(21 if cols > 1 else 27, 700), font(px, 600)
    y, pitch = 404, (1004 - 404) // len(ROWS)
    for lab, t in ROWS:
        d.text((lx, y + pitch // 2), lab, font=fl, fill=ORANGE, anchor='lm')
        d.text((tx, y + pitch // 2), t, font=ft, fill=WHITE, anchor='lm')
        y += pitch
    print('list type', px, 'px')
    im.save(out, optimize=True)
    print(out, out.stat().st_size, 'bytes')

if __name__ == '__main__':
    main()
