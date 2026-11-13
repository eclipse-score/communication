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

#ifndef SCORE_MW_COM_TEST_HV_GATEWAY_TEST_COMMON_SERVICE_INTERFACE_H
#define SCORE_MW_COM_TEST_HV_GATEWAY_TEST_COMMON_SERVICE_INTERFACE_H

#include "score/mw/com/types.h"

#include <cstdint>

namespace score::mw::com::gateway::test
{

template <typename Trait>
class GatewayTestService : public Trait::Base
{
  public:
    using Trait::Base::Base;

    typename Trait::template Event<std::uint64_t> test_event{*this, "test_event"};
};

using GatewayTestSkeleton = score::mw::com::AsSkeleton<GatewayTestService>;
using GatewayTestProxy = score::mw::com::AsProxy<GatewayTestService>;

}  // namespace score::mw::com::gateway::test

#endif  // SCORE_MW_COM_TEST_HV_GATEWAY_TEST_COMMON_SERVICE_INTERFACE_H
