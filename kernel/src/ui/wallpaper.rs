//! Wallpaper synthesis.
//!
//! The wallpaper is generated once and cached as a full-screen ARGB buffer.
//! A desktop redraw is therefore a single copy rather than a million floating
//! point operations.  The four styles share the same deterministic star field
//! and finishing pass (vignette plus ordered dither) so switching between them
//! feels like the same desktop seen in different light, and the large area
//! gradients never band on an 8-bit panel.

use alloc::vec::Vec;

use core::f32::consts::TAU;

use zirvstdlib::math;

use crate::ui::theme::{argb, rgb};

/// A tiny deterministic generator so the pattern is identical every boot.
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
        let alpha = (coverage * 255.0 + 0.5) as u32;
        self.pixels[index] = super::widgets::blend(color, destination, alpha);
    }

    fn fill_vertical(&mut self, top: u32, bottom: u32) {
        let height = self.height.max(1) as f32;
        for y in 0..self.height {
            let t = y as f32 / height;
            // Smooth the ramp slightly so the horizon reads as light, not as a
            // linear ramp.
            let t = math::smoothstep(0.0, 1.0, t) * 0.72 + t * 0.28;
            let row = super::widgets::lerp_color(top, bottom, t);
            let start = (y * self.width) as usize;
            let row_slice = &mut self.pixels[start..start + self.width as usize];
            row_slice.fill(row);
        }
    }

    /// A soft, additive-looking light.  The quadratic falloff avoids the hard
    /// rim a linear gradient would show.
    fn glow(&mut self, cx: f32, cy: f32, radius: f32, color: u32, strength: f32) {
        let width = self.width as f32;
        let height = self.height as f32;
        let center_x = cx * width;
        let center_y = cy * height;
        let radius_pixels = (radius * width.max(height)).max(1.0);
        let radius_squared = radius_pixels * radius_pixels;
        let left = math::floor(center_x - radius_pixels) as i32;
        let right = math::ceil(center_x + radius_pixels) as i32;
        let top = math::floor(center_y - radius_pixels) as i32;
        let bottom = math::ceil(center_y + radius_pixels) as i32;
        for y in top.max(0)..bottom.min(self.height) {
            let dy = y as f32 + 0.5 - center_y;
            for x in left.max(0)..right.min(self.width) {
                let dx = x as f32 + 0.5 - center_x;
                let distance_squared = dx * dx + dy * dy;
                if distance_squared >= radius_squared {
                    continue;
                }
                let t = 1.0 - distance_squared / radius_squared;
                // Squared again: a broad core with a long, clean tail.
                self.blend(x, y, color, t * t * strength);
            }
        }
    }
}

/// Draw the shared star field.  Three brightness classes give the sky depth:
/// dust, normal stars, and a handful of bright stars with a cross halo.
fn stars(canvas: &mut Canvas, count: usize, tint: u32) {
    let mut random = Random::new(0x5a17_2e11_c0de_0001);
    for _ in 0..count {
        let x = (random.unit() * canvas.width as f32) as i32;
        let y = (random.unit() * canvas.height as f32) as i32;
        let roll = random.unit();
        let brightness = 0.06 + roll * 0.20;
        let color = if roll > 0.90 {
            rgb(0xffffff)
        } else if roll > 0.55 {
            tint
        } else {
            rgb(0xb9c8e8)
        };
        canvas.blend(x, y, color, brightness);
        if roll > 0.82 {
            canvas.blend(x - 1, y, color, brightness * 0.30);
            canvas.blend(x + 1, y, color, brightness * 0.30);
            canvas.blend(x, y - 1, color, brightness * 0.30);
            canvas.blend(x, y + 1, color, brightness * 0.30);
        }
        if roll > 0.975 {
            // A rare bright star gets a two-pixel halo.
            canvas.blend(x, y, rgb(0xffffff), 0.85);
            for (dx, dy, amount) in [
                (-2, 0, 0.16),
                (2, 0, 0.16),
                (0, -2, 0.16),
                (0, 2, 0.16),
                (-1, 0, 0.28),
                (1, 0, 0.28),
                (0, -1, 0.28),
                (0, 1, 0.28),
            ] {
                canvas.blend(x + dx, y + dy, color, amount);
            }
        }
    }
}

