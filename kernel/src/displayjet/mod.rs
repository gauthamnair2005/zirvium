//! DisplayJet: Zirvium's display engine.
//!
//! DisplayJet owns a persistent 32-bit ARGB back buffer, a damage tracker and
//! a set of anti-aliased drawing operations.  All rasterisation currently
//! happens on the CPU using SSE2; the finished surface is presented to the
//! GPU-backed framebuffer that firmware programmed during boot.  The GPU
//! backend boundary is deliberately narrow (`present`) so that a native
//! blitter or 3D engine can take over without touching the UI.

pub mod font;

use alloc::vec::Vec;

use zirvstdlib::color::blend_argb;
use zirvstdlib::geometry::{Point, Rect};
use zirvstdlib::math;

use crate::uefi::gop::Framebuffer;
use zirvstdlib::abi::{GpuDriver, GpuInfo, GpuVendor};
use zirvstdlib::sync::Once;

const FALLBACK_COLOR: u32 = 0xff10_1116;
const CURSOR_WIDTH: i32 = 13;
const CURSOR_HEIGHT: i32 = 20;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum CursorStyle {
    Pointer,
    Precision,
    Hand,
}

/// Lookup tables that let text compositing happen in linear light instead of
/// sRGB.  Blending coverage in sRGB darkens anti-aliased edges and makes small
/// glyphs look muddy; converting through these tables instead keeps stems
/// crisp and gives the same perceived weight at every size.  Gamma 2.0 is used
/// because a square and a square root are exact and cheap without a maths
/// library.
struct GammaTables {
    to_linear: [u16; 256],
    from_linear: [u8; 1024],
}

fn build_gamma_tables() -> GammaTables {
    let mut to_linear = [0u16; 256];
    let mut index = 0u32;
    while index < 256 {
        // (i / 255)^2 * 65535
        to_linear[index as usize] = ((index * index * 65535 + 32_512) / 65_025) as u16;
        index += 1;
    }
    let mut from_linear = [0u8; 1024];
    let mut slot = 0usize;
    while slot < 1024 {
        let linear = slot as f32 / 1023.0;
        from_linear[slot] = (math::sqrt(linear) * 255.0 + 0.5) as u8;
        slot += 1;
    }
    GammaTables {
        to_linear,
        from_linear,
    }
}

static GAMMA: Once<GammaTables> = Once::new();

#[inline]
fn gamma_tables() -> &'static GammaTables {
    GAMMA.call_once(build_gamma_tables)
}

/// Blend `source` into `destination` through linear light with `alpha` 0..=255.
#[inline]
fn blend_linear(source: u32, destination: u32, alpha: u32, gamma: &GammaTables) -> u32 {
    let inverse = 255 - alpha;
    let channel = |shift: u32| -> u32 {
        let source_linear = gamma.to_linear[((source >> shift) & 0xff) as usize] as u32;
        let destination_linear = gamma.to_linear[((destination >> shift) & 0xff) as usize] as u32;
        let mixed = (source_linear * alpha + destination_linear * inverse + 127) / 255;
        gamma.from_linear[(mixed >> 6) as usize] as u32
    };
    let r = channel(16);
    let g = channel(8);
    let b = channel(0);
    0xff00_0000 | (r << 16) | (g << 8) | b
}

/// A drawing surface with damage tracking and a firmware framebuffer sink.
pub struct Display {
    pub width: i32,
    pub height: i32,
    framebuffer: Framebuffer,
    pixels: Vec<u32>,
    dirty: Option<Rect>,
    clip: Rect,
    clip_stack: [Rect; 8],
    clip_depth: usize,
    cursor_visible: bool,
    cursor_style: CursorStyle,
    cursor: Point,
    cursor_saved: Vec<u32>,
    cursor_saved_rect: Rect,
    pub gpu: GpuInfo,
}

impl Display {
    /// Create the display over a GOP framebuffer.
    pub fn new(framebuffer: Framebuffer) -> Option<Self> {
        if !framebuffer.is_valid() {
            return None;
        }
        let width = framebuffer.width as i32;
        let height = framebuffer.height as i32;
        let mut pixels = Vec::new();
        pixels.resize((width * height) as usize, FALLBACK_COLOR);
        let screen = Rect::new(0, 0, width, height);
        Some(Self {
            width,
            height,
            framebuffer,
            pixels,
            dirty: None,
            clip: screen,
            clip_stack: [screen; 8],
            clip_depth: 0,
            cursor_visible: false,
            cursor_style: CursorStyle::Pointer,
            cursor: Point::new(width / 2, height / 2),
            cursor_saved: Vec::new(),
            cursor_saved_rect: Rect::ZERO,
            gpu: GpuInfo {
                vendor: GpuVendor::None,
                driver: GpuDriver::FramebufferSimd,
                vendor_id: 0,
                device_id: 0,
                mmio_base: framebuffer.address as u64,
                mmio_size: framebuffer.size,
                accelerated: false,
                _reserved: [0; 7],
                name: [0; 64],
            },
        })
    }

