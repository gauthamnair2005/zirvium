//! The writable RAM-backed filesystem.
//!
//! This is a small, read-mostly in-memory filesystem that exposes the
//! kernel's state under `/proc` alongside a handful of documentation files.
//! It is deliberately allocation-friendly: the desktop browser and the
//! terminal can both use the same API without a block device or a disk
//! driver.

use alloc::string::{String, ToString};
use alloc::vec::Vec;

use zirvstdlib::sync::Once;

#[derive(Clone)]
pub struct Entry {
    pub name: String,
    pub path: String,
    pub is_dir: bool,
    pub size: u64,
}

impl Entry {
    pub fn icon(&self) -> &'static str {
        if self.is_dir {
            "dir"
        } else {
            match self.name.rsplit('.').next() {
                Some("md") | Some("txt") => "text",
                Some("log") => "log",
                _ => "file",
            }
        }
    }
}

struct File {
    path: String,
    content: String,
}

struct Vfs {
    files: zirvstdlib::sync::SpinLock<Vec<File>>,
}

static VFS: Once<Vfs> = Once::new();

const DIRECTORIES: &[&str] = &[
    "/",
    "/dev",
    "/etc",
    "/home",
    "/home/zirvium",
    "/proc",
    "/tmp",
    "/usr",
    "/usr/share",
    "/usr/share/docs",
    "/var",
    "/var/log",
];

