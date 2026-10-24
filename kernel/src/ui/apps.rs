//! The Zirvium desktop applications.
//!
//! Each application owns its state and draws into a clipped content rectangle.
//! The desktop only routes input and decides when a repaint is needed.

use alloc::format;
use alloc::string::{String, ToString};
use alloc::vec::Vec;

use zirvstdlib::abi::{NodeState, key};
use zirvstdlib::geometry::{Point, Rect};

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
    display.text(rect.x + 48, rect.y + 17, label, 10, true, theme.text_muted());
    display.text(rect.x + 13, rect.y + 36, value, 23, true, theme.text());
    display.text_ellipsized(
        rect.x + 13,
        rect.y + 66,
        detail,
        rect.w - 26,
        10,
        false,
        theme.text_dim(),
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
                String::from("Current system load"),
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
        display.text(rect.x + 16, rect.y + 13, "CPU load", 13, true, theme.text());
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
            display.fill_rect(
                Rect::new(plot.x, y, plot.w, 1),
                if theme.is_light() { argb(0x22, 0x000000) } else { argb(0x24, 0xffffff) },
            );
        }
        for step in 1..4 {
            let x = plot.x + plot.w * step / 4;
            display.fill_rect(
                Rect::new(x, plot.y, 1, plot.h),
                if theme.is_light() { argb(0x14, 0x000000) } else { argb(0x14, 0xffffff) },
            );
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
        display.text(rect.x + 16, rect.y + 13, "Memory", 13, true, theme.text());
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
            display.text(rect.x + 16, y, label, 12, false, theme.text_muted());
            let value = format!("{:.0}%", ratio * 100.0);
            display.text(
                rect.right() - 16 - widgets::text_width(&value, 12, true),
                y,
                &value,
                12,
                true,
                theme.text(),
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
            theme.text_muted(),
        );
        display.text_ellipsized(
            rect.x + 16,
            y + 22,
            "Smooth system rendering",
            rect.w - 32,
            10,
            false,
            theme.text_dim(),
        );
    }

    fn draw_processes(&mut self, display: &mut Display, rect: Rect, theme: &Theme) {
        widgets::card(display, rect, theme);
        display.text(rect.x + 14, rect.y + 12, "Running processes", 12, true, theme.text());
        let count = crate::mosix::process_count();
        let label = format!("{count} total");
        display.text(
            rect.right() - 14 - widgets::text_width(&label, 11, false),
            rect.y + 13,
            &label,
            11,
            false,
            theme.text_dim(),
        );
        if rect.h < 70 {
            return;
        }
        let header_y = rect.y + 36;
        display.text(rect.x + 14, header_y, "PID", 10, true, theme.text_dim());
        display.text(rect.x + 56, header_y, "STATE", 10, true, theme.text_dim());
        display.text(rect.x + 132, header_y, "NAME", 10, true, theme.text_dim());
        display.text(rect.right() - 112, header_y, "NODE", 10, true, theme.text_dim());
        display.text(rect.right() - 62, header_y, "TICKS", 10, true, theme.text_dim());
        display.fill_rect(
            Rect::new(rect.x + 14, header_y + 16, rect.w - 28, 1),
            theme.line(),
        );
        let row_height = 24;
        let available = ((rect.bottom() - (header_y + 22)) / row_height).max(0) as usize;
        for (index, process) in crate::mosix::processes().iter().take(available).enumerate() {
            let y = header_y + 22 + index as i32 * row_height;
            if index % 2 == 1 {
                display.fill_rect(
                    Rect::new(rect.x + 10, y - 2, rect.w - 20, row_height - 2),
                    if theme.is_light() { argb(0x10, 0x000000) } else { argb(0x18, 0xffffff) },
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
                zirvstdlib::abi::ProcessState::Ready => theme.text(),
                zirvstdlib::abi::ProcessState::Sleeping => theme.text_dim(),
                zirvstdlib::abi::ProcessState::Blocked => WARNING,
                zirvstdlib::abi::ProcessState::Migrating => theme.accent_alt,
                _ => theme.text_dim(),
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
                theme.text(),
            );
            display.text(
                rect.right() - 112,
                y,
                &format!("{}", process.current_node),
                11,
                false,
                theme.text_muted(),
            );
            display.text(
                rect.right() - 62,
                y,
                &format!("{}", process.cpu_ticks),
                11,
                false,
                theme.text_muted(),
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
            theme.text_muted(),
        );
        display.text_ellipsized(
            header.x + 76,
            header.y + 7,
            &self.cwd,
            (header.w - 220).max(40),
            15,
            true,
            theme.text(),
        );
        display.text(
            header.right() - widgets::text_width(&self.status, 11, false),
            header.y + 11,
            &self.status,
            11,
            false,
            theme.text_dim(),
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
                theme.text_muted()
            };
            widgets::icon(display, icon_rect, icon, color);
            display.text_ellipsized(
                row.x + 42,
                row.y + 7,
                &entry.name,
                row.w - 116,
                14,
                entry.is_dir,
                theme.text(),
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
                theme.text_dim(),
            );
            if entry.is_dir {
                widgets::row_chevron(display, row, theme.text_dim());
            }
        }
        if self.entries.is_empty() {
            display.text(
                inner.x + 16,
                inner.y + 20,
                "This folder is empty.",
                12,
                false,
                theme.text_dim(),
            );
        }
        if self.entries.len() > visible {
            let track = Rect::new(list.right() - 7, list.y + 8, 3, list.h - 16);
            display.fill_rounded(track, 2, theme.line());
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
                theme.text_dim(),
            );
            display.text_centered(
                Rect::new(inner.x, inner.y + inner.h / 2 + 8, inner.w, 20),
                "Select a file to preview",
                12,
                false,
                theme.text_dim(),
            );
            return;
        };
        let Some(entry) = self.entries.get(index) else {
            return;
        };
        display.text_ellipsized(inner.x, inner.y, &entry.name, inner.w, 16, true, theme.text());
        let meta = if entry.is_dir {
            String::from("Directory")
        } else {
            format!("{} · text", widgets::byte_size(entry.size))
        };
        display.text(inner.x, inner.y + 24, &meta, 10, false, theme.text_dim());
        display.fill_rect(Rect::new(inner.x, inner.y + 42, inner.w, 1), theme.line());
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
                    if child.is_dir { theme.accent } else { theme.text_dim() },
                );
                display.text_ellipsized(
                    body.x + 20,
                    y,
                    &child.name,
                    body.w - 20,
                    12,
                    child.is_dir,
                    theme.text(),
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
                    display.text_face(body.x, y + face.ascent, &line, face, theme.text_muted());
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
    AddPeer,
    Migrate(u32, u32),
}