    pub fn set_gpu(&mut self, gpu: GpuInfo) {
        self.gpu = gpu;
    }

    pub fn screen(&self) -> Rect {
        Rect::new(0, 0, self.width, self.height)
    }

    pub fn stride(&self) -> u32 {
        self.framebuffer.stride
    }

    pub fn swap_red_blue(&self) -> bool {
        self.framebuffer.swap_red_blue
    }

    // ---------------------------------------------------------------------
    // Clipping and damage
    // ---------------------------------------------------------------------

    pub fn push_clip(&mut self, rect: Rect) {
        let clipped = rect.intersect(self.clip);
        if self.clip_depth < self.clip_stack.len() {
            self.clip_stack[self.clip_depth] = self.clip;
            self.clip_depth += 1;
            self.clip = clipped;
        }
    }

    pub fn pop_clip(&mut self) {
        if self.clip_depth > 0 {
            self.clip_depth -= 1;
            self.clip = self.clip_stack[self.clip_depth];
        }
    }

    pub fn reset_clip(&mut self) {
        self.clip_depth = 0;
        self.clip = self.screen();
    }

    pub fn clip(&self) -> Rect {
        self.clip
    }

    fn mark(&mut self, rect: Rect) {
        let rect = rect.intersect(self.screen());
        if rect.is_empty() {
            return;
        }
        self.dirty = Some(match self.dirty {
            Some(existing) => existing.union(rect),
            None => rect,
        });
    }

    fn clipped(&self, rect: Rect) -> Rect {
        rect.intersect(self.clip).intersect(self.screen())
    }

    pub fn mark_all(&mut self) {
        self.dirty = Some(self.screen());
    }

    // ---------------------------------------------------------------------
    // Raw surface access
    // ---------------------------------------------------------------------

    /// Replace the whole surface with `buffer`, which must match the display
    /// size.  Used for the cached wallpaper.
    pub fn copy_surface(&mut self, buffer: &[u32]) {
        if buffer.len() != self.pixels.len() {
            return;
        }
        self.pixels.copy_from_slice(buffer);
        self.mark_all();
    }

    /// Copy a rectangle out of the surface.
    pub fn save_rect(&self, rect: Rect) -> Vec<u32> {
        let rect = rect.intersect(self.screen());
        if rect.is_empty() {
            return Vec::new();
        }
        let mut output = Vec::with_capacity((rect.w * rect.h) as usize);
        for y in rect.y..rect.bottom() {
            let start = (y * self.width + rect.x) as usize;
            output.extend_from_slice(&self.pixels[start..start + rect.w as usize]);
        }
        output
    }

    pub fn clear(&mut self, color: u32) {
        self.fill_rect(self.screen(), color);
    }

    /// Fill a rectangle.  Translucent colours are blended.
    pub fn fill_rect(&mut self, rect: Rect, color: u32) {
        let rect = self.clipped(rect);
        if rect.is_empty() {
            return;
        }
        let alpha = color >> 24;
        if alpha == 0 {
            return;
        }
        if alpha == 255 {
            let width = self.width as usize;
            for y in rect.y..rect.bottom() {
                let start = (y as usize) * width + rect.x as usize;
                let row = &mut self.pixels[start..start + rect.w as usize];
                fill_row(row, color);
            }
        } else {
            let width = self.width;
            for y in rect.y..rect.bottom() {
                let start = (y * width + rect.x) as usize;
                let row = &mut self.pixels[start..start + rect.w as usize];
                for pixel in row.iter_mut() {
                    *pixel = blend_argb(color, *pixel);
                }
            }
        }
        self.mark(rect);
    }

    /// A vertical linear gradient between two opaque colours.
    pub fn vertical_gradient(&mut self, rect: Rect, top: u32, bottom: u32) {
        let rect = self.clipped(rect);
        if rect.is_empty() {
            return;
        }
        let height = rect.h.max(1);
        let width = self.width as usize;
        for offset in 0..rect.h {
            let t = if height <= 1 {
                0
            } else {
                (offset * 255) / (height - 1)
            };
            let color = lerp_argb(top, bottom, t as u32);
            let y = rect.y + offset;
            let start = (y as usize) * width + rect.x as usize;
            let row = &mut self.pixels[start..start + rect.w as usize];
            fill_row(row, color);
        }
        self.mark(rect);
    }

