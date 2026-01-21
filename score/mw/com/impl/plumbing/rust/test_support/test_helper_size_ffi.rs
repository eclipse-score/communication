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

/// Unified FFI bindings to C++ TestSizeProvider for both SampleAllocateePtr and SamplePtr size verification
use test_utils_rs::SizeInfo;

unsafe extern "C" {
    // FFI bindings for sample_ptr.rs struct types
    // Safety: These functions are safe to call as they are read-only accessors that return constant size information
    // with no side effects or undefined behavior risks.
    safe fn ffi_get_sample_ptr_variant_i32_size() -> SizeInfo;
    safe fn ffi_get_sample_ptr_variant_u8_size() -> SizeInfo;
    safe fn ffi_get_sample_ptr_variant_u64_size() -> SizeInfo;
    safe fn ffi_get_sample_ptr_variant_user_defined_type_size() -> SizeInfo;
    safe fn ffi_get_control_slot_indicator_size() -> SizeInfo;
    safe fn ffi_get_slot_decrementer_size() -> SizeInfo;
    safe fn ffi_get_sample_ptr_size() -> SizeInfo;
    safe fn ffi_get_mock_binding_sample_ptr_size() -> SizeInfo;

    // FFI bindings for sample_allocatee_ptr.rs struct types
    // Safety: These functions are safe to call as they are read-only accessors that return constant size information
    // with no side effects or undefined behavior risks.
    safe fn ffi_get_sample_allocatee_variant_ptr_i32_size() -> SizeInfo;
    safe fn ffi_get_sample_allocatee_variant_ptr_u8_size() -> SizeInfo;
    safe fn ffi_get_sample_allocatee_variant_ptr_u64_size() -> SizeInfo;
    safe fn ffi_get_sample_allocatee_variant_ptr_user_defined_type_size() -> SizeInfo;
    safe fn ffi_get_control_slot_composite_indicator_size() -> SizeInfo;
    safe fn ffi_get_event_data_control_composite_size() -> SizeInfo;
    safe fn ffi_get_std_unique_ptr_size() -> SizeInfo;
    safe fn ffi_get_sample_allocatee_ptr_size() -> SizeInfo;
}

/// C++ size provider for SampleAllocateePtr types
pub struct SampleAllocateePtrLola;

impl SampleAllocateePtrLola {
    pub fn get_variant_int32() -> SizeInfo {
        ffi_get_sample_allocatee_variant_ptr_i32_size()
    }

    pub fn get_variant_unsigned_char() -> SizeInfo {
        ffi_get_sample_allocatee_variant_ptr_u8_size()
    }

    pub fn get_variant_unsigned_long_long() -> SizeInfo {
        ffi_get_sample_allocatee_variant_ptr_u64_size()
    }

    pub fn get_variant_user_defined_type() -> SizeInfo {
        ffi_get_sample_allocatee_variant_ptr_user_defined_type_size()
    }

    pub fn get_control_slot_composite_indicator_size() -> SizeInfo {
        ffi_get_control_slot_composite_indicator_size()
    }

    pub fn get_event_data_control_composite_size() -> SizeInfo {
        ffi_get_event_data_control_composite_size()
    }

    pub fn get_std_unique_ptr_size() -> SizeInfo {
        ffi_get_std_unique_ptr_size()
    }

    pub fn get_sample_allocatee_ptr_size() -> SizeInfo {
        ffi_get_sample_allocatee_ptr_size()
    }
}

/// C++ size provider for SamplePtr types
pub struct SamplePtrLola;

impl SamplePtrLola {
    pub fn get_variant_int32() -> SizeInfo {
        ffi_get_sample_ptr_variant_i32_size()
    }

    pub fn get_variant_unsigned_char() -> SizeInfo {
        ffi_get_sample_ptr_variant_u8_size()
    }

    pub fn get_variant_unsigned_long_long() -> SizeInfo {
        ffi_get_sample_ptr_variant_u64_size()
    }

    pub fn get_variant_user_defined_type() -> SizeInfo {
        ffi_get_sample_ptr_variant_user_defined_type_size()
    }

    pub fn get_control_slot_indicator_size() -> SizeInfo {
        ffi_get_control_slot_indicator_size()
    }

    pub fn get_slot_decrementer_size() -> SizeInfo {
        ffi_get_slot_decrementer_size()
    }

    pub fn get_sample_ptr_size() -> SizeInfo {
        ffi_get_sample_ptr_size()
    }

    pub fn get_mock_binding_sample_ptr_size() -> SizeInfo {
        ffi_get_mock_binding_sample_ptr_size()
    }
}
