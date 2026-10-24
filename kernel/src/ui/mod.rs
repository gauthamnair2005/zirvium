//! The desktop shell.
//!
//! The desktop is the first DisplayJet client: it owns the wallpaper cache,
//! the window stack, the top bar and the application navigator, and it routes
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

use crate::displayjet::{CursorStyle, Display};
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
    Settings,
    Calculator,
    Notes,
    Network,
}

/// Applications shown by the application navigator.
const NAVIGATOR_APPS: [AppKind; 7] = [
    AppKind::Terminal,
    AppKind::Files,
    AppKind::Monitor,
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
            AppKind::Settings => "Settings",
            AppKind::Calculator => "Calculator",
            AppKind::Notes => "Notes",
            AppKind::Network => "Network",
        }
    }

    pub fn subtitle(self) -> &'static str {
        match self {
            AppKind::Terminal => "Shell and diagnostics",
            AppKind::Files => "Browse the virtual filesystem",
            AppKind::Monitor => "CPU, memory and process telemetry",
            AppKind::Settings => "System, colors and Start",
            AppKind::Calculator => "Quick arithmetic",
            AppKind::Notes => "Text notes and scratchpad",
            AppKind::Network => "Wi-Fi and connections",
        }
    }

    pub fn icon(self) -> Icon {
        match self {
            AppKind::Terminal => Icon::Terminal,
            AppKind::Files => Icon::Files,
            AppKind::Monitor => Icon::Monitor,
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
            App::Settings(_) => false,
            App::Calculator(app) => app.key(code, event_modifiers),
            App::Notes(app) => app.key(code, event_modifiers),
            App::Network(_) => false,
        }
    }

    fn clipboard_copy(&self) -> String {
        match self {
            App::Terminal(app) => app.clipboard_copy(),
            App::Notes(app) => app.clipboard_copy(),
            _ => String::new(),
        }
    }

    fn clipboard_paste(&mut self, text: &str) -> bool {
        match self {
            App::Terminal(app) => app.clipboard_paste(text),
            App::Notes(app) => app.clipboard_paste(text),
            _ => false,
        }
    }

    fn select_all(&mut self) -> bool {
        match self {
            App::Terminal(app) => app.select_all(),
            App::Notes(app) => app.select_all(),
            _ => false,
        }
    }

    fn scroll(&mut self, point: Point, content: Rect, amount: i32) {
        match self {
            App::Terminal(app) => app.scroll(amount),
            App::Files(app) => app.scroll(point, content, amount),
            App::Monitor(_) => {}
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

fn launch_panel_rect(width: i32, height: i32) -> Rect {
    let panel_width = 720.min(width - 64);
    let panel_height = 540.min(height - TOP_BAR_HEIGHT - 64);
    Rect::new(
        (width - panel_width) / 2,
        TOP_BAR_HEIGHT + (height - TOP_BAR_HEIGHT - panel_height) / 2,
        panel_width,
        panel_height,
    )
}

fn launch_row_rect(panel: Rect, index: usize) -> Rect {
    let column = index % 2;
    let row = index / 2;
    Rect::new(
        panel.x + 28 + column as i32 * 326,
        panel.y + 184 + row as i32 * 82,
        306,
        68,
    )
}

fn title_buttons(rect: Rect) -> (Rect, Rect, Rect) {
    let size = 22;
    let y = rect.y + 6;
    let right = rect.right();
    (
        Rect::new(right - 28, y, size, size),
        Rect::new(right - 54, y, size, size),
        Rect::new(right - 80, y, size, size),
    )
}

fn contains_case_insensitive(text: &str, query: &str) -> bool {
    if query.is_empty() {
        return true;
    }
    let text: Vec<u8> = text.bytes().map(|byte| byte.to_ascii_lowercase()).collect();
    let query: Vec<u8> = query.bytes().map(|byte| byte.to_ascii_lowercase()).collect();
    text.windows(query.len()).any(|window| window == query)
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
    launcher_search: String,
    launcher_search_active: bool,
    cursor: Point,
    wallpaper: Vec<u32>,
    last_wallpaper_key: Option<(u8, u32, u32)>,
    notifications: Vec<Notification>,
    last_second: u64,
    last_title_click: (u32, u64),
    launcher_started: u64,
    logged_in: bool,
    clipboard: String,
    context_menu: Option<Point>,
    cursor_style: CursorStyle,
    start_power_open: bool,
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
            launcher_search: String::new(),
            launcher_search_active: false,
            cursor: Point::new(cursor_x, cursor_y),
            wallpaper: Vec::new(),
            last_wallpaper_key: None,
            notifications: Vec::new(),
            last_second: 0,
            last_title_click: (0, 0),
            launcher_started: zirvstdlib::time::monotonic_ms(),
            logged_in: false,
            clipboard: String::new(),
            context_menu: None,
            cursor_style: CursorStyle::Pointer,
            start_power_open: false,
        };
        crate::displayjet::font::set_scale_for_resolution(width, height);
        desktop.theme.text_scale = crate::displayjet::font::scale();
        desktop.notify(
            "Welcome to Zirvium",
            "Open the launcher to reach your apps and workspace.",
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
        let max_h = (self.height - TOP_BAR_HEIGHT - 16).max(240);
        let w = desired_w.min(max_w);
        let h = desired_h.min(max_h);
        let cascade = (self.windows.len() as i32 % 6) * 28;
        let max_x = (self.width - w - 16).max(16);
        let max_y = (self.height - h - 8).max(TOP_BAR_HEIGHT + 8);
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
                window.rect = Rect::new(0, TOP_BAR_HEIGHT, width, (height - TOP_BAR_HEIGHT).max(160));
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

    fn launcher_matches(&self) -> Vec<AppKind> {
        if self.launcher_search.is_empty() {
            return NAVIGATOR_APPS.to_vec();
        }
        NAVIGATOR_APPS
            .iter()
            .copied()
            .filter(|kind| {
                contains_case_insensitive(kind.title(), &self.launcher_search)
                    || contains_case_insensitive(kind.subtitle(), &self.launcher_search)
            })
            .collect()
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

    fn context_item(&self, point: Point) -> Option<usize> {
        let origin = self.context_menu?;
        let menu = Rect::new(origin.x, origin.y, 190, 176);
        if !menu.contains(point) {
            return None;
        }
        let index = ((point.y - menu.y - 36) / 32) as usize;
        (index < 4).then_some(index)
    }

    fn handle_context_action(&mut self, index: usize) -> bool {
        match index {
            0 => {
                if let Some(id) = self.focused
                    && let Some(window) = self.window(id)
                {
                    self.clipboard = window.app.clipboard_copy();
                    self.notify("Copied", "Selection copied to the clipboard.", 2600);
                }
            }
            1 => {
                if let Some(id) = self.focused {
                    let text = self.clipboard.clone();
                    if let Some(window) = self.window_mut(id) {
                        window.app.clipboard_paste(&text);
                    }
                }
            }
            2 => {
                if let Some(id) = self.focused
                    && let Some(window) = self.window_mut(id)
                {
                    window.app.select_all();
                }
            }
            3 => {
                self.cursor_style = match self.cursor_style {
                    CursorStyle::Pointer => CursorStyle::Precision,
                    CursorStyle::Precision => CursorStyle::Hand,
                    CursorStyle::Hand => CursorStyle::Pointer,
                };
            }
            _ => return false,
        }
        self.context_menu = None;
        true
    }

    // -- Input --------------------------------------------------------------

    fn target_at(&self, point: Point) -> Target {
        if !self.logged_in {
            return Target::Nothing;
        }

        if self.launcher {
            return Target::Nothing;
        }

        if point.y < TOP_BAR_HEIGHT {
            if Rect::new(12, 6, 34, 32).contains(point) {
                return Target::Start;
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
            if let Some(index) = self.context_item(point) {
                return self.handle_context_action(index);
            }
            if self.context_menu.is_some() {
                self.context_menu = None;
                return true;
            }
            return false;
        }
        if self.launcher {
            let panel = launch_panel_rect(self.width, self.height);
            let logo = Rect::new(panel.x + 22, panel.y + 24, 36, 36);
            if logo.contains(point) {
                self.launcher = false;
                return true;
            }
            let user = Rect::new(panel.right() - 142, panel.y + 24, 112, 36);
            if user.contains(point) {
                self.logged_in = false;
                self.launcher = false;
                self.start_power_open = false;
                self.focused = None;
                self.notify("Signed out", "The session is ready for the next user.", 4000);
                return true;
            }
            let power = Rect::new(panel.right() - 54, panel.y + 24, 36, 36);
            if power.contains(point) {
                self.start_power_open = !self.start_power_open;
                return true;
            }
            let search = Rect::new(panel.x + 22, panel.y + 82, panel.w - 44, 40);
            if search.contains(point) {
                self.launcher_search_active = true;
                return true;
            }
            let elapsed = zirvstdlib::time::monotonic_ms().saturating_sub(self.launcher_started);
            let progress = (elapsed as f32 / 420.0).clamp(0.0, 1.0);
            let eased = progress * progress * (3.0 - 2.0 * progress);
            let matches = self.launcher_matches();
            for (i, kind) in matches.iter().enumerate() {
                let mut rect = launch_row_rect(panel, i);
                rect.x -= ((1.0 - eased) * 28.0) as i32;
                if rect.contains(point) {
                    self.launcher = false;
                    self.open(*kind);
                    return true;
                }
            }
            if !self.start_power_open {
                if !panel.contains(point) {
                    self.launcher = false;
                }
                return true;
            }
            let power_panel = Rect::new(panel.right() - 194, panel.y + 72, 176, 148);
            let shutdown = Rect::new(power_panel.x + 12, power_panel.y + 34, power_panel.w - 24, 30);
            let reboot = Rect::new(power_panel.x + 12, power_panel.y + 70, power_panel.w - 24, 30);
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
            let logout = Rect::new(power_panel.x + 12, power_panel.y + 106, power_panel.w - 24, 30);
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
                } else if event.buttons & buttons::RIGHT != 0 {
                    if self.logged_in && !self.launcher {
                        self.context_menu = Some(self.cursor);
                        true
                    } else {
                        false
                    }
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
            self.launcher_search.clear();
            self.launcher_search_active = self.launcher;
            if self.launcher {
                self.launcher_started = zirvstdlib::time::monotonic_ms();
            }
            return true;
        }
        if event_modifiers & modifiers::CTRL != 0 {
            match code as u8 {
                b'c' | b'C' => {
                    if let Some(id) = self.focused
                        && let Some(window) = self.window(id)
                    {
                        self.clipboard = window.app.clipboard_copy();
                    }
                    return true;
                }
                b'v' | b'V' => {
                    if let Some(id) = self.focused {
                        let text = self.clipboard.clone();
                        if let Some(window) = self.window_mut(id) {
                            window.app.clipboard_paste(&text);
                        }
                    }
                    return true;
                }
                b'a' | b'A' => {
                    if let Some(id) = self.focused
                        && let Some(window) = self.window_mut(id)
                    {
                        window.app.select_all();
                    }
                    return true;
                }
                _ => {}
            }
        }
        if event_modifiers & modifiers::ALT != 0 && code == key::TAB {
            self.cycle_windows();
            return true;
        }
        if self.launcher {
            match code {
                key::ESCAPE => {
                    if !self.launcher_search.is_empty() {
                        self.launcher_search.clear();
                        self.launcher_selected = 0;
                    } else {
                        self.launcher = false;
                        self.launcher_search_active = false;
                    }
                    return true;
                }
                key::BACKSPACE => {
                    self.launcher_search.pop();
                    self.launcher_selected = 0;
                    return true;
                }
                key::UP => {
                    self.launcher_selected = self.launcher_selected.saturating_sub(1);
                    return true;
                }
                key::DOWN => {
                    let count = self.launcher_matches().len();
                    if count > 0 {
                        self.launcher_selected = (self.launcher_selected + 1).min(count - 1);
                    }
                    return true;
                }
                key::ENTER => {
                    if let Some(kind) = self.launcher_matches().get(self.launcher_selected).copied() {
                        self.launcher = false;
                        self.launcher_search_active = false;
                        self.open(kind);
                    }
                    return true;
                }
                _ => {
                    if self.launcher_search_active
                        && code >= 0x20
                        && code != 0x7f
                        && let Some(character) = char::from_u32(code)
                    {
                        self.launcher_search.push(character);
                        self.launcher_selected = 0;
                        return true;
                    }
                    return false;
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

        let theme = self.theme;
        let focused = self.focused;
        let hover = self.hover;
        for window in self.windows.iter_mut() {
            if window.minimized {
                continue;
            }
            draw_window(display, window, &theme, focused == Some(window.id), hover);
        }
        self.draw_top_bar(display);
        self.draw_notifications(display);
        if self.launcher {
            self.draw_start_screen(display);
        }

        if let Some(origin) = self.context_menu {
            self.draw_context_menu(display, origin);
        }

        let cursor = self.cursor;
        display.cursor_show_with_style(cursor.x, cursor.y, self.cursor_style);
        display.present();
    }

    fn draw_top_bar(&self, display: &mut Display) {
        let bar = Rect::new(0, 0, self.width, TOP_BAR_HEIGHT);
        display.fill_rect(bar, self.theme.chrome());
        display.fill_rect(Rect::new(0, TOP_BAR_HEIGHT - 1, self.width, 1), self.theme.line());
        widgets::logo(display, Rect::new(18, 10, 24, 24));
        display.text(52, 13, "Zirvium", 12, true, self.theme.text());
        display.text(132, 14, "APPLICATIONS", 8, false, self.theme.text_dim());
        let status = crate::net::status();
        let right = Rect::new(self.width - 350, 8, 332, 28);
        display.fill_rounded(right, 14, self.theme.well());
        display.fill_circle(Point::new(right.x + 14, right.y + 14), 3, self.theme.accent);
        display.text(right.x + 25, right.y + 8, if status.ethernet { "Connected" } else { "Ready" }, 9, false, self.theme.text_muted());
        let now = crate::time::now();
        let clock = if self.theme.show_seconds {
            alloc::format!("{:02}:{:02}:{:02}", now.hour, now.minute, now.second)
        } else {
            alloc::format!("{:02}:{:02}", now.hour, now.minute)
        };
        display.text(right.right() - 80, right.y + 8, &clock, 9, true, self.theme.text());
    }

    fn draw_context_menu(&self, display: &mut Display, origin: Point) {
        let menu = Rect::new(
            origin.x.clamp(8, self.width - 198),
            origin.y.clamp(8, self.height - 184),
            190,
            176,
        );
        display.fill_rounded(menu, 10, self.theme.card());
        display.text(menu.x + 14, menu.y + 12, "Quick actions", 11, true, self.theme.text());
        let items = ["Copy", "Paste", "Select all", "Cursor: change"];
        for (index, label) in items.iter().enumerate() {
            let row = Rect::new(menu.x + 8, menu.y + 30 + index as i32 * 32, menu.w - 16, 28);
            display.fill_rounded(row, 6, if self.context_item(self.cursor) == Some(index) {
                self.theme.accent_soft(34)
            } else {
                self.theme.card()
            });
            display.text(row.x + 10, row.y + 8, label, 10, false, self.theme.text());
        }
    }

    fn draw_notifications(&self, display: &mut Display) {
        if self.notifications.is_empty() {
            return;
        }
        let width = (self.width - 40).clamp(240, 348);
        let height = 74;
        let mut bottom = self.height - 12;
        for note in self.notifications.iter().rev() {
            let rect = Rect::new(self.width - width - 18, bottom - height, width, height);
            bottom -= height + 12;
            let now = zirvstdlib::time::monotonic_ms();
            let remaining = note.life.saturating_sub(now.saturating_sub(note.born));
            let fade = if remaining < 600 { 160 } else { 246 };
            let rise = if remaining < 240 { ((240 - remaining) / 12) as i32 } else { 0 };
            let rect = Rect::new(rect.x, rect.y + rise, rect.w, rect.h);
            display.fill_rect(rect, argb(fade, self.theme.card() & 0x00ff_ffff));
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
        let panel = launch_panel_rect(self.width, self.height);
        display.fill_rect(
            Rect::new(0, TOP_BAR_HEIGHT, self.width, self.height - TOP_BAR_HEIGHT),
            argb(0x42, 0x000000),
        );
        let panel = Rect::new(panel.x, panel.y, panel.w, panel.h);
        display.fill_rounded(panel, 18, if self.theme.is_light() { argb(0xf5, 0xffffff) } else { argb(0xf2, 0x111b2c) });

        widgets::logo(display, Rect::new(panel.x + 22, panel.y + 24, 36, 36));
        display.text(panel.x + 70, panel.y + 25, "Zirvium", 18, true, self.theme.text());
        display.text(panel.x + 70, panel.y + 48, "Your workspace", 9, false, self.theme.text_muted());

        let user = Rect::new(panel.right() - 142, panel.y + 24, 112, 36);
        display.fill_rounded(user, 10, self.theme.well());
        widgets::icon(display, Rect::new(user.x + 9, user.y + 9, 18, 18), Icon::User, self.theme.accent);
        display.text(user.x + 34, user.y + 12, "liveuser", 9, true, self.theme.text());
        let power = Rect::new(panel.right() - 54, panel.y + 24, 36, 36);
        display.fill_rounded(power, 10, if self.start_power_open { self.theme.accent_soft(50) } else { self.theme.well() });
        widgets::icon(display, Rect::new(power.x + 7, power.y + 7, 22, 22), Icon::Power, self.theme.accent);

        let search = Rect::new(panel.x + 22, panel.y + 82, panel.w - 44, 40);
        display.fill_rounded(search, 11, self.theme.well());
        widgets::icon(display, Rect::new(search.x + 13, search.y + 12, 16, 16), Icon::Search, self.theme.text_muted());
        if self.launcher_search.is_empty() {
            display.text(search.x + 40, search.y + 12, "Search apps and settings", 10, false, self.theme.text_muted());
        } else {
            display.text(search.x + 40, search.y + 12, &self.launcher_search, 10, false, self.theme.text());
        }
        if self.launcher_search_active
            && (zirvstdlib::time::monotonic_ms() / 530).is_multiple_of(2)
        {
            let cursor_x = search.x + 40 + display.text_width(&self.launcher_search, 10, false);
            display.fill_rect(Rect::new(cursor_x, search.y + 10, 2, 20), self.theme.accent);
        }
        display.text(panel.x + 22, panel.y + 140, "Applications", 10, true, self.theme.text());

        let matches = self.launcher_matches();
        for (index, kind) in matches.iter().enumerate() {
            let mut rect = launch_row_rect(panel, index);
            rect.x -= ((1.0 - eased) * 28.0) as i32;
            display.fill_rounded(rect, 12, if index == self.launcher_selected || self.focused_kind() == Some(*kind) {
                self.theme.accent_soft(72)
            } else {
                self.theme.well()
            });
            widgets::icon(display, Rect::new(rect.x + 15, rect.y + 17, 32, 32), kind.icon(), self.theme.accent);
            display.text(rect.x + 60, rect.y + 16, kind.title(), 10, true, self.theme.text());
            display.text(rect.x + 60, rect.y + 37, kind.subtitle(), 8, false, self.theme.text_muted());
        }
        if matches.is_empty() {
            display.text(panel.x + 22, panel.y + 190, "No matching apps", 11, true, self.theme.text());
            display.text(panel.x + 22, panel.y + 214, "Try another name or keyword.", 9, false, self.theme.text_muted());
        }

        if self.start_power_open {
            let power_panel = Rect::new(panel.right() - 194, panel.y + 72, 176, 148);
                display.fill_rounded(power_panel, 14, self.theme.card());
                display.text(power_panel.x + 14, power_panel.y + 12, "Power", 11, true, self.theme.text());
            let shutdown = Rect::new(power_panel.x + 12, power_panel.y + 34, power_panel.w - 24, 30);
            display.fill_rounded(shutdown, 6, argb(0x50, 0xe81123));
            display.text_centered(shutdown, "Shut down", 10, false, self.theme.text());
            let reboot = Rect::new(power_panel.x + 12, power_panel.y + 70, power_panel.w - 24, 30);
            display.fill_rounded(reboot, 6, argb(0x30, 0xffffff));
            display.text_centered(reboot, "Restart", 10, false, self.theme.text());
            let logout = Rect::new(power_panel.x + 12, power_panel.y + 106, power_panel.w - 24, 30);
            display.fill_rounded(logout, 6, argb(0x30, 0xffffff));
            display.text_centered(logout, "Log out", 10, false, self.theme.text());
        }

    }

    fn draw_login(&self, display: &mut Display) {
        let screen = Rect::new(0, 0, self.width, self.height);
        display.clear(rgb(0x030405));
        display.vertical_gradient(screen, rgb(0x05070b), rgb(0x0d1420));

        let card = Rect::new(self.width / 2 - 176, self.height / 2 - 152, 352, 330);
        display.fill_rounded(card, 16, self.theme.card());
        widgets::logo(display, Rect::new(self.width / 2 - 38, card.y + 28, 76, 76));
        display.text_centered(
            Rect::new(card.x, card.y + 116, card.w, 26),
            "Welcome to Zirvium",
            18,
            true,
            self.theme.text(),
        );
        display.text_centered(
            Rect::new(card.x, card.y + 148, card.w, 16),
            "Sign in to continue",
            10,
            false,
            self.theme.text_muted(),
        );
        let user = Rect::new(card.x + 42, card.y + 184, card.w - 84, 36);
        display.fill_rounded(user, 8, self.theme.well());
        widgets::icon(
            display,
            Rect::new(user.x + 10, user.y + 10, 16, 16),
            Icon::About,
            self.theme.text_muted(),
        );
        display.text(user.x + 36, user.y + 11, "liveuser", 11, true, self.theme.text());
        let button = Rect::new(self.width / 2 - 92, self.height / 2 + 92, 184, 38);
        widgets::button(display, button, "Enter workspace", &self.theme, widgets::ButtonStyle::Primary, false, true);
        display.text_centered(
            Rect::new(card.x, card.bottom() - 28, card.w, 14),
            "Press Enter or click Sign in",
            9,
            false,
            self.theme.text_dim(),
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
    display.fill_rounded(rect, WINDOW_RADIUS, theme.surface());

    let title_rect = Rect::new(rect.x, rect.y, rect.w, TITLE_HEIGHT);
    display.fill_rounded(title_rect, WINDOW_RADIUS, if focused { theme.card() } else { theme.chrome() });
    display.fill_rect(Rect::new(rect.x, rect.y + WINDOW_RADIUS, rect.w, TITLE_HEIGHT - WINDOW_RADIUS), if focused { theme.card() } else { theme.chrome() });
    display.fill_rect(Rect::new(rect.x + 18, rect.y + TITLE_HEIGHT - 1, rect.w - 36, 1), if focused { theme.accent } else { theme.line() });

    let kind = window.app.kind();
    widgets::icon(display, Rect::new(rect.x + 6, rect.y + 7, 16, 16), kind.icon(), if focused { theme.accent } else { theme.text_dim() });
    display.text_ellipsized(rect.x + 28, rect.y + 7, kind.title(), (rect.w - 150).max(30), 11, false, if focused { theme.text() } else { theme.text_muted() });

    let id = window.id;
    let hot_close = hover == Target::Close(id);
    let hot_min = hover == Target::Minimize(id);
    let hot_max = hover == Target::Maximize(id);
    let close = Rect::new(rect.right() - 28, rect.y + 6, 22, 22);
    let min = Rect::new(rect.right() - 54, rect.y + 6, 22, 22);
    let max = Rect::new(rect.right() - 80, rect.y + 6, 22, 22);
    display.fill_rounded(min, 4, if hot_min { theme.well() } else { theme.card() });
    widgets::icon(
        display,
        min,
        Icon::Minimize,
        if hot_min { theme.text() } else { theme.text_muted() },
    );
    display.fill_rounded(max, 4, if hot_max { theme.well() } else { theme.card() });
    widgets::icon(
        display,
        max,
        Icon::Maximize,
        if hot_max { theme.text() } else { theme.text_muted() },
    );
    display.fill_rounded(close, 4, if hot_close { rgb(0xe81123) } else { theme.card() });
    widgets::icon(
        display,
        close,
        Icon::Close,
        if hot_close { rgb(0xffffff) } else { theme.text_muted() },
    );

    let content = window.content();
    if !content.is_empty() {
        display.push_clip(content);
        window.app.draw(display, content, theme, focused);
        display.pop_clip();
        display.reset_clip();
    }
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