const FILES: &[(&str, &str)] = &[
    (
        "/README.md",
        "# Zirvium v2\n\
         \n\
         Zirvium is a 64-bit operating system written from scratch in Rust and\n\
         assembly. It follows two ideas:\n\
         \n\
         * MOSIX: the cluster is one computer. Processes have a home node and a\n\
           current node, and can migrate without losing their identity.\n\
         * DisplayJet: a display engine that treats the screen as a first-class\n\
           resource, with damage-tracked compositing and anti-aliased text.\n\
         \n\
         Everything you see was rendered by DisplayJet from a 32-bit ARGB\n\
         surface and presented to the firmware's GPU-backed scanout.\n\
         \n\
         Explore /proc for live kernel state, or open the System Monitor.\n",
    ),
    (
        "/etc/motd",
        "Welcome to Zirvium 2.0.\n\
         \n\
         This machine is node 0 of a MOSIX cluster. Kernel, drivers,\n\
         compositor and desktop are all Rust, with assembly only where the CPU\n\
         requires it: interrupt entry, descriptor-table loads and the syscall\n\
         gate.\n",
    ),
    (
        "/etc/os-release",
        "NAME=Zirvium\n\
         VERSION=2.0.1\n\
         ID=zirvium\n\
         PRETTY_NAME=\"Zirvium 2.0\"\n\
         ANSI_COLOR=\"38;2;96;165;250\"\n\
         HOME_URL=\"https://github.com/gauthamnair2005/zirvium\"\n",
    ),
    (
        "/home/zirvium/welcome.txt",
        "Hello, and welcome to Zirvium.\n\
         \n\
         Try the terminal:\n\
           help        list commands\n\
           neofetch    system summary\n\
           ps          MOSIX processes\n\
           nodes       cluster members\n\
           gpu         display adapter\n\
           dmesg       boot log\n\
           ls /proc    live kernel data\n\
         \n\
         Windows can be dragged by their title bar. The dock opens apps, and\n\
         the launcher is available from the top-left button or the Super key.\n",
    ),
    (
        "/home/zirvium/notes.txt",
        "Design notes\n\
         ============\n\
         \n\
         DisplayJet keeps a persistent back buffer. The desktop repaints the\n\
         scene only when something changes, and moving the pointer restores a\n\
         small saved patch instead of repainting windows.\n\
         \n\
         Text is rendered from an embedded anti-aliased atlas generated from\n\
         DejaVu Sans at seven sizes in two weights. Every glyph is an 8-bit\n\
         coverage mask, so text scales to the UI without hinting artefacts.\n",
    ),
    (
        "/usr/share/docs/displayjet.txt",
        "DisplayJet\n\
         ==========\n\
         \n\
         DisplayJet is the display abstraction. It owns:\n\
         \n\
         * the ARGB surface and its clipping stack;\n\
         * damage tracking for partial presentation;\n\
         * anti-aliased rounded rectangles, strokes, gradients and shadows;\n\
         * the font atlas renderer and text metrics;\n\
         * the hardware cursor overlay.\n\
         \n\
         The presentation boundary is intentionally small. Firmware provides a\n\
         GPU-backed linear framebuffer via GOP; DisplayJet copies damaged rows\n\
         into it. A native iGPU blitter can replace that copy without changing\n\
         any code above the boundary.\n",
    ),
    (
        "/usr/share/docs/mosix.txt",
        "MOSIX\n\
         =====\n\
         \n\
         A MOSIX cluster presents one pool of resources. Every process records\n\
         its home node (where it was created) and its current node (where it\n\
         runs). Migration changes only the current node.\n\
         \n\
         Node load is published in permille so the scheduler can compare\n\
         machines with different core counts. A process may only migrate to a\n\
         node that is online or busy; offline and draining nodes are excluded.\n",
    ),
    (
        "/usr/share/docs/architecture.txt",
        "Zirvium architecture\n\
         ====================\n\
         \n\
         Boot: UEFI loads a PE32+ image. Zirvium uses firmware only for the\n\
         memory map and the Graphics Output Protocol, then calls\n\
         ExitBootServices and never returns to it.\n\
         \n\
         Memory: a 64 GiB identity map uses 2 MiB pages. A bitmap frame\n\
         allocator tracks 4 KiB frames; a boundary-tagged free-list heap backs\n\
         Rust's alloc.\n\
         \n\
         Interrupts: the legacy PIC is remapped to vectors 32..48. Exception\n\
         stubs and IRQ stubs are assembly; dispatch is Rust. The syscall gate\n\
         uses vector 0x80.\n\
         \n\
         There is no 32-bit code anywhere in the tree.\n",
    ),
    (
        "/var/log/boot.log",
        "boot log is available in the terminal with `dmesg`\n",
    ),
    ("/proc/cpuinfo", ""),
    ("/proc/meminfo", ""),
    ("/proc/nodes", ""),
    ("/proc/processes", ""),
    ("/proc/version", ""),
    ("/proc/uptime", ""),
    (
        "/proc/mounts",
        "procfs /proc proc rw,nosuid,nodev,noexec 0 0\nramfs / ramfs rw 0 0\n",
    ),
];

fn initialise() -> Vfs {
    let mut files = Vec::new();
    for (path, content) in FILES {
        files.push(File {
            path: (*path).to_string(),
            content: (*content).to_string(),
        });
    }
    Vfs {
        files: zirvstdlib::sync::SpinLock::new(files),
    }
}

fn vfs() -> &'static Vfs {
    VFS.call_once(initialise)
}

/// Normalise a path: leading slash, no trailing slash except root.
pub fn normalize(path: &str) -> String {
    let trimmed = path.trim();
    let mut output = String::new();
    if !trimmed.starts_with('/') {
        output.push('/');
    }
    let mut parts: Vec<&str> = Vec::new();
    for part in trimmed.split('/') {
        match part {
            "" | "." => {}
            ".." => {
                parts.pop();
            }
            other => parts.push(other),
        }
    }
    output.push_str(&parts.join("/"));
    if output.is_empty() {
        output.push('/');
    }
    output
}

fn parent(path: &str) -> &str {
    if path == "/" {
        return "/";
    }
    match path.rfind('/') {
        Some(0) => "/",
        Some(index) => &path[..index],
        None => "/",
    }
}

