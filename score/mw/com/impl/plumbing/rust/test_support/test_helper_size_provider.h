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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_TEST_HELPER_SIZE_PROVIDER_H
#define SCORE_MW_COM_IMPL_PLUMBING_TEST_HELPER_SIZE_PROVIDER_H

#include <cstdint>
#include <string>

namespace score::mw::com::impl
{

/// Structure to hold size and alignment information of a type
struct SizeInfo
{
    std::uint64_t size;
    std::uint64_t align;
};

struct UserType
{
    uint32_t value1;
    const char* value2;
    float value3;
};

/// Interface to provide size information for FFI binding verification
class TestSizeProvider
{
  public:
    /// Get size info for SampleAllocateePtr<int32_t> variant
    static SizeInfo GetSampleAllocateePtrVariantInt32Size() noexcept;

    /// Get size info for SampleAllocateePtr<unsigned char> variant
    static SizeInfo GetSampleAllocateePtrVariantUnsignedCharSize() noexcept;

    /// Get size info for SampleAllocateePtr<unsigned long long> variant
    static SizeInfo GetSampleAllocateePtrVariantUnsignedLongLongSize() noexcept;

    /// Get size info for SampleAllocateePtr<UserType> variant
    static SizeInfo GetSampleAllocateePtrVariantUserDefinedTypeSize() noexcept;

    /// Get size info for SamplePtr<int32_t> variant
    static SizeInfo GetSamplePtrVariantInt32Size() noexcept;

    /// Get size info for SamplePtr<unsigned char> variant
    static SizeInfo GetSamplePtrVariantUnsignedCharSize() noexcept;

    /// Get size info for SamplePtr<unsigned long long> variant
    static SizeInfo GetSamplePtrVariantUnsignedLongLongSize() noexcept;

    /// Get size info for SamplePtr<UserType> variant
    static SizeInfo GetSamplePtrVariantUserDefinedTypeSize() noexcept;

    /// Get size info for ControlSlotIndicator struct
    static SizeInfo GetControlSlotIndicatorSize() noexcept;

    /// Get size info for SlotDecrementer struct
    static SizeInfo GetSlotDecrementerSize() noexcept;

    /// Get size info for ControlSlotCompositeIndicator struct
    static SizeInfo GetControlSlotCompositeIndicatorSize() noexcept;

    /// Get size info for EventDataControlComposite struct
    static SizeInfo GetEventDataControlCompositeSize() noexcept;

    /// Get size info for std::unique_ptr<int32_t>
    static SizeInfo GetStdUniquePtrSize() noexcept;

    /// SampleAllocateePtr size info
    static SizeInfo GetSampleAllocateePtrSize() noexcept;

    /// SamplePtr size info
    static SizeInfo GetSamplePtrSize() noexcept;

    /// mock_binding::SamplePtr size info
    static SizeInfo GetMockBindingSamplePtrSize() noexcept;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_TEST_HELPER_SIZE_PROVIDER_H