    /// A horizontal linear gradient between two colours.  Translucent stops are
    /// alpha-blended, so a soft accent hairline fades into the surface below.
    pub fn horizontal_gradient(&mut self, rect: Rect, left: u32, right: u32) {
        let rect = self.clipped(rect);
        if rect.is_empty() {
            return;
        }
        let width = rect.w.max(1);
        for x in 0..rect.w {
            let t = if width <= 1 {
                0
            } else {
                (x * 255) / (width - 1)
            };
            let color = lerp_argb(left, right, t as u32);
            let column_x = rect.x + x;
            let alpha = color >> 24;
            if alpha == 0 {
                continue;
            }
            let mut y = rect.y;
            while y < rect.bottom() {
                let index = (y * self.width + column_x) as usize;
                if alpha == 255 {
                    self.pixels[index] = color;
                } else {
                    self.pixels[index] = blend_argb(color, self.pixels[index]);
                }
                y += 1;
            }
        }
        self.mark(rect);
    }

    /// A solid or translucent rounded rectangle.
    pub fn fill_rounded(&mut self, rect: Rect, radius: i32, color: u32) {
        let clipped = self.clipped(rect);
        if clipped.is_empty() || color >> 24 == 0 {
            return;
        }
        let radius = radius.min(rect.w / 2).min(rect.h / 2).max(0);
        let alpha = color >> 24;
        let x0 = clipped.x;
        let x1 = clipped.right();
        let y0 = clipped.y;
        let y1 = clipped.bottom();
        let width = self.width;
        let middle_top = rect.y + radius;
        let middle_bottom = rect.bottom() - radius;
        for y in y0..y1 {
            let py = y as f32 + 0.5;
            let row_start = (y * width) as usize;
            let row = &mut self.pixels[row_start + x0 as usize..row_start + x1 as usize];
            // The straight middle section has no rounded corners, so it can be
            // filled in bulk; only the corner bands need a distance field.
            if py >= middle_top as f32 && py < middle_bottom as f32 {
                if alpha == 255 {
                    fill_row(row, color | 0xff00_0000);
                } else {
                    for pixel in row.iter_mut() {
                        *pixel = blend_argb(color, *pixel);
                    }
                }
                continue;
            }
            for x in x0..x1 {
                let distance = math::rounded_rect_sdf(
                    x as f32 + 0.5,
                    py,
                    rect.x as f32,
                    rect.y as f32,
                    rect.w as f32,
                    rect.h as f32,
                    radius as f32,
                );
                let coverage = math::coverage_from_sdf(distance);
                if coverage <= 0.0 {
                    continue;
                }
                let pixel_alpha = (alpha as f32 * coverage) as u32;
                if pixel_alpha == 0 {
                    continue;
                }
                let index = (x - x0) as usize;
                if pixel_alpha >= 255 {
                    row[index] = color | 0xff00_0000;
                } else {
                    let source = (color & 0x00ff_ffff) | (pixel_alpha << 24);
                    row[index] = blend_argb(source, row[index]);
                }
            }
        }
        self.mark(rect);
    }

    /// A rounded rectangle outline.
    pub fn stroke_rounded(&mut self, rect: Rect, radius: i32, thickness: i32, color: u32) {
        let clipped = self.clipped(rect);
        if clipped.is_empty() || color >> 24 == 0 {
            return;
        }
        let radius = radius.min(rect.w / 2).min(rect.h / 2).max(0);
        let half = thickness as f32 * 0.5;
        let alpha = color >> 24;
        let width = self.width;
        let middle_top = rect.y + radius;
        let middle_bottom = rect.bottom() - radius;
        let band = thickness.max(1) + 1;
        for y in clipped.y..clipped.bottom() {
            let py = y as f32 + 0.5;
            let row_start = (y * width) as usize;
            // Rows through the straight sides only need the two vertical
            // edges; the interior cannot contribute to the outline.
            if py >= middle_top as f32 && py < middle_bottom as f32 {
                let left_end = (rect.x + band).min(clipped.right());
                for x in clipped.x..left_end {
                    self.stroke_pixel(row_start, x, py, rect, radius, half, alpha, color);
                }
                let right_start = (rect.right() - band).max(clipped.x);
                for x in right_start..clipped.right() {
                    self.stroke_pixel(row_start, x, py, rect, radius, half, alpha, color);
                }
                continue;
            }
            for x in clipped.x..clipped.right() {
                self.stroke_pixel(row_start, x, py, rect, radius, half, alpha, color);
            }
        }
        self.mark(rect);
    }

