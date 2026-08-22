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

template <typename SampleType>
SampleAllocateePtr<SampleType> MakeFakeSampleAllocateePtr(SampleType* fake_sample_allocatee_ptr)
{
    return impl::MakeSampleAllocateePtr(
        mock_binding::SampleAllocateePtr{fake_sample_allocatee_ptr, [](void*) noexcept {}});
}

template <typename SampleType>
SamplePtr<SampleType> MakeFakeSamplePtr(std::unique_ptr<SampleType> fake_sample_ptr)
{
    return SamplePtr<SampleType>(std::move(fake_sample_ptr));
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_MOCKING_TEST_TYPE_UTILITIES_H
