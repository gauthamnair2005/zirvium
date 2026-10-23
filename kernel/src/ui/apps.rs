//! The Zirvium desktop applications.
//!
//! Each application owns its state and draws into a clipped content rectangle.
//! The desktop only routes input and decides when a repaint is needed.

use alloc::format;
use alloc::string::{String, ToString};
use alloc::vec::Vec;

use zirvstdlib::abi::{NodeState, key};
use zirvstdlib::geometry::{Point, Rect};
use zirvstdlib::math;

use crate::displayjet::Display;
use crate::mosix::vfs;
use crate::ui::theme::*;
use crate::ui::widgets::{self, Icon};

/// What an application wants the desktop to do after an interaction.
#[derive(Clone, Copy, Debug, Default)]
pub struct Response {
    pub redraw: bool,
    pub theme_changed: bool,
}

impl Response {
    pub const NONE: Self = Self {
        redraw: false,
        theme_changed: false,
    };
    pub const REDRAW: Self = Self {
        redraw: true,
        theme_changed: false,
    };
    pub const THEME: Self = Self {
        redraw: true,
        theme_changed: true,
    };
}

#[allow(clippy::too_many_arguments)]
fn stat_card(
    display: &mut Display,
    rect: Rect,
    theme: &Theme,
    icon: Icon,
    label: &str,
    value: &str,
    detail: &str,
    accent: u32,
) {
    widgets::card(display, rect, theme);
    let badge = Rect::new(rect.x + 13, rect.y + 13, 28, 28);
    display.fill_rounded(badge, 8, argb(0x2c, accent & 0x00ff_ffff));
    display.stroke_rounded(badge, 8, 1, argb(0x38, accent & 0x00ff_ffff));
    widgets::icon(display, badge.inset(5), icon, accent);
    display.text(rect.x + 48, rect.y + 17, label, 10, true, TEXT_MUTED);
    display.text(rect.x + 13, rect.y + 36, value, 23, true, TEXT_STRONG);
    display.text_ellipsized(
        rect.x + 13,
        rect.y + 66,
        detail,
        rect.w - 26,
        10,
        false,
        TEXT_DIM,
    );
}

// ---------------------------------------------------------------------------
// System monitor
// ---------------------------------------------------------------------------

pub struct Monitor {
    samples: Vec<u32>,
    last_sample_ms: u64,
    last_redraw_ms: u64,
}

impl Monitor {
    pub fn new() -> Self {
        let samples = alloc::vec![0; 72];
        Self {
            samples,
            last_sample_ms: 0,
            last_redraw_ms: 0,
        }
    }

    pub fn tick(&mut self) -> bool {
        let now = zirvstdlib::time::monotonic_ms();
        if now.saturating_sub(self.last_sample_ms) >= 250 {
            self.last_sample_ms = now;
            self.samples.remove(0);
            self.samples.push(crate::mosix::load_permille());
        }
        if now.saturating_sub(self.last_redraw_ms) >= 500 {
            self.last_redraw_ms = now;
            return true;
        }
        false
    }

    pub fn draw(&mut self, display: &mut Display, content: Rect, theme: &Theme, _focused: bool) {
        let inner = content.inset(16);
        if inner.w < 240 || inner.h < 200 {
            return;
        }
        let stats = crate::mm::stats();
        let load = crate::mosix::load_permille();
        let heap_ratio = if stats.heap_total == 0 {
            0.0
        } else {
            stats.heap_used as f32 / stats.heap_total as f32
        };

        let wide = inner.w >= 520;
        let columns: usize = if wide { 4 } else { 2 };
        let gap = 10;
        let card_width = (inner.w - gap * (columns as i32 - 1)) / columns as i32;
        let card_height = 88;
        let rows = if wide { 1 } else { 2 };
        let cards = [
            (
                Icon::Cpu,
                "COMPOSITOR",
                format!("{}%", load / 10),
                String::from("DisplayJet frame load"),
                theme.accent,
            ),
            (
                Icon::Memory,
                "HEAP",
                format!("{:.0}%", heap_ratio * 100.0),
                format!(
                    "{} of {}",
                    widgets::byte_size(stats.heap_used as u64),
                    widgets::byte_size(stats.heap_total as u64)
                ),
                rgb(0x8b5cf6),
            ),
            (
                Icon::Nodes,
                "PROCESSES",
                format!("{}", crate::mosix::process_count()),
                format!("{} node(s) online", crate::mosix::node_count()),
                SUCCESS,
            ),
            (
                Icon::Clock,
                "UPTIME",
                String::from(crate::time::uptime_string().as_str()),
                format!("{} ticks", crate::arch::pit::ticks()),
                rgb(0xf5c451),
            ),
        ];
        for (index, card) in cards.iter().enumerate() {
            let row = index / columns;
            let column = index % columns;
            let rect = Rect::new(
                inner.x + column as i32 * (card_width + gap),
                inner.y + row as i32 * (card_height + gap),
                card_width,
                card_height,
            );
            stat_card(
                display, rect, theme, card.0, card.1, &card.2, &card.3, card.4,
            );
        }

        let mut y = inner.y + rows * (card_height + gap) + 2;
        let graph_height = if inner.h > 420 { 150 } else { 112 };
        let side_by_side = inner.w >= 620;
        let graph_width = if side_by_side {
            inner.w * 62 / 100
        } else {
            inner.w
        };
        let graph = Rect::new(inner.x, y, graph_width, graph_height);
        self.draw_graph(display, graph, theme, load);

        if side_by_side {
            let memory = Rect::new(
                inner.x + graph_width + gap,
                y,
                inner.w - graph_width - gap,
                graph_height,
            );
            self.draw_memory(display, memory, theme, &stats, heap_ratio);
        }
        y += graph_height + gap;

        let remaining = inner.bottom() - y;
        if remaining >= 96 {
            let table = Rect::new(inner.x, y, inner.w, remaining);
            self.draw_processes(display, table, theme);
        }
    }

