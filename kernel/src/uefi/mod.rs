//! Minimal, dependency-free UEFI bindings.
//!
//! Zirvium uses UEFI only as a 64-bit bootloader environment: firmware loads
//! the PE32+ image, we acquire a framebuffer and the memory map, and then we
//! call `ExitBootServices` and never touch firmware code again.  No UEFI
//! runtime services are used.

pub mod gop;

use crate::arch::serial;
use core::ffi::c_void;
use core::ptr;

pub type Handle = *mut c_void;
pub type Status = usize;

pub const EFI_SUCCESS: Status = 0;
pub const EFI_ERROR: Status = 1usize << 63;
pub const EFI_INVALID_PARAMETER: Status = EFI_ERROR | 2;
pub const EFI_BUFFER_TOO_SMALL: Status = EFI_ERROR | 5;
pub const EFI_NOT_FOUND: Status = EFI_ERROR | 14;

#[inline]
pub fn is_error(status: Status) -> bool {
    status & EFI_ERROR != 0
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(C)]
pub struct Guid {
    pub data1: u32,
    pub data2: u16,
    pub data3: u16,
    pub data4: [u8; 8],
}

impl Guid {
    pub const fn new(data1: u32, data2: u16, data3: u16, data4: [u8; 8]) -> Self {
        Self {
            data1,
            data2,
            data3,
            data4,
        }
    }
}

#[repr(C)]
pub struct TableHeader {
    pub signature: u64,
    pub revision: u32,
    pub header_size: u32,
    pub crc32: u32,
    pub reserved: u32,
}

#[repr(C)]
pub struct SystemTable {
    pub header: TableHeader,
    pub firmware_vendor: *mut u16,
    pub firmware_revision: u32,
    pub console_in_handle: Handle,
    pub console_in: *mut c_void,
    pub console_out_handle: Handle,
    pub console_out: *mut SimpleTextOutput,
    pub standard_error_handle: Handle,
    pub standard_error: *mut SimpleTextOutput,
    pub runtime_services: *mut c_void,
    pub boot_services: *mut BootServices,
    pub number_of_table_entries: usize,
    pub configuration_table: *mut ConfigurationTable,
}

#[repr(C)]
pub struct ConfigurationTable {
    pub vendor_guid: Guid,
    pub vendor_table: *mut c_void,
}

#[repr(C)]
pub struct SimpleTextOutput {
    pub reset: unsafe extern "efiapi" fn(*mut SimpleTextOutput, u8) -> Status,
    pub output_string: unsafe extern "efiapi" fn(*mut SimpleTextOutput, *const u16) -> Status,
    pub test_string: usize,
    pub query_mode: usize,
    pub set_mode: usize,
    pub set_attribute: usize,
    pub clear_screen: unsafe extern "efiapi" fn(*mut SimpleTextOutput) -> Status,
    pub set_cursor_position: usize,
    pub enable_cursor: usize,
    pub mode: *mut c_void,
}

impl SimpleTextOutput {
    pub fn write(&mut self, text: &str) {
        let mut buffer = [0u16; 256];
        let mut count = 0;
        for character in text.chars() {
            if count + 1 >= buffer.len() {
                break;
            }
            if character == '\n' {
                buffer[count] = b'\r' as u16;
                count += 1;
            }
            buffer[count] = character as u16;
            count += 1;
        }
        buffer[count] = 0;
        // SAFETY: this is the firmware's own output protocol and the string is
        // NUL-terminated in a stack buffer that outlives the call.
        unsafe {
            (self.output_string)(self, buffer.as_ptr());
        }
    }
}

