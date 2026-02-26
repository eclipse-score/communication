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

///This crate gives same memory layout as SampleAllocateePtr in C++ implementation
///It can be used for FFI bindings where SampleAllocateePtr is needed
///It does not provide any functionality beyond memory layout compatibility
use core::fmt::Debug;
use std::mem::ManuallyDrop;

use common_rs::{BlankBinding, ControlSlotType, CxxOptional, EventDataControl, SlotIndexType};

#[repr(C)]
struct ControlSlotCompositeIndicator {
    _slot_index: SlotIndexType,
    _slot_pointer_qm: *mut ControlSlotType,
    _slot_pointer_asil_b: *mut ControlSlotType,
}

#[repr(C)]
struct EventDataControlComposite {
    _event_data_control_qm: *mut EventDataControl,
    _event_data_control_asil_b: *mut EventDataControl,
    _ignore_qm_control_: bool,
}

#[repr(C)]
struct LolaSampleAllocateePtrBinding<T> {
    _managed_object: *mut T,
    _event_slot_indicator: ControlSlotCompositeIndicator,
    _event_data_control: CxxOptional<EventDataControlComposite>,
}

#[repr(C)]
struct UniquePtrBinding<T> {
    _ptr: *mut T,
}

#[repr(C)]
union UniquePtrVariant<T> {
    _variant: ManuallyDrop<UniquePtrBinding<T>>,
}

#[repr(C)]
union LolaSampleAllocateePtrVariant<T> {
    _variant: ManuallyDrop<LolaSampleAllocateePtrBinding<T>>,
    _unique_ptr: ManuallyDrop<UniquePtrVariant<T>>,
}

#[repr(C)]
union BlankVariant<T> {
    _variant: ManuallyDrop<BlankBinding>,
    _other: ManuallyDrop<LolaSampleAllocateePtrVariant<T>>,
}

#[repr(C)]
struct AllocationVariant<T> {
    _data: BlankVariant<T>,
    _index: u8,
}

#[repr(C, align(16))]
pub struct SampleAllocateePtr<T> {
    _internal: AllocationVariant<T>,
}

// SAFETY: There is no connection of any data to a particular thread, also not on C++ side.
// Therefore, it is safe to send this struct to another thread.
unsafe impl<T> Send for SampleAllocateePtr<T> {}

impl<T> Debug for SampleAllocateePtr<T> {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        let state = match self._internal._index {
            0 => "Blank",
            1 => "LolaSampleAllocateePtr",
            2 => "UniquePtr",
            _ => "Unknown",
        };

        f.debug_struct("SampleAllocateePtr")
            .field("state", &state)
            .finish()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use test_helper_size_ffi_rs::SampleAllocateePtrLola;
    use test_utils_rs::*;

    #[test]
    fn test_sample_allocatee_ptr_variant_int32_size() {
        let cpp_size = SampleAllocateePtrLola::get_variant_int32();
        verify_size_and_align!(SampleAllocateePtr<i32>, cpp_size, "SampleAllocateePtr<i32>");
    }

    #[test]
    fn test_sample_allocatee_ptr_variant_u8_size() {
        let cpp_size = SampleAllocateePtrLola::get_variant_unsigned_char();
        verify_size_and_align!(SampleAllocateePtr<u8>, cpp_size, "SampleAllocateePtr<u8>");
    }

    #[test]
    fn test_sample_allocatee_ptr_variant_u64_size() {
        let cpp_size = SampleAllocateePtrLola::get_variant_unsigned_long_long();
        verify_size_and_align!(SampleAllocateePtr<u64>, cpp_size, "SampleAllocateePtr<u64>");
    }

    #[test]
    fn test_sample_allocatee_ptr_variant_user_defined_type_size() {
        let cpp_size = SampleAllocateePtrLola::get_variant_user_defined_type();
        verify_size_and_align!(
            SampleAllocateePtr<UserType>,
            cpp_size,
            "SampleAllocateePtr<UserType>"
        );
    }

    #[test]
    fn test_control_slot_composite_indicator_size() {
        let cpp_size = SampleAllocateePtrLola::get_control_slot_composite_indicator_size();
        verify_size_and_align!(
            ControlSlotCompositeIndicator,
            cpp_size,
            "ControlSlotCompositeIndicator"
        );
    }

    #[test]
    fn test_event_data_control_composite_size() {
        let cpp_size = SampleAllocateePtrLola::get_event_data_control_composite_size();
        verify_size_and_align!(
            EventDataControlComposite,
            cpp_size,
            "EventDataControlComposite"
        );
    }

    #[test]
    fn test_std_unique_ptr_size() {
        let cpp_size = SampleAllocateePtrLola::get_std_unique_ptr_size();
        verify_size_and_align!(UniquePtrBinding<i32>, cpp_size, "std::unique_ptr<i32>");
    }

    #[test]
    fn test_sample_allocatee_ptr_size() {
        let cpp_size = SampleAllocateePtrLola::get_sample_allocatee_ptr_size();
        verify_size_and_align!(
            LolaSampleAllocateePtrBinding<i32>,
            cpp_size,
            "LolaSampleAllocateePtrBinding<i32>"
        );
    }

    #[test]
    #[should_panic(expected = "size mismatch")]
    fn test_negative_allocatee_ptr_size_mismatch() {
        let cpp_size = SampleAllocateePtrLola::get_variant_int32();
        let incorrect = cpp_size.size + 1;
        assert_eq!(
            incorrect, cpp_size.size,
            "SampleAllocateePtr size mismatch!"
        );
    }

    #[test]
    #[should_panic(expected = "align mismatch")]
    fn test_negative_allocatee_ptr_align_mismatch() {
        let cpp_size = SampleAllocateePtrLola::get_variant_int32();
        let incorrect = cpp_size.align + 1;
        assert_eq!(
            incorrect, cpp_size.align,
            "SampleAllocateePtr align mismatch!"
        );
    }
}