    fn draw_graph(&mut self, display: &mut Display, rect: Rect, theme: &Theme, load: u32) {
        widgets::card(display, rect, theme);
        display.text(rect.x + 16, rect.y + 13, "CPU load", 13, true, TEXT);
        let value = format!("{}%", load / 10);
        display.text(
            rect.right() - 16 - widgets::text_width(&value, 15, true),
            rect.y + 11,
            &value,
            15,
            true,
            theme.accent,
        );
        let plot = Rect::new(rect.x + 16, rect.y + 40, rect.w - 32, rect.h - 56);
        if plot.w <= 2 || plot.h <= 2 {
            return;
        }
        for step in 1..4 {
            let y = plot.y + plot.h * step / 4;
            display.fill_rect(Rect::new(plot.x, y, plot.w, 1), argb(0x24, 0xffffff));
        }
        for step in 1..4 {
            let x = plot.x + plot.w * step / 4;
            display.fill_rect(Rect::new(x, plot.y, 1, plot.h), argb(0x14, 0xffffff));
        }

        let count = self.samples.len();
        if count < 2 {
            return;
        }
        let last = self.samples[count - 1] as f32 / 1000.0;
        let mut previous: Option<(i32, i32)> = None;
        for x in 0..plot.w {
            let position = (x as f32 / (plot.w - 1) as f32) * (count - 1) as f32;
            let index = position as usize;
            let fraction = position - index as f32;
            let a = self.samples[index] as f32 / 1000.0;
            let b = self
                .samples
                .get(index + 1)
                .map(|value| *value as f32 / 1000.0)
                .unwrap_or(a);
            let value = a + (b - a) * fraction;
            let y = plot.bottom() - (value * plot.h as f32) as i32;
            display.fill_rect(
                Rect::new(plot.x + x, y, 1, (plot.bottom() - y).max(1)),
                theme.accent_soft(26),
            );
            if let Some((px, py)) = previous {
                widgets::line(display, px, py, plot.x + x, y, 2, theme.accent);
            }
            previous = Some((plot.x + x, y));
        }
        let marker = plot.bottom() - (last * plot.h as f32) as i32;
        display.fill_circle(Point::new(plot.right() - 1, marker), 4, theme.accent);
        display.fill_circle(Point::new(plot.right() - 1, marker), 2, WHITE);
    }

    fn draw_memory(
        &mut self,
        display: &mut Display,
        rect: Rect,
        theme: &Theme,
        stats: &crate::mm::MemoryStats,
        heap_ratio: f32,
    ) {
        widgets::card(display, rect, theme);
        display.text(rect.x + 16, rect.y + 13, "Memory", 13, true, TEXT);
        let mut y = rect.y + 42;
        let physical_ratio = if stats.total_bytes == 0 {
            0.0
        } else {
            (stats.total_bytes - stats.free_bytes) as f32 / stats.total_bytes as f32
        };
        let rows = [
            ("Heap", heap_ratio, theme.accent),
            ("Physical", physical_ratio, rgb(0x8b5cf6)),
        ];
        for (label, ratio, color) in rows {
            display.text(rect.x + 16, y, label, 12, false, TEXT_MUTED);
            let value = format!("{:.0}%", ratio * 100.0);
            display.text(
                rect.right() - 16 - widgets::text_width(&value, 12, true),
                y,
                &value,
                12,
                true,
                TEXT,
            );
            widgets::progress(
                display,
                Rect::new(rect.x + 16, y + 18, rect.w - 32, 7),
                ratio,
                color,
            );
            y += 34;
        }
        let gpu = crate::drivers::pci::gpu();
        display.text_ellipsized(
            rect.x + 16,
            y + 4,
            gpu.name_str(),
            rect.w - 32,
            11,
            false,
            TEXT_MUTED,
        );
        display.text_ellipsized(
            rect.x + 16,
            y + 22,
            "DisplayJet ARGB · CPU SIMD",
            rect.w - 32,
            10,
            false,
            TEXT_DIM,
        );
    }