struct Hit {
    rect: Rect,
    action: Action,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum SettingsPage {
    Home,
    System,
    Personalization,
    Devices,
    Network,
    Update,
    About,
}

impl SettingsPage {
    fn title(self) -> &'static str {
        match self {
            SettingsPage::Home => "Settings",
            SettingsPage::System => "System",
            SettingsPage::Personalization => "Personalization",
            SettingsPage::Devices => "Devices",
            SettingsPage::Network => "Network & Internet",
            SettingsPage::Update => "Update & Security",
            SettingsPage::About => "About",
        }
    }

    fn subtitle(self) -> &'static str {
        match self {
            SettingsPage::Home => "ZirvUI \u{00b7} one focused workspace",
            SettingsPage::System => "Display, sound and power",
            SettingsPage::Personalization => "Appearance, surfaces and wallpaper",
            SettingsPage::Devices => "Hardware attached to this machine",
            SettingsPage::Network => "Connections and diagnostics",
            SettingsPage::Update => "Version, build and recovery",
            SettingsPage::About => "The design behind Zirvium",
        }
    }
}

#[derive(Clone, Copy)]
enum SettingAction {
    Page(SettingsPage),
    ModeDark,
    ModeLight,
    Accent(usize),
    Start(usize),
    Wallpaper(usize),
    Seconds,
    Animations,
    Grid,
    TextScale(u32),
    Toggle(usize),
    CheckUpdate,
    Reset,
}

struct SettingHit {
    rect: Rect,
    action: SettingAction,
}

pub struct Settings {
    hits: Vec<SettingHit>,
    page: SettingsPage,
    toggles: [bool; 4],
    update_checked: bool,
}

impl Settings {
    pub fn new() -> Self {
        Self {
            hits: Vec::new(),
            page: SettingsPage::Home,
            toggles: [true, false, true, true],
            update_checked: false,
        }
    }

    pub fn draw(&mut self, display: &mut Display, content: Rect, theme: &Theme) {
        self.hits.clear();
        if content.w < 430 || content.h < 260 {
            return;
        }
        display.fill_rect(content, theme.surface());

        // ---- Left navigation -------------------------------------------------
        let sidebar_w = 196.min(content.w / 3);
        let sidebar = Rect::new(content.x, content.y, sidebar_w, content.h);
        display.fill_rect(sidebar, theme.card());
        display.fill_rect(
            Rect::new(sidebar.right() - 1, sidebar.y, 1, sidebar.h),
            theme.line(),
        );
        widgets::logo(display, Rect::new(sidebar.x + 16, sidebar.y + 14, 22, 22));
        display.text(sidebar.x + 46, sidebar.y + 15, "Settings", 16, true, theme.text());
        display.text(sidebar.x + 46, sidebar.y + 35, "ZirvUI", 9, false, theme.text_dim());

        let nav: [(SettingsPage, &str, widgets::Icon); 7] = [
            (SettingsPage::Home, "Home", widgets::Icon::About),
            (SettingsPage::System, "System", widgets::Icon::Monitor),
            (SettingsPage::Personalization, "Personalization", widgets::Icon::Spark),
            (SettingsPage::Devices, "Devices", widgets::Icon::Cpu),
            (SettingsPage::Network, "Network & Internet", widgets::Icon::Nodes),
            (SettingsPage::Update, "Update & Security", widgets::Icon::Power),
            (SettingsPage::About, "About", widgets::Icon::Logo),
        ];
        let mut ny = sidebar.y + 62;
        for (page, label, icon) in nav {
            let row = Rect::new(sidebar.x + 6, ny, sidebar.w - 12, 32);
            if page == self.page {
                display.fill_rect(row, theme.accent_soft(38));
                display.fill_rect(Rect::new(row.x, row.y, 3, row.h), theme.accent);
            }
            widgets::icon(
                display,
                Rect::new(row.x + 12, row.y + 9, 14, 14),
                icon,
                if page == self.page { theme.accent } else { theme.text_dim() },
            );
            display.text(
                row.x + 36,
                row.y + 9,
                label,
                11,
                page == self.page,
                if page == self.page { theme.text() } else { theme.text_muted() },
            );
            self.hits.push(SettingHit {
                rect: row,
                action: SettingAction::Page(page),
            });
            ny += 34;
        }

        display.fill_rect(
            Rect::new(sidebar.x + 16, sidebar.bottom() - 40, sidebar.w - 32, 1),
            theme.line(),
        );
        display.text(
            sidebar.x + 16,
            sidebar.bottom() - 30,
            concat!("v", env!("CARGO_PKG_VERSION")),
            9,
            false,
            theme.text_dim(),
        );

        // ---- Page content ----------------------------------------------------
        let body = Rect::new(
            content.x + sidebar_w + 22,
            content.y + 18,
            content.w - sidebar_w - 44,
            content.h - 36,
        );
        if body.w < 200 {
            return;
        }
        display.text(body.x, body.y, self.page.title(), 24, true, theme.text());
        display.text(body.x, body.y + 32, self.page.subtitle(), 10, false, theme.text_muted());
        display.fill_rect(Rect::new(body.x, body.y + 50, body.w, 1), theme.line());
        let page_area = Rect::new(body.x, body.y + 64, body.w, body.h - 64);
        match self.page {
            SettingsPage::Home => self.draw_home(display, page_area, theme),
            SettingsPage::System => self.draw_system(display, page_area, theme),
            SettingsPage::Personalization => self.draw_personalization(display, page_area, theme),
            SettingsPage::Devices => self.draw_devices(display, page_area, theme),
            SettingsPage::Network => self.draw_network(display, page_area, theme),
            SettingsPage::Update => self.draw_update(display, page_area, theme),
            SettingsPage::About => self.draw_about(display, page_area, theme),
        }
    }

