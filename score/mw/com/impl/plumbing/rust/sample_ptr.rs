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

use common_rs::{
    BlankBinding, 
    CxxOptional,
    EventDataControl,
    SlotIndexType,
    TransactionLogIndex,
    UniquePtr,
    CustomDeleter,
};

type MockBinding<T> = UniquePtr<T, CustomDeleter>;

#[repr(C)]
union MockVariant<T> {
    _variant: ManuallyDrop<MockBinding<T>>,
}

#[repr(C)]
struct SlotDecrementer {
    _event_data_control: *mut EventDataControl,
    _event_slot_index: SlotIndexType,
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

#[cfg(test)]
mod tests {
    use super::*;
    use test_helper_size_ffi_rs::SamplePtrLola;
    use test_utils_rs::*;

    #[test]
    fn test_sample_ptr_variant_int32_size() {
        let cpp_size = SamplePtrLola::get_variant_int32();
        verify_size_and_align!(SamplePtr<i32>, cpp_size, "SamplePtr<i32>");
    }

    #[test]
    fn test_sample_ptr_variant_u8_size() {
        let cpp_size = SamplePtrLola::get_variant_unsigned_char();
        verify_size_and_align!(SamplePtr<u8>, cpp_size, "SamplePtr<u8>");
    }

    #[test]
    fn test_sample_ptr_variant_u64_size() {
        let cpp_size = SamplePtrLola::get_variant_unsigned_long_long();
        verify_size_and_align!(SamplePtr<u64>, cpp_size, "SamplePtr<u64>");
    }

    #[test]
    fn test_sample_ptr_variant_user_defined_type_size() {
        let cpp_size = SamplePtrLola::get_variant_user_defined_type();
        verify_size_and_align!(SamplePtr<UserType>, cpp_size, "SamplePtr<UserType>");
    }

    #[test]
    fn test_sample_ptr_lolabinding_size() {
        let cpp_size = SamplePtrLola::get_sample_ptr_size();
        verify_size_and_align!(LolaBinding<i32>, cpp_size, "LolaBinding<i32>");
    }

    #[test]
    fn test_mock_binding_sample_ptr_size() {
        let cpp_size = SamplePtrLola::get_mock_binding_sample_ptr_size();
        verify_size_and_align!(MockBinding<i32>, cpp_size, "MockBinding<i32>");
    }

    #[test]
    fn test_slot_decrementer_size() {
        let cpp_size = SamplePtrLola::get_slot_decrementer_size();
        verify_size_and_align!(SlotDecrementer, cpp_size, "SlotDecrementer");
    }

    #[test]
    #[should_panic(expected = "size mismatch")]
    fn test_negative_sample_ptr_size_mismatch() {
        let cpp_size = SamplePtrLola::get_variant_int32();
        let incorrect = cpp_size.size + 1;
        assert_eq!(incorrect, cpp_size.size, "SamplePtr size mismatch!");
    }

    #[test]
    #[should_panic(expected = "align mismatch")]
    fn test_negative_sample_ptr_align_mismatch() {
        let cpp_size = SamplePtrLola::get_variant_int32();
        let incorrect = cpp_size.align + 1;
        assert_eq!(incorrect, cpp_size.align, "SamplePtr align mismatch!");
    }
}