    fn draw_processes(&mut self, display: &mut Display, rect: Rect, theme: &Theme) {
        widgets::card(display, rect, theme);
        display.text(rect.x + 14, rect.y + 12, "MOSIX processes", 12, true, TEXT);
        let count = crate::mosix::process_count();
        let label = format!("{count} total");
        display.text(
            rect.right() - 14 - widgets::text_width(&label, 11, false),
            rect.y + 13,
            &label,
            11,
            false,
            TEXT_DIM,
        );
        if rect.h < 70 {
            return;
        }
        let header_y = rect.y + 36;
        display.text(rect.x + 14, header_y, "PID", 10, true, TEXT_DIM);
        display.text(rect.x + 56, header_y, "STATE", 10, true, TEXT_DIM);
        display.text(rect.x + 132, header_y, "NAME", 10, true, TEXT_DIM);
        display.text(rect.right() - 112, header_y, "NODE", 10, true, TEXT_DIM);
        display.text(rect.right() - 62, header_y, "TICKS", 10, true, TEXT_DIM);
        display.fill_rect(
            Rect::new(rect.x + 14, header_y + 16, rect.w - 28, 1),
            BORDER_SOFT,
        );
        let row_height = 24;
        let available = ((rect.bottom() - (header_y + 22)) / row_height).max(0) as usize;
        for (index, process) in crate::mosix::processes().iter().take(available).enumerate() {
            let y = header_y + 22 + index as i32 * row_height;
            if index % 2 == 1 {
                display.fill_rect(
                    Rect::new(rect.x + 10, y - 2, rect.w - 20, row_height - 2),
                    argb(0x18, 0xffffff),
                );
            }
            display.text(
                rect.x + 14,
                y,
                &format!("{}", process.pid),
                11,
                false,
                theme.accent,
            );
            let state_color = match process.state {
                zirvstdlib::abi::ProcessState::Running => SUCCESS,
                zirvstdlib::abi::ProcessState::Ready => TEXT,
                zirvstdlib::abi::ProcessState::Sleeping => TEXT_DIM,
                zirvstdlib::abi::ProcessState::Blocked => WARNING,
                zirvstdlib::abi::ProcessState::Migrating => theme.accent_alt,
                _ => TEXT_FAINT,
            };
            display.text(
                rect.x + 56,
                y,
                crate::mosix::process_state_name(process.state),
                11,
                false,
                state_color,
            );
            display.text_ellipsized(
                rect.x + 132,
                y,
                process.name_str(),
                (rect.w - 132 - 120).max(40),
                11,
                false,
                TEXT,
            );
            display.text(
                rect.right() - 112,
                y,
                &format!("{}", process.current_node),
                11,
                false,
                TEXT_MUTED,
            );
            display.text(
                rect.right() - 62,
                y,
                &format!("{}", process.cpu_ticks),
                11,
                false,
                TEXT_MUTED,
            );
        }
    }
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------

pub struct Files {
    cwd: String,
    entries: Vec<vfs::Entry>,
    selected: Option<usize>,
    preview: Option<String>,
    preview_scroll: i32,
    scroll: usize,
    status: String,
}

impl Files {
    pub fn new() -> Self {
        let mut files = Self {
            cwd: String::from("/home/zirvium"),
            entries: Vec::new(),
            selected: None,
            preview: None,
            preview_scroll: 0,
            scroll: 0,
            status: String::new(),
        };
        files.reload();
        files
    }

    fn reload(&mut self) {
        self.entries = vfs::list(&self.cwd);
        self.selected = None;
        self.preview = None;
        self.preview_scroll = 0;
        self.scroll = 0;
        self.status = format!("{} item(s)", self.entries.len());
    }

    fn open(&mut self, path: &str) {
        if !vfs::is_dir(path) {
            return;
        }
        self.cwd = vfs::normalize(path);
        self.reload();
    }

    fn parent(&mut self) {
        if self.cwd == "/" {
            return;
        }
        let trimmed = self.cwd.trim_end_matches('/');
        let parent = match trimmed.rfind('/') {
            Some(0) | None => String::from("/"),
            Some(index) => trimmed[..index].to_string(),
        };
        self.open(&parent);
    }

    fn select(&mut self, index: usize) {
        let Some(entry) = self.entries.get(index).cloned() else {
            return;
        };
        self.selected = Some(index);
        self.preview_scroll = 0;
        if entry.is_dir {
            self.preview = Some(String::new());
            self.status = format!("{}/ · directory", entry.name);
        } else {
            match vfs::read_limited(&entry.path, 64 * 1024) {
                Some(content) => {
                    self.status = format!("{} · {}", entry.name, widgets::byte_size(entry.size));
                    self.preview = Some(content);
                }
                None => {
                    self.status = format!("{}: unreadable", entry.name);
                    self.preview = None;
                }
            }
        }
    }

    fn activate(&mut self, index: usize) {
        let Some(entry) = self.entries.get(index).cloned() else {
            return;
        };
        if entry.is_dir {
            self.open(&entry.path);
        } else {
            self.select(index);
        }
    }

    fn layout(&self, content: Rect) -> (Rect, Rect, Rect, i32) {
        let padding = 16;
        let header = Rect::new(
            content.x + padding,
            content.y + 14,
            content.w - padding * 2,
            36,
        );
        let body_top = header.bottom() + 10;
        let body_height = (content.bottom() - 14 - body_top).max(40);
        let list_width = if content.w >= 620 {
            (content.w - padding * 2) * 58 / 100
        } else {
            content.w - padding * 2
        };
        let list = Rect::new(header.x, body_top, list_width, body_height);
        let preview = Rect::new(
            list.right() + 10,
            body_top,
            header.right() - list.right() - 10,
            body_height,
        );
        let row_height = 42;
        (header, list, preview, row_height)
    }

