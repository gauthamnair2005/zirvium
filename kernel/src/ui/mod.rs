//! The Zirvium desktop.
//!
//! The desktop is the first DisplayJet client: it owns the wallpaper cache,
//! the window stack, the top bar, the dock and the launcher, and it routes
//! input to the focused application.  Repainting is deliberately explicit —
//! a scene redraw only happens when something structural changes, while
//! pointer motion just moves the saved cursor patch.  That keeps motion
//! smooth even though the scene itself is rasterised on the CPU.

pub mod apps;
pub mod terminal;
pub mod theme;
pub mod wallpaper;
pub mod widgets;

use alloc::string::String;
use alloc::vec::Vec;
use core::sync::atomic::{AtomicU32, Ordering};

use zirvstdlib::abi::{InputEvent, InputKind, buttons, key, modifiers};
use zirvstdlib::geometry::{Point, Rect};

use crate::displayjet::Display;
use crate::ui::apps::Response;
use crate::ui::theme::*;
use crate::ui::widgets::Icon;

/// Latest desktop size, published for applications that render without a
/// direct reference to the display (the terminal's `neofetch`, for example).
pub static WIDTH_HINT: AtomicU32 = AtomicU32::new(1280);
pub static HEIGHT_HINT: AtomicU32 = AtomicU32::new(800);

// ---------------------------------------------------------------------------
// Applications and windows
// ---------------------------------------------------------------------------

/// The applications the desktop knows how to open.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum AppKind {
    Terminal,
    Files,
    Monitor,
    Nodes,
    Settings,
    Calculator,
    Notes,
    Network,
}

/// Apps pinned to the taskbar.
const DOCK_APPS: [AppKind; 5] = [
    AppKind::Terminal,
    AppKind::Files,
    AppKind::Monitor,
    AppKind::Nodes,
    AppKind::Settings,
];

/// Apps shown on the Start screen. The taskbar only carries the essentials.
const START_APPS: [AppKind; 8] = [
    AppKind::Terminal,
    AppKind::Files,
    AppKind::Monitor,
    AppKind::Nodes,
    AppKind::Settings,
    AppKind::Calculator,
    AppKind::Notes,
    AppKind::Network,
];

impl AppKind {
    pub fn title(self) -> &'static str {
        match self {
            AppKind::Terminal => "Terminal",
            AppKind::Files => "Files",
            AppKind::Monitor => "System Monitor",
            AppKind::Nodes => "MOSIX Nodes",
            AppKind::Settings => "Settings",
            AppKind::Calculator => "Calculator",
            AppKind::Notes => "Notes",
            AppKind::Network => "Network",
        }
    }

    pub fn subtitle(self) -> &'static str {
        match self {
            AppKind::Terminal => "MOSIX shell and diagnostics",
            AppKind::Files => "Browse the virtual filesystem",
            AppKind::Monitor => "CPU, memory and process telemetry",
            AppKind::Nodes => "Cluster members and migration",
            AppKind::Settings => "System, colors and Start",
            AppKind::Calculator => "Quick arithmetic",
            AppKind::Notes => "Text notes and scratchpad",
            AppKind::Network => "Wi-Fi and cluster links",
        }
    }

    pub fn icon(self) -> Icon {
        match self {
            AppKind::Terminal => Icon::Terminal,
            AppKind::Files => Icon::Files,
            AppKind::Monitor => Icon::Monitor,
            AppKind::Nodes => Icon::Nodes,
            AppKind::Settings => Icon::Settings,
            AppKind::Calculator => Icon::Plus,
            AppKind::Notes => Icon::Document,
            AppKind::Network => Icon::Nodes,
        }
    }

    fn default_size(self) -> (i32, i32) {
        match self {
            AppKind::Terminal => (780, 470),
            AppKind::Files => (830, 520),
            AppKind::Monitor => (760, 540),
            AppKind::Nodes => (820, 590),
            AppKind::Settings => (860, 640),
            AppKind::Calculator => (320, 430),
            AppKind::Notes => (640, 460),
            AppKind::Network => (620, 500),
        }
    }
}

enum App {
    Terminal(terminal::Terminal),
    Files(apps::Files),
    Monitor(apps::Monitor),
    Nodes(apps::Nodes),
    Settings(apps::Settings),
    Calculator(apps::Calculator),
    Notes(apps::Notes),
    Network(apps::Network),
}

impl App {
    fn new(kind: AppKind) -> Self {
        match kind {
            AppKind::Terminal => App::Terminal(terminal::Terminal::new()),
            AppKind::Files => App::Files(apps::Files::new()),
            AppKind::Monitor => App::Monitor(apps::Monitor::new()),
            AppKind::Nodes => App::Nodes(apps::Nodes::new()),
            AppKind::Settings => App::Settings(apps::Settings::new()),
            AppKind::Calculator => App::Calculator(apps::Calculator::new()),
            AppKind::Notes => App::Notes(apps::Notes::new()),
            AppKind::Network => App::Network(apps::Network::new()),
        }
    }

    fn kind(&self) -> AppKind {
        match self {
            App::Terminal(_) => AppKind::Terminal,
            App::Files(_) => AppKind::Files,
            App::Monitor(_) => AppKind::Monitor,
            App::Nodes(_) => AppKind::Nodes,
            App::Settings(_) => AppKind::Settings,
            App::Calculator(_) => AppKind::Calculator,
            App::Notes(_) => AppKind::Notes,
            App::Network(_) => AppKind::Network,
        }
    }

    fn draw(&mut self, display: &mut Display, content: Rect, theme: &Theme, focused: bool) {
        match self {
            App::Terminal(app) => app.draw(display, content, theme, focused),
            App::Files(app) => app.draw(display, content, theme, focused),
            App::Monitor(app) => app.draw(display, content, theme, focused),
            App::Nodes(app) => app.draw(display, content, theme, focused),
            App::Settings(app) => { let _ = focused; app.draw(display, content, theme); },
            App::Calculator(app) => app.draw(display, content, theme, focused),
            App::Notes(app) => app.draw(display, content, theme, focused),
            App::Network(app) => app.draw(display, content, theme, focused),
        }
    }

