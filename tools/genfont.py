#!/usr/bin/env python3
"""Generate an improved 8x13 bitmap font + minimal TrueType font file.

Outputs:
  - stdout: Rust source for font.rs glyph data (95 glyphs × 13 bytes)
  - font_8x13.h: C header with the glyph data
  - font_8x13.ttf: Minimal TrueType font

The glyphs are hand-optimised for readability at 8×13 monochrome.
"""

import struct
import os

OUT_DIR = os.path.dirname(os.path.abspath(__file__))

# ── Glyph data (95 printable ASCII chars 32–126) ──────────────────────
# 13 rows per glyph, MSB = leftmost pixel
GLYPHS = [
    # 32  space
    [0x00]*13,
    [0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0x18,0x18],  #  1  '!'
    [0x24,0x24,0x24,0x24,0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00],  #  2  '"'
    [0x00,0x00,0x32,0x26,0x7F,0x7E,0x24,0x7E,0xFE,0x4C,0x4C,0x00,0x00],  #  3  '#'
    [0x08,0x18,0x7E,0x4A,0x48,0x38,0x1E,0x0A,0x4B,0x7E,0x3C,0x00,0x00],  #  4  '$'
    [0x00,0x00,0x00,0x64,0x64,0x28,0x16,0x26,0x26,0x00,0x00,0x00,0x00],  #  5  '%'
    [0x00,0x00,0x38,0x6C,0x6C,0x38,0x30,0x4B,0xCE,0x46,0x3F,0x00,0x00],  #  6  '&'
    [0x10,0x10,0x10,0x10,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00],  #  7  "'"
    [0x08,0x18,0x18,0x10,0x10,0x30,0x30,0x30,0x10,0x10,0x18,0x00,0x00],  #  8  '('
    [0x20,0x30,0x10,0x10,0x18,0x18,0x18,0x18,0x18,0x10,0x10,0x00,0x00],  #  9  ')'
    [0x00,0x18,0x5A,0x7E,0x3C,0x7E,0x18,0x00,0x00,0x00,0x00,0x00,0x00],  # 10  '*'
    [0x00,0x00,0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00,0x00,0x00,0x00],  # 11  '+'
    [0x00,0x00,0x1C,0x1C,0x18,0x38,0x00,0x00,0x00,0x00,0x00,0x00,0x00],  # 12  ','
    [0x00,0x00,0x00,0x7E,0x7C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00],  # 13  '-'
    [0x00,0x00,0x00,0x00,0x18,0x3C,0x3C,0x18,0x00,0x00,0x00,0x00,0x00],  # 14  '.'
    [0x0C,0x0C,0x08,0x08,0x18,0x10,0x10,0x10,0x30,0x20,0x20,0x20,0x00],  # 15  '/'
    [0x00,0x00,0x3C,0x66,0x42,0x42,0x43,0x42,0x42,0x66,0x3C,0x00,0x00],  # 16  '0'
    [0x18,0x78,0x68,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08],  # 17  '1'
    [0x00,0x00,0x3C,0x66,0x42,0x06,0x04,0x08,0x10,0x20,0x7E,0x00,0x00],  # 18  '2'
    [0x00,0x00,0x3C,0x66,0x02,0x04,0x1C,0x06,0x02,0x66,0x3C,0x00,0x00],  # 19  '3'
    [0x00,0x00,0x0C,0x0C,0x1C,0x34,0x24,0x64,0x7E,0x04,0x04,0x00,0x00],  # 20  '4'
    [0x00,0x3E,0x60,0x60,0x78,0x7E,0x02,0x02,0x42,0x66,0x3C,0x00,0x00],  # 21  '5'
    [0x00,0x00,0x3C,0x66,0x40,0x5C,0x66,0x42,0x42,0x66,0x3C,0x00,0x00],  # 22  '6'
    [0x00,0x7E,0x06,0x06,0x04,0x0C,0x08,0x18,0x10,0x30,0x20,0x00,0x00],  # 23  '7'
    [0x00,0x00,0x3C,0x66,0x42,0x24,0x3C,0x66,0x42,0x66,0x3C,0x00,0x00],  # 24  '8'
    [0x00,0x00,0x3C,0x66,0x42,0x42,0x6E,0x3A,0x42,0x66,0x3C,0x00,0x00],  # 25  '9'
    [0x18,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x18],  # 26  ':'
    [0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x10,0x30,0x10,0x00,0x00,0x00],  # 27  ';'
    [0x00,0x00,0x00,0x06,0x1C,0x60,0x70,0x1C,0x06,0x00,0x00,0x00,0x00],  # 28  '<'
    [0x00,0x00,0x00,0x00,0x7E,0x00,0x3C,0x3C,0x00,0x00,0x00,0x00,0x00],  # 29  '='
    [0x00,0x00,0x00,0x60,0x38,0x0E,0x0E,0x38,0x40,0x00,0x00,0x00,0x00],  # 30  '>'
    [0x00,0x3C,0x66,0x42,0x02,0x06,0x1C,0x18,0x10,0x00,0x18,0x18,0x00],  # 31  '?'
    [0x00,0x00,0x00,0x3C,0x42,0xFD,0xA5,0xFE,0x40,0x00,0x00,0x00,0x00],  # 32  '@'
    [0x00,0x00,0x18,0x18,0x3C,0x24,0x66,0x7E,0x42,0xC3,0x00,0x00,0x00],  # 33  'A'
    [0x00,0x00,0x7C,0x46,0x42,0x7C,0x66,0x43,0x42,0x7E,0x00,0x00,0x00],  # 34  'B'
    [0x00,0x00,0x3C,0x66,0x42,0x40,0x40,0x42,0x66,0x3C,0x00,0x00,0x00],  # 35  'C'
    [0x00,0x00,0x7C,0x46,0x42,0x43,0x43,0x42,0x46,0x7C,0x00,0x00,0x00],  # 36  'D'
    [0x00,0x00,0x7E,0x60,0x60,0x60,0x7E,0x60,0x60,0x60,0x7E,0x00,0x00],  # 37  'E'
    [0x00,0x00,0x7E,0x60,0x60,0x60,0x7E,0x60,0x60,0x60,0x60,0x00,0x00],  # 38  'F'
    [0x00,0x00,0x3C,0x66,0x42,0xC0,0xCE,0x42,0x66,0x3C,0x00,0x00,0x00],  # 39  'G'
    [0x00,0x00,0x00,0x42,0x42,0x42,0x7E,0x42,0x42,0x42,0x00,0x00,0x00],  # 40  'H'
    [0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18],  # 41  'I'
    [0x00,0x06,0x06,0x06,0x06,0x06,0x06,0x46,0x46,0x6C,0x38,0x00,0x00],  # 42  'J'
    [0x00,0x00,0x42,0x44,0x48,0x78,0x78,0x4C,0x46,0x42,0x00,0x00,0x00],  # 43  'K'
    [0x00,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x7F,0x00,0x00],  # 44  'L'
    [0x00,0x00,0x00,0x42,0x66,0x66,0x56,0x5A,0x5A,0x00,0x00,0x00,0x00],  # 45  'M'
    [0x00,0x00,0x00,0x62,0x62,0x52,0x5A,0x4A,0x46,0x42,0x00,0x00,0x00],  # 46  'N'
    [0x00,0x00,0x00,0x38,0x44,0xC6,0xC6,0xC6,0x44,0x38,0x00,0x00,0x00],  # 47  'O'
    [0x00,0x00,0x7C,0x46,0x43,0x42,0x66,0x7C,0x40,0x40,0x40,0x00,0x00],  # 48  'P'
    [0x00,0x00,0x38,0x44,0x44,0xC6,0xC6,0x4C,0x7C,0x00,0x00,0x00,0x00],  # 49  'Q'
    [0x00,0x00,0x7C,0x42,0x43,0x42,0x7E,0x7C,0x46,0x46,0x43,0x00,0x00],  # 50  'R'
    [0x00,0x00,0x3C,0x66,0x40,0x60,0x3C,0x06,0x43,0x66,0x3C,0x00,0x00],  # 51  'S'
    [0x00,0x00,0xFF,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00],  # 52  'T'
    [0x00,0x00,0x42,0x42,0x42,0x42,0x42,0x42,0x66,0x3C,0x00,0x00,0x00],  # 53  'U'
    [0x00,0x00,0xC3,0x42,0x62,0x26,0x24,0x3C,0x18,0x18,0x00,0x00,0x00],  # 54  'V'
    [0x00,0x00,0x00,0x00,0x99,0xDB,0x5A,0x66,0x24,0x00,0x00,0x00,0x00],  # 55  'W'
    [0x00,0x00,0x42,0x66,0x3C,0x18,0x18,0x2C,0x66,0x42,0x00,0x00,0x00],  # 56  'X'
    [0x00,0x00,0x43,0x66,0x24,0x1C,0x18,0x18,0x18,0x18,0x00,0x00,0x00],  # 57  'Y'
    [0x00,0x00,0x7E,0x06,0x04,0x08,0x18,0x10,0x20,0x60,0x7E,0x00,0x00],  # 58  'Z'
    [0x18,0x18,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x00,0x00],  # 59  '['
    [0x60,0x60,0x20,0x20,0x30,0x10,0x10,0x10,0x18,0x08,0x08,0x08,0x00],  # 60  '\\'
    [0x18,0x18,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x00],  # 61  ']'
    [0x18,0x3C,0x24,0x66,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00],  # 62  '^'
    [0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00],  # 63  '_'
    [0x30,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00],  # 64  '`'
    [0x00,0x00,0x3C,0x66,0x06,0x3E,0x76,0x46,0x6E,0x3E,0x00,0x00,0x00],  # 65  'a'
    [0x00,0x00,0x40,0x40,0x7C,0x66,0x42,0x43,0x42,0x66,0x5C,0x00,0x00],  # 66  'b'
    [0x00,0x00,0x3C,0x66,0x40,0xC0,0x40,0x42,0x66,0x3C,0x00,0x00,0x00],  # 67  'c'
    [0x00,0x00,0x02,0x02,0x3E,0x66,0x42,0xC2,0x42,0x66,0x3A,0x00,0x00],  # 68  'd'
    [0x00,0x00,0x3C,0x66,0x42,0x7F,0x40,0x40,0x66,0x3C,0x00,0x00,0x00],  # 69  'e'
    [0x0E,0x1C,0x18,0x3C,0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18],  # 70  'f'
    [0x00,0x3E,0x66,0x42,0xC2,0x42,0x66,0x3E,0x02,0x00,0x00,0x00,0x00],  # 71  'g'
    [0x00,0x00,0x40,0x40,0x7C,0x66,0x42,0x42,0x42,0x42,0x42,0x00,0x00],  # 72  'h'
    [0x18,0x18,0x00,0x10,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18],  # 73  'i'
    [0x10,0x10,0x00,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x00,0x00],  # 74  'j'
    [0x00,0x60,0x60,0x62,0x66,0x6C,0x78,0x78,0x6C,0x66,0x63,0x00,0x00],  # 75  'k'
    [0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18],  # 76  'l'
    [0x00,0x00,0x00,0x00,0x7E,0x5B,0x5B,0x5B,0x5B,0x00,0x00,0x00,0x00],  # 77  'm'
    [0x00,0x00,0x00,0x7C,0x66,0x42,0x42,0x42,0x42,0x42,0x00,0x00,0x00],  # 78  'n'
    [0x00,0x00,0x00,0x3C,0x66,0x42,0xC3,0x42,0x66,0x3C,0x00,0x00,0x00],  # 79  'o'
    [0x00,0x7C,0x66,0x42,0x43,0x43,0x62,0x7E,0x48,0x00,0x00,0x00,0x00],  # 80  'p'
    [0x00,0x3E,0x66,0x42,0xC2,0xC2,0x46,0x7E,0x12,0x00,0x00,0x00,0x00],  # 81  'q'
    [0x37,0x3F,0x38,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x00],  # 82  'r'
    [0x00,0x00,0x3C,0x66,0x60,0x78,0x1E,0x02,0x66,0x3C,0x00,0x00,0x00],  # 83  's'
    [0x10,0x10,0x7E,0x3C,0x10,0x10,0x10,0x10,0x10,0x10,0x1C,0x0E,0x00],  # 84  't'
    [0x00,0x00,0x00,0x42,0x42,0x42,0x42,0x42,0x66,0x3E,0x00,0x00,0x00],  # 85  'u'
    [0x00,0x00,0xC3,0x42,0x66,0x24,0x24,0x3C,0x18,0x18,0x00,0x00,0x00],  # 86  'v'
    [0x00,0x00,0x00,0x00,0xDB,0x5A,0x5A,0x66,0x24,0x00,0x00,0x00,0x00],  # 87  'w'
    [0x00,0x00,0x62,0x26,0x3C,0x18,0x18,0x3C,0x66,0x42,0x00,0x00,0x00],  # 88  'x'
    [0x00,0xC3,0x42,0x66,0x24,0x3C,0x3C,0x18,0x18,0x00,0x00,0x00,0x00],  # 89  'y'
    [0x00,0x00,0x7E,0x06,0x0C,0x08,0x18,0x30,0x60,0x7E,0x00,0x00,0x00],  # 90  'z'
    [0x08,0x18,0x10,0x10,0x10,0x10,0x20,0x30,0x10,0x10,0x10,0x10,0x00],  # 91  '{'
    [0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x00,0x00],  # 92  '|'
    [0x20,0x30,0x10,0x10,0x10,0x18,0x08,0x18,0x10,0x10,0x10,0x10,0x00],  # 93  '}'
    [0x00,0x00,0x00,0x00,0x72,0x4E,0x00,0x00,0x00,0x00,0x00,0x00,0x00],  # 94  '~'
]

