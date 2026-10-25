#!/usr/bin/env python3
"""Generate the ShallotsOS boot logo shown by the stock firmware at power-on.

Writes the two formats the firmware reads from cubegm/xgame-logo.bmp:

  sdcard/cubegm/xgame-logo.bmp  640x480 panels (R36SX, R36HD, GB350):
                                32-bit BGRA, BITMAPV5 header, bottom-up
  xgame-logo.bmp                854x480 panels (SF3000 family): 32-bit BGRA,
                                BITMAPINFOHEADER, 480x854 top-down, the
                                landscape picture turned 90 degrees clockwise

The headers match the TreeFrogUI files these replace byte for byte, apart
from the pixels. Artwork comes from the ShallotsOS theme generator.

    tools/onion_theme/make_boot_logo.py [ROOT]

Requires Pillow. ROOT defaults to the repository root.
"""
import math
import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, str(Path(__file__).resolve().parent))
import make_shallotsos_theme as theme  # noqa: E402


def screen(w, h):
    # The theme's background look, sized for the panel, with the logo centred.
    im = Image.new("RGB", (w, h))
    px = im.load()
    for y in range(h):
        t = y / (h - 1)
        for x in range(w):
            dx, dy = (x - w / 2) / (w * 0.6), (y - h / 2) / (h * 0.75)
            glow = max(0.0, 1.0 - math.sqrt(dx * dx + dy * dy)) ** 2 * 0.28
            c = [theme.BG_TOP[i] * (1 - t) + theme.BG_BOTTOM[i] * t for i in range(3)]
            c = [min(255, int(c[i] + (theme.ACCENT[i] - c[i]) * glow)) for i in range(3)]
            px[x, y] = tuple(c)
    layer = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for i in range(7):
        x = -140 + i * 34
        d.arc((x, h - 180, x + 420, h + 420), 200, 290, fill=(255, 255, 255, 7), width=2)
    im = Image.alpha_composite(im.convert("RGBA"), layer.filter(ImageFilter.GaussianBlur(0.6)))
    mark = theme.logo(2)
    im.alpha_composite(mark, ((w - mark.width) // 2, (h - mark.height) // 2))
    return im.convert("RGB")


def bgra(im):
    return im.convert("RGBA").tobytes("raw", "BGRA")


def bmp_640(im):
    # BITMAPV5HEADER, BI_BITFIELDS with an alpha mask, sRGB, bottom-up rows.
    w, h = im.size
    pixels = bgra(im.transpose(Image.FLIP_TOP_BOTTOM))
    endpoints = bytes.fromhex("8fc2f52851b81e151e85eb01333333136666662666666606"
                              "999999093d0ad703285c8f32")
    info = struct.pack("<IiiHHIIiiII", 124, w, h, 1, 32, 3, len(pixels), 2834, 2834, 0, 0)
    info += struct.pack("<IIII", 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
    info += b"BGRs" + endpoints + bytes(12) + struct.pack("<IIII", 4, 0, 0, 0)
    head = b"BM" + struct.pack("<IHHI", 14 + len(info) + len(pixels), 0, 0, 14 + len(info))
    return head + info + pixels


def bmp_854(im):
    # BITMAPINFOHEADER, 480 wide and 854 high stored top-down (negative height).
    # The size fields keep the stock file's values (2 and 18 bytes over the
    # real sizes): the firmware has only ever been seen reading those.
    portrait = im.transpose(Image.ROTATE_270)
    w, h = portrait.size
    pixels = bgra(portrait)
    info = struct.pack("<IiiHHIIiiII", 40, w, -h, 1, 32, 0, len(pixels) + 2, 2834, 2834, 0, 0)
    head = b"BM" + struct.pack("<IHHI", 14 + len(info) + len(pixels) + 18, 0, 0, 14 + len(info))
    return head + info + pixels


def main():
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else theme.ROOT
    out = root / "sdcard/cubegm/xgame-logo.bmp"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(bmp_640(screen(640, 480)))
    (root / "xgame-logo.bmp").write_bytes(bmp_854(screen(854, 480)))
    print(out)
    print(root / "xgame-logo.bmp")


if __name__ == "__main__":
    main()
