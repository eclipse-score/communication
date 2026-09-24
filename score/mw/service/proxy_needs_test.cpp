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

#include "score/mw/service/proxy_needs.h"

#include "score/mw/service/find_service_strategy.h"
#include "score/mw/service/proxy_needs_factory.h"
#include "score/mw/service/test_doubles/test_doubles.h"
#include "score/concurrency/interruptible_wait.h"

#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <future>
#include <memory>

namespace score::mw::service::test
{
namespace
{

std::chrono::milliseconds gWaitTimeToServiceFoundIFakeProxy{0};
std::chrono::milliseconds gWaitTimeToServiceFoundIOtherFakeProxy{0};
std::future<void> gAsyncFindFutureIFakeProxy{};
std::future<void> gAsyncFindFutureIOtherFakeProxy{};
std::atomic_uint64_t gStopFindInvokedCount{0};
score::cpp::stop_source gWaitStopSource{};

class TestNotFoundStrategy : public FindServiceStrategy
{
  public:
    using BaseProxy = FakeProxyBase;

    void Find(std::unique_ptr<typename ProxyBuilderBase<FakeProxyBase>::BuilderCallback> on_found)
    {
        score::cpp::ignore = on_found;
    }

    void StopFind() noexcept override
    {
        ++gStopFindInvokedCount;
    }
};

class TestProxyFindStrategy : public FindServiceStrategy
{
  public:
    using BaseProxy = FakeProxyBase;

    void Find(std::unique_ptr<typename ProxyBuilderBase<FakeProxyBase>::BuilderCallback> on_found)
    {
        gAsyncFindFutureIFakeProxy = std::async(std::launch::async, [on_found = std::move(on_found)]() mutable {
            score::concurrency::wait_for(gWaitStopSource.get_token(), gWaitTimeToServiceFoundIFakeProxy);
            auto found_service = std::make_unique<FakeProxy>(42);
            (*on_found)(std::move(found_service));
        });
    }

    void StopFind() noexcept override {}
};

class TestProxyFindStrategyMultiInstances : public FindServiceStrategy
{
  public:
    using BaseProxy = Multiple<FakeProxyBase>;

    void Find(std::unique_ptr<typename ProxyBuilderBase<Multiple<FakeProxyBase>>::BuilderCallback> on_found)
    {
        gAsyncFindFutureIFakeProxy = std::async(std::launch::async, [on_found = std::move(on_found)]() mutable {
            score::concurrency::wait_for(gWaitStopSource.get_token(), gWaitTimeToServiceFoundIFakeProxy);
            auto found_service = std::make_unique<FakeProxy>(42);
            (*on_found)(std::move(found_service));

            // simulate that a second service instance was found
            found_service = std::make_unique<FakeProxy>(44);
            (*on_found)(std::move(found_service));
        });
    }

    void StopFind() noexcept override {}
};

class TestOtherProxyFindStrategy : public FindServiceStrategy
{
  public:
    using BaseProxy = OtherFakeProxyBase;

    void Find(std::unique_ptr<typename ProxyBuilderBase<OtherFakeProxyBase>::BuilderCallback> on_found)
    {
        gAsyncFindFutureIOtherFakeProxy = std::async(std::launch::async, [on_found = std::move(on_found)]() mutable {
            score::concurrency::wait_for(gWaitStopSource.get_token(), gWaitTimeToServiceFoundIOtherFakeProxy);
            auto found_service = std::make_unique<OtherFakeProxy>();
            (*on_found)(std::move(found_service));
        });
    }

    void StopFind() noexcept override {}
};

class TestAlternativeProxyFindStrategy : public FindServiceStrategy
{
  public:
    using BaseProxy = Variant<FakeProxyBase, OtherFakeProxyBase>;

    void Find(std::unique_ptr<typename ProxyBuilderBase<Variant<FakeProxyBase, OtherFakeProxyBase>>::BuilderCallback>
                  on_found)
    {
        gAsyncFindFutureIFakeProxy = std::async(std::launch::async, [on_found = std::move(on_found)]() mutable {
            score::concurrency::wait_for(gWaitStopSource.get_token(), gWaitTimeToServiceFoundIFakeProxy);
            SingleInstanceHolder<FakeProxyBase> found_service = std::make_unique<FakeProxy>(44);
            (*on_found)({std::move(found_service)});
        });
    }

    void StopFind() noexcept override {}
};

class TestAlternativeProxyFindOtherStrategy : public FindServiceStrategy
{
  public:
    using BaseProxy = Variant<FakeProxyBase, OtherFakeProxyBase>;

