//! The DisplayJet terminal.
//!
//! There is no fork/exec yet, so commands are implemented in Rust against the
//! kernel APIs directly.  The command set is deliberately useful: it exposes
//! MOSIX, the VFS, the display engine and the boot log, which makes the
//! terminal a real diagnostic tool rather than a mock-up.

use alloc::format;
use alloc::string::{String, ToString};
use alloc::vec::Vec;

use zirvstdlib::abi::{key, modifiers};
use zirvstdlib::geometry::{Point, Rect};

use crate::displayjet::Display;
use crate::ui::theme::*;
use crate::ui::widgets;

const MAX_LINES: usize = 400;

pub struct Terminal {
    lines: Vec<String>,
    input: String,
    cursor: usize,
    cwd: String,
    scroll: usize,
    history: Vec<String>,
    history_position: Option<usize>,
}

impl Terminal {
    pub fn new() -> Self {
        let uptime = crate::time::uptime_string();
        let mut terminal = Self {
            lines: Vec::new(),
            input: String::new(),
            cursor: 0,
            cwd: String::from("/home/zirvium"),
            scroll: 0,
            history: Vec::new(),
            history_position: None,
        };
        terminal.push(format!("Zirvium 2.0 — DisplayJet terminal"));
        terminal.push(format!(
            "Kernel booted in {} on {} logical processor(s).",
            uptime,
            crate::arch::cpu::logical_processors()
        ));
        terminal.push(String::from(
            "Type 'help' for commands. Tab completion is not implemented yet.",
        ));
        terminal.push(String::new());
        terminal
    }

    fn push(&mut self, line: String) {
        self.lines.push(line);
        if self.lines.len() > MAX_LINES {
            let excess = self.lines.len() - MAX_LINES;
            self.lines.drain(0..excess);
        }
    }

    fn push_text(&mut self, text: &str) {
        for line in text.lines() {
            self.push(line.to_string());
        }
        if text.is_empty() {
            self.push(String::new());
        }
    }

    fn prompt(&self) -> String {
        let path = self.display_path();
        format!("{path} $ ")
    }

    fn display_path(&self) -> String {
        if self.cwd == "/home/zirvium" {
            String::from("~")
        } else if let Some(rest) = self.cwd.strip_prefix("/home/zirvium/") {
            format!("~/{rest}")
        } else {
            self.cwd.clone()
        }
    }

    fn submit(&mut self) {
        let prompt = self.prompt();
        let input = self.input.clone();
        self.push(format!("{prompt}{input}"));
        self.input.clear();
        self.cursor = 0;
        self.scroll = 0;
        if input.trim().is_empty() {
            return;
        }
        if self.history.last().map(|entry| entry != &input).unwrap_or(true) {
            self.history.push(input.clone());
        }
        self.history_position = None;
        self.execute(&input);
    }