    pub fn draw(&mut self, display: &mut Display, content: Rect, theme: &Theme, _focused: bool) {
        if content.w < 300 || content.h < 200 {
            return;
        }
        let (header, list, preview, row_height) = self.layout(content);
        // Header: up button, path, count.
        let up = Rect::new(header.x, header.y, 62, 34);
        widgets::button(
            display,
            up,
            "Up",
            theme,
            widgets::ButtonStyle::Subtle,
            false,
            self.cwd != "/",
        );
        widgets::icon(
            display,
            Rect::new(up.x + 10, up.y + 10, 14, 14),
            Icon::Up,
            TEXT_MUTED,
        );
        display.text_ellipsized(
            header.x + 76,
            header.y + 7,
            &self.cwd,
            (header.w - 220).max(40),
            15,
            true,
            TEXT_STRONG,
        );
        display.text(
            header.right() - widgets::text_width(&self.status, 11, false),
            header.y + 11,
            &self.status,
            11,
            false,
            TEXT_DIM,
        );

        // File list.
        widgets::card(display, list, theme);
        let inner = list.inset(6);
        let visible = ((inner.h - 8) / row_height).max(1) as usize;
        self.scroll = self.scroll.min(
            self.entries
                .len()
                .saturating_sub(visible.min(self.entries.len())),
        );
        let start = self.scroll.min(self.entries.len());
        for (offset, entry) in self.entries.iter().skip(start).take(visible).enumerate() {
            let index = start + offset;
            let row = Rect::new(
                inner.x + 2,
                inner.y + 4 + offset as i32 * row_height,
                inner.w - 4,
                row_height - 4,
            );
            let selected = self.selected == Some(index);
            if selected {
                display.fill_rounded(row, 7, theme.accent_soft(38));
                display.stroke_rounded(row, 7, 1, theme.accent_soft(90));
            }
            let icon_rect = Rect::new(row.x + 9, row.y + 8, 24, 24);
            let icon = if entry.is_dir {
                Icon::Files
            } else {
                Icon::Document
            };
            let color = if entry.is_dir {
                theme.accent
            } else {
                TEXT_MUTED
            };
            widgets::icon(display, icon_rect, icon, color);
            display.text_ellipsized(
                row.x + 42,
                row.y + 7,
                &entry.name,
                row.w - 116,
                14,
                entry.is_dir,
                TEXT,
            );
            let detail = if entry.is_dir {
                String::from("folder")
            } else {
                widgets::byte_size(entry.size)
            };
            display.text(
                row.right() - 10 - widgets::text_width(&detail, 10, false),
                row.y + 9,
                &detail,
                10,
                false,
                TEXT_DIM,
            );
            if entry.is_dir {
                widgets::row_chevron(display, row, TEXT_FAINT);
            }
        }
        if self.entries.is_empty() {
            display.text(
                inner.x + 16,
                inner.y + 20,
                "This folder is empty.",
                12,
                false,
                TEXT_DIM,
            );
        }
        if self.entries.len() > visible {
            let track = Rect::new(list.right() - 7, list.y + 8, 3, list.h - 16);
            display.fill_rounded(track, 2, argb(0x30, 0xffffff));
            let length =
                ((track.h as f32) * visible as f32 / self.entries.len() as f32).max(18.0) as i32;
            let max_scroll = self.entries.len() - visible;
            let offset = (((track.h - length) as f32) * self.scroll as f32
                / max_scroll.max(1) as f32) as i32;
            display.fill_rounded(
                Rect::new(track.x, track.y + offset, 3, length),
                2,
                theme.accent_soft(180),
            );
        }

        // Preview.
        if preview.w > 60 {
            widgets::card(display, preview, theme);
            self.draw_preview(display, preview, theme);
        }
    }

    fn draw_preview(&mut self, display: &mut Display, rect: Rect, theme: &Theme) {
        let inner = rect.inset(14);
        let Some(index) = self.selected else {
            widgets::icon(
                display,
                Rect::new(inner.center().x - 22, inner.y + inner.h / 2 - 40, 44, 44),
                Icon::Document,
                TEXT_FAINT,
            );
            display.text_centered(
                Rect::new(inner.x, inner.y + inner.h / 2 + 8, inner.w, 20),
                "Select a file to preview",
                12,
                false,
                TEXT_DIM,
            );
            return;
        };
        let Some(entry) = self.entries.get(index) else {
            return;
        };
        display.text_ellipsized(inner.x, inner.y, &entry.name, inner.w, 16, true, TEXT);
        let meta = if entry.is_dir {
            String::from("Directory")
        } else {
            format!("{} · text", widgets::byte_size(entry.size))
        };
        display.text(inner.x, inner.y + 24, &meta, 10, false, TEXT_DIM);
        display.fill_rect(Rect::new(inner.x, inner.y + 42, inner.w, 1), BORDER_SOFT);
        let body = Rect::new(inner.x, inner.y + 52, inner.w, inner.h - 52);
        if entry.is_dir {
            let children = vfs::list(&entry.path);
            let mut y = body.y
                - self
                    .preview_scroll
                    .saturating_mul(15)
                    .min(children.len() as i32 * 15);
            for child in children.iter().take(64) {
                if y > body.bottom() - 14 {
                    break;
                }
                let icon = if child.is_dir {
                    Icon::Files
                } else {
                    Icon::Document
                };
                widgets::icon(
                    display,
                    Rect::new(body.x, y + 1, 12, 12),
                    icon,
                    if child.is_dir { theme.accent } else { TEXT_DIM },
                );
                display.text_ellipsized(
                    body.x + 20,
                    y,
                    &child.name,
                    body.w - 20,
                    12,
                    child.is_dir,
                    TEXT,
                );
                y += 17;
            }
            return;
        }
        let Some(content) = self.preview.as_ref() else {
            return;
        };
        let face = crate::displayjet::font::face(14, false);
        let line_height = face.line_height + 2;
        let content = content.trim_end();
        let mut y = body.y - self.preview_scroll * line_height;
        let max_lines = (body.h / line_height).max(1) as usize;
        let mut drawn = 0;
        let mut line_count = 0usize;
        for paragraph in content.split('\n') {
            let wrapped = widgets::wrap(face, paragraph, body.w);
            for line in wrapped {
                line_count += 1;
                if y + line_height >= body.y && y < body.bottom() && drawn < max_lines + 2 {
                    display.text_face(body.x, y + face.ascent, &line, face, TEXT_MUTED);
                    drawn += 1;
                }
                y += line_height;
            }
        }
        let max_scroll = (line_count as i32 - max_lines as i32).max(0);
        self.preview_scroll = self.preview_scroll.min(max_scroll);
    }

