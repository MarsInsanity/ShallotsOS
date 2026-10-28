#!/usr/bin/env python3
"""Render the ShallotsOS "Updating" screen as a 24-bit BMP.

    tools/make_update_splash.py <output.bmp> [--width 640 --height 480]

shallots_osd --splash shows it full screen while update.zip installs and draws
the live progress bar and percentage over the space left at BAR_Y (keep the
two in step: apps/osd/shallots_osd.c SPLASH_BAR_Y is measured from the
image's centre). Uses the ShallotsOS theme's background and Exo 2 font.
"""
import argparse
import os

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BACKGROUND = os.path.join(ROOT, "assets/onion/Themes/ShallotsOS/skin/background.png")
FONT = os.path.join(ROOT, "assets/onion/fonts/Exo-2-Bold-Italic.ttf")
TITLE = "Updating ShallotsOS"
NOTE = "Please don't switch the console off."
AFTER = "It restarts by itself when the update is done."


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--width", type=int, default=640)
    ap.add_argument("--height", type=int, default=480)
    a = ap.parse_args()
    w, h = a.width, a.height

    img = Image.open(BACKGROUND).convert("RGB")
    scale = max(w / img.width, h / img.height)
    img = img.resize((round(img.width * scale), round(img.height * scale)), Image.LANCZOS)
    left, top = (img.width - w) // 2, (img.height - h) // 2
    img = img.crop((left, top, left + w, top + h))
    # Darken for legible text, as OnionOS dims behind its dialogs.
    img = Image.blend(img, Image.new("RGB", img.size, (0, 0, 0)), 0.45)

    d = ImageDraw.Draw(img)
    title = ImageFont.truetype(FONT, 38)
    small = ImageFont.truetype(FONT, 21)
    cy = h // 2
    for text, font, y, fill in ((TITLE, title, cy - 92, (255, 255, 255)),
                                (NOTE, small, cy - 34, (230, 225, 235)),
                                (AFTER, small, cy + 96, (190, 182, 200))):
        tw = d.textlength(text, font=font)
        d.text(((w - tw) / 2, y), text, font=font, fill=fill)
    # The progress bar (drawn live) sits at cy + 16 .. cy + 32, the
    # percentage below it at cy + 44.
    img.save(a.out, format="BMP")


if __name__ == "__main__":
    main()