    fn execute(&mut self, input: &str) {
        let mut parts = input.split_whitespace();
        let command = parts.next().unwrap_or("");
        let arguments: Vec<&str> = parts.collect();

        match command {
            "help" => {
                self.push(String::from("Zirvium shell commands"));
                self.push(String::from("  help                 this list"));
                self.push(String::from("  neofetch             system summary"));
                self.push(String::from("  ps                   MOSIX processes"));
                self.push(String::from("  nodes                cluster members"));
                self.push(String::from("  migrate PID NODE     move a process"));
                self.push(String::from("  spawn NAME           create a process"));
                self.push(String::from("  kill PID             remove a process"));
                self.push(String::from("  gpu                  display adapter"));
                self.push(String::from("  fonts                loaded font faces"));
                self.push(String::from("  dmesg                boot log"));
                self.push(String::from("  ls [PATH]            list a directory"));
                self.push(String::from("  cat PATH             print a file"));
                self.push(String::from("  tree [PATH]          directory tree"));
                self.push(String::from("  cd [PATH]            change directory"));
                self.push(String::from("  pwd                  working directory"));
                self.push(String::from("  mem                  memory usage"));
                self.push(String::from("  uname | uptime | date"));
                self.push(String::from("  echo TEXT | clear | history"));
                self.push(String::from("  about                DisplayJet and MOSIX"));
            }
            "neofetch" => {
                let gpu = crate::drivers::pci::gpu();
                let memory = crate::mm::stats();
                let thrust = crate::time::uptime_string();
                let rows = [
                    ("OS", format!("Zirvium {} (64-bit)", zirvstdlib::VERSION)),
                    ("Host", String::from("UEFI / x86_64")),
                    ("Kernel", String::from("Rust + assembly, no libc")),
                    ("CPU", format!(
                        "{} ({} logical)",
                        crate::arch::cpu::vendor_str(),
                        crate::arch::cpu::logical_processors()
                    )),
                    ("GPU", gpu.name_str().to_string()),
                    ("Display", format!(
                        "DisplayJet {}x{} SIMD compositor",
                        crate::ui::WIDTH_HINT.load(core::sync::atomic::Ordering::Relaxed),
                        crate::ui::HEIGHT_HINT.load(core::sync::atomic::Ordering::Relaxed)
                    )),
                    ("Memory", format!("{} free", widgets::byte_size(memory.free_bytes))),
                    ("Cluster", format!(
                        "MOSIX node 0 of {} ({} processes)",
                        crate::mosix::node_count(),
                        crate::mosix::process_count()
                    )),
                    ("Uptime", thrust.to_string()),
                ];
                let logo = [
                    "        /\\        ",
                    "       /  \\       ",
                    "      / /\\ \\      ",
                    "     / /  \\ \\     ",
                    "    / /____\\ \\    ",
                    "   /__________\\   ",
                    "   \\__________/   ",
                ];
                for (index, row) in rows.iter().enumerate() {
                    let art = logo.get(index).copied().unwrap_or("                  ");
                    self.push(format!("{art}  {:<8} {}", row.0, row.1));
                }
                self.push(String::from("                  Zirvium: one cluster, one display."));
            }
            "ps" => {
                self.push(format!(
                    "{:>4} {:>5} {:<9} {:<24} {:>8}",
                    "PID", "PPID", "STATE", "NAME", "TICKS"
                ));
                for process in crate::mosix::processes() {
                    self.push(format!(
                        "{:>4} {:>5} {:<9} {:<24} {:>8}",
                        process.pid,
                        process.parent_pid,
                        crate::mosix::process_state_name(process.state),
                        process.name_str(),
                        process.cpu_ticks
                    ));
                }
            }
            "nodes" => {
                self.push_text(&crate::mosix::nodes_text());
                self.push(format!(
                    "{} migration(s) since boot.",
                    crate::mosix::migrations()
                ));
            }
            "migrate" => {
                let pid = arguments.first().and_then(|value| value.parse::<u32>().ok());
                let node = arguments.get(1).and_then(|value| value.parse::<u32>().ok());
                match (pid, node) {
                    (Some(pid), Some(node)) => {
                        if crate::mosix::migrate(pid, node) {
                            self.push(format!("migrated process {pid} to node {node}"));
                        } else {
                            self.push(format!(
                                "migration failed: pid {pid} or node {node} unavailable"
                            ));
                        }
                    }
                    _ => self.push(String::from("usage: migrate PID NODE")),
                }
            }
            "spawn" => {
                let name = arguments.join(" ");
                if name.is_empty() {
                    self.push(String::from("usage: spawn NAME"));
                } else {
                    let pid = crate::mosix::spawn_process(
                        crate::mosix::desktop_pid(),
                        &name,
                        1024,
                    );
                    if pid == 0 {
                        self.push(String::from("spawn failed: process table full"));
                    } else {
                        self.push(format!("spawned '{name}' as pid {pid}"));
                    }
                }
            }
            "kill" => {
                let pid = arguments.first().and_then(|value| value.parse::<u32>().ok());
                match pid {
                    Some(pid) if pid > 7 => {
                        if crate::mosix::exit_process(pid) {
                            self.push(format!("process {pid} exited"));
                        } else {
                            self.push(format!("no process with pid {pid}"));
                        }
                    }
                    Some(pid) => self.push(format!("pid {pid} is a kernel process")),
                    None => self.push(String::from("usage: kill PID")),
                }
            }
            "gpu" => {
                let gpu = crate::drivers::pci::gpu();
                self.push(format!("adapter   {}", gpu.name_str()));
                self.push(format!("vendor    {:?} ({:04x})", gpu.vendor, gpu.vendor_id));
                self.push(format!("device    {:04x}", gpu.device_id));
                self.push(format!(
                    "mmio      {:#x}..{:#x}",
                    gpu.mmio_base,
                    gpu.mmio_base + gpu.mmio_size
                ));
                self.push(format!("backend   {:?}", gpu.driver));
                self.push(String::from(
                    "path      DisplayJet ARGB surface -> firmware scanout",
                ));
            }
            "fonts" => {
                for face in crate::displayjet::font::faces() {
                    self.push(format!(
                        "  {:<4} {:>2}px  ascent {:>2}  descent {:>2}  line {:>2}  {}",
                        if face.bold { "bold" } else { "reg" },
                        face.size,
                        face.ascent,
                        face.descent,
                        face.line_height,
                        face.glyphs.len()
                    ));
                }
            }
            "dmesg" => {
                let mut lines = [crate::logger::LogLine {
                    level: zirvstdlib::log::Level::Info,
                    len: 0,
                    text: [0; 192],
                }; 64];
                let count = crate::logger::recent(&mut lines);
                for line in &lines[..count] {
                    self.push(line.as_str().to_string());
                }
            }
            "ls" => {
                let path = arguments.first().copied().unwrap_or(".");
                let resolved = crate::mosix::vfs::resolve(&self.cwd, path);
                if crate::mosix::vfs::is_dir(&resolved) {
                    let mut line = String::new();
                    for entry in crate::mosix::vfs::list(&resolved) {
                        let label = if entry.is_dir {
                            format!("{}/", entry.name)
                        } else {
                            entry.name.clone()
                        };
                        if line.len() + label.len() + 2 > 72 {
                            self.push(line);
                            line = String::new();
                        }
                        if !line.is_empty() {
                            line.push_str("  ");
                        }
                        line.push_str(&label);
                    }
                    if !line.is_empty() {
                        self.push(line);
                    }
                } else {
                    self.push(format!("ls: {path}: not a directory"));
                }
            }
            "cat" => {
                let path = arguments.first().copied().unwrap_or("");
                if path.is_empty() {
                    self.push(String::from("usage: cat PATH"));
                } else {
                    let resolved = crate::mosix::vfs::resolve(&self.cwd, path);
                    match crate::mosix::vfs::read_limited(&resolved, 32 * 1024) {
                        Some(content) => self.push_text(content.trim_end()),
                        None => self.push(format!("cat: {path}: no such file")),
                    }
                }
            }
            "tree" => {
                let path = arguments.first().copied().unwrap_or(".");
                let resolved = crate::mosix::vfs::resolve(&self.cwd, path);
                if crate::mosix::vfs::is_dir(&resolved) {
                    for line in crate::mosix::vfs::tree(&resolved, 0) {
                        self.push(line);
                    }
                } else {
                    self.push(format!("tree: {path}: not a directory"));
                }
            }
            "cd" => {
                let path = arguments.first().copied().unwrap_or("/home/zirvium");
                let resolved = crate::mosix::vfs::resolve(&self.cwd, path);
                if crate::mosix::vfs::is_dir(&resolved) {
                    self.cwd = resolved;
                } else {
                    self.push(format!("cd: {path}: not a directory"));
                }
            }
            "pwd" => self.push(self.cwd.clone()),
            "mem" => {
                let stats = crate::mm::stats();
                self.push(format!(
                    "physical  {} total, {} free",
                    widgets::byte_size(stats.total_bytes),
                    widgets::byte_size(stats.free_bytes)
                ));
                self.push(format!(
                    "heap      {} used of {}",
                    widgets::byte_size(stats.heap_used as u64),
                    widgets::byte_size(stats.heap_total as u64)
                ));
            }
            "uname" => self.push(format!(
                "Zirvium {} x86_64 Rust/assembly",
                zirvstdlib::VERSION
            )),
            "uptime" => self.push(format!(
                "up {}, load {}‰",
                crate::time::uptime_string(),
                crate::mosix::load_permille()
            )),
            "date" => {
                let now = crate::time::now();
                self.push(format!(
                    "{} {} {} {:02}:{:02}:{:02}",
                    now.weekday_name(),
                    now.month_name(),
                    now.day,
                    now.hour,
                    now.minute,
                    now.second
                ));
            }
            "echo" => {
                let text = input
                    .split_once(' ')
                    .map(|(_, rest)| rest)
                    .unwrap_or("");
                self.push(text.to_string());
            }
            "clear" => {
                self.lines.clear();
                self.scroll = 0;
            }
            "history" => {
                let mut lines = Vec::new();
                for (index, entry) in self.history.iter().enumerate() {
                    lines.push(format!("{:>4}  {}", index + 1, entry));
                }
                for line in lines {
                    self.push(line);
                }
            }
            "whoami" => self.push(String::from("zirvium")),
            "about" | "displayjet" | "mosix" => {
                self.push(String::from(
                    "Zirvium is built around two ideas from earlier systems:",
                ));
                self.push(String::from(
                    "  MOSIX      the cluster is one pool of resources; processes",
                ));
                self.push(String::from(
                    "             have a home node and a current node and migrate.",
                ));
                self.push(String::from(
                    "  DisplayJet the display is a first-class engine with damage,",
                ));
                self.push(String::from(
                    "             anti-aliased text and a hardware-boundary blit.",
                ));
                self.push(String::from(
                    "Every line of this system is Rust or x86-64 assembly.",
                ));
            }
            "version" => self.push(format!(
                "Zirvium {}  zirvstdlib {}  ABI {}",
                zirvstdlib::VERSION,
                zirvstdlib::VERSION,
                zirvstdlib::abi::ABI_VERSION
            )),
            "reboot" | "poweroff" | "halt" => {
                self.push(String::from(
                    "power management is not wired to the ACPI driver yet",
                ));
            }
            other => {
                self.push(format!("{other}: command not found. Try 'help'."));
            }
        }
    }