fn file_name(path: &str) -> &str {
    if path == "/" {
        return "/";
    }
    path.rsplit('/').next().unwrap_or(path)
}

/// Resolve a path relative to `cwd`.
pub fn resolve(cwd: &str, path: &str) -> String {
    if path.starts_with('/') {
        normalize(path)
    } else if path == ".." {
        normalize(parent(cwd))
    } else {
        normalize(&alloc::format!("{cwd}/{path}"))
    }
}

pub fn exists(path: &str) -> bool {
    let path = normalize(path);
    DIRECTORIES.contains(&path.as_str()) || vfs().files.lock().iter().any(|file| file.path == path)
}

pub fn is_dir(path: &str) -> bool {
    let path = normalize(path);
    DIRECTORIES.contains(&path.as_str())
}

/// List a directory, directories first.
pub fn list(path: &str) -> Vec<Entry> {
    let path = normalize(path);
    if !is_dir(&path) {
        return Vec::new();
    }
    let mut entries = Vec::new();
    for directory in DIRECTORIES {
        if *directory == path || *directory == "/" && path == "/" {
            continue;
        }
        if parent(directory) == path {
            entries.push(Entry {
                name: file_name(directory).to_string(),
                path: (*directory).to_string(),
                is_dir: true,
                size: 0,
            });
        }
    }
    let files = vfs().files.lock();
    for file in files.iter() {
        if parent(&file.path) == path {
            entries.push(Entry {
                name: file_name(&file.path).to_string(),
                path: file.path.clone(),
                is_dir: false,
                size: file.content.len() as u64,
            });
        }
    }
    entries.sort_by(|a, b| {
        b.is_dir
            .cmp(&a.is_dir)
            .then_with(|| a.name.to_lowercase().cmp(&b.name.to_lowercase()))
    });
    entries
}

fn dynamic(path: &str) -> Option<String> {
    match path {
        "/proc/cpuinfo" => {
            use core::fmt::Write;
            let mut output = String::new();
            let features = crate::arch::cpu::features();
            let _ = writeln!(output, "vendor\t\t: {}", crate::arch::cpu::vendor_str());
            let _ = writeln!(output, "architecture\t: x86_64");
            let _ = writeln!(output, "mode\t\t: 64-bit");
            let _ = writeln!(output, "sse2\t\t: {}", features.sse2);
            let _ = writeln!(output, "sse4.2\t\t: {}", features.sse4_2);
            let _ = writeln!(output, "avx\t\t: {}", features.avx);
            let _ = writeln!(output, "avx2\t\t: {}", features.avx2);
            let _ = writeln!(output, "tsc\t\t: invariant={}", features.invariant_tsc);
            Some(output)
        }
        "/proc/meminfo" => {
            use core::fmt::Write;
            let stats = crate::mm::stats();
            let mut output = String::new();
            let _ = writeln!(output, "MemTotal:  {} KiB", stats.total_bytes / 1024);
            let _ = writeln!(output, "MemFree:   {} KiB", stats.free_bytes / 1024);
            let _ = writeln!(output, "HeapTotal: {} KiB", stats.heap_total / 1024);
            let _ = writeln!(output, "HeapUsed:  {} KiB", stats.heap_used / 1024);
            let (used, total) = crate::mm::heap::stats();
            let _ = writeln!(
                output,
                "HeapUsage: {:.1}%",
                if total == 0 {
                    0.0
                } else {
                    used as f32 * 100.0 / total as f32
                }
            );
            Some(output)
        }
        "/proc/nodes" => Some(crate::mosix::nodes_text()),
        "/proc/processes" => {
            use core::fmt::Write;
            let mut output = String::new();
            let _ = writeln!(
                output,
                "{:>5} {:>5} {:<10} {:<24} {:>6}",
                "PID", "PPID", "STATE", "NAME", "TICKS"
            );
            for process in crate::mosix::processes() {
                let _ = writeln!(
                    output,
                    "{:>5} {:>5} {:<10} {:<24} {:>6}",
                    process.pid,
                    process.parent_pid,
                    crate::mosix::process_state_name(process.state),
                    process.name_str(),
                    process.cpu_ticks
                );
            }
            Some(output)
        }
        "/proc/version" => Some(alloc::format!(
            "Zirvium version {} ({}), built with Rust, 64-bit, no libc\n",
            zirvstdlib::VERSION,
            zirvstdlib::TARGET
        )),
        "/proc/uptime" => {
            let milliseconds = crate::arch::pit::uptime_ms();
            Some(alloc::format!(
                "{}.{:03} {}.{:03}\n",
                milliseconds / 1000,
                milliseconds % 1000,
                milliseconds / 1000,
                milliseconds % 1000
            ))
        }
        "/var/log/boot.log" => {
            use core::fmt::Write;
            let mut lines = [crate::logger::LogLine {
                level: zirvstdlib::log::Level::Info,
                len: 0,
                text: [0; 192],
            }; 128];
            let count = crate::logger::recent(&mut lines);
            let mut output = String::new();
            for line in &lines[..count] {
                let _ = writeln!(output, "{}", line.as_str());
            }
            Some(output)
        }
        _ => None,
    }
}

