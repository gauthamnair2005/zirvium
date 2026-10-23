//! Small drawing helpers shared by the desktop chrome and the applications.

use alloc::string::String;
use alloc::vec::Vec;

use zirvstdlib::geometry::{Point, Rect};
use zirvstdlib::math;

use crate::displayjet::Display;
use crate::displayjet::font::Face;
use crate::ui::theme::*;

/// Alpha-blend `source` over `destination` with an explicit 0..=255 alpha.
#[inline]
pub fn blend(source: u32, destination: u32, alpha: u32) -> u32 {
    if alpha == 0 {
        return destination;
    }
    if alpha >= 255 {
        return (source & 0x00ff_ffff) | 0xff00_0000;
    }
    let inverse = 255 - alpha;
    let rb =
        (((source & 0x00ff_00ff) * alpha + (destination & 0x00ff_00ff) * inverse + 0x0080_0080)
            >> 8)
            & 0x00ff_00ff;
    let g =
        (((source & 0x0000_ff00) * alpha + (destination & 0x0000_ff00) * inverse + 0x0000_8000)
            >> 8)
            & 0x0000_ff00;
    rb | g
}

pub fn lerp_color(first: u32, second: u32, t: f32) -> u32 {
    let t = t.clamp(0.0, 1.0);
    let channel = |shift: u32| -> u32 {
        let a = ((first >> shift) & 0xff) as f32;
        let b = ((second >> shift) & 0xff) as f32;
        (a + (b - a) * t) as u32 & 0xff
    };
    argb(0xff, (channel(16) << 16) | (channel(8) << 8) | channel(0))
}

/// A line rasterised as a sequence of small squares.
pub fn line(display: &mut Display, x0: i32, y0: i32, x1: i32, y1: i32, thickness: i32, color: u32) {
    let dx = x1 - x0;
    let dy = y1 - y0;
    let steps = dx.abs().max(dy.abs()).max(1);
    let half = (thickness / 2).max(0);
    for step in 0..=steps {
        let x = x0 + dx * step / steps;
        let y = y0 + dy * step / steps;
        display.fill_rect(
            Rect::new(x - half, y - half, thickness.max(1), thickness.max(1)),
            color,
        );
    }
}

pub fn circle_outline(
    display: &mut Display,
    center: Point,
    radius: i32,
    thickness: i32,
    color: u32,
) {
    let rect = Rect::new(center.x - radius, center.y - radius, radius * 2, radius * 2);
    display.stroke_rounded(rect, radius, thickness, color);
}

pub fn panel(display: &mut Display, rect: Rect, radius: i32, fill: u32, border: u32) {
    display.fill_rounded(rect, radius, fill);
    if border >> 24 != 0 {
        display.stroke_rounded(rect, radius, 1, border);
    }
    if rect.h >= 24 {
        // A one-pixel inner highlight along the top edge suggests a bevel
        // without adding a second visible border.
        let inset = radius.max(4);
        if rect.w > inset * 2 {
            display.fill_rect(
                Rect::new(rect.x + inset, rect.y + 1, rect.w - inset * 2, 1),
                argb(0x12, 0xffffff),
            );
        }
    }
}

