#![no_std]
#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

// Manual FFI bindings will go here or be generated via bindgen if clang is installed
// For now, an empty stub to allow compilation of the C library via cc.

pub type relinow_err_t = i32;
pub const RELINOW_OK: relinow_err_t = 0;
pub const RELINOW_ERR_INVALID_ARG: relinow_err_t = -1;
