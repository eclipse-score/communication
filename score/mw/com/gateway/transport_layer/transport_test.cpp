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
#include "score/mw/com/gateway/transport_layer/transport.h"

#include "score/mw/com/gateway/transport_layer/transport_error.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/service_element_type.h"

#include <gtest/gtest.h>

namespace score::mw::com::gateway
{
namespace
{

/// \brief Minimal concrete Transport implementing only the pure virtual APIs, so that the default (non-pure)
/// implementations of ForwardSamples()/Subscribe()/Unsubscribe() can be exercised directly.
class MinimalTransport final : public Transport
{
  public:
    bool IsMemorySharingSupported() const override
    {
        return true;
    }
    score::Result<void> Setup() override
    {
        return {};
    }
    void Shutdown() override {}
    score::Result<void> ProvideService(score::mw::com::InstanceSpecifier,
                                       std::vector<score::mw::com::EventInfo>) override
    {
        return {};
    }
    score::Result<void> OfferService(score::mw::com::InstanceSpecifier) override
    {
        return {};
    }
    score::Result<void> StopOfferService(score::mw::com::InstanceSpecifier) override
    {
        return {};
    }
    score::Result<void> NotifyUpdate(score::mw::com::InstanceSpecifier, impl::ServiceElementType, std::string) override
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
};

class TransportNoMemorySharingApisTest : public ::testing::Test
{
  protected:
    impl::InstanceSpecifier specifier_{impl::InstanceSpecifier::Create("svc/a").value()};
    MinimalTransport transport_{};
};

TEST_F(TransportNoMemorySharingApisTest, ForwardSamplesDefaultsToNotSupported)
{
    const auto result = transport_.ForwardSamples(specifier_, {});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kNotSupported);
}

TEST_F(TransportNoMemorySharingApisTest, SubscribeDefaultsToNotSupported)
{
    const auto result = transport_.Subscribe(specifier_, impl::ServiceElementType::EVENT, "event");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kNotSupported);
}

TEST_F(TransportNoMemorySharingApisTest, UnsubscribeDefaultsToNotSupported)
{
    const auto result = transport_.Unsubscribe(specifier_, impl::ServiceElementType::EVENT, "event");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TransportErrorc::kNotSupported);
}

}  // namespace
}  // namespace score::mw::com::gateway