/// The subset of the boot services table that Zirvium uses.  Fields are in
/// the exact order defined by the UEFI specification; unused entries are
/// represented as `usize` so the layout stays correct.
#[repr(C)]
pub struct BootServices {
    pub header: TableHeader,
    pub raise_tpl: usize,
    pub restore_tpl: usize,
    pub allocate_pages: usize,
    pub free_pages: usize,
    pub get_memory_map:
        unsafe extern "efiapi" fn(*mut usize, *mut MemoryDescriptor, *mut usize, *mut usize, *mut u32) -> Status,
    pub allocate_pool: unsafe extern "efiapi" fn(u32, usize, *mut *mut c_void) -> Status,
    pub free_pool: unsafe extern "efiapi" fn(*mut c_void) -> Status,
    pub create_event: usize,
    pub set_timer: usize,
    pub wait_for_event: usize,
    pub signal_event: usize,
    pub close_event: usize,
    pub check_event: usize,
    pub install_protocol_interface: usize,
    pub reinstall_protocol_interface: usize,
    pub uninstall_protocol_interface: usize,
    pub handle_protocol: usize,
    pub reserved: usize,
    pub register_protocol_notify: usize,
    pub locate_handle: usize,
    pub locate_device_path: usize,
    pub install_configuration_table: usize,
    pub load_image: usize,
    pub start_image: usize,
    pub exit: usize,
    pub unload_image: usize,
    pub exit_boot_services: unsafe extern "efiapi" fn(Handle, usize) -> Status,
    pub get_next_monotonic_count: usize,
    pub stall: unsafe extern "efiapi" fn(usize) -> Status,
    pub set_watchdog_timer: unsafe extern "efiapi" fn(usize, u64, usize, *const u16) -> Status,
    pub connect_controller: usize,
    pub disconnect_controller: usize,
    pub open_protocol: usize,
    pub close_protocol: usize,
    pub open_protocol_information: usize,
    pub protocols_per_handle: usize,
    pub locate_handle_buffer: usize,
    pub locate_protocol: unsafe extern "efiapi" fn(*const Guid, *mut c_void, *mut *mut c_void) -> Status,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct MemoryDescriptor {
    pub memory_type: u32,
    pub _pad: u32,
    pub physical_start: u64,
    pub virtual_start: u64,
    pub number_of_pages: u64,
    pub attributes: u64,
}

impl MemoryDescriptor {
    pub fn end(&self) -> u64 {
        self.physical_start + self.number_of_pages * 4096
    }
}

pub mod memory_type {
    pub const RESERVED: u32 = 0;
    pub const LOADER_CODE: u32 = 1;
    pub const LOADER_DATA: u32 = 2;
    pub const BOOT_SERVICES_CODE: u32 = 3;
    pub const BOOT_SERVICES_DATA: u32 = 4;
    pub const RUNTIME_SERVICES_CODE: u32 = 5;
    pub const RUNTIME_SERVICES_DATA: u32 = 6;
    pub const CONVENTIONAL: u32 = 7;
    pub const UNUSABLE: u32 = 8;
    pub const ACPI_RECLAIM: u32 = 9;
    pub const ACPI_NVS: u32 = 10;
    pub const MMIO: u32 = 11;
    pub const MMIO_PORT: u32 = 12;
    pub const PERSISTENT: u32 = 14;
    pub const UNACCEPTED: u32 = 15;
}

pub const PAGE_SIZE: u64 = 4096;
const MEMORY_MAP_CAPACITY: usize = 256 * 1024;

#[repr(C, align(4096))]
struct MemoryMapBuffer([u8; MEMORY_MAP_CAPACITY]);

static mut MEMORY_MAP_BUFFER: MemoryMapBuffer = MemoryMapBuffer([0; MEMORY_MAP_CAPACITY]);

/// Information captured while boot services are still available.
pub struct BootInfo {
    pub system_table: *mut SystemTable,
    pub image_handle: Handle,
    pub image_base: u64,
    pub image_size: u64,
    pub memory_map: *const u8,
    pub memory_map_len: usize,
    pub memory_descriptor_size: usize,
    pub memory_map_key: usize,
    pub framebuffer: gop::Framebuffer,
    pub acpi_rsdp: u64,
}

impl BootInfo {
    pub fn descriptors(&self) -> MemoryMapIter<'_> {
        MemoryMapIter {
            pointer: self.memory_map,
            remaining: self.memory_map_len,
            stride: self.memory_descriptor_size,
            _marker: core::marker::PhantomData,
        }
    }
}

pub struct MemoryMapIter<'a> {
    pointer: *const u8,
    remaining: usize,
    stride: usize,
    _marker: core::marker::PhantomData<&'a MemoryDescriptor>,
}

impl<'a> Iterator for MemoryMapIter<'a> {
    type Item = &'a MemoryDescriptor;

    fn next(&mut self) -> Option<Self::Item> {
        if self.remaining < self.stride || self.stride < core::mem::size_of::<MemoryDescriptor>() {
            return None;
        }
        // SAFETY: the firmware wrote `remaining / stride` descriptors into the
        // static buffer, and they remain valid after ExitBootServices.
        let descriptor = unsafe { &*(self.pointer as *const MemoryDescriptor) };
        self.pointer = unsafe { self.pointer.add(self.stride) };
        self.remaining -= self.stride;
        Some(descriptor)
    }
}

/// Capture the memory map, framebuffer and image information.
pub fn init(image_handle: Handle, system_table: *mut SystemTable) -> BootInfo {
    // SAFETY: the firmware passes a valid system table and image handle.
    let table = unsafe { &mut *system_table };
    let boot_services = table.boot_services;
    // SAFETY: boot services are valid until ExitBootServices.
    let services = unsafe { &mut *boot_services };

    // SAFETY: disarming the watchdog is a standard part of taking over.
    unsafe {
        (services.set_watchdog_timer)(0, 0, 0, ptr::null());
    }

    let (memory_map_len, descriptor_size, map_key) = get_memory_map(services);
    let framebuffer = gop::init(services, system_table);
    let (image_base, image_size) = image_range();
    let acpi_rsdp = find_acpi_rsdp(table);

    serial::write_str("UEFI: memory map and GOP captured\n");

    BootInfo {
        system_table,
        image_handle,
        image_base,
        image_size,
        // SAFETY: reading the address of the static buffer is sound.
        memory_map: unsafe { ptr::addr_of!(MEMORY_MAP_BUFFER.0) } as *const u8,
        memory_map_len,
        memory_descriptor_size: descriptor_size,
        memory_map_key: map_key,
        framebuffer,
        acpi_rsdp,
    }
}

