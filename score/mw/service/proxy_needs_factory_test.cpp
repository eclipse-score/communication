/*********************************************************************************
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
 *******************************************************************************/

#include "score/mw/service/proxy_needs_factory.h"

#include "score/mw/service/find_service_strategy.h"
#include "score/mw/service/test_doubles/test_doubles.h"

#include "gtest/gtest.h"

#include <atomic>
#include <set>

namespace score::mw::service::test
{
namespace
{

class TestStrategyWithPortIdentifier : public FindServiceStrategy
{
  public:
    using BaseProxy = FakeProxyBase;

    TestStrategyWithPortIdentifier() = default;
    TestStrategyWithPortIdentifier(const std::string_view) {}

    void Find(std::unique_ptr<typename ProxyBuilderBase<FakeProxyBase>::BuilderCallback> on_found)
    {
        score::cpp::ignore = on_found;
    }

    void StopFind() noexcept override {}
};

TEST(ProxyNeedsFactoryTest, CreateWithProvidedStrategyInstancesAsUniquePtrs)
{
    // Given that two single mandatory and one optional proxy are needed
    using TestNeeds = ProxyNeeds<FakeProxyBase, FakeProxyBase, Optional<FakeProxyBase>>;

    // When creating a `ProxyNeeds` instance via `ProxyNeedsFactory` in conj. with `TestStrategyWithPortIdentifier`
    auto unit =
        ProxyNeedsFactory<TestNeeds>::Create(std::make_unique<TestStrategyWithPortIdentifier>("port/identifier/1"),
                                             std::make_unique<TestStrategyWithPortIdentifier>("port/identifier/2"),
                                             std::make_unique<TestStrategyWithPortIdentifier>("port/identifier/3"));

    // And initiating service discovery without providing a stop_token
    auto requested_proxies = unit.InitiateServiceDiscovery();

    // Then three requested proxies must get returned
    ASSERT_EQ(3U, requested_proxies.Count());
}

TEST(ProxyNeedsFactoryTest, CreateWithProvidedStrategyInstances)
{
    // Given that two single mandatory and one optional proxy are needed
    using TestNeeds = ProxyNeeds<FakeProxyBase, FakeProxyBase, Optional<FakeProxyBase>>;

    // When creating a `ProxyNeeds` instance via `ProxyNeedsFactory` in conj. with `TestStrategyWithPortIdentifier`
    auto unit = ProxyNeedsFactory<TestNeeds>::Create(TestStrategyWithPortIdentifier{"port/identifier/1"},
                                                     TestStrategyWithPortIdentifier{"port/identifier/2"},
                                                     TestStrategyWithPortIdentifier{"port/identifier/3"});

    // And initiating service discovery without providing a stop_token
    auto requested_proxies = unit.InitiateServiceDiscovery();

    // Then three requested proxies must get returned
    ASSERT_EQ(3U, requested_proxies.Count());
}

TEST(ProxyNeedsFactoryTest, CreateWithProvidedStrategyTypes)
{
    // Given that two single mandatory and one optional proxy are needed
    using TestNeeds = ProxyNeeds<FakeProxyBase, FakeProxyBase, Optional<FakeProxyBase>>;

    // When creating a `ProxyNeeds` instance via `ProxyNeedsFactory` in conj. with `TestStrategyWithPortIdentifier`
    auto unit = ProxyNeedsFactory<TestNeeds>::
        Create<TestStrategyWithPortIdentifier, TestStrategyWithPortIdentifier, TestStrategyWithPortIdentifier>();

    // And initiating service discovery without providing a stop_token
    auto requested_proxies = unit.InitiateServiceDiscovery();

    // Then three requested proxies must get returned
    ASSERT_EQ(3U, requested_proxies.Count());
}

TEST(ProxyNeedsFactoryTest, EmptyRequestedProxies)
{
    // Given a default-constructed `RequestedProxies` instance
    RequestedProxies<FakeProxyBase, FakeProxyBase, Optional<FakeProxyBase>> requested_proxies{};

    // When attempting to count their amount
    // Then zero must be returned
    EXPECT_EQ(0U, requested_proxies.Count());
}

// see score/mw/service/proxy_needs_test.cpp
// for furter unit tests concerning
// mw::service::ProxyNeedsFactory

}  // namespace
}  // namespace score::mw::service::test