    #[inline]
    #[allow(clippy::too_many_arguments)]
    fn stroke_pixel(
        &mut self,
        row_start: usize,
        x: i32,
        py: f32,
        rect: Rect,
        radius: i32,
        half: f32,
        alpha: u32,
        color: u32,
    ) {
        let distance = math::rounded_rect_sdf(
            x as f32 + 0.5,
            py,
            rect.x as f32,
            rect.y as f32,
            rect.w as f32,
            rect.h as f32,
            radius as f32,
        );
        let ring = (distance.abs() - half).max(0.0);
        let coverage = (1.0 - ring).clamp(0.0, 1.0);
        if coverage <= 0.0 {
            return;
        }
        let pixel_alpha = (alpha as f32 * coverage) as u32;
        if pixel_alpha == 0 {
            return;
        }
        let index = row_start + x as usize;
        if pixel_alpha >= 255 {
            self.pixels[index] = color | 0xff00_0000;
        } else {
            let source = (color & 0x00ff_ffff) | (pixel_alpha << 24);
            self.pixels[index] = blend_argb(source, self.pixels[index]);
        }
    }

    /// A soft drop shadow.  This is a single pass over the shadow ring: pixels
    /// under the window are skipped, and the falloff is evaluated only where
    /// the shadow can actually be seen.  The previous implementation painted
    /// ten full-area layers, which dominated frame time.
    pub fn drop_shadow(&mut self, rect: Rect, radius: i32, spread: i32, color: u32) {
        let base_alpha = color >> 24;
        if base_alpha == 0 || spread <= 0 {
            return;
        }
        let offset = spread / 2;
        let shadow = Rect::new(rect.x, rect.y + offset, rect.w, rect.h);
        let expanded = Rect::new(
            rect.x - spread,
            rect.y - spread + offset,
            rect.w + spread * 2,
            rect.h + spread * 2,
        );
        let clipped = self.clipped(expanded);
        if clipped.is_empty() {
            return;
        }
        let radius = radius.min(rect.w / 2).min(rect.h / 2).max(0);
        let spread_f = spread as f32;
        let window_top = rect.y;
        let window_bottom = rect.bottom();
        let window_left = rect.x;
        let window_right = rect.right();
        for y in clipped.y..clipped.bottom() {
            let py = y as f32 + 0.5;
            let row_start = (y * self.width) as usize;
            // The central span of a row crossing the window is covered by the
            // window itself.  In the corner bands only the straight middle is
            // certainly covered, so the corner notches still receive shadow.
            let (skip_start, skip_end) = if y < window_top || y >= window_bottom {
                (clipped.right(), clipped.right())
            } else if y < window_top + radius || y >= window_bottom - radius {
                (
                    (window_left + radius).clamp(clipped.x, clipped.right()),
                    (window_right - radius).clamp(clipped.x, clipped.right()),
                )
            } else {
                (
                    window_left.clamp(clipped.x, clipped.right()),
                    window_right.clamp(clipped.x, clipped.right()),
                )
            };
            for x in clipped.x..skip_start {
                self.shadow_pixel(
                    row_start, x, py, shadow, radius, spread_f, base_alpha, color,
                );
            }
            for x in skip_end..clipped.right() {
                self.shadow_pixel(
                    row_start, x, py, shadow, radius, spread_f, base_alpha, color,
                );
            }
        }
        self.mark(expanded);
    }

    #[inline]
    #[allow(clippy::too_many_arguments)]
    fn shadow_pixel(
        &mut self,
        row_start: usize,
        x: i32,
        py: f32,
        shadow: Rect,
        radius: i32,
        spread: f32,
        base_alpha: u32,
        color: u32,
    ) {
        // Anything inside the shadow rectangle is covered by the window, so it
        // can never be visible; the distance test handles that along with the
        // outer falloff.
        let distance = math::rounded_rect_sdf(
            x as f32 + 0.5,
            py,
            shadow.x as f32,
            shadow.y as f32,
            shadow.w as f32,
            shadow.h as f32,
            radius as f32,
        );
        if distance <= 0.0 || distance >= spread {
            return;
        }
        let falloff = 1.0 - distance / spread;
        let alpha = (base_alpha as f32 * falloff * falloff) as u32;
        if alpha == 0 {
            return;
        }
        let index = row_start + x as usize;
        let source = (color & 0x00ff_ffff) | (alpha.min(255) << 24);
        self.pixels[index] = blend_argb(source, self.pixels[index]);
    }

