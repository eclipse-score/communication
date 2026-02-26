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
#include "test_helper_size_provider.h"
#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"
#include "score/mw/com/impl/plumbing/sample_ptr.h"
#include <memory>

// This is test helper file which provides the FFI interface for Rust test cases
// to validate the C++ type with Rust side manually defined type

namespace score::mw::com::impl
{

SizeInfo TestSizeProvider::GetSampleAllocateePtrVariantInt32Size() noexcept
{
    return {sizeof(score::mw::com::impl::SampleAllocateePtr<int32_t>),
            alignof(score::mw::com::impl::SampleAllocateePtr<int32_t>)};
}

SizeInfo TestSizeProvider::GetSampleAllocateePtrVariantUnsignedCharSize() noexcept
{
    return {sizeof(score::mw::com::impl::SampleAllocateePtr<unsigned char>),
            alignof(score::mw::com::impl::SampleAllocateePtr<unsigned char>)};
}

SizeInfo TestSizeProvider::GetSampleAllocateePtrVariantUnsignedLongLongSize() noexcept
{
    return {sizeof(score::mw::com::impl::SampleAllocateePtr<unsigned long long>),
            alignof(score::mw::com::impl::SampleAllocateePtr<unsigned long long>)};
}

SizeInfo TestSizeProvider::GetSampleAllocateePtrVariantUserDefinedTypeSize() noexcept
{
    return {sizeof(score::mw::com::impl::SampleAllocateePtr<UserType>),
            alignof(score::mw::com::impl::SampleAllocateePtr<UserType>)};
}

SizeInfo TestSizeProvider::GetSamplePtrVariantInt32Size() noexcept
{
    return {sizeof(score::mw::com::impl::SamplePtr<int32_t>), alignof(score::mw::com::impl::SamplePtr<int32_t>)};
}

SizeInfo TestSizeProvider::GetSamplePtrVariantUnsignedCharSize() noexcept
{
    return {sizeof(score::mw::com::impl::SamplePtr<unsigned char>),
            alignof(score::mw::com::impl::SamplePtr<unsigned char>)};
}

SizeInfo TestSizeProvider::GetSamplePtrVariantUnsignedLongLongSize() noexcept
{
    return {sizeof(score::mw::com::impl::SamplePtr<unsigned long long>),
            alignof(score::mw::com::impl::SamplePtr<unsigned long long>)};
}

SizeInfo TestSizeProvider::GetSamplePtrVariantUserDefinedTypeSize() noexcept
{
    return {sizeof(score::mw::com::impl::SamplePtr<UserType>), alignof(score::mw::com::impl::SamplePtr<UserType>)};
}

SizeInfo TestSizeProvider::GetControlSlotIndicatorSize() noexcept
{
    return {sizeof(score::mw::com::impl::lola::ControlSlotIndicator),
            alignof(score::mw::com::impl::lola::ControlSlotIndicator)};
}

SizeInfo TestSizeProvider::GetSlotDecrementerSize() noexcept
{
    return {sizeof(score::mw::com::impl::lola::SlotDecrementer), alignof(score::mw::com::impl::lola::SlotDecrementer)};
}

SizeInfo TestSizeProvider::GetControlSlotCompositeIndicatorSize() noexcept
{
    return {sizeof(score::mw::com::impl::lola::ControlSlotCompositeIndicator),
            alignof(score::mw::com::impl::lola::ControlSlotCompositeIndicator)};
}

SizeInfo TestSizeProvider::GetEventDataControlCompositeSize() noexcept
{
    return {sizeof(score::mw::com::impl::lola::EventDataControlComposite),
            alignof(score::mw::com::impl::lola::EventDataControlComposite)};
}

SizeInfo TestSizeProvider::GetStdUniquePtrSize() noexcept
{
    return {sizeof(std::unique_ptr<int32_t>), alignof(std::unique_ptr<int32_t>)};
}

SizeInfo TestSizeProvider::GetSampleAllocateePtrSize() noexcept
{
    return {sizeof(score::mw::com::impl::lola::SampleAllocateePtr<int32_t>),
            alignof(score::mw::com::impl::lola::SampleAllocateePtr<int32_t>)};
}

SizeInfo TestSizeProvider::GetSamplePtrSize() noexcept
{
    return {sizeof(score::mw::com::impl::lola::SamplePtr<int32_t>),
            alignof(score::mw::com::impl::lola::SamplePtr<int32_t>)};
}

SizeInfo TestSizeProvider::GetMockBindingSamplePtrSize() noexcept
{
    return {sizeof(score::mw::com::impl::mock_binding::SamplePtr<int32_t>),
            alignof(score::mw::com::impl::mock_binding::SamplePtr<int32_t>)};
}

}  // namespace score::mw::com::impl

