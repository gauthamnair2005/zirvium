//! Colours and metrics for the Zirvium desktop.
//!
//! All colours are packed `0xAARRGGBB`, matching DisplayJet's surface format.
//! The palette is deliberately cool and low-chroma so that the accent can be
//! the only saturated element on screen: dark blue-graphite surfaces, a small
//! ladder of text greys with clearly separated contrast steps, and a single
//! accent ramp used for focus, selection and the system status light.

/// Pack an opaque `0xRRGGBB` colour.
pub const fn rgb(value: u32) -> u32 {
    0xff00_0000 | (value & 0x00ff_ffff)
}

/// Pack a translucent `0xRRGGBB` colour with an 8-bit alpha.
pub const fn argb(alpha: u32, value: u32) -> u32 {
    ((alpha & 0xff) << 24) | (value & 0x00ff_ffff)
}

// ---------------------------------------------------------------------------
// Metrics
// ---------------------------------------------------------------------------

pub const TOP_BAR_HEIGHT: i32 = 28;
pub const DOCK_HEIGHT: i32 = 72;
pub const DOCK_ICON: i32 = 40;
pub const TITLE_HEIGHT: i32 = 32;
pub const WINDOW_RADIUS: i32 = 10;
pub const CONTENT_PADDING: i32 = 18;

// ---------------------------------------------------------------------------
// Surfaces
// ---------------------------------------------------------------------------

pub const BACKDROP: u32 = rgb(0x070a11);
pub const TOP_BAR: u32 = argb(0xe8, 0x0c0f16);
pub const TOP_BAR_BORDER: u32 = argb(0xff, 0x1e2432);
pub const PANEL: u32 = argb(0xf7, 0x11141c);
pub const PANEL_SOFT: u32 = argb(0xff, 0x181c26);
pub const PANEL_RAISED: u32 = argb(0xff, 0x212734);
pub const PANEL_GLASS: u32 = argb(0xdf, 0x0f131c);
pub const BORDER: u32 = argb(0xff, 0x2b3343);
pub const BORDER_SOFT: u32 = argb(0xff, 0x212836);
pub const WINDOW_BG: u32 = argb(0xff, 0x10131b);
pub const WINDOW_BG_DEEP: u32 = argb(0xff, 0x0b0e15);
pub const TITLE_BG: u32 = argb(0xff, 0x181c26);
pub const TITLE_BG_FOCUS: u32 = argb(0xff, 0x1e2432);
pub const SHADOW: u32 = argb(0xc0, 0x000000);

// ---------------------------------------------------------------------------
// Type
// ---------------------------------------------------------------------------

/// The four text steps form a perceptually even ladder on the dark surfaces:
/// primary, secondary, tertiary (labels) and quaternary (hints).
pub const TEXT: u32 = rgb(0xf1f4fb);
pub const TEXT_STRONG: u32 = rgb(0xffffff);
pub const TEXT_MUTED: u32 = rgb(0xa8b2c6);
pub const TEXT_DIM: u32 = rgb(0x788398);
pub const TEXT_FAINT: u32 = rgb(0x525c70);
pub const WHITE: u32 = rgb(0xffffff);

// ---------------------------------------------------------------------------
// Semantic colours
// ---------------------------------------------------------------------------

pub const SUCCESS: u32 = rgb(0x4ade9c);
pub const WARNING: u32 = rgb(0xf7c95c);
pub const DANGER: u32 = rgb(0xf4727b);
pub const CLOSE: u32 = rgb(0xff6b6b);
pub const MINIMIZE: u32 = rgb(0xffc857);
pub const MAXIMIZE: u32 = rgb(0x4ade80);

/// Accent colours offered in Settings.  Each is bright enough to read as
/// foreground text on the window surfaces as well as a fill behind dark text.
pub const ACCENTS: [(u32, &str); 6] = [
    (0x5aa2ff, "Azure"),
    (0x9b7bff, "Violet"),
    (0x35e0a1, "Mint"),
    (0xfb7185, "Rose"),
    (0xffc24d, "Amber"),
    (0x2ed8ee, "Cyan"),
];

/// Wallpaper styles.
pub const WALLPAPERS: [&str; 4] = ["Nebula", "Aurora", "Graphite", "Ember"];

#[derive(Clone, Copy, Debug)]
pub struct Theme {
    pub accent: u32,
    pub accent_alt: u32,
    pub wallpaper: u8,
    pub show_seconds: bool,
    pub animations: bool,
    pub dock_magnification: bool,
    pub show_grid: bool,
}

impl Default for Theme {
    fn default() -> Self {
        Self {
            accent: rgb(0x00aaff),
            accent_alt: rgb(0x66ccff),
            wallpaper: 0,
            show_seconds: true,
            animations: false,
            dock_magnification: false,
            show_grid: false,
        }
    }
}

impl Theme {
    pub fn accent_soft(&self, alpha: u32) -> u32 {
        argb(alpha, self.accent & 0x00ff_ffff)
    }

    pub fn accent_alt_soft(&self, alpha: u32) -> u32 {
        argb(alpha, self.accent_alt & 0x00ff_ffff)
    }

    /// The midpoint between the two accents, useful for gradients and glows.
    pub fn accent_mix(&self) -> u32 {
        let channel = |shift: u32| -> u32 {
            let a = (self.accent >> shift) & 0xff;
            let b = (self.accent_alt >> shift) & 0xff;
            ((a + b) / 2) << shift
        };
        rgb(channel(16) | channel(8) | channel(0))
    }

    /// A colour guaranteed to contrast with `background`, used for text placed
    /// on top of accents and status pills.
    pub fn on_color(&self, background: u32) -> u32 {
        let r = (background >> 16) & 0xff;
        let g = (background >> 8) & 0xff;
        let b = background & 0xff;
        let luma = (r * 299 + g * 587 + b * 114) / 1000;
        if luma > 148 {
            rgb(0x0a0d14)
        } else {
            rgb(0xffffff)
        }
    }
}