    // -- Shared widgets --------------------------------------------------------

    fn card(&self, display: &mut Display, rect: Rect, theme: &Theme) {
        widgets::card(display, rect, theme);
    }

    fn section(&self, display: &mut Display, x: i32, y: i32, text: &str, theme: &Theme) {
        display.text(x, y, text, 12, true, theme.text());
    }

    fn toggle(&mut self, display: &mut Display, rect: Rect, on: bool, theme: &Theme) -> bool {
        let w = 38;
        let track = Rect::new(rect.right() - w, rect.y + (rect.h - 18) / 2, w, 18);
        display.fill_rect(track, if on { theme.accent } else { theme.well() });
        display.stroke_rounded(track, 0, 1, if on { theme.accent } else { theme.line() });
        let knob = Rect::new(
            if on { track.right() - 16 } else { track.x + 2 },
            track.y + 2,
            14,
            14,
        );
        display.fill_rect(knob, if on { theme.on_color(theme.accent) } else { theme.text_dim() });
        on
    }

    fn toggle_row(
        &mut self,
        display: &mut Display,
        rect: Rect,
        index: usize,
        title: &str,
        detail: &str,
        theme: &Theme,
    ) {
        widgets::card(display, rect, theme);
        display.text(rect.x + 14, rect.y + 12, title, 11, true, theme.text());
        display.text(rect.x + 14, rect.y + 30, detail, 9, false, theme.text_dim());
        self.toggle(display, Rect::new(rect.right() - 60, rect.y, 50, rect.h), self.toggles[index], theme);
        self.hits.push(SettingHit {
            rect,
            action: SettingAction::Toggle(index),
        });
    }

    fn swatches(
        &mut self,
        display: &mut Display,
        x: i32,
        y: i32,
        colors: &[u32],
        selected: u32,
        action_for: fn(usize) -> SettingAction,
    ) {
        let mut sx = x;
        for (index, color) in colors.iter().enumerate() {
            let r = Rect::new(sx, y, 30, 30);
            display.fill_rect(r, rgb(*color));
            display.stroke_rounded(
                r,
                0,
                2,
                if *color == (selected & 0x00ff_ffff) {
                    rgb(0xffffff)
                } else {
                    argb(0x60, 0x000000)
                },
            );
            self.hits.push(SettingHit {
                rect: r,
                action: action_for(index),
            });
            sx += 40;
        }
    }

    // -- Pages -----------------------------------------------------------------

    fn draw_home(&mut self, display: &mut Display, area: Rect, theme: &Theme) {
        // A short introduction that folds in the old About app.
        let intro_h = 92;
        let intro = Rect::new(area.x, area.y, area.w, intro_h);
        self.card(display, intro, theme);
        let mark = Rect::new(intro.x + 14, intro.y + 14, 64, 64);
        widgets::logo(display, mark);
        display.text(intro.x + 92, intro.y + 14, "ZirvUI", 18, true, theme.text());
        display.text(
            intro.x + 92,
            intro.y + 40,
            "A focused operating environment with calm, responsive tools.",
            10,
            false,
            theme.text_muted(),
        );
        display.text(
            intro.x + 92,
            intro.y + 58,
            "A focused desktop with thoughtful, responsive tools.",
            9,
            false,
            theme.text_dim(),
        );
        let about_button = Rect::new(intro.right() - 92, intro.y + 34, 78, 26);
        display.fill_rect(about_button, theme.accent);
        display.text_centered(about_button, "About", 10, false, theme.on_color(theme.accent));
        self.hits.push(SettingHit {
            rect: about_button,
            action: SettingAction::Page(SettingsPage::About),
        });

        // Tiles.
        let tiles: [(SettingsPage, &str, &str); 5] = [
            (SettingsPage::System, "System", "Display and power"),
            (SettingsPage::Personalization, "Personalization", "Colors and Start"),
            (SettingsPage::Devices, "Devices", "Hardware"),
            (SettingsPage::Network, "Network", "Connections"),
            (SettingsPage::Update, "Update", "Version and build"),
        ];
        let top = intro.bottom() + 14;
        let gap = 10;
        let tile_w = (area.w - gap) / 2;
        let tile_h = ((area.bottom() - top - gap * 2) / 3).max(52);
        for (index, (page, title, detail)) in tiles.into_iter().enumerate() {
            let col = (index % 2) as i32;
            let row = (index / 2) as i32;
            let r = Rect::new(area.x + col * (tile_w + gap), top + row * (tile_h + gap), tile_w, tile_h);
            self.card(display, r, theme);
            display.fill_rect(Rect::new(r.x, r.y, 3, r.h), theme.accent_soft(160));
            display.text(r.x + 16, r.y + 12, title, 13, true, theme.text());
            display.text(r.x + 16, r.y + 34, detail, 9, false, theme.text_dim());
            self.hits.push(SettingHit {
                rect: r,
                action: SettingAction::Page(page),
            });
        }
    }