fn nebula(canvas: &mut Canvas, accent: u32, accent_alt: u32) {
    canvas.fill_vertical(rgb(0x05070e), rgb(0x0c1422));

    // A soft, dark-blue wash across the lower half keeps system surfaces
    // readable while still adding depth behind the windows.
    canvas.glow(0.50, 0.98, 0.85, rgb(0x132a4a), 0.42);

    // A milky diagonal band, drawn as a chain of small glows.
    let mut random = Random::new(0x9e37_79b9_7f4a_7c15);
    for _ in 0..150 {
        let t = random.unit();
        let x = 0.06 + t * 0.94;
        let y = 0.62 - t * 0.50 + (random.unit() - 0.5) * 0.06;
        let size = 0.022 + random.unit() * 0.030;
        canvas.glow(x, y, size, rgb(0xa9c2ff), 0.020);
    }

    canvas.glow(0.16, 0.16, 0.72, accent, 0.30);
    canvas.glow(0.84, 0.74, 0.90, accent_alt, 0.33);
    canvas.glow(0.55, 0.30, 0.34, accent_alt, 0.10);
    canvas.glow(0.62, 0.08, 0.30, rgb(0xffffff), 0.032);
    stars(canvas, 380, rgb(0xdfe8ff));
}

fn aurora(canvas: &mut Canvas, accent: u32, accent_alt: u32) {
    let width = canvas.width as f32;
    let height = canvas.height as f32;
    canvas.fill_vertical(rgb(0x040b12), rgb(0x091a22));

    for y in 0..canvas.height {
        let fy = y as f32 / height;
        for band in 0..3 {
            let phase = band as f32 * 1.7;
            let amplitude = 0.085 + band as f32 * 0.025;
            let center = 0.30 + band as f32 * 0.17;
            let frequency = 1.9 + band as f32 * 1.1;
            for x in 0..canvas.width {
                let fx = x as f32 / width;
                let wave = math::sin(fx * frequency * TAU + phase + fy * 2.6)
                    + math::sin(fx * 8.7 + phase * 2.0 + fy * 4.0) * 0.32;
                let offset = wave * amplitude * height;
                let distance = (y as f32 - (center * height + offset)).abs();
                let thickness = height * (0.060 + band as f32 * 0.022);
                let t = (1.0 - distance / thickness).max(0.0);
                if t > 0.0 {
                    // Vertical striations make the ribbons read as curtains.
                    let curtain = 0.72 + 0.28 * math::sin(fx * 46.0 + band as f32 * 2.3 + fy * 3.0);
                    let color = match band {
                        0 => accent,
                        1 => accent_alt,
                        _ => rgb(0x7ce7d0),
                    };
                    canvas.blend(x, y, color, t * t * curtain * 0.115);
                }
            }
        }
    }
    canvas.glow(0.22, 0.34, 0.58, accent, 0.16);
    canvas.glow(0.78, 0.40, 0.58, accent_alt, 0.14);
    canvas.glow(0.50, 1.02, 0.70, rgb(0x0b3b46), 0.30);
    stars(canvas, 170, rgb(0xcfe9ff));
}

fn graphite(canvas: &mut Canvas, accent: u32, accent_alt: u32) {
    let width = canvas.width as f32;
    let height = canvas.height as f32;
    canvas.fill_vertical(rgb(0x07080b), rgb(0x14171e));

    // Broad diagonal beams, computed once per pixel.
    for y in 0..canvas.height {
        let fy = y as f32 / height;
        for x in 0..canvas.width {
            let fx = x as f32 / width;
            let value = fx * 0.85 - fy * 0.62 + 0.30;
            let distance = math::rem_euclid(value * 4.0, 1.0) - 0.5;
            let band = (1.0 - math::abs(distance) * 14.0).max(0.0);
            if band > 0.0 {
                canvas.blend(x, y, accent, band * 0.030);
            }
        }
    }
    canvas.glow(0.50, 0.00, 0.75, accent, 0.070);
    canvas.glow(0.12, 0.86, 0.55, accent_alt, 0.045);
    let mut random = Random::new(0x1e5b_7a3c_0000_0002);
    for _ in 0..1100 {
        let x = (random.unit() * width) as i32;
        let y = (random.unit() * height) as i32;
        let brightness = random.unit() * 0.045;
        canvas.blend(x, y, rgb(0xffffff), brightness);
    }
}

