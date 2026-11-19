"""A picture for sharing PING OS-1 (a post, a thread): the logo and the feature list, in a border of the firmware's
screens at their own size. 1680 x 1680.
    python tools/make_share.py SHOTS_DIR LOGO.png OUT.png
SHOTS_DIR: the simulator's captures (240 x 240 .bmp). LOGO.png: the logo drawn white and orange on black, about 600
wide (app/web/brand/ping-os-1-logo.svg through a browser, as for the hub card)."""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
FONT = ROOT / 'app' / 'assets' / 'fonts' / 'Jost[wght].ttf'
ORANGE, WHITE, GRAY = (255, 132, 36), (242, 242, 240), (150, 150, 146)
# the border, clockwise from the top left: 7 across the top, 5 down the right, 7 back along the bottom, 5 up the left
BORDER = ['tracks_play', 'scope', 'line', 'step', 'edit_synth', 'layout_mixer', 'mod',
          'fx_20', 'envelope', 'perform', 'fx_10', 'macros',
          'library', 'fx_04', 'glo_autopilot', 'edit_kit', 'fx_09', 'key_tempo', 'song',
          'step_synth', 'fx_14', 'master', 'step_lock', 'browser']
ROWS = [('SOUND', '49 drum and 20 synth models, plus a supplemental sampler'),
        ('EFFECTS', '23 effects, each drawn live. Up to 20 at once'),
        ('STEPS', 'Parameter locks, ratchets, chance, ties, micro timing'),
        ('GENERATE', 'One-click algorithmic line generator'),
        ('AUTOPILOT', 'Keeps varying your sounds and patterns by itself'),
        ('KEY', 'Key lock: one key and scale holds every track in tune'),
        ('MOVE', 'Four modulators a track, four macros'),
        ('SONG', '16 patterns, chained in song mode'),
        ('LIVE', 'Performance mode with stutter'),
        ('SEE', '3D visualizer, custom theme colour'),
        ('DAW', 'VST3 plugin suite, and it plays in the browser')]
FOOT = 'Free and open source for the M-VAVE FM-1'
# the labels run through PING's colours, top to bottom: red, orange, yellow, pale, teal
STOPS = [(255, 92, 48), (240, 122, 0), (224, 200, 0), (223, 224, 58), (228, 228, 160), (108, 196, 168), (47, 176, 156)]
def ramp(t):
    x = t * (len(STOPS) - 1); i = min(int(x), len(STOPS) - 2); f = x - i
    return tuple(round(STOPS[i][k] + (STOPS[i + 1][k] - STOPS[i][k]) * f) for k in range(3))

def font(px, wght):
    f = ImageFont.truetype(str(FONT), px)
    f.set_variation_by_axes([wght])
    return f

def main():
    shots, logo, out = Path(sys.argv[1]), Image.open(sys.argv[2]).convert('RGB'), Path(sys.argv[3])
    T, K = 240, 7
    N = T * K
    im = Image.new('RGB', (N, N), 'black'); d = ImageDraw.Draw(im)
    cells = [(c, 0) for c in range(K)] + [(K - 1, r) for r in range(1, K - 1)] + [(c, K - 1) for c in range(K - 1, -1, -1)] + [(0, r) for r in range(K - 2, 0, -1)]
    assert len(cells) == len(BORDER), (len(cells), len(BORDER))
    for (c, r), name in zip(cells, BORDER):
        im.paste(Image.open(shots / (name + '.bmp')).convert('RGB'), (c * T, r * T))
    d.rectangle((T, T, N - T - 1, N - T - 1), fill='black')
    for k in range(1, K):                                   # thin black lines between the screens
        d.line((k * T, 0, k * T, N), fill='black', width=4); d.line((0, k * T, N, k * T), fill='black', width=4)
    d.rectangle((T + 2, T + 2, N - T - 3, N - T - 3), fill='black')
    cx = N // 2
    y = T + 46
    im.paste(logo, (cx - logo.width // 2, y)); y += logo.height + 16
    d.text((cx, y), 'by PING AUDIO', font=font(30, 600), fill=GRAY, anchor='mt'); y += 74
    lx, tx, right = T + 70, T + 290, N - T - 60
    px = 40
    while max(d.textlength(t, font=font(px, 500)) for _, t in ROWS) > right - tx: px -= 1
    fl, ft = font(28, 700), font(px, 500)
    bottom = N - T - 84
    pitch = (bottom - y) // len(ROWS)
    for k, (lab, t) in enumerate(ROWS):
        d.text((lx, y + pitch // 2), lab, font=fl, fill=ramp(k / (len(ROWS) - 1)), anchor='lm')
        d.text((tx, y + pitch // 2), t, font=ft, fill=WHITE, anchor='lm')
        y += pitch
    d.text((cx, N - T - 52), FOOT, font=font(28, 600), fill=GRAY, anchor='mt')
    im.save(out, optimize=True)
    print(out, im.size, out.stat().st_size, 'bytes; list type', px, 'px')

if __name__ == '__main__':
    main()
