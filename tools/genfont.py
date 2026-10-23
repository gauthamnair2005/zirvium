#!/usr/bin/env python3
"""Generate the Zirvium anti-aliased font atlas.

The output is a compact binary consumed by DisplayJet at boot.  It contains
ASCII 0x20..0x7e plus the Latin-1 letters, typographic punctuation and arrows
the desktop actually uses, rendered from DejaVu Sans (regular and bold) at
several pixel sizes.  DejaVu is distributed under the Bitstream Vera licence;
see assets/FONT-LICENSE.txt.

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

# Every glyph the desktop can ask for.  ASCII is always present (the renderer
# falls back to '?'), and the rest cover the Latin-1 letters plus the
# typographic punctuation, arrows and symbols used by the interface.  The list
# is sorted and de-duplicated so the runtime can binary-search it.
CODEPOINTS = sorted(
    set(range(0x20, 0x7F))
    | set(range(0xA0, 0x100))          # Latin-1 supplement
    | set(range(0x2010, 0x2028))       # dashes and typographic punctuation
    | {0x2030}                          # per mille
    | {0x2039, 0x203A}                  # single guillemets
    | {0x20AC}                          # euro
    | {0x2022, 0x2026}                  # bullet, ellipsis
    | set(range(0x2190, 0x219A))       # arrows
    | {0x21B5, 0x21E7}                  # return, shift
    | {0x2212, 0x2260, 0x2264, 0x2265} # minus, not-equal, <=, >=
    | {0x25A0, 0x25A1, 0x25B2, 0x25B6, 0x25BC, 0x25C0, 0x25CF, 0x25CB}
    | {0x2713, 0x2717}                  # check, cross
)


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
            for codepoint in CODEPOINTS:
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
