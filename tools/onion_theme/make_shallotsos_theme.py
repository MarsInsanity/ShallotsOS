#!/usr/bin/env python3
"""Generate the ShallotsOS default theme in OnionOS theme format.

The output is a regular OnionOS theme folder (config.json, skin/, icons/app/,
sound/change.wav, preview.png). It works on ShallotsOS and on a Miyoo Mini
running OnionOS. All artwork is drawn here; fonts are referenced by their
OnionOS paths (Exo 2 Bold Italic and WenQuanYi Micro Hei).

    tools/onion_theme/make_shallotsos_theme.py [OUT_DIR]

Requires Pillow. OUT_DIR defaults to assets/onion/Themes/ShallotsOS.
"""
import json
import math
import struct
import sys
import wave
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[2]
EXO = ROOT / "assets/onion/fonts/Exo-2-Bold-Italic.ttf"
SS = 4  # supersampling factor

# Palette: deep plum background, shallot violet accent, copper highlights.
BG_TOP = (33, 24, 40)
BG_BOTTOM = (20, 15, 26)
ACCENT = (168, 92, 196)
ACCENT_SOFT = (122, 74, 146)
COPPER = (232, 150, 98)
CARD = (255, 255, 255, 14)
CARD_FOCUS = (58, 40, 70, 255)
TEXT = (255, 255, 255)
MUTED = (138, 122, 148)


def hexc(rgb):
    return "#%02X%02X%02X" % rgb[:3]


def canvas(w, h, fill=(0, 0, 0, 0)):
    return Image.new("RGBA", (w * SS, h * SS), fill)


