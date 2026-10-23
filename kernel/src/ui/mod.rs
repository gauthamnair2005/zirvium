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

use zirvstdlib::abi::{buttons, key, modifiers, InputEvent, InputKind};
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
    About,
}

/// Dock order, also the launcher order.
const DOCK_APPS: [AppKind; 6] = [
    AppKind::Terminal,
    AppKind::Files,
    AppKind::Monitor,
    AppKind::Nodes,
    AppKind::Settings,
    AppKind::About,
];

impl AppKind {
    pub fn title(self) -> &'static str {
        match self {
            AppKind::Terminal => "Terminal",
            AppKind::Files => "Files",
            AppKind::Monitor => "System Monitor",
            AppKind::Nodes => "MOSIX Nodes",
            AppKind::Settings => "Settings",
            AppKind::About => "About Zirvium",
        }
    }

    pub fn subtitle(self) -> &'static str {
        match self {
            AppKind::Terminal => "MOSIX shell and diagnostics",
            AppKind::Files => "Browse the virtual filesystem",
            AppKind::Monitor => "CPU, memory and process telemetry",
            AppKind::Nodes => "Cluster members and migration",
            AppKind::Settings => "Wallpaper, accent and display",
            AppKind::About => "The design behind Zirvium",
        }
    }

    pub fn icon(self) -> Icon {
        match self {
            AppKind::Terminal => Icon::Terminal,
            AppKind::Files => Icon::Files,
            AppKind::Monitor => Icon::Monitor,
            AppKind::Nodes => Icon::Nodes,
            AppKind::Settings => Icon::Settings,
            AppKind::About => Icon::About,
        }
    }

    fn default_size(self) -> (i32, i32) {
        match self {
            AppKind::Terminal => (780, 470),
            AppKind::Files => (830, 520),
            AppKind::Monitor => (760, 540),
            AppKind::Nodes => (820, 590),
            AppKind::Settings => (700, 600),
            AppKind::About => (600, 470),
        }
    }
}

enum App {
    Terminal(terminal::Terminal),
    Files(apps::Files),
    Monitor(apps::Monitor),
    Nodes(apps::Nodes),
    Settings(apps::Settings),
    About(apps::About),
}

impl App {
    fn new(kind: AppKind) -> Self {
        match kind {
            AppKind::Terminal => App::Terminal(terminal::Terminal::new()),
            AppKind::Files => App::Files(apps::Files::new()),
            AppKind::Monitor => App::Monitor(apps::Monitor::new()),
            AppKind::Nodes => App::Nodes(apps::Nodes::new()),
            AppKind::Settings => App::Settings(apps::Settings::new()),
            AppKind::About => App::About(apps::About::new()),
        }
    }

    fn kind(&self) -> AppKind {
        match self {
            App::Terminal(_) => AppKind::Terminal,
            App::Files(_) => AppKind::Files,
            App::Monitor(_) => AppKind::Monitor,
            App::Nodes(_) => AppKind::Nodes,
            App::Settings(_) => AppKind::Settings,
            App::About(_) => AppKind::About,
        }
    }

    fn draw(&mut self, display: &mut Display, content: Rect, theme: &Theme, focused: bool) {
        match self {
            App::Terminal(app) => app.draw(display, content, theme, focused),
            App::Files(app) => app.draw(display, content, theme, focused),
            App::Monitor(app) => app.draw(display, content, theme, focused),
            App::Nodes(app) => app.draw(display, content, theme, focused),
            App::Settings(app) => app.draw(display, content, theme, focused),
            App::About(app) => app.draw(display, content, theme, focused),
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
            App::About(_) => Response::NONE,
        }
    }

    fn key(&mut self, code: u32, event_modifiers: u16) -> bool {
        match self {
            App::Terminal(app) => app.key(code, event_modifiers),
            App::Files(app) => app.key(code, event_modifiers),
            App::Monitor(_) => false,
            App::Nodes(_) => false,
            App::Settings(_) => false,
            App::About(_) => false,
        }
    }

    fn scroll(&mut self, point: Point, content: Rect, amount: i32) {
        match self {
            App::Terminal(app) => app.scroll(amount),
            App::Files(app) => app.scroll(point, content, amount),
            App::Monitor(_) => {}
            App::Nodes(_) => {}
            App::Settings(app) => app.scroll(point, content, amount),
            App::About(_) => {}
        }
    }

