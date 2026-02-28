use arbitrary_int::{traits::Integer, u1, u2, u4};
use chrono::{Datelike, Timelike};

use crate::{scan_var, state::scan_var};

fn u8_to_bcd(val: u8) -> u8 {
    assert!(val < 100, "BCD value out of range");
    ((val / 10) << 4) | (val % 10)
}

/// TODO:
///   - Add Test Mode Support for faster time keeping
///   - Add copy ram to clock and copy clock to ram support
///   - Add update flags support
#[repr(C)]
pub struct V3021 {
    value: u8,
    command: u4,
    counter: u2,
}

impl V3021 {
    pub fn new() -> Self {
        V3021 {
            value: 0,
            command: u4::new(0),
            counter: u2::new(0),
        }
    }

    pub fn write(&mut self, data: u1) {
        self.command = self.command.wrapping_shl(1) | u4::from_(data);
        self.counter = self.counter.wrapping_add(u2::new(1));
        if self.counter == u2::new(0) {
            let cur_cal = chrono::offset::Local::now();
            self.value = match self.command.reverse_bits().as_u8() {
                // bit4: 0 => enable copy ram to clock, 1 => enable copy clock to ram
                // bit3-2: Test Mode
                //  - 00 => normal
                //  - 01 => all time keeping accelerated by 32,
                //  - 10 => Parallel increment of all time data at 1 Hz with no carry over
                //  - 11 => Parallel increment of all time data at 32 Hz with carry over
                0x0 => 0,
                // read_only
                // bit7: week_number updated
                // bit6: weekday updated
                // bit5: year updated
                // bit4: month updated
                // bit3: day updated
                // bit2: hour updated
                // bit1: minute updated
                // bit0: second updated
                0x1 => 0,
                // seconds: BCD format, 0-59
                0x2 => u8_to_bcd(cur_cal.second() as u8),
                // minutes: BCD format, 0-59
                0x3 => u8_to_bcd(cur_cal.minute() as u8),
                // hours: BCD format, 0-23
                0x4 => u8_to_bcd(cur_cal.hour() as u8),
                // days: BCD format, 1-31
                0x5 => u8_to_bcd(cur_cal.day() as u8),
                // months: BCD format, 1-12
                0x6 => u8_to_bcd(cur_cal.month() as u8),
                // years: BCD format, 0-99
                0x7 => u8_to_bcd((cur_cal.year() % 100) as u8),
                // weekdays: BCD format, 1-7 (1=Sunday, 2=Monday, ..., 7=Saturday)
                0x8 => u8_to_bcd(cur_cal.weekday().number_from_sunday() as u8),
                // week number: BCD format, 0-52
                0x9 => u8_to_bcd(cur_cal.iso_week().week0() as u8),

                0xa | 0xb | 0xc | 0xd => 0, // ?

                0xe => 0, // copy_ram_to_clock
                0xf => 1, // copy_clock_to_ram
                _ => unreachable!(),
            };
        }
    }

    pub fn read(&mut self) -> u1 {
        let ret = u1::new(self.value & 0x01);
        self.value >>= 1;
        return ret;
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn v3021Init() -> *mut V3021 {
    let rtc = Box::new(V3021::new());
    Box::into_raw(rtc)
}

#[unsafe(no_mangle)]
pub extern "C" fn v3021Write(handle: *mut V3021, data: u16) {
    if handle.is_null() {
        return;
    }

    let rtc = unsafe { &mut *handle };
    rtc.write(u1::new(data as u8 & 0x01));
}

#[unsafe(no_mangle)]
pub extern "C" fn v3021Read(handle: *mut V3021) -> u8 {
    if handle.is_null() {
        return 0;
    }

    let rtc = unsafe { &mut *handle };
    rtc.read().as_u8()
}

#[unsafe(no_mangle)]
pub extern "C" fn v3021Exit(handle: *mut V3021) {
    if !handle.is_null() {
        unsafe {
            let _ = Box::from_raw(handle);
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn v3021Scan(handle: *mut V3021) -> i32 {
    if handle.is_null() {
        return 0;
    }

    let rtc = unsafe { &mut *handle };

    scan_var!(rtc.value);
    scan_var!(rtc.command);
    scan_var!(rtc.counter);

    return 0;
}
