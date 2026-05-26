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

#ifndef SCORE_MW_COM_GATEWAY_CORE_MOCK_H
#define SCORE_MW_COM_GATEWAY_CORE_MOCK_H

#include "score/mw/com/gateway/gateway_application/gateway_core.h"

#include <gmock/gmock.h>

namespace score::mw::com::gateway
{

class GatewayCoreMock : public GatewayCore
{
  public:
    MOCK_METHOD((score::Result<void>),
                ProvideService,
                (impl::InstanceSpecifier, std::vector<ServiceElementConfiguration>),
                (override));
    MOCK_METHOD((score::Result<void>), OfferService, (impl::InstanceSpecifier), (override));
    MOCK_METHOD(void, StopOfferService, (impl::InstanceSpecifier), (override));
    MOCK_METHOD((score::Result<void>),
                NotifyUpdate,
                (impl::InstanceSpecifier, impl::ServiceElementType, std::string),
                (override));
    MOCK_METHOD((score::Result<void>),
                RegisterUpdateNotification,
                (impl::InstanceSpecifier, impl::ServiceElementType, std::string),
                (override));
    MOCK_METHOD((score::Result<void>),
                UnregisterUpdateNotification,
                (impl::InstanceSpecifier, impl::ServiceElementType, std::string),
                (override));
};

}  // namespace score::mw::com::gateway

#endif  // SCORE_MW_COM_GATEWAY_CORE_MOCK_H