    fn pointer(&mut self, point: Point, content: Rect, theme: &mut Theme) -> Response {
        match self {
            App::Terminal(app) => {
                if app.pointer(point, content) {
                    Response::REDRAW
                } else {
                    Response::NONE
                }
            }
            App::Files(app) => app.pointer(point, content, theme),
            App::Monitor(_) => Response::NONE,
            App::Nodes(app) => app.pointer(point, content, theme),
            App::Settings(app) => app.pointer(point, content, theme),
            App::Calculator(app) => app.pointer(point, content, theme),
            App::Notes(app) => app.pointer(point, content, theme),
            App::Network(app) => app.pointer(point, content, theme),
        }
    }

    fn key(&mut self, code: u32, event_modifiers: u16) -> bool {
        match self {
            App::Terminal(app) => app.key(code, event_modifiers),
            App::Files(app) => app.key(code, event_modifiers),
            App::Monitor(_) => false,
            App::Nodes(_) => false,
            App::Settings(_) => false,
            App::Calculator(app) => app.key(code, event_modifiers),
            App::Notes(app) => app.key(code, event_modifiers),
            App::Network(_) => false,
        }
    }

    fn scroll(&mut self, point: Point, content: Rect, amount: i32) {
        match self {
            App::Terminal(app) => app.scroll(amount),
            App::Files(app) => app.scroll(point, content, amount),
            App::Monitor(_) => {}
            App::Nodes(_) => {}
            App::Settings(app) => app.scroll(point, content, amount),
            App::Calculator(app) => app.scroll(point, content, amount),
            App::Notes(app) => app.scroll(point, content, amount),
            App::Network(app) => app.scroll(point, content, amount),
        }
    }

    fn tick(&mut self) -> bool {
        match self {
            App::Monitor(app) => app.tick(),
            App::Calculator(app) => app.tick(),
            App::Notes(app) => app.tick(),
            App::Network(app) => app.tick(),
            _ => false,
        }
    }
}

struct Window {
    id: u32,
    app: App,
    rect: Rect,
    /// Geometry to restore when leaving the maximised state.
    restore: Option<Rect>,
    minimized: bool,
    maximized: bool,
}

impl Window {
    fn content(&self) -> Rect {
        Rect::new(
            self.rect.x + 1,
            self.rect.y + TITLE_HEIGHT,
            self.rect.w - 2,
            self.rect.h - TITLE_HEIGHT - 1,
        )
    }
}

// ---------------------------------------------------------------------------
// Hit testing
// ---------------------------------------------------------------------------

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum Target {
    Nothing,
    Start,
    Power,
    Dock(AppKind),
    Close(u32),
    Minimize(u32),
    Maximize(u32),
    Title(u32),
    Content(u32),
    Resize(u32),
    Desktop,
}

#[derive(Clone, Copy)]
enum Drag {
    Move { id: u32, offset: Point },
    Resize { id: u32, origin: Point, rect: Rect },
}

struct Notification {
    title: String,
    body: String,
    born: u64,
    life: u64,
}

// ---------------------------------------------------------------------------
// Layout helpers
// ---------------------------------------------------------------------------

const DOCK_GAP: i32 = 14;

fn launcher_button_rect() -> Rect {
    Rect::new(10, 6, 116, 24)
}

fn clock_rect(width: i32) -> Rect {
    Rect::new((width - 196).max(0), 0, 192, TOP_BAR_HEIGHT)
}

fn dock_panel(width: i32, height: i32) -> Rect {
    let count = DOCK_APPS.len() as i32;
    let panel_width = count * DOCK_ICON + (count - 1) * DOCK_GAP + 34;
    Rect::new(
        (width - panel_width) / 2,
        height - DOCK_HEIGHT + 4,
        panel_width,
        68,
    )
}

fn dock_icon_rect(panel: Rect, index: usize) -> Rect {
    Rect::new(
        panel.x + 17 + index as i32 * (DOCK_ICON + DOCK_GAP),
        panel.y + 11,
        DOCK_ICON,
        DOCK_ICON,
    )
}

fn launch_panel_rect() -> Rect {
    let rows = DOCK_APPS.len() as i32;
    Rect::new(14, TOP_BAR_HEIGHT + 10, 306, 60 + rows * 50 + 34)
}

fn launch_row_rect(index: usize) -> Rect {
    let panel = launch_panel_rect();
    Rect::new(
        panel.x + 10,
        panel.y + 52 + index as i32 * 50,
        panel.w - 20,
        46,
    )
}

fn title_buttons(rect: Rect) -> (Rect, Rect, Rect) {
    let size = 14;
    let y = rect.y + (TITLE_HEIGHT - size) / 2;
    let right = rect.right();
    (
        Rect::new(right - 30, y, size, size),
        Rect::new(right - 52, y, size, size),
        Rect::new(right - 74, y, size, size),
    )
}

/// Draw a faint alignment grid.  Settings exposes this as a toggle.
fn draw_grid(display: &mut Display, width: i32, height: i32, theme: &Theme) {
    let step = 64;
    let color = argb(0x0e, 0xffffff);
    let mut x = step;
    while x < width {
        display.fill_rect(Rect::new(x, 0, 1, height), color);
        x += step;
    }
    let mut y = step;
    while y < height {
        display.fill_rect(Rect::new(0, y, width, 1), color);
        y += step;
    }
    let _ = theme;
}

// ---------------------------------------------------------------------------
// Desktop
// ---------------------------------------------------------------------------

pub struct Desktop {
    width: i32,
    height: i32,
    theme: Theme,
    windows: Vec<Window>,
    next_id: u32,
    focused: Option<u32>,
    hover: Target,
    drag: Option<Drag>,
    launcher: bool,
    launcher_selected: usize,
    cursor: Point,
    wallpaper: Vec<u32>,
    last_wallpaper_key: Option<(u8, u32, u32)>,
    notifications: Vec<Notification>,
    last_second: u64,
    last_title_click: (u32, u64),
    launcher_started: u64,
    logged_in: bool,
}