    fn draw_system(&mut self, display: &mut Display, area: Rect, theme: &Theme) {
        let row_h = 54;
        let mut y = area.y;

        self.section(display, area.x, y, "Display", theme);
        y += 22;
        let resolution = Rect::new(area.x, y, area.w, 64);
        self.card(display, resolution, theme);
        let resolution_label = format!(
            "{} × {}",
            crate::ui::WIDTH_HINT.load(core::sync::atomic::Ordering::Relaxed),
            crate::ui::HEIGHT_HINT.load(core::sync::atomic::Ordering::Relaxed)
        );
        display.text(resolution.x + 14, resolution.y + 12, "Resolution", 11, true, theme.text());
        display.text(resolution.x + 14, resolution.y + 32, &resolution_label, 10, false, theme.text_muted());
        display.text(
            resolution.right() - 190,
            resolution.y + 22,
            "Firmware-selected native mode",
            9,
            false,
            theme.text_dim(),
        );
        y = resolution.bottom() + 14;

        self.section(display, area.x, y, "Scale", theme);
        y += 22;
        let scale_card = Rect::new(area.x, y, area.w, 64);
        self.card(display, scale_card, theme);
        display.text(scale_card.x + 14, scale_card.y + 12, "Interface scale", 11, true, theme.text());
        display.text(scale_card.x + 14, scale_card.y + 32, "Applies immediately to all UI text", 9, false, theme.text_dim());
        for (index, value) in [100u32, 115, 125, 140].iter().enumerate() {
            let button = Rect::new(scale_card.right() - 196 + index as i32 * 46, scale_card.y + 18, 40, 28);
            let selected = theme.text_scale == *value;
            display.fill_rounded(button, 8, if selected { theme.accent } else { theme.well() });
            display.stroke_rounded(button, 8, 1, if selected { theme.accent } else { theme.line() });
            display.text_centered(button, &format!("{}%", value), 8, selected, if selected { theme.on_color(theme.accent) } else { theme.text_muted() });
            self.hits.push(SettingHit { rect: button, action: SettingAction::TextScale(*value) });
        }
        y = scale_card.bottom() + 14;

        let wallpaper = Rect::new(area.x, y, area.w, 78);
        self.card(display, wallpaper, theme);
        display.text(wallpaper.x + 14, wallpaper.y + 14, "Wallpaper", 11, true, theme.text());
        display.text(wallpaper.x + 14, wallpaper.y + 34, "Choose a desktop backdrop", 9, false, theme.text_dim());
        let names = ["Nebula", "Aurora", "Graphite", "Ember"];
        let mut bx = wallpaper.x + 14;
        for (index, name) in names.iter().enumerate() {
            let b = Rect::new(bx, wallpaper.y + 50, 66, 20);
            display.fill_rect(b, if theme.wallpaper as usize == index { theme.accent } else { theme.well() });
            display.stroke_rounded(b, 0, 1, theme.line());
            display.text_centered(
                b,
                name,
                8,
                false,
                if theme.wallpaper as usize == index { theme.on_color(theme.accent) } else { theme.text_muted() },
            );
            self.hits.push(SettingHit {
                rect: b,
                action: SettingAction::Wallpaper(index),
            });
            bx += 72;
        }
        y = wallpaper.bottom() + 14;

        self.section(display, area.x, y, "Workspace & effects", theme);
        y += 22;
        let labels = [
            ("Show seconds in the clock", "Precise time in the system header"),
            ("Animate workspace changes", "Smooth transitions"),
            ("Show the desktop grid", "Faint alignment grid"),
        ];
        let actions = [SettingAction::Seconds, SettingAction::Animations, SettingAction::Grid];
        for (index, (title, detail)) in labels.iter().enumerate() {
            let r = Rect::new(area.x, y, area.w, row_h);
            display.fill_rect(r, theme.card());
            display.stroke_rounded(r, 0, 1, theme.line());
            display.text(r.x + 14, r.y + 12, title, 11, true, theme.text());
            display.text(r.x + 14, r.y + 30, detail, 9, false, theme.text_dim());
            let state = match actions[index] {
                SettingAction::Seconds => theme.show_seconds,
                SettingAction::Animations => theme.animations,
                SettingAction::Grid => theme.show_grid,
                _ => false,
            };
            self.toggle(display, Rect::new(r.right() - 60, r.y, 50, r.h), state, theme);
            self.hits.push(SettingHit { rect: r, action: actions[index] });
            y += row_h + 8;
        }

        if y + 48 <= area.bottom() {
            let reset = Rect::new(area.x, y + 4, 140, 30);
            display.fill_rect(reset, theme.well());
            display.stroke_rounded(reset, 0, 1, theme.line());
            display.text_centered(reset, "Reset all settings", 10, false, theme.text_muted());
            self.hits.push(SettingHit { rect: reset, action: SettingAction::Reset });
        }
    }

    fn draw_personalization(&mut self, display: &mut Display, area: Rect, theme: &Theme) {
        let mut y = area.y;
        self.section(display, area.x, y, "Color mode", theme);
        y += 22;
        let modes = Rect::new(area.x, y, area.w, 62);
        self.card(display, modes, theme);
        display.text(modes.x + 14, modes.y + 12, "Choose your mode", 11, true, theme.text());
        display.text(modes.x + 14, modes.y + 30, "Light is easier in bright rooms", 9, false, theme.text_dim());
        let light = Rect::new(modes.right() - 176, modes.y + 18, 76, 26);
        let dark = Rect::new(modes.right() - 92, modes.y + 18, 76, 26);
        for (rect, label, selected) in [
            (light, "Light", theme.is_light()),
            (dark, "Dark", !theme.is_light()),
        ] {
            display.fill_rect(rect, if selected { theme.accent } else { theme.well() });
            display.stroke_rounded(rect, 0, 1, if selected { theme.accent } else { theme.line() });
            display.text_centered(rect, label, 10, selected, if selected { theme.on_color(theme.accent) } else { theme.text_muted() });
        }
        self.hits.push(SettingHit { rect: light, action: SettingAction::ModeLight });
        self.hits.push(SettingHit { rect: dark, action: SettingAction::ModeDark });
        y = modes.bottom() + 14;

        self.section(display, area.x, y, "Accent color", theme);
        y += 22;
        let accent_card = Rect::new(area.x, y, area.w, 62);
        self.card(display, accent_card, theme);
        display.text(accent_card.x + 14, accent_card.y + 12, "Used for focus, selection and highlights", 9, false, theme.text_dim());
        self.swatches(
            display,
            accent_card.x + 14,
            accent_card.y + 26,
            &[0x00aaff, 0x9b7bff, 0x35e0a1, 0xfb7185, 0xffc24d, 0xff8c00],
            theme.accent,
            SettingAction::Accent,
        );
        y = accent_card.bottom() + 14;

        self.section(display, area.x, y, "Navigator surface", theme);
        y += 22;
        let start_card = Rect::new(area.x, y, area.w, 62);
        self.card(display, start_card, theme);
        display.text(start_card.x + 14, start_card.y + 12, "Navigator highlight color", 9, false, theme.text_dim());
        self.swatches(
            display,
            start_card.x + 14,
            start_card.y + 26,
            &[0x0078d7, 0x464eb8, 0x881798, 0xc30052, 0x107c10, 0x005a9e],
            theme.start_color,
            SettingAction::Start,
        );
    }