    /// A vertical gradient confined to a rounded rectangle.  Interior rows are
    /// filled in bulk; only the corner bands pay for a distance field.
    pub fn fill_rounded_vertical(&mut self, rect: Rect, radius: i32, top: u32, bottom: u32) {
        let clipped = self.clipped(rect);
        if clipped.is_empty() {
            return;
        }
        let radius = radius.min(rect.w / 2).min(rect.h / 2).max(0);
        let height = rect.h.max(1);
        let interior_top = rect.y as f32 + radius as f32;
        let interior_bottom = rect.bottom() as f32 - radius as f32;
        for y in clipped.y..clipped.bottom() {
            let t = if height <= 1 {
                0
            } else {
                ((y - rect.y) * 255) / (height - 1)
            };
            let color = lerp_argb(top, bottom, t as u32);
            let alpha = color >> 24;
            let row_start = (y * self.width) as usize;
            let row = &mut self.pixels
                [row_start + clipped.x as usize..row_start + clipped.right() as usize];
            let py = y as f32 + 0.5;
            if py >= interior_top && py < interior_bottom {
                if alpha == 255 {
                    fill_row(row, color);
                } else if alpha != 0 {
                    for pixel in row.iter_mut() {
                        let source = (color & 0x00ff_ffff) | (alpha << 24);
                        *pixel = blend_argb(source, *pixel);
                    }
                }
                continue;
            }
            for x in clipped.x..clipped.right() {
                let distance = math::rounded_rect_sdf(
                    x as f32 + 0.5,
                    py,
                    rect.x as f32,
                    rect.y as f32,
                    rect.w as f32,
                    rect.h as f32,
                    radius as f32,
                );
                let coverage = math::coverage_from_sdf(distance);
                if coverage <= 0.0 {
                    continue;
                }
                let pixel_alpha = (alpha as f32 * coverage) as u32;
                if pixel_alpha == 0 {
                    continue;
                }
                let index = (x - clipped.x) as usize;
                if pixel_alpha >= 255 {
                    row[index] = color | 0xff00_0000;
                } else {
                    let source = (color & 0x00ff_ffff) | (pixel_alpha << 24);
                    row[index] = blend_argb(source, row[index]);
                }
            }
        }
        self.mark(rect);
    }

    /// Add a soft radial light.  `strength` scales the source alpha at the
    /// centre; the falloff is quadratic so the edge never bands.
    pub fn radial_glow(&mut self, center: Point, radius: i32, color: u32, strength: f32) {
        if radius <= 0 || color >> 24 == 0 {
            return;
        }
        let rect = Rect::new(center.x - radius, center.y - radius, radius * 2, radius * 2);
        let clipped = self.clipped(rect);
        if clipped.is_empty() {
            return;
        }
        let radius_f = radius as f32;
        let radius_squared = radius_f * radius_f;
        let base = ((color >> 24) as f32) * strength;
        let inv_radius_squared = 1.0 / radius_squared;
        let color_rgb = color & 0x00ff_ffff;
        for y in clipped.y..clipped.bottom() {
            let dy = y as f32 + 0.5 - center.y as f32;
            let row_start = (y * self.width) as usize;
            for x in clipped.x..clipped.right() {
                let dx = x as f32 + 0.5 - center.x as f32;
                let distance_squared = dx * dx + dy * dy;
                if distance_squared >= radius_squared {
                    continue;
                }
                let falloff = 1.0 - distance_squared * inv_radius_squared;
                let alpha_f = base * falloff * falloff;
                if alpha_f < 0.5 {
                    continue;
                }
                let alpha = alpha_f as u32;
                if alpha == 0 {
                    continue;
                }
                let index = row_start + x as usize;
                let source = color_rgb | ((alpha.min(255)) << 24);
                self.pixels[index] = blend_argb(source, self.pixels[index]);
            }
        }
        self.mark(rect);
    }

    /// A filled circle.
    pub fn fill_circle(&mut self, center: Point, radius: i32, color: u32) {
        let rect = Rect::new(center.x - radius, center.y - radius, radius * 2, radius * 2);
        let clipped = self.clipped(rect);
        if clipped.is_empty() || color >> 24 == 0 {
            return;
        }
        let radius_f = radius as f32;
        let alpha = color >> 24;
        for y in clipped.y..clipped.bottom() {
            let dy = y as f32 + 0.5 - center.y as f32;
            let row_start = (y * self.width) as usize;
            for x in clipped.x..clipped.right() {
                let dx = x as f32 + 0.5 - center.x as f32;
                let distance = math::sqrt(dx * dx + dy * dy);
                let coverage = (radius_f + 0.5 - distance).clamp(0.0, 1.0);
                if coverage <= 0.0 {
                    continue;
                }
                let pixel_alpha = (alpha as f32 * coverage) as u32;
                if pixel_alpha == 0 {
                    continue;
                }
                let index = row_start + x as usize;
                let source = (color & 0x00ff_ffff) | (pixel_alpha << 24);
                self.pixels[index] = blend_argb(source, self.pixels[index]);
            }
        }
        self.mark(rect);
    }