impl Desktop {
    fn new(display: &Display) -> Self {
        let width = display.width;
        let height = display.height;
        let (cursor_x, cursor_y) = crate::drivers::ps2::cursor_position();
        let mut desktop = Self {
            width,
            height,
            theme: Theme::default(),
            windows: Vec::new(),
            next_id: 1,
            focused: None,
            hover: Target::Nothing,
            drag: None,
            launcher: false,
            launcher_selected: 0,
            cursor: Point::new(cursor_x, cursor_y),
            wallpaper: Vec::new(),
            last_wallpaper_key: None,
            notifications: Vec::new(),
            last_second: 0,
            last_title_click: (0, 0),
            launcher_started: zirvstdlib::time::monotonic_ms(),
            logged_in: false,
        };
        desktop.notify(
            "Welcome to Zirvium 2.0",
            "Press Start or click the logo to open the Start screen.",
            9000,
        );
        // Boot into the desktop after the splash; Start remains one click away.
        desktop.launcher = false;
        desktop
    }

    // -- Window management --------------------------------------------------

    fn window_index(&self, id: u32) -> Option<usize> {
        self.windows.iter().position(|window| window.id == id)
    }

    fn window(&self, id: u32) -> Option<&Window> {
        self.windows.iter().find(|window| window.id == id)
    }

    fn window_mut(&mut self, id: u32) -> Option<&mut Window> {
        self.windows.iter_mut().find(|window| window.id == id)
    }

    fn focus(&mut self, id: u32) {
        let Some(index) = self.window_index(id) else {
            return;
        };
        if index + 1 != self.windows.len() {
            let window = self.windows.remove(index);
            self.windows.push(window);
        }
        self.focused = Some(id);
    }

    fn focus_top(&mut self) {
        self.focused = self
            .windows
            .iter()
            .rev()
            .find(|window| !window.minimized)
            .map(|window| window.id);
    }

    fn open(&mut self, kind: AppKind) {
        if let Some(index) = self
            .windows
            .iter()
            .position(|window| window.app.kind() == kind)
        {
            let id = self.windows[index].id;
            self.windows[index].minimized = false;
            self.focus(id);
            return;
        }
        let (desired_w, desired_h) = kind.default_size();
        let max_w = (self.width - 32).max(360);
        let max_h = (self.height - TOP_BAR_HEIGHT - DOCK_HEIGHT - 16).max(240);
        let w = desired_w.min(max_w);
        let h = desired_h.min(max_h);
        let cascade = (self.windows.len() as i32 % 6) * 28;
        let max_x = (self.width - w - 16).max(16);
        let max_y = (self.height - h - DOCK_HEIGHT - 8).max(TOP_BAR_HEIGHT + 8);
        let x = ((self.width - w) / 2 - 60 + cascade).clamp(16, max_x);
        let y = (TOP_BAR_HEIGHT + 36 + cascade).clamp(TOP_BAR_HEIGHT + 8, max_y);
        let id = self.next_id;
        self.next_id += 1;
        self.windows.push(Window {
            id,
            app: App::new(kind),
            rect: Rect::new(x, y, w, h),
            restore: None,
            minimized: false,
            maximized: false,
        });
        self.focused = Some(id);
    }

    fn close(&mut self, id: u32) {
        if let Some(index) = self.window_index(id) {
            self.windows.remove(index);
        }
        if self.focused == Some(id) {
            self.focus_top();
        }
        if matches!(self.drag, Some(Drag::Move { id: drag_id, .. }) | Some(Drag::Resize { id: drag_id, .. }) if drag_id == id)
        {
            self.drag = None;
        }
    }

    fn minimize(&mut self, id: u32) {
        if let Some(window) = self.window_mut(id) {
            window.minimized = true;
        }
        if self.focused == Some(id) {
            self.focus_top();
        }
        self.drag = None;
    }

    fn toggle_maximize(&mut self, id: u32) {
        let (width, height) = (self.width, self.height);
        if let Some(window) = self.window_mut(id) {
            if window.maximized {
                if let Some(restore) = window.restore.take() {
                    window.rect = restore;
                }
                window.maximized = false;
            } else {
                window.restore = Some(window.rect);
                window.rect = Rect::new(0, TOP_BAR_HEIGHT, width, (height - TOP_BAR_HEIGHT - DOCK_HEIGHT).max(160));
                window.maximized = true;
            }
            self.focused = Some(id);
        }
    }

    fn cycle_windows(&mut self) {
        let visible: Vec<u32> = self
            .windows
            .iter()
            .filter(|window| !window.minimized)
            .map(|window| window.id)
            .collect();
        if visible.len() < 2 {
            return;
        }
        let current = self
            .focused
            .and_then(|id| visible.iter().position(|candidate| *candidate == id))
            .unwrap_or(0);
        let next = visible[(current + 1) % visible.len()];
        self.focus(next);
    }

    fn focused_kind(&self) -> Option<AppKind> {
        self.focused
            .and_then(|id| self.window(id))
            .map(|window| window.app.kind())
    }

    // -- Notifications ------------------------------------------------------

    fn notify(&mut self, title: &str, body: &str, life: u64) {
        self.notifications.push(Notification {
            title: String::from(title),
            body: String::from(body),
            born: zirvstdlib::time::monotonic_ms(),
            life,
        });
        if self.notifications.len() > 3 {
            self.notifications.remove(0);
        }
    }

    // -- Input --------------------------------------------------------------

    fn target_at(&self, point: Point) -> Target {
        if !self.logged_in {
            return Target::Nothing;
        }
        if self.launcher {
            return Target::Nothing;
        }

        if point.y >= self.height - DOCK_HEIGHT {
            let dock = dock_panel(self.width, self.height);
            if dock.contains(point) {
                for (index, kind) in DOCK_APPS.iter().enumerate() {
                    if dock_icon_rect(dock, index).contains(point) {
                        return Target::Dock(*kind);
                    }
                }
                return Target::Nothing;
            }
            if Rect::new(8, self.height - DOCK_HEIGHT + 6, 52, 60).contains(point) {
                return Target::Start;
            }
            if Rect::new(self.width - 60, self.height - DOCK_HEIGHT + 6, 52, 60).contains(point) {
                return Target::Power;
            }
            return Target::Nothing;
        }

        if point.y < TOP_BAR_HEIGHT {
            return Target::Nothing;
        }

        for window in self.windows.iter().rev() {
            if window.minimized {
                continue;
            }
            let rect = window.rect;
            if !rect.contains(point) {
                continue;
            }
            let (close, minimize, maximize) = title_buttons(rect);
            if close.contains(point) {
                return Target::Close(window.id);
            }
            if minimize.contains(point) {
                return Target::Minimize(window.id);
            }
            if maximize.contains(point) {
                return Target::Maximize(window.id);
            }
            if !window.maximized
                && rect.bottom() - point.y <= 16
                && rect.right() - point.x <= 16
            {
                return Target::Resize(window.id);
            }
            if point.y < rect.y + TITLE_HEIGHT {
                return Target::Title(window.id);
            }
            return Target::Content(window.id);
        }
        Target::Desktop
    }

