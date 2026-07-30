# -*- coding: utf-8 -*-
"""أيقونة AdhanBox — مربع مدوّر بتدرّج أزرق→أخضر (هوية MagicWeb) + مسجد أبيض.
الطريقة: نبني ماسك أبيض للمسجد ونقصّ منه الباب والهلال، فالتدرّج يبان من خلالهم."""
from PIL import Image, ImageDraw
import os

S = 1024
cx = S // 2

# ── الخلفية: تدرّج أزرق فوق → أخضر تحت داخل مربع مدوّر ──
grad = Image.new("RGB", (1, S))
top, bot = (26, 86, 219), (34, 175, 110)
for y in range(S):
    t = y / (S - 1)
    grad.putpixel((0, y), tuple(int(top[i] + (bot[i] - top[i]) * t) for i in range(3)))
grad = grad.resize((S, S))

bgmask = Image.new("L", (S, S), 0)
ImageDraw.Draw(bgmask).rounded_rectangle([0, 0, S - 1, S - 1], radius=int(S * 0.22), fill=255)
img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
img.paste(grad, (0, 0), bgmask)

# ── ماسك المسجد ──
m = Image.new("L", (S, S), 0)
d = ImageDraw.Draw(m)
ON, OFF = 255, 0

dome_w = int(S * 0.44)
dome_top, dome_bot = int(S * 0.31), int(S * 0.55)
d.pieslice([cx - dome_w // 2, dome_top, cx + dome_w // 2, dome_bot + dome_w // 2], 180, 360, fill=ON)
d.rectangle([cx - dome_w // 2, int(S * 0.50), cx + dome_w // 2, int(S * 0.735)], fill=ON)

# المئذنتان
for sgn in (-1, 1):
    mx = cx + sgn * int(S * 0.305)
    mw = int(S * 0.088)
    d.rectangle([mx - mw // 2, int(S * 0.44), mx + mw // 2, int(S * 0.735)], fill=ON)
    d.pieslice([mx - mw // 2, int(S * 0.385), mx + mw // 2, int(S * 0.495)], 180, 360, fill=ON)
    d.ellipse([mx - int(mw * 0.20), int(S * 0.345), mx + int(mw * 0.20), int(S * 0.392)], fill=ON)

# القاعدة
d.rounded_rectangle([int(S * 0.155), int(S * 0.715), int(S * 0.845), int(S * 0.80)],
                    radius=int(S * 0.022), fill=ON)

# الهلال فوق القبة (دائرة ناقصة دائرة مزاحة)
r1 = int(S * 0.062)
cy = dome_top - int(S * 0.055)
d.ellipse([cx - r1, cy - r1, cx + r1, cy + r1], fill=ON)
off, r2 = int(r1 * 0.62), int(r1 * 0.90)
d.ellipse([cx - r2 + off, cy - r2 - int(r1 * 0.18),
           cx + r2 + off, cy + r2 - int(r1 * 0.18)], fill=OFF)

# الباب المقوّس — مقصوص فالتدرّج بيبان من خلاله
dw = int(S * 0.115)
dtop = int(S * 0.565)
d.pieslice([cx - dw, dtop, cx + dw, dtop + dw * 2], 180, 360, fill=OFF)
d.rectangle([cx - dw, dtop + dw, cx + dw, int(S * 0.716)], fill=OFF)

# شبّاكان صغيران على جانبي الباب
for sgn in (-1, 1):
    wx = cx + sgn * int(S * 0.155)
    ww = int(S * 0.035)
    wt = int(S * 0.60)
    d.pieslice([wx - ww, wt, wx + ww, wt + ww * 2], 180, 360, fill=OFF)
    d.rectangle([wx - ww, wt + ww, wx + ww, wt + int(S * 0.075)], fill=OFF)

white = Image.new("RGBA", (S, S), (255, 255, 255, 255))
img = Image.alpha_composite(img, Image.composite(white, Image.new("RGBA", (S, S), (0, 0, 0, 0)), m))

here = os.path.dirname(os.path.abspath(__file__))
out = os.path.join(here, "adhanbox.ico")
img.save(out, format="ICO", sizes=[(s, s) for s in [256, 128, 64, 48, 32, 24, 20, 16]])
img.resize((512, 512), Image.LANCZOS).save(os.path.join(here, "adhanbox-512.png"))
# معاينة مصغّرة للتأكد إن الشكل بيقرا في الأحجام الصغيرة
prev = Image.new("RGBA", (16 + 24 + 32 + 48 + 64 + 60, 72), (24, 30, 44, 255))
x = 8
for s in (16, 24, 32, 48, 64):
    prev.paste(img.resize((s, s), Image.LANCZOS), (x, (72 - s) // 2), img.resize((s, s), Image.LANCZOS))
    x += s + 10
prev.save(os.path.join(here, "icon-preview.png"))
print("saved:", out, os.path.getsize(out), "bytes")
