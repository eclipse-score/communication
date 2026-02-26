/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/// Structure matching C++ SizeInfo struct
use std::ffi::c_char;
#[repr(C)]
#[derive(Debug, PartialEq, Eq)]
pub struct SizeInfo {
    // Size in bytes of the type
    pub size: u64,
    // Alignment requirement in bytes
    pub align: u64,
}
#[repr(C)]
pub struct UserType {
    value1: u32,
    value2: *const c_char,
    value3: f32,
}

/// Macro to verify that Rust type size and alignment match C++ size info
///
/// # Panics
/// Panics if size or alignment don't match (checked at test runtime)
#[macro_export]
macro_rules! verify_size_and_align {
    ($rust_type:ty, $cpp_size_info:expr, $type_name:expr) => {
        assert_eq!(
            std::mem::size_of::<$rust_type>() as u64,
            $cpp_size_info.size,
            "{} size mismatch!",
            $type_name
        );
        assert_eq!(
            std::mem::align_of::<$rust_type>() as u64,
            $cpp_size_info.align,
            "{} align mismatch!",
            $type_name
        );
    };
}
