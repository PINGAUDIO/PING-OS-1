"""Runs fm1-base's tools/gen_font.py on a Pillow without libraqm: font
features are dropped when none are asked for (the basic layout is the same)."""
import runpy, sys
from PIL import ImageFont
for name in ("getbbox", "getmask", "getmask2", "getlength"):
    orig = getattr(ImageFont.FreeTypeFont, name)
    def wrap(self, *a, _o=orig, **k):
        k.pop("features", None)   # tabular digits need libraqm; the default digits are used
        return _o(self, *a, **k)
    setattr(ImageFont.FreeTypeFont, name, wrap)
from PIL import ImageDraw
_t = ImageDraw.ImageDraw.text
def _text(self, *a, **k):
    k.pop("features", None)
    return _t(self, *a, **k)
ImageDraw.ImageDraw.text = _text
sys.argv = [sys.argv[1]] + sys.argv[2:]
runpy.run_path(sys.argv[0], run_name="__main__")