    fn insert(&mut self, character: char) {
        if character == '\0' {
            return;
        }
        self.input.insert(self.cursor, character);
        self.cursor += character.len_utf8();
    }

    fn previous_character(&mut self) {
        if self.cursor == 0 {
            return;
        }
        let mut index = self.cursor - 1;
        while index > 0 && !self.input.is_char_boundary(index) {
            index -= 1;
        }
        self.cursor = index;
    }

    fn next_character(&mut self) {
        if self.cursor >= self.input.len() {
            return;
        }
        let mut index = self.cursor + 1;
        while index < self.input.len() && !self.input.is_char_boundary(index) {
            index += 1;
        }
        self.cursor = index;
    }

    pub fn draw(
        &mut self,
        display: &mut Display,
        content: Rect,
        theme: &Theme,
        focused: bool,
    ) {
        let body = Rect::new(content.x, content.y, content.w, content.h);
        display.fill_rect(body, rgb(0x0b0e15));
        // A faint top edge separates the terminal from the title bar.
        display.fill_rect(
            Rect::new(body.x, body.y, body.w, 1),
            argb(0x40, 0xffffff),
        );

        let face = crate::displayjet::font::face(14, false);
        let line_height = (face.line_height + 1).max(14);
        let padding = 14;
        let text_width = body.w - padding * 2 - 8;
        let available = (body.h - padding * 2) / line_height - 1;
        if available <= 0 {
            return;
        }

        // Wrap all logical lines to the current width.
        let mut wrapped: Vec<String> = Vec::new();
        for line in &self.lines {
            if line.is_empty() {
                wrapped.push(String::new());
                continue;
            }
            if face.width(line) <= text_width {
                wrapped.push(line.clone());
                continue;
            }
            let mut current = String::new();
            for character in line.chars() {
                let mut probe = current.clone();
                probe.push(character);
                if face.width(&probe) > text_width && !current.is_empty() {
                    wrapped.push(current);
                    current = String::new();
                }
                current.push(character);
            }
            if !current.is_empty() {
                wrapped.push(current);
            }
        }
        let total = wrapped.len() + 1;
        let max_scroll = total.saturating_sub(available as usize);
        self.scroll = self.scroll.min(max_scroll);
        let start = total.saturating_sub(available as usize + self.scroll);
        let end = (start + available as usize).min(wrapped.len());

        let mut y = body.y + padding;
        for line in &wrapped[start.min(wrapped.len())..end] {
            let color = if line.starts_with('~') && line.contains(" $ ") {
                theme.accent
            } else if line.starts_with("  ") {
                rgb(0xc3cad8)
            } else {
                TEXT
            };
            display.text_face(body.x + padding, y + face.ascent, line, face, color);
            y += line_height;
        }

        // The prompt and input, if the view is not scrolled up.
        if self.scroll == 0 {
            let prompt = self.prompt();
            let prompt_width = face.width(&prompt);
            display.text_face(
                body.x + padding,
                y + face.ascent,
                &prompt,
                face,
                theme.accent,
            );
            let input_x = body.x + padding + prompt_width;
            display.text_face(input_x, y + face.ascent, &self.input, face, TEXT);
            let caret_x = input_x + face.prefix_width(&self.input, self.char_count_before_cursor());
            let blink = (zirvstdlib::time::monotonic_ms() / 530) % 2 == 0;
            if focused && blink {
                display.fill_rect(
                    Rect::new(caret_x, y + 2, 2, (face.line_height - 4).max(8)),
                    theme.accent,
                );
            }
        }

        // Scroll indicator.
        if max_scroll > 0 {
            let track = Rect::new(body.right() - 7, body.y + 6, 3, body.h - 12);
            display.fill_rounded(track, 2, argb(0x30, 0xffffff));
            let length = ((track.h as f32) * available as f32 / total as f32)
                .max(18.0) as i32;
            let offset = (((track.h - length) as f32)
                * self.scroll as f32
                / max_scroll.max(1) as f32) as i32;
            display.fill_rounded(
                Rect::new(track.x, track.bottom() - length - offset, 3, length),
                2,
                theme.accent_soft(180),
            );
        }
    }