// C wrapper functions for FFI
extern "C" {

score::mw::com::impl::SizeInfo ffi_get_sample_allocatee_variant_ptr_i32_size() noexcept
{
    return score::mw::com::impl::TestSizeProvider::GetSampleAllocateePtrVariantInt32Size();
}

score::mw::com::impl::SizeInfo ffi_get_sample_allocatee_variant_ptr_u8_size() noexcept
{
    return score::mw::com::impl::TestSizeProvider::GetSampleAllocateePtrVariantUnsignedCharSize();
}

score::mw::com::impl::SizeInfo ffi_get_sample_allocatee_variant_ptr_user_defined_type_size() noexcept
{
    return score::mw::com::impl::TestSizeProvider::GetSampleAllocateePtrVariantUserDefinedTypeSize();
}

score::mw::com::impl::SizeInfo ffi_get_sample_allocatee_variant_ptr_u64_size() noexcept
{
    return score::mw::com::impl::TestSizeProvider::GetSampleAllocateePtrVariantUnsignedLongLongSize();
}

score::mw::com::impl::SizeInfo ffi_get_sample_ptr_variant_i32_size() noexcept
{
    return score::mw::com::impl::TestSizeProvider::GetSamplePtrVariantInt32Size();
}

score::mw::com::impl::SizeInfo ffi_get_sample_ptr_variant_u8_size() noexcept
{
    return score::mw::com::impl::TestSizeProvider::GetSamplePtrVariantUnsignedCharSize();
}

score::mw::com::impl::SizeInfo ffi_get_sample_ptr_variant_u64_size() noexcept
{
    return score::mw::com::impl::TestSizeProvider::GetSamplePtrVariantUnsignedLongLongSize();
}

score::mw::com::impl::SizeInfo ffi_get_sample_ptr_variant_user_defined_type_size() noexcept
{
    return score::mw::com::impl::TestSizeProvider::GetSamplePtrVariantUserDefinedTypeSize();
}

score::mw::com::impl::SizeInfo ffi_get_control_slot_indicator_size() noexcept
{
    return score::mw::com::impl::TestSizeProvider::GetControlSlotIndicatorSize();
}

score::mw::com::impl::SizeInfo ffi_get_slot_decrementer_size() noexcept
{
    return score::mw::com::impl::TestSizeProvider::GetSlotDecrementerSize();
}

score::mw::com::impl::SizeInfo ffi_get_control_slot_composite_indicator_size() noexcept
{
    return score::mw::com::impl::TestSizeProvider::GetControlSlotCompositeIndicatorSize();
}

score::mw::com::impl::SizeInfo ffi_get_event_data_control_composite_size() noexcept
{
    return score::mw::com::impl::TestSizeProvider::GetEventDataControlCompositeSize();
}

score::mw::com::impl::SizeInfo ffi_get_std_unique_ptr_size() noexcept
{
    return score::mw::com::impl::TestSizeProvider::GetStdUniquePtrSize();
}

score::mw::com::impl::SizeInfo ffi_get_sample_allocatee_ptr_size() noexcept
{
    return score::mw::com::impl::TestSizeProvider::GetSampleAllocateePtrSize();
}

score::mw::com::impl::SizeInfo ffi_get_sample_ptr_size() noexcept
{
    return score::mw::com::impl::TestSizeProvider::GetSamplePtrSize();
}

score::mw::com::impl::SizeInfo ffi_get_mock_binding_sample_ptr_size() noexcept
{
    return score::mw::com::impl::TestSizeProvider::GetMockBindingSamplePtrSize();
}

}  // extern "C"