/// A rounded, tinted card with a hairline border and a faint accent edge.
pub fn card(display: &mut Display, rect: Rect, theme: &Theme) {
    display.fill_rounded_vertical(rect, 11, PANEL_SOFT, argb(0xff, 0x141821));
    display.stroke_rounded(rect, 11, 1, BORDER_SOFT);
    display.horizontal_gradient(
        Rect::new(rect.x + 12, rect.y + 1, (rect.w - 24).max(1), 1),
        theme.accent_soft(70),
        theme.accent_alt_soft(22),
    );
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Icon {
    Logo,
    Terminal,
    Files,
    Monitor,
    Settings,
    About,
    Nodes,
    Close,
    Minimize,
    Maximize,
    Up,
    Down,
    Right,
    Search,
    Cpu,
    Memory,
    Gpu,
    Clock,
    Plus,
    Check,
    Power,
    Spark,
    Document,
}

/// Draw a vector icon inside `rect`.
pub fn icon(display: &mut Display, rect: Rect, kind: Icon, color: u32) {
    let size = rect.w.min(rect.h);
    if size <= 0 {
        return;
    }
    let scale = size as f32;
    let ox = rect.x as f32 + (rect.w - size) as f32 * 0.5;
    let oy = rect.y as f32 + (rect.h - size) as f32 * 0.5;
    let x = |value: f32| (ox + value * scale) as i32;
    let y = |value: f32| (oy + value * scale) as i32;
    let thickness = ((scale * 0.085) as i32).max(1);
    let thin = ((scale * 0.065) as i32).max(1);

    match kind {
        Icon::Logo => {
            line(
                display,
                x(0.26),
                y(0.28),
                x(0.74),
                y(0.28),
                thickness,
                color,
            );
            line(
                display,
                x(0.74),
                y(0.28),
                x(0.26),
                y(0.72),
                thickness,
                color,
            );
            line(
                display,
                x(0.26),
                y(0.72),
                x(0.74),
                y(0.72),
                thickness,
                color,
            );
            line(
                display,
                x(0.26),
                y(0.28),
                x(0.30),
                y(0.28),
                thickness,
                color,
            );
        }
        Icon::Terminal => {
            let frame = Rect::new(
                x(0.14),
                y(0.18),
                (scale * 0.72) as i32,
                (scale * 0.64) as i32,
            );
            display.stroke_rounded(frame, (scale * 0.10) as i32, thin, color);
            line(display, x(0.30), y(0.39), x(0.41), y(0.50), thin, color);
            line(display, x(0.41), y(0.50), x(0.30), y(0.61), thin, color);
            line(display, x(0.52), y(0.61), x(0.68), y(0.61), thin, color);
        }
        Icon::Files => {
            let folder = Rect::new(
                x(0.14),
                y(0.28),
                (scale * 0.72) as i32,
                (scale * 0.50) as i32,
            );
            display.stroke_rounded(folder, (scale * 0.07) as i32, thin, color);
            line(display, x(0.14), y(0.36), x(0.48), y(0.36), thin, color);
            line(display, x(0.30), y(0.28), x(0.30), y(0.22), thin, color);
            line(display, x(0.30), y(0.22), x(0.46), y(0.22), thin, color);
            line(display, x(0.46), y(0.22), x(0.46), y(0.28), thin, color);
        }
        Icon::Monitor => {
            let screen = Rect::new(
                x(0.12),
                y(0.16),
                (scale * 0.76) as i32,
                (scale * 0.54) as i32,
            );
            display.stroke_rounded(screen, (scale * 0.08) as i32, thin, color);
            line(display, x(0.50), y(0.70), x(0.50), y(0.80), thin, color);
            line(display, x(0.34), y(0.82), x(0.66), y(0.82), thin, color);
            line(display, x(0.24), y(0.56), x(0.36), y(0.42), thin, color);
            line(display, x(0.36), y(0.42), x(0.48), y(0.52), thin, color);
            line(display, x(0.48), y(0.52), x(0.60), y(0.32), thin, color);
            line(display, x(0.60), y(0.32), x(0.76), y(0.32), thin, color);
        }
        Icon::Settings => {
            line(display, x(0.20), y(0.32), x(0.80), y(0.32), thin, color);
            display.fill_circle(Point::new(x(0.38), y(0.32)), (scale * 0.075) as i32, color);
            line(display, x(0.20), y(0.50), x(0.80), y(0.50), thin, color);
            display.fill_circle(Point::new(x(0.64), y(0.50)), (scale * 0.075) as i32, color);
            line(display, x(0.20), y(0.68), x(0.80), y(0.68), thin, color);
            display.fill_circle(Point::new(x(0.46), y(0.68)), (scale * 0.075) as i32, color);
        }
        Icon::About => {
            circle_outline(
                display,
                Point::new(x(0.50), y(0.50)),
                (scale * 0.32) as i32,
                thin,
                color,
            );
            display.fill_circle(Point::new(x(0.50), y(0.34)), (scale * 0.05) as i32, color);
            line(
                display,
                x(0.50),
                y(0.46),
                x(0.50),
                y(0.66),
                thickness,
                color,
            );
        }
        Icon::Nodes => {
            let radius = (scale * 0.095) as i32;
            let a = Point::new(x(0.28), y(0.28));
            let b = Point::new(x(0.72), y(0.30));
            let c = Point::new(x(0.50), y(0.74));
            line(display, a.x, a.y, b.x, b.y, thin, color);
            line(display, a.x, a.y, c.x, c.y, thin, color);
            line(display, b.x, b.y, c.x, c.y, thin, color);
            circle_outline(display, a, radius, thin, color);
            circle_outline(display, b, radius, thin, color);
            circle_outline(display, c, radius, thin, color);
        }
        Icon::Close => {
            line(display, x(0.34), y(0.34), x(0.66), y(0.66), thin, color);
            line(display, x(0.66), y(0.34), x(0.34), y(0.66), thin, color);
        }
        Icon::Minimize => {
            line(display, x(0.32), y(0.55), x(0.68), y(0.55), thin, color);
        }
        Icon::Maximize => {
            let frame = Rect::new(
                x(0.34),
                y(0.34),
                (scale * 0.32) as i32,
                (scale * 0.32) as i32,
            );
            display.stroke_rounded(frame, (scale * 0.04) as i32, thin, color);
        }
        Icon::Up => {
            line(display, x(0.32), y(0.60), x(0.50), y(0.40), thin, color);
            line(display, x(0.50), y(0.40), x(0.68), y(0.60), thin, color);
        }
        Icon::Down => {
            line(display, x(0.32), y(0.40), x(0.50), y(0.60), thin, color);
            line(display, x(0.50), y(0.60), x(0.68), y(0.40), thin, color);
        }
        Icon::Right => {
            line(display, x(0.40), y(0.32), x(0.60), y(0.50), thin, color);
            line(display, x(0.60), y(0.50), x(0.40), y(0.68), thin, color);
        }
        Icon::Search => {
            circle_outline(
                display,
                Point::new(x(0.44), y(0.44)),
                (scale * 0.22) as i32,
                thin,
                color,
            );
            line(
                display,
                x(0.59),
                y(0.59),
                x(0.76),
                y(0.76),
                thickness,
                color,
            );
        }
        Icon::Cpu => {
            let body = Rect::new(
                x(0.28),
                y(0.28),
                (scale * 0.44) as i32,
                (scale * 0.44) as i32,
            );
            display.stroke_rounded(body, (scale * 0.05) as i32, thin, color);
            let inner = Rect::new(
                x(0.40),
                y(0.40),
                (scale * 0.20) as i32,
                (scale * 0.20) as i32,
            );
            display.fill_rounded(inner, (scale * 0.03) as i32, color);
            for offset in [0.36, 0.50, 0.64] {
                line(display, x(offset), y(0.16), x(offset), y(0.28), thin, color);
                line(display, x(offset), y(0.72), x(offset), y(0.84), thin, color);
                line(display, x(0.16), y(offset), x(0.28), y(offset), thin, color);
                line(display, x(0.72), y(offset), x(0.84), y(offset), thin, color);
            }
        }
        Icon::Memory => {
            for (index, height) in [0.34f32, 0.54, 0.44, 0.64].iter().enumerate() {
                let left = x(0.22 + index as f32 * 0.16);
                let top = y(0.80 - height);
                let width = (scale * 0.10) as i32;
                let body = Rect::new(left, top, width, (scale * *height) as i32);
                display.fill_rounded(body, (scale * 0.03) as i32, color);
            }
        }
        Icon::Gpu => {
            let body = Rect::new(
                x(0.16),
                y(0.26),
                (scale * 0.68) as i32,
                (scale * 0.48) as i32,
            );
            display.stroke_rounded(body, (scale * 0.07) as i32, thin, color);
            circle_outline(
                display,
                Point::new(x(0.44), y(0.50)),
                (scale * 0.11) as i32,
                thin,
                color,
            );
            line(display, x(0.30), y(0.82), x(0.40), y(0.74), thin, color);
            line(display, x(0.50), y(0.82), x(0.60), y(0.74), thin, color);
        }
        Icon::Clock => {
            circle_outline(
                display,
                Point::new(x(0.50), y(0.50)),
                (scale * 0.32) as i32,
                thin,
                color,
            );
            line(display, x(0.50), y(0.50), x(0.50), y(0.31), thin, color);
            line(display, x(0.50), y(0.50), x(0.64), y(0.58), thin, color);
        }
        Icon::Plus => {
            line(
                display,
                x(0.50),
                y(0.28),
                x(0.50),
                y(0.72),
                thickness,
                color,
            );
            line(
                display,
                x(0.28),
                y(0.50),
                x(0.72),
                y(0.50),
                thickness,
                color,
            );
        }
        Icon::Check => {
            line(
                display,
                x(0.28),
                y(0.52),
                x(0.44),
                y(0.68),
                thickness,
                color,
            );
            line(
                display,
                x(0.44),
                y(0.68),
                x(0.74),
                y(0.34),
                thickness,
                color,
            );
        }
        Icon::Power => {
            circle_outline(
                display,
                Point::new(x(0.50), y(0.54)),
                (scale * 0.28) as i32,
                thin,
                color,
            );
            display.fill_rect(
                Rect::new(x(0.50) - thin / 2, y(0.50), thin, (scale * 0.16) as i32),
                argb(0, 0),
            );
            // Redraw the top gap in the accent colour, then the stem.
            line(
                display,
                x(0.50),
                y(0.24),
                x(0.50),
                y(0.52),
                thickness,
                color,
            );
        }
        Icon::Spark => {
            line(display, x(0.50), y(0.20), x(0.50), y(0.80), thin, color);
            line(display, x(0.20), y(0.50), x(0.80), y(0.50), thin, color);
            line(display, x(0.30), y(0.30), x(0.70), y(0.70), thin, color);
            line(display, x(0.70), y(0.30), x(0.30), y(0.70), thin, color);
        }
        Icon::Document => {
            let page = Rect::new(
                x(0.24),
                y(0.14),
                (scale * 0.46) as i32,
                (scale * 0.72) as i32,
            );
            display.stroke_rounded(page, (scale * 0.04) as i32, thin, color);
            line(display, x(0.58), y(0.14), x(0.70), y(0.26), thin, color);
            line(display, x(0.70), y(0.26), x(0.58), y(0.26), thin, color);
            line(display, x(0.58), y(0.26), x(0.58), y(0.14), thin, color);
            line(display, x(0.34), y(0.44), x(0.62), y(0.44), thin, color);
            line(display, x(0.34), y(0.56), x(0.62), y(0.56), thin, color);
            line(display, x(0.34), y(0.68), x(0.52), y(0.68), thin, color);
        }
    }
}

pub fn text(display: &mut Display, x: i32, y: i32, text: &str, size: u32, bold: bool, color: u32) {
    display.text(x, y, text, size, bold, color);
}

/// Draw text with a soft one-pixel shadow.  Reserved for wordmarks and large
/// headings where the extra depth pays for the second pass.
pub fn text_shadow(
    display: &mut Display,
    x: i32,
    y: i32,
    text: &str,
    size: u32,
    bold: bool,
    color: u32,
) {
    display.text(x, y + 1, text, size, bold, argb(0x80, 0x000000));
    display.text(x, y, text, size, bold, color);
}

pub fn text_width(text: &str, size: u32, bold: bool) -> i32 {
    crate::displayjet::font::face(size, bold).width(text)
}

/// Wrap text to `width` pixels, honouring explicit newlines and long words.
pub fn wrap(face: &Face, text: &str, width: i32) -> Vec<String> {
    let mut lines = Vec::new();
    let width = width.max(16);
    for paragraph in text.split('\n') {
        if paragraph.is_empty() {
            lines.push(String::new());
            continue;
        }
        let mut current = String::new();
        for word in paragraph.split(' ') {
            let candidate = if current.is_empty() {
                String::from(word)
            } else {
                alloc::format!("{current} {word}")
            };
            if face.width(&candidate) <= width {
                current = candidate;
                continue;
            }
            if !current.is_empty() {
                lines.push(current);
                current = String::new();
            }
            if face.width(word) <= width {
                current.push_str(word);
                continue;
            }
            // Break a word that is wider than the panel.
            let mut piece = String::new();
            for character in word.chars() {
                let mut probe = piece.clone();
                probe.push(character);
                if face.width(&probe) > width && !piece.is_empty() {
                    lines.push(piece);
                    piece = String::new();
                }
                piece.push(character);
            }
            current = piece;
        }
        lines.push(current);
    }
    lines
}

pub fn section_header(
    display: &mut Display,
    x: i32,
    y: i32,
    title: &str,
    subtitle: &str,
    theme: &Theme,
) {
    display.text(x, y, title, 19, true, TEXT);
    if !subtitle.is_empty() {
        display.text(x, y + 26, subtitle, 12, false, TEXT_MUTED);
    }
    let underline = Rect::new(x, y + 47, 38, 3);
    display.fill_rounded(underline, 2, theme.accent_soft(235));
    display.fill_rounded(
        Rect::new(underline.right() + 4, y + 47, 10, 3),
        2,
        theme.accent_alt_soft(120),
    );
}

#[derive(Clone, Copy, PartialEq, Eq)]
pub enum ButtonStyle {
    Primary,
    Subtle,
    Ghost,
    Danger,
}

pub fn button(
    display: &mut Display,
    rect: Rect,
    label: &str,
    theme: &Theme,
    style: ButtonStyle,
    hovered: bool,
    enabled: bool,
) {
    let (fill, fill_bottom, border, text_color) = if !enabled {
        (
            argb(0x40, 0x2a2f3a),
            argb(0x30, 0x222732),
            BORDER_SOFT,
            TEXT_FAINT,
        )
    } else {
        match style {
            ButtonStyle::Primary => (
                if hovered {
                    theme.accent
                } else {
                    theme.accent_soft(238)
                },
                if hovered {
                    theme.accent_alt_soft(255)
                } else {
                    theme.accent_soft(210)
                },
                argb(0, 0),
                rgb(0x06101f),
            ),
            ButtonStyle::Subtle => (
                if hovered { PANEL_RAISED } else { PANEL_SOFT },
                if hovered {
                    argb(0xff, 0x1d222d)
                } else {
                    argb(0xff, 0x151923)
                },
                BORDER,
                TEXT,
            ),
            ButtonStyle::Ghost => (
                if hovered {
                    argb(0x36, 0xffffff)
                } else {
                    argb(0, 0)
                },
                if hovered {
                    argb(0x18, 0xffffff)
                } else {
                    argb(0, 0)
                },
                argb(0, 0),
                TEXT_MUTED,
            ),
            ButtonStyle::Danger => (
                if hovered {
                    DANGER
                } else {
                    argb(0x38, 0xf07178)
                },
                if hovered {
                    argb(0xff, 0xd94a55)
                } else {
                    argb(0x24, 0xd94a55)
                },
                if hovered {
                    DANGER
                } else {
                    argb(0x70, 0xf07178)
                },
                if hovered { rgb(0x1a0608) } else { DANGER },
            ),
        }
    };
    display.fill_rounded_vertical(rect, 9, fill, fill_bottom);
    if border >> 24 != 0 {
        display.stroke_rounded(rect, 9, 1, border);
    }
    let face = crate::displayjet::font::face(13, true);
    let width = face.width(label);
    let x = rect.x + (rect.w - width) / 2;
    let y = rect.y + (rect.h - face.line_height) / 2;
    display.text_face(x, y + face.ascent, label, face, text_color);
}

pub fn pill(display: &mut Display, center: Point, label: &str, fg: u32, bg: u32) {
    let face = crate::displayjet::font::face(11, true);
    let width = face.width(label) + 18;
    let rect = Rect::new(center.x - width / 2, center.y - 9, width, 18);
    panel(display, rect, 9, bg, argb(0x28, 0xffffff));
    display.text_face(
        rect.x + 9,
        rect.y + (18 - face.line_height) / 2 + face.ascent,
        label,
        face,
        fg,
    );
}

pub fn progress(display: &mut Display, rect: Rect, value: f32, color: u32) {
    display.fill_rounded(rect, rect.h / 2, argb(0x46, 0x59627a));
    display.fill_rect(
        Rect::new(rect.x + 1, rect.y + 1, (rect.w - 2).max(1), 1),
        argb(0x20, 0xffffff),
    );
    let value = value.clamp(0.0, 1.0);
    let width = (rect.w as f32 * value) as i32;
    if width > 0 {
        let filled = Rect::new(rect.x, rect.y, width.max(rect.h), rect.h);
        display.fill_rounded_vertical(
            filled,
            rect.h / 2,
            color,
            argb((color >> 24).min(220), color & 0x00ff_ffff),
        );
        if width >= rect.h {
            // A tiny specular highlight across the top of the bar.
            display.fill_rounded(
                Rect::new(filled.x + rect.h / 2, filled.y + 1, filled.w - rect.h, 1),
                1,
                argb(0x60, 0xffffff),
            );
        }
    }
}

pub fn toggle(display: &mut Display, rect: Rect, on: bool, theme: &Theme) {
    let fill = if on {
        theme.accent
    } else {
        argb(0x70, 0x4b5364)
    };
    display.fill_rounded(rect, rect.h / 2, fill);
    display.stroke_rounded(rect, rect.h / 2, 1, argb(0x30, 0xffffff));
    let knob_radius = rect.h / 2 - 3;
    let knob_x = if on {
        rect.right() - rect.h / 2
    } else {
        rect.x + rect.h / 2
    };
    display.fill_circle(
        Point::new(knob_x, rect.y + rect.h / 2 + 1),
        knob_radius,
        argb(0x50, 0x000000),
    );
    display.fill_circle(
        Point::new(knob_x, rect.y + rect.h / 2),
        knob_radius,
        if on { WHITE } else { TEXT_MUTED },
    );
}

/// Draw a small chevron used by list rows.
pub fn row_chevron(display: &mut Display, rect: Rect, color: u32) {
    icon(
        display,
        Rect::new(rect.right() - 22, rect.y + rect.h / 2 - 7, 14, 14),
        Icon::Right,
        color,
    );
}

/// Approximate brightness of a packed colour, used to choose foregrounds.
pub fn luminance(color: u32) -> f32 {
    let r = ((color >> 16) & 0xff) as f32;
    let g = ((color >> 8) & 0xff) as f32;
    let b = (color & 0xff) as f32;
    (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255.0
}

/// A compact byte-size string.
pub fn byte_size(value: u64) -> String {
    let (length, buffer) = math::format_bytes(value);
    String::from_utf8_lossy(&buffer[..length]).into_owned()
}
