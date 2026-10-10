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
#include "score/mw/com/impl/methods/method_signature_element_ptr.h"
#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"
#include "score/mw/com/impl/plumbing/sample_ptr.h"
#include <cstring>
#include <memory>
#include <optional>
#include <utility>

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

SizeInfo TestSizeProvider::GetSlotDecrementerSize() noexcept
{
    return {sizeof(score::mw::com::impl::lola::SlotDecrementer), alignof(score::mw::com::impl::lola::SlotDecrementer)};
}

SizeInfo TestSizeProvider::GetEventDataControlCompositeSize() noexcept
{
    return {sizeof(score::mw::com::impl::lola::EventDataControlComposite<>),
            alignof(score::mw::com::impl::lola::EventDataControlComposite<>)};
}

SizeInfo TestSizeProvider::GetStdUniquePtrSize() noexcept
{
    using MockSampleAllocateePtrInt32 = std::unique_ptr<int32_t, mock_binding::CustomDeleter>;
    return {sizeof(MockSampleAllocateePtrInt32), alignof(MockSampleAllocateePtrInt32)};
}

SizeInfo TestSizeProvider::GetSampleAllocateePtrSize() noexcept
{
    return {sizeof(score::mw::com::impl::lola::SampleAllocateePtr),
            alignof(score::mw::com::impl::lola::SampleAllocateePtr)};
}

SizeInfo TestSizeProvider::GetSamplePtrSize() noexcept
{
    return {sizeof(score::mw::com::impl::lola::SamplePtr), alignof(score::mw::com::impl::lola::SamplePtr)};
}

SizeInfo TestSizeProvider::GetMockBindingSamplePtrSize() noexcept
{
    return {sizeof(score::mw::com::impl::mock_binding::SamplePtr),
            alignof(score::mw::com::impl::mock_binding::SamplePtr)};
}

SizeInfo TestSizeProvider::GetMethodInArgPtrInt32Size() noexcept
{
    return {sizeof(score::mw::com::impl::MethodInArgPtr<int32_t>),
            alignof(score::mw::com::impl::MethodInArgPtr<int32_t>)};
}

SizeInfo TestSizeProvider::GetMethodInArgPtrUserDefinedTypeSize() noexcept
{
    return {sizeof(score::mw::com::impl::MethodInArgPtr<UserType>),
            alignof(score::mw::com::impl::MethodInArgPtr<UserType>)};
}

}  // namespace score::mw::com::impl

// C wrapper functions for FFI
extern "C" {
// Snapshot the actual private C++ representation through character bytes. No C++
// object is fabricated by reinterpreting Rust storage, and ownership never crosses
// this test boundary. The reference slot is checked without offsetof on a reference.
bool ffi_verify_method_in_arg_cpp(const void* rust_owner,
                                  const void* element,
                                  const void* active,
                                  std::size_t queue_position) noexcept
{
    struct Representation
    {
        const void* element;
        const void* active;
        std::size_t position;
    };
    std::int32_t cpp_element{42};
    bool cpp_active{false};
    using Pointer = score::mw::com::impl::MethodInArgPtr<std::int32_t>;
    static_assert(sizeof(Pointer) == sizeof(Representation));
    static_assert(alignof(Pointer) == alignof(Representation));
    Representation cpp_bytes{};
    Representation rust_bytes{};
    bool move_keeps_active{false};
    std::optional<Pointer> destination{};
    {
        Pointer source{cpp_element, cpp_active, queue_position};
        std::memcpy(&cpp_bytes, &source, sizeof(cpp_bytes));
        destination.emplace(std::move(source));
        move_keeps_active = cpp_active && source.get() == nullptr && destination->get() == &cpp_element;
    }
    // The moved-from destructor runs while the destination still owns the flag.
    move_keeps_active = move_keeps_active && cpp_active;
    destination.reset();
    std::memcpy(&rust_bytes, rust_owner, sizeof(rust_bytes));
    return cpp_bytes.element == &cpp_element && cpp_bytes.active == &cpp_active &&
           cpp_bytes.position == queue_position && rust_bytes.element == element && rust_bytes.active == active &&
           rust_bytes.position == queue_position && move_keeps_active && !cpp_active && cpp_element == 42;
}

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

score::mw::com::impl::SizeInfo ffi_get_slot_decrementer_size() noexcept
{
    return score::mw::com::impl::TestSizeProvider::GetSlotDecrementerSize();
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

score::mw::com::impl::SizeInfo ffi_get_method_in_arg_ptr_i32_size() noexcept
{
    return score::mw::com::impl::TestSizeProvider::GetMethodInArgPtrInt32Size();
}

score::mw::com::impl::SizeInfo ffi_get_method_in_arg_ptr_user_defined_type_size() noexcept
{
    return score::mw::com::impl::TestSizeProvider::GetMethodInArgPtrUserDefinedTypeSize();
}

}  // extern "C"
