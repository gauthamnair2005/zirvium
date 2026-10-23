//! Wallpaper synthesis.
//!
//! The wallpaper is generated once and cached as a full-screen ARGB buffer.
//! A desktop redraw is therefore a single copy rather than a million floating
//! point operations.  The four styles share the same deterministic star field
//! so switching between them feels like the same desktop seen in different
//! light.

use alloc::vec::Vec;

use zirvstdlib::math;

use crate::ui::theme::{argb, rgb};

/// A tiny deterministic generator so the star field is identical every boot.
struct Random(u64);

impl Random {
    fn new(seed: u64) -> Self {
        Self(seed | 1)
    }

    fn next(&mut self) -> u32 {
        self.0 ^= self.0 << 13;
        self.0 ^= self.0 >> 7;
        self.0 ^= self.0 << 17;
        (self.0 >> 32) as u32
    }

    fn unit(&mut self) -> f32 {
        (self.next() >> 8) as f32 / 16_777_216.0
    }
}

struct Canvas {
    pixels: Vec<u32>,
    width: i32,
    height: i32,
}

impl Canvas {
    fn new(width: i32, height: i32, base: u32) -> Self {
        let mut pixels = Vec::new();
        pixels.resize((width.max(1) * height.max(1)) as usize, base);
        Self {
            pixels,
            width,
            height,
        }
    }

    #[inline]
    fn blend(&mut self, x: i32, y: i32, color: u32, coverage: f32) {
        if x < 0 || y < 0 || x >= self.width || y >= self.height {
            return;
        }
        let coverage = coverage.clamp(0.0, 1.0);
        if coverage <= 0.0 {
            return;
        }
        let index = (y * self.width + x) as usize;
        let destination = self.pixels[index];
        let alpha = (coverage * 255.0) as u32;
        self.pixels[index] = super::widgets::blend(color, destination, alpha);
    }
}

/// Add a soft radial light to a canvas.
fn glow(canvas: &mut Canvas, cx: f32, cy: f32, radius: f32, color: u32, strength: f32) {
    let width = canvas.width as f32;
    let height = canvas.height as f32;
    let center_x = cx * width;
    let center_y = cy * height;
    let radius_pixels = radius * width.max(height);
    let radius_squared = radius_pixels * radius_pixels;
    let left = (center_x - radius_pixels) as i32;
    let right = (center_x + radius_pixels) as i32;
    let top = (center_y - radius_pixels) as i32;
    let bottom = (center_y + radius_pixels) as i32;
    for y in top.max(0)..bottom.min(canvas.height) {
        let dy = y as f32 - center_y;
        for x in left.max(0)..right.min(canvas.width) {
            let dx = x as f32 - center_x;
            let distance_squared = dx * dx + dy * dy;
            if distance_squared >= radius_squared {
                continue;
            }
            let t = 1.0 - distance_squared / radius_squared;
            let coverage = t * t * strength;
            canvas.blend(x, y, color, coverage);
        }
    }
}

fn stars(canvas: &mut Canvas, count: usize, color: u32) {
    let mut random = Random::new(0x5a17_2e11_c0de_0001);
    for _ in 0..count {
        let x = (random.unit() * canvas.width as f32) as i32;
        let y = (random.unit() * canvas.height as f32) as i32;
        let brightness = 0.10 + random.unit() * 0.45;
        canvas.blend(x, y, color, brightness);
        if random.unit() > 0.82 {
            // A few stars get a small cross-shaped halo.
            canvas.blend(x - 1, y, color, brightness * 0.35);
            canvas.blend(x + 1, y, color, brightness * 0.35);
            canvas.blend(x, y - 1, color, brightness * 0.35);
            canvas.blend(x, y + 1, color, brightness * 0.35);
        }
    }
}

fn nebula(canvas: &mut Canvas, accent: u32, accent_alt: u32) {
    let width = canvas.width as f32;
    let height = canvas.height as f32;
    let base_top = rgb(0x080a12);
    let base_bottom = rgb(0x121a2c);
    for y in 0..canvas.height {
        let t = y as f32 / height.max(1.0);
        let row = super::widgets::lerp_color(base_top, base_bottom, t);
        let start = (y * canvas.width) as usize;
        for pixel in &mut canvas.pixels[start..start + canvas.width as usize] {
            *pixel = row;
        }
    }
    // A faint diagonal band suggests a galactic plane.
    let mut random = Random::new(0x9e37_79b9_7f4a_7c15);
    for _ in 0..220 {
        let x = random.unit() * width;
        let y = random.unit() * height;
        let along = (x / width + y / height) * 0.5;
        let band = 1.0 - (along - 0.46).abs() * 5.0;
        if band > 0.0 {
            canvas.blend(x as i32, y as i32, accent, band * 0.045);
        }
    }
    glow(canvas, 0.16, 0.18, 0.62, accent, 0.30);
    glow(canvas, 0.82, 0.72, 0.78, accent_alt, 0.34);
    glow(canvas, 0.52, 0.94, 0.40, rgb(0x1f6feb), 0.10);
    glow(canvas, 0.62, 0.10, 0.30, rgb(0xffffff), 0.035);
    stars(canvas, 260, rgb(0xdfe8ff));
}