def finish(im):
    return im.resize((im.width // SS, im.height // SS), Image.LANCZOS)


def rrect(draw, box, r, **kw):
    x0, y0, x1, y1 = [v * SS for v in box]
    draw.rounded_rectangle((x0, y0, x1, y1), radius=r * SS, **kw)


def save(im, path):
    path.parent.mkdir(parents=True, exist_ok=True)
    im.save(path, optimize=True)


# --------------------------------------------------------------------------
# Line-art glyphs (drawn into a supersampled canvas, coordinates in 1x px)
# --------------------------------------------------------------------------

def stroke(draw, pts, color, width):
    pts = [(x * SS, y * SS) for x, y in pts]
    draw.line(pts, fill=color, width=int(width * SS), joint="curve")
    r = width * SS / 2
    for x, y in (pts[0], pts[-1]):
        draw.ellipse((x - r, y - r, x + r, y + r), fill=color)


def arc(draw, cx, cy, rad, start, end, color, width):
    box = ((cx - rad) * SS, (cy - rad) * SS, (cx + rad) * SS, (cy + rad) * SS)
    draw.arc(box, start, end, fill=color, width=int(width * SS))


def glyph_recent(d, cx, cy, s, c, w):
    arc(d, cx, cy, s, -50, 250, c, w)
    # Arrowhead at the open end, pointing back along the dial (counter-clockwise).
    a = math.radians(-50)
    px, py = cx + s * math.cos(a), cy + s * math.sin(a)
    tx, ty = math.sin(a), -math.cos(a)          # counter-clockwise tangent
    nx, ny = math.cos(a), math.sin(a)           # outward normal
    L, half = s * 0.42, s * 0.3
    tri = [(px + tx * L, py + ty * L), (px + nx * half, py + ny * half),
           (px - nx * half, py - ny * half)]
    d.polygon([(x * SS, y * SS) for x, y in tri], fill=c)
    stroke(d, [(cx, cy - s * 0.55), (cx, cy), (cx + s * 0.42, cy + s * 0.28)], c, w)


def glyph_star(d, cx, cy, s, c, w):
    pts = []
    for i in range(10):
        r = s if i % 2 == 0 else s * 0.45
        a = math.radians(-90 + i * 36)
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    stroke(d, pts + [pts[0]], c, w)


def glyph_game(d, cx, cy, s, c, w):
    x0, y0, x1, y1 = cx - s * 0.72, cy - s, cx + s * 0.72, cy + s
    d.rounded_rectangle((x0 * SS, y0 * SS, x1 * SS, y1 * SS), radius=s * 0.22 * SS,
                        outline=c, width=int(w * SS))
    sx0, sy0, sx1, sy1 = cx - s * 0.46, cy - s * 0.72, cx + s * 0.46, cy - s * 0.08
    d.rounded_rectangle((sx0 * SS, sy0 * SS, sx1 * SS, sy1 * SS), radius=s * 0.06 * SS,
                        outline=c, width=int(w * 0.8 * SS))
    py = cy + s * 0.45
    stroke(d, [(cx - s * 0.5, py), (cx - s * 0.18, py)], c, w * 0.8)
    stroke(d, [(cx - s * 0.34, py - s * 0.16), (cx - s * 0.34, py + s * 0.16)], c, w * 0.8)
    for dx, dy in ((0.36, 0.33), (0.18, 0.55)):
        r = s * 0.09
        bx, by = cx + s * dx, cy + s * dy
        d.ellipse(((bx - r) * SS, (by - r) * SS, (bx + r) * SS, (by + r) * SS), fill=c)


def glyph_apps(d, cx, cy, s, c, w):
    g = s * 0.62
    for dx in (-1, 1):
        for dy in (-1, 1):
            x, y = cx + dx * g * 0.62, cy + dy * g * 0.62
            h = g * 0.46
            d.rounded_rectangle(((x - h) * SS, (y - h) * SS, (x + h) * SS, (y + h) * SS),
                                radius=h * 0.4 * SS, outline=c, width=int(w * SS))


def glyph_gear(d, cx, cy, s, c, w):
    teeth = 8
    pts = []
    for i in range(teeth * 4):
        a = math.radians(i * 360 / (teeth * 4) - 90)
        r = s if (i % 4) in (1, 2) else s * 0.78
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    stroke(d, pts + [pts[0]], c, w)
    arc(d, cx, cy, s * 0.32, 0, 360, c, w)


def glyph_chip(d, cx, cy, s, c, w):
    h = s * 0.6
    d.rounded_rectangle(((cx - h) * SS, (cy - h) * SS, (cx + h) * SS, (cy + h) * SS),
                        radius=h * 0.2 * SS, outline=c, width=int(w * SS))
    for i in (-1, 0, 1):
        o = i * h * 0.55
        stroke(d, [(cx + o, cy - h), (cx + o, cy - s)], c, w * 0.7)
        stroke(d, [(cx + o, cy + h), (cx + o, cy + s)], c, w * 0.7)
        stroke(d, [(cx - h, cy + o), (cx - s, cy + o)], c, w * 0.7)
        stroke(d, [(cx + h, cy + o), (cx + s, cy + o)], c, w * 0.7)


def glyph_book(d, cx, cy, s, c, w):
    stroke(d, [(cx, cy - s * 0.6), (cx, cy + s * 0.75)], c, w)
    for side in (-1, 1):
        stroke(d, [(cx, cy - s * 0.6), (cx + side * s * 0.9, cy - s * 0.75),
                   (cx + side * s * 0.9, cy + s * 0.6), (cx, cy + s * 0.75)], c, w)


def glyph_image(d, cx, cy, s, c, w):
    d.rounded_rectangle(((cx - s) * SS, (cy - s * 0.75) * SS, (cx + s) * SS, (cy + s * 0.75) * SS),
                        radius=s * 0.15 * SS, outline=c, width=int(w * SS))
    stroke(d, [(cx - s * 0.75, cy + s * 0.5), (cx - s * 0.2, cy - s * 0.1),
               (cx + s * 0.15, cy + s * 0.25), (cx + s * 0.4, cy), (cx + s * 0.75, cy + s * 0.5)], c, w)
    r = s * 0.14
    d.ellipse(((cx + s * 0.4 - r) * SS, (cy - s * 0.4 - r) * SS,
               (cx + s * 0.4 + r) * SS, (cy - s * 0.4 + r) * SS), fill=c)


def glyph_video(d, cx, cy, s, c, w):
    d.rounded_rectangle(((cx - s) * SS, (cy - s * 0.7) * SS, (cx + s) * SS, (cy + s * 0.7) * SS),
                        radius=s * 0.2 * SS, outline=c, width=int(w * SS))
    p = [(cx - s * 0.25, cy - s * 0.35), (cx + s * 0.4, cy), (cx - s * 0.25, cy + s * 0.35)]
    d.polygon([(x * SS, y * SS) for x, y in p], fill=c)


def glyph_music(d, cx, cy, s, c, w):
    stroke(d, [(cx - s * 0.35, cy + s * 0.55), (cx - s * 0.35, cy - s * 0.75),
               (cx + s * 0.65, cy - s * 0.95), (cx + s * 0.65, cy + s * 0.35)], c, w)
    for x, y in ((cx - s * 0.6, cy + s * 0.6), (cx + s * 0.4, cy + s * 0.4)):
        r = s * 0.25
        d.ellipse(((x - r) * SS, (y - r) * SS, (x + r) * SS, (y + r) * SS), fill=c)


def glyph_folder(d, cx, cy, s, c, w):
    stroke(d, [(cx - s, cy + s * 0.7), (cx - s, cy - s * 0.7), (cx - s * 0.3, cy - s * 0.7),
               (cx - s * 0.1, cy - s * 0.45), (cx + s, cy - s * 0.45), (cx + s, cy + s * 0.7),
               (cx - s, cy + s * 0.7)], c, w)


def glyph_usb(d, cx, cy, s, c, w):
    stroke(d, [(cx, cy + s * 0.9), (cx, cy - s * 0.8)], c, w)
    stroke(d, [(cx - s * 0.3, cy - s * 0.5), (cx, cy - s * 0.95), (cx + s * 0.3, cy - s * 0.5)], c, w)
    stroke(d, [(cx, cy + s * 0.35), (cx - s * 0.55, cy - s * 0.05), (cx - s * 0.55, cy - s * 0.3)], c, w)
    stroke(d, [(cx, cy + s * 0.1), (cx + s * 0.55, cy - s * 0.25), (cx + s * 0.55, cy - s * 0.45)], c, w)


def glyph_power(d, cx, cy, s, c, w):
    arc(d, cx, cy, s * 0.85, -55, 235, c, w)
    stroke(d, [(cx, cy - s), (cx, cy - s * 0.15)], c, w)


def glyph_chart(d, cx, cy, s, c, w):
    stroke(d, [(cx - s, cy - s), (cx - s, cy + s), (cx + s, cy + s)], c, w)
    stroke(d, [(cx - s * 0.6, cy + s * 0.3), (cx - s * 0.1, cy - s * 0.2),
               (cx + s * 0.3, cy + s * 0.1), (cx + s * 0.85, cy - s * 0.6)], c, w)


# --------------------------------------------------------------------------
# Skin
# --------------------------------------------------------------------------

def background():
    w, h = 640, 480
    im = Image.new("RGB", (w, h))
    px = im.load()
    for y in range(h):
        t = y / (h - 1)
        for x in range(w):
            # Soft violet glow from the upper right, like light through a bulb.
            dx, dy = (x - 520) / 520, (y - 40) / 420
            glow = max(0.0, 1.0 - math.sqrt(dx * dx + dy * dy)) ** 2 * 0.22
            c = [BG_TOP[i] * (1 - t) + BG_BOTTOM[i] * t for i in range(3)]
            c = [min(255, int(c[i] + (ACCENT[i] - c[i]) * glow)) for i in range(3)]
            px[x, y] = tuple(c)
    # Faint shallot-skin stripes in the lower left.
    layer = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for i in range(7):
        x = -140 + i * 34
        d.arc((x, 300, x + 420, 900), 200, 290, fill=(255, 255, 255, 7), width=2)
    im = Image.alpha_composite(im.convert("RGBA"), layer.filter(ImageFilter.GaussianBlur(0.6)))
    return im.convert("RGB")


def bar(h, top_line=False):
    im = Image.new("RGBA", (640, h), (10, 7, 14, 150))
    d = ImageDraw.Draw(im)
    if top_line:
        d.line((0, 0, 639, 0), fill=(255, 255, 255, 28))
    else:
        d.line((0, h - 1, 639, h - 1), fill=(255, 255, 255, 28))
    return im


def list_highlight(h):
    im = Image.new("RGBA", (640, h), (0, 0, 0, 0))
    px = im.load()
    for x in range(640):
        t = x / 639
        a = int(235 * (1 - t ** 3))
        c = tuple(int(ACCENT[i] * (1 - t * 0.35) + ACCENT_SOFT[i] * t * 0.35) for i in range(3))
        for y in range(h):
            px[x, y] = c + (a,)
    d = ImageDraw.Draw(im)
    d.rectangle((0, 0, 5, h - 1), fill=COPPER + (255,))
    return im


def home_tile(glyph, focused):
    w, h = 156, 278
    im = canvas(w, h)
    d = ImageDraw.Draw(im)
    if focused:
        shadow = canvas(w, h)
        rrect(ImageDraw.Draw(shadow), (10, 16, 146, 272), 18, fill=(0, 0, 0, 120))
        im = Image.alpha_composite(im, shadow.filter(ImageFilter.GaussianBlur(10 * SS)))
        d = ImageDraw.Draw(im)
        rrect(d, (8, 8, 148, 266), 18, fill=CARD_FOCUS, outline=ACCENT + (255,), width=3 * SS)
        color, width = ACCENT + (255,), 7
    else:
        rrect(d, (8, 8, 148, 266), 18, fill=CARD)
        color, width = ACCENT_SOFT + (150,), 6
    glyph(d, 78, 104, 46, color, width)
    if focused:
        # Copper underline below the label (the UI draws labels at y=285,
        # which is y=184 inside a tile placed at y=101).
        rrect(d, (60, 210, 96, 214), 2, fill=COPPER + (255,))
    return finish(im)


def game_item(focused):
    w, h = 154, 170
    im = canvas(w, h)
    d = ImageDraw.Draw(im)
    if focused:
        rrect(d, (4, 4, 150, 166), 16, fill=CARD_FOCUS, outline=ACCENT + (255,), width=3 * SS)
    else:
        rrect(d, (4, 4, 150, 166), 16, fill=(255, 255, 255, 8))
    return finish(im)


def button_icon(letter):
    im = canvas(32, 32)
    d = ImageDraw.Draw(im)
    d.ellipse((1 * SS, 1 * SS, 31 * SS, 31 * SS), fill=ACCENT + (255,))
    font = ImageFont.truetype(str(EXO), 20 * SS)
    box = d.textbbox((0, 0), letter, font=font)
    tw, th = box[2] - box[0], box[3] - box[1]
    d.text((16 * SS - tw / 2 - box[0], 16 * SS - th / 2 - box[1]), letter, font=font, fill=TEXT)
    return finish(im)


def arrow(direction):
    im = canvas(24, 24)
    d = ImageDraw.Draw(im)
    pts = [(9, 5), (16, 12), (9, 19)] if direction > 0 else [(15, 5), (8, 12), (15, 19)]
    stroke(d, pts, (255, 255, 255, 230), 3)
    return finish(im)


def gs_arrow(direction):
    # GameSwitcher name-bar arrows (Onion's extra/arrowLeft and arrowRight).
    im = canvas(20, 32)
    d = ImageDraw.Draw(im)
    pts = [(6, 6), (14, 16), (6, 26)] if direction > 0 else [(14, 6), (6, 16), (14, 26)]
    stroke(d, pts, TEXT + (255,), 3)
    return finish(im)


def gs_top_bar():
    # Like Onion's default theme: the logo is part of the GameSwitcher header.
    im = bar(40)
    mark = logo()
    mark = mark.resize((mark.width * 30 // mark.height, 30), Image.LANCZOS)
    im.alpha_composite(mark, (14, 5))
    return im


def pill_icon(label, w):
    font = ImageFont.truetype(str(EXO), 13 * SS)
    im = canvas(w, 22)
    d = ImageDraw.Draw(im)
    rrect(d, (0, 0, w, 22), 11, fill=ACCENT + (255,))
    box = d.textbbox((0, 0), label, font=font)
    d.text((w * SS / 2 - (box[0] + box[2]) / 2, 11 * SS - (box[1] + box[3]) / 2),
           label, font=font, fill=TEXT)
    return finish(im)


def legend_glyph(kind):
    # 24x24 white glyphs for the GameSwitcher button legend.
    im = canvas(24, 24)
    d = ImageDraw.Draw(im)
    c = TEXT + (255,)
    if kind == "fullscreen":
        for x, y, dx, dy in ((3, 3, 1, 1), (21, 3, -1, 1), (3, 21, 1, -1), (21, 21, -1, -1)):
            stroke(d, [(x + 6 * dx, y), (x, y), (x, y + 6 * dy)], c, 2)
    elif kind == "remove":
        stroke(d, [(4, 6), (20, 6)], c, 2)
        stroke(d, [(9, 6), (9, 3), (15, 3), (15, 6)], c, 2)
        stroke(d, [(6, 6), (7, 21), (17, 21), (18, 6)], c, 2)
        stroke(d, [(10, 10), (10, 17)], c, 2)
        stroke(d, [(14, 10), (14, 17)], c, 2)
    elif kind == "time":
        arc(d, 12, 13, 8, 0, 360, c, 2)
        stroke(d, [(12, 13), (12, 8)], c, 2)
        stroke(d, [(10, 3), (14, 3)], c, 2)
        stroke(d, [(18, 5), (20, 7)], c, 2)
    elif kind == "home":
        stroke(d, [(3, 12), (12, 4), (21, 12)], c, 2)
        stroke(d, [(6, 10), (6, 20), (18, 20), (18, 10)], c, 2)
        stroke(d, [(10, 20), (10, 14), (14, 14), (14, 20)], c, 2)
    return finish(im)


def gs_bottom_bar():
    # Like Onion's default theme: the button legend is part of the footer
    # image (Onion draws nothing over a custom gs-bottom-bar).
    im = bar(40, top_line=True)
    d = ImageDraw.Draw(im)
    font = ImageFont.truetype(str(EXO), 20)
    x = 12
    for letter, label in (("A", "RESUME"), ("B", "EXIT")):
        btn = button_icon(letter).resize((26, 26), Image.LANCZOS)
        im.alpha_composite(btn, (x, 7))
        x += 26 + 5
        box = d.textbbox((0, 0), label, font=font)
        d.text((x - box[0], 20 - (box[1] + box[3]) / 2), label, font=font, fill=TEXT)
        x += box[2] - box[0] + 18
    items = [(button_icon("Y").resize((26, 26), Image.LANCZOS), "fullscreen"),
             (button_icon("X").resize((26, 26), Image.LANCZOS), "remove"),
             (pill_icon("SELECT", 56), "time"),
             (pill_icon("START", 50), "home")]
    x = 640 - 12
    for btn, glyph in reversed(items):
        g = legend_glyph(glyph)
        x -= g.width
        im.alpha_composite(g, (x, 8))
        x -= 4 + btn.width
        im.alpha_composite(btn, (x, 20 - btn.height // 2))
        x -= 14
    return im


def toggle(on):
    im = canvas(60, 32)
    d = ImageDraw.Draw(im)
    rrect(d, (2, 4, 58, 28), 12, fill=(ACCENT + (255,)) if on else (255, 255, 255, 40))
    cx = 44 if on else 16
    d.ellipse(((cx - 10) * SS, 6 * SS, (cx + 10) * SS, 26 * SS),
              fill=(255, 255, 255, 255) if on else (190, 180, 196, 255))
    return finish(im)


def dot(active):
    im = canvas(12, 12)
    d = ImageDraw.Draw(im)
    if active:
        d.ellipse((1 * SS, 1 * SS, 11 * SS, 11 * SS), fill=COPPER + (255,))
    else:
        d.ellipse((3 * SS, 3 * SS, 9 * SS, 9 * SS), fill=(255, 255, 255, 60))
    return finish(im)


def battery(level, charging=False):
    """88x48 like Onion's Silky: glyph on the left, percentage drawn at
    offsetX 26 by the UI (config "fixed": true)."""
    im = canvas(88, 48)
    d = ImageDraw.Draw(im)
    rrect(d, (6, 10, 20, 38), 3, outline=(255, 255, 255, 230), width=2 * SS)
    rrect(d, (10, 7, 16, 10), 1, fill=(255, 255, 255, 230))
    if charging:
        p = [(15, 13), (9, 25), (13, 25), (11, 35), (17, 22), (13, 22)]
        d.polygon([(x * SS, y * SS) for x, y in p], fill=COPPER + (255,))
    else:
        fill_h = round(24 * level)
        color = (232, 84, 84, 255) if level <= 0.2 else (255, 255, 255, 230)
        if fill_h > 0:
            rrect(d, (9, 35 - fill_h, 17, 35), 1, fill=color)
    return finish(im)


def logo():
    font = ImageFont.truetype(str(EXO), 30 * SS)
    im = canvas(200, 42)
    d = ImageDraw.Draw(im)
    # Bulb mark: a small shallot made of two arcs and a sprout.
    cx, cy = 14, 23
    d.ellipse(((cx - 10) * SS, (cy - 9) * SS, (cx + 10) * SS, (cy + 12) * SS), fill=ACCENT + (255,))
    stroke(d, [(cx, cy - 9), (cx + 1, cy - 17)], COPPER + (255,), 3)
    stroke(d, [(cx - 4, cy - 2), (cx - 2, cy + 8)], (255, 255, 255, 120), 2)
    x = 30 * SS
    for text, color in (("shallots", ACCENT + (255,)), ("os", TEXT + (255,))):
        box = d.textbbox((0, 0), text, font=font)
        d.text((x, 21 * SS - (box[1] + box[3]) / 2), text, font=font, fill=color)
        x += box[2] - box[0] + 1 * SS
    return finish(im)


def preview_bg():
    im = canvas(256, 364)
    rrect(ImageDraw.Draw(im), (8, 8, 248, 356), 16, fill=(0, 0, 0, 70))
    return finish(im)


def favorite_mark():
    im = canvas(34, 39)
    glyph_star(ImageDraw.Draw(im), 17, 20, 13, COPPER + (255,), 3)
    return finish(im)


def app_icon(glyph):
    im = canvas(70, 70)
    d = ImageDraw.Draw(im)
    rrect(d, (3, 3, 67, 67), 16, fill=(255, 255, 255, 18))
    glyph(d, 35, 35, 19, (255, 255, 255, 235), 4)
    return finish(im)


def change_sound(path):
    rate = 22050
    frames = []
    for i in range(int(rate * 0.035)):
        t = i / rate
        env = math.exp(-t * 140)
        s = math.sin(2 * math.pi * 1900 * t) * 0.55 + math.sin(2 * math.pi * 950 * t) * 0.25
        frames.append(int(max(-1, min(1, s * env)) * 9000))
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(rate)
        f.writeframes(b"".join(struct.pack("<h", v) for v in frames))


def main():
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "assets/onion/Themes/ShallotsOS"
    skin = out / "skin"
    save(background(), skin / "background.png")
    save(bar(60), skin / "bg-title.png")
    save(bar(60, top_line=True), skin / "tips-bar-bg.png")
    save(gs_top_bar(), skin / "extra/gs-top-bar.png")
    save(gs_bottom_bar(), skin / "extra/gs-bottom-bar.png")
    save(gs_arrow(-1), skin / "extra/arrowLeft.png")
    save(gs_arrow(1), skin / "extra/arrowRight.png")
    save(list_highlight(60), skin / "bg-list-s.png")
    save(list_highlight(90), skin / "bg-list-l.png")
    save(Image.new("RGBA", (640, 4), (255, 255, 255, 14)), skin / "div-line-h.png")
    tiles = {"recent": glyph_recent, "favorite": glyph_star, "game": glyph_game,
             "app": glyph_apps, "setting": glyph_gear, "retroarch": glyph_chip}
    for key, glyph in tiles.items():
        save(home_tile(glyph, False), skin / f"ic-{key}-n.png")
        save(home_tile(glyph, True), skin / f"ic-{key}-f.png")
    save(game_item(False), skin / "bg-game-item-n.png")
    save(game_item(True), skin / "bg-game-item-f.png")
    save(button_icon("A"), skin / "icon-A-54.png")
    save(button_icon("B"), skin / "icon-B-54.png")
    save(arrow(-1), skin / "icon-left-arrow-24.png")
    save(arrow(1), skin / "icon-right-arrow-24.png")
    save(toggle(True), skin / "extra/toggle-on.png")
    save(toggle(False), skin / "extra/toggle-off.png")
    save(dot(True), skin / "dot-a.png")
    save(dot(False), skin / "dot-n.png")
    for name, level in (("power-0%-icon", 0.0), ("power-20%-icon", 0.2), ("power-50%-icon", 0.5),
                        ("power-80%-icon", 0.8), ("power-full-icon", 1.0)):
        save(battery(level), skin / f"{name}.png")
    save(battery(1.0, charging=True), skin / "ic-power-charge-100%.png")
    save(logo(), skin / "miyoo-topbar.png")
    save(preview_bg(), skin / "preview-bg.png")
    save(favorite_mark(), skin / "ic-favorite-mark.png")
    folder = canvas(60, 60)
    glyph_folder(ImageDraw.Draw(folder), 30, 31, 20, COPPER + (255,), 4)
    save(finish(folder), skin / "icon-folder.png")
    apps = {"activity": glyph_chart, "commander": glyph_folder, "ftp": glyph_usb,
            "ereader": glyph_book, "gallery": glyph_image, "ffplay": glyph_video,
            "music": glyph_music, "tweaks_system": glyph_power, "search": glyph_star}
    for name, glyph in apps.items():
        save(app_icon(glyph), out / "icons/app" / f"{name}.png")
    change_sound(out / "sound/change.wav")
    exo = "/mnt/SDCARD/miyoo/app/Exo-2-Bold-Italic.ttf"
    config = {
        "name": "ShallotsOS",
        "author": "ShallotsOS",
        "description": "ShallotsOS default theme (OnionOS format)",
        "hideLabels": {"icons": False, "hints": False},
        "batteryPercentage": {"visible": True, "font": exo, "size": 18,
                              "color": hexc(TEXT), "offsetX": 26, "fixed": True},
        "title": {"font": exo, "size": 27, "color": hexc(TEXT)},
        "hint": {"font": exo, "size": 25, "color": hexc(TEXT)},
        "currentpage": {"color": hexc(COPPER)},
        "total": {"color": hexc(TEXT)},
        "grid": {"font": exo, "grid1x4": 25, "grid3x4": 18,
                 "color": hexc(MUTED), "selectedcolor": hexc(TEXT)},
        "list": {"font": "/mnt/SDCARD/miyoo/app/wqy-microhei.ttc", "size": 25,
                 "color": hexc(TEXT)},
    }
    (out / "config.json").write_text(json.dumps(config, indent=4) + "\n")
    (out / "readme.txt").write_text(
        "ShallotsOS default theme, in OnionOS theme format.\n"
        "Artwork generated by tools/onion_theme/make_shallotsos_theme.py.\n"
        "Fonts: Exo 2 Bold Italic (SIL OFL 1.1) and WenQuanYi Micro Hei, as on OnionOS.\n")
    print(out)


if __name__ == "__main__":
    main()
