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
#include "score/mw/com/gateway/gateway_application/gateway_core.h"

#include "score/mw/com/gateway/gateway_application/gateway_error.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/service_element_type.h"

#include <gtest/gtest.h>

namespace score::mw::com::gateway
{
namespace
{

/// \brief Minimal concrete GatewayCore implementing only the pure virtual APIs, so that the default (non-pure)
/// implementations of AllocateSamples()/SendOrUpdateSamples()/Subscribe()/Unsubscribe() can be exercised directly.
class MinimalGatewayCore final : public GatewayCore
{
  public:
    score::Result<void> ProvideService(score::mw::com::InstanceSpecifier,
                                       std::vector<score::mw::com::EventInfo>) override
    {
        return {};
    }
    void StopOfferService(score::mw::com::InstanceSpecifier) override {}
    score::Result<void> OfferService(score::mw::com::InstanceSpecifier) override
    {
        return {};
    }
    score::Result<void> RegisterUpdateNotification(score::mw::com::InstanceSpecifier,
                                                   impl::ServiceElementType,
                                                   std::string) override
    {
        return {};
    }
    score::Result<void> UnregisterUpdateNotification(score::mw::com::InstanceSpecifier,
                                                     impl::ServiceElementType,
                                                     std::string) override
    {
        return {};
    }
    score::Result<void> NotifyUpdate(score::mw::com::InstanceSpecifier, impl::ServiceElementType, std::string) override
    {
        return {};
    }
};

class GatewayCoreNoMemorySharingApisTest : public ::testing::Test
{
  protected:
    impl::InstanceSpecifier specifier_{impl::InstanceSpecifier::Create("svc/a").value()};
    MinimalGatewayCore core_{};
};

TEST_F(GatewayCoreNoMemorySharingApisTest, AllocateSamplesDefaultsToNotSupported)
{
    const auto result = core_.AllocateSamples(specifier_, {});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), GatewayErrorc::kNotSupported);
}

TEST_F(GatewayCoreNoMemorySharingApisTest, SendOrUpdateSamplesDefaultsToNotSupported)
{
    const auto result = core_.SendOrUpdateSamples(specifier_, {});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), GatewayErrorc::kNotSupported);
}

TEST_F(GatewayCoreNoMemorySharingApisTest, SubscribeDefaultsToNotSupported)
{
    const auto result = core_.Subscribe(specifier_, impl::ServiceElementType::EVENT, "event");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), GatewayErrorc::kNotSupported);
}

TEST_F(GatewayCoreNoMemorySharingApisTest, UnsubscribeDefaultsToNotSupported)
{
    const auto result = core_.Unsubscribe(specifier_, impl::ServiceElementType::EVENT, "event");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), GatewayErrorc::kNotSupported);
}

}  // namespace
}  // namespace score::mw::com::gateway
