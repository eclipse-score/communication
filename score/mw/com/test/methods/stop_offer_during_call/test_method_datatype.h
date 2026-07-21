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
#ifndef SCORE_MW_COM_TEST_METHODS_STOP_OFFER_DURING_CALL_METHOD_DATATYPE_H
#define SCORE_MW_COM_TEST_METHODS_STOP_OFFER_DURING_CALL_METHOD_DATATYPE_H

#include "score/mw/com/types.h"

#include <cstdint>

namespace score::mw::com::test
{

/// \brief Test interface for StopOfferService during active method call scenario
template <typename T>
class StopOfferDuringCallInterface : public T::Base
{
  public:
    using T::Base::Base;

    /// \brief Method that simulates a long-running operation
    /// The handler will sleep for the specified milliseconds to allow testing
    /// StopOfferService being called while the method is executing
    typename T::template Method<std::int32_t(std::int32_t)> long_running_method{*this, "long_running_method"};
};

/// \brief Proxy side of the test service
using StopOfferDuringCallProxy = score::mw::com::AsProxy<StopOfferDuringCallInterface>;

/// \brief Skeleton side of the test service
using StopOfferDuringCallSkeleton = score::mw::com::AsSkeleton<StopOfferDuringCallInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_METHODS_STOP_OFFER_DURING_CALL_METHOD_DATATYPE_H