    void Find(std::unique_ptr<typename ProxyBuilderBase<Variant<FakeProxyBase, OtherFakeProxyBase>>::BuilderCallback>
                  on_found)
    {
        gAsyncFindFutureIOtherFakeProxy = std::async(std::launch::async, [on_found = std::move(on_found)]() mutable {
            score::concurrency::wait_for(gWaitStopSource.get_token(), gWaitTimeToServiceFoundIOtherFakeProxy);
            SingleInstanceHolder<OtherFakeProxyBase> found_service = std::make_unique<OtherFakeProxy>();
            (*on_found)({std::move(found_service)});
        });
    }

    void StopFind() noexcept override {}
};

class ProxyNeedsTest : public ::testing::Test
{
  public:
    void TearDown() override
    {
        gWaitStopSource.request_stop();
        if (gAsyncFindFutureIFakeProxy.valid())
        {
            gAsyncFindFutureIFakeProxy.get();
        }

        if (gAsyncFindFutureIOtherFakeProxy.valid())
        {
            gAsyncFindFutureIOtherFakeProxy.get();
        }
        gStopFindInvokedCount = 0;
        gWaitStopSource = score::cpp::stop_source{};
        gWaitTimeToServiceFoundIFakeProxy = std::chrono::milliseconds{0};
        gWaitTimeToServiceFoundIOtherFakeProxy = std::chrono::milliseconds{0};
    }
};

TEST_F(ProxyNeedsTest, WithOnServiceFound)
{
    // Given that one single mandatory proxy is needed
    using TestNeeds = ProxyNeeds<FakeProxyBase>;
    bool was_callback_executed{false};
    auto on_found_callback = [&was_callback_executed](const FakeProxyBase& proxy) {
        ASSERT_EQ(42U, proxy.What());
        was_callback_executed = true;
    };

    // When waiting for mandatory proxies in combination with an on-service-found callback
    gWaitTimeToServiceFoundIFakeProxy = std::chrono::seconds{0};
    auto container = ProxyNeedsFactory<TestNeeds>::Create<TestProxyFindStrategy>()
                         .WithOnServiceFound(std::move(on_found_callback))
                         .WaitForMandatoryProxies({});
    auto& proxy = container.Get<FakeProxyBase>();

    // Then the proxy is available and callback was executed
    EXPECT_TRUE(was_callback_executed);
    EXPECT_EQ(proxy->What(), 42);

    // When calling `WithOnServiceFound()` on a ProxyNeeds instance whose internal state does not allow that
    // Then immediate termination is expected due to violated precondition
    ASSERT_EXIT(score::cpp::ignore = TestNeeds{}.WithOnServiceFound({}), ::testing::KilledBySignal(SIGABRT), "");
}

TEST_F(ProxyNeedsTest, InitiateServiceDiscovery)
{
    // Given that one single mandatory proxy is needed
    using TestNeeds = ProxyNeeds<FakeProxyBase>;
    auto unit = ProxyNeedsFactory<TestNeeds>::Create<TestProxyFindStrategy>();

    // When initiating service discovery
    gWaitTimeToServiceFoundIFakeProxy = std::chrono::seconds{0};
    auto proxies = unit.InitiateServiceDiscovery({});

    // Then the proxy must get found correctly
    auto container = proxies.WaitForMandatoryProxies({});
    auto& proxy = container.Get<FakeProxyBase>();
    ASSERT_NE(proxy, nullptr);
    EXPECT_EQ(proxy->What(), 42);

    // When calling `InitiateServiceDiscovery()` once more now
    // Then immediate termination is expected due to violated precondition
    ASSERT_EXIT(score::cpp::ignore = unit.InitiateServiceDiscovery(), ::testing::KilledBySignal{SIGABRT}, "");
    ASSERT_EXIT(score::cpp::ignore = unit.InitiateServiceDiscovery({}), ::testing::KilledBySignal{SIGABRT}, "");
    ASSERT_EXIT(score::cpp::ignore = decltype(unit){}.InitiateServiceDiscovery(), ::testing::KilledBySignal{SIGABRT}, "");
    ASSERT_EXIT(score::cpp::ignore = decltype(unit){}.InitiateServiceDiscovery({}), ::testing::KilledBySignal{SIGABRT}, "");
}

TEST_F(ProxyNeedsTest, InitiateServiceDiscoveryWithMandatoryProxiesButWithoutStopToken)
{
    // Given that one single mandatory and one optional proxy are needed
    using TestNeeds = ProxyNeeds<FakeProxyBase, FakeProxyBase, Optional<FakeProxyBase>>;
    auto unit =
        ProxyNeedsFactory<TestNeeds>::Create<TestNotFoundStrategy, TestNotFoundStrategy, TestNotFoundStrategy>();

    // When initiating service discovery without providing a stop_token
    std::optional<decltype(unit.InitiateServiceDiscovery())> proxies = unit.InitiateServiceDiscovery();

    // Then `TestNotFoundStrategy`'s method `StopFind()` must not have gotten invoked yet
    ASSERT_EQ(0U, gStopFindInvokedCount.load());

    // When destroying the `proxies` object now
    proxies.reset();

    // Then `TestNotFoundStrategy`'s method `StopFind()` must have gotten invoked thrice
    ASSERT_EQ(3U, gStopFindInvokedCount.load());
}

TEST_F(ProxyNeedsTest, InitiateServiceDiscoveryWithOptionalProxiesButWithoutStopToken)
{
    // Given that three optional proxies are needed
    using TestNeeds = ProxyNeeds<Optional<FakeProxyBase>, Optional<FakeProxyBase>, Optional<FakeProxyBase>>;
    auto unit =
        ProxyNeedsFactory<TestNeeds>::Create<TestNotFoundStrategy, TestNotFoundStrategy, TestNotFoundStrategy>();

    // When initiating service discovery without providing a stop_token
    auto proxies = unit.InitiateServiceDiscovery();

    // Then `TestNotFoundStrategy`'s method `StopFind()` must not have gotten invoked yet
    ASSERT_EQ(0U, gStopFindInvokedCount.load());

    // When obtaining the corresponding `ProxyContainer`
    std::optional<decltype(proxies.GetProxyContainer())> container = proxies.GetProxyContainer();

    // Then `TestNotFoundStrategy`'s method `StopFind()` must not have gotten invoked yet
    ASSERT_EQ(0U, gStopFindInvokedCount.load());

    // When extracting one of the `ProxyContainer`'s `ProxyFuture` elements
    std::optional<decltype(container->Extract<Optional<FakeProxyBase>, 1U>())> proxy_future{
        std::in_place_t{}, container->Extract<Optional<FakeProxyBase>, 1U>()};

    // Then `TestNotFoundStrategy`'s method `StopFind()` must not have gotten invoked yet
    ASSERT_EQ(0U, gStopFindInvokedCount.load());

    // When destroying the extracted `ProxyFuture` element
    proxy_future.reset();

    // Then `TestNotFoundStrategy`'s method `StopFind()` must have gotten invoked once
    ASSERT_EQ(1U, gStopFindInvokedCount.load());

    // When resetting the `ProxyContainer` afterwards
    container.reset();

    // Then `TestNotFoundStrategy`'s method `StopFind()` must have gotten invoked twice more
    ASSERT_EQ(3U, gStopFindInvokedCount.load());
}

TEST_F(ProxyNeedsTest, GetProxyContainer)
{
    // Given that one single mandatory proxy is needed
    using TestNeeds = ProxyNeeds<Optional<FakeProxyBase>>;
    gWaitTimeToServiceFoundIFakeProxy = std::chrono::seconds{0};
    auto unit = ProxyNeedsFactory<TestNeeds>::Create<TestProxyFindStrategy>().InitiateServiceDiscovery({});

    // When calling `GetProxyContainer()` after initiating service discovery
    auto container = unit.GetProxyContainer();

    // Then the returned ProxyContainer must have its elements populated
    ASSERT_TRUE(container.Has<Optional<FakeProxyBase>>());

    // When calling `GetProxyContainer()` once more now
    // Then immediate termination is expected due to violated precondition
    ASSERT_EXIT(score::cpp::ignore = unit.GetProxyContainer(), ::testing::KilledBySignal{SIGABRT}, "");
    ASSERT_EXIT(score::cpp::ignore = decltype(unit){}.GetProxyContainer(), ::testing::KilledBySignal{SIGABRT}, "");
}

TEST_F(ProxyNeedsTest, SingleMandatoryProxyWaitWithoutBlocking)
{
    // Given that one single mandatory proxy is needed
    using TestNeeds = ProxyNeeds<FakeProxyBase>;
    auto unit = ProxyNeedsFactory<TestNeeds>::Create<TestProxyFindStrategy>();

    // When waiting for mandatory proxies without blocking
    gWaitTimeToServiceFoundIFakeProxy = std::chrono::seconds{0};
    auto container = unit.WaitForMandatoryProxies({});

    // Then the proxy must be available
    auto& proxy = container.Get<FakeProxyBase>();
    ASSERT_NE(proxy, nullptr);
    EXPECT_EQ(proxy->What(), 42);

    // When calling WaitForMandatoryProxies() once more after that
    // Then immediate termination is expected due to violated precondition
    ASSERT_EXIT(score::cpp::ignore = unit.WaitForMandatoryProxies({}), ::testing::KilledBySignal{SIGABRT}, "");
    ASSERT_EXIT(score::cpp::ignore = decltype(unit){}.WaitForMandatoryProxies({}), ::testing::KilledBySignal{SIGABRT}, "");
}

TEST_F(ProxyNeedsTest, SingleOptionalProxyWaitForever)
{
    // Given that one single optional proxy is needed
    using TestNeeds = ProxyNeeds<Optional<FakeProxyBase>>;
    auto unit = ProxyNeedsFactory<TestNeeds>::Create<TestProxyFindStrategy>();

    // When specifying the optional proxy to get never found
    gWaitTimeToServiceFoundIFakeProxy = std::chrono::hours{10};

    // Then the ProxyContainer must nonetheless be populated immediately
    auto container = unit.InitiateServiceDiscovery({}).GetProxyContainer();

    // And the optional proxy must not be available yet
    ASSERT_TRUE(container.Has<Optional<FakeProxyBase>>());
    EXPECT_FALSE(container.Get<Optional<FakeProxyBase>>().Ready());
}

TEST_F(ProxyNeedsTest, MultipleMandatoryProxyWaitWithoutBlocking)
{
    // Given that multiple mandatory proxies are needed
    using TestNeeds = ProxyNeeds<FakeProxyBase, OtherFakeProxyBase, FakeProxyBase>;
    auto unit = ProxyNeedsFactory<
        TestNeeds>::Create<TestProxyFindStrategy, TestOtherProxyFindStrategy, TestProxyFindStrategy>();

    // When waiting for all mandatory proxies
    auto container = unit.WaitForMandatoryProxies({});

    auto& proxy_1 = container.Get<FakeProxyBase>();
    auto& proxy_2 = container.Get<FakeProxyBase, 1>();

    // Then both proxies must have gotten found and can be accessed
    EXPECT_NE(proxy_1, proxy_2);
    EXPECT_EQ(proxy_1->What(), 42);
    EXPECT_EQ(proxy_2->What(), 42);
    EXPECT_NE(container.Get<OtherFakeProxyBase>(), nullptr);
}

TEST_F(ProxyNeedsTest, MultipleOptionalProxyWaitForever)
{
    // Given that multiple optional proxies are needed
    using TestNeeds = ProxyNeeds<Optional<FakeProxyBase>, Optional<OtherFakeProxyBase>>;
    auto unit = ProxyNeedsFactory<TestNeeds>::Create<TestProxyFindStrategy, TestOtherProxyFindStrategy>();

    // When waiting for all proxies, while the first service gets set to get never found
    gWaitTimeToServiceFoundIFakeProxy = std::chrono::hours{10};
    auto container = unit.InitiateServiceDiscovery({}).GetProxyContainer();

    // Then one proxy was not found, while the other was
    ASSERT_TRUE(container.Has<Optional<FakeProxyBase>>());
    EXPECT_FALSE(container.Get<Optional<FakeProxyBase>>().Ready());
    ASSERT_TRUE(container.Has<Optional<OtherFakeProxyBase>>());
    EXPECT_TRUE(container.Get<Optional<OtherFakeProxyBase>>().WaitFor({}, std::chrono::seconds{1}).has_value());
}

TEST_F(ProxyNeedsTest, MultipleMandatoryProxiesWaitForEverAndAreStopedViaToken)
{
    // Given multiple mandatory proxies
    using TestNeeds = ProxyNeeds<FakeProxyBase, OtherFakeProxyBase>;
    auto unit = ProxyNeedsFactory<TestNeeds>::Create<TestProxyFindStrategy, TestOtherProxyFindStrategy>();

    // When waiting for all mandatory proxies until forever, while the stop was requested
    score::cpp::stop_source source{};
    gWaitTimeToServiceFoundIFakeProxy = std::chrono::hours{100};
    source.request_stop();
    auto container = unit.WaitForMandatoryProxies(source.get_token());

    // Then no proxy is found
    EXPECT_EQ(container.Get<FakeProxyBase>(), nullptr);
}

#if 0  // disabled temporarily due to testing unused code as well as being the reason for Ticket-63214
TEST_F(ProxyNeedsTest, MultipleMandatoryProxiesAreFoundWhileWaitingWithTimeout)
{
    // Given multiple mandatory proxies
    using TestNeeds = ProxyNeeds<FakeProxyBase, OtherFakeProxyBase>;
    auto unit = ProxyNeedsFactory<TestNeeds>::Create<TestProxyFindStrategy, TestOtherProxyFindStrategy>();

    // When waiting for them with a timeout (but both are found immediately)
    auto container = unit.WaitForMandatoryProxies({}, std::chrono::milliseconds{100});

    // Then they are found
    EXPECT_EQ(container.Get<FakeProxyBase>()->What(), 42);
}

TEST_F(ProxyNeedsTest, MultipleMandatoryProxiesAreNotFoundDueToExceededWaitTimeout)
{
    // Given multiple mandatory proxies
    using TestNeeds = ProxyNeeds<FakeProxyBase, OtherFakeProxyBase>;
    auto unit = ProxyNeedsFactory<TestNeeds>::Create<TestProxyFindStrategy, TestOtherProxyFindStrategy>();

    gWaitTimeToServiceFoundIFakeProxy = std::chrono::milliseconds{100};
    auto container = unit.WaitForMandatoryProxies({}, std::chrono::milliseconds{50});

    EXPECT_EQ(container.Get<FakeProxyBase>(), nullptr);
}

TEST_F(ProxyNeedsTest, MultipleMandatoryProxiesReduceOveralWaitTimeForNextProxy)
{
    // Given two mandatory proxies
    using TestNeeds = ProxyNeeds<FakeProxyBase, OtherFakeProxyBase>;
    auto unit = ProxyNeedsFactory<TestNeeds>::Create<TestProxyFindStrategy, TestOtherProxyFindStrategy>();

    // Given that we have to wait for FakeProxyBase until forever and for OtherFakeProxyBase only 20 milliseconds
    gWaitTimeToServiceFoundIFakeProxy = std::chrono::milliseconds{1000};
    gWaitTimeToServiceFoundIOtherFakeProxy = std::chrono::milliseconds{20};

    // When waiting for all mandatory proxies only 70 ms
    auto container = unit.WaitForMandatoryProxies({}, std::chrono::milliseconds{70});

    // Then we only find one of the proxies (the one within the timeout)
    EXPECT_EQ(container.Get<FakeProxyBase>(), nullptr);
    EXPECT_NE(container.Get<OtherFakeProxyBase>(), nullptr);
}
#endif

TEST_F(ProxyNeedsTest, CombiningMandatoryAndOptionalProxyInOneNeed)
{
    // Given one mandatory and one optional proxy of the same kind
    using TestNeeds = ProxyNeeds<FakeProxyBase, Optional<FakeProxyBase>>;
    auto unit = ProxyNeedsFactory<TestNeeds>::Create<TestProxyFindStrategy, TestProxyFindStrategy>();

    // When waiting for all mandatory proxies
    auto container = unit.WaitForMandatoryProxies({});

    // Then the mandatory one must have gotten found
    EXPECT_EQ(container.Get<FakeProxyBase>()->What(), 42);

    // And the optional one as well as after some time passed
    EXPECT_TRUE(container.Get<Optional<FakeProxyBase>>().WaitFor({}, std::chrono::seconds{1}).has_value());
}

TEST_F(ProxyNeedsTest, AlternativeMandatoryProxyNeed)
{
    // Given a mandatory alternative proxy (either FakeProxyBase or OtherFakeProxyBase)
    using TestNeeds = ProxyNeeds<Variant<FakeProxyBase, OtherFakeProxyBase>>;
    auto unit = ProxyNeedsFactory<TestNeeds>::Create<TestAlternativeProxyFindStrategy>();

    // When waiting for all mandatory proxies
    auto container = unit.WaitForMandatoryProxies({});

    // Then one of them is found
    EXPECT_TRUE(std::holds_alternative<SingleInstanceHolder<FakeProxyBase>>(
        container.Get<Variant<FakeProxyBase, OtherFakeProxyBase>>()));
}

TEST_F(ProxyNeedsTest, AlternativeOptionalProxyNeed)
{
    // Given an optional alternative proxy (either FakeProxyBase or OtherFakeProxyBase)
    using TestNeeds = ProxyNeeds<Optional<Variant<FakeProxyBase, OtherFakeProxyBase>>>;
    auto unit = ProxyNeedsFactory<TestNeeds>::Create<TestAlternativeProxyFindStrategy>();

    // When waiting for all mandatory proxies
    auto container = unit.WaitForMandatoryProxies({});

    // Then one of them is found
    auto& future = container.Get<Optional<Variant<FakeProxyBase, OtherFakeProxyBase>>>();
    auto variant = future.Get({}).value();
    EXPECT_TRUE(std::holds_alternative<SingleInstanceHolder<FakeProxyBase>>(variant));
}

TEST_F(ProxyNeedsTest, MultipleProxyNeeded)
{
    // Given the need for multiple instances of one proxy
    using TestNeeds = ProxyNeeds<Multiple<FakeProxyBase>>;
    auto unit = ProxyNeedsFactory<TestNeeds>::Create<TestProxyFindStrategyMultiInstances>();
    auto container = unit.WaitForMandatoryProxies({});

    // When requesting the container for the instances
    auto& multi_instance_holder = container.Get<Multiple<FakeProxyBase>>();

    gAsyncFindFutureIFakeProxy.get();  // proxy is now found

    // Then we found multiple proxies
    bool found_proxy_42{false};
    bool found_proxy_44{false};
    multi_instance_holder.Visit([&found_proxy_42, &found_proxy_44](const auto& proxy) {
        if (proxy.What() == 42U)
        {
            found_proxy_42 = true;
        }
        if (proxy.What() == 44U)
        {
            found_proxy_44 = true;
        }
    });
    EXPECT_TRUE(found_proxy_42);
    EXPECT_TRUE(found_proxy_44);
}

TEST_F(ProxyNeedsTest, SingleMandatoryProxyWaitWithoutBlockingWithOnFoundCallback)
{
    // Given that one single mandatory proxy is needed in combination with an on-service-found callback
    using TestNeeds = ProxyNeeds<FakeProxyBase>;
    bool was_callback_executed{false};
    auto on_found_callback = [&was_callback_executed](const FakeProxyBase& proxy) {
        ASSERT_EQ(42U, proxy.What());
        was_callback_executed = true;
    };
    auto unit = ProxyNeedsFactory<TestNeeds>::Create<TestProxyFindStrategy>();

    // When waiting for mandatory proxies without blocking
    auto container = unit.WithOnServiceFound(std::move(on_found_callback)).WaitForMandatoryProxies({});
    auto& proxy = container.Get<FakeProxyBase>();

    // Then the proxy is available and callback was executed
    EXPECT_TRUE(was_callback_executed);
    EXPECT_EQ(proxy->What(), 42);
}

TEST_F(ProxyNeedsTest, SingleMandatoryProxyWaitWithoutBlockingWithNullCallback)
{
    // Given that one single mandatory proxy + null callback
    using TestNeeds = ProxyNeeds<FakeProxyBase>;
    details::ProxySpecTraits<FakeProxyBase>::UserCallback null_callback{};
    auto unit = ProxyNeedsFactory<TestNeeds>::Create<TestProxyFindStrategy>();

    // When waiting for mandatory proxies without blocking
    auto container = unit.WithOnServiceFound(std::move(null_callback)).WaitForMandatoryProxies({});
    auto& proxy = container.Get<FakeProxyBase>();

    // Then the proxy is available and a null callback did not lead to a crash
    EXPECT_EQ(proxy->What(), 42);
}

TEST_F(ProxyNeedsTest, MultipleOptionalProxiesWithOnFoundCallbacks)
{
    // Given that multiple optional proxies are needed and respective on_found callbacks got provided
    using TestNeeds = ProxyNeeds<Optional<FakeProxyBase>, Optional<OtherFakeProxyBase>>;

    bool was_fake_proxy_found_callback_executed{false};
    auto fake_proxy_found_callback = [&was_fake_proxy_found_callback_executed](const FakeProxyBase& proxy) {
        ASSERT_EQ(42U, proxy.What());
        was_fake_proxy_found_callback_executed = true;
    };

    bool was_other_fake_proxy_found_callback_executed{false};
    auto other_fake_proxy_found_callback =
        [&was_other_fake_proxy_found_callback_executed](const OtherFakeProxyBase& other_proxy) {
            ASSERT_EQ(123U, other_proxy.What());
            was_other_fake_proxy_found_callback_executed = true;
        };

    auto unit = ProxyNeedsFactory<TestNeeds>::Create<TestProxyFindStrategy, TestOtherProxyFindStrategy>();

    // When initiating service discovery
    auto container =
        unit.WithOnServiceFound({std::move(fake_proxy_found_callback), std::move(other_fake_proxy_found_callback)})
            .InitiateServiceDiscovery({})
            .GetProxyContainer();

    // Then both proxies must have gotten found and both callbacks must have gotten executed
    EXPECT_TRUE(container.Get<Optional<OtherFakeProxyBase>>().Get({}).has_value());
    EXPECT_TRUE(container.Get<Optional<FakeProxyBase>>().Get({}).has_value());
    EXPECT_TRUE(was_other_fake_proxy_found_callback_executed);
    EXPECT_TRUE(was_fake_proxy_found_callback_executed);

    // We call TearDown() already here to prevent access to our local bool variables that were captured by the callbacks
    TearDown();
}

TEST_F(ProxyNeedsTest, MultipleOptionalProxiesWithOnFoundCallbacksButFirstServiceNotFound)
{
    // Given that multiple optional proxies are needed and respective on_found callbacks got provided
    using TestNeeds = ProxyNeeds<Optional<FakeProxyBase>, Optional<OtherFakeProxyBase>>;

    bool was_fake_proxy_found_callback_executed{false};
    auto fake_proxy_found_callback = [&was_fake_proxy_found_callback_executed](const FakeProxyBase& proxy) {
        ASSERT_EQ(42U, proxy.What());
        was_fake_proxy_found_callback_executed = true;
    };

    bool was_other_fake_proxy_found_callback_executed{false};
    auto other_fake_proxy_found_callback =
        [&was_other_fake_proxy_found_callback_executed](const OtherFakeProxyBase& other_proxy) {
            ASSERT_EQ(123U, other_proxy.What());
            was_other_fake_proxy_found_callback_executed = true;
        };

    auto unit = ProxyNeedsFactory<TestNeeds>::Create<TestProxyFindStrategy, TestOtherProxyFindStrategy>();

    // When initiating service discovery while the first service gets set to be not discoverable
    gWaitTimeToServiceFoundIFakeProxy = std::chrono::hours{10};
    auto container =
        unit.WithOnServiceFound({std::move(fake_proxy_found_callback), std::move(other_fake_proxy_found_callback)})
            .InitiateServiceDiscovery({})
            .GetProxyContainer();

    // Then one proxy was not found, while the other was. One callback was not executed, while the other one was
    EXPECT_TRUE(container.Get<Optional<OtherFakeProxyBase>>().Get({}).has_value());
    EXPECT_TRUE(was_other_fake_proxy_found_callback_executed);
    EXPECT_FALSE(container.Get<Optional<FakeProxyBase>>().Ready());
    EXPECT_FALSE(was_fake_proxy_found_callback_executed);

    // We call TearDown() already here to prevent access to our local bool variables that were captured by the callbacks
    TearDown();
}

TEST_F(ProxyNeedsTest, MandatoryAndOptionalProxiesWithOnFoundCallbacksButFirstServiceNotFound)
{
    // Given that multiple optional proxies are needed and respective on_found callbacks got provided
    using TestNeeds = ProxyNeeds<Optional<FakeProxyBase>, OtherFakeProxyBase>;

    bool was_fake_proxy_found_callback_executed{false};
    auto fake_proxy_found_callback = [&was_fake_proxy_found_callback_executed](const FakeProxyBase& proxy) {
        ASSERT_EQ(42U, proxy.What());
        was_fake_proxy_found_callback_executed = true;
    };

    bool was_other_fake_proxy_found_callback_executed{false};
    auto other_fake_proxy_found_callback =
        [&was_other_fake_proxy_found_callback_executed](const OtherFakeProxyBase& other_proxy) {
            ASSERT_EQ(123U, other_proxy.What());
            was_other_fake_proxy_found_callback_executed = true;
        };

    auto unit = ProxyNeedsFactory<TestNeeds>::Create<TestProxyFindStrategy, TestOtherProxyFindStrategy>();

    // When initiating service discovery while the first service gets set to be not discoverable
    gWaitTimeToServiceFoundIFakeProxy = std::chrono::hours{10};
    auto container =
        unit.WithOnServiceFound({std::move(fake_proxy_found_callback), std::move(other_fake_proxy_found_callback)})
            .WaitForMandatoryProxies({});

    // Then one proxy was not found, while the other was. One callback was not executed, while the other one was
    EXPECT_FALSE(container.Get<Optional<FakeProxyBase>>().Ready());
    EXPECT_NE(container.Get<OtherFakeProxyBase>(), nullptr);
    EXPECT_TRUE(was_other_fake_proxy_found_callback_executed);
    EXPECT_FALSE(was_fake_proxy_found_callback_executed);

    // We call TearDown() already here to prevent access to our local bool variables that were captured by the callbacks
    TearDown();
}

TEST_F(ProxyNeedsTest, MultipleAndOptionalProxiesWithOnFoundCallbacks)
{
    // Given that multiple as well as optional proxies are needed and respective on_found callbacks got provided
    using TestNeeds = ProxyNeeds<Multiple<FakeProxyBase>, Optional<OtherFakeProxyBase>>;

    bool found_proxy_42{false};
    bool found_proxy_44{false};
    auto fake_proxy_found_callback = [&found_proxy_42, &found_proxy_44](const FakeProxyBase& proxy) {
        if (proxy.What() == 42U)
        {
            found_proxy_42 = true;
        }
        if (proxy.What() == 44U)
        {
            found_proxy_44 = true;
        }
    };

    bool was_other_fake_proxy_found_callback_executed{false};
    auto other_fake_proxy_found_callback =
        [&was_other_fake_proxy_found_callback_executed](const OtherFakeProxyBase& other_proxy) {
            ASSERT_EQ(123U, other_proxy.What());
            was_other_fake_proxy_found_callback_executed = true;
        };

    auto unit = ProxyNeedsFactory<TestNeeds>::Create<TestProxyFindStrategyMultiInstances, TestOtherProxyFindStrategy>();

    // When initiating service discovery
    auto container =
        unit.WithOnServiceFound({std::move(fake_proxy_found_callback), std::move(other_fake_proxy_found_callback)})
            .InitiateServiceDiscovery({})
            .GetProxyContainer();

    // And also waiting for optional proxies
    gAsyncFindFutureIOtherFakeProxy.wait();
    gAsyncFindFutureIFakeProxy.wait();

    // Then all proxies must have been found as well as as all callbacks must have gotten executed
    EXPECT_NE(container.Get<Optional<OtherFakeProxyBase>>().Get({}).value(), nullptr);
    EXPECT_EQ(container.Get<Multiple<FakeProxyBase>>().NumInstances(), 2U);
    EXPECT_TRUE(was_other_fake_proxy_found_callback_executed);
    EXPECT_TRUE(found_proxy_44);
    EXPECT_TRUE(found_proxy_42);

    // We call TearDown() already here to prevent access to our local bool variables that were captured by the callbacks
    TearDown();
}

TEST_F(ProxyNeedsTest, AlternativeAndOptionalProxiesWithOnFoundCallbacks)
{
    // Given that alternative as well as optional proxies are needed and respective on_found callbacks got provided
    using AlternativeProxies = Variant<FakeProxyBase, OtherFakeProxyBase>;
    using TestNeeds = ProxyNeeds<AlternativeProxies, Optional<AlternativeProxies>>;
    using OnFoundCallbackParameterType = std::variant<FakeProxyBase*, OtherFakeProxyBase*>;

    bool was_fake_proxy_found_callback_executed{false};
    auto fake_proxy_found_callback = [&was_fake_proxy_found_callback_executed](OnFoundCallbackParameterType proxy) {
        if (std::holds_alternative<FakeProxyBase*>(proxy))
        {
            EXPECT_EQ(44U, std::get<FakeProxyBase*>(proxy)->What());
            was_fake_proxy_found_callback_executed = true;
        }
        else
        {
            FAIL() << "received proxy is not of type `FakeProxyBase`";
        }
    };

    bool was_other_fake_proxy_found_callback_executed{false};
    auto other_fake_proxy_found_callback =
        [&was_other_fake_proxy_found_callback_executed](OnFoundCallbackParameterType other_proxy) {
            if (std::holds_alternative<OtherFakeProxyBase*>(other_proxy))
            {
                EXPECT_EQ(123U, std::get<OtherFakeProxyBase*>(other_proxy)->What());
                was_other_fake_proxy_found_callback_executed = true;
            }
            else
            {
                FAIL() << "received proxy is not of type `OtherFakeProxyBase`";
            }
        };

    auto unit =
        ProxyNeedsFactory<TestNeeds>::Create<TestAlternativeProxyFindStrategy, TestAlternativeProxyFindOtherStrategy>();

    // When initiating service discovery and waiting for the mandatory proxy
    auto container =
        unit.WithOnServiceFound({std::move(fake_proxy_found_callback), std::move(other_fake_proxy_found_callback)})
            .WaitForMandatoryProxies({});

    // And also waiting for optional proxies
    gAsyncFindFutureIOtherFakeProxy.wait();

    // Then all proxies must have been found as well as as all callbacks must have gotten executed
    EXPECT_TRUE(std::holds_alternative<SingleInstanceHolder<FakeProxyBase>>(container.Get<AlternativeProxies>()));
    EXPECT_TRUE(std::holds_alternative<SingleInstanceHolder<OtherFakeProxyBase>>(
        container.Get<Optional<AlternativeProxies>>().Get({}).value()));
    EXPECT_TRUE(was_other_fake_proxy_found_callback_executed);
    EXPECT_TRUE(was_fake_proxy_found_callback_executed);

    // We call TearDown() already here to prevent access to our local bool variables that were captured by the callbacks
    TearDown();
}

}  // namespace
}  // namespace score::mw::service::test
