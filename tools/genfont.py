#!/usr/bin/env python3
"""Generate the Zirvium anti-aliased font atlas.

The output is a compact binary consumed by DisplayJet at boot.  It contains
ASCII 0x20..0x7e rendered from DejaVu Sans (regular and bold) at several
pixel sizes.  DejaVu is distributed under the Bitstream Vera licence; see
assets/FONT-LICENSE.txt.

Usage: python3 tools/genfont.py [output]
"""

from __future__ import annotations

import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

MAGIC = b"ZFNT"
VERSION = 1

REGULAR = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

SIZES = [12, 14, 16, 18, 22, 28, 40]
WEIGHTS = [(0, REGULAR), (1, BOLD)]


def render_glyph(font: ImageFont.FreeTypeFont, char: str):
    bbox = font.getbbox(char)
    if bbox is None:
        return (0, 0, 0, 0, 0, b"")
    left, top, right, bottom = bbox
    width = max(0, right - left)
    height = max(0, bottom - top)
    advance = int(round(font.getlength(char)))
    if width == 0 or height == 0:
        return (advance, left, top, 0, 0, b"")
    image = Image.new("L", (width, height), 0)
    draw = ImageDraw.Draw(image)
    draw.text((-left, -top), char, font=font, fill=255)
    return (advance, left, top, width, height, image.tobytes())


def main() -> int:
    output = Path(sys.argv[1] if len(sys.argv) > 1 else "assets/font.bin")
    output.parent.mkdir(parents=True, exist_ok=True)

    fonts = []
    for weight, path in WEIGHTS:
        for size in SIZES:
            font = ImageFont.truetype(path, size)
            ascent, descent = font.getmetrics()
            line_height = ascent + descent
            glyphs = []
            data = bytearray()
            for codepoint in range(0x20, 0x7F):
                advance, bearing_x, bearing_y, width, height, alpha = render_glyph(
                    font, chr(codepoint)
                )
                offset = len(data)
                data.extend(alpha)
                glyphs.append(
                    (
                        codepoint,
                        advance,
                        bearing_x,
                        bearing_y - ascent,
                        width,
                        height,
                        offset,
                        len(alpha),
                    )
                )
            fonts.append(
                {
                    "weight": weight,
                    "size": size,
                    "ascent": ascent,
                    "descent": descent,
                    "line_height": line_height,
                    "glyphs": glyphs,
                    "data": bytes(data),
                }
            )

    with output.open("wb") as handle:
        handle.write(MAGIC)
        handle.write(struct.pack("<II", VERSION, len(fonts)))
        for font in fonts:
            handle.write(
                struct.pack(
                    "<IIiiiI",
                    font["weight"],
                    font["size"],
                    font["ascent"],
                    font["descent"],
                    font["line_height"],
                    len(font["glyphs"]),
                )
            )
            for glyph in font["glyphs"]:
                handle.write(struct.pack("<IIiiiIII", *glyph))
            handle.write(font["data"])

    print(
        f"wrote {output} ({output.stat().st_size} bytes, {len(fonts)} faces, "
        f"{len(fonts[0]['glyphs'])} glyphs each)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