    pub fn pointer(&mut self, point: Point, content: Rect, _theme: &mut Theme) -> Response {
        let (header, list, _preview, row_height) = self.layout(content);
        let up = Rect::new(header.x, header.y, 62, 34);
        if up.contains(point) && self.cwd != "/" {
            self.parent();
            return Response::REDRAW;
        }
        if list.contains(point) {
            let inner = list.inset(6);
            let offset = (point.y - (inner.y + 4)) / row_height;
            if offset >= 0 {
                let index = self.scroll + offset as usize;
                if index < self.entries.len() {
                    if self.entries[index].is_dir || self.selected == Some(index) {
                        self.activate(index);
                    } else {
                        self.select(index);
                    }
                    return Response::REDRAW;
                }
            }
        }
        Response::NONE
    }

    pub fn scroll(&mut self, point: Point, content: Rect, amount: i32) {
        let (_, list, preview, row_height) = self.layout(content);
        let visible = ((list.h - 14) / row_height).max(1) as usize;
        if list.contains(point) {
            if amount > 0 {
                self.scroll =
                    (self.scroll + amount as usize).min(self.entries.len().saturating_sub(visible));
            } else {
                self.scroll = self.scroll.saturating_sub((-amount) as usize);
            }
        } else if preview.contains(point) {
            self.preview_scroll = (self.preview_scroll - amount * 2).max(0);
        }
    }

    pub fn key(&mut self, code: u32, _modifiers: u16) -> bool {
        match code {
            key::BACKSPACE => {
                self.parent();
                true
            }
            key::UP => {
                if self.entries.is_empty() {
                    return true;
                }
                let next = match self.selected {
                    None => 0,
                    Some(index) => index.saturating_sub(1),
                };
                self.select(next);
                true
            }
            key::DOWN => {
                if self.entries.is_empty() {
                    return true;
                }
                let next = match self.selected {
                    None => 0,
                    Some(index) => (index + 1).min(self.entries.len() - 1),
                };
                self.select(next);
                true
            }
            key::ENTER => {
                if let Some(index) = self.selected {
                    self.activate(index);
                }
                true
            }
            _ => false,
        }
    }
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

#[derive(Clone, Copy)]
enum Action {
    Wallpaper(u8),
    Accent(usize),
    Seconds,
    Animations,
    Dock,
    Grid,
    AddPeer,
    Migrate(u32, u32),
}

struct Hit {
    rect: Rect,
    action: Action,
}

pub struct Settings {
    hits: Vec<Hit>,
    scroll: i32,
}

impl Settings {
    pub fn new() -> Self {
        Self {
            hits: Vec::new(),
            scroll: 0,
        }
    }