    /// Blend the saved pixels back outside a rounded-rectangle shape.  The UI
    /// uses this to give windows and panels clean anti-aliased corners.
    pub fn mask_rounded(&mut self, rect: Rect, radius: i32, saved: &[u32]) {
        let rect = rect.intersect(self.screen());
        if rect.is_empty() || saved.len() != (rect.w * rect.h) as usize {
            return;
        }
        let radius = radius.min(rect.w / 2).min(rect.h / 2).max(0);
        for y in 0..rect.h {
            for x in 0..rect.w {
                let distance = math::rounded_rect_sdf(
                    (rect.x + x) as f32 + 0.5,
                    (rect.y + y) as f32 + 0.5,
                    rect.x as f32,
                    rect.y as f32,
                    rect.w as f32,
                    rect.h as f32,
                    radius as f32,
                );
                let coverage = math::coverage_from_sdf(distance);
                if coverage >= 0.999 {
                    continue;
                }
                let index = (y * rect.w + x) as usize;
                let destination =
                    (rect.y + y) as usize * self.width as usize + (rect.x + x) as usize;
                let background = saved[index];
                let current = self.pixels[destination];
                self.pixels[destination] = blend_coverage(background, current, coverage);
            }
        }
        self.mark(rect);
    }

    // ---------------------------------------------------------------------
    // Text
    // ---------------------------------------------------------------------

    /// Width of `text` at a size and weight.
    pub fn text_width(&self, text: &str, size: u32, bold: bool) -> i32 {
        font::face(size, bold).width(text)
    }

    /// Draw text with the top-left corner at `(x, y)`.
    pub fn text(&mut self, x: i32, y: i32, text: &str, size: u32, bold: bool, color: u32) {
        let face = font::face(size, bold);
        self.text_face(x, y + face.ascent, text, face, color);
    }

    /// Draw text using a baseline coordinate.
    pub fn text_face(&mut self, x: i32, baseline: i32, text: &str, face: &font::Face, color: u32) {
        if color >> 24 == 0 {
            return;
        }
        let gamma = gamma_tables();
        let clip = self.clip;
        let opaque = color >> 24 == 255;
        let mut pen_x = x;
        for character in text.chars() {
            let glyph = match face.glyph(character) {
                Some(glyph) => glyph,
                None => continue,
            };
            if glyph.width > 0 && glyph.height > 0 {
                let coverage = face.coverage(glyph);
                let glyph_x = pen_x + glyph.bearing_x;
                let glyph_y = baseline + glyph.bearing_y;
                let source_alpha = color >> 24;
                for row in 0..glyph.height {
                    let y = glyph_y + row;
                    if y < clip.y || y >= clip.bottom() {
                        continue;
                    }
                    let row_start = (row * glyph.width) as usize;
                    for column in 0..glyph.width {
                        let x = glyph_x + column;
                        if x < clip.x || x >= clip.right() {
                            continue;
                        }
                        let cov = match coverage.get(row_start + column as usize) {
                            Some(value) => *value as u32,
                            None => continue,
                        };
                        if cov == 0 {
                            continue;
                        }
                        let alpha = (source_alpha * cov + 127) / 255;
                        if alpha == 0 {
                            continue;
                        }
                        let index = (y * self.width + x) as usize;
                        if opaque && alpha >= 255 {
                            self.pixels[index] = color;
                        } else {
                            self.pixels[index] =
                                blend_linear(color, self.pixels[index], alpha, gamma);
                        }
                    }
                }
                self.mark(Rect::new(glyph_x, glyph_y, glyph.width, glyph.height));
            }
            pen_x += glyph.advance as i32;
        }
    }

    /// Draw text centred inside a rectangle.
    pub fn text_centered(&mut self, rect: Rect, text: &str, size: u32, bold: bool, color: u32) {
        let face = font::face(size, bold);
        let width = face.width(text);
        let x = rect.x + (rect.w - width) / 2;
        let y = rect.y + (rect.h - face.line_height) / 2;
        self.text(x, y, text, size, bold, color);
    }

