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

#include "score/mw/service/proxy_container.h"

#include "score/concurrency/future/interruptible_promise.h"
#include "score/mw/service/test_doubles/test_doubles.h"

#include "gtest/gtest.h"

#include <cstdint>
#include <memory>
#include <tuple>
#include <utility>

namespace score::mw::service::test
{
namespace
{

class CountingStopServiceDiscoveryAction final : public StopServiceDiscoveryAction
{
  public:
    explicit CountingStopServiceDiscoveryAction(std::uint32_t& stop_counter) noexcept : stop_counter_{stop_counter} {}

    void Stop() noexcept override
    {
        ++stop_counter_;
    }

  private:
    std::uint32_t& stop_counter_;
};

TEST(ProxyContainerDeathTest, StoreAndExtractSingleProxy)
{
    // Given a proxy container that contains a single proxy with its stop action
    auto container_content = std::make_tuple(std::make_unique<FakeProxy>(42));

    ProxyContainer<FakeProxyBase> unit{std::move(container_content)};
    ASSERT_TRUE(unit.Has<FakeProxyBase>());

    // When extracting it from the container
    auto proxy = unit.Extract<FakeProxyBase>();
    EXPECT_FALSE(unit.Has<FakeProxyBase>());

    // Then it must be valid and the expected value must be returned
    ASSERT_NE(proxy, nullptr);
    EXPECT_EQ(proxy->What(), 42);

    // When attempting to extract it from the container once more, then termination is expected
    ASSERT_EXIT(
        { [[maybe_unused]] auto unused = unit.Extract<FakeProxyBase>(); }, ::testing::KilledBySignal(SIGABRT), "");
}

TEST(ProxyContainerTest, StoreAndRetrieveSingleProxy)
{
    // Given a proxy container that contains a single proxy
    auto container_content = std::make_tuple(std::make_unique<FakeProxy>(42));
    ProxyContainer<FakeProxyBase> unit{std::move(container_content)};

    // When retrieving it as reference from the container via lvalue overload
    auto& proxy = unit.Get<FakeProxyBase>();
    ASSERT_NE(proxy, nullptr);

    // Then the correct one is accessed
    EXPECT_EQ(proxy->What(), 42);
}

TEST(ProxyContainerDeathTest, GetAfterExtractFails)
{
    // Given a proxy container that contains a single proxy
    auto container_content = std::make_tuple(std::make_unique<FakeProxy>(42));
    ProxyContainer<FakeProxyBase> unit{std::move(container_content)};

    // When extracting the proxy from the container
    auto proxy = unit.Extract<FakeProxyBase>();
    ASSERT_NE(proxy, nullptr);
    EXPECT_EQ(proxy->What(), 42);
    EXPECT_FALSE(unit.Has<FakeProxyBase>());

    // Then attempting to Get the extracted proxy must fail
    ASSERT_EXIT({ [[maybe_unused]] auto& unused = unit.Get<FakeProxyBase>(); }, ::testing::KilledBySignal(SIGABRT), "");
}

TEST(ProxyContainerTest, StoreAndExtractMultipleProxiesThatAreDifferent)
{
    // Given a proxy container that contains multiple (different) proxy instances
    auto container_content = std::make_tuple(std::make_unique<FakeProxy>(42), std::make_unique<OtherFakeProxy>());
    ProxyContainer<FakeProxyBase, OtherFakeProxyBase> unit{std::move(container_content)};
    ASSERT_TRUE(unit.Has<OtherFakeProxyBase>());
    ASSERT_TRUE(unit.Has<FakeProxyBase>());

    // When retrieving both
    auto fake_proxy = unit.Extract<FakeProxyBase>();
    EXPECT_FALSE(unit.Has<FakeProxyBase>());
    ASSERT_TRUE(unit.Has<OtherFakeProxyBase>());
    auto other_fake_proxy = unit.Extract<OtherFakeProxyBase>();
    EXPECT_FALSE(unit.Has<OtherFakeProxyBase>());
    EXPECT_FALSE(unit.Has<FakeProxyBase>());

    // Then both instances must have gotten extracted correctly
    ASSERT_NE(fake_proxy, nullptr);
    EXPECT_EQ(fake_proxy->What(), 42);
    ASSERT_NE(other_fake_proxy, nullptr);
}

TEST(ProxyContainerTest, StoreAndRetrieveMultipleProxiesThatAreDifferent)
{
    // Given a proxy container that contains multiple (different) proxy instances
    auto container_content = std::make_tuple(std::make_unique<FakeProxy>(42), std::make_unique<OtherFakeProxy>());
    ProxyContainer<FakeProxyBase, OtherFakeProxyBase> unit{std::move(container_content)};

    // When retrieving both
    auto& proxy = unit.Get<FakeProxyBase>();
    auto& other_proxy = unit.Get<OtherFakeProxyBase>();

    // Then both instances are correctly retrieved
    EXPECT_EQ(proxy->What(), 42);
    EXPECT_NE(other_proxy, nullptr);
}

TEST(ProxyContainerTest, StoreAndRetrieveMultipleProxiesOfSameClass)
{
    // Given a proxy container that contains multiple proxy instances of the same class
    auto container_content = std::make_tuple(std::make_unique<FakeProxy>(42), std::make_unique<FakeProxy>(43));
    ProxyContainer<FakeProxyBase, FakeProxyBase> unit{std::move(container_content)};

    // When retrieving both of them
    auto& proxy = unit.Get<FakeProxyBase>();           // retrieve by default parameter index 0
    auto& other_proxy = unit.Get<FakeProxyBase, 1>();  // retrieve by custom parameter index 1

    // Then both are correctly ordered returned
    EXPECT_EQ(proxy->What(), 42);
    EXPECT_EQ(other_proxy->What(), 43);
}

TEST(ProxyContainerTest, StoreAndRetrieveMultipleProxiesOfTheSameClassWithDifferentSpecs)
{
    // Given a proxy container that contains different instances of the same class, but with different specs (mandatory
    // and optional)
    auto container_content =
        std::make_tuple(std::make_unique<FakeProxy>(42),
                        OptionalProxyData<FakeProxyBase>{ProxyFuture<SingleInstanceHolder<FakeProxyBase>>{}, nullptr});
    ProxyContainer<FakeProxyBase, Optional<FakeProxyBase>> unit{std::move(container_content)};

    // When retrieving both instances
    auto& proxy = unit.Get<FakeProxyBase>();
    auto& optional_proxy = unit.Get<Optional<FakeProxyBase>>();

    // Then both instances are found correctly and associated
    EXPECT_EQ(proxy->What(), 42);
    EXPECT_FALSE(optional_proxy.Valid());
    EXPECT_FALSE(optional_proxy.Ready());
}

TEST(ProxyContainerTest, StoreAndRetrieveSingleOptionalProxy)
{
    // Given a proxy container that contains an optional proxy
    score::concurrency::InterruptiblePromise<SingleInstanceHolder<FakeProxyBase>> promise{};
    promise.SetValue(std::make_unique<FakeProxy>(42));

    auto container_content = std::make_tuple(OptionalProxyData<FakeProxyBase>{
        ProxyFuture<SingleInstanceHolder<FakeProxyBase>>{std::move(*promise.GetInterruptibleFuture())}, nullptr});
    ProxyContainer<Optional<FakeProxyBase>> unit{std::move(container_content)};

    // When retrieving that proxy
    auto& optional_proxy = unit.Get<Optional<FakeProxyBase>>();

    // Then the returned proxy equals the expected one.
    EXPECT_EQ(optional_proxy.Get({}).value()->What(), 42);
}

TEST(ProxyContainerTest, StoreAndRetrieveOptionalProxy)
{
    // Given a proxy container that contains an optional proxy
    score::concurrency::InterruptiblePromise<SingleInstanceHolder<FakeProxyBase>> promise{};
    promise.SetValue(std::make_unique<FakeProxy>(42));

    auto container_content = std::make_tuple(
        std::make_unique<FakeProxy>(42),
        OptionalProxyData<FakeProxyBase>{
            ProxyFuture<SingleInstanceHolder<FakeProxyBase>>{std::move(*promise.GetInterruptibleFuture())}, nullptr});
    ProxyContainer<FakeProxyBase, Optional<FakeProxyBase>> unit{std::move(container_content)};

    // When retrieving that proxy
    auto& optional_proxy = unit.Get<Optional<FakeProxyBase>>();

    // Then the returned proxy equals the expected one.
    EXPECT_EQ(optional_proxy.Get({}).value()->What(), 42);
}

TEST(ProxyContainerTest, StoreAndRetrieveAlternativeProxy)
{
    // Given a proxy container that contains a variant of possible proxies
    using ProxySpec = Variant<FakeProxyBase, OtherFakeProxyBase>;

    SingleInstanceHolder<FakeProxyBase> proxy_instance = std::make_unique<FakeProxy>(42);
    auto container_content =
        std::make_tuple(std::variant<SingleInstanceHolder<FakeProxyBase>, SingleInstanceHolder<OtherFakeProxyBase>>{
            std::move(proxy_instance)});
    ProxyContainer<ProxySpec> unit{std::move(container_content)};

    // When retrieving it from the container
    auto& variant_proxy = unit.Get<ProxySpec>();

    // Then the correct one is accessed
    EXPECT_EQ(std::get<SingleInstanceHolder<FakeProxyBase>>(variant_proxy)->What(), 42);
}

TEST(ProxyContainerTest, StoreAndRetrieveOptionalAlternativeProxy)
{
    // Given a proxy container that maybe contains a variant of possible proxies
    using ProxySpec = Optional<Variant<FakeProxyBase, OtherFakeProxyBase>>;
    SingleInstanceHolder<FakeProxyBase> proxy_instance = std::make_unique<FakeProxy>(42);
    score::concurrency::InterruptiblePromise<
        std::variant<SingleInstanceHolder<FakeProxyBase>, SingleInstanceHolder<OtherFakeProxyBase>>>
        promise{};
    promise.SetValue({std::move(proxy_instance)});

    using VariantType = std::variant<SingleInstanceHolder<FakeProxyBase>, SingleInstanceHolder<OtherFakeProxyBase>>;
    auto container_content = std::make_tuple(OptionalProxyData<Variant<FakeProxyBase, OtherFakeProxyBase>>{
        ProxyFuture<VariantType>{std::move(promise.GetInterruptibleFuture()).value()}, nullptr});
    ProxyContainer<ProxySpec> unit{std::move(container_content)};

    // When retrieving it from the container
    auto variant_value = std::move(unit.Get<ProxySpec>().Get({})).value();

    // Then the correct one is accessed
    EXPECT_EQ(std::get<SingleInstanceHolder<FakeProxyBase>>(variant_value)->What(), 42);
}

TEST(ProxyDataTest, MoveAssignmentStopsExistingServiceDiscoveryAndTakesOverNewAction)
{
    std::uint32_t lhs_stop_count{0U};
    std::uint32_t rhs_stop_count{0U};

    OptionalProxyData<FakeProxyBase> lhs{ProxyFuture<SingleInstanceHolder<FakeProxyBase>>{},
                                         std::make_unique<CountingStopServiceDiscoveryAction>(lhs_stop_count)};
    OptionalProxyData<FakeProxyBase> rhs{ProxyFuture<SingleInstanceHolder<FakeProxyBase>>{},
                                         std::make_unique<CountingStopServiceDiscoveryAction>(rhs_stop_count)};

    lhs = std::move(rhs);

    EXPECT_EQ(lhs_stop_count, 1U);
    EXPECT_EQ(rhs_stop_count, 0U);

    lhs.StopServiceDiscovery();
    EXPECT_EQ(rhs_stop_count, 1U);
}

}  // namespace
}  // namespace score::mw::service::test