fn aurora(canvas: &mut Canvas, accent: u32, accent_alt: u32) {
    let width = canvas.width as f32;
    let height = canvas.height as f32;
    for y in 0..canvas.height {
        let fy = y as f32 / height;
        let row = super::widgets::lerp_color(rgb(0x07131a), rgb(0x0b1c24), fy);
        let start = (y * canvas.width) as usize;
        for pixel in &mut canvas.pixels[start..start + canvas.width as usize] {
            *pixel = row;
        }
        // Two ribbons of light, each a phase-shifted sine wave.
        for band in 0..2 {
            let phase = band as f32 * 1.7;
            let amplitude = 0.10 + band as f32 * 0.03;
            let center = 0.36 + band as f32 * 0.20;
            let frequency = 2.1 + band as f32 * 1.3;
            for x in 0..canvas.width {
                let fx = x as f32 / width;
                let wave = math::sin(fx * frequency * 6.283 + phase + fy * 3.0);
                let offset = (wave * amplitude
                    + math::sin(fx * 9.0 + phase * 2.0) * 0.025)
                    * height;
                let distance = (y as f32 - (center * height + offset)).abs();
                let thickness = height * (0.055 + band as f32 * 0.03);
                let t = (1.0 - distance / thickness).max(0.0);
                if t > 0.0 {
                    let color = if band == 0 { accent } else { accent_alt };
                    canvas.blend(x, y, color, t * t * 0.16);
                }
            }
        }
    }
    glow(canvas, 0.22, 0.30, 0.50, accent, 0.14);
    glow(canvas, 0.78, 0.36, 0.50, accent_alt, 0.12);
    stars(canvas, 120, rgb(0xcfe9ff));
}

fn graphite(canvas: &mut Canvas, accent: u32) {
    let width = canvas.width as f32;
    let height = canvas.height as f32;
    let top = rgb(0x0a0b0e);
    let bottom = rgb(0x171a21);
    for y in 0..canvas.height {
        let t = y as f32 / height.max(1.0);
        let row = super::widgets::lerp_color(top, bottom, t);
        let start = (y * canvas.width) as usize;
        for pixel in &mut canvas.pixels[start..start + canvas.width as usize] {
            *pixel = row;
        }
    }
    // Broad diagonal strokes.
    for stroke in 0..7 {
        let offset = stroke as f32 * 0.23 - 0.4;
        let alpha = 0.025 + (stroke as f32 * 0.006);
        for y in 0..canvas.height {
            let fy = y as f32 / height;
            for x in 0..canvas.width {
                let fx = x as f32 / width;
                let value = fx * 0.9 - fy * 0.6 + offset;
                let distance = math::rem_euclid(value * 5.0, 1.0) - 0.5;
                let band = 1.0 - math::abs(distance) * 18.0;
                if band > 0.0 {
                    canvas.blend(x, y, accent, band * alpha);
                }
            }
        }
    }
    glow(canvas, 0.50, 0.02, 0.70, accent, 0.06);
    let mut random = Random::new(0x1e5b_7a3c_0000_0002);
    for _ in 0..900 {
        let x = (random.unit() * width) as i32;
        let y = (random.unit() * height) as i32;
        let brightness = random.unit() * 0.035;
        canvas.blend(x, y, rgb(0xffffff), brightness);
    }
}

fn ember(canvas: &mut Canvas, accent: u32, accent_alt: u32) {
    let width = canvas.width as f32;
    let height = canvas.height as f32;
    for y in 0..canvas.height {
        let t = y as f32 / height.max(1.0);
        let row = super::widgets::lerp_color(rgb(0x120a0a), rgb(0x24120f), t);
        let start = (y * canvas.width) as usize;
        for pixel in &mut canvas.pixels[start..start + canvas.width as usize] {
            *pixel = row;
        }
    }
    glow(canvas, 0.18, 0.82, 0.72, rgb(0xff7a3d), 0.22);
    glow(canvas, 0.78, 0.20, 0.65, accent_alt, 0.18);
    glow(canvas, 0.50, 1.05, 0.55, accent, 0.20);
    // Rising embers.
    let mut random = Random::new(0x00c0_ffee_1234_5678);
    for _ in 0..170 {
        let x = random.unit() * width;
        let y = random.unit() * height;
        let drift = math::sin(y * 0.05 + x * 0.01) * 18.0;
        let brightness = 0.08 + random.unit() * 0.30;
        let color = if random.unit() > 0.5 {
            rgb(0xffb36b)
        } else {
            accent
        };
        canvas.blend((x + drift) as i32, y as i32, color, brightness);
    }
}

/// Produce a full-screen wallpaper for `style`.
pub fn generate(width: i32, height: i32, style: u8, accent: u32, accent_alt: u32) -> Vec<u32> {
    let mut canvas = Canvas::new(width, height, rgb(0x0b0d14));
    match style {
        1 => aurora(&mut canvas, accent, accent_alt),
        2 => graphite(&mut canvas, accent),
        3 => ember(&mut canvas, accent, accent_alt),
        _ => nebula(&mut canvas, accent, accent_alt),
    }
    // A gentle vignette keeps the desktop chrome readable over bright glows.
    let width_f = canvas.width as f32;
    let height_f = canvas.height as f32;
    for y in 0..canvas.height {
        let fy = (y as f32 / height_f - 0.5).abs() * 2.0;
        for x in 0..canvas.width {
            let fx = (x as f32 / width_f - 0.5).abs() * 2.0;
            let edge = (fx * fx * 0.45 + fy * fy * 0.55).clamp(0.0, 1.0);
            let index = (y * canvas.width + x) as usize;
            let pixel = canvas.pixels[index];
            // Darken by up to ~28% at the very corners.
            let factor = 1.0 - edge * 0.28;
            let r = (((pixel >> 16) & 0xff) as f32 * factor) as u32;
            let g = (((pixel >> 8) & 0xff) as f32 * factor) as u32;
            let b = ((pixel & 0xff) as f32 * factor) as u32;
            canvas.pixels[index] = argb(0xff, (r << 16) | (g << 8) | b);
        }
    }
    canvas.pixels
}
