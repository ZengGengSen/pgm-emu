/// Bootleg / complex decryption routines (originally in pgm_crypt.cpp).
///
/// These operations rearrange bytes within large ROM blocks using
/// bitswap index permutations, matching the bootleg PCB hardware.

use core::slice;

use super::{bitswap08, bitswap16, bitswap24, ffi::{ICSSNDROM, PGM68KROM, PGMSPRMaskROM}};

// ============================================================================
// Helper – slice helpers
// ============================================================================

unsafe fn pgm68k_words(len: usize) -> &'static mut [u16] {
    let ptr = unsafe { PGM68KROM } as *mut u16;
    unsafe { slice::from_raw_parts_mut(ptr, len) }
}

unsafe fn spr_mask_bytes(offset: usize, len: usize) -> &'static mut [u8] {
    let ptr = unsafe { PGMSPRMaskROM.add(offset) };
    unsafe { slice::from_raw_parts_mut(ptr, len) }
}

unsafe fn ics_snd_bytes(len: usize) -> &'static mut [u8] {
    let ptr = unsafe { ICSSNDROM };
    unsafe { slice::from_raw_parts_mut(ptr, len) }
}

// ============================================================================
// Happy 6-in-1 data descramble
// ============================================================================

/// Descramble one 0x800000-byte block of sprite colour data in-place.
fn descramble_happy6_block(src: &mut [u8]) {
    let mut buffer = vec![0u8; 0x800000];
    for i in 0..0x800000usize {
        let j = (i & 0xf8c01ff)
            | ((i >> 12) & 0x600)
            | ((i << 2) & 0x43f800)
            | ((i << 4) & 0x300000);
        buffer[i] = src[j];
    }
    src.copy_from_slice(&buffer);
}

/// # Safety
/// `gfx` must point to at least `len` bytes of valid writable memory.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn pgm_descramble_happy6_data(gfx: *mut u8, len: i32) {
    let data = unsafe { slice::from_raw_parts_mut(gfx, len as usize) };
    let mut offset = 0usize;
    while offset + 0x800000 <= data.len() {
        descramble_happy6_block(&mut data[offset..offset + 0x800000]);
        offset += 0x800000;
    }
}

// ============================================================================
// KOV QHSGS / LSQH2 / KOVLSQHO GFX block decode (shared helper)
// ============================================================================

fn decode_kovqhsgs_gfx_block(src: &mut [u8]) {
    let mut dec = vec![0u8; 0x800000];
    for i in 0..0x800000usize {
        let j = bitswap24(
            i as u32,
            23, 10, 9, 22, 19, 18, 20, 21,
            17, 16, 15, 14, 13, 12, 11, 8,
            7, 6, 5, 4, 3, 2, 1, 0,
        ) as usize;
        dec[j] = src[i];
    }
    src.copy_from_slice(&dec);
}

/// # Safety
/// `gfx` must point to at least `len` bytes of valid writable memory,
/// where `len` is a multiple of 0x800000.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn pgm_decode_kovqhsgs_gfx(gfx: *mut u8, len: i32) {
    let data = unsafe { slice::from_raw_parts_mut(gfx, len as usize) };
    let mut offset = 0usize;
    while offset + 0x800000 <= data.len() {
        decode_kovqhsgs_gfx_block(&mut data[offset..offset + 0x800000]);
        offset += 0x800000;
    }
}

// ============================================================================
// KOV QHSGS tile data decode
// ============================================================================

/// # Safety
/// `source` must point to at least `len` bytes of valid writable memory.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn pgm_decode_kovqhsgs_tile_data(source: *mut u8, len: i32) {
    let len = len as usize;
    let src = unsafe { slice::from_raw_parts_mut(source as *mut u16, len / 2) };
    let mut dst = vec![0u16; len / 2];

    for i in 0..(len / 2) {
        let j = bitswap24(
            i as u32,
            23, 22, 9, 8, 21, 18, 0, 1, 2, 3,
            16, 15, 14, 13, 12, 11, 10, 19, 20, 17,
            7, 6, 5, 4,
        ) as usize;
        dst[j] = bitswap16(
            src[i],
            1, 14, 8, 7, 0, 15, 6, 9, 13, 2, 5, 10, 12, 3, 4, 11,
        );
    }

    src.copy_from_slice(&dst);
}

