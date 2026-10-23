//! Anti-aliased font atlas parsing and text metrics.
//!
//! The atlas is generated at build time by `tools/genfont.py` from DejaVu Sans
//! (Bitstream Vera licence, see `assets/FONT-LICENSE.txt`).  Each face stores
//! 95 ASCII glyphs as 8-bit coverage masks, which DisplayJet alpha-blends into
//! the surface.  Using coverage masks instead of a bitmap font gives clean
//! text at every size and lets the UI use the same renderer on CPU and on a
//! future GPU backend.

use alloc::vec::Vec;

use zirvstdlib::sync::Once;

const ATLAS: &[u8] = include_bytes!("../../../assets/font.bin");
const MAGIC: &[u8; 4] = b"ZFNT";

#[derive(Clone, Copy, Debug)]
pub struct Glyph {
    pub code: u32,
    pub advance: u32,
    pub bearing_x: i32,
    /// Offset from the baseline; negative values are above it.
    pub bearing_y: i32,
    pub width: i32,
    pub height: i32,
    pub offset: u32,
    pub len: u32,
}

#[derive(Debug)]
pub struct Face {
    pub size: u32,
    pub bold: bool,
    pub ascent: i32,
    pub descent: i32,
    pub line_height: i32,
    pub glyphs: Vec<Glyph>,
    data_start: usize,
}

impl Face {
    pub fn glyph(&self, character: char) -> Option<&Glyph> {
        let code = character as u32;
        match self.glyphs.binary_search_by_key(&code, |glyph| glyph.code) {
            Ok(index) => self.glyphs.get(index),
            Err(_) => self
                .glyphs
                .iter()
                .find(|glyph| glyph.code == b'?' as u32),
        }
    }

    /// The 8-bit coverage mask for a glyph.
    pub fn coverage(&self, glyph: &Glyph) -> &[u8] {
        let start = self.data_start + glyph.offset as usize;
        let end = start + glyph.len as usize;
        ATLAS.get(start..end).unwrap_or(&[])
    }

    /// Width of a string in pixels.
    pub fn width(&self, text: &str) -> i32 {
        let mut width = 0;
        for character in text.chars() {
            if let Some(glyph) = self.glyph(character) {
                width += glyph.advance as i32;
            } else {
                width += self.size as i32 / 2;
            }
        }
        width
    }

    /// Width of a prefix, used for caret positioning.
    pub fn prefix_width(&self, text: &str, count: usize) -> i32 {
        let mut width = 0;
        for (index, character) in text.chars().enumerate() {
            if index >= count {
                break;
            }
            if let Some(glyph) = self.glyph(character) {
                width += glyph.advance as i32;
            }
        }
        width
    }

    /// Number of characters that fit in `width` pixels.
    pub fn fit_chars(&self, text: &str, width: i32) -> usize {
        let mut used = 0;
        for (index, character) in text.chars().enumerate() {
            let advance = self
                .glyph(character)
                .map(|glyph| glyph.advance as i32)
                .unwrap_or(self.size as i32 / 2);
            if used + advance > width {
                return index;
            }
            used += advance;
        }
        text.chars().count()
    }
}

pub struct FontSet {
    pub faces: Vec<Face>,
}

static FONTS: Once<FontSet> = Once::new();

fn read_u32(bytes: &[u8], offset: usize) -> u32 {
    let mut value = [0u8; 4];
    if let Some(slice) = bytes.get(offset..offset + 4) {
        value.copy_from_slice(slice);
    }
    u32::from_le_bytes(value)
}

fn read_i32(bytes: &[u8], offset: usize) -> i32 {
    read_u32(bytes, offset) as i32
}

fn parse() -> FontSet {
    let mut faces = Vec::new();
    if ATLAS.len() < 12 || &ATLAS[0..4] != MAGIC {
        return FontSet { faces };
    }
    let count = read_u32(ATLAS, 8) as usize;
    let mut position = 12;
    for _ in 0..count {
        if ATLAS.len() < position + 24 {
            break;
        }
        let weight = read_u32(ATLAS, position);
        let size = read_u32(ATLAS, position + 4);
        let ascent = read_i32(ATLAS, position + 8);
        let descent = read_i32(ATLAS, position + 12);
        let line_height = read_i32(ATLAS, position + 16);
        let glyph_count = read_u32(ATLAS, position + 20) as usize;
        position += 24;

        let mut glyphs = Vec::with_capacity(glyph_count);
        for _ in 0..glyph_count {
            if ATLAS.len() < position + 32 {
                break;
            }
            glyphs.push(Glyph {
                code: read_u32(ATLAS, position),
                advance: read_u32(ATLAS, position + 4),
                bearing_x: read_i32(ATLAS, position + 8),
                bearing_y: read_i32(ATLAS, position + 12),
                width: read_i32(ATLAS, position + 16),
                height: read_i32(ATLAS, position + 20),
                offset: read_u32(ATLAS, position + 24),
                len: read_u32(ATLAS, position + 28),
            });
            position += 32;
        }

        let mut data_len = 0usize;
        for glyph in &glyphs {
            data_len = data_len.max(glyph.offset as usize + glyph.len as usize);
        }
        let data_start = position;
        position += data_len;

        faces.push(Face {
            size,
            bold: weight != 0,
            ascent,
            descent,
            line_height,
            glyphs,
            data_start,
        });
    }
    FontSet { faces }
}

/// Parse the embedded atlas.  Safe to call more than once.
pub fn init() {
    let set = FONTS.call_once(parse);
    crate::sprintln!(
        "FONT: {} faces, {} glyphs each, {} byte atlas",
        set.faces.len(),
        set.faces.first().map(|face| face.glyphs.len()).unwrap_or(0),
        ATLAS.len()
    );
}

/// Select the face closest to the requested size and weight.
pub fn face(size: u32, bold: bool) -> &'static Face {
    let set = FONTS.get().unwrap_or_else(|| FONTS.call_once(parse));
    let mut best: Option<&Face> = None;
    let mut best_score = i32::MAX;
    for candidate in &set.faces {
        let score = (candidate.size as i32 - size as i32).abs() * 4
            + if candidate.bold == bold { 0 } else { 1 };
        if score < best_score {
            best_score = score;
            best = Some(candidate);
        }
    }
    best.unwrap_or_else(|| &set.faces[0])
}

pub fn faces() -> &'static [Face] {
    let set = FONTS.get().unwrap_or_else(|| FONTS.call_once(parse));
    &set.faces
}
