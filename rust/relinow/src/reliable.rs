use relinow_sys::*;
use core::mem::MaybeUninit;

pub fn default_reliable_config() -> relinow_reliable_config_t {
    let mut cfg = MaybeUninit::uninit();
    unsafe {
        relinow_reliable_default_config(cfg.as_mut_ptr());
        cfg.assume_init()
    }
}
