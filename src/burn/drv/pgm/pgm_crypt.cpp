// IGS PGM System Decryptions – Rust bridge
//
// All decryption logic has been migrated to Rust (src/crypt/).
// Function declarations with C linkage are in pgm.h.
// This file is intentionally empty – the Rust library provides
// all pgm_decrypt_* and pgm_decode_* symbols directly.

#include "pgm.h"
