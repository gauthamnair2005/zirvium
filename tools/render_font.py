#!/usr/bin/env python3
"""Render Inter font characters to 8x13 bitmap glyphs, update genfont.py and regenerate."""

from PIL import Image, ImageDraw, ImageFont
import os
import sys

FONT_PATH = "/tmp/inter/InterVariable.ttf"
FONT_W = 8
FONT_H = 13
CHARS = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"

OUT_DIR = os.path.dirname(os.path.abspath(__file__))
GENFONT_PY = os.path.join(OUT_DIR, "genfont.py")
OUT_C = os.path.join(os.path.dirname(OUT_DIR), "zirvui", "src", "font_8x13.h")
OUT_TTF_ZIRV = os.path.join(OUT_DIR, "zirvium_system.ttf")
OUT_RUST = os.path.join(os.path.dirname(OUT_DIR), "zirvtk", "src", "font.rs")


def glyph_to_rows(arr_2d):
    rows = []
    for y in range(FONT_H):
        byte_val = 0
        for x in range(FONT_W):
            if y < len(arr_2d) and x < len(arr_2d[y]) and arr_2d[y][x]:
                byte_val |= (1 << (7 - x))
        rows.append(byte_val)
    return rows


def render_glyph(font, char):
    render_size = 64
    img = Image.new("L", (render_size, render_size), 0)
    draw = ImageDraw.Draw(img)
    draw.text((0, 0), char, font=font, fill=255)

    bbox = font.getbbox(char)
    if not bbox or (bbox[2] - bbox[0] <= 0) or (bbox[3] - bbox[1] <= 0):
        return [[0] * FONT_W for _ in range(FONT_H)]

    x0, y0, x1, y1 = bbox
    bw = x1 - x0
    bh = y1 - y0

    glyph_img = img.crop((x0, y0, x1, y1))

    scale = min(FONT_W / max(bw, 1), FONT_H / max(bh, 1))
    new_w = max(1, int(bw * scale))
    new_h = max(1, int(bh * scale))

    glyph_img = glyph_img.resize((new_w, new_h), Image.LANCZOS)

    raw = list(glyph_img.getdata())
    arr = [[1 if raw[y * new_w + x] > 100 else 0 for x in range(new_w)] for y in range(new_h)]

    out = [[0] * FONT_W for _ in range(FONT_H)]
    x_off = (FONT_W - new_w) // 2
    y_off = (FONT_H - new_h) // 2

    for y in range(new_h):
        for x in range(new_w):
            if arr[y][x]:
                py = y_off + y
                px = x_off + x
                if 0 <= py < FONT_H and 0 <= px < FONT_W:
                    out[py][px] = 1
    return out


def update_genfont_py(new_glyphs):
    with open(GENFONT_PY, "r") as f:
        content = f.read()

    glyph_start = content.find("GLYPHS = [")
    if glyph_start < 0:
        print("ERROR: Could not find GLYPHS line")
        sys.exit(1)

    bracket_start = content.find("[", glyph_start)
    depth = 0
    close_idx = -1
    for j in range(bracket_start, len(content)):
        if content[j] == "[":
            depth += 1
        elif content[j] == "]":
            depth -= 1
            if depth == 0:
                close_idx = j
                break

    if close_idx < 0:
        print("ERROR: Could not find closing bracket")
        sys.exit(1)

    lines = []
    lines.append("GLYPHS = [")
    lines.append("    # 32  space")
    lines.append("    [0x00]*13,")
    for i in range(1, 95):
        glyph = new_glyphs[i]
        row_strs = [f"0x{b:02X}" for b in glyph_to_rows(glyph)]
        row_text = ",".join(row_strs)
        c = CHARS[i]
        label = f"  # {i:2d}  {repr(c)}"
        lines.append(f"    [{row_text}],{label}")
    lines.append("]")

    new_text = "\n".join(lines)
    new_content = content[:glyph_start] + new_text + content[close_idx + 1:]

    with open(GENFONT_PY, "w") as f:
        f.write(new_content)
    print(f"Updated {GENFONT_PY}")


def gen_rust(glyphs_rows):
    """Generate full Rust font.rs source."""
    lines = []
    lines.append("/// 8×13 bitmap font glyph data, ASCII 32–126.")
    lines.append("pub const FONT_W: u32 = 8;")
    lines.append("pub const FONT_H: u32 = 13;")
    lines.append("")
    lines.append("/// Return the pixel width of a string in the built-in font.")
    lines.append("pub fn text_width(text: &str) -> u32 {")
    lines.append("    let chars = text.chars().count() as u32;")
    lines.append("    chars * (FONT_W + 1)")
    lines.append("}")
    lines.append("")
    lines.append("/// Return the line height for the built-in font.")
    lines.append("pub fn line_height() -> u32 {")
    lines.append("    FONT_H + 3")
    lines.append("}")
    lines.append("")
    lines.append("/// Return the glyph bitmap for a character (or space for out-of-range).")
    lines.append("pub fn font_get(c: char) -> &'static [u8; FONT_H as usize] {")
    lines.append("    let idx = (c as usize).wrapping_sub(32);")
    lines.append("    if idx < GLYPHS.len() {")
    lines.append("        &GLYPHS[idx]")
    lines.append("    } else {")
    lines.append("        &GLYPHS[0]")
    lines.append("    }")
    lines.append("}")
    lines.append("")
    lines.append("#[rustfmt::skip]")
    lines.append("static GLYPHS: [[u8; 13]; 95] = [")
    for i, rows in enumerate(glyphs_rows):
        row_strs = [f"0x{b:02X}" for b in glyph_to_rows(rows)]
        row_text = ",".join(row_strs)
        c = CHARS[i]
        label = f" /* {i:2d} {repr(c)} */"
        lines.append(f"    [{row_text}],{label}")
    lines.append("];")
    return "\n".join(lines) + "\n"


def main():
    font = ImageFont.truetype(FONT_PATH, 64)

    print("Rendering glyphs...")
    new_glyphs = []
    for i, ch in enumerate(CHARS):
        arr = render_glyph(font, ch)
        new_glyphs.append(arr)
        sys.stdout.write(f"\r  [{i:2d}/94] '{ch}'")
        sys.stdout.flush()
    print()

    update_genfont_py(new_glyphs)

    # Import updated genfont module
    sys.path.insert(0, OUT_DIR)
    import importlib.util
    spec = importlib.util.spec_from_file_location("genfont", GENFONT_PY)
    genfont = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(genfont)

    # C header
    c_header = genfont.gen_c_header()
    with open(OUT_C, "w") as f:
        f.write(c_header)
    print(f"Generated: {OUT_C}")

    # TTF
    ttf_data = genfont.gen_ttf()
    with open(OUT_TTF_ZIRV, "wb") as f:
        f.write(ttf_data)
    print(f"Generated: {OUT_TTF_ZIRV} ({len(ttf_data)} bytes)")

    # Rust - write complete file with API
    rust = gen_rust(new_glyphs)
    with open(OUT_RUST, "w") as f:
        f.write(rust)
    print(f"Generated: {OUT_RUST}")

    print("\nDone!")


if __name__ == "__main__":
    main()