    fn pointer_down(&mut self, point: Point) -> bool {
        if !self.logged_in {
            let button = Rect::new(self.width / 2 - 92, self.height / 2 + 92, 184, 38);
            if button.contains(point) {
                self.logged_in = true;
                self.notify("Welcome back", "Signed in as liveuser.", 5000);
                return true;
            }
            return false;
        }
        if self.launcher {
            let screen_w = self.width;
            // Click logo to close
            let logo = Rect::new(58, 42, 44, 44);
            if logo.contains(point) {
                self.launcher = false;
                return true;
            }
            let mut x = 60i32;
            let mut y = 190i32;
            let mut i = 0;
            for kind in START_APPS.iter() {
                let (tw, th) = match i {
                    0 => (236, 110),
                    _ => (110, 110),
                };
                let rect = Rect::new(x, y, tw, th);
                if rect.contains(point) {
                    self.launcher = false;
                    self.open(*kind);
                    return true;
                }
                if tw == 236 {
                    x += 252;
                } else {
                    x += 126;
                }
                i += 1;
                if x > 60 + 340 {
                    x = 60;
                    y += 126;
                }
            }
            let power_panel = Rect::new(screen_w - 210, self.height - 236, 150, 176);
            let shutdown = Rect::new(power_panel.x + 14, power_panel.y + 44, power_panel.w - 28, 36);
            let reboot = Rect::new(power_panel.x + 14, power_panel.y + 90, power_panel.w - 28, 36);
            if shutdown.contains(point) {
                self.launcher = false;
                crate::sprintln!("UI: shutdown requested");
                return true;
            }
            if reboot.contains(point) {
                self.launcher = false;
                crate::sprintln!("UI: reboot requested");
                return true;
            }
            let logout = Rect::new(power_panel.x + 14, power_panel.y + 136, power_panel.w - 28, 36);
            if logout.contains(point) {
                self.logged_in = false;
                self.launcher = false;
                self.focused = None;
                self.drag = None;
                crate::sprintln!("UI: user logged out");
                return true;
            }
            return true;
        }

        let target = self.target_at(point);
        let mut redraw = true;
        match target {
            Target::Start => {
                self.launcher = !self.launcher;
                if self.launcher {
                    self.launcher_selected = 0;
                    self.launcher_started = zirvstdlib::time::monotonic_ms();
                }
            }
            Target::Power => {
                self.launcher = !self.launcher;
                if self.launcher {
                    self.launcher_selected = 0;
                    self.launcher_started = zirvstdlib::time::monotonic_ms();
                }
            }
            Target::Dock(kind) => {
                let focused_same = self.focused_kind() == Some(kind);
                let existing = self
                    .windows
                    .iter()
                    .find(|window| window.app.kind() == kind)
                    .map(|window| (window.id, window.minimized));
                match existing {
                    Some((id, minimized)) => {
                        if minimized {
                            if let Some(window) = self.window_mut(id) {
                                window.minimized = false;
                            }
                            self.focus(id);
                        } else if focused_same {
                            self.minimize(id);
                        } else {
                            self.focus(id);
                        }
                    }
                    None => self.open(kind),
                }
            }
            Target::Close(id) => self.close(id),
            Target::Minimize(id) => self.minimize(id),
            Target::Maximize(id) => self.toggle_maximize(id),
            Target::Title(id) => {
                let now = zirvstdlib::time::monotonic_ms();
                if self.last_title_click.0 == id
                    && now.saturating_sub(self.last_title_click.1) < 420
                {
                    self.last_title_click = (0, 0);
                    self.toggle_maximize(id);
                } else {
                    self.last_title_click = (id, now);
                    let rect = self.window(id).map(|window| window.rect);
                    let (screen_width, screen_height) = (self.width, self.height);
                    if let Some(rect) = rect {
                        self.focus(id);
                        if let Some(window) = self.windows.iter_mut().find(|window| window.id == id)
                        {
                            if let Some(restore) = window.restore.take() {
                                // Dragging a maximised window restores it
                                // under the pointer.
                                let scale_x = if rect.w > 0 {
                                    (point.x - rect.x) as f32 / rect.w as f32
                                } else {
                                    0.5
                                };
                                let scale_y = if rect.h > 0 {
                                    (point.y - rect.y) as f32 / rect.h as f32
                                } else {
                                    0.05
                                };
                                window.rect = restore;
                                let new_x = point.x - (restore.w as f32 * scale_x) as i32;
                                let new_y = point.y - (restore.h as f32 * scale_y) as i32;
                                window.rect.x = new_x.clamp(16 - restore.w, screen_width - 64);
                                window.rect.y = new_y.clamp(TOP_BAR_HEIGHT, screen_height - 40);
                            }
                            let offset =
                                Point::new(point.x - window.rect.x, point.y - window.rect.y);
                            let id = window.id;
                            self.drag = Some(Drag::Move { id, offset });
                        }
                    }
                }
            }
            Target::Content(id) => {
                self.focus(id);
                let content = self.window(id).map(|window| window.content());
                if let Some(content) = content {
                    let mut changed_theme = false;
                    let mut changed = false;
                    if let Some(window) = self.windows.iter_mut().find(|window| window.id == id) {
                        let response = window.app.pointer(point, content, &mut self.theme);
                        changed = response.redraw;
                        changed_theme = response.theme_changed;
                    }
                    if changed_theme {
                        self.relayout_wallpaper();
                    }
                    let _ = changed;
                    redraw = true;
                }
            }
            Target::Desktop => {
                self.launcher = false;
                self.drag = None;
                redraw = true;
            }
            Target::Resize(id) => {
                let rect = self.window(id).map(|window| window.rect);
                if let Some(rect) = rect {
                    self.focus(id);
                    self.drag = Some(Drag::Resize {
                        id,
                        origin: point,
                        rect,
                    });
                }
            }
            Target::Nothing => redraw = false,
        }
        self.hover = self.target_at(self.cursor);
        redraw
    }

