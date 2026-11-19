"""The panel drawing for the manual (docs/img/panel.png): the FM-1's layout as fm1-base's emulator has it, with the
firmware's names for the controls. Jost from app/assets/fonts. Run: python docs/draw_panel.py"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
FONT = ROOT / 'app' / 'assets' / 'fonts' / 'Jost[wght].ttf'
S = 2                                        # pixels per emulator unit (a 1000 x 640 drawing)
BODY, TRAY, KNOB, BTN, KEYW, KEYB = (22, 23, 26), (34, 36, 41), (58, 60, 66), (48, 50, 56), (236, 236, 238), (28, 29, 33)
LABEL, DIM, ACC = (228, 228, 232), (150, 152, 160), (255, 132, 36)
TRACK = [(255, 92, 61), (221, 220, 1), (228, 230, 186), (51, 178, 159)]


def font(px, wght=600):
    f = ImageFont.truetype(str(FONT), px)
    try:
        f.set_variation_by_axes([wght])
    except Exception:
        pass
    return f


def main():
    W, H = 1000 * S, 640 * S
    im = Image.new('RGB', (W, H), (255, 255, 255))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((4 * S, 4 * S, 996 * S, 636 * S), 44 * S, fill=BODY)

    def text(x, y, s, px, fill=LABEL, wght=600, anchor='mm'):
        d.text((x * S, y * S), s, font=font(px * S, wght), fill=fill, anchor=anchor)

    def knob(x, y, r, label, sub=None):
        d.ellipse(((x - r - 5) * S, (y - r - 5) * S, (x + r + 5) * S, (y + r + 5) * S), fill=TRAY)
        d.ellipse(((x - r) * S, (y - r) * S, (x + r) * S, (y + r) * S), fill=KNOB, outline=(68, 71, 76), width=2)
        d.line(((x) * S, (y - r + 5) * S, x * S, (y - r + 15) * S), fill=LABEL, width=3 * S)
        text(x, y - r - 12, label, 15)
        if sub:
            text(x, y + r + 14, sub, 12, ACC)

    def button(x, y, w, h, label, two=None, sub=None):
        d.rounded_rectangle((x * S, y * S, (x + w) * S, (y + h) * S), 9 * S, fill=BTN, outline=(74, 77, 82), width=2)
        if two:
            text(x + w / 2, y + h / 2 - 7, label, 11); text(x + w / 2, y + h / 2 + 7, two, 11)
        else:
            text(x + w / 2, y + h / 2, label, 14)
        if sub:
            text(x + w / 2, y + h + 9, sub, 10, ACC)

    knob(105, 100, 24, 'MASTER', 'volume'); knob(215, 100, 24, 'SELECT', 'page · cursor')
    knob(105, 205, 24, 'PRESETS', 'track'); knob(215, 205, 24, 'ALGORITHM', 'engine · effect')
    for i in range(4):
        knob(610 + i * 104, 100, 22, f'KNOB {i + 1}', f'K{i + 1}')
    d.rounded_rectangle((58 * S, 258 * S, 268 * S, 316 * S), 14 * S, fill=TRAY)
    button(72, 268, 82, 38, 'OCT-'); button(170, 268, 82, 38, 'OCT+')
    d.rounded_rectangle((300 * S, 44 * S, 536 * S, 280 * S), 26 * S, fill=(17, 18, 20))
    text(418, 162, 'SCREEN', 13, DIM)
    d.rounded_rectangle((568 * S, 160 * S, 968 * S, 300 * S), 18 * S, fill=TRAY)
    row1 = [('FX', 'effects'), ('SEL', 'keys'), ('ENV', 'mutes'), ('LFO', 'mods'), ('EDIT', 'engine'), ('GLO', 'mix')]
    row2 = [('HOME', 'tracks'), ('SAVE', 'project'), ('ARP', ''), ('SEQ', 'steps'), ('PLAY', ''), ('REC', '')]
    for i, (l, sub) in enumerate(row1):
        button(582 + i * 63, 174, 54, 36, l, sub=sub)
    for i, (l, sub) in enumerate(row2):
        button(582 + i * 63, 236, 54, 36, l, 'STOP' if l == 'PLAY' else None, sub=sub)
    # the keybed, white keys 1-16 and black keys 1-11 numbered
    x0, y0, w, h = 36, 340, 928, 270
    d.rounded_rectangle((x0 * S, y0 * S, (x0 + w) * S, (y0 + h) * S), 18 * S, fill=TRAY)
    pitch = (w - 24) / 16; kw = pitch * 0.78
    black_after = [0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14]
    for i in range(16):
        x = x0 + 12 + i * pitch + (pitch - kw) / 2; y = y0 + h * 0.43
        d.rounded_rectangle((x * S, y * S, (x + kw) * S, (y + h * 0.52) * S), int(kw / 2 * S), fill=KEYW)
        text(x + kw / 2, y + h * 0.52 - 16, str(i + 1), 14, (40, 40, 44))
    for k, a in enumerate(black_after):
        x = x0 + 12 + (a + 1) * pitch - kw / 2; y = y0 + h * 0.06
        d.rounded_rectangle((x * S, y * S, (x + kw) * S, (y + h * 0.33) * S), int(kw / 2 * S), fill=KEYB, outline=(60, 62, 68), width=2)
        text(x + kw / 2, y + h * 0.33 - 14, str(k + 1), 13, (200, 200, 204))
    out = ROOT / 'docs' / 'img' / 'panel.png'
    im.save(out)
    print(out)


if __name__ == '__main__':
    main()