    fn draw_devices(&mut self, display: &mut Display, area: Rect, theme: &Theme) {
        let rows = [
            ("Processor", "x86-64 CPU \u{00b7} UEFI boot", "Ready"),
            ("Graphics", "Hardware scanout \u{00b7} smooth compositing", "Active"),
            ("Memory", "Unified system memory", "Managed"),
            ("Storage", "In-memory virtual filesystem", "Mounted"),
            ("Input", "PS/2 keyboard and pointer", "Connected"),
        ];
        let mut y = area.y;
        self.section(display, area.x, y, "This PC", theme);
        y += 22;
        for (name, detail, state) in rows {
            let r = Rect::new(area.x, y, area.w, 50);
            self.card(display, r, theme);
            widgets::icon(
                display,
                Rect::new(r.x + 14, r.y + 15, 18, 18),
                widgets::Icon::Cpu,
                theme.accent,
            );
            display.text(r.x + 44, r.y + 12, name, 11, true, theme.text());
            display.text(r.x + 44, r.y + 30, detail, 9, false, theme.text_dim());
            let pill_rect = Rect::new(r.right() - 92, r.y + 16, 78, 18);
            display.fill_rect(pill_rect, theme.accent_soft(36));
            display.text_centered(pill_rect, state, 8, true, theme.accent);
            y += 58;
        }
    }

    fn draw_network(&mut self, display: &mut Display, area: Rect, theme: &Theme) {
        let mut y = area.y;
        self.section(display, area.x, y, "System services", theme);
        y += 22;
        let processes = crate::mosix::process_count();
        let summary = Rect::new(area.x, y, area.w, 74);
        self.card(display, summary, theme);
        display.text(summary.x + 14, summary.y + 14, "Local services", 12, true, theme.text());
        display.text(
            summary.x + 14,
            summary.y + 34,
            &alloc::format!(
                "{} service(s) \u{00b7} {} active task(s)",
                1, processes
            ),
            9,
            false,
            theme.text_muted(),
        );
        display.text(
            summary.x + 14,
            summary.y + 52,
            "Core services share system resources and remain available while apps run.",
            9,
            false,
            theme.text_dim(),
        );
        y = summary.bottom() + 14;

        self.section(display, area.x, y, "Interfaces", theme);
        y += 22;
        let rows = [
            ("System services", "Core background services", "Online"),
            ("Local connection", "Internal application services", "Ready"),
            ("Loopback", "127.0.0.1 \u{00b7} internal services", "Up"),
            ("Session", "User applications and desktop", "Active"),
        ];
        for (name, detail, state) in rows {
            let r = Rect::new(area.x, y, area.w, 48);
            self.card(display, r, theme);
            display.fill_rect(Rect::new(r.x + 14, r.y + 18, 8, 8), if state == "Up" || state == "Online" || state == "Linked" { rgb(0x4ade9c) } else { theme.text_dim() });
            display.text(r.x + 34, r.y + 10, name, 11, true, theme.text());
            display.text(r.x + 34, r.y + 28, detail, 9, false, theme.text_dim());
            display.text(r.right() - 14 - widgets::text_width(state, 9, false), r.y + 17, state, 9, false, theme.text_muted());
            y += 54;
        }
    }


    fn draw_update(&mut self, display: &mut Display, area: Rect, theme: &Theme) {
        let mut y = area.y;
        self.section(display, area.x, y, "Build", theme);
        y += 22;
        let build = Rect::new(area.x, y, area.w, 78);
        self.card(display, build, theme);
        display.text(build.x + 14, build.y + 14, "ZirvUI", 13, true, theme.text());
        display.text(
            build.x + 14,
            build.y + 36,
            &alloc::format!(
                "ABI {} \u{00b7} zirvstdlib {} \u{00b7} x86_64-unknown-uefi",
                zirvstdlib::abi::ABI_VERSION,
                zirvstdlib::VERSION
            ),
            9,
            false,
            theme.text_muted(),
        );
        display.text(
            build.x + 14,
            build.y + 54,
            "Rust kernel \u{00b7} GPL-3.0-or-later",
            9,
            false,
            theme.text_dim(),
        );
        y = build.bottom() + 14;

        let status = if self.update_checked {
            "You are running the latest build available on this machine."
        } else {
            "Check for a newer build of Zirvium."
        };
        let update = Rect::new(area.x, y, area.w, 76);
        self.card(display, update, theme);
        display.text(update.x + 14, update.y + 14, "Updates", 12, true, theme.text());
        display.text_ellipsized(update.x + 14, update.y + 34, status, update.w - 150, 9, false, theme.text_muted());
        let button = Rect::new(update.right() - 128, update.y + 24, 114, 28);
        display.fill_rect(button, if self.update_checked { theme.well() } else { theme.accent });
        display.stroke_rounded(button, 0, 1, theme.line());
        display.text_centered(
            button,
            if self.update_checked { "Checked" } else { "Check now" },
            9,
            false,
            if self.update_checked { theme.text_muted() } else { theme.on_color(theme.accent) },
        );
        self.hits.push(SettingHit { rect: button, action: SettingAction::CheckUpdate });
        y = update.bottom() + 14;

        self.section(display, area.x, y, "Recovery", theme);
        y += 22;
        let recovery = Rect::new(area.x, y, area.w, 58);
        self.card(display, recovery, theme);
        display.text(recovery.x + 14, recovery.y + 12, "Reset and restart", 11, true, theme.text());
        display.text(recovery.x + 14, recovery.y + 30, "Restore the default theme and desktop state.", 9, false, theme.text_dim());
        let reset = Rect::new(recovery.right() - 92, recovery.y + 15, 78, 26);
        display.fill_rect(reset, theme.well());
        display.stroke_rounded(reset, 0, 1, theme.line());
        display.text_centered(reset, "Reset", 9, false, theme.text_muted());
        self.hits.push(SettingHit { rect: reset, action: SettingAction::Reset });
    }

