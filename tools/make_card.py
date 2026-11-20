"""The Firmware Hub card picture (fmhub-card.png): the PING mark, the name and the main features, beside a
mosaic of the firmware's own screens. Square, 1024 x 1024, a PNG under the hub's 300 KB.
    python tools/make_card.py SHOTS_DIR [OUT.png]     (SHOTS_DIR: the simulator's captures, 240 x 240 .bmp)"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
FONT = ROOT / 'app' / 'assets' / 'fonts' / 'Jost[wght].ttf'
ORANGE, WHITE, GRAY = (255, 132, 36), (242, 242, 240), (150, 150, 146)
# the screens on the right, at their own size: COLS across, four down (python tools/make_card.py DIR OUT [COLS])
SCREENS = {1: ['tracks_play', 'scope', 'line', 'fx_20'], 2: ['tracks_play', 'scope', 'line', 'step', 'edit_synth', 'fx_20', 'layout_mixer', 'mod']}
# the features, a few words each so they read at the hub's card size; each behind a square in PING's colours
ROWS = ['49 synthesized drums', '20 synth models', '23 modular FX', 'Parameter locks', 'Line generator', 'Autopilot', 'Key lock',
        'Performance mode', '3D visualizer', 'VST3 plugin']
STOPS = [(255, 92, 48), (240, 122, 0), (224, 200, 0), (223, 224, 58), (228, 228, 160), (108, 196, 168), (47, 176, 156)]
def ramp(t):
    x = t * (len(STOPS) - 1); i = min(int(x), len(STOPS) - 2); f = x - i
    return tuple(round(STOPS[i][k] + (STOPS[i + 1][k] - STOPS[i][k]) * f) for k in range(3))

def font(px, wght):
    f = ImageFont.truetype(str(FONT), px)
    f.set_variation_by_axes([wght])
    return f

def main():
    shots = Path(sys.argv[1]); out = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / 'fmhub-card.png'
    N, T, G = 1024, 240, 4                       # the picture, a tile (the screen as it is), the gap
    im = Image.new('RGB', (N, N), 'black'); d = ImageDraw.Draw(im)
    cols = int(sys.argv[3]) if len(sys.argv) > 3 else 2
    x0, y0 = N - cols * (T + G) - 22, (N - 4 * T - 3 * G) // 2   # (in from the frame)
    for k, name in enumerate(SCREENS[cols]):
        t = Image.open(shots / (name + '.bmp')).convert('RGB')
        im.paste(t, (x0 + (k % cols) * (T + G), y0 + (k // cols) * (T + G)))
    cx = x0 // 2
    # the logo with its name and tagline: PING AUDIO's own drawing (app/web/brand/ping-os-1-logo.svg, Futura), rendered
    # white and orange on black as ping-os-1-logo-dark.png (a browser draws it: the type is not in this repository)
    logo = Image.open(ROOT / 'app' / 'web' / 'brand' / 'ping-os-1-logo-dark.png').convert('RGB')
    wmax = x0 - 2 * 40                                   # (clear of the frame and of the screens)
    if logo.width > wmax: logo = logo.resize((wmax, round(logo.height * wmax / logo.width)), Image.LANCZOS)
    im.paste(logo, (cx - logo.width // 2, 40))
    def text(y, s, f, fill):
        d.text((cx, y), s, font=f, fill=fill, anchor='mt')
    # the list: a coloured square, then the feature, set left and as large as the room allows
    px = 46
    y0, y1 = 36 + logo.height + 34, 1006
    pitch = (y1 - y0) // len(ROWS)
    while max(d.textlength(t, font=font(px, 600)) for t in ROWS) + 46 > x0 - 40 - 24: px -= 1   # (clear of the screens)
    ft = font(px, 600)
    width = max(d.textlength(t, font=ft) for t in ROWS) + 46
    lx = max(40, cx - int(width / 2))
    y = y0
    for k, t in enumerate(ROWS):
        ym = y + pitch // 2
        d.rectangle((lx, ym - 11, lx + 22, ym + 11), fill=ramp(k / (len(ROWS) - 1)))
        d.text((lx + 46, ym), t, font=ft, fill=WHITE, anchor='lm')
        y += pitch
    d.rectangle((10, 10, N - 11, N - 11), outline=WHITE, width=3)   # the frame: a thin white line, a little in from the edge
    im.save(out, optimize=True)
    print(out, out.stat().st_size, 'bytes')

if __name__ == '__main__':
    main()