assert len(GLYPHS) == 95, f"Expected 95 glyphs, got {len(GLYPHS)}"

CHARS = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"

def fmt_row(b):
    return f"0x{b:02X}"

def gen_rust():
    """Generate Rust source for font.rs GLYPHS array."""
    lines = []
    lines.append("#[rustfmt::skip]")
    lines.append("static GLYPHS: [[u8; 13]; 95] = [")
    for i, glyph in enumerate(GLYPHS):
        row_strs = [fmt_row(b) for b in glyph]
        row_text = ",".join(row_strs)
        c = CHARS[i]
        label = f" /* {i:2d} {repr(c)} */"
        lines.append(f"    [{row_text}],{label}")
    lines.append("];")
    return "\n".join(lines) + "\n"

def gen_c_header():
    """Generate C header string for font_8x13.h"""
    lines = []
    lines.append("#ifndef FONT_8X13_H")
    lines.append("#define FONT_8X13_H")
    lines.append("")
    lines.append("#ifndef FONT_W")
    lines.append("  #define FONT_W 8")
    lines.append("#endif")
    lines.append("#ifndef FONT_H")
    lines.append("  #define FONT_H 13")
    lines.append("#endif")
    lines.append("#define FONT_NUM_GLYPHS 95")
    lines.append("#define FONT_FIRST_CHAR 32")
    lines.append("")
    lines.append("static const uint8_t font_8x13[95][13] = {")
    for i, glyph in enumerate(GLYPHS):
        row_strs = [fmt_row(b) for b in glyph]
        row_text = ", ".join(row_strs)
        c = CHARS[i]
        label = f"  /* {i:2d} {repr(c)} */"
        lines.append(f"    {{{row_text}}},{label}")
    lines.append("};")
    lines.append("")
    lines.append("#endif /* FONT_8X13_H */")
    return "\n".join(lines) + "\n"

