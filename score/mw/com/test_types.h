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
#ifndef SCORE_MW_COM_TEST_TYPES_H
#define SCORE_MW_COM_TEST_TYPES_H

#include "score/mw/com/types.h"

#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/mocking/proxy_event_mock.h"
#include "score/mw/com/impl/mocking/proxy_wrapper_class_test_view.h"
#include "score/mw/com/impl/mocking/skeleton_event_mock.h"
#include "score/mw/com/impl/mocking/skeleton_wrapper_class_test_view.h"
#include "score/mw/com/impl/mocking/test_type_utilities.h"
#include "score/mw/com/mocking/i_runtime.h"

#include <cstdint>
#include <memory>

/// @brief This header contains all of the public functions and types that should be used by a user in order to mock
/// mw::com.
///
/// test_types does not contain any implementation, but instead simply exposes specific internals of mw::com.
namespace score::mw::com
{
namespace runtime
{

void InjectRuntimeMock(IRuntime& runtime_mock);

}

using HandleType = impl::HandleType;

template <typename SampleType>
using ProxyEventMock = impl::ProxyEventMock<SampleType>;

template <typename ProxyWrapperClass>
using ProxyWrapperClassTestView = impl::ProxyWrapperClassTestView<ProxyWrapperClass>;

template <typename EventType>
using NamedProxyEventMock = impl::NamedProxyEventMock<EventType>;

template <typename FieldType>
using NamedProxyFieldMock = impl::NamedProxyFieldMock<FieldType>;

using SkeletonBaseMock = impl::SkeletonBaseMock;

template <typename SampleType>
using SkeletonEventMock = impl::SkeletonEventMock<SampleType>;

template <typename SampleType>
using SkeletonFieldMock = impl::SkeletonFieldMock<SampleType>;

template <typename SkeletonWrapperClass>
using SkeletonWrapperClassTestView = impl::SkeletonWrapperClassTestView<SkeletonWrapperClass>;

template <typename EventType>
using NamedSkeletonEventMock = impl::NamedSkeletonEventMock<EventType>;

template <typename FieldType, typename... Tags>
using NamedSkeletonFieldMock = impl::NamedSkeletonFieldMock<FieldType, Tags...>;

InstanceIdentifier MakeFakeInstanceIdentifier(const std::uint16_t unique_identifier);

HandleType MakeFakeHandle(const std::uint16_t unique_identifier);

/// @brief Resets the configuration objects which are used by fake InstanceIdentifiers / Handles and stored in a static
/// context (created with MakeFakeInstanceIdentifier / MakeFakeHandler above)
///
/// This function should be called in the TearDown of any test which calls MakeFakeInstanceIdentifier or
/// MakeFakeHandler.
void ResetInstanceIdentifierConfiguration();

template <typename SampleType>
SampleAllocateePtr<SampleType> MakeFakeSampleAllocateePtr(std::unique_ptr<SampleType> fake_sample_allocatee_ptr)
{
    return impl::MakeFakeSampleAllocateePtr(std::move(fake_sample_allocatee_ptr));
}

template <typename SampleType>
SamplePtr<SampleType> MakeFakeSamplePtr(std::unique_ptr<SampleType> fake_sample_ptr)
{
    return impl::MakeFakeSamplePtr(std::move(fake_sample_ptr));
}

}  // namespace score::mw::com

#endif  // SCORE_MW_COM_TEST_TYPES_H