    /// Truncate text with an ellipsis so it fits in `width` pixels.
    #[allow(clippy::too_many_arguments)]
    pub fn text_ellipsized(
        &mut self,
        x: i32,
        y: i32,
        text: &str,
        width: i32,
        size: u32,
        bold: bool,
        color: u32,
    ) {
        let face = font::face(size, bold);
        if face.width(text) <= width {
            self.text(x, y, text, size, bold, color);
            return;
        }
        let ellipsis = "...";
        let ellipsis_width = face.width(ellipsis);
        let available = (width - ellipsis_width).max(0);
        let count = face.fit_chars(text, available);
        let mut truncated = alloc::string::String::new();
        for character in text.chars().take(count) {
            truncated.push(character);
        }
        truncated.push_str(ellipsis);
        self.text(x, y, &truncated, size, bold, color);
    }

    // ---------------------------------------------------------------------
    // Cursor
    // ---------------------------------------------------------------------

    pub fn cursor_position(&self) -> Point {
        self.cursor
    }

    pub fn cursor_hide(&mut self) {
        if !self.cursor_visible {
            return;
        }
        let rect = self.cursor_saved_rect;
        if !rect.is_empty() && self.cursor_saved.len() == (rect.w * rect.h) as usize {
            for y in 0..rect.h {
                let source = (y * rect.w) as usize;
                let destination = (rect.y + y) as usize * self.width as usize + rect.x as usize;
                let row = &self.cursor_saved[source..source + rect.w as usize];
                self.pixels[destination..destination + rect.w as usize].copy_from_slice(row);
            }
            self.mark(rect);
        }
        self.cursor_visible = false;
    }

    /// Draw the cursor at `(x, y)`, saving what lies underneath.
    pub fn cursor_show(&mut self, x: i32, y: i32) {
        self.cursor_show_with_style(x, y, CursorStyle::Pointer);
    }

    pub fn cursor_show_with_style(&mut self, x: i32, y: i32, style: CursorStyle) {
        self.cursor_hide();
        self.cursor_style = style;
        let x = x.clamp(0, self.width - 1);
        let y = y.clamp(0, self.height - 1);
        self.cursor = Point::new(x, y);
        let rect = Rect::new(x, y, CURSOR_WIDTH + 2, CURSOR_HEIGHT + 2).intersect(self.screen());
        if rect.is_empty() {
            return;
        }
        self.cursor_saved = self.save_rect(rect);
        self.cursor_saved_rect = rect;
        self.draw_cursor_shape(x, y, style);
        self.cursor_visible = true;
        self.mark(rect);
    }

    /// Move the cursor without repainting the scene.
    pub fn cursor_move(&mut self, x: i32, y: i32) {
        if self.cursor_visible && self.cursor.x == x && self.cursor.y == y {
            return;
        }
        self.cursor_show_with_style(x, y, self.cursor_style);
    }

    fn draw_cursor_shape(&mut self, x: i32, y: i32, style: CursorStyle) {
        const POINTER: [(i32, i32); 7] = [
            (0, 0),
            (0, 16),
            (4, 12),
            (7, 19),
            (9, 18),
            (6, 11),
            (11, 11),
        ];
        const PRECISION: [(i32, i32); 12] = [
            (6, 1), (8, 1), (8, 6), (13, 6), (13, 8), (8, 8),
            (8, 14), (6, 14), (6, 8), (1, 8), (1, 6), (6, 6),
        ];
        const HAND: [(i32, i32); 12] = [
            (3, 19), (1, 16), (2, 13), (4, 13), (4, 5), (6, 5),
            (7, 10), (8, 4), (10, 5), (10, 11), (11, 8), (13, 10),
        ];
        let points = match style {
            CursorStyle::Pointer => &POINTER[..],
            CursorStyle::Precision => &PRECISION[..],
            CursorStyle::Hand => &HAND[..],
        };
        let clip = self.clip.intersect(self.screen());
        for py in 0..CURSOR_HEIGHT {
            for px in 0..CURSOR_WIDTH {
                let gx = x + px;
                let gy = y + py;
                if gx < clip.x || gy < clip.y || gx >= clip.right() || gy >= clip.bottom() {
                    continue;
                }
                let inside = point_in_polygon(px, py, points);
                if !inside {
                    continue;
                }
                let edge = distance_to_edges(px, py, points);
                let color = if edge > 1.25 { 0xffff_ffff } else { 0xff0a0a0c };
                let index = (gy * self.width + gx) as usize;
                self.pixels[index] = color;
            }
        }
    }

    // ---------------------------------------------------------------------
    // Presentation
    // ---------------------------------------------------------------------