fn get_memory_map(services: &mut BootServices) -> (usize, usize, usize) {
    let mut size = MEMORY_MAP_CAPACITY;
    let mut key = 0usize;
    let mut descriptor_size = 0usize;
    let mut descriptor_version = 0u32;
    // SAFETY: the static buffer is large enough for the firmware's map; if it
    // is not, the firmware returns BUFFER_TOO_SMALL and we report it.
    let status = unsafe {
        (services.get_memory_map)(
            &mut size,
            ptr::addr_of_mut!(MEMORY_MAP_BUFFER.0) as *mut MemoryDescriptor,
            &mut key,
            &mut descriptor_size,
            &mut descriptor_version,
        )
    };
    if is_error(status) {
        serial::write_str("UEFI: GetMemoryMap failed\n");
        panic!("GetMemoryMap failed");
    }
    if descriptor_size < core::mem::size_of::<MemoryDescriptor>() {
        panic!("invalid memory descriptor size");
    }
    (size, descriptor_size, key)
}

/// Exit boot services, retrying once if the map key changed.
pub fn exit_boot_services(info: &mut BootInfo) {
    // SAFETY: boot services are still valid here.
    let table = unsafe { &*info.system_table };
    let services = unsafe { &*table.boot_services };
    for attempt in 0..4 {
        let status = unsafe { (services.exit_boot_services)(info.image_handle, info.memory_map_key) };
        if status == EFI_SUCCESS {
            serial::write_str("UEFI: boot services exited\n");
            return;
        }
        serial::write_str("UEFI: ExitBootServices retry\n");
        if attempt == 3 {
            panic!("ExitBootServices failed");
        }
        // The memory map key changed; refresh it.  This call also updates the
        // static buffer, but the descriptor pointer stays valid.
        let mut size = MEMORY_MAP_CAPACITY;
        let mut key = info.memory_map_key;
        let mut descriptor_size = info.memory_descriptor_size;
        let mut descriptor_version = 0u32;
        let status = unsafe {
            (services.get_memory_map)(
                &mut size,
                ptr::addr_of_mut!(MEMORY_MAP_BUFFER.0) as *mut MemoryDescriptor,
                &mut key,
                &mut descriptor_size,
                &mut descriptor_version,
            )
        };
        if is_error(status) {
            panic!("GetMemoryMap retry failed");
        }
        info.memory_map_key = key;
        info.memory_map_len = size;
        info.memory_descriptor_size = descriptor_size;
    }
}

fn image_range() -> (u64, u64) {
    // rust-lld defines __ImageBase for PE/COFF images.  If that ever changes,
    // fall back to the address of efi_main aligned down to a 64 KiB boundary.
    unsafe extern "C" {
        static __ImageBase: u8;
    }
    let base = ptr::addr_of!(__ImageBase) as u64;
    let mut size = 16 * 1024 * 1024;
    // SAFETY: reading the loaded PE headers is safe; they are part of our
    // image and mapped by the firmware.
    unsafe {
        let dos = base as *const u8;
        let pe_offset = ptr::read_unaligned(dos.add(0x3c) as *const u32) as usize;
        let pe = dos.add(pe_offset);
        if ptr::read_unaligned(pe as *const u32) == 0x0000_4550 {
            let optional = pe.add(24);
            let magic = ptr::read_unaligned(optional as *const u16);
            if magic == 0x20b {
                // PE32+ optional header: SizeOfImage is at offset 56.
                size = ptr::read_unaligned(optional.add(56) as *const u32) as u64;
            }
        }
    }
    (base, size)
}

fn find_acpi_rsdp(table: &SystemTable) -> u64 {
    const ACPI_20_GUID: Guid = Guid::new(
        0x8868_e871,
        0xe4f1,
        0x4b88,
        [0x81, 0x9f, 0x1c, 0x3b, 0x8b, 0x2c, 0x0c, 0x30],
    );
    if table.number_of_table_entries == 0 || table.configuration_table.is_null() {
        return 0;
    }
    for index in 0..table.number_of_table_entries {
        // SAFETY: the firmware's configuration table array has the advertised
        // number of entries.
        let entry = unsafe { &*table.configuration_table.add(index) };
        if entry.vendor_guid == ACPI_20_GUID {
            return entry.vendor_table as u64;
        }
    }
    0
}

/// Print to the firmware console before ExitBootServices.
pub fn console_write(system_table: *mut SystemTable, text: &str) {
    // SAFETY: only called before ExitBootServices.
    unsafe {
        if let Some(console) = (*system_table).console_out.as_mut() {
            console.write(text);
        }
    }
}
