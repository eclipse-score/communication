/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#ifndef SCORE_MW_COM_TEST_GENERIC_PROXY_UNSUBSCRIBE_ON_DESTRUCTION_TEST_EVENT_DATATYPE_H
#define SCORE_MW_COM_TEST_GENERIC_PROXY_UNSUBSCRIBE_ON_DESTRUCTION_TEST_EVENT_DATATYPE_H

#include "score/mw/com/types.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace score::mw::com::test
{

constexpr std::string_view kTestEventName{"test_event"};

/// \brief Number of samples the provider sends and the consumer waits for before destroying its GenericProxy
constexpr std::size_t kNumberOfSamples{5U};

/// \brief Test interface template following the mw::com traits pattern
/// \tparam T Either ProxyTrait or SkeletonTrait
template <typename T>
class TestEventInterface : public T::Base
{
  public:
    using T::Base::Base;

    /// \brief Single event which the consumer accesses via a GenericProxy
    typename T::template Event<std::uint32_t> test_event{*this, kTestEventName};
};

/// \brief Skeleton side of the test service. The consumer side uses a GenericProxy, so no typed proxy is needed.
using TestEventSkeleton = score::mw::com::AsSkeleton<TestEventInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_GENERIC_PROXY_UNSUBSCRIBE_ON_DESTRUCTION_TEST_EVENT_DATATYPE_H