    /// Copy the damaged region to the framebuffer.
    pub fn present(&mut self) {
        if let Some(rect) = self.dirty.take() {
            self.blit(rect);
        }
    }

    /// Present the whole surface.
    pub fn present_all(&mut self) {
        self.dirty = None;
        self.blit(self.screen());
    }

    pub fn present_rect(&mut self, rect: Rect) {
        self.blit(rect.intersect(self.screen()));
    }

    fn blit(&mut self, rect: Rect) {
        if rect.is_empty() || self.framebuffer.address.is_null() {
            return;
        }
        let stride = self.framebuffer.stride as usize;
        let swap = self.framebuffer.swap_red_blue;
        let base = self.framebuffer.address;
        let source_width = self.width as usize;
        for y in rect.y..rect.bottom() {
            let source_start = (y as usize) * source_width + rect.x as usize;
            // SAFETY: the framebuffer covers `stride * height` pixels and the
            // rectangle is clipped to the screen.  The back buffer is large
            // enough for every pixel in the rectangle.
            unsafe {
                let destination = base.add((y as usize) * stride + rect.x as usize);
                let source = self.pixels.as_ptr().add(source_start);
                if swap {
                    for column in 0..rect.w as usize {
                        let pixel = *source.add(column);
                        let swapped = (pixel & 0xff00_ff00)
                            | ((pixel & 0x0000_00ff) << 16)
                            | ((pixel >> 16) & 0x0000_00ff);
                        core::ptr::write_volatile(destination.add(column), swapped);
                    }
                } else {
                    core::ptr::copy_nonoverlapping(source, destination, rect.w as usize);
                }
            }
        }
        // Make sure the stores are ordered after the back buffer reads.
        core::sync::atomic::fence(core::sync::atomic::Ordering::Release);
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

fn fill_row(row: &mut [u32], color: u32) {
    // An explicit loop lets LLVM emit SSE2 stores directly.  `slice::fill`
    // lowers to a `memset`-shaped loop which, in this freestanding kernel,
    // ended up far slower than the blended path for large spans.
    let mut index = 0;
    while index < row.len() {
        row[index] = color;
        index += 1;
    }
}

fn lerp_argb(first: u32, second: u32, t: u32) -> u32 {
    let inverse = 255 - t;
    let channel = |shift: u32| -> u32 {
        let a = (first >> shift) & 0xff;
        let b = (second >> shift) & 0xff;
        ((a * inverse + b * t + 127) / 255) << shift
    };
    channel(24) | channel(16) | channel(8) | channel(0)
}

fn blend_coverage(background: u32, current: u32, coverage: f32) -> u32 {
    if coverage <= 0.0 {
        return background;
    }
    if coverage >= 1.0 {
        return current;
    }
    let alpha = (coverage * 255.0) as u32;
    let inverse = 255 - alpha;
    let channel = |shift: u32| -> u32 {
        let b = (background >> shift) & 0xff;
        let c = (current >> shift) & 0xff;
        ((b * inverse + c * alpha + 127) / 255) << shift
    };
    channel(24) | channel(16) | channel(8) | channel(0)
}

fn point_in_polygon(x: i32, y: i32, points: &[(i32, i32)]) -> bool {
    let mut inside = false;
    let mut j = points.len() - 1;
    for i in 0..points.len() {
        let (xi, yi) = points[i];
        let (xj, yj) = points[j];
        let intersects = (yi > y) != (yj > y)
            && (x as f32) < (xj - xi) as f32 * (y - yi) as f32 / (yj - yi) as f32 + xi as f32;
        if intersects {
            inside = !inside;
        }
        j = i;
    }
    inside
}

fn distance_to_edges(x: i32, y: i32, points: &[(i32, i32)]) -> f32 {
    let mut closest = f32::MAX;
    for i in 0..points.len() {
        let (x0, y0) = points[i];
        let (x1, y1) = points[(i + 1) % points.len()];
        let dx = (x1 - x0) as f32;
        let dy = (y1 - y0) as f32;
        let length_squared = dx * dx + dy * dy;
        let t = if length_squared == 0.0 {
            0.0
        } else {
            (((x - x0) as f32 * dx + (y - y0) as f32 * dy) / length_squared).clamp(0.0, 1.0)
        };
        let px = x0 as f32 + dx * t;
        let py = y0 as f32 + dy * t;
        let distance =
            math::sqrt((x as f32 - px) * (x as f32 - px) + (y as f32 - py) * (y as f32 - py));
        if distance < closest {
            closest = distance;
        }
    }
    closest
}
