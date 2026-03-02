/// IGS PGM System Decryption (Rust rewrite of pgm_crypt.cpp)
///
/// Organized into sub-modules:
///   - pgm_rom   : decrypts using PGM68KROM (68K main ROM)
///   - user_rom  : decrypts using PGMUSER0 (external ARM ROM)
///   - bootleg   : bootleg/complex decrypts using bit-reorder operations

pub mod pgm_rom;
pub mod user_rom;
pub mod bootleg;

// ============================================================================
// Shared C FFI declarations
// ============================================================================

/// C external variables shared across sub-modules.
pub(crate) mod ffi {
    unsafe extern "C" {
        pub static mut PGM68KROM: *mut u8;
        pub static nPGM68KROMLen: i32;
        pub static mut PGMUSER0: *mut u8;
        pub static nPGMExternalARMLen: i32;
        pub static mut PGMSPRMaskROM: *mut u8;
        pub static mut ICSSNDROM: *mut u8;
    }
}

// ============================================================================
// Shared bit-manipulation helpers
// ============================================================================

/// Extract bit `b` from value `n` (0-based, LSB = bit 0).
#[inline(always)]
pub(crate) const fn bit(n: u32, b: u32) -> u32 {
    (n >> b) & 1
}

/// Reorder bits of a 16-bit value.
/// Bit positions listed from MSB (b15) to LSB (b0).
#[inline(always)]
#[allow(clippy::too_many_arguments)]
pub(crate) const fn bitswap16(
    n: u16,
    b15: u32, b14: u32, b13: u32, b12: u32,
    b11: u32, b10: u32, b9: u32,  b8: u32,
    b7: u32,  b6: u32,  b5: u32,  b4: u32,
    b3: u32,  b2: u32,  b1: u32,  b0: u32,
) -> u16 {
    let n = n as u32;
    ((bit(n, b15) << 15)
        | (bit(n, b14) << 14)
        | (bit(n, b13) << 13)
        | (bit(n, b12) << 12)
        | (bit(n, b11) << 11)
        | (bit(n, b10) << 10)
        | (bit(n, b9) << 9)
        | (bit(n, b8) << 8)
        | (bit(n, b7) << 7)
        | (bit(n, b6) << 6)
        | (bit(n, b5) << 5)
        | (bit(n, b4) << 4)
        | (bit(n, b3) << 3)
        | (bit(n, b2) << 2)
        | (bit(n, b1) << 1)
        | bit(n, b0)) as u16
}

/// Reorder bits of a 24-bit value.
/// Bit positions listed from MSB (b23) to LSB (b0).
#[inline(always)]
#[allow(clippy::too_many_arguments)]
pub(crate) const fn bitswap24(
    n: u32,
    b23: u32, b22: u32, b21: u32, b20: u32,
    b19: u32, b18: u32, b17: u32, b16: u32,
    b15: u32, b14: u32, b13: u32, b12: u32,
    b11: u32, b10: u32, b9: u32,  b8: u32,
    b7: u32,  b6: u32,  b5: u32,  b4: u32,
    b3: u32,  b2: u32,  b1: u32,  b0: u32,
) -> u32 {
    (bit(n, b23) << 23)
        | (bit(n, b22) << 22)
        | (bit(n, b21) << 21)
        | (bit(n, b20) << 20)
        | (bit(n, b19) << 19)
        | (bit(n, b18) << 18)
        | (bit(n, b17) << 17)
        | (bit(n, b16) << 16)
        | (bit(n, b15) << 15)
        | (bit(n, b14) << 14)
        | (bit(n, b13) << 13)
        | (bit(n, b12) << 12)
        | (bit(n, b11) << 11)
        | (bit(n, b10) << 10)
        | (bit(n, b9) << 9)
        | (bit(n, b8) << 8)
        | (bit(n, b7) << 7)
        | (bit(n, b6) << 6)
        | (bit(n, b5) << 5)
        | (bit(n, b4) << 4)
        | (bit(n, b3) << 3)
        | (bit(n, b2) << 2)
        | (bit(n, b1) << 1)
        | bit(n, b0)
}

/// Reorder 8 bits of a value.
/// Bit positions listed from MSB (b7) to LSB (b0).
#[inline(always)]
pub(crate) const fn bitswap08(
    n: u32,
    b7: u32, b6: u32, b5: u32, b4: u32,
    b3: u32, b2: u32, b1: u32, b0: u32,
) -> u32 {
    (bit(n, b7) << 7)
        | (bit(n, b6) << 6)
        | (bit(n, b5) << 5)
        | (bit(n, b4) << 4)
        | (bit(n, b3) << 3)
        | (bit(n, b2) << 2)
        | (bit(n, b1) << 1)
        | bit(n, b0)
}