    pub fn draw(&mut self, display: &mut Display, content: Rect, theme: &Theme, _focused: bool) {
        self.hits.clear();
        let inner = content.inset(16);
        if inner.w < 260 {
            return;
        }
        let mut y = inner.y - self.scroll;
        widgets::section_header(
            display,
            inner.x,
            y,
            "Settings",
            "Make the desktop yours. Changes apply immediately.",
            theme,
        );
        y += 62;

        // Wallpaper card.
        let wall_card = Rect::new(inner.x, y, inner.w, 104);
        widgets::card(display, wall_card, theme);
        display.text(inner.x + 14, y + 12, "Wallpaper", 14, true, TEXT);
        let option_width = (inner.w - 28 - 3 * 8) / 4;
        for (index, name) in WALLPAPERS.iter().enumerate() {
            let rect = Rect::new(
                inner.x + 14 + index as i32 * (option_width + 8),
                y + 32,
                option_width,
                58,
            );
            let selected = theme.wallpaper as usize == index;
            let base = match index {
                1 => rgb(0x0b2a30),
                2 => rgb(0x171a21),
                3 => rgb(0x2a1210),
                _ => rgb(0x121a2c),
            };
            display.fill_rounded(rect, 7, base);
            display.horizontal_gradient(
                Rect::new(rect.x + 2, rect.y + 2, rect.w - 4, 30),
                argb(0x50, theme.accent & 0x00ff_ffff),
                argb(0x20, theme.accent_alt & 0x00ff_ffff),
            );
            display.stroke_rounded(
                rect,
                7,
                if selected { 2 } else { 1 },
                if selected { theme.accent } else { BORDER },
            );
            if selected {
                display.fill_circle(Point::new(rect.right() - 11, rect.y + 11), 6, theme.accent);
                widgets::icon(
                    display,
                    Rect::new(rect.right() - 15, rect.y + 7, 8, 8),
                    Icon::Check,
                    rgb(0x06101f),
                );
            }
            display.text_centered(
                Rect::new(rect.x, rect.bottom() - 20, rect.w, 14),
                name,
                10,
                selected,
                if selected { TEXT } else { TEXT_MUTED },
            );
            self.hits.push(Hit {
                rect,
                action: Action::Wallpaper(index as u8),
            });
        }
        y = wall_card.bottom() + 10;

        // Accent card.
        let accent_card = Rect::new(inner.x, y, inner.w, 76);
        widgets::card(display, accent_card, theme);
        display.text(inner.x + 14, y + 12, "Accent colour", 14, true, TEXT);
        for (index, (value, name)) in ACCENTS.iter().enumerate() {
            let center = Point::new(inner.x + 30 + index as i32 * 44, y + 46);
            let color = rgb(*value);
            let selected = (theme.accent & 0x00ff_ffff) == *value;
            if selected {
                display.fill_circle(center, 15, argb(0x50, *value));
            }
            display.fill_circle(center, 11, color);
            if selected {
                widgets::icon(
                    display,
                    Rect::new(center.x - 6, center.y - 6, 12, 12),
                    Icon::Check,
                    rgb(0x0a0c12),
                );
            }
            display.text(
                center.x - widgets::text_width(name, 9, false) / 2,
                y + 58,
                name,
                9,
                false,
                if selected { TEXT_MUTED } else { TEXT_FAINT },
            );
            self.hits.push(Hit {
                rect: Rect::new(center.x - 16, center.y - 16, 32, 32),
                action: Action::Accent(index),
            });
        }
        y = accent_card.bottom() + 10;

        // Behaviour rows.
        let rows = [
            (
                "Show seconds in the clock",
                theme.show_seconds,
                Action::Seconds,
            ),
            (
                "Animate window and dock changes",
                theme.animations,
                Action::Animations,
            ),
            (
                "Magnify dock icons on hover",
                theme.dock_magnification,
                Action::Dock,
            ),
            (
                "Show alignment grid on the desktop",
                theme.show_grid,
                Action::Grid,
            ),
        ];
        let behaviour = Rect::new(inner.x, y, inner.w, 24 + rows.len() as i32 * 38);
        widgets::card(display, behaviour, theme);
        display.text(inner.x + 14, y + 12, "Behaviour", 14, true, TEXT);
        for (index, (label, value, action)) in rows.iter().enumerate() {
            let row_y = y + 32 + index as i32 * 38;
            display.text(inner.x + 14, row_y + 8, label, 13, false, TEXT);
            let toggle = Rect::new(inner.right() - 56, row_y + 3, 38, 21);
            widgets::toggle(display, toggle, *value, theme);
            self.hits.push(Hit {
                rect: Rect::new(inner.x + 14, row_y, inner.w - 28, 30),
                action: *action,
            });
        }
        y = behaviour.bottom() + 10;

        // Display and system card.
        let info = Rect::new(inner.x, y, inner.w, 132);
        widgets::card(display, info, theme);
        display.text(inner.x + 14, y + 12, "Display & system", 14, true, TEXT);
        let width = crate::ui::WIDTH_HINT.load(core::sync::atomic::Ordering::Relaxed);
        let height = crate::ui::HEIGHT_HINT.load(core::sync::atomic::Ordering::Relaxed);
        let gpu = crate::drivers::pci::gpu();
        let fields = [
            ("Resolution", format!("{width} x {height}")),
            ("Renderer", String::from("DisplayJet · CPU SIMD surface")),
            ("GPU", gpu.name_str().to_string()),
            (
                "Cluster",
                format!(
                    "MOSIX · {} node(s) · {} process(es)",
                    crate::mosix::node_count(),
                    crate::mosix::process_count()
                ),
            ),
        ];
        for (index, (label, value)) in fields.iter().enumerate() {
            let row_y = y + 36 + index as i32 * 23;
            display.text(inner.x + 14, row_y, label, 11, false, TEXT_DIM);
            display.text_ellipsized(inner.x + 110, row_y, value, inner.w - 124, 11, false, TEXT);
        }
        y = info.bottom() + 10;

        display.text(
            inner.x,
            y,
            "Zirvium 2.0 · Rust and x86-64 assembly · no C, no 32-bit code",
            10,
            false,
            TEXT_FAINT,
        );
        let total_height = (y + 20) - (inner.y - self.scroll);
        let maximum = (total_height - inner.h).max(0);
        self.scroll = self.scroll.min(maximum);
        if total_height > inner.h {
            let track = Rect::new(inner.right() - 3, inner.y, 3, inner.h);
            display.fill_rounded(track, 2, argb(0x28, 0xffffff));
            let length = ((inner.h as f32) * inner.h as f32 / total_height as f32).max(20.0) as i32;
            let offset =
                (((inner.h - length) as f32) * self.scroll as f32 / maximum.max(1) as f32) as i32;
            display.fill_rounded(
                Rect::new(track.x, track.y + offset, 3, length),
                2,
                theme.accent_soft(180),
            );
        }
    }

    pub fn pointer(&mut self, point: Point, _content: Rect, theme: &mut Theme) -> Response {
        for hit in &self.hits {
            if !hit.rect.contains(point) {
                continue;
            }
            match hit.action {
                Action::Wallpaper(style) => {
                    if theme.wallpaper != style {
                        theme.wallpaper = style;
                        return Response::THEME;
                    }
                }
                Action::Accent(index) => {
                    if let Some((value, _)) = ACCENTS.get(index) {
                        let color = rgb(*value);
                        if theme.accent != color {
                            theme.accent = color;
                            theme.accent_alt = rgb(ACCENTS[(index + 1) % ACCENTS.len()].0);
                            return Response::THEME;
                        }
                    }
                }
                Action::Seconds => {
                    theme.show_seconds = !theme.show_seconds;
                    return Response::THEME;
                }
                Action::Animations => {
                    theme.animations = !theme.animations;
                    return Response::THEME;
                }
                Action::Dock => {
                    theme.dock_magnification = !theme.dock_magnification;
                    return Response::THEME;
                }
                Action::Grid => {
                    theme.show_grid = !theme.show_grid;
                    return Response::THEME;
                }
                _ => {}
            }
            return Response::REDRAW;
        }
        Response::NONE
    }

    pub fn scroll(&mut self, _point: Point, _content: Rect, amount: i32) {
        self.scroll = (self.scroll - amount * 24).max(0);
    }
}

// ---------------------------------------------------------------------------
// About
// ---------------------------------------------------------------------------

pub struct About;

impl About {
    pub fn new() -> Self {
        Self
    }

