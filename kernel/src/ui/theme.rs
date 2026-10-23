//! Colours and metrics for the Zirvium desktop.
//!
//! All colours are packed `0xAARRGGBB`, matching DisplayJet's surface format.

/// Pack an opaque `0xRRGGBB` colour.
pub const fn rgb(value: u32) -> u32 {
    0xff00_0000 | (value & 0x00ff_ffff)
}

/// Pack a translucent `0xRRGGBB` colour with an 8-bit alpha.
pub const fn argb(alpha: u32, value: u32) -> u32 {
    ((alpha & 0xff) << 24) | (value & 0x00ff_ffff)
}

pub const TOP_BAR_HEIGHT: i32 = 34;
pub const DOCK_HEIGHT: i32 = 74;
pub const DOCK_ICON: i32 = 42;
pub const TITLE_HEIGHT: i32 = 36;
pub const WINDOW_RADIUS: i32 = 12;
pub const CONTENT_PADDING: i32 = 16;

pub const BACKDROP: u32 = rgb(0x0b0d14);
pub const TOP_BAR: u32 = argb(0xf2, 0x14161d);
pub const TOP_BAR_BORDER: u32 = argb(0xff, 0x242832);
pub const PANEL: u32 = argb(0xfa, 0x161922);
pub const PANEL_SOFT: u32 = argb(0xff, 0x1c2029);
pub const PANEL_RAISED: u32 = argb(0xff, 0x232834);
pub const BORDER: u32 = argb(0xff, 0x2d3340);
pub const BORDER_SOFT: u32 = argb(0xff, 0x252a35);
pub const WINDOW_BG: u32 = argb(0xff, 0x151820);
pub const TITLE_BG: u32 = argb(0xff, 0x1c2029);
pub const TEXT: u32 = rgb(0xe8ebf2);
pub const TEXT_MUTED: u32 = rgb(0x9aa3b5);
pub const TEXT_DIM: u32 = rgb(0x6b7488);
pub const TEXT_FAINT: u32 = rgb(0x4b5364);
pub const WHITE: u32 = rgb(0xffffff);
pub const SUCCESS: u32 = rgb(0x3ddc97);
pub const WARNING: u32 = rgb(0xf5c451);
pub const DANGER: u32 = rgb(0xf07178);
pub const CLOSE: u32 = rgb(0xff615e);
pub const MINIMIZE: u32 = rgb(0xf5c451);
pub const MAXIMIZE: u32 = rgb(0x3ddc97);

/// Accent colours offered in Settings.
pub const ACCENTS: [(u32, &str); 6] = [
    (0x5aa2ff, "Azure"),
    (0x8b5cf6, "Violet"),
    (0x34d399, "Mint"),
    (0xfb7185, "Rose"),
    (0xfbbf24, "Amber"),
    (0x22d3ee, "Cyan"),
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
            accent: rgb(ACCENTS[0].0),
            accent_alt: rgb(ACCENTS[1].0),
            wallpaper: 0,
            show_seconds: true,
            animations: true,
            dock_magnification: true,
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

    pub fn accent_mix(&self) -> u32 {
        let inverse = 255;
        let channel = |shift: u32| -> u32 {
            let a = (self.accent >> shift) & 0xff;
            let b = (self.accent_alt >> shift) & 0xff;
            (a * inverse + b * (255 - inverse)) / 255 << shift
        };
        channel(16) | channel(8) | channel(0)
    }
}
