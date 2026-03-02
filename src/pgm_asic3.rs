//! IGS ASIC3 emulation
//!
//! Used by: Oriental Legends

use crate::{scan_var, state::scan_var};

/// 从 n 中取第 bit 位 (0-based)
#[inline]
const fn bit(n: u32, bit: u32) -> u16 {
    ((n >> bit) & 1) as u16
}

/// BITSWAP08(n, b7, b6, b5, b4, b3, b2, b1, b0)
/// 将 n 的指定位重新排列到 8 位结果中
#[inline]
const fn bitswap08(
    n: u16,
    b7: u32,
    b6: u32,
    b5: u32,
    b4: u32,
    b3: u32,
    b2: u32,
    b1: u32,
    b0: u32,
) -> u16 {
    (bit(n as u32, b7) << 7)
        | (bit(n as u32, b6) << 6)
        | (bit(n as u32, b5) << 5)
        | (bit(n as u32, b4) << 4)
        | (bit(n as u32, b3) << 3)
        | (bit(n as u32, b2) << 2)
        | (bit(n as u32, b1) << 1)
        | bit(n as u32, b0)
}

/// 区域模式表，根据 PgmInput[7] & 7 索引
const MODES: [i32; 8] = [1, 1, 3, 2, 4, 4, 4, 4];

#[repr(C)]
pub struct Asic3 {
    latch: [u8; 3],
    x: u8,
    reg: u8,
    hold: u16,
    hilo: u16,
}

impl Asic3 {
    pub fn new() -> Self {
        Asic3 {
            latch: [0; 3],
            x: 0,
            reg: 0,
            hold: 0,
            hilo: 0,
        }
    }

    pub fn reset(&mut self) {
        self.latch = [0; 3];
        self.hold = 0;
        self.reg = 0;
        self.x = 0;
        self.hilo = 0;
    }

    fn compute_hold(&mut self, y: i32, z: u16, region: u8) {
        let old = self.hold;

        self.hold = old.rotate_left(1);
        self.hold ^= 0x2bad;
        self.hold ^= bit(z as u32, y as u32);
        self.hold ^= bit(self.x as u32, 2) << 10;
        self.hold ^= bit(old as u32, 5);

        let mode = MODES[(region & 7) as usize];
        match mode {
            1 => {
                self.hold ^= bit(old as u32, 10)
                    ^ bit(old as u32, 8)
                    ^ (bit(self.x as u32, 0) << 1)
                    ^ (bit(self.x as u32, 1) << 6)
                    ^ (bit(self.x as u32, 3) << 14);
            }
            2 => {
                self.hold ^= bit(old as u32, 7)
                    ^ bit(old as u32, 6)
                    ^ (bit(self.x as u32, 0) << 4)
                    ^ (bit(self.x as u32, 1) << 6)
                    ^ (bit(self.x as u32, 3) << 12);
            }
            3 => {
                self.hold ^= bit(old as u32, 10)
                    ^ bit(old as u32, 8)
                    ^ (bit(self.x as u32, 0) << 4)
                    ^ (bit(self.x as u32, 1) << 6)
                    ^ (bit(self.x as u32, 3) << 12);
            }
            4 => {
                self.hold ^= bit(old as u32, 7)
                    ^ bit(old as u32, 6)
                    ^ (bit(self.x as u32, 0) << 3)
                    ^ (bit(self.x as u32, 1) << 8)
                    ^ (bit(self.x as u32, 3) << 14);
            }
            _ => {}
        }
    }

    /// 读操作：对应 asic3_read_word
    /// `region` 参数对应 C++ 中的 PgmInput[7]
    pub fn read_word(&self, region: u8) -> u16 {
        match self.reg {
            0x00 => (self.latch[0] as u16 & 0xf7) | ((region as u16) << 3 & 0x08),
            0x01 => self.latch[1] as u16,
            0x02 => (self.latch[2] as u16 & 0x7f) | ((region as u16) << 6 & 0x80),
            0x03 => bitswap08(self.hold, 5, 2, 9, 7, 10, 13, 12, 15),

            // 固定身份识别值
            0x20 => 0x49,
            0x21 => 0x47,
            0x22 => 0x53,

            0x24 => 0x41,
            0x25 => 0x41,
            0x26 => 0x7f,
            0x27 => 0x41,
            0x28 => 0x41,

            0x2a => 0x3e,
            0x2b => 0x41,
            0x2c => 0x49,
            0x2d => 0xf9,
            0x2e => 0x0a,

            0x30 => 0x26,
            0x31 => 0x49,
            0x32 => 0x49,
            0x33 => 0x49,
            0x34 => 0x32,

            _ => 0,
        }
    }