    pub fn draw(&mut self, display: &mut Display, content: Rect, theme: &Theme, _focused: bool) {
        let inner = content.inset(22);
        if inner.w < 320 || inner.h < 240 {
            return;
        }
        let mark = Rect::new(inner.x, inner.y, 84, 84);
        display.radial_glow(mark.center(), 110, theme.accent_soft(255), 0.30);
        display.fill_rounded_vertical(mark, 20, theme.accent_soft(64), theme.accent_alt_soft(24));
        display.stroke_rounded(mark, 20, 1, theme.accent_soft(170));
        widgets::icon(display, mark.inset(19), Icon::Logo, theme.accent);
        display.text(inner.x + 102, inner.y, "Zirvium 2.0", 28, true, TEXT_STRONG);
        display.text(
            inner.x + 104,
            inner.y + 40,
            "one cluster, one display",
            14,
            false,
            theme.accent,
        );
        display.text(
            inner.x + 104,
            inner.y + 62,
            "A 64-bit operating system written from scratch.",
            11,
            false,
            TEXT_MUTED,
        );
        let mut y = inner.y + 104;
        display.horizontal_gradient(
            Rect::new(inner.x, y, inner.w, 1),
            theme.accent_soft(120),
            argb(0x20, 0xffffff),
        );
        y += 16;
        let ideas = [
            (
                Icon::Nodes,
                "MOSIX",
                "The cluster is one computer. Processes keep a home node and a current node, and migrate without changing identity.",
                rgb(0x8b5cf6),
            ),
            (
                Icon::Gpu,
                "DisplayJet",
                "The display is an engine: a damage-tracked ARGB surface, anti-aliased text and shapes, a hardware cursor, and a narrow GPU boundary.",
                theme.accent,
            ),
            (
                Icon::Cpu,
                "Rust and assembly",
                "Every component is Rust or x86-64 assembly. There is no C, no libc and no 32-bit stage anywhere in the tree.",
                SUCCESS,
            ),
        ];
        for (icon, title, body, color) in ideas {
            let badge = Rect::new(inner.x, y, 32, 32);
            display.fill_rounded(badge, 9, argb(0x30, color & 0x00ff_ffff));
            display.stroke_rounded(badge, 9, 1, argb(0x38, color & 0x00ff_ffff));
            widgets::icon(display, badge.inset(7), icon, color);
            display.text(inner.x + 44, y - 1, title, 14, true, TEXT);
            let lines = widgets::wrap(crate::displayjet::font::face(12, false), body, inner.w - 44);
            for (index, line) in lines.iter().take(2).enumerate() {
                display.text(
                    inner.x + 44,
                    y + 19 + index as i32 * 16,
                    line,
                    12,
                    false,
                    TEXT_MUTED,
                );
            }
            y += 60;
        }
        let footer = Rect::new(inner.x, inner.bottom() - 54, inner.w, 44);
        if footer.y > y {
            display.fill_rect(Rect::new(inner.x, footer.y - 10, inner.w, 1), BORDER_SOFT);
            display.text(
                inner.x,
                footer.y,
                "GPL-3.0-or-later · github.com/gauthamnair2005/zirvium",
                10,
                false,
                TEXT_DIM,
            );
            display.text(
                inner.x,
                footer.y + 16,
                &format!(
                    "ABI {} · zirvstdlib {} · built with Rust for x86_64-unknown-uefi",
                    zirvstdlib::abi::ABI_VERSION,
                    zirvstdlib::VERSION
                ),
                10,
                false,
                TEXT_FAINT,
            );
        }
    }
}

// ---------------------------------------------------------------------------
// MOSIX nodes
// ---------------------------------------------------------------------------

pub struct Nodes {
    hits: Vec<Hit>,
}

impl Nodes {
    pub fn new() -> Self {
        Self { hits: Vec::new() }
    }

