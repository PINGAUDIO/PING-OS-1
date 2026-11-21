"""The Firmware Hub card picture (fmhub-card.png): a four by four grid, twelve of the firmware's own screens at
their own size around the PING mark and the name. Square, 1024 x 1024, a PNG under the hub's 300 KB.
    python tools/make_card.py SHOTS_DIR [OUT.png]     (SHOTS_DIR: the simulator's captures, 240 x 240 .bmp)"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
WHITE = (242, 242, 240)
# the screens, row by row, the middle four left for the logo: the tracks' colours spread round the ring
SCREENS = ['tracks_play', 'panel_wobble', 'card_mixer', 'envelope',
           'step', None, None, 'card_line',
           'card_reverb', None, None, 'card_drive',
           'fx_21', 'scope', 'step_synth', 'card_lfo']

def main():
    shots = Path(sys.argv[1]); out = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / 'fmhub-card.png'
    N, M, G, T = 1024, 20, 8, 240                # the picture, the margin (the frame is inside it), the gap, a tile (a screen as it is)
    im = Image.new('RGB', (N, N), 'black'); d = ImageDraw.Draw(im)
    for k, name in enumerate(SCREENS):
        if name:
            im.paste(Image.open(shots / (name + '.bmp')).convert('RGB'), (M + (k % 4) * (T + G), M + (k // 4) * (T + G)))
    # the middle: the mark over the name, no tagline: PING AUDIO's own drawing (PING_OS-1_logo_tag_large.svg), drawn
    # white on black as app/web/brand/ping-os-1-logo-card.png, with room round it
    logo = Image.open(ROOT / 'app' / 'web' / 'brand' / 'ping-os-1-logo-card.png').convert('RGB')
    w = 280
    logo = logo.resize((w, round(logo.height * w / logo.width)), Image.LANCZOS)
    im.paste(logo, ((N - logo.width) // 2, (N - logo.height) // 2))
    d.rectangle((10, 10, N - 11, N - 11), outline=WHITE, width=3)   # the frame: a thin white line, a little in from the edge
    im = im.quantize(256, method=Image.MEDIANCUT, kmeans=3, dither=Image.NONE)   # (the enlarged screens: 256 colours keep it under the hub's limit)
    im.save(out, optimize=True)
    print(out, out.stat().st_size, 'bytes')

if __name__ == '__main__':
    main()