    fn tick(&mut self) -> bool {
        match self {
            App::Monitor(app) => app.tick(),
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
    Launcher,
    Clock,
    Dock(AppKind),
    Close(u32),
    Minimize(u32),
    Maximize(u32),
    Title(u32),
    Content(u32),
    Resize(u32),
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

const DOCK_GAP: i32 = 12;

fn launcher_button_rect() -> Rect {
    Rect::new(9, 5, 108, 24)
}

fn clock_rect(width: i32) -> Rect {
    Rect::new((width - 196).max(0), 0, 192, TOP_BAR_HEIGHT)
}

fn dock_panel(width: i32, height: i32) -> Rect {
    let count = DOCK_APPS.len() as i32;
    let panel_width = count * DOCK_ICON + (count - 1) * DOCK_GAP + 28;
    Rect::new(
        (width - panel_width) / 2,
        height - DOCK_HEIGHT + 6,
        panel_width,
        62,
    )
}

fn dock_icon_rect(panel: Rect, index: usize) -> Rect {
    Rect::new(
        panel.x + 14 + index as i32 * (DOCK_ICON + DOCK_GAP),
        panel.y + 10,
        DOCK_ICON,
        DOCK_ICON,
    )
}

fn launch_panel_rect() -> Rect {
    let rows = DOCK_APPS.len() as i32;
    Rect::new(14, TOP_BAR_HEIGHT + 8, 264, 42 + rows * 44 + 28)
}

fn launch_row_rect(index: usize) -> Rect {
    let panel = launch_panel_rect();
    Rect::new(
        panel.x + 8,
        panel.y + 38 + index as i32 * 44,
        panel.w - 16,
        40,
    )
}

fn title_buttons(rect: Rect) -> (Rect, Rect, Rect) {
    let y = rect.y + TITLE_HEIGHT / 2 - 7;
    let right = rect.right();
    (
        Rect::new(right - 28, y, 14, 14),
        Rect::new(right - 48, y, 14, 14),
        Rect::new(right - 68, y, 14, 14),
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
        };
        desktop.notify(
            "Welcome to Zirvium 2.0",
            "Super or the top-left button opens the launcher.",
            9000,
        );
        desktop.open(AppKind::Terminal);
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
            if let Some(restore) = window.restore.take() {
                window.rect = restore;
            } else {
                window.restore = Some(window.rect);
                window.rect = Rect::new(
                    0,
                    TOP_BAR_HEIGHT,
                    width,
                    (height - TOP_BAR_HEIGHT).max(160),
                );
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
        if self.launcher {
            let panel = launch_panel_rect();
            if panel.contains(point) {
                for index in 0..DOCK_APPS.len() {
                    if launch_row_rect(index).contains(point) {
                        return Target::Dock(DOCK_APPS[index]);
                    }
                }
                return Target::Nothing;
            }
        }

        let dock = dock_panel(self.width, self.height);
        if dock.contains(point) {
            for (index, kind) in DOCK_APPS.iter().enumerate() {
                if dock_icon_rect(dock, index).contains(point) {
                    return Target::Dock(*kind);
                }
            }
            return Target::Nothing;
        }

        if point.y < TOP_BAR_HEIGHT {
            if launcher_button_rect().contains(point) {
                return Target::Launcher;
            }
            if clock_rect(self.width).contains(point) {
                return Target::Clock;
            }
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
            if window.restore.is_none()
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
        Target::Nothing
    }

    fn pointer_down(&mut self, point: Point) -> bool {
        if self.launcher {
            if let Some(index) = (0..DOCK_APPS.len())
                .find(|index| launch_row_rect(*index).contains(point))
            {
                let kind = DOCK_APPS[index];
                self.launcher = false;
                self.open(kind);
                return true;
            }
            if launch_panel_rect().contains(point) {
                return false;
            }
            self.launcher = false;
            return true;
        }

        let target = self.target_at(point);
        let mut redraw = true;
        match target {
            Target::Launcher => {
                self.launcher = !self.launcher;
                if self.launcher {
                    self.launcher_selected = 0;
                }
            }
            Target::Clock => self.open(AppKind::Settings),
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
                        if let Some(window) = self.windows.iter_mut().find(|window| window.id == id) {
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
                                let new_x =
                                    point.x - (restore.w as f32 * scale_x) as i32;
                                let new_y = point.y - (restore.h as f32 * scale_y) as i32;
                                window.rect.x = new_x.clamp(16 - restore.w, screen_width - 64);
                                window.rect.y = new_y.clamp(TOP_BAR_HEIGHT, screen_height - 40);
                            }
                            let offset = Point::new(
                                point.x - window.rect.x,
                                point.y - window.rect.y,
                            );
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
                        let new_w = (rect.w + point.x - origin.x)
                            .clamp(360, width - rect.x);
                        let new_h = (rect.h + point.y - origin.y)
                            .clamp(260, height - rect.y - 8);
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
                if let Target::Content(id) = target {
                    if let Some(content) = self.window(id).map(|window| window.content()) {
                        if let Some(window) = self.window_mut(id) {
                            window.app.scroll(point, content, event.value);
                        }
                        return true;
                    }
                }
                false
            }
            InputKind::KeyDown => self.handle_key(event.code, event.modifiers),
            _ => false,
        }
    }

    fn handle_key(&mut self, code: u32, event_modifiers: u16) -> bool {
        if code == key::SUPER {
            self.launcher = !self.launcher;
            self.launcher_selected = 0;
            return true;
        }
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
                key::UP => {
                    if self.launcher_selected > 0 {
                        self.launcher_selected -= 1;
                    }
                    return true;
                }
                key::DOWN => {
                    if self.launcher_selected + 1 < DOCK_APPS.len() {
                        self.launcher_selected += 1;
                    }
                    return true;
                }
                key::ENTER => {
                    let kind = DOCK_APPS[self.launcher_selected.min(DOCK_APPS.len() - 1)];
                    self.launcher = false;
                    self.open(kind);
                    return true;
                }
                _ => return false,
            }
        }
        let Some(id) = self.focused else {
            return false;
        };
        let changed = self
            .window_mut(id)
            .map(|window| window.app.key(code, event_modifiers))
            .unwrap_or(false);
        changed
    }

    // -- Timers -------------------------------------------------------------

    fn tick(&mut self) -> bool {
        let now_ms = zirvstdlib::time::monotonic_ms();
        let second = now_ms / 1000;
        let mut redraw = false;

        if second != self.last_second {
            self.last_second = second;
            crate::mosix::tick();
            let noise = second.wrapping_mul(2_654_435_761) >> 9;
            let drag_boost = if self.drag.is_some() { 180 } else { 0 };
            let load = (140 + (noise % 300) as u32 + drag_boost).min(920);
            crate::mosix::set_load(load);
            crate::mosix::account(4, 1);
            crate::mosix::account(6, 1);
            if self.theme.show_seconds || second % 60 == 0 {
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
        self.ensure_wallpaper();
        display.cursor_hide();
        display.reset_clip();
        display.copy_surface(&self.wallpaper);
        if self.theme.show_grid {
            draw_grid(display, self.width, self.height, &self.theme);
        }

        let theme = self.theme;
        let focused = self.focused;
        let hover = self.hover;
        for window in self.windows.iter_mut() {
            if window.minimized {
                continue;
            }
            draw_window(display, window, &theme, focused == Some(window.id), hover);
        }

        self.draw_dock(display);
        self.draw_top_bar(display);
        if self.launcher {
            self.draw_launcher(display);
        }
        self.draw_notifications(display);

        let cursor = self.cursor;
        display.cursor_show(cursor.x, cursor.y);
        display.present();
    }

    fn draw_top_bar(&self, display: &mut Display) {
        let width = self.width;
        display.fill_rect(Rect::new(0, 0, width, TOP_BAR_HEIGHT), TOP_BAR);
        display.fill_rect(
            Rect::new(0, TOP_BAR_HEIGHT - 1, width, 1),
            TOP_BAR_BORDER,
        );

        let button = launcher_button_rect();
        let hot = self.hover == Target::Launcher || self.launcher;
        if hot {
            display.fill_rounded(button, 7, self.theme.accent_soft(46));
        }
        widgets::icon(
            display,
            Rect::new(button.x + 7, button.y + 4, 16, 16),
            Icon::Logo,
            if hot { self.theme.accent } else { TEXT_MUTED },
        );
        display.text(
            button.x + 30,
            button.y + 4,
            "Zirvium",
            12,
            true,
            if hot { TEXT } else { TEXT_MUTED },
        );

        let title = self
            .focused
            .and_then(|id| self.window(id))
            .map(|window| window.app.kind().title())
            .unwrap_or("Desktop");
        let center = Rect::new(140, 0, (width - 420).max(80), TOP_BAR_HEIGHT);
        display.text_centered(center, title, 11, false, TEXT_DIM);

        // Right side: date, clock and cluster status.
        let now = crate::time::now();
        let clock = if self.theme.show_seconds {
            alloc::format!("{:02}:{:02}:{:02}", now.hour, now.minute, now.second)
        } else {
            alloc::format!("{:02}:{:02}", now.hour, now.minute)
        };
        let clock_width = widgets::text_width(&clock, 12, true);
        display.text(
            width - 16 - clock_width,
            9,
            &clock,
            12,
            true,
            TEXT,
        );
        let date = alloc::format!("{} {}", now.month_name(), now.day);
        let date_width = widgets::text_width(&date, 11, false);
        display.text(
            width - 30 - clock_width - date_width,
            10,
            &date,
            11,
            false,
            TEXT_DIM,
        );
        // Cluster indicator: an accent dot plus the process count.
        let processes = crate::mosix::process_count();
        let status = alloc::format!("{processes} proc");
        let status_width = widgets::text_width(&status, 10, true);
        let status_x = width - 44 - clock_width - date_width - status_width;
        display.fill_circle(
            Point::new(status_x - 8, TOP_BAR_HEIGHT / 2),
            3,
            self.theme.accent,
        );
        display.text(
            status_x,
            11,
            &status,
            10,
            true,
            TEXT_MUTED,
        );
    }

    fn draw_dock(&self, display: &mut Display) {
        let panel = dock_panel(self.width, self.height);
        display.drop_shadow(panel, 16, 12, argb(0x88, 0x000000));
        widgets::panel(
            display,
            panel,
            16,
            argb(0xf4, 0x12141b),
            TOP_BAR_BORDER,
        );
        for (index, kind) in DOCK_APPS.iter().enumerate() {
            let base = dock_icon_rect(panel, index);
            let hot = !self.launcher && self.hover == Target::Dock(*kind);
            let open = self.windows.iter().any(|window| window.app.kind() == *kind);
            let focused = self.focused_kind() == Some(*kind);
            let rect = if hot && self.theme.dock_magnification {
                Rect::new(base.x - 3, base.y - 7, base.w + 6, base.h + 6)
            } else {
                base
            };
            display.fill_rounded(
                rect,
                11,
                if hot {
                    self.theme.accent_soft(58)
                } else if open {
                    argb(0xff, 0x1c2029)
                } else {
                    argb(0x90, 0x171a22)
                },
            );
            display.stroke_rounded(
                rect,
                11,
                1,
                if hot {
                    self.theme.accent_soft(160)
                } else {
                    BORDER_SOFT
                },
            );
            widgets::icon(
                display,
                rect.inset(if hot { 11 } else { 10 }),
                kind.icon(),
                if focused {
                    self.theme.accent
                } else if open {
                    TEXT
                } else {
                    TEXT_MUTED
                },
            );
            if open {
                display.fill_circle(
                    Point::new(rect.center().x, panel.bottom() - 5),
                    2,
                    if focused {
                        self.theme.accent
                    } else {
                        TEXT_FAINT
                    },
                );
            }
        }
    }

    fn draw_launcher(&self, display: &mut Display) {
        let panel = launch_panel_rect();
        display.drop_shadow(panel, 14, 16, argb(0x99, 0x000000));
        widgets::panel(
            display,
            panel,
            14,
            argb(0xf8, 0x12141b),
            TOP_BAR_BORDER,
        );
        display.text(panel.x + 16, panel.y + 12, "Applications", 12, true, TEXT);
        display.text(
            panel.right() - 16 - widgets::text_width("Super", 10, false),
            panel.y + 14,
            "Super",
            10,
            false,
            TEXT_DIM,
        );
        for (index, kind) in DOCK_APPS.iter().enumerate() {
            let row = launch_row_rect(index);
            let selected = index == self.launcher_selected;
            let hot = self.hover == Target::Dock(*kind);
            if selected || hot {
                display.fill_rounded(
                    row,
                    9,
                    if selected {
                        self.theme.accent_soft(46)
                    } else {
                        argb(0x30, 0xffffff)
                    },
                );
                display.stroke_rounded(row, 9, 1, self.theme.accent_soft(80));
            }
            let badge = Rect::new(row.x + 8, row.y + 6, 28, 28);
            display.fill_rounded(
                badge,
                7,
                if selected {
                    self.theme.accent_soft(70)
                } else {
                    argb(0x30, 0xffffff)
                },
            );
            widgets::icon(
                display,
                badge.inset(6),
                kind.icon(),
                if selected { self.theme.accent } else { TEXT_MUTED },
            );
            display.text(row.x + 46, row.y + 6, kind.title(), 12, true, TEXT);
            display.text_ellipsized(
                row.x + 46,
                row.y + 23,
                kind.subtitle(),
                row.w - 54,
                10,
                false,
                TEXT_DIM,
            );
            if selected {
                widgets::icon(
                    display,
                    Rect::new(row.right() - 22, row.y + 13, 12, 12),
                    Icon::Right,
                    self.theme.accent,
                );
            }
        }
        display.text_centered(
            Rect::new(panel.x, panel.bottom() - 24, panel.w, 18),
            "Enter opens · Esc closes",
            10,
            false,
            TEXT_FAINT,
        );
    }

    fn draw_notifications(&self, display: &mut Display) {
        if self.notifications.is_empty() {
            return;
        }
        let width = (self.width - 40).clamp(200, 330);
        let height = 62;
        let mut bottom = self.height - DOCK_HEIGHT - 10;
        for note in self.notifications.iter().rev() {
            let rect = Rect::new(self.width - width - 16, bottom - height, width, height);
            bottom -= height + 10;
            let now = zirvstdlib::time::monotonic_ms();
            let remaining = note.life.saturating_sub(now.saturating_sub(note.born));
            let fade = if remaining < 600 { 150 } else { 250 };
            display.drop_shadow(rect, 12, 10, argb(0x88, 0x000000));
            widgets::panel(display, rect, 12, argb(fade, 0x161922), TOP_BAR_BORDER);
            display.fill_rounded(
                Rect::new(rect.x, rect.y + 10, 3, rect.h - 20),
                2,
                self.theme.accent,
            );
            display.text(rect.x + 16, rect.y + 11, &note.title, 12, true, TEXT);
            display.text_ellipsized(
                rect.x + 16,
                rect.y + 31,
                &note.body,
                rect.w - 30,
                11,
                false,
                TEXT_MUTED,
            );
            // A small progress hairline shows how long the toast will stay.
            let ratio = if note.life == 0 {
                0.0
            } else {
                remaining as f32 / note.life as f32
            };
            display.fill_rect(
                Rect::new(rect.x + 16, rect.bottom() - 10, rect.w - 32, 2),
                argb(0x30, 0xffffff),
            );
            display.fill_rect(
                Rect::new(
                    rect.x + 16,
                    rect.bottom() - 10,
                    ((rect.w - 32) as f32 * ratio) as i32,
                    2,
                ),
                self.theme.accent_soft(190),
            );
        }
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
    let radius = WINDOW_RADIUS.min(rect.w / 2).min(rect.h / 2);
    display.reset_clip();
    display.drop_shadow(rect, radius, 12, argb(0x78, 0x000000));
    display.fill_rounded(rect, radius, WINDOW_BG);

    let title_color = if focused {
        argb(0xff, 0x202634)
    } else {
        TITLE_BG
    };
    display.fill_rounded(
        Rect::new(rect.x, rect.y, rect.w, TITLE_HEIGHT),
        radius,
        title_color,
    );
    display.fill_rect(
        Rect::new(
            rect.x,
            rect.y + TITLE_HEIGHT - radius,
            rect.w,
            radius + 1,
        ),
        title_color,
    );
    display.horizontal_gradient(
        Rect::new(
            rect.x + radius,
            rect.y + 1,
            (rect.w - radius * 2).max(1),
            1,
        ),
        theme.accent_soft(if focused { 110 } else { 30 }),
        theme.accent_alt_soft(if focused { 40 } else { 12 }),
    );
    display.fill_rect(
        Rect::new(rect.x, rect.y + TITLE_HEIGHT - 1, rect.w, 1),
        BORDER_SOFT,
    );

    let kind = window.app.kind();
    widgets::icon(
        display,
        Rect::new(rect.x + 13, rect.y + 9, 18, 18),
        kind.icon(),
        if focused { theme.accent } else { TEXT_DIM },
    );
    display.text_ellipsized(
        rect.x + 40,
        rect.y + 10,
        kind.title(),
        (rect.w - 150).max(30),
        13,
        true,
        if focused { TEXT } else { TEXT_MUTED },
    );

    let (close, minimize, maximize) = title_buttons(rect);
    let id = window.id;
    let buttons = [
        (close, Icon::Close, CLOSE, Target::Close(id)),
        (minimize, Icon::Minimize, MINIMIZE, Target::Minimize(id)),
        (maximize, Icon::Maximize, MAXIMIZE, Target::Maximize(id)),
    ];
    for (hit, glyph, color, target) in buttons {
        let hot = hover == target;
        let center = hit.center();
        display.fill_circle(center, 6, if hot { color } else { argb(0x55, 0xffffff) });
        if hot {
            widgets::icon(display, hit.inset(3), glyph, rgb(0x140609));
        }
    }

    let content = window.content();
    if !content.is_empty() {
        display.push_clip(content);
        window.app.draw(display, content, theme, focused);
        display.pop_clip();
        display.reset_clip();
    }

    display.stroke_rounded(
        rect,
        radius,
        1,
        if focused {
            theme.accent_soft(160)
        } else {
            BORDER
        },
    );

    if hover == Target::Resize(id) {
        for step in 0..3 {
            let offset = 4 + step * 4;
            widgets::line(
                display,
                rect.right() - offset - 6,
                rect.bottom() - 4,
                rect.right() - 4,
                rect.bottom() - offset - 6,
                1,
                theme.accent_soft(150),
            );
        }
    }
}

// ---------------------------------------------------------------------------
// Boot splash and event loop
// ---------------------------------------------------------------------------

/// Paint a boot splash before the desktop takes over.
pub fn splash(display: &mut Display) {
    WIDTH_HINT.store(display.width.max(1) as u32, Ordering::Relaxed);
    HEIGHT_HINT.store(display.height.max(1) as u32, Ordering::Relaxed);
    let theme = Theme::default();
    let screen = display.screen();
    let (width, height) = (screen.w, screen.h);
    display.reset_clip();
    display.vertical_gradient(screen, rgb(0x06080f), rgb(0x101b2e));

    let center_x = width / 2;
    let center_y = height / 2;
    let mark = Rect::new(center_x - 44, center_y - 132, 88, 88);
    display.drop_shadow(mark, 22, 18, argb(0xa0, 0x000000));
    display.fill_rounded(mark, 22, theme.accent_soft(44));
    display.stroke_rounded(mark, 22, 1, theme.accent_soft(150));
    widgets::icon(display, mark.inset(22), Icon::Logo, theme.accent);

    display.text_centered(
        Rect::new(0, center_y - 28, width, 48),
        "Zirvium",
        40,
        true,
        TEXT,
    );
    display.text_centered(
        Rect::new(0, center_y + 24, width, 20),
        "one cluster, one display",
        13,
        false,
        theme.accent,
    );
    display.text_centered(
        Rect::new(0, center_y + 52, width, 18),
        "2.0 · MOSIX + DisplayJet · 64-bit Rust and assembly",
        10,
        false,
        TEXT_DIM,
    );

    let bar = Rect::new(center_x - 110, center_y + 104, 220, 3);
    display.fill_rounded(bar, 1, argb(0x40, 0xffffff));
    display.fill_rounded(
        Rect::new(bar.x, bar.y, 150, 3),
        1,
        theme.accent_soft(220),
    );
    display.text_centered(
        Rect::new(0, center_y + 118, width, 16),
        "initialising DisplayJet scanout",
        9,
        false,
        TEXT_FAINT,
    );

    display.present_all();
}

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