    /// 写操作：对应 asic3_write_word
    /// `address` 为 M68K 总线地址, `data` 为写入数据, `region` 对应 PgmInput[7]
    pub fn write_word(&mut self, address: u32, data: u16, region: u8) {
        if address == 0xc04000 {
            self.reg = data as u8;
            return;
        }

        match self.reg {
            0x00 | 0x01 | 0x02 => {
                self.latch[self.reg as usize] = (data << 1) as u8;
            }

            0x40 => {
                self.hilo = (self.hilo << 8) | (data & 0xff);
            }

            0x48 => {
                self.x = 0;
                if (self.hilo & 0x0090) == 0 {
                    self.x |= 0x01;
                }
                if (self.hilo & 0x0006) == 0 {
                    self.x |= 0x02;
                }
                if (self.hilo & 0x9000) == 0 {
                    self.x |= 0x04;
                }
                if (self.hilo & 0x0a00) == 0 {
                    self.x |= 0x08;
                }
            }

            0x80..=0x87 => {
                self.compute_hold((self.reg & 0x07) as i32, data, region);
            }

            0xa0 => {
                self.hold = 0;
            }

            _ => {}
        }
    }
}

// ============================================================================
// C FFI 接口
// ============================================================================

// 外部 C 变量：PgmInput 数组（在 pgm_run.cpp 中定义）
unsafe extern "C" {
    static PgmInput: [u8; 9];
}

/// 从 C 端获取 region 值的辅助函数
#[inline]
fn get_region() -> u8 {
    unsafe { PgmInput[7] }
}

#[unsafe(no_mangle)]
pub extern "C" fn asic3Init() -> *mut Asic3 {
    let asic3 = Box::new(Asic3::new());
    Box::into_raw(asic3)
}