    pub fn draw(&mut self, display: &mut Display, content: Rect, theme: &Theme, _focused: bool) {
        self.hits.clear();
        let inner = content.inset(16);
        if inner.w < 300 || inner.h < 220 {
            return;
        }
        widgets::section_header(
            display,
            inner.x,
            inner.y,
            "MOSIX",
            "A cluster is one pool of resources. Processes migrate, not their identity.",
            theme,
        );
        let add = Rect::new(inner.right() - 132, inner.y + 4, 132, 30);
        let has_peer = crate::mosix::node_count() > 1;
        widgets::button(
            display,
            add,
            if has_peer {
                "Peer online"
            } else {
                "Add demo peer"
            },
            theme,
            widgets::ButtonStyle::Primary,
            false,
            !has_peer,
        );
        if !has_peer {
            self.hits.push(Hit {
                rect: add,
                action: Action::AddPeer,
            });
        }

        let mut y = inner.y + 62;
        let nodes = crate::mosix::nodes();
        let card_width = if nodes.len() > 1 {
            (inner.w - 10) / 2
        } else {
            inner.w
        };
        for (index, node) in nodes.iter().enumerate() {
            let rect = Rect::new(
                inner.x + index as i32 * (card_width + 10),
                y,
                card_width,
                112,
            );
            widgets::card(display, rect, theme);
            let color = match node.state {
                NodeState::Online => SUCCESS,
                NodeState::Busy => WARNING,
                NodeState::Booting => theme.accent,
                NodeState::Draining => rgb(0xf5a742),
                NodeState::Offline => TEXT_DIM,
            };
            display.fill_rounded(
                Rect::new(rect.x + 14, rect.y + 14, 30, 30),
                8,
                argb(0x28, color & 0x00ff_ffff),
            );
            widgets::icon(
                display,
                Rect::new(rect.x + 20, rect.y + 20, 18, 18),
                Icon::Nodes,
                color,
            );
            display.text(rect.x + 52, rect.y + 14, node.name_str(), 14, true, TEXT);
            widgets::pill(
                display,
                Point::new(rect.right() - 46, rect.y + 28),
                crate::mosix::state_name(node.state),
                color,
                argb(0x28, color & 0x00ff_ffff),
            );
            display.text(
                rect.x + 14,
                rect.y + 54,
                &format!(
                    "{} CPU · {} MiB · id {}",
                    node.cpu_count, node.memory_mib, node.id
                ),
                11,
                false,
                TEXT_MUTED,
            );
            display.text(rect.x + 14, rect.y + 72, "load", 10, false, TEXT_DIM);
            widgets::progress(
                display,
                Rect::new(rect.x + 46, rect.y + 78, rect.w - 60, 6),
                node.load_permille as f32 / 1000.0,
                color,
            );
            let load = format!("{}%", node.load_permille / 10);
            display.text(
                rect.right() - 14 - widgets::text_width(&load, 10, true),
                rect.y + 70,
                &load,
                10,
                true,
                TEXT,
            );
            display.text(
                rect.x + 14,
                rect.y + 92,
                if index == 0 {
                    "local node · scheduler"
                } else {
                    "peer · migration target"
                },
                10,
                false,
                TEXT_FAINT,
            );
        }
        y += 122;

        // Explanation card.
        let explanation = Rect::new(inner.x, y, inner.w, 56);
        widgets::card(display, explanation, theme);
        display.text(
            inner.x + 14,
            y + 10,
            "Migration changes current_node. home_node always records where the process was created.",
            11,
            false,
            TEXT_MUTED,
        );
        display.text(
            inner.x + 14,
            y + 30,
            &format!(
                "{} node(s) · {} process(es) · {} migration(s)",
                crate::mosix::node_count(),
                crate::mosix::process_count(),
                crate::mosix::migrations()
            ),
            10,
            false,
            TEXT_DIM,
        );
        y += 66;

        // Process migration table.
        let table = Rect::new(inner.x, y, inner.w, inner.bottom() - y);
        if table.h < 70 {
            return;
        }
        widgets::card(display, table, theme);
        display.text(inner.x + 14, y + 11, "Processes", 12, true, TEXT);
        let peer = nodes.iter().find(|node| node.id != 0).map(|node| node.id);
        let label = match peer {
            Some(id) => format!("migrate to node {id}"),
            None => String::from("add a peer to migrate"),
        };
        display.text(
            table.right() - 14 - widgets::text_width(&label, 10, false),
            y + 13,
            &label,
            10,
            false,
            TEXT_DIM,
        );
        let header_y = y + 34;
        display.text(inner.x + 14, header_y, "PID", 10, true, TEXT_DIM);
        display.text(inner.x + 56, header_y, "PROCESS", 10, true, TEXT_DIM);
        display.text(
            inner.x + 250,
            header_y,
            "HOME → CURRENT",
            10,
            true,
            TEXT_DIM,
        );
        display.fill_rect(
            Rect::new(inner.x + 14, header_y + 15, table.w - 28, 1),
            BORDER_SOFT,
        );
        let processes = crate::mosix::processes();
        let row_height = 28;
        let available = ((table.bottom() - (header_y + 22)) / row_height).max(0) as usize;
        for (index, process) in processes.iter().take(available.min(8)).enumerate() {
            let row_y = header_y + 20 + index as i32 * row_height;
            if index % 2 == 1 {
                display.fill_rect(
                    Rect::new(table.x + 10, row_y - 2, table.w - 20, row_height - 2),
                    argb(0x14, 0xffffff),
                );
            }
            display.text(
                inner.x + 14,
                row_y + 5,
                &format!("{}", process.pid),
                11,
                false,
                theme.accent,
            );
            display.text_ellipsized(
                inner.x + 56,
                row_y + 5,
                process.name_str(),
                180,
                11,
                false,
                TEXT,
            );
            display.text(
                inner.x + 250,
                row_y + 5,
                &format!("{} → {}", process.home_node, process.current_node),
                11,
                false,
                if process.current_node != process.home_node {
                    theme.accent_alt
                } else {
                    TEXT_MUTED
                },
            );
            let target = if process.current_node == 0 {
                peer
            } else {
                Some(0)
            };
            if let Some(target) = target {
                let button = Rect::new(table.right() - 96, row_y + 2, 82, 22);
                let label = if target == 0 { "Bring home" } else { "Migrate" };
                widgets::button(
                    display,
                    button,
                    label,
                    theme,
                    widgets::ButtonStyle::Subtle,
                    false,
                    true,
                );
                self.hits.push(Hit {
                    rect: button,
                    action: Action::Migrate(process.pid, target),
                });
            }
        }
    }

    pub fn pointer(&mut self, point: Point, _content: Rect, _theme: &mut Theme) -> Response {
        for hit in &self.hits {
            if !hit.rect.contains(point) {
                continue;
            }
            match hit.action {
                Action::AddPeer => {
                    if crate::mosix::node_count() < 2 {
                        let id = crate::mosix::register_node(
                            "zirvium-1-demo",
                            8,
                            8192,
                            NodeState::Online,
                        );
                        crate::mosix::update_node_load(id, 218);
                    }
                    return Response::REDRAW;
                }
                Action::Migrate(pid, target) => {
                    crate::mosix::migrate(pid, target);
                    return Response::REDRAW;
                }
                _ => {}
            }
        }
        Response::NONE
    }
}

/// Tick sample for apps that need it.
pub fn monitor_tick(monitor: &mut Monitor) -> bool {
    monitor.tick()
}

/// A small helper used by the monitor and terminal.
pub fn percentage(value: f32) -> i32 {
    math::round(value * 100.0) as i32
}