fn ember(canvas: &mut Canvas, accent: u32, accent_alt: u32) {
    let width = canvas.width as f32;
    let height = canvas.height as f32;
    canvas.fill_vertical(rgb(0x100708), rgb(0x1d0e0b));
    canvas.glow(0.18, 0.86, 0.80, rgb(0xff7a3d), 0.26);
    canvas.glow(0.82, 0.20, 0.72, accent_alt, 0.20);
    canvas.glow(0.50, 1.08, 0.62, accent, 0.22);
    canvas.glow(0.50, -0.05, 0.50, rgb(0x3a1420), 0.24);

    // Rising embers; each has a tiny halo so they read as points of light.
    let mut random = Random::new(0x00c0_ffee_1234_5678);
    for _ in 0..180 {
        let x = random.unit() * width;
        let y = random.unit() * height;
        let drift = math::sin(y * 0.05 + x * 0.01) * 18.0;
        let brightness = 0.08 + random.unit() * 0.30;
        let color = if random.unit() > 0.5 {
            rgb(0xffb36b)
        } else {
            accent
        };
        let px = (x + drift) as i32;
        let py = y as i32;
        canvas.blend(px, py, color, brightness);
        canvas.blend(px - 1, py, color, brightness * 0.18);
        canvas.blend(px + 1, py, color, brightness * 0.18);
        canvas.blend(px, py - 1, color, brightness * 0.18);
        canvas.blend(px, py + 1, color, brightness * 0.18);
    }
}

/// Ordered 4x4 Bayer matrix, centred on zero.
const BAYER: [[i32; 4]; 4] = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]];

/// Apply a vignette and a sub-LSB dither.  The dither is what turns a smooth
/// 8-bit gradient into a continuous ramp instead of visible bands.
fn finish(canvas: &mut Canvas) {
    let width = canvas.width as f32;
    let height = canvas.height as f32;
    for y in 0..canvas.height {
        let fy = y as f32 / height - 0.5;
        let row_start = (y * canvas.width) as usize;
        for x in 0..canvas.width {
            let fx = x as f32 / width - 0.5;
            let distance = math::sqrt(fx * fx * 0.85 + fy * fy);
            let edge = math::smoothstep(0.28, 0.72, distance);
            let factor = 1.0 - edge * 0.34;
            let index = row_start + x as usize;
            let pixel = canvas.pixels[index];
            let dither = BAYER[(y & 3) as usize][(x & 3) as usize] - 8;
            let channel = |shift: u32| -> u32 {
                let value = ((pixel >> shift) & 0xff) as f32 * factor + dither as f32 * 0.22;
                (value.clamp(0.0, 255.0) + 0.5) as u32
            };
            canvas.pixels[index] = argb(0xff, (channel(16) << 16) | (channel(8) << 8) | channel(0));
        }
    }
}

/// Produce a full-screen wallpaper for `style`.
pub fn generate(width: i32, height: i32, style: u8, accent: u32, accent_alt: u32) -> Vec<u32> {
    let mut canvas = Canvas::new(width, height, rgb(0x0b0d14));
    match style {
        1 => aurora(&mut canvas, accent, accent_alt),
        2 => graphite(&mut canvas, accent, accent_alt),
        3 => ember(&mut canvas, accent, accent_alt),
        _ => nebula(&mut canvas, accent, accent_alt),
    }
    finish(&mut canvas);
    canvas.pixels
}