#[unsafe(no_mangle)]
pub extern "C" fn asic3Exit(handle: *mut Asic3) {
    if !handle.is_null() {
        unsafe {
            let _ = Box::from_raw(handle);
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn asic3Reset(handle: *mut Asic3) {
    if handle.is_null() {
        return;
    }
    let asic3 = unsafe { &mut *handle };
    asic3.reset();
}

/// 供 SekSetReadWordHandler 使用的回调：读 word
/// 注意：这个函数需要通过全局指针访问 Asic3 实例
/// 因为 M68K handler 回调签名是 `UINT16 __fastcall (UINT32 address)`
/// 不支持传递用户数据，所以需要一个全局 static
static mut ASIC3_INSTANCE: *mut Asic3 = std::ptr::null_mut();

#[unsafe(no_mangle)]
pub extern "C" fn asic3SetInstance(handle: *mut Asic3) {
    unsafe {
        ASIC3_INSTANCE = handle;
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn asic3ReadWord(_address: u32) -> u16 {
    let handle = unsafe { ASIC3_INSTANCE };
    if handle.is_null() {
        return 0;
    }
    let asic3 = unsafe { &*handle };
    asic3.read_word(get_region())
}

#[unsafe(no_mangle)]
pub extern "C" fn asic3WriteWord(address: u32, data: u16) {
    let handle = unsafe { ASIC3_INSTANCE };
    if handle.is_null() {
        return;
    }
    let asic3 = unsafe { &mut *handle };
    asic3.write_word(address, data, get_region());
}

#[unsafe(no_mangle)]
pub extern "C" fn asic3Scan(handle: *mut Asic3) -> i32 {
    if handle.is_null() {
        return 0;
    }

    let asic3 = unsafe { &mut *handle };

    scan_var!(asic3.reg);
    scan_var!(asic3.latch[0]);
    scan_var!(asic3.latch[1]);
    scan_var!(asic3.latch[2]);
    scan_var!(asic3.x);
    scan_var!(asic3.hilo);
    scan_var!(asic3.hold);

    0
}

// ============================================================================
// 单元测试
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_bit() {
        assert_eq!(bit(0b1010, 0), 0);
        assert_eq!(bit(0b1010, 1), 1);
        assert_eq!(bit(0b1010, 3), 1);
    }

    #[test]
    fn test_bitswap08() {
        // 恒等映射: bitswap08(val, 7,6,5,4,3,2,1,0) == val & 0xff
        assert_eq!(bitswap08(0xab, 7, 6, 5, 4, 3, 2, 1, 0), 0xab);
        // 全反转: bitswap08(val, 0,1,2,3,4,5,6,7) == bit_reverse_8(val)
        assert_eq!(bitswap08(0b10110001, 0, 1, 2, 3, 4, 5, 6, 7), 0b10001101);
    }

    #[test]
    fn test_reset() {
        let mut asic3 = Asic3::new();
        asic3.hold = 0x1234;
        asic3.reg = 0x56;
        asic3.x = 0x78;
        asic3.hilo = 0x9abc;
        asic3.latch = [1, 2, 3];

        asic3.reset();

        assert_eq!(asic3.hold, 0);
        assert_eq!(asic3.reg, 0);
        assert_eq!(asic3.x, 0);
        assert_eq!(asic3.hilo, 0);
        assert_eq!(asic3.latch, [0, 0, 0]);
    }

    #[test]
    fn test_read_identity_values() {
        let asic3 = Asic3 {
            latch: [0; 3],
            x: 0,
            reg: 0x20,
            hold: 0,
            hilo: 0,
        };
        assert_eq!(asic3.read_word(0), 0x49); // 'I'

        let asic3 = Asic3 { reg: 0x21, ..asic3 };
        assert_eq!(asic3.read_word(0), 0x47); // 'G'

        let asic3 = Asic3 { reg: 0x22, ..asic3 };
        assert_eq!(asic3.read_word(0), 0x53); // 'S'
    }

    #[test]
    fn test_write_reg_select() {
        let mut asic3 = Asic3::new();
        // 写入地址 0xc04000 → 设置 reg
        asic3.write_word(0xc04000, 0x42, 0);
        assert_eq!(asic3.reg, 0x42);
    }

    #[test]
    fn test_write_latch() {
        let mut asic3 = Asic3::new();

        // reg=0x00, 写入 latch[0] = data << 1
        asic3.reg = 0x00;
        asic3.write_word(0xc04002, 0x15, 0);
        assert_eq!(asic3.latch[0], 0x2a); // 0x15 << 1 = 0x2a

        // reg=0x01, 写入 latch[1]
        asic3.reg = 0x01;
        asic3.write_word(0xc04002, 0x20, 0);
        assert_eq!(asic3.latch[1], 0x40);
    }

    #[test]
    fn test_write_hilo_and_compute_x() {
        let mut asic3 = Asic3::new();

        // 先写 hilo
        asic3.reg = 0x40;
        asic3.write_word(0xc04002, 0xab, 0);
        assert_eq!(asic3.hilo, 0x00ab);

        asic3.write_word(0xc04002, 0xcd, 0);
        assert_eq!(asic3.hilo, 0xabcd);

        // 然后触发 0x48 来计算 x
        asic3.reg = 0x48;
        asic3.write_word(0xc04002, 0, 0);
        // hilo=0xabcd:
        //   0xabcd & 0x0090 = 0x0080 != 0 → bit0 不设
        //   0xabcd & 0x0006 = 0x0004 != 0 → bit1 不设
        //   0xabcd & 0x9000 = 0x8000 != 0 → bit2 不设
        //   0xabcd & 0x0a00 = 0x0a00 != 0 → bit3 不设
        assert_eq!(asic3.x, 0x00);
    }

    #[test]
    fn test_hold_reset_via_a0() {
        let mut asic3 = Asic3::new();
        asic3.hold = 0xffff;

        asic3.reg = 0xa0;
        asic3.write_word(0xc04002, 0, 0);
        assert_eq!(asic3.hold, 0);
    }

    #[test]
    fn test_compute_hold_mode1() {
        let mut asic3 = Asic3::new();
        asic3.hold = 0;
        asic3.x = 0;

        // region=0 → modes[0]=1 → mode 1
        asic3.compute_hold(0, 0x0000, 0);
        // old=0x0000
        // hold = (0<<1)|(0>>15) = 0
        // hold ^= 0x2bad → 0x2bad
        // hold ^= BIT(0,0) = 0
        // hold ^= BIT(0,2)<<10 = 0
        // hold ^= BIT(0,5) = 0
        // mode 1: hold ^= BIT(0,10)^BIT(0,8)^(BIT(0,0)<<1)^(BIT(0,1)<<6)^(BIT(0,3)<<14) = 0
        assert_eq!(asic3.hold, 0x2bad);
    }
}