/// Read a file into a string.
pub fn read(path: &str) -> Option<String> {
    let path = normalize(path);
    if let Some(content) = dynamic(&path) {
        return Some(content);
    }
    vfs()
        .files
        .lock()
        .iter()
        .find(|file| file.path == path)
        .map(|file| file.content.clone())
}

/// Read at most `limit` bytes.
pub fn read_limited(path: &str, limit: usize) -> Option<String> {
    let content = read(path)?;
    if content.len() <= limit {
        Some(content)
    } else {
        let mut end = limit;
        while end > 0 && !content.is_char_boundary(end) {
            end -= 1;
        }
        Some(content[..end].to_string())
    }
}

/// Create or replace a file.  Parent directories must exist.
pub fn write(path: &str, content: &str) -> bool {
    let path = normalize(path);
    if path == "/" || is_dir(&path) {
        return false;
    }
    if !is_dir(parent(&path)) {
        return false;
    }
    let vfs = VFS.call_once(initialise);
    let mut files = vfs.files.lock();
    if let Some(file) = files.iter_mut().find(|file| file.path == path) {
        file.content = content.to_string();
        return true;
    }
    files.push(File {
        path,
        content: content.to_string(),
    });
    true
}

/// Delete a file.
pub fn remove(path: &str) -> bool {
    let path = normalize(path);
    let vfs = VFS.call_once(initialise);
    let mut files = vfs.files.lock();
    let before = files.len();
    files.retain(|file| file.path != path);
    files.len() != before
}

/// File size, or zero when the file does not exist.
pub fn size(path: &str) -> u64 {
    read(path).map(|content| content.len() as u64).unwrap_or(0)
}

/// This build has no block-device filesystem yet. Writable user data is kept
/// in the RAM-backed filesystem and is available for the lifetime of a boot.
pub fn storage_label() -> &'static str {
    "Temporary storage (RAM)"
}

/// A short textual tree used by the Files application.
pub fn tree(path: &str, depth: usize) -> Vec<String> {
    let mut output = Vec::new();
    fn walk(path: &str, depth: usize, maximum: usize, output: &mut Vec<String>) {
        if depth > maximum {
            return;
        }
        for entry in list(path) {
            let mut line = String::new();
            for _ in 0..depth {
                line.push_str("  ");
            }
            line.push_str(if entry.is_dir { "+ " } else { "  " });
            line.push_str(&entry.name);
            output.push(line);
            if entry.is_dir && depth < maximum {
                walk(&entry.path, depth + 1, maximum, output);
            }
        }
    }
    walk(&normalize(path), depth, depth + 2, &mut output);
    output
}