// ============================================================================
// KOV QHSGS samples shuffle
// ============================================================================

fn pgm_decode_kovqhsgs_samples() {
    // Copy 0x400000 bytes from offset 0xc00001 to offset 0x400001
    let snd = unsafe { ics_snd_bytes(0x1000002) };
    for i in (0..0x400000usize).step_by(2) {
        snd[i + 0x400001] = snd[i + 0xc00001];
    }
}

// ============================================================================
// KOV QHSGS program decrypt
// ============================================================================

fn pgm_decrypt_kovqhsgs_program() {
    let words = unsafe { pgm68k_words(0x400000 / 2) };
    let mut dst = vec![0u16; 0x400000 / 2];

    for i in 0..(0x400000 / 2usize) {
        let j = bitswap24(
            i as u32,
            23, 22, 21, 20, 19, 18, 17, 16,
            15, 14, 13, 12, 11, 10, 9, 8,
            6, 7, 5, 4, 3, 2, 1, 0,
        ) as usize;
        dst[j] = bitswap16(words[i], 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 4, 5, 3, 2, 1, 0);
    }

    words.copy_from_slice(&dst);
}

#[unsafe(no_mangle)]
pub extern "C" fn pgm_decrypt_kovqhsgs() {
    pgm_decrypt_kovqhsgs_program();

    let spr0 = unsafe { spr_mask_bytes(0x000000, 0x800000) };
    decode_kovqhsgs_gfx_block(spr0);
    let spr1 = unsafe { spr_mask_bytes(0x800000, 0x800000) };
    decode_kovqhsgs_gfx_block(spr1);

    // sprite colors are decoded in pgm_run
    pgm_decode_kovqhsgs_samples();
}

// ============================================================================
// KOV LSQH2
// ============================================================================

fn pgm_decrypt_kovlsqh2_program() {
    let words = unsafe { pgm68k_words(0x400000 / 2) };
    let mut dst = vec![0u16; 0x400000 / 2];

    for i in 0..(0x400000 / 2usize) {
        let j = bitswap24(
            i as u32,
            23, 22, 21, 20, 19, 16, 15, 14,
            13, 12, 11, 10, 9, 8, 0, 1,
            2, 3, 4, 5, 6, 18, 17, 7,
        ) as usize;
        dst[j] = words[i];
    }

    words.copy_from_slice(&dst);
}

#[unsafe(no_mangle)]
pub extern "C" fn pgm_decrypt_kovlsqh2() {
    pgm_decrypt_kovlsqh2_program();

    let spr0 = unsafe { spr_mask_bytes(0x000000, 0x800000) };
    decode_kovqhsgs_gfx_block(spr0);
    let spr1 = unsafe { spr_mask_bytes(0x800000, 0x800000) };
    decode_kovqhsgs_gfx_block(spr1);

    // sprite colors are decoded in pgm_run.cpp
    pgm_decode_kovqhsgs_samples();
}

// ============================================================================
// KOV ASSG Plus
// ============================================================================

fn pgm_decrypt_kovassgplus_program() {
    const ROM_LEN: usize = 0x400000;
    let words = unsafe { pgm68k_words(ROM_LEN / 2) };
    let mut dst = vec![0u16; ROM_LEN / 2];

    for i in 0..(ROM_LEN / 2) {
        let addr_low = bitswap16(i as u16, 15, 14, 13, 12, 11, 10, 7, 3, 1, 9, 4, 8, 6, 0, 2, 5)
            ^ 0x019c;
        let j = ((i as u32 & !0xffff) | addr_low as u32) as usize;
        dst[i] = bitswap16(words[j], 13, 9, 10, 11, 2, 0, 12, 5, 4, 1, 14, 8, 15, 6, 3, 7)
            ^ 0x9d05;
    }

    words.copy_from_slice(&dst);

    // Additional encryption for 0x300000..0x3f0000 region
    let start = 0x300000 / 2;
    let end = 0x3f0000 / 2;
    let words2 = unsafe { pgm68k_words(ROM_LEN / 2) };
    let mut dst2 = vec![0u16; ROM_LEN / 2];
    dst2[start..end].copy_from_slice(&words2[start..end]);

    for i in start..end {
        let addr_perm = bitswap16(i as u16, 15, 14, 13, 12, 11, 10, 0, 8, 7, 6, 5, 9, 2, 4, 1, 3)
            ^ 0x00c5;
        let addr_low = u16::swap_bytes(addr_perm);
        let j = ((i as u32 & !0xffff) | addr_low as u32) as usize;
        dst2[i] = bitswap16(
            words2[j] ^ 0xffd1,
            5, 11, 3, 7, 15, 10, 2, 12, 14, 1, 4, 8, 6, 0, 13, 9,
        );
    }

    words2[start..end].copy_from_slice(&dst2[start..end]);
}

