use std::os::raw::{c_char, c_void};

// 1. 对应 struct BurnArea
#[repr(C)]
pub struct BurnArea {
    pub data: *mut c_void,      // void *Data
    pub len: u32,               // UINT32 nLen
    pub address: i32,           // INT32 nAddress
    pub sz_name: *const c_char, // char *szName
}

// 2. 声明外部 C 回调函数指针 BurnAcb
unsafe extern "C" {
    // FBNeo 定义的全局函数指针变量
    pub static mut BurnAcb: Option<extern "C" fn(pba: *mut BurnArea) -> i32>;
}

/// 对应 C 语言的 ScanVar 逻辑
#[inline]
pub fn scan_var(pv: *mut c_void, n_size: i32, sz_name: *const c_char) {
    unsafe {
        let mut ba = BurnArea {
            data: pv,
            len: n_size as u32,
            address: 0,
            sz_name: sz_name,
        };
        BurnAcb.unwrap()(std::ptr::addr_of_mut!(ba));
    }
}

#[macro_export]
macro_rules! scan_var {
    ($var:expr) => {
        scan_var(
            std::ptr::addr_of_mut!($var) as *mut std::ffi::c_void,
            std::mem::size_of_val(&$var) as i32,
            concat!(stringify!($var), "\0").as_ptr() as *const std::ffi::c_char,
        );
    };
}