# ── TrueType font generator ──────────────────────────────────────
# We generate a minimal TrueType font with monochrome bitmaps
# using the glyf table's simple glyph format.

UPM = 1024  # units per em

def gen_ttf():
    """Generate a minimal TrueType font and return raw bytes."""
    # Scale: each pixel column = UPM / 9 (8px glyph + 1 gap)
    # Each pixel row = UPM / 14 (13px glyph + 1 for descender room)
    PIX_W = UPM // 9
    PIX_H = UPM // 14
    ADVANCE = 9 * PIX_W  # advance width

    # Build glyph outlines from bitmaps
    # Each glyph has a single contour: the outline of the filled region
    # We use simple rectangular pixel outlines

    glyph_outlines = []  # list of (xMin,yMin,xMax,yMax, contour_points, end_pts)
    glyph_advances = []

    for glyph in GLYPHS:
        # Convert bitmap to outline
        # For each filled pixel, add a unit square
        points = []
        for row in range(13):
            bits = glyph[row]
            for col in range(8):
                if bits & (1 << (7 - col)):
                    # Pixel at (col, 12-row) in TrueType coords (y-up)
                    # Baseline is at row 10 (0-indexed) = rows 10-12 below baseline
                    y_base = (12 - row) * PIX_H - 2 * PIX_H  # descender offset
                    x0 = col * PIX_W
                    y0 = y_base
                    x1 = (col + 1) * PIX_W
                    y1 = y_base + PIX_H
                    points.append((x0, y0, x1, y1))

        if len(points) == 0:
            # Empty glyph (e.g. space) — no contours
            glyph_outlines.append(None)
            glyph_advances.append(ADVANCE)
            continue

        # Single contour: trace the outline using the pixel rects
        # For a set of rectangles, we create a single closed contour
        # The contour goes around the bounding hull of all pixels
        # We use the marching squares approach decomposed into pixel edges

        # Simple approach: each pixel is a separate contour
        # This creates multiple contours but is simpler
        contours = []
        for (x0, y0, x1, y1) in points:
            # 4 points clockwise: bottom-left -> bottom-right -> top-right -> top-left
            cntr = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
            contours.append(cntr)

        # Flatten all contour points
        all_pts = []
        end_pts = []
        for cntr in contours:
            start_idx = len(all_pts)
            all_pts.extend(cntr)
            end_pts.append(start_idx + len(cntr) - 1)

        # Compute bbox
        xs = [p[0] for c in contours for p in c]
        ys = [p[1] for c in contours for p in c]
        xMin = min(xs)
        yMin = min(ys)
        xMax = max(xs)
        yMax = max(ys)

        glyph_outlines.append((xMin, yMin, xMax, yMax, all_pts, end_pts))
        glyph_advances.append(ADVANCE)

    # Now build TTF binary
    return _build_ttf(glyph_outlines, glyph_advances)


