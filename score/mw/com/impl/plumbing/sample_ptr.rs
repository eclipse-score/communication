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
use core::fmt::Debug;
use std::mem::ManuallyDrop;

#[cfg(target_os = "nto")]
mod util_nto {
    #![allow(non_camel_case_types)]

    // There is no canonical definition of c_longdouble in Rust. For both AArch64 and x86_64,
    // however, the size and alignment properties are that of the gcc __int128 which corresponds (at
    // least on rustc 1.78+ with LLVM 18, see
    // https://blog.rust-lang.org/2024/03/30/i128-layout-update/) to u128. Use this instead until we
    // get native f128 support.
    #[repr(C)]
    #[derive(Copy, Clone, Default, Debug)]
    pub struct max_align_t {
        _ll: libc::c_longlong,
        _ld: u128,
    }
}

#[cfg(target_os = "nto")]
use util_nto::max_align_t;
#[cfg(not(target_os = "nto"))]
use libc::max_align_t;

// @todo check whether we can get this info "somehow" from C++ code
type CustomDeleterAlignment = max_align_t;
const CUSTOM_DELETER_SIZE: usize = 32;

#[repr(C)]
union CustomDeleterInner {
    _callback: [u8; CUSTOM_DELETER_SIZE],
    _align: [CustomDeleterAlignment; 0],
}

#[repr(C)]
struct WrapperBase {
    _dummy: [u8; 0],
}

#[repr(C)]
struct CustomDeleter {
    _inner: CustomDeleterInner,
    _wrapper: *mut WrapperBase,
}

#[repr(C)]
struct UniquePtr<T, D = ()> {
    _deleter: D,
    _ptr: *mut T,
}

type MockBinding<T> = UniquePtr<T, CustomDeleter>;

#[repr(C)]
union MockVariant<T> {
    _variant: ManuallyDrop<MockBinding<T>>,
}

#[repr(C)]
struct CxxOptional<T> {
    _data: T,
    _engaged: bool,
}

#[repr(C)]
struct EventDataControl {
    _dummy: [u8; 0],
}

#[repr(C)]
struct ControlSlotType  {
    _dummy: [u8; 0],
}

type SlotIndexType = u16;
type TransactionLogIndex = u8;

#[repr(C)]
struct ControlSlotIndicator {
    _slot_index: SlotIndexType,
    _slot_pointer: *mut ControlSlotType,
}

#[repr(C)]
struct SlotDecrementer {
    _event_data_control: *mut EventDataControl,
    _control_slot_indicator: ControlSlotIndicator,
    _transaction_log_idx: TransactionLogIndex,
}

#[repr(C)]
struct LolaBinding<T> {
    _managed_object: *const T,
    _slot_decrementer: CxxOptional<SlotDecrementer>,
}

#[repr(C)]
union LolaVariant<T> {
    _variant: ManuallyDrop<LolaBinding<T>>,
    _other: ManuallyDrop<MockVariant<T>>,
}

#[repr(C)]
struct BlankBinding {
    _data: [u8; 0],
}

#[repr(C)]
union BlankVariant<T> {
    _variant: ManuallyDrop<BlankBinding>,
    _other: ManuallyDrop<LolaVariant<T>>,
}

#[repr(C)]
struct BindingVariant<T> {
    _data: BlankVariant<T>,
    _index: u8,
}

#[repr(C)]
struct SampleReferenceTracker {
    _dummy: [u8; 0],
}

#[repr(C)]
struct SampleReferenceGuard {
    _tracker: *mut SampleReferenceTracker,
}

#[repr(C)]
pub struct SamplePtr<T> {
    _binding_sample_ptr: BindingVariant<T>,
    _sample_reference_guard: SampleReferenceGuard,
}

// SAFETY: There is no connection of any data to a particular thread, also not on C++ side.
// Therefore, it is safe to send this struct to another thread.
unsafe impl<T> Send for SamplePtr<T> {}

impl<T> Debug for SamplePtr<T> {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("SamplePtr").finish()
    }
}
