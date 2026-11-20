"""A picture for sharing PING OS-1 (a post, a thread): the logo and the feature list, in a border of the firmware's
screens at their own size. 1680 x 1680.
    python tools/make_share.py SHOTS_DIR LOGO.png OUT.png [para|plain]     (para: one paragraph; plain: the list without labels)
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
ROWS = [('SOUND', '49 synthesized drums, 20 synths, granular and one-shot sampler'),
        ('EFFECTS', '23 modular FX. Up to 20 at once'),
        ('STEPS', 'Parameter locks, ratchets, chance, ties, micro timing'),
        ('GENERATE', 'One-click algorithmic line generator'),
        ('AUTOPILOT', 'Autopilot mode for hands-free noodling'),
        ('KEY', 'Key lock: one key and scale holds every track in tune'),
        ('MOVE', 'Four modulators a track, four macros'),
        ('SONG', '16 patterns, chained in song mode'),
        ('LIVE', 'Performance mode with stutter and snapback'),
        ('SEE', '3D visualizer, custom theme colors'),
        ('DAW', 'VST3 plugin suite, and it plays in the browser')]
FOOT = ('Available NOW at The Firmware Hub', 'thefirmwarehub.com')   # (what it says, then where)
# without the labels (the picture's "plain" form) a line that leaned on its label says the word itself
PLAIN = {'SOUND': '49 synthesized drums, 20 synths, granular and one-shot sampler',
         'KEY': 'Key lock holds every track in one key and scale'}
# the same features as one paragraph (the picture's "para" form); *...* is set in the colours, one after another
PARA = ('*49 synthesized drum models* and 20 synth models, plus a granular and one-shot sampler. *23 modular FX*, up to 20 at once. '
        '*Parameter locks*, ratchets, chance, ties and micro timing on every step. A one-click *algorithmic line generator*, '
        'and an *Autopilot* mode for hands-free noodling. *Key lock* holds every track in tune. '
        'Four modulators a track and *four macros*. 16 patterns chained in *song mode*, a *performance mode* with stutter and snapback, '
        'a *3D visualizer* and custom theme colors. A *VST3 plugin suite*, and it plays in the browser.')
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
    y += 48
    bottom = N - T - 100
    if len(sys.argv) > 4 and sys.argv[4] == 'para':          # one paragraph, set in the middle: the features run on, a
        f = font(35, 500); lh = 54                           # coloured dot between them
        maxw = N - 2 * T - 220
        segs = PARA.split('*'); nkey = len(segs) // 2
        words = []                                           # (text, colour): a key phrase takes the next colour
        for k, seg in enumerate(segs):
            col = ramp((k // 2) / max(1, nkey - 1)) if k % 2 else WHITE
            for w_ in seg.split(' '):
                if w_: words.append([w_, col])
            if seg[:1] in ',.' and len(words) > 1 and not seg[1:2].strip() and k % 2 == 0: pass
        # punctuation that follows a key phrase belongs to its last word (no space before it)
        fixed = []
        for wd, col in words:
            if wd[0] in ',.' and fixed: fixed[-1].append(wd[0]); wd = wd[1:]
            if wd: fixed.append([wd, col])
        words = [(x[0], x[1], x[2] if len(x) > 2 else '') for x in fixed]
        lines, cur, w = [], [], 0
        sp = d.textlength(' ', font=f)
        for word, col, tail in words:
            ww = d.textlength(word + tail, font=f)
            if cur and w + sp + ww > maxw: lines.append((cur, w)); cur, w = [], 0
            w += (sp if cur else 0) + ww; cur.append((word, col, ww, tail))
        if cur: lines.append((cur, w))
        yy = y + (bottom - y - len(lines) * lh) // 2
        for cur, w in lines:
            x = cx - w / 2
            for word, col, ww, tail in cur:
                d.text((x, yy), word, font=f, fill=col)
                if tail: d.text((x + d.textlength(word, font=f), yy), tail, font=f, fill=WHITE)
                x += ww + sp
            yy += lh
    elif len(sys.argv) > 4 and sys.argv[4] == 'plain':       # the list with no labels: a coloured mark, then the feature,
        rows = [PLAIN.get(lab, t) for lab, t in ROWS]        # set left and as large as the longest line allows
        lx, right = T + 120, N - T - 120
        px = 48
        while max(d.textlength(t, font=font(px, 500)) for t in rows) > right - lx - 44: px -= 1
        ft = font(px, 500); y += 10
        pitch = (bottom - y) // len(rows)
        for k, t in enumerate(rows):
            col, ym = ramp(k / (len(rows) - 1)), y + pitch // 2
            d.rectangle((lx, ym - 8, lx + 16, ym + 8), fill=col)
            d.text((lx + 44, ym), t, font=ft, fill=WHITE, anchor='lm')
            y += pitch
    else:                                                    # the list: the labels in brackets, then the features
        lx, tx, right = T + 120, T + 352, N - T - 120        # (set in from the screens)
        px = 40
        while max(d.textlength(t, font=font(px, 500)) for _, t in ROWS) > right - tx: px -= 1
        fl, ft = font(max(20, px - 3), 700), font(px, 500)
        y += 10
        pitch = (bottom - y) // len(ROWS)
        lw = max(d.textlength(lab, font=fl) for lab, _ in ROWS)
        b0, b1 = lx, lx + 16 + lw + 16
        tx = b1 + 34
        for k, (lab, t) in enumerate(ROWS):
            col, ym = ramp(k / (len(ROWS) - 1)), y + pitch // 2
            d.text((b0, ym - 2), '[', font=fl, fill=col, anchor='lm'); d.text((b1, ym - 2), ']', font=fl, fill=col, anchor='rm')
            d.text((b0 + 16, ym), lab, font=fl, fill=col, anchor='lm')
            d.text((tx, ym), t, font=ft, fill=WHITE, anchor='lm')
            y += pitch
    ff = font(28, 600); gap = 18
    w0, w1 = d.textlength(FOOT[0], font=ff), d.textlength(FOOT[1], font=ff)
    x = cx - (w0 + gap + w1) / 2
    d.text((x, N - T - 52), FOOT[0], font=ff, fill=GRAY, anchor='lt')
    d.text((x + w0 + gap, N - T - 52), FOOT[1], font=ff, fill=ORANGE, anchor='lt')
    im.save(out, optimize=True)
    print(out, im.size, out.stat().st_size, 'bytes')

if __name__ == '__main__':
    main()
