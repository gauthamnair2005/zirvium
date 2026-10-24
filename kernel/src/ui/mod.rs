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
    About,
    Calculator,
    Notes,
    Network,
}

/// Dock order, also the launcher order.
const DOCK_APPS: [AppKind; 9] = [
    AppKind::Terminal,
    AppKind::Files,
    AppKind::Monitor,
    AppKind::Nodes,
    AppKind::Network,
    AppKind::Calculator,
    AppKind::Notes,
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
            AppKind::Settings => "Wallpaper, accent and display",
            AppKind::About => "The design behind Zirvium",
            AppKind::Calculator => "Quick arithmetic",
            AppKind::Notes => "Text notes and scratchpad",
            AppKind::Network => "Connections and diagnostics",
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
            AppKind::Calculator => Icon::Settings,
            AppKind::Notes => Icon::Files,
            AppKind::Network => Icon::Nodes,
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
            AppKind::Calculator => (320, 380),
            AppKind::Notes => (640, 460),
            AppKind::Network => (680, 520),
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
            AppKind::About => App::About(apps::About::new()),
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
            App::About(_) => AppKind::About,
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
            App::Settings(app) => app.draw(display, content, theme, focused),
            App::About(app) => app.draw(display, content, theme, focused),
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
            App::About(_) => Response::NONE,
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
            App::About(_) => false,
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
            App::About(_) => {}
            App::Calculator(_) => {}
            App::Notes(app) => {
                let _ = (point, content);
                app.scroll = (app.scroll - amount * 24).max(0);
            }
            App::Network(_) => {}
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
                window.rect =
                    Rect::new(0, TOP_BAR_HEIGHT, width, (height - TOP_BAR_HEIGHT).max(160));
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
                for (index, app) in DOCK_APPS.iter().enumerate() {
                    if launch_row_rect(index).contains(point) {
                        return Target::Dock(*app);
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
            if let Some(index) =
                (0..DOCK_APPS.len()).find(|index| launch_row_rect(*index).contains(point))
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
        self.window_mut(id)
            .map(|window| window.app.key(code, event_modifiers))
            .unwrap_or(false)
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
        let bar = Rect::new(0, 0, width, TOP_BAR_HEIGHT);
        display.fill_rounded_vertical(bar, 0, argb(0xf4, 0x0d1018), argb(0xe2, 0x090c12));
        display.fill_rect(Rect::new(0, TOP_BAR_HEIGHT - 1, width, 1), TOP_BAR_BORDER);
        // The menubar's only saturated element is the launcher chip, so the
        // eye always knows where to start.
        let button = launcher_button_rect();
        let hot = self.hover == Target::Launcher || self.launcher;
        display.fill_rounded_vertical(
            button,
            8,
            if hot {
                self.theme.accent_soft(66)
            } else {
                argb(0x22, 0xffffff)
            },
            if hot {
                self.theme.accent_soft(30)
            } else {
                argb(0x10, 0xffffff)
            },
        );
        display.stroke_rounded(
            button,
            8,
            1,
            if hot {
                self.theme.accent_soft(150)
            } else {
                argb(0x28, 0xffffff)
            },
        );
        widgets::icon(
            display,
            Rect::new(button.x + 8, button.y + 4, 16, 16),
            Icon::Logo,
            if hot { self.theme.accent } else { TEXT_MUTED },
        );
        display.text(
            button.x + 32,
            button.y + 4,
            "Zirvium",
            12,
            true,
            if hot { TEXT_STRONG } else { TEXT_MUTED },
        );

        let title = self
            .focused
            .and_then(|id| self.window(id))
            .map(|window| window.app.kind().title())
            .unwrap_or("Desktop");
        let center = Rect::new(148, 0, (width - 440).max(80), TOP_BAR_HEIGHT);
        display.text_centered(center, title, 11, false, TEXT_DIM);

        // Right cluster: cluster status, date and time, separated by hairlines.
        let now = crate::time::now();
        let clock = if self.theme.show_seconds {
            alloc::format!("{:02}:{:02}:{:02}", now.hour, now.minute, now.second)
        } else {
            alloc::format!("{:02}:{:02}", now.hour, now.minute)
        };
        let clock_width = widgets::text_width(&clock, 13, true);
        let clock_x = width - 16 - clock_width;
        display.text(clock_x, 8, &clock, 13, true, TEXT);

        let date = alloc::format!("{} {}", now.month_name(), now.day);
        let date_width = widgets::text_width(&date, 11, false);
        let date_x = clock_x - 14 - date_width;
        display.text(date_x, 10, &date, 11, false, TEXT_DIM);
        display.fill_rect(
            Rect::new(date_x - 12, 12, 1, TOP_BAR_HEIGHT - 24),
            argb(0x40, 0xffffff),
        );

        let processes = crate::mosix::process_count();
        let status = alloc::format!("{processes} proc");
        let status_width = widgets::text_width(&status, 10, true);
        let status_x = date_x - 24 - status_width;
        display.fill_circle(
            Point::new(status_x - 9, TOP_BAR_HEIGHT / 2),
            6,
            self.theme.accent_soft(36),
        );
        display.fill_circle(
            Point::new(status_x - 9, TOP_BAR_HEIGHT / 2),
            3,
            self.theme.accent,
        );
        display.text(status_x, 11, &status, 10, true, TEXT_MUTED);
    }

    fn draw_dock(&self, display: &mut Display) {
        let panel = dock_panel(self.width, self.height);
        display.radial_glow(
            Point::new(panel.center().x, panel.center().y),
            panel.w / 2 + 26,
            self.theme.accent_soft(60),
            0.55,
        );
        display.drop_shadow(panel, 20, 18, argb(0xb0, 0x000000));
        display.fill_rounded_vertical(panel, 20, argb(0xf2, 0x141925), argb(0xd8, 0x0a0d14));
        display.stroke_rounded(panel, 20, 1, argb(0xff, 0x2a3242));
        display.fill_rounded(
            Rect::new(panel.x + 18, panel.y + 1, panel.w - 36, 1),
            1,
            argb(0x24, 0xffffff),
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
            if hot {
                display.fill_rounded(rect, 13, self.theme.accent_soft(26));
            }
            display.fill_rounded_vertical(
                rect,
                13,
                if open {
                    argb(0xff, 0x232a39)
                } else {
                    argb(0xd0, 0x171c27)
                },
                if open {
                    argb(0xff, 0x161b25)
                } else {
                    argb(0xb0, 0x10141d)
                },
            );
            display.stroke_rounded(
                rect,
                13,
                1,
                if hot {
                    self.theme.accent_soft(190)
                } else if open {
                    argb(0xff, 0x354052)
                } else {
                    BORDER_SOFT
                },
            );
            if hot {
                // A soft inner glow gives the magnified tile a pressed-glass
                // feel without a second border.
                display.stroke_rounded(rect.inset(1), 12, 1, self.theme.accent_soft(60));
            }
            widgets::icon(
                display,
                rect.inset(if hot { 12 } else { 11 }),
                kind.icon(),
                if focused {
                    self.theme.accent
                } else if hot {
                    TEXT_STRONG
                } else if open {
                    TEXT
                } else {
                    TEXT_MUTED
                },
            );
            if open {
                let dot = Point::new(rect.center().x, panel.bottom() - 7);
                display.fill_circle(dot, 4, self.theme.accent_soft(34));
                display.fill_circle(
                    dot,
                    2,
                    if focused {
                        self.theme.accent
                    } else {
                        TEXT_FAINT
                    },
                );
            }
        }
        // Tooltip: the dock is icon-only, so hovering names the target.
        if !self.launcher
            && let Target::Dock(kind) = self.hover
            && let Some(index) = DOCK_APPS.iter().position(|candidate| *candidate == kind)
        {
            let base = dock_icon_rect(panel, index);
            let face = crate::displayjet::font::face(11, true);
            let label = kind.title();
            let width = face.width(label) + 20;
            let rect = Rect::new(
                (base.center().x - width / 2).clamp(8, self.width - width - 8),
                panel.y - 30,
                width,
                22,
            );
            display.drop_shadow(rect, 7, 6, argb(0x80, 0x000000));
            widgets::panel(display, rect, 7, argb(0xf8, 0x151a24), BORDER);
            display.text_centered(rect, label, 11, true, TEXT);
        }
    }

    fn draw_launcher(&self, display: &mut Display) {
        let panel = launch_panel_rect();
        display.radial_glow(
            Point::new(panel.x + 70, panel.y + 20),
            180,
            self.theme.accent_soft(50),
            0.5,
        );
        display.drop_shadow(panel, 18, 22, argb(0xb0, 0x000000));
        display.fill_rounded_vertical(panel, 18, argb(0xfa, 0x161b26), argb(0xec, 0x0c0f16));
        display.stroke_rounded(panel, 18, 1, argb(0xff, 0x303a4c));
        display.horizontal_gradient(
            Rect::new(panel.x + 18, panel.y + 1, panel.w - 36, 1),
            self.theme.accent_soft(210),
            self.theme.accent_alt_soft(40),
        );

        display.text(panel.x + 20, panel.y + 14, "Applications", 15, true, TEXT);
        let hint = "Super";
        let hint_width = widgets::text_width(hint, 10, true);
        let hint_rect = Rect::new(
            panel.right() - 18 - hint_width - 16,
            panel.y + 16,
            hint_width + 16,
            18,
        );
        display.fill_rounded(hint_rect, 9, argb(0x24, 0xffffff));
        display.text_centered(hint_rect, hint, 10, true, TEXT_DIM);

        for (index, kind) in DOCK_APPS.iter().enumerate() {
            let row = launch_row_rect(index);
            let selected = index == self.launcher_selected;
            let hot = self.hover == Target::Dock(*kind);
            if selected || hot {
                display.fill_rounded_vertical(
                    row,
                    10,
                    if selected {
                        self.theme.accent_soft(46)
                    } else {
                        argb(0x28, 0xffffff)
                    },
                    if selected {
                        self.theme.accent_soft(18)
                    } else {
                        argb(0x10, 0xffffff)
                    },
                );
                display.stroke_rounded(
                    row,
                    10,
                    1,
                    self.theme.accent_soft(if selected { 120 } else { 50 }),
                );
                if selected {
                    display.fill_rounded(
                        Rect::new(row.x + 5, row.y + 11, 3, row.h - 22),
                        2,
                        self.theme.accent,
                    );
                }
            }
            let badge = Rect::new(row.x + 14, row.y + 7, 32, 32);
            display.fill_rounded_vertical(
                badge,
                9,
                if selected {
                    self.theme.accent_soft(76)
                } else {
                    argb(0x38, 0xffffff)
                },
                if selected {
                    self.theme.accent_soft(28)
                } else {
                    argb(0x16, 0xffffff)
                },
            );
            display.stroke_rounded(badge, 9, 1, argb(0x30, 0xffffff));
            widgets::icon(
                display,
                badge.inset(8),
                kind.icon(),
                if selected {
                    self.theme.accent
                } else {
                    TEXT_MUTED
                },
            );
            display.text(row.x + 58, row.y + 6, kind.title(), 13, true, TEXT);
            display.text_ellipsized(
                row.x + 58,
                row.y + 26,
                kind.subtitle(),
                row.w - 94,
                11,
                false,
                TEXT_DIM,
            );
            if selected {
                widgets::icon(
                    display,
                    Rect::new(row.right() - 26, row.y + 16, 13, 13),
                    Icon::Right,
                    self.theme.accent,
                );
            } else if self.windows.iter().any(|window| window.app.kind() == *kind) {
                display.fill_circle(Point::new(row.right() - 18, row.center().y), 3, TEXT_FAINT);
            }
        }
        display.text_centered(
            Rect::new(panel.x, panel.bottom() - 28, panel.w, 20),
            "↑ ↓ select    Enter open    Esc close",
            10,
            false,
            TEXT_FAINT,
        );
    }

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
            let rise = if remaining < 240 {
                ((240 - remaining) / 12) as i32
            } else {
                0
            };
            let rect = Rect::new(rect.x, rect.y + rise, rect.w, rect.h);
            display.drop_shadow(rect, 14, 14, argb(0xa0, 0x000000));
            display.fill_rounded_vertical(
                rect,
                14,
                argb(fade, 0x1a1f2b),
                argb(fade.saturating_sub(12), 0x10141c),
            );
            display.stroke_rounded(rect, 14, 1, argb(0xff, 0x333d50));
            display.fill_rounded(
                Rect::new(rect.x + 10, rect.y + 12, 3, rect.h - 24),
                2,
                self.theme.accent,
            );
            let badge = Rect::new(rect.x + 22, rect.y + 14, 30, 30);
            display.fill_rounded(badge, 9, self.theme.accent_soft(38));
            widgets::icon(display, badge.inset(8), Icon::Spark, self.theme.accent);
            display.text(rect.x + 62, rect.y + 13, &note.title, 12, true, TEXT);
            display.text_ellipsized(
                rect.x + 62,
                rect.y + 33,
                &note.body,
                rect.w - 78,
                11,
                false,
                TEXT_MUTED,
            );
            // A hairline shows how long the toast will stay.
            let ratio = if note.life == 0 {
                0.0
            } else {
                remaining as f32 / note.life as f32
            };
            display.fill_rounded(
                Rect::new(rect.x + 62, rect.bottom() - 15, rect.w - 84, 2),
                1,
                argb(0x2e, 0xffffff),
            );
            display.fill_rounded(
                Rect::new(
                    rect.x + 62,
                    rect.bottom() - 15,
                    (((rect.w - 84) as f32 * ratio) as i32).max(2),
                    2,
                ),
                1,
                self.theme.accent_soft(205),
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
    display.drop_shadow(
        rect,
        radius,
        18,
        argb(if focused { 0xa0 } else { 0x70 }, 0x000000),
    );
    display.fill_rounded_vertical(rect, radius, WINDOW_BG, WINDOW_BG_DEEP);

    let title_top = if focused { TITLE_BG_FOCUS } else { TITLE_BG };
    let title_bottom = if focused {
        argb(0xff, 0x191e28)
    } else {
        argb(0xff, 0x141821)
    };
    let title_rect = Rect::new(rect.x, rect.y, rect.w, TITLE_HEIGHT);
    display.fill_rounded_vertical(title_rect, radius, title_top, title_bottom);
    display.fill_rect(
        Rect::new(rect.x, rect.y + TITLE_HEIGHT - radius, rect.w, radius),
        title_bottom,
    );
    display.horizontal_gradient(
        Rect::new(rect.x + radius, rect.y + 1, (rect.w - radius * 2).max(1), 1),
        theme.accent_soft(if focused { 88 } else { 26 }),
        theme.accent_alt_soft(if focused { 14 } else { 6 }),
    );
    display.fill_rect(
        Rect::new(rect.x + 1, rect.y + TITLE_HEIGHT - 1, rect.w - 2, 1),
        argb(0xff, 0x0a0d13),
    );

    let kind = window.app.kind();
    if focused {
        let tile = Rect::new(rect.x + 12, rect.y + 8, 22, 22);
        display.fill_rounded(tile, 7, theme.accent_soft(34));
        widgets::icon(display, tile.inset(4), kind.icon(), theme.accent);
        display.text_ellipsized(
            rect.x + 42,
            rect.y + 10,
            kind.title(),
            (rect.w - 158).max(30),
            13,
            true,
            TEXT_STRONG,
        );
    } else {
        widgets::icon(
            display,
            Rect::new(rect.x + 14, rect.y + 10, 18, 18),
            kind.icon(),
            TEXT_DIM,
        );
        display.text_ellipsized(
            rect.x + 42,
            rect.y + 10,
            kind.title(),
            (rect.w - 158).max(30),
            13,
            true,
            TEXT_MUTED,
        );
    }

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
        display.fill_circle(
            center,
            8,
            if hot {
                argb(0x66, color & 0x00ff_ffff)
            } else {
                argb(0x20, color & 0x00ff_ffff)
            },
        );
        display.fill_circle(
            center,
            6,
            if hot {
                color
            } else {
                argb(0xc0, color & 0x00ff_ffff)
            },
        );
        if hot {
            widgets::icon(display, hit.inset(4), glyph, rgb(0x16080a));
        }
    }

    let content = window.content();
    if !content.is_empty() {
        display.push_clip(content);
        window.app.draw(display, content, theme, focused);
        display.pop_clip();
        display.reset_clip();
    }
    if focused {
        // Inner sparkle: a faint accent line just inside the window edge.
        display.stroke_rounded(rect.inset(1), radius - 1, 1, theme.accent_soft(34));
    }
    display.stroke_rounded(
        rect,
        radius,
        1,
        if focused {
            theme.accent_soft(82)
        } else {
            BORDER
        },
    );

    // Resize grip: always present but quiet until the corner is hot.
    let grip_alpha = if hover == Target::Resize(id) { 190 } else { 70 };
    for step in 0..3 {
        let offset = 5 + step * 4;
        widgets::line(
            display,
            rect.right() - offset - 5,
            rect.bottom() - 5,
            rect.right() - 5,
            rect.bottom() - offset - 5,
            1,
            if hover == Target::Resize(id) {
                theme.accent_soft(grip_alpha)
            } else {
                argb(grip_alpha, 0xffffff)
            },
        );
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
    display.vertical_gradient(screen, rgb(0x050608), rgb(0x0a0d14));

    let center_x = width / 2;
    let center_y = height / 2;

    let cx = center_x;
    let cy = center_y - 48;
    let size = 42;
    // Diamond logo
    for y in 0..size {
        let h = if y < size / 2 { y * 2 + 2 } else { (size - y) * 2 };
        display.fill_rect(Rect::new(cx - h / 2, cy - size / 2 + y, h, 1), rgb(0x6aa6ff));
    }
    display.fill_rect(Rect::new(cx - 1, cy - 6, 2, 12), rgb(0xa9c8ff));

    widgets::text_shadow(
        display,
        cx - widgets::text_width("Zirvium", 28, true) / 2,
        cy + 28,
        "Zirvium",
        28,
        true,
        TEXT_STRONG,
    );
    display.text_centered(
        Rect::new(0, cy + 62, width, 14),
        "64-bit Rust OS",
        10,
        false,
        TEXT_MUTED,
    );

    let bar = Rect::new(cx - 80, cy + 98, 160, 2);
    display.fill_rounded(bar, 1, argb(0x28, 0xffffff));
    display.fill_rounded(Rect::new(bar.x, bar.y, 110, bar.h), 1, rgb(0x6aa6ff));

    display.present_all();
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