    fn draw_about(&mut self, display: &mut Display, area: Rect, theme: &Theme) {
        let mark = Rect::new(area.x, area.y, 72, 72);
        widgets::logo(display, mark);
        display.text(area.x + 90, area.y + 4, "ZirvUI", 24, true, theme.text());
        display.text(area.x + 90, area.y + 34, "A focused desktop experience", 11, false, theme.accent);
        display.text(
            area.x + 90,
            area.y + 52,
            "A 64-bit operating system written from scratch.",
            9,
            false,
            theme.text_muted(),
        );

        let mut y = area.y + 90;
        let ideas = [
            ("Designed for clarity", "A calm interface with readable typography, soft surfaces and focused workflows."),
            ("Built for reliability", "Core services are isolated and the desktop keeps working when optional features are unavailable."),
            ("Ready to grow", "The system is designed to gain more apps, hardware support and personalization over time."),
        ];
        for (title, body) in ideas {
            display.text(area.x, y, title, 12, true, theme.text());
            let lines = widgets::wrap(crate::displayjet::font::face(9, false), body, area.w);
            for (index, line) in lines.iter().take(2).enumerate() {
                display.text(area.x, y + 20 + index as i32 * 13, line, 9, false, theme.text_muted());
            }
            y += 54;
        }

        if area.bottom() - y > 40 {
            display.fill_rect(Rect::new(area.x, area.bottom() - 32, area.w, 1), theme.line());
            display.text(area.x, area.bottom() - 24, "GPL-3.0-or-later \u{00b7} github.com/gauthamnair2005/zirvium", 8, false, theme.text_dim());
        }
    }

    pub fn pointer(&mut self, point: Point, _content: Rect, theme: &mut Theme) -> Response {
        for hit in &self.hits {
            if !hit.rect.contains(point) {
                continue;
            }
            match hit.action {
                SettingAction::Page(p) => {
                    self.page = p;
                    return Response::REDRAW;
                }
                SettingAction::ModeDark => {
                    theme.mode = crate::ui::theme::ColorMode::Dark;
                    return Response::THEME;
                }
                SettingAction::ModeLight => {
                    theme.mode = crate::ui::theme::ColorMode::Light;
                    return Response::THEME;
                }
                SettingAction::Accent(i) => {
                    let colors = [0x00aaff, 0x9b7bff, 0x35e0a1, 0xfb7185, 0xffc24d, 0xff8c00];
                    if let Some(color) = colors.get(i) {
                        theme.accent = rgb(*color);
                        theme.accent_alt = rgb(*color);
                    }
                    return Response::THEME;
                }
                SettingAction::Start(i) => {
                    let colors = [0x0078d7, 0x464eb8, 0x881798, 0xc30052, 0x107c10, 0x005a9e];
                    if let Some(color) = colors.get(i) {
                        theme.start_color = rgb(*color);
                    }
                    return Response::THEME;
                }
                SettingAction::Wallpaper(i) => {
                    theme.wallpaper = i as u8;
                    return Response::THEME;
                }
                SettingAction::Seconds => {
                    theme.show_seconds = !theme.show_seconds;
                    return Response::THEME;
                }
                SettingAction::Animations => {
                    theme.animations = !theme.animations;
                    return Response::THEME;
                }
                SettingAction::Grid => {
                    theme.show_grid = !theme.show_grid;
                    return Response::THEME;
                }
                SettingAction::TextScale(value) => {
                    theme.text_scale = value;
                    crate::displayjet::font::set_scale(value);
                    return Response::THEME;
                }
                SettingAction::Toggle(index) => {
                    if index < self.toggles.len() {
                        self.toggles[index] = !self.toggles[index];
                    }
                    return Response::REDRAW;
                }
                SettingAction::CheckUpdate => {
                    self.update_checked = true;
                    return Response::REDRAW;
                }
                SettingAction::Reset => {
                    *theme = crate::ui::theme::Theme::default();
                    self.toggles = [true, false, true, true];
                    self.update_checked = false;
                    return Response::THEME;
                }
            }
        }
        Response::NONE
    }

    pub fn key(&mut self, _code: u32, _mods: u16) -> bool {
        false
    }

    pub fn scroll(&mut self, _point: Point, _content: Rect, amount: i32) {
        let _ = amount;
    }

    pub fn tick(&mut self) -> bool {
        false
    }
}

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
                NodeState::Offline => theme.text_dim(),
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
            display.text(rect.x + 52, rect.y + 14, node.name_str(), 14, true, theme.text());
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
                theme.text_muted(),
            );
            display.text(rect.x + 14, rect.y + 72, "load", 10, false, theme.text_dim());
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
                theme.text(),
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
                theme.text_dim(),
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
            theme.text_muted(),
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
            theme.text_dim(),
        );
        y += 66;

        // Process migration table.
        let table = Rect::new(inner.x, y, inner.w, inner.bottom() - y);
        if table.h < 70 {
            return;
        }
        widgets::card(display, table, theme);
        display.text(inner.x + 14, y + 11, "Processes", 12, true, theme.text());
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
            theme.text_dim(),
        );
        let header_y = y + 34;
        display.text(inner.x + 14, header_y, "PID", 10, true, theme.text_dim());
        display.text(inner.x + 56, header_y, "PROCESS", 10, true, theme.text_dim());
        display.text(
            inner.x + 250,
            header_y,
            "HOME → CURRENT",
            10,
            true,
            theme.text_dim(),
        );
        display.fill_rect(
            Rect::new(inner.x + 14, header_y + 15, table.w - 28, 1),
            theme.line(),
        );
        let processes = crate::mosix::processes();
        let row_height = 28;
        let available = ((table.bottom() - (header_y + 22)) / row_height).max(0) as usize;
        for (index, process) in processes.iter().take(available.min(8)).enumerate() {
            let row_y = header_y + 20 + index as i32 * row_height;
            if index % 2 == 1 {
                display.fill_rect(
                    Rect::new(table.x + 10, row_y - 2, table.w - 20, row_height - 2),
                    if theme.is_light() { argb(0x10, 0x000000) } else { argb(0x14, 0xffffff) },
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
                theme.text(),
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
                    theme.text_muted()
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
            }
        }
        Response::NONE
    }
}



// ---------------------------------------------------------------------------
// Calculator
// ---------------------------------------------------------------------------

#[derive(Clone, Copy)]
enum CalcKey {
    Digit(char),
    Dot,
    Op(char),
    Equals,
    Clear,
    Back,
    Percent,
    Negate,
}

