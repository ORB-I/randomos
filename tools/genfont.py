#!/usr/bin/env python3
"""Generate an 8x16 monospace bitmap font as a C header for userspace GUI.

Renders ASCII 0x20-0x7E from DejaVu Sans Mono, anti-aliased, thresholded
to 1-bit. Output is a packed bit array (MSB-first within each byte).
1 = pixel on. Each glyph is exactly GUI_FONT_H bytes (one per row).
"""
from PIL import Image, ImageDraw, ImageFont

FONT_PATH = "/usr/share/fonts/TTF/DejaVuSansMono.ttf"
GLYPH_W = 8
GLYPH_H = 16
FIRST = 0x20
LAST = 0x7E

font = ImageFont.truetype(FONT_PATH, GLYPH_H)

glyphs = []
for code in range(FIRST, LAST + 1):
    ch = chr(code)
    img = Image.new("L", (GLYPH_W, GLYPH_H), 0)
    draw = ImageDraw.Draw(img)
    draw.text((0, 0), ch, font=font, fill=255)
    px = img.load()
    rows = []
    for y in range(GLYPH_H):
        byte = 0
        bit = 0
        for x in range(GLYPH_W):
            if px[x, y] >= 128:
                byte |= (1 << (7 - bit))
            bit += 1
            if bit == 8:
                rows.append(byte)
                byte = 0
                bit = 0
        if bit != 0:
            rows.append(byte)
    glyphs.append((code, rows))

out = []
out.append("/* Auto-generated 8x16 bitmap font (DejaVu Sans Mono), ASCII 0x20-0x7E.")
out.append("   Bitpacked MSB-first. 1 = pixel on. */")
out.append("#pragma once")
out.append("")
out.append("#define GUI_FONT_W 8")
out.append("#define GUI_FONT_H 16")
out.append("#define GUI_FONT_FIRST 0x20")
out.append("")
out.append("const u8 gui_font[256][GUI_FONT_H] = {")
for code in range(256):
    rows = None
    for gcode, grow in glyphs:
        if gcode == code:
            rows = grow
            break
    if rows is None:
        rows = [0] * GLYPH_H
    assert len(rows) == GLYPH_H, (code, len(rows))
    out.append("    {" + ",".join(str(b) for b in rows) + "},")
out.append("};")
out.append("")

with open("/tmp/gui_font.h", "w") as f:
    f.write("\n".join(out))

print("Wrote /tmp/gui_font.h with %d glyphs" % (LAST - FIRST + 1))