    fn pointer_move(&mut self, point: Point) -> bool {
        let mut redraw = false;
        if let Some(drag) = self.drag {
            match drag {
                Drag::Move { id, offset } => {
                    let (width, height) = (self.width, self.height);
                    if let Some(window) = self.window_mut(id) {
                        let w = window.rect.w;
                        let min_x = 96 - w;
                        let max_x = width - 64;
                        let x = (point.x - offset.x).clamp(min_x.min(max_x), max_x);
                        let y = (point.y - offset.y).clamp(TOP_BAR_HEIGHT, height - 40);
                        window.rect.x = x;
                        window.rect.y = y;
                    }
                    redraw = true;
                }
                Drag::Resize { id, origin, rect } => {
                    let (width, height) = (self.width, self.height);
                    if let Some(window) = self.window_mut(id) {
                        let new_w = (rect.w + point.x - origin.x).clamp(360, width - rect.x);
                        let new_h = (rect.h + point.y - origin.y).clamp(260, height - rect.y - 8);
                        window.rect.w = new_w;
                        window.rect.h = new_h;
                    }
                    redraw = true;
                }
            }
        }
        let target = self.target_at(point);
        if target != self.hover {
            self.hover = target;
            redraw = true;
        }
        redraw
    }

    fn handle(&mut self, event: &InputEvent) -> bool {
        match event.kind {
            InputKind::PointerMove => {
                self.cursor = Point::new(event.x, event.y);
                self.pointer_move(self.cursor)
            }
            InputKind::PointerDown => {
                self.cursor = Point::new(event.x, event.y);
                if event.buttons & buttons::LEFT != 0 {
                    self.pointer_down(self.cursor)
                } else {
                    false
                }
            }
            InputKind::PointerUp => {
                self.cursor = Point::new(event.x, event.y);
                let had_drag = self.drag.is_some();
                self.drag = None;
                self.hover = self.target_at(self.cursor);
                had_drag
            }
            InputKind::Scroll => {
                let point = Point::new(event.x, event.y);
                let target = self.target_at(point);
                if let Target::Content(id) = target
                    && let Some(content) = self.window(id).map(|window| window.content())
                {
                    if let Some(window) = self.window_mut(id) {
                        window.app.scroll(point, content, event.value);
                    }
                    return true;
                }
                false
            }
            InputKind::KeyDown => self.handle_key(event.code, event.modifiers),
            _ => false,
        }
    }

    fn handle_key(&mut self, code: u32, event_modifiers: u16) -> bool {
        if !self.logged_in {
            if code == key::ENTER {
                self.logged_in = true;
                self.notify("Welcome back", "Signed in as liveuser.", 5000);
                return true;
            }
            return false;
        }
        if code == key::SUPER {
            self.launcher = !self.launcher;
            self.launcher_selected = 0;
            if self.launcher {
                self.launcher_started = zirvstdlib::time::monotonic_ms();
            }
            return true;
        }
        // Right click simulation via shift+click not needed, handle via events later; keep simple
        if event_modifiers & modifiers::ALT != 0 && code == key::TAB {
            self.cycle_windows();
            return true;
        }
        if self.launcher {
            match code {
                key::ESCAPE => {
                    self.launcher = false;
                    return true;
                }
                _ => {
                    self.launcher = false;
                    return true;
                }
            }
        }
        let Some(id) = self.focused else {
            return false;
        };
        self.window_mut(id)
            .map(|window| window.app.key(code, event_modifiers))
            .unwrap_or(false)
    }

    // -- Timers -------------------------------------------------------------

    fn tick(&mut self) -> bool {
        let now_ms = zirvstdlib::time::monotonic_ms();
        let second = now_ms / 1000;
        let mut redraw = false;

        if self.launcher && now_ms.saturating_sub(self.launcher_started) < 420 {
            redraw = true;
        }

        if second != self.last_second {
            self.last_second = second;
            crate::mosix::tick();
            let noise = second.wrapping_mul(2_654_435_761) >> 9;
            let drag_boost = if self.drag.is_some() { 180 } else { 0 };
            let load = (140 + (noise % 300) as u32 + drag_boost).min(920);
            crate::mosix::set_load(load);
            crate::mosix::account(4, 1);
            crate::mosix::account(6, 1);
            if self.theme.show_seconds || second.is_multiple_of(60) {
                redraw = true;
            }
        }

        for window in self.windows.iter_mut() {
            if !window.minimized && window.app.tick() {
                redraw = true;
            }
        }

        let before = self.notifications.len();
        self.notifications
            .retain(|note| now_ms.saturating_sub(note.born) < note.life);
        if self.notifications.len() != before {
            redraw = true;
        }
        redraw
    }

    fn relayout_wallpaper(&mut self) {
        self.last_wallpaper_key = None;
    }

    fn ensure_wallpaper(&mut self) {
        let key = (
            self.theme.wallpaper,
            self.theme.accent,
            self.theme.accent_alt,
        );
        let expected = (self.width * self.height) as usize;
        if self.wallpaper.len() != expected || self.last_wallpaper_key != Some(key) {
            self.wallpaper = wallpaper::generate(
                self.width,
                self.height,
                self.theme.wallpaper,
                self.theme.accent,
                self.theme.accent_alt,
            );
            self.last_wallpaper_key = Some(key);
        }
    }

    // -- Drawing ------------------------------------------------------------

    fn draw(&mut self, display: &mut Display) {
        if !self.logged_in {
            self.draw_login(display);
            return;
        }
        self.ensure_wallpaper();
        display.cursor_hide();
        display.reset_clip();
        display.copy_surface(&self.wallpaper);
        if self.theme.show_grid && !self.launcher {
            draw_grid(display, self.width, self.height, &self.theme);
        }

        if self.launcher {
            self.draw_start_screen(display);
        } else {
            let theme = self.theme;
            let focused = self.focused;
            let hover = self.hover;
            for window in self.windows.iter_mut() {
                if window.minimized {
                    continue;
                }
                draw_window(display, window, &theme, focused == Some(window.id), hover);
            }
            self.draw_taskbar(display);
            self.draw_notifications(display);
        }

        let cursor = self.cursor;
        display.cursor_show(cursor.x, cursor.y);
        display.present();
    }

