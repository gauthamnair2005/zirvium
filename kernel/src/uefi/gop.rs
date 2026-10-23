//! Graphics Output Protocol (GOP) framebuffer acquisition.

use super::{is_error, BootServices, Guid, Status, SystemTable, EFI_NOT_FOUND};
use crate::arch::serial;
use core::ffi::c_void;
use core::ptr;

pub const GOP_GUID: Guid = Guid::new(
    0x9042_a9de,
    0x23dc,
    0x4a38,
    [0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a],
);

#[repr(C)]
pub struct GraphicsOutputProtocol {
    pub query_mode: unsafe extern "efiapi" fn(
        *mut GraphicsOutputProtocol,
        u32,
        *mut usize,
        *mut *mut ModeInfo,
    ) -> Status,
    pub set_mode: unsafe extern "efiapi" fn(*mut GraphicsOutputProtocol, u32) -> Status,
    pub blt: usize,
    pub mode: *mut GraphicsOutputMode,
}

#[repr(C)]
pub struct GraphicsOutputMode {
    pub max_mode: u32,
    pub mode: u32,
    /// Current mode information.
    pub info: *mut ModeInfo,
    /// Size of the buffer `info` points at.
    pub size_of_info: usize,
    pub frame_buffer_base: u64,
    pub frame_buffer_size: usize,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct PixelBitmask {
    pub red: u32,
    pub green: u32,
    pub blue: u32,
    pub reserved: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ModeInfo {
    pub version: u32,
    pub horizontal_resolution: u32,
    pub vertical_resolution: u32,
    pub pixel_format: u32,
    pub pixel_bitmask: PixelBitmask,
    pub pixels_per_scan_line: u32,
}

pub const PIXEL_RGB: u32 = 0;
pub const PIXEL_BGR: u32 = 1;
pub const PIXEL_BITMASK: u32 = 2;
pub const PIXEL_BLT_ONLY: u32 = 3;

/// A framebuffer we can write directly.  `address` is a physical address that
/// is also valid as a virtual address because Zirvium identity-maps memory.
#[derive(Clone, Copy, Debug)]
pub struct Framebuffer {
    pub address: *mut u32,
    pub width: u32,
    pub height: u32,
    pub stride: u32,
    pub size: u64,
    pub swap_red_blue: bool,
}

impl Framebuffer {
    pub fn is_valid(&self) -> bool {
        !self.address.is_null() && self.width > 0 && self.height > 0 && self.stride >= self.width
    }

    pub fn pixel_count(&self) -> usize {
        self.stride as usize * self.height as usize
    }
}

/// Locate GOP, choose a sensible mode, and return the framebuffer.
pub fn init(services: &mut BootServices, system_table: *mut SystemTable) -> Framebuffer {
    let mut interface: *mut c_void = ptr::null_mut();
    // SAFETY: this is the standard GOP protocol lookup.
    let status = unsafe {
        (services.locate_protocol)(&GOP_GUID, ptr::null_mut(), &mut interface)
    };
    if is_error(status) || interface.is_null() {
        serial::write_str("GOP: protocol not found\n");
        return Framebuffer {
            address: ptr::null_mut(),
            width: 0,
            height: 0,
            stride: 0,
            size: 0,
            swap_red_blue: false,
        };
    }

    // SAFETY: LocateProtocol returned a valid GOP interface.
    let gop = interface as *mut GraphicsOutputProtocol;
    let mode = unsafe { (*gop).mode };
    if mode.is_null() {
        serial::write_str("GOP: no current mode\n");
        return invalid();
    }

    let max_mode = unsafe { (*mode).max_mode };
    let mut best_index = unsafe { (*mode).mode };
    let mut best_score = 0i64;
    let mut best_info: Option<ModeInfo> = None;

    for index in 0..max_mode {
        let mut size = 0usize;
        let mut info: *mut ModeInfo = ptr::null_mut();
        // SAFETY: index is within the firmware's advertised mode count.
        let status = unsafe { ((*gop).query_mode)(gop, index, &mut size, &mut info) };
        if is_error(status) || info.is_null() {
            continue;
        }
        // SAFETY: QueryMode returned a valid mode info structure.
        let mode_info = unsafe { *info };
        // SAFETY: the buffer belongs to boot services and is ours to free.
        unsafe {
            let _ = (services.free_pool)(info as *mut c_void);
        }
        if mode_info.pixel_format == PIXEL_BLT_ONLY
            || mode_info.horizontal_resolution < 800
            || mode_info.vertical_resolution < 600
        {
            continue;
        }
        let score = mode_score(
            mode_info.horizontal_resolution,
            mode_info.vertical_resolution,
            mode_info.pixel_format,
        );
        if score > best_score {
            best_score = score;
            best_index = index;
            best_info = Some(mode_info);
        }
    }

    if let Some(_info) = best_info {
        if best_index != unsafe { (*mode).mode } {
            // SAFETY: best_index came from the firmware's mode list.
            let status = unsafe { ((*gop).set_mode)(gop, best_index) };
            if is_error(status) {
                serial::write_str("GOP: SetMode failed, using current mode\n");
            }
        }
        // Read the mode again: SetMode may have changed the framebuffer base.
        let mode = unsafe { (*gop).mode };
        if mode.is_null() {
            return invalid();
        }
        let current = unsafe { &*mode };
        if current.info.is_null() {
            return invalid();
        }
        let info = unsafe { &*current.info };
        return Framebuffer {
            address: current.frame_buffer_base as usize as *mut u32,
            width: info.horizontal_resolution,
            height: info.vertical_resolution,
            stride: info.pixels_per_scan_line,
            size: current.frame_buffer_size as u64,
            swap_red_blue: info.pixel_format == PIXEL_RGB,
        };
    }

    // No suitable queried mode: use the current one.
    let current = unsafe { &*mode };
    if current.info.is_null() {
        return invalid();
    }
    let info = unsafe { &*current.info };
    let _ = system_table;
    Framebuffer {
        address: current.frame_buffer_base as usize as *mut u32,
        width: info.horizontal_resolution,
        height: info.vertical_resolution,
        stride: info.pixels_per_scan_line,
        size: current.frame_buffer_size as u64,
        swap_red_blue: info.pixel_format == PIXEL_RGB,
    }
}

fn mode_score(width: u32, height: u32, format: u32) -> i64 {
    let base = match (width, height) {
        (1280, 800) => 2000,
        (1280, 720) => 1980,
        (1366, 768) => 1960,
        (1440, 900) => 1940,
        (1600, 900) => 1920,
        (1920, 1080) => 1900,
        (1024, 768) => 1500,
        (1024, 600) => 1480,
        (800, 600) => 1000,
        _ => (width as i64 * height as i64) / 1000,
    };
    let format_bonus = match format {
        PIXEL_BGR => 100,
        PIXEL_RGB => 80,
        PIXEL_BITMASK => 40,
        _ => 0,
    };
    base + format_bonus
}

fn invalid() -> Framebuffer {
    Framebuffer {
        address: ptr::null_mut(),
        width: 0,
        height: 0,
        stride: 0,
        size: 0,
        swap_red_blue: false,
    }
}

/// Locate the GOP interface without selecting a mode.  Useful for diagnostics.
pub fn locate(services: &BootServices) -> Result<*mut GraphicsOutputProtocol, Status> {
    let mut interface: *mut c_void = ptr::null_mut();
    // SAFETY: standard protocol lookup.
    let status = unsafe { (services.locate_protocol)(&GOP_GUID, ptr::null_mut(), &mut interface) };
    if status == EFI_NOT_FOUND || is_error(status) || interface.is_null() {
        Err(status)
    } else {
        Ok(interface as *mut GraphicsOutputProtocol)
    }
}