def _calc_table_checksum(data):
    """Calculate the TTF table checksum (sum of uint32, padded to 4-byte boundary)."""
    if len(data) % 4 != 0:
        data = data + b'\x00' * (4 - len(data) % 4)
    s = 0
    for i in range(0, len(data), 4):
        s += struct.unpack_from('>I', data, i)[0]
        s &= 0xFFFFFFFF
    return s


def _build_ttf(glyph_outlines, glyph_advances):
    """Build a minimal TrueType font.

    Required tables: cmap, head, hhea, hmtx, maxp, name, OS/2, post, glyf, loca
    """
    # ── Build tables ────────────────────────────────────────

    # Offset Table
    sf_version = 0x00010000  # TrueType

    # Head table (54 bytes)
    head = b''
    head += struct.pack('>I', 0x00010000)  # table version
    head += struct.pack('>I', 0x000F0000)  # font revision 1.0
    head += struct.pack('>I', 0)           # checksum adjustment (placeholder)
    head += struct.pack('>I', 0x5F0F3CF5)  # magic number
    head += struct.pack('>H', 0x0400)      # flags
    head += struct.pack('>H', UPM)         # units per em
    head += struct.pack('>q', 0)           # created (long datetime)
    head += struct.pack('>q', 0)           # modified (long datetime)
    head += struct.pack('>h', 0)           # xMin
    head += struct.pack('>h', -2 * UPM // 14)  # yMin (descender)
    head += struct.pack('>h', 8 * UPM // 9)     # xMax
    head += struct.pack('>h', 11 * UPM // 14)   # yMax (ascender)
    head += struct.pack('>H', 0)           # macStyle
    head += struct.pack('>H', 8)           # lowestRecPPEM
    head += struct.pack('>h', 2)           # fontDirectionHint
    head += struct.pack('>h', 0)           # indexToLocFormat (short offsets)
    head += struct.pack('>h', 0)           # glyphDataFormat

    # Maxp table (32 bytes)
    maxp = b''
    maxp += struct.pack('>I', 0x00010000)  # table version
    maxp += struct.pack('>H', 95)          # numGlyphs
    maxp += struct.pack('>H', 95)          # maxPoints (generous)
    maxp += struct.pack('>H', 1)           # maxContours
    maxp += struct.pack('>H', 0)           # maxComponentPoints
    maxp += struct.pack('>H', 0)           # maxComponentContours
    maxp += struct.pack('>H', 0)           # maxZones
    maxp += struct.pack('>H', 0)           # maxTwilightPoints
    maxp += struct.pack('>H', 0)           # maxStorage
    maxp += struct.pack('>H', 0)           # maxFunctionDefs
    maxp += struct.pack('>H', 0)           # maxInstructionDefs
    maxp += struct.pack('>H', 0)           # maxStackElements
    maxp += struct.pack('>H', 0)           # maxSizeOfInstructions
    maxp += struct.pack('>H', 0)           # maxComponentElements
    maxp += struct.pack('>H', 0)           # maxComponentDepth

    # Hhea table (36 bytes)
    hhea = b''
    hhea += struct.pack('>I', 0x00010000)  # table version
    hhea += struct.pack('>h', 11 * UPM // 14)  # ascent
    hhea += struct.pack('>h', -2 * UPM // 14)  # descent
    hhea += struct.pack('>h', 0)           # lineGap
    hhea += struct.pack('>H', 0)           # advanceWidthMax
    hhea += struct.pack('>h', 0)           # minLeftSideBearing
    hhea += struct.pack('>h', 0)           # minRightSideBearing
    hhea += struct.pack('>h', 0)           # xMaxExtent
    hhea += struct.pack('>h', 1)           # caretSlopeRise
    hhea += struct.pack('>h', 0)           # caretSlopeRun
    hhea += struct.pack('>h', 0)           # caretOffset
    hhea += struct.pack('>h', 0) * 4       # reserved
    hhea += struct.pack('>h', 0)           # metricDataFormat
    hhea += struct.pack('>H', 95)          # numOfLongHorMetrics

    # Hmtx table (4 bytes per glyph: advance + lsb)
    hmtx = b''
    for i in range(95):
        hmtx += struct.pack('>H', glyph_advances[i])  # advance width
        hmtx += struct.pack('>h', 0)                   # lsb

    # Name table (minimal)
    name = b''
    name += struct.pack('>H', 0)       # format
    name += struct.pack('>H', 1)       # count
    name += struct.pack('>H', 6)       # stringOffset (after the 6-byte header)
    # NameRecord: platformID=1 (Mac), encodingID=0, languageID=0, nameID=1 (font family)
    name += struct.pack('>H', 1)       # platformID
    name += struct.pack('>H', 0)       # encodingID
    name += struct.pack('>H', 0)       # languageID
    name += struct.pack('>H', 1)       # nameID (font family)
    name += struct.pack('>H', 12)      # length
    name += struct.pack('>H', 0)       # offset
    name += b'Zirvium\0System\0'       # "Zirvium System" in Pascal string

    # OS/2 table (78 bytes, version 4)
    os2 = b''
    os2 += struct.pack('>H', 4)        # version
    os2 += struct.pack('>h', 11 * UPM // 14)  # xAvgCharWidth
    os2 += struct.pack('>H', 0x0020)   # usWeightClass (400 normal)
    os2 += struct.pack('>H', 5)        # usWidthClass
    os2 += struct.pack('>H', 0)        # fsType
    os2 += struct.pack('>h', 0)        # ySubscriptXSize
    os2 += struct.pack('>h', 0)        # ySubscriptYSize
    os2 += struct.pack('>h', 0)        # ySubscriptXOffset
    os2 += struct.pack('>h', 0)        # ySubscriptYOffset
    os2 += struct.pack('>h', 0)        # ySuperscriptXSize
    os2 += struct.pack('>h', 0)        # ySuperscriptYSize
    os2 += struct.pack('>h', 0)        # ySuperscriptXOffset
    os2 += struct.pack('>h', 0)        # ySuperscriptYOffset
    os2 += struct.pack('>h', 0)        # yStrikeoutSize
    os2 += struct.pack('>h', 0)        # yStrikeoutPosition
    os2 += struct.pack('>h', 0)        # sFamilyClass
    os2 += b'\x00' * 10                # panose
    os2 += struct.pack('>I', 0)        # ulUnicodeRange1
    os2 += struct.pack('>I', 0)        # ulUnicodeRange2
    os2 += struct.pack('>I', 0)        # ulUnicodeRange3
    os2 += struct.pack('>I', 0)        # ulUnicodeRange4
    os2 += b'Zirv'                     # achVendID
    os2 += struct.pack('>H', 0)        # fsSelection
    os2 += struct.pack('>H', 0x0020)   # usFirstCharIndex (space)
    os2 += struct.pack('>H', 0x007E)   # usLastCharIndex (~)
    os2 += struct.pack('>h', 0)        # sTypoAscender
    os2 += struct.pack('>h', 0)        # sTypoDescender
    os2 += struct.pack('>h', 0)        # sTypoLineGap
    os2 += struct.pack('>H', 0)        # usWinAscent
    os2 += struct.pack('>H', 0)        # usWinDescent
    os2 += struct.pack('>I', 0)        # ulCodePageRange1 (Latin 1)
    os2 += struct.pack('>I', 0)        # ulCodePageRange2
    os2 += struct.pack('>h', 0)        # sxHeight
    os2 += struct.pack('>h', 0)        # sCapHeight
    os2 += struct.pack('>H', 0)        # usDefaultChar
    os2 += struct.pack('>H', 0)        # usBreakChar
    os2 += struct.pack('>H', 3)        # usMaxContext

    # Post table (32 bytes, version 2.0)
    post = b''
    post += struct.pack('>I', 0x00020000)  # format 2.0
    post += struct.pack('>I', 0)          # italicAngle
    post += struct.pack('>h', 0)          # underlinePosition
    post += struct.pack('>h', 0)          # underlineThickness
    post += struct.pack('>I', 0)          # isFixedPitch
    post += struct.pack('>I', 0)          # minMemType42
    post += struct.pack('>I', 0)          # maxMemType42
    post += struct.pack('>I', 0)          # minMemType1
    post += struct.pack('>I', 0)          # maxMemType1

    # Cmap table (format 4: segment mapping to delta values)
    # We need a cmap that maps Unicode 0x0020..0x007E to glyph indices 1..95
    # Glyph 0 is .notdef

    # Build format 4 subtable
    seg_count = 1  # one segment
    seg_count_x2 = seg_count * 2
    search_range = 2 * (2 ** int(seg_count_x2 ** 0.5).bit_length() - 1)
    # This is getting complex. Let me use format 0 (byte encoding) for simplicity
    # since we only need 95 glyphs in the 0x20-0x7E range.

    # Format 0: 262-byte array
    cmap_format0 = b''
    cmap_format0 += struct.pack('>H', 0)     # format
    cmap_format0 += struct.pack('>H', 262)   # length
    cmap_format0 += struct.pack('>H', 0)     # language
    # Glyph index array (256 entries, 0x20..0x7E = 1..95, rest = 0)
    for i in range(256):
        if 0x20 <= i <= 0x7E:
            cmap_format0 += struct.pack('B', i - 0x20 + 1)  # glyph 1=space, 2=! etc.
        else:
            cmap_format0 += struct.pack('B', 0)  # .notdef

    cmap = b''
    cmap += struct.pack('>H', 0)      # version
    cmap += struct.pack('>H', 1)      # numTables
    cmap += struct.pack('>H', 3)      # platformID (Windows)
    cmap += struct.pack('>H', 1)      # encodingID (Unicode BMP)
    cmap += struct.pack('>I', 12)     # offset to subtable
    cmap += cmap_format0

    # Glyf table: glyph data
    # index 0: .notdef (empty)
    # index 1..95: space..~

    glyf_data = []

    # Glyph 0: .notdef (empty, invisible)
    glyf_data.append(b'\x00' * 10)  # numberOfContours=0, bbox all zero (10 bytes)

    for i, outline in enumerate(glyph_outlines):
        if outline is None:
            # Empty glyph (space) - single contour of zero size
            glyf_data.append(b'\x00' * 10)
            continue

        xMin, yMin, xMax, yMax, points, end_pts = outline
        n_contours = len(end_pts)
        n_points = len(points)

        # Glyph header (10 bytes)
        glyf = struct.pack('>h', n_contours)
        glyf += struct.pack('>h', xMin)
        glyf += struct.pack('>h', yMin)
        glyf += struct.pack('>h', xMax)
        glyf += struct.pack('>h', yMax)

        # End points of contours
        for ep in end_pts:
            glyf += struct.pack('>H', ep)

        # Instructions (empty)
        glyf += struct.pack('>H', 0)  # instruction length

        # Flags: all on-curve points (0x01)
        for _ in points:
            glyf += struct.pack('B', 0x01)

        # X coordinates
        for (x, y) in points:
            glyf += struct.pack('>h', x)

        # Y coordinates
        for (x, y) in points:
            glyf += struct.pack('>h', y)

        glyf_data.append(glyf)

    # Build loca table (short offsets, format 0)
    loca = b''
    offset = 0
    for gd in glyf_data:
        loca += struct.pack('>H', offset // 2)  # short format: offset/2
        offset += len(gd)
    loca += struct.pack('>H', offset // 2)

    # Concatenate glyf table data
    glyf_table = b''.join(glyf_data)

    # ── Build TTF file ───────────────────────────────────────
    tables = [
        (b'cmap', cmap),
        (b'head', head),
        (b'hhea', hhea),
        (b'hmtx', hmtx),
        (b'maxp', maxp),
        (b'name', name),
        (b'OS/2', os2),
        (b'post', post),
        (b'glyf', glyf_table),
        (b'loca', loca),
    ]

    # Sort tables alphabetically by tag
    tables.sort(key=lambda x: x[0])

    num_tables = len(tables)
    entry_selector = num_tables.bit_length() - 1
    search_range = (1 << entry_selector) * 16
    range_shift = num_tables * 16 - search_range

    # Offset table
    offset_table = struct.pack('>H', sf_version >> 16)
    offset_table += struct.pack('>H', sf_version & 0xFFFF)
    offset_table += struct.pack('>H', num_tables)
    offset_table += struct.pack('>H', search_range)
    offset_table += struct.pack('>H', entry_selector)
    offset_table += struct.pack('>H', range_shift)

    # Table directory
    data_start = 12 + num_tables * 16
    current_offset = data_start

    directory = b''
    table_data = b''
    for tag, data in tables:
        # Pad to 4 bytes
        padded = data
        if len(padded) % 4 != 0:
            padded += b'\x00' * (4 - len(padded) % 4)

        checksum = _calc_table_checksum(data)
        directory += tag
        directory += struct.pack('>I', checksum)
        directory += struct.pack('>I', current_offset)
        directory += struct.pack('>I', len(data))

        table_data += padded
        current_offset += len(padded)

    ttf = offset_table + directory + table_data

    # Compute and set head checksum adjustment
    file_checksum = _calc_table_checksum(ttf)
    # The checksum adjustment = 0xB1B0AFBA - file_checksum (mod 2^32)
    adj = (0xB1B0AFBA - file_checksum) & 0xFFFFFFFF

    # Patch head table with checksum adjustment
    # head starts at offset_table + directory + head_table_offset_relative_to_data
    # head is the second table in the sorted list (cmap, head, ...)
    head_offset = None
    curr = data_start
    for tag, _ in tables:
        if tag == b'head':
            head_offset = curr
            break
        padded_len = len(dict(tables)[tag]) if False else None
        # We need to recompute
        # Let's find the offset from the directory
        break

    # Actually, let's just find head's offset from the directory we already built
    curr_off = data_start
    for tag, data in tables:
        padded = data
        if len(padded) % 4 != 0:
            padded += b'\x00' * (4 - len(padded) % 4)
        if tag == b'head':
            head_offset = curr_off
            break
        curr_off += len(padded)

    if head_offset is not None:
        # Patch bytes 8-11 of head (checksum adjustment)
        ttf_list = bytearray(ttf)
        struct.pack_into('>I', ttf_list, head_offset + 8, adj)
        ttf = bytes(ttf_list)

    return ttf


# ── Main ──────────────────────────────────────────────────────
if __name__ == '__main__':
    import sys

    out_c = os.path.join(os.path.dirname(OUT_DIR), 'zirvui', 'src', 'font_8x13.h')
    out_ttf = os.path.join(OUT_DIR, 'zirvium_system.ttf')

    # Generate Rust
    rust = gen_rust()
    with open(out_c, 'w') as f:
        f.write(gen_c_header())
    print(f"Generated C header: {out_c}")

    # Generate TTF
    ttf_data = gen_ttf()
    with open(out_ttf, 'wb') as f:
        f.write(ttf_data)
    print(f"Generated TTF: {out_ttf} ({len(ttf_data)} bytes)")

    # Print Rust to stdout
    print("\n── Rust font data ──")
    print(rust)