pub struct Calculator {
    entry: String,
    accumulator: f64,
    op: Option<char>,
    fresh: bool,
    hits: Vec<(Rect, CalcKey)>,
}

impl Calculator {
    pub fn new() -> Self {
        Self {
            entry: String::from("0"),
            accumulator: 0.0,
            op: None,
            fresh: true,
            hits: Vec::new(),
        }
    }

    fn value(&self) -> f64 {
        self.entry.parse::<f64>().unwrap_or(0.0)
    }

    fn show(&self) -> &str {
        if self.entry.is_empty() { "0" } else { &self.entry }
    }

    fn push_digit(&mut self, digit: char) {
        if self.fresh {
            self.entry.clear();
            self.fresh = false;
        }
        if self.entry.len() < 16 {
            if digit == '0' && self.entry == "0" {
                return;
            }
            if self.entry == "0" {
                self.entry.clear();
            }
            self.entry.push(digit);
        }
    }

    fn push_dot(&mut self) {
        if self.fresh {
            self.entry = String::from("0");
            self.fresh = false;
        }
        if !self.entry.contains('.') {
            if self.entry.is_empty() {
                self.entry.push('0');
            }
            self.entry.push('.');
        }
    }

    fn apply(&mut self, op: char, rhs: f64) {
        let lhs = self.accumulator;
        let result = match op {
            '+' => lhs + rhs,
            '-' => lhs - rhs,
            '*' => lhs * rhs,
            '/' => {
                if rhs == 0.0 {
                    self.entry = String::from("Error");
                    self.accumulator = 0.0;
                    self.fresh = true;
                    self.op = None;
                    return;
                }
                lhs / rhs
            }
            _ => rhs,
        };
        self.accumulator = result;
        self.entry = alloc::format!("{}", result);
        self.fresh = true;
    }

    fn press(&mut self, key: CalcKey) {
        match key {
            CalcKey::Digit(digit) => self.push_digit(digit),
            CalcKey::Dot => self.push_dot(),
            CalcKey::Op(op) => {
                let value = self.value();
                if let Some(previous) = self.op {
                    self.apply(previous, value);
                } else {
                    self.accumulator = value;
                    self.fresh = true;
                }
                self.op = Some(op);
            }
            CalcKey::Equals => {
                if let Some(op) = self.op.take() {
                    let value = self.value();
                    self.apply(op, value);
                }
            }
            CalcKey::Clear => {
                self.entry = String::from("0");
                self.accumulator = 0.0;
                self.op = None;
                self.fresh = true;
            }
            CalcKey::Back => {
                if self.fresh {
                    self.entry = String::from("0");
                    self.fresh = true;
                } else {
                    self.entry.pop();
                    if self.entry.is_empty() {
                        self.entry.push('0');
                        self.fresh = true;
                    }
                }
            }
            CalcKey::Percent => {
                let value = self.value() / 100.0;
                self.entry = alloc::format!("{}", value);
                self.fresh = true;
            }
            CalcKey::Negate => {
                if let Some(rest) = self.entry.strip_prefix('-') {
                    self.entry = rest.to_string();
                } else if self.entry != "0" {
                    self.entry.insert(0, '-');
                }
            }
        }
    }

    pub fn draw(&mut self, display: &mut Display, content: Rect, theme: &Theme, _focused: bool) {
        self.hits.clear();
        let inner = content.inset(12);
        display.fill_rect(inner, theme.surface());
        let pad = Rect::new(inner.x, inner.y, inner.w, 64);
        display.fill_rect(pad, theme.card());
        display.stroke_rounded(pad, 0, 1, theme.line());
        display.text(pad.x + 12, pad.y + 10, "STANDARD", 8, true, theme.text_dim());
        let shown = self.show();
        display.text(
            pad.right() - 12 - widgets::text_width(shown, 26, false),
            pad.y + 22,
            shown,
            26,
            false,
            theme.text(),
        );

        let rows: [[(&str, CalcKey); 4]; 5] = [
            [("C", CalcKey::Clear), ("±", CalcKey::Negate), ("%", CalcKey::Percent), ("÷", CalcKey::Op('/'))],
            [("7", CalcKey::Digit('7')), ("8", CalcKey::Digit('8')), ("9", CalcKey::Digit('9')), ("×", CalcKey::Op('*'))],
            [("4", CalcKey::Digit('4')), ("5", CalcKey::Digit('5')), ("6", CalcKey::Digit('6')), ("−", CalcKey::Op('-'))],
            [("1", CalcKey::Digit('1')), ("2", CalcKey::Digit('2')), ("3", CalcKey::Digit('3')), ("+", CalcKey::Op('+'))],
            [("0", CalcKey::Digit('0')), (".", CalcKey::Dot), ("=", CalcKey::Equals), ("⌫", CalcKey::Back)],
        ];
        let top = pad.bottom() + 8;
        let gap = 6;
        let cell_w = (inner.w - gap * 3) / 4;
        let cell_h = ((inner.bottom() - top - gap * 4) / 5).max(28);
        for (row, entries) in rows.iter().enumerate() {
            for (col, (label, action)) in entries.iter().enumerate() {
                let r = Rect::new(
                    inner.x + col as i32 * (cell_w + gap),
                    top + row as i32 * (cell_h + gap),
                    cell_w,
                    cell_h,
                );
                let accent = matches!(action, CalcKey::Equals | CalcKey::Op(_));
                display.fill_rect(r, if accent { theme.accent_soft(50) } else { theme.card() });
                display.stroke_rounded(r, 0, 1, theme.line());
                display.text_centered(
                    r,
                    label,
                    13,
                    accent,
                    if accent { theme.accent } else { theme.text() },
                );
                self.hits.push((r, *action));
            }
        }
    }

    pub fn pointer(&mut self, point: Point, _content: Rect, _theme: &mut Theme) -> Response {
        for (rect, action) in &self.hits {
            if rect.contains(point) {
                self.press(*action);
                return Response::REDRAW;
            }
        }
        Response::NONE
    }

