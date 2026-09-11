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
#ifndef SCORE_MW_COM_IMPL_MOCKING_TEST_TYPE_UTILITIES_H
#define SCORE_MW_COM_IMPL_MOCKING_TEST_TYPE_UTILITIES_H

#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"
#include "score/mw/com/impl/plumbing/sample_ptr.h"

#include <cstdint>
#include <list>
#include <memory>

/// @brief test_type_utilities contains all of the factory functions that are required for creating fake mw::com::impl
/// internals which are required for mocking.
///
/// These types and functions should not be accessed directly by applications, but rather they should use
/// mw/com/test_types.h
namespace score::mw::com::impl
{

/// Note. Since the implementation of MakeInstanceIdentifier uses global lists, the instance identifiers should not be
/// created in a global context to avoid static initialisation fiasco issues.
InstanceIdentifier MakeFakeInstanceIdentifier(const std::uint16_t unique_identifier);

HandleType MakeFakeHandle(const std::uint16_t unique_identifier);

void ResetInstanceIdentifierConfiguration();

/// \brief Helper to create a SampleAllocateePtr with a mock_binding::SampleAllocateePtr as its internal variant.
/// \details In this overload a raw-pointer to a SampleType is passed in, which is non-owning and intended for
/// stack-allocated fakes/allocation buffers. The returned SampleAllocateePtr will not take ownership of the pointer.
/// The deleter won't try to delete the given pointer!
/// \note From the context one could expect this method being named MakeMockSampleAllocateePtr, but the internal pointer
/// of type mock_binding::SampleAllocateePtr being created is no real mock in the sense of gmock. It is just a simple
/// alias to a std::unique_ptr, thus "MakeFakeSampleAllocateePtr" is a more appropriate name.
template <typename SampleType>
SampleAllocateePtr<SampleType> MakeFakeSampleAllocateePtr(SampleType* fake_sample_allocatee_ptr)
{
    return impl::MakeSampleAllocateePtr(
        mock_binding::SampleAllocateePtr{fake_sample_allocatee_ptr, [](void*) noexcept {}});
}

/// \brief Overload taking ownership of fake_sample_allocatee_ptr, deleting it once the last reference is released.
///
/// Unlike the raw-pointer overload above (which is non-owning and intended for stack-allocated fakes), this overload
/// is used when the caller wants to transfer ownership of a heap-allocated fake sample to the returned
/// SampleAllocateePtr.
template <typename SampleType>
SampleAllocateePtr<SampleType> MakeFakeSampleAllocateePtr(std::unique_ptr<SampleType> fake_sample_allocatee_ptr)
{
    return impl::MakeSampleAllocateePtr(
        mock_binding::SampleAllocateePtr{fake_sample_allocatee_ptr.release(), [](void* ptr) noexcept {
                                             delete static_cast<SampleType*>(ptr);
                                         }});
}

template <typename SampleType>
SamplePtr<SampleType> MakeFakeSamplePtr(std::unique_ptr<SampleType> fake_sample_ptr)
{
    return SamplePtr<SampleType>(std::move(fake_sample_ptr));
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_MOCKING_TEST_TYPE_UTILITIES_H
