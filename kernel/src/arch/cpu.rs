//! CPU control, feature detection and cycle counting.

use core::arch::{asm, x86_64};

#[inline]
pub fn cli() {
    // SAFETY: disabling interrupts is always permitted in ring 0.
    unsafe { asm!("cli", options(nomem, nostack)) };
}

#[inline]
pub fn sti() {
    // SAFETY: enabling interrupts is always permitted in ring 0.
    unsafe { asm!("sti", options(nomem, nostack)) };
}

#[inline]
pub fn halt() {
    // SAFETY: `hlt` is privileged but we run in ring 0.
    unsafe { asm!("hlt", options(nomem, nostack)) };
}

#[inline]
pub fn hlt_loop() -> ! {
    loop {
        halt();
    }
}

/// Wait for an interrupt with interrupts enabled.
#[inline]
pub fn idle() {
    sti();
    halt();
}

#[inline]
pub fn read_cr0() -> u64 {
    let value: u64;
    // SAFETY: reading CR0 is permitted in ring 0.
    unsafe { asm!("mov {}, cr0", out(reg) value, options(nomem, nostack, preserves_flags)) };
    value
}

#[inline]
pub fn write_cr0(value: u64) {
    // SAFETY: the caller is responsible for preserving required CR0 bits.
    unsafe { asm!("mov cr0, {}", in(reg) value, options(nomem, nostack, preserves_flags)) };
}

#[inline]
pub fn read_cr2() -> u64 {
    let value: u64;
    // SAFETY: reading CR2 is permitted in ring 0.
    unsafe { asm!("mov {}, cr2", out(reg) value, options(nomem, nostack, preserves_flags)) };
    value
}

#[inline]
pub fn read_cr3() -> u64 {
    let value: u64;
    // SAFETY: reading CR3 is permitted in ring 0.
    unsafe { asm!("mov {}, cr3", out(reg) value, options(nomem, nostack, preserves_flags)) };
    value
}

#[inline]
pub fn write_cr3(value: u64) {
    // SAFETY: the caller must install a valid page table.
    unsafe { asm!("mov cr3, {}", in(reg) value, options(nomem, nostack, preserves_flags)) };
}

#[inline]
pub fn read_cr4() -> u64 {
    let value: u64;
    // SAFETY: reading CR4 is permitted in ring 0.
    unsafe { asm!("mov {}, cr4", out(reg) value, options(nomem, nostack, preserves_flags)) };
    value
}

#[inline]
pub fn write_cr4(value: u64) {
    // SAFETY: the caller is responsible for preserving required CR4 bits.
    unsafe { asm!("mov cr4, {}", in(reg) value, options(nomem, nostack, preserves_flags)) };
}

#[inline]
pub fn rdtsc() -> u64 {
    // SAFETY: RDTSC is unprivileged on every x86-64 CPU.
    unsafe { x86_64::_rdtsc() }
}

/// Read the current RFLAGS register.
#[inline]
pub fn rflags() -> u64 {
    let value: u64;
    // SAFETY: reading flags has no side effects.
    unsafe {
        asm!("pushfq", "pop {}", out(reg) value, options(nomem, preserves_flags));
    }
    value
}

/// Are maskable interrupts currently enabled?
#[inline]
pub fn interrupts_enabled() -> bool {
    rflags() & (1 << 9) != 0
}

/// Enable or disable interrupts, returning the previous state.
#[inline]
pub fn set_interrupts(enabled: bool) -> bool {
    let were_enabled = interrupts_enabled();
    if enabled {
        sti();
    } else {
        cli();
    }
    were_enabled
}

#[inline]
pub fn pause() {
    // SAFETY: PAUSE is a hint with no side effects.
    unsafe { asm!("pause", options(nomem, nostack, preserves_flags)) };
}

#[derive(Clone, Copy, Debug, Default)]
pub struct CpuFeatures {
    pub sse2: bool,
    pub sse3: bool,
    pub ssse3: bool,
    pub sse4_1: bool,
    pub sse4_2: bool,
    pub avx: bool,
    pub avx2: bool,
    pub osxsave: bool,
    pub xsave: bool,
    pub fsgsbase: bool,
    pub invariant_tsc: bool,
    pub hypervisor: bool,
}

pub fn cpuid(leaf: u32, subleaf: u32) -> x86_64::CpuidResult {
    x86_64::__cpuid_count(leaf, subleaf)
}