#[unsafe(no_mangle)]
pub extern "C" fn pgm_decrypt_kovassgplus() {
    pgm_decrypt_kovassgplus_program();

    let spr0 = unsafe { spr_mask_bytes(0x000000, 0x800000) };
    decode_kovqhsgs_gfx_block(spr0);
    let spr1 = unsafe { spr_mask_bytes(0x800000, 0x800000) };
    decode_kovqhsgs_gfx_block(spr1);

    // sprite colors are decoded in pgm_run.cpp
    pgm_decode_kovqhsgs_samples();
}

// ============================================================================
// KOV ASSGE
// ============================================================================

fn pgm_decrypt_kovassge_program() {
    const ROM_LEN: usize = 0x400000;
    let words = unsafe { pgm68k_words(ROM_LEN / 2) };
    let mut dst = vec![0u16; ROM_LEN / 2];

    for i in 0..(ROM_LEN / 2) {
        let addr_low = bitswap16(i as u16, 15, 14, 13, 12, 11, 10, 5, 0, 3, 4, 1, 7, 8, 6, 2, 9)
            ^ 0x00f9;
        let j = ((i as u32 & !0xffff) | addr_low as u32) as usize;
        dst[i] = bitswap16(words[j] ^ 0x43df, 4, 7, 11, 2, 5, 15, 10, 12, 0, 13, 3, 6, 1, 14, 8, 9);
    }

    words.copy_from_slice(&dst);

    // Additional encryption for 0x300000..0x3f0000 region
    let start = 0x300000 / 2;
    let end = 0x3f0000 / 2;
    let words2 = unsafe { pgm68k_words(ROM_LEN / 2) };
    let mut dst2 = vec![0u16; ROM_LEN / 2];
    dst2[start..end].copy_from_slice(&words2[start..end]);

    for i in start..end {
        let addr_low = bitswap16(i as u16, 15, 14, 13, 12, 11, 10, 7, 9, 5, 4, 6, 1, 2, 0, 8, 3)
            ^ 0x00cf;
        let j = ((i as u32 & !0xffff) | addr_low as u32) as usize;
        dst2[i] = bitswap16(
            words2[j] ^ 0x107d,
            9, 15, 14, 7, 10, 6, 12, 4, 2, 0, 8, 11, 3, 13, 1, 5,
        );
    }

    words2[start..end].copy_from_slice(&dst2[start..end]);
}

#[unsafe(no_mangle)]
pub extern "C" fn pgm_decrypt_kovassge() {
    pgm_decrypt_kovassge_program();
}

// ============================================================================
// KOV ASSGN
// ============================================================================

fn pgm_decrypt_kovassgn_program() {
    const ROM_LEN: usize = 0x400000;
    let words = unsafe { pgm68k_words(ROM_LEN / 2) };
    let mut dst = vec![0u16; ROM_LEN / 2];

    for i in 0..(ROM_LEN / 2) {
        // 0x1fff00: preserve bits 8–20 (the upper address portion), permute only bits 0–7
        let j = (i & 0x1fff00) | bitswap08(i as u32, 6, 7, 5, 4, 3, 2, 1, 0) as usize;
        dst[i] = bitswap16(words[j], 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 4, 5, 3, 2, 1, 0);
    }

    words.copy_from_slice(&dst);
}

#[unsafe(no_mangle)]
pub extern "C" fn pgm_decrypt_kovassgn() {
    pgm_decrypt_kovassgn_program();
}