    fn char_count_before_cursor(&self) -> usize {
        self.input[..self.cursor].chars().count()
    }

    pub fn key(&mut self, code: u32, event_modifiers: u16) -> bool {
        let control = event_modifiers & modifiers::CTRL != 0;
        match code {
            key::ENTER => self.submit(),
            key::BACKSPACE => {
                if self.cursor > 0 {
                    self.previous_character();
                    let cursor = self.cursor;
                    self.input.remove(cursor);
                }
                self.scroll = 0;
            }
            key::DELETE => {
                if self.cursor < self.input.len() {
                    let cursor = self.cursor;
                    self.input.remove(cursor);
                }
            }
            key::LEFT => self.previous_character(),
            key::RIGHT => self.next_character(),
            key::HOME => self.cursor = 0,
            key::END => self.cursor = self.input.len(),
            key::UP => {
                if !self.history.is_empty() {
                    let position = match self.history_position {
                        None => self.history.len() - 1,
                        Some(0) => 0,
                        Some(position) => position - 1,
                    };
                    self.history_position = Some(position);
                    self.input = self.history[position].clone();
                    self.cursor = self.input.len();
                }
            }
            key::DOWN => {
                if let Some(position) = self.history_position {
                    if position + 1 < self.history.len() {
                        self.history_position = Some(position + 1);
                        self.input = self.history[position + 1].clone();
                    } else {
                        self.history_position = None;
                        self.input.clear();
                    }
                    self.cursor = self.input.len();
                }
            }
            key::PAGE_UP => self.scroll = self.scroll.saturating_add(8),
            key::PAGE_DOWN => self.scroll = self.scroll.saturating_sub(8),
            key::TAB => {
                for _ in 0..4 {
                    self.insert(' ');
                }
            }
            key::ESCAPE => {
                self.input.clear();
                self.cursor = 0;
            }
            _ => {
                if control {
                    match code as u8 {
                        b'c' | b'C' => {
                            self.input.clear();
                            self.cursor = 0;
                        }
                        b'l' | b'L' => {
                            self.lines.clear();
                            self.scroll = 0;
                        }
                        b'u' | b'U' => {
                            self.input.clear();
                            self.cursor = 0;
                        }
                        _ => {}
                    }
                    return true;
                }
                if code >= 0x20 && code != 0x7f {
                    if let Some(character) = char::from_u32(code) {
                        self.insert(character);
                        self.scroll = 0;
                    }
                }
            }
        }
        true
    }

    pub fn scroll(&mut self, amount: i32) {
        if amount > 0 {
            self.scroll = self.scroll.saturating_add(amount as usize * 3);
        } else {
            self.scroll = self.scroll.saturating_sub((-amount) as usize * 3);
        }
    }

    pub fn pointer(&mut self, point: Point, content: Rect) -> bool {
        // Clicking the body scrolls to the bottom, like a terminal emulator.
        if content.contains(point) {
            self.scroll = 0;
            return true;
        }
        false
    }
}