    pub fn key(&mut self, code: u32, _mods: u16) -> bool {
        match code {
            key::ENTER => self.press(CalcKey::Equals),
            key::BACKSPACE => self.press(CalcKey::Back),
            key::ESCAPE => self.press(CalcKey::Clear),
            c if c == b'=' as u32 => self.press(CalcKey::Equals),
            c if c == b'+' as u32 => self.press(CalcKey::Op('+')),
            c if c == b'-' as u32 => self.press(CalcKey::Op('-')),
            c if c == b'*' as u32 || c == b'x' as u32 => self.press(CalcKey::Op('*')),
            c if c == b'/' as u32 => self.press(CalcKey::Op('/')),
            c if c == b'%' as u32 => self.press(CalcKey::Percent),
            c if c == b'.' as u32 || c == b',' as u32 => self.press(CalcKey::Dot),
            _ => {
                if let Some(character) = char::from_u32(code) {
                    if character.is_ascii_digit() {
                        self.press(CalcKey::Digit(character));
                        return true;
                    }
                }
                return false;
            }
        }
        true
    }

    pub fn scroll(&mut self, _point: Point, _content: Rect, _amount: i32) {}
    pub fn tick(&mut self) -> bool {
        false
    }
}

// ---------------------------------------------------------------------------
// Notes
// ---------------------------------------------------------------------------

pub struct Notes {
    text: String,
    scroll: i32,
}

impl Notes {
    pub fn new() -> Self {
        Self {
            text: String::from("Zirvium Notes\n\nCapture ideas here. Typing is saved for this session.\n"),
            scroll: 0,
        }
    }

    pub fn draw(&mut self, display: &mut Display, content: Rect, theme: &Theme, _focused: bool) {
        let inner = content.inset(12);
        display.fill_rect(inner, theme.surface());
        let header = Rect::new(inner.x, inner.y, inner.w, 30);
        display.fill_rect(header, theme.card());
        display.text(header.x + 12, header.y + 9, "Notes", 12, true, theme.text());
        display.text(header.right() - 120, header.y + 10, "session scratchpad", 9, false, theme.text_dim());

        let area = Rect::new(inner.x, header.bottom() + 8, inner.w, inner.bottom() - header.bottom() - 8);
        display.fill_rect(area, theme.card());
        display.stroke_rounded(area, 0, 1, theme.line());
        display.push_clip(area);
        let face = crate::displayjet::font::face(11, false);
        let mut y = area.y + 10 - self.scroll;
        let width = area.w - 24;
        for paragraph in self.text.split('\n') {
            if paragraph.is_empty() {
                y += face.line_height + 2;
                continue;
            }
            for line in widgets::wrap(face, paragraph, width) {
                display.text(area.x + 12, y, &line, 11, false, theme.text());
                y += face.line_height + 2;
            }
        }
        display.fill_rect(Rect::new(area.x + 12, y, 1, 13), theme.accent);
        display.pop_clip();
    }

    pub fn pointer(&mut self, _point: Point, _content: Rect, _theme: &mut Theme) -> Response {
        Response::NONE
    }

    pub fn key(&mut self, code: u32, _mods: u16) -> bool {
        match code {
            key::BACKSPACE => {
                self.text.pop();
                true
            }
            key::ENTER => {
                self.text.push('\n');
                true
            }
            key::TAB => {
                self.text.push_str("    ");
                true
            }
            key::PAGE_UP => {
                self.scroll = (self.scroll - 24).max(0);
                true
            }
            key::PAGE_DOWN => {
                self.scroll += 24;
                true
            }
            key::ESCAPE => false,
            _ => {
                if code >= 0x20
                    && code != 0x7f
                    && let Some(character) = char::from_u32(code)
                {
                    if self.text.len() < 64 * 1024 {
                        self.text.push(character);
                    }
                    return true;
                }
                false
            }
        }
    }

    pub fn scroll(&mut self, _point: Point, _content: Rect, amount: i32) {
        self.scroll = (self.scroll - amount * 24).max(0);
    }

    pub fn tick(&mut self) -> bool {
        false
    }

    pub fn clipboard_copy(&self) -> String {
        self.text.clone()
    }

    pub fn clipboard_paste(&mut self, text: &str) -> bool {
        if text.is_empty() {
            return false;
        }
        self.text.push_str(text);
        true
    }

    pub fn select_all(&mut self) -> bool {
        self.scroll = 0;
        true
    }
}

// ---------------------------------------------------------------------------
// Network
// ---------------------------------------------------------------------------

pub struct Network {
}

impl Network {
    pub fn new() -> Self {
        Self {}
    }

    pub fn draw(&mut self, display: &mut Display, content: Rect, theme: &Theme, _focused: bool) {
        let inner = content.inset(12);
        display.fill_rect(inner, theme.surface());
        display.text(inner.x, inner.y, "Network & Internet", 18, true, theme.text());
        display.text(inner.x, inner.y + 26, "Connection status and local diagnostics", 10, false, theme.text_muted());

        let summary = Rect::new(inner.x, inner.y + 52, inner.w, 60);
        display.fill_rect(summary, theme.card());
        display.stroke_rounded(summary, 0, 1, theme.line());
        display.text(summary.x + 12, summary.y + 12, "Network status", 11, true, theme.text());
        display.text(
            summary.x + 12,
            summary.y + 32,
            "No external interfaces configured",
            9,
            false,
            theme.text_muted(),
        );

        let y = summary.bottom() + 12;
        let r = Rect::new(inner.x, y, inner.w, 46);
        display.fill_rect(r, theme.card());
        display.stroke_rounded(r, 0, 1, theme.line());
        display.fill_rect(Rect::new(r.x + 12, r.y + 18, 8, 8), rgb(0xf7c95c));
        display.text(r.x + 32, r.y + 10, "External network", 11, true, theme.text());
        display.text(r.x + 32, r.y + 27, "Network support is not available in this build", 8, false, theme.text_dim());
    }

    pub fn pointer(&mut self, point: Point, _content: Rect, _theme: &mut Theme) -> Response {
        let _ = point;
        Response::NONE
    }

    pub fn key(&mut self, _code: u32, _mods: u16) -> bool {
        false
    }

    pub fn scroll(&mut self, _point: Point, _content: Rect, _amount: i32) {}

    pub fn tick(&mut self) -> bool {
        false
    }
}