#[unsafe(no_mangle)]
pub extern "C" fn pgm_decrypt_kovlsqho() {
    pgm_decrypt_kovassgn_program();

    let spr0 = unsafe { spr_mask_bytes(0x000000, 0x800000) };
    decode_kovqhsgs_gfx_block(spr0);
    let spr1 = unsafe { spr_mask_bytes(0x800000, 0x800000) };
    decode_kovqhsgs_gfx_block(spr1);

    // sprite colors are decoded in pgm_run.cpp
    pgm_decode_kovqhsgs_samples();
}

// ============================================================================
// KOV GSYX
// ============================================================================

fn pgm_decrypt_kovgsyx_program_inner() {
    const ROM_LEN: usize = 0x400000;
    let words = unsafe { pgm68k_words(ROM_LEN / 2) };
    let mut dst = vec![0u16; ROM_LEN / 2];

    for i in 0..(ROM_LEN / 2usize) {
        let base = (i as u32 & 0x7ffff) | (((((i as u32 >> 19) + 1) & 3) << 19));
        let j = bitswap24(
            base,
            23, 22, 21, 20, 19,
            18, 16, 1, 3, 5, 7, 9, 11, 13, 15, 17,
            14, 12, 10, 8, 6, 4, 0, 2,
        ) as usize;
        dst[i] = bitswap16(words[j], 15, 0, 10, 12, 3, 4, 11, 5, 2, 13, 9, 6, 1, 14, 8, 7);
    }

    words.copy_from_slice(&dst);
}

/// Public C-callable entry. The C++ driver calls this via `pgm_decrypt_kovgsyx_program`.
#[unsafe(no_mangle)]
pub extern "C" fn pgm_decrypt_kovgsyx_program() {
    pgm_decrypt_kovgsyx_program_inner();
}

#[unsafe(no_mangle)]
pub extern "C" fn pgm_decrypt_kovgsyx() {
    pgm_decrypt_kovgsyx_program_inner();
    // graphics when dumped?
}

// ============================================================================
// KOV FYZQ 4-in-1 (just the GFX decode + samples)
// ============================================================================

#[unsafe(no_mangle)]
pub extern "C" fn pgm_decrypt_kovfyzq4in1() {
    // program ROM decryption is not yet implemented for this variant
    // (pgm_decrypt_kovqhsgs_program is intentionally commented out in original)

    let spr0 = unsafe { spr_mask_bytes(0x000000, 0x800000) };
    decode_kovqhsgs_gfx_block(spr0);
    let spr1 = unsafe { spr_mask_bytes(0x800000, 0x800000) };
    decode_kovqhsgs_gfx_block(spr1);

    // sprite colors are decoded in pgm_run
    pgm_decode_kovqhsgs_samples();
}

// ============================================================================
// Unit Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;
    use crate::crypt::{bitswap16, bitswap24};

    #[test]
    fn bitswap16_identity() {
        // Identity permutation: bitswap16(v, 15..0) == v
        assert_eq!(
            bitswap16(0xabcd, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0),
            0xabcd
        );
    }

    #[test]
    fn bitswap24_identity() {
        let v: u32 = 0x123456;
        assert_eq!(
            bitswap24(v, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0),
            v
        );
    }

    #[test]
    fn bitswap08_swap_bits6_7() {
        use crate::crypt::bitswap08;
        // Swap bits 6 and 7 of 0b11000000 = 0xc0
        // bitswap08(0xc0, 6,7,5,4,3,2,1,0) should put bit-6 at pos-7 and bit-7 at pos-6
        let v: u32 = 0b11000000;
        // original: bit7=1, bit6=1 – swap is still 0xc0
        assert_eq!(bitswap08(v, 6, 7, 5, 4, 3, 2, 1, 0), 0b11000000);
        let v2: u32 = 0b01000000; // only bit6 set
        // After swap: bit6 goes to position 7, so result is 0b10000000
        assert_eq!(bitswap08(v2, 6, 7, 5, 4, 3, 2, 1, 0), 0b10000000);
    }

    #[test]
    fn descramble_happy6_round_trip_no_change_on_identity() {
        // descramble_happy6_block permutes indices; on a block of all-zero bytes
        // the output should still be all zeros.
        let mut buf = vec![0u8; 0x800000];
        descramble_happy6_block(&mut buf);
        assert!(buf.iter().all(|&b| b == 0));
    }
}