    fn draw_taskbar(&self, display: &mut Display) {
        let width = self.width;
        let height = self.height;
        let chrome = self.theme.chrome();
        let line = self.theme.line();
        let bar = Rect::new(0, height - DOCK_HEIGHT, width, DOCK_HEIGHT);
        display.fill_rect(bar, chrome);
        display.fill_rect(Rect::new(0, height - DOCK_HEIGHT, width, 1), line);

        let start = Rect::new(4, height - DOCK_HEIGHT + 4, 44, 44);
        let start_hot = self.hover == Target::Start || self.launcher;
        if start_hot {
            display.fill_rect(start, self.theme.accent_soft(40));
        }
        widgets::logo(display, Rect::new(start.x + 10, start.y + 10, 24, 24));

        for (index, kind) in DOCK_APPS.iter().enumerate() {
            let rect = Rect::new(52 + index as i32 * 48, height - DOCK_HEIGHT + 4, 44, 44);
            let hot = self.hover == Target::Dock(*kind);
            let open = self.windows.iter().any(|w| w.app.kind() == *kind && !w.minimized);
            let focused = self.focused_kind() == Some(*kind);
            if focused {
                display.fill_rect(rect, self.theme.accent_soft(34));
            } else if hot {
                display.fill_rect(rect, self.theme.card());
            }
            widgets::icon(
                display,
                Rect::new(rect.x + 10, rect.y + 10, 24, 24),
                kind.icon(),
                if focused {
                    self.theme.accent
                } else if hot || open {
                    self.theme.text()
                } else {
                    self.theme.text_dim()
                },
            );
            if open {
                display.fill_rect(
                    Rect::new(rect.x + 12, rect.bottom() - 2, 20, 2),
                    if focused { self.theme.accent } else { self.theme.text_dim() },
                );
            }
        }

        let power = Rect::new(width - 48, height - DOCK_HEIGHT + 4, 44, 44);
        let power_hot = self.hover == Target::Power;
        if power_hot {
            display.fill_rect(power, argb(0x40, 0xe81123));
        }
        widgets::icon(
            display,
            Rect::new(power.x + 10, power.y + 10, 24, 24),
            Icon::Close,
            if power_hot { rgb(0xe81123) } else { self.theme.text_dim() },
        );

        let now = crate::time::now();
        let clock = if self.theme.show_seconds {
            alloc::format!("{:02}:{:02}:{:02}", now.hour, now.minute, now.second)
        } else {
            alloc::format!("{:02}:{:02}", now.hour, now.minute)
        };
        display.text(
            width - 110,
            height - DOCK_HEIGHT + 18,
            &clock,
            9,
            false,
            self.theme.text_muted(),
        );
    }

    fn draw_dock(&self, _display: &mut Display) {}

    fn draw_launcher(&self, _display: &mut Display) {}

    fn draw_notifications(&self, display: &mut Display) {
        if self.notifications.is_empty() {
            return;
        }
        let width = (self.width - 40).clamp(240, 348);
        let height = 74;
        let mut bottom = self.height - DOCK_HEIGHT - 12;
        for note in self.notifications.iter().rev() {
            let rect = Rect::new(self.width - width - 18, bottom - height, width, height);
            bottom -= height + 12;
            let now = zirvstdlib::time::monotonic_ms();
            let remaining = note.life.saturating_sub(now.saturating_sub(note.born));
            let fade = if remaining < 600 { 160 } else { 246 };
            let rise = if remaining < 240 { ((240 - remaining) / 12) as i32 } else { 0 };
            let rect = Rect::new(rect.x, rect.y + rise, rect.w, rect.h);
            display.drop_shadow(rect, 0, 12, argb(0x60, 0x000000));
            display.fill_rect(rect, argb(fade, self.theme.card() & 0x00ff_ffff));
            display.stroke_rounded(rect, 0, 1, self.theme.line());
            display.fill_rect(Rect::new(rect.x + 10, rect.y + 12, 3, rect.h - 24), self.theme.accent);
            display.text(rect.x + 22, rect.y + 13, &note.title, 12, true, self.theme.text());
            display.text_ellipsized(rect.x + 22, rect.y + 33, &note.body, rect.w - 38, 11, false, self.theme.text_muted());
            let ratio = if note.life == 0 { 0.0 } else { remaining as f32 / note.life as f32 };
            display.fill_rect(Rect::new(rect.x + 22, rect.bottom() - 14, rect.w - 38, 2), self.theme.line());
            display.fill_rect(
                Rect::new(
                    rect.x + 22,
                    rect.bottom() - 14,
                    (((rect.w - 38) as f32 * ratio) as i32).max(2),
                    2,
                ),
                self.theme.accent,
            );
        }
    }