pub fn vendor() -> [u8; 12] {
    let result = cpuid(0, 0);
    let mut vendor = [0u8; 12];
    vendor[0..4].copy_from_slice(&result.ebx.to_le_bytes());
    vendor[4..8].copy_from_slice(&result.edx.to_le_bytes());
    vendor[8..12].copy_from_slice(&result.ecx.to_le_bytes());
    vendor
}

pub fn vendor_str() -> &'static str {
    match &vendor() {
        b"GenuineIntel" => "GenuineIntel",
        b"AuthenticAMD" => "AuthenticAMD",
        b"KVMKVMKVM\0\0\0" => "KVM",
        b"TCGTCGTCGTCG" => "QEMU TCG",
        _ => "Unknown",
    }
}

pub fn features() -> CpuFeatures {
    let leaf1 = cpuid(1, 0);
    let leaf7 = cpuid(7, 0);
    let ext = cpuid(0x8000_0001, 0);
    let mut features = CpuFeatures {
        sse2: leaf1.edx & (1 << 26) != 0,
        sse3: leaf1.ecx & (1 << 0) != 0,
        ssse3: leaf1.ecx & (1 << 9) != 0,
        sse4_1: leaf1.ecx & (1 << 19) != 0,
        sse4_2: leaf1.ecx & (1 << 20) != 0,
        avx: leaf1.ecx & (1 << 28) != 0,
        osxsave: leaf1.ecx & (1 << 27) != 0,
        xsave: leaf1.ecx & (1 << 26) != 0,
        avx2: leaf7.ebx & (1 << 5) != 0,
        fsgsbase: leaf7.ebx & (1 << 0) != 0,
        invariant_tsc: ext.edx & (1 << 8) != 0,
        hypervisor: leaf1.ecx & (1 << 31) != 0,
    };
    if !features.osxsave {
        features.avx = false;
        features.avx2 = false;
    }
    features
}

/// Number of logical processors reported by CPUID.
pub fn logical_processors() -> u32 {
    let leaf = cpuid(1, 0);
    ((leaf.ebx >> 16) & 0xFF).max(1)
}

/// Enable SSE, SSE2 and the OS support bits required for FXSAVE.
pub fn enable_sse() {
    let mut cr0 = read_cr0();
    cr0 &= !(1 << 2); // EM = 0
    cr0 |= 1 << 1; // MP = 1
    write_cr0(cr0);

    let mut cr4 = read_cr4();
    cr4 |= 1 << 9; // OSFXSR
    cr4 |= 1 << 10; // OSXMMEXCPT
    write_cr4(cr4);
}

/// Enable AVX state saving when the CPU and firmware support it.
pub fn enable_avx() -> bool {
    let features = features();
    if !(features.xsave && features.osxsave && features.avx) {
        return false;
    }
    let mut cr4 = read_cr4();
    cr4 |= 1 << 18; // OSXSAVE
    write_cr4(cr4);

    // XCR0: x87 | SSE | AVX.
    // SAFETY: XSETBV is permitted when CR4.OSXSAVE is set.
    unsafe {
        let (low, high): (u32, u32);
        asm!("xgetbv", in("ecx") 0u32, out("eax") low, out("edx") high, options(nomem, nostack));
        let mut value = ((high as u64) << 32) | low as u64;
        value |= 0b111;
        asm!("xsetbv", in("ecx") 0u32, in("eax") value as u32, in("edx") (value >> 32) as u32, options(nomem, nostack));
    }
    true
}

/// A calibrated TSC-to-nanoseconds scale.  Calibrated against the PIT at boot.
#[derive(Clone, Copy, Debug, Default)]
pub struct TscClock {
    pub tsc_hz: u64,
}

impl TscClock {
    pub const fn new(tsc_hz: u64) -> Self {
        Self { tsc_hz }
    }

    pub fn nanoseconds(&self, cycles: u64) -> u64 {
        if self.tsc_hz == 0 {
            0
        } else {
            (cycles as u128 * 1_000_000_000u128 / self.tsc_hz as u128) as u64
        }
    }
}

/// A simple calibrated delay loop, used before the PIT is running.
pub fn delay_cycles(cycles: u64) {
    let start = rdtsc();
    while rdtsc().wrapping_sub(start) < cycles {
        pause();
    }
}