    fn draw_start_screen(&self, display: &mut Display) {
        let now = zirvstdlib::time::monotonic_ms();
        let elapsed = now.saturating_sub(self.launcher_started);
        let progress = (elapsed as f32 / 420.0).clamp(0.0, 1.0);
        let eased = progress * progress * (3.0 - 2.0 * progress);
        let screen = Rect::new(0, 0, self.width, self.height);
        let base = self.theme.start_color;
        let dark = {
            let r = ((base >> 16) & 0xff) * 58 / 100;
            let g = ((base >> 8) & 0xff) * 58 / 100;
            let b = (base & 0xff) * 64 / 100;
            rgb((r << 16) | (g << 8) | b)
        };
        display.vertical_gradient(screen, base, dark);

        // Header.
        let slide = ((1.0 - eased) * 72.0) as i32;
        widgets::logo(display, Rect::new(58 - slide, 42, 44, 44));
        display.text(120 - slide, 44, "Start", 40, true, rgb(0xffffff));

        let search = Rect::new(60, 130, 340, 34);
        let search = Rect::new(search.x - slide, search.y, search.w, search.h);
        display.fill_rect(search, argb(0x28, 0x000000));
        display.stroke_rounded(search, 0, 1, argb(0x90, 0xffffff));
        widgets::icon(display, Rect::new(search.x + 12, search.y + 10, 14, 14), Icon::Search, argb(0xc0, 0xffffff));
        display.text(search.x + 38, search.y + 10, "Search apps and settings", 10, false, argb(0xa0, 0xffffff));

        // User tile.
        let user = Rect::new(self.width - 260 - slide, 50, 200, 32);
        display.text(user.right() - widgets::text_width("liveuser@zirvium", 10, false), user.y + 8, "liveuser@zirvium", 10, false, argb(0xd0, 0xffffff));

        // Live tiles.
        let mut x = 60;
        let mut y = 190;
        let now = crate::time::now();
        for (index, kind) in START_APPS.iter().enumerate() {
            let (tw, th) = match index {
                0 => (236, 110), // wide tile
                1 => (110, 110),
                2 => (110, 110),
                _ => (110, 110),
            };
            let rect = Rect::new(x - slide, y, tw, th);
            let tint = if index % 3 == 0 { 0x46 } else if index % 3 == 1 { 0x34 } else { 0x28 };
            display.fill_rect(rect, argb(tint, 0xffffff));
            display.stroke_rounded(rect, 0, 1, argb(0x60, 0xffffff));
            if index == 0 {
                // Live tile - shows time
                display.text(rect.x + 16, rect.y + 16, &alloc::format!("{:02}:{:02}", now.hour, now.minute), 28, true, rgb(0xffffff));
                display.text(rect.x + 16, rect.y + 56, kind.title(), 12, true, rgb(0xffffff));
                display.text(rect.x + 16, rect.y + 76, "Desktop", 9, false, argb(0xb0, 0xffffff));
            } else if index == 3 {
                // Live tile - cluster info
                display.text(rect.x + 14, rect.y + 14, &alloc::format!("{}", crate::mosix::node_count()), 22, true, rgb(0xffffff));
                display.text(rect.x + 14, rect.y + 44, "Nodes", 10, false, argb(0xc0, 0xffffff));
                display.text(rect.x + 14, rect.y + 66, &alloc::format!("{} procs", crate::mosix::process_count()), 9, false, argb(0xa0, 0xffffff));
            } else if index == 4 {
                display.text(rect.x + 14, rect.y + 16, "Settings", 11, true, rgb(0xffffff));
                display.fill_rect(Rect::new(rect.x + 14, rect.y + 42, rect.w - 28, 3), argb(0x70, 0xffffff));
                display.text(rect.x + 14, rect.y + 58, "Personalize", 9, false, argb(0xb0, 0xffffff));
            } else {
                widgets::icon(
                    display,
                    Rect::new(rect.x + (tw - 38) / 2, rect.y + 22, 38, 38),
                    kind.icon(),
                    rgb(0xffffff),
                );
                display.text_centered(
                    Rect::new(rect.x, rect.bottom() - 28, rect.w, 16),
                    kind.title(),
                    9,
                    false,
                    rgb(0xffffff),
                );
            }

            display.fill_rect(Rect::new(rect.x + 10, rect.bottom() - 5, 30, 2), argb(0x80, 0xffffff));
            if tw == 236 {
                x += 252;
            } else {
                x += 126;
            }
            if x > 60 + 340 {
                x = 60;
                y += 126;
            }
        }

        // Power panel
        let power_panel = Rect::new(self.width - 210, self.height - 236, 150, 176);
        display.fill_rect(power_panel, argb(0x40, 0x000000));
        display.stroke_rounded(power_panel, 0, 1, argb(0x70, 0xffffff));
        display.text(power_panel.x + 16, power_panel.y + 14, "Power", 11, true, rgb(0xffffff));
        let shutdown = Rect::new(power_panel.x + 14, power_panel.y + 44, power_panel.w - 28, 36);
        display.fill_rect(shutdown, argb(0x50, 0xe81123));
        display.text_centered(shutdown, "Shut down", 10, false, rgb(0xffffff));
        let reboot = Rect::new(power_panel.x + 14, power_panel.y + 90, power_panel.w - 28, 36);
        display.fill_rect(reboot, argb(0x30, 0xffffff));
        display.text_centered(reboot, "Restart", 10, false, rgb(0xffffff));
        let logout = Rect::new(power_panel.x + 14, power_panel.y + 136, power_panel.w - 28, 36);
        display.fill_rect(logout, argb(0x30, 0xffffff));
        display.text_centered(logout, "Log out", 10, false, rgb(0xffffff));

        if eased < 1.0 {
            display.fill_rect(
                screen,
                argb(((1.0 - eased) * 190.0) as u32, 0x000000),
            );
        }

    }

    fn draw_login(&self, display: &mut Display) {
        let screen = Rect::new(0, 0, self.width, self.height);
        display.clear(rgb(0x030405));
        display.vertical_gradient(screen, rgb(0x05070b), rgb(0x0d1420));

        let card = Rect::new(self.width / 2 - 176, self.height / 2 - 152, 352, 330);
        display.drop_shadow(card, 0, 18, argb(0x80, 0x000000));
        display.fill_rounded(card, 12, argb(0xf0, 0x111722));
        display.stroke_rounded(card, 12, 1, argb(0xff, 0x2b3850));
        widgets::logo(display, Rect::new(self.width / 2 - 38, card.y + 28, 76, 76));
        display.text_centered(
            Rect::new(card.x, card.y + 116, card.w, 26),
            "Welcome to Zirvium",
            18,
            true,
            TEXT_STRONG,
        );
        display.text_centered(
            Rect::new(card.x, card.y + 148, card.w, 16),
            "Sign in to continue",
            10,
            false,
            TEXT_MUTED,
        );
        let user = Rect::new(card.x + 42, card.y + 184, card.w - 84, 36);
        display.fill_rect(user, argb(0xff, 0x202938));
        display.stroke_rounded(user, 5, 1, argb(0xff, 0x3a4962));
        widgets::icon(
            display,
            Rect::new(user.x + 10, user.y + 10, 16, 16),
            Icon::About,
            TEXT_MUTED,
        );
        display.text(user.x + 36, user.y + 11, "liveuser", 11, true, TEXT);
        let button = Rect::new(self.width / 2 - 92, self.height / 2 + 92, 184, 38);
        display.fill_rounded(button, 5, rgb(0x5aa2ff));
        display.text_centered(button, "Sign in", 11, true, rgb(0x07101d));
        display.text_centered(
            Rect::new(card.x, card.bottom() - 28, card.w, 14),
            "Press Enter or click Sign in",
            9,
            false,
            TEXT_DIM,
        );
    }
}

// ---------------------------------------------------------------------------
// Window painting
// ---------------------------------------------------------------------------

fn draw_window(
    display: &mut Display,
    window: &mut Window,
    theme: &Theme,
    focused: bool,
    hover: Target,
) {
    let rect = window.rect;
    if rect.w < 160 || rect.h < 100 {
        return;
    }
    display.reset_clip();
    display.fill_rect(rect, theme.surface());
    display.stroke_rounded(rect, 0, 1, if focused { theme.accent } else { theme.line() });

    let title_rect = Rect::new(rect.x, rect.y, rect.w, TITLE_HEIGHT);
    display.fill_rect(title_rect, if focused { theme.card() } else { theme.chrome() });
    display.fill_rect(Rect::new(rect.x, rect.y + TITLE_HEIGHT - 1, rect.w, 1), if focused { theme.accent } else { theme.line() });

    let kind = window.app.kind();
    widgets::icon(display, Rect::new(rect.x + 6, rect.y + 7, 16, 16), kind.icon(), if focused { theme.accent } else { theme.text_dim() });
    display.text_ellipsized(rect.x + 28, rect.y + 7, kind.title(), (rect.w - 150).max(30), 11, false, if focused { theme.text() } else { theme.text_muted() });

    let id = window.id;
    let hot_close = hover == Target::Close(id);
    let hot_min = hover == Target::Minimize(id);
    let hot_max = hover == Target::Maximize(id);
    let close = Rect::new(rect.right() - 28, rect.y + 6, 22, 18);
    let min = Rect::new(rect.right() - 54, rect.y + 6, 22, 18);
    let max = Rect::new(rect.right() - 80, rect.y + 6, 22, 18);
    display.fill_rect(min, if hot_min { theme.well() } else { theme.card() });
    display.stroke_rounded(min, 0, 1, if hot_min { theme.accent } else { theme.line() });
    display.text_centered(min, "–", 12, false, if hot_min { theme.text() } else { theme.text_muted() });
    display.fill_rect(max, if hot_max { theme.well() } else { theme.card() });
    display.stroke_rounded(max, 0, 1, if hot_max { theme.accent } else { theme.line() });
    display.text_centered(max, "□", 10, false, if hot_max { theme.text() } else { theme.text_muted() });
    display.fill_rect(close, if hot_close { rgb(0xe81123) } else { theme.card() });
    display.stroke_rounded(close, 0, 1, if hot_close { rgb(0xff3344) } else { theme.line() });
    display.text_centered(close, "×", 14, false, if hot_close { rgb(0xffffff) } else { theme.text_muted() });

    let content = window.content();
    if !content.is_empty() {
        display.push_clip(content);
        window.app.draw(display, content, theme, focused);
        display.pop_clip();
        display.reset_clip();
    }
    display.stroke_rounded(rect, 0, 1, if focused { theme.accent } else { theme.line() });
}

// ---------------------------------------------------------------------------
// Boot splash and event loop
// ---------------------------------------------------------------------------

fn draw_splash_frame(display: &mut Display, frame: usize) {
    WIDTH_HINT.store(display.width.max(1) as u32, Ordering::Relaxed);
    HEIGHT_HINT.store(display.height.max(1) as u32, Ordering::Relaxed);
    let screen = display.screen();
    let (width, height) = (screen.w, screen.h);
    display.reset_clip();
    display.clear(rgb(0x000000));

    let center_x = width / 2;
    let center_y = height / 2;

    let cx = center_x;
    let cy = center_y - 58;
    widgets::logo(display, Rect::new(cx - 34, cy - 34, 68, 68));

    widgets::text_shadow(
        display,
        cx - widgets::text_width("Zirvium", 28, true) / 2,
        cy + 42,
        "Zirvium",
        28,
        true,
        TEXT_STRONG,
    );
    display.text_centered(
        Rect::new(0, cy + 78, width, 14),
        "Loading desktop",
        10,
        false,
        TEXT_MUTED,
    );

    let dots_y = cy + 116;
    for index in 0..8 {
        let distance = (index as isize - (frame % 8) as isize).unsigned_abs();
        let alpha = match distance {
            0 => 255,
            1 => 190,
            2 => 130,
            _ => 70,
        };
        display.fill_circle(
            Point::new(cx - 28 + index as i32 * 8, dots_y),
            if distance == 0 { 3 } else { 2 },
            argb(alpha, 0xffffff),
        );
    }

    display.present_all();
}

/// Paint a black boot splash with a custom Zirvium loading animation.
pub fn splash(display: &mut Display) {
    crate::arch::cpu::sti();
    for frame in 0..70 {
        draw_splash_frame(display, frame);
        crate::arch::pit::spin_wait_ms(100);
    }
}

static FRAMES: core::sync::atomic::AtomicU64 = core::sync::atomic::AtomicU64::new(0);
static FRAME_MS: core::sync::atomic::AtomicU64 = core::sync::atomic::AtomicU64::new(0);

/// Run the desktop.  This never returns: the kernel has no idle task yet.
pub fn run(display: &mut Display) -> ! {
    WIDTH_HINT.store(display.width.max(1) as u32, Ordering::Relaxed);
    HEIGHT_HINT.store(display.height.max(1) as u32, Ordering::Relaxed);
    crate::drivers::ps2::set_screen(display.width.max(1), display.height.max(1));

    crate::sprintln!(
        "UI: desktop starting at {}x{}",
        display.width,
        display.height
    );
    let mut desktop = Desktop::new(display);
    desktop.draw(display);

    let mut events = [InputEvent::key(InputKind::KeyDown, key::NONE, 0); 64];
    loop {
        let mut redraw = false;
        let count = crate::input::drain_into(&mut events);
        for event in &events[..count] {
            if desktop.handle(event) {
                redraw = true;
            }
        }
        if desktop.tick() {
            redraw = true;
        }

        if redraw {
            desktop.draw(display);
        } else {
            display.cursor_move(desktop.cursor.x, desktop.cursor.y);
            display.present();
        }

        crate::arch::cpu::sti();
        crate::arch::cpu::halt();
    }
}
pub mod logo_data;
