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

#include "score/mw/service/details/proxy_builder.h"

#include "score/mw/service/details/proxy_spec_traits.h"
#include "score/mw/service/find_service_strategy.h"
#include "score/mw/service/proxy_builder_base.h"
#include "score/mw/service/test_doubles/test_doubles.h"

#include "score/concurrency/notification.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <score/utility.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <future>
#include <optional>
#include <string>
#include <utility>

namespace score::mw::service::details
{
namespace
{

using namespace std::chrono_literals;
using test::FakeProxy;
using test::FakeProxyBase;
using test::OtherFakeProxyBase;

constexpr auto kAsyncFindFutureTimeout{100ms};

score::concurrency::Notification gFindServiceIsRunningNotification{};
score::concurrency::Notification gServiceAvailableNotification{};
std::atomic<bool> gIsFindStopped{false};
std::future<void> gAsyncFindFuture{};

const std::string kDummyPortIdentifier{"Port/Identifier"};

class TestProxyFindStrategy : public FindServiceStrategy
{
  public:
    using BaseProxy = FakeProxyBase;

    void Find(std::unique_ptr<typename ProxyBuilderBase<FakeProxyBase>::BuilderCallback> on_found)
    {
        gAsyncFindFuture = std::async(std::launch::async, [on_found = std::move(on_found)]() {
            gFindServiceIsRunningNotification.notify();
            gServiceAvailableNotification.waitWithAbort({});
            if (gIsFindStopped)
            {
                return;
            }

            auto found_service = std::make_unique<FakeProxy>(42);
            (*on_found)(std::move(found_service));
            (*on_found)(nullptr);  // on_found callback gets invoked multiple times to simulate that multiple
                                   // "StartFindService" calls have been launched and multiple find a service
        });
        gFindServiceIsRunningNotification.waitWithAbort({});
    }

    void StopFind() noexcept override
    {
        gIsFindStopped = true;
        gServiceAvailableNotification.notify();
        ASSERT_EQ(std::future_status::ready, gAsyncFindFuture.wait_for(kAsyncFindFutureTimeout));
    }
};

class TestMandatoryImmediateProxyFindStrategy : public FindServiceStrategy
{
  public:
    using BaseProxy = FakeProxyBase;

    void Find(std::unique_ptr<typename ProxyBuilderBase<BaseProxy>::BuilderCallback> on_found)
    {
        auto proxy = std::make_unique<FakeProxy>(42);
        (*on_found)(std::move(proxy));
    }

    void StopFind() noexcept override {}
};

class TestOptionalImmediateProxyFindStrategy : public FindServiceStrategy
{
  public:
    using BaseProxy = Optional<FakeProxyBase>;

    void Find(std::unique_ptr<typename ProxyBuilderBase<BaseProxy>::BuilderCallback> on_found)
    {
        auto proxy = std::make_unique<FakeProxy>(42);
        (*on_found)(std::move(proxy));
    }

    void StopFind() noexcept override {}
};

class TestMultipleImmediateProxyFindStrategy : public FindServiceStrategy
{
  public:
    using BaseProxy = Multiple<FakeProxyBase>;

    void Find(std::unique_ptr<typename ProxyBuilderBase<BaseProxy>::BuilderCallback> on_found)
    {
        auto proxy = std::make_unique<FakeProxy>(42);
        auto proxy2 = std::make_unique<FakeProxy>(43);

        (*on_found)(std::move(proxy));
        (*on_found)(std::move(proxy2));
    }

    void StopFind() noexcept override {}
};

class TestVariantProxyImmediateFindStrategy : public FindServiceStrategy
{
  public:
    using BaseProxy = Variant<FakeProxyBase, OtherFakeProxyBase>;

    TestVariantProxyImmediateFindStrategy(const std::string& port_identifier)
    {
        EXPECT_FALSE(port_identifier.empty());
    }

    void Find(std::unique_ptr<typename ProxyBuilderBase<BaseProxy>::BuilderCallback> on_found)
    {
        auto proxy = std::make_unique<FakeProxy>(42);
        std::variant<SingleInstanceHolder<FakeProxyBase>, SingleInstanceHolder<OtherFakeProxyBase>> found{
            std::move(proxy)};
        (*on_found)(std::move(found));
    }

    void StopFind() noexcept override {}
};

class ProxyBuilderFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        gFindServiceIsRunningNotification.reset();
        gServiceAvailableNotification.reset();
        gIsFindStopped = false;
        gAsyncFindFuture = std::future<void>{};
    }

    void TearDown() override
    {
        if (gAsyncFindFuture.valid())
        {
            gAsyncFindFuture.wait();
        }
        gAsyncFindFuture = std::future<void>{};  // to shutdown the thread spawned by std::async
    }

    void ServiceIsNowAvailable()
    {
        gServiceAvailableNotification.notify();
    }

    void ExpectServiceFindIsRunning()
    {
        EXPECT_FALSE(gIsFindStopped);
    }

    void ExpectServiceFindIsStopped()
    {
        EXPECT_TRUE(gIsFindStopped);
    }

    ProxyBuilder<TestProxyFindStrategy> unit{std::make_unique<TestProxyFindStrategy>()};
};

class MultipleProxyBuilderFixture : public ::testing::Test
{
  public:
    using InitCallback = ProxyBuilderBase<Multiple<FakeProxyBase>>::InitCallback;

    ProxyBuilder<TestMultipleImmediateProxyFindStrategy> unit{
        std::make_unique<TestMultipleImmediateProxyFindStrategy>()};
};

template <typename FindStrategy>
class SingleProxyBuilderFixture : public ::testing::Test
{
  public:
    using InitCallback = typename ProxyBuilderBase<typename FindStrategy::BaseProxy>::InitCallback;

    InitCallback CreateInitCallbackThatRecordsInvocation()
    {
        return [this](auto&) noexcept -> InitCallbackResult {
            init_callback_invoked = true;
            return InitCallbackResult::kSuccess;
        };
    }

    InitCallback CreateInitCallbackThatReturnsError()
    {
        return [](auto&) noexcept -> InitCallbackResult {
            return InitCallbackResult::kError;
        };
    }

    static InitCallback CreateEmptyInitCallback()
    {
        return InitCallback{};
    }

    static FakeProxyBase::WhatType GetProxyValue(
        const typename ProxySpecTraits<typename FindStrategy::BaseProxy>::ContainerType& proxy_container)
    {
        return proxy_container->What();
    }

    ProxyBuilder<FindStrategy> unit{std::make_unique<FindStrategy>()};
    bool init_callback_invoked{false};
};

/// We need an explicit template specialisation for TestVariantProxyImmediateFindStrategy since the callback type is
/// different (its InitCallback takes a parameter by value instead of reference) and also the value must be extracted
/// differently to a Mandatory / Optional Proxy.
template <>
class SingleProxyBuilderFixture<TestVariantProxyImmediateFindStrategy> : public ::testing::Test
{
  public:
    using InitCallback =
        typename ProxyBuilderBase<typename TestVariantProxyImmediateFindStrategy::BaseProxy>::InitCallback;

    InitCallback CreateInitCallbackThatRecordsInvocation()
    {
        return [this](auto) noexcept -> InitCallbackResult {
            init_callback_invoked = true;
            return InitCallbackResult::kSuccess;
        };
    }

    static InitCallback CreateInitCallbackThatReturnsError()
    {
        return [](auto) noexcept -> InitCallbackResult {
            return InitCallbackResult::kError;
        };
    }

    static InitCallback CreateEmptyInitCallback()
    {
        return InitCallback{};
    }

    static FakeProxyBase::WhatType GetProxyValue(
        const typename ProxySpecTraits<typename TestVariantProxyImmediateFindStrategy::BaseProxy>::ContainerType&
            proxy_container)
    {
        return std::get<SingleInstanceHolder<FakeProxyBase>>(proxy_container)->What();
    }

    ProxyBuilder<TestVariantProxyImmediateFindStrategy> unit{
        std::make_unique<TestVariantProxyImmediateFindStrategy>(kDummyPortIdentifier)};
    bool init_callback_invoked{false};
};
using SingleFindStrategies = ::testing::Types<TestMandatoryImmediateProxyFindStrategy,
                                              TestOptionalImmediateProxyFindStrategy,
                                              TestVariantProxyImmediateFindStrategy>;
TYPED_TEST_SUITE(SingleProxyBuilderFixture, SingleFindStrategies, /* unused */);

TEST_F(ProxyBuilderFixture, FindsAndConstructsServiceImmediate)
{
    // Given a proxy builder and that a service is available
    ServiceIsNowAvailable();

    // When Building a proxy
    auto [future, _] = unit.Build({});
    ASSERT_TRUE(future.Valid());

    // The proxy is constructed
    auto found_service = future.Get({});
    EXPECT_EQ(found_service.value()->What(), 42);
}

TEST_F(ProxyBuilderFixture, FindsAndConstructsService)
{
    // Given we build a proxy and are still searching for it
    auto [future, _] = unit.Build({});
    ASSERT_TRUE(future.Valid());
    ASSERT_EQ(future.WaitFor({}, 1ms).error(), score::concurrency::Error::kTimeout);

    // When the service comes available
    ServiceIsNowAvailable();

    // The proxy is constructed
    auto found_service = future.Get({});
    EXPECT_EQ(found_service.value()->What(), 42);
}

TEST_F(ProxyBuilderFixture, StopFindWhenFutureNotInterested)
{
    // Given we build a proxy, which we do not find and are not interested into the result
    score::cpp::ignore = unit.Build({});

    // When the service comes available
    ServiceIsNowAvailable();

    // Then service discovery must have gotten stopped
    ExpectServiceFindIsStopped();
}

TEST_F(ProxyBuilderFixture, StopFindWhenFutureGetsDestroyed)
{
    // Given we build a proxy and keep the returned future so that service discovery is still running
    {
        auto result = unit.Build({});
        ExpectServiceFindIsRunning();

        // When the future is now destroyed
    }

    // Then service discovery must have gotten stopped
    ExpectServiceFindIsStopped();
}

TEST_F(ProxyBuilderFixture, UserCallbackInvokedOnServiceFound)
{
    // Given a proxy builder and that a service is available
    ServiceIsNowAvailable();

    // When Building a proxy together with an on_service_found user callback
    bool user_callback_invoked{false};
    unit.WithOnServiceFound([&user_callback_invoked](FakeProxyBase&) noexcept {
        user_callback_invoked = true;
    });

    auto [future, _] = this->unit.Build({});
    ASSERT_TRUE(future.Valid());

    // Then the user callback is invoked
    future.Wait({});
    EXPECT_TRUE(user_callback_invoked);
}

TEST_F(ProxyBuilderFixture, SecondWithOnServiceFoundUserCallbackIsIgnored)
{
    // Given a proxy builder with an already registered user callback
    ServiceIsNowAvailable();
    bool first_callback_invoked{false};
    bool second_callback_invoked{false};
    unit.WithOnServiceFound([&first_callback_invoked](FakeProxyBase&) noexcept {
        first_callback_invoked = true;
    });

    // When registering a second user callback (false decision: callback_ no longer holds NoUserCallback)
    unit.WithOnServiceFound([&second_callback_invoked](FakeProxyBase&) noexcept {
        second_callback_invoked = true;
    });

    auto [future, _] = unit.Build({});
    ASSERT_TRUE(future.Valid());
    future.Wait({});

    // Then only the first callback is invoked; the second registration is silently ignored
    EXPECT_TRUE(first_callback_invoked);
    EXPECT_FALSE(second_callback_invoked);
}

TYPED_TEST(SingleProxyBuilderFixture, InitCallbackInvokedOnServiceFound)
{
    // Given a proxy builder and that a service is available

    // and given a registered InitCallback which records when it's invoked
    auto init_callback = this->CreateInitCallbackThatRecordsInvocation();
    this->unit.WithOnServiceFound(std::move(init_callback));

    // When building the proxy
    auto [future, _] = this->unit.Build({});
    ASSERT_TRUE(future.Valid());

    // Then the init callback is invoked
    future.Wait({});
    EXPECT_TRUE(this->init_callback_invoked);
}

TEST_F(MultipleProxyBuilderFixture, InitCallbackInvokedOnEveryServiceFound)
{
    // Given a proxy builder for multiple proxies and that 2 service instances are immediately available

    // and given a registered InitCallback which records how many times it's invoked
    std::size_t init_callback_invocation_count{0U};
    InitCallback init_callback = [&init_callback_invocation_count](FakeProxyBase&) noexcept -> InitCallbackResult {
        init_callback_invocation_count++;
        return InitCallbackResult::kSuccess;
    };
    unit.WithOnServiceFound(std::move(init_callback));

    // When building the proxies
    auto [future, _] = this->unit.Build({});
    ASSERT_TRUE(future.Valid());

    // Then the init callback is invoked twice
    future.Wait({});
    EXPECT_EQ(init_callback_invocation_count, 2U);
}

TYPED_TEST(SingleProxyBuilderFixture, ConstructsServiceWhenInitCallbackReturnsValidResult)
{
    // Given a proxy builder and that a service is available

    // and given that we have registered an on_service_found init callback which returns a valid result
    auto init_callback = this->CreateInitCallbackThatRecordsInvocation();
    this->unit.WithOnServiceFound(std::move(init_callback));

    // When building the Proxy
    auto [future, _] = this->unit.Build({});
    ASSERT_TRUE(future.Valid());

    // Then the proxy is constructed
    auto found_service = future.Get({});
    ASSERT_TRUE(found_service.has_value());
    EXPECT_EQ(this->GetProxyValue(found_service.value()), 42);
}

TYPED_TEST(SingleProxyBuilderFixture, ConstructsServiceWhenInitCallbackIsEmpty)
{
    // Given a proxy builder and that a service is available

    // and given that we have registered an on_service_found init callback which is empty
    auto init_callback = this->CreateEmptyInitCallback();
    this->unit.WithOnServiceFound(std::move(init_callback));

    // When building the Proxy
    auto [future, _] = this->unit.Build({});
    ASSERT_TRUE(future.Valid());

    // Then the proxy is constructed
    auto found_service = future.Get({});
    ASSERT_TRUE(found_service.has_value());
    EXPECT_EQ(this->GetProxyValue(found_service.value()), 42);
}

TEST_F(MultipleProxyBuilderFixture, ConstructsServiceWhenInitCallbackReturnsValidResult)
{
    // Given a proxy builder for multiple proxies and that 2 service instances are immediately available

    // and given that we have registered an on_service_found init callback which always returns a valid result
    ProxyBuilderBase<FakeProxyBase>::InitCallback init_callback = [](FakeProxyBase&) noexcept -> InitCallbackResult {
        return InitCallbackResult::kSuccess;
    };
    unit.WithOnServiceFound(std::move(init_callback));

    // When building the proxies
    auto [future, _] = this->unit.Build({});
    ASSERT_TRUE(future.Valid());

    // Then 2 proxies are constructed
    auto found_service = future.Get({});
    ASSERT_TRUE(found_service.has_value());
    EXPECT_EQ(found_service.value().NumInstances(), 2U);
}

TEST_F(MultipleProxyBuilderFixture, OnlyConstructsServiceWhenInitCallbackReturnsValidResult)
{
    // Given a proxy builder for multiple proxies and that 2 service instances are immediately available

    // and given that we have registered an on_service_found init callback which returns a valid result the first time
    // and an error the second time
    std::size_t call_counter{0U};
    ProxyBuilderBase<FakeProxyBase>::InitCallback init_callback =
        [&call_counter](FakeProxyBase&) noexcept -> InitCallbackResult {
        ++call_counter;
        if (call_counter == 1)
        {
            return InitCallbackResult::kSuccess;
        }
        else
        {
            return InitCallbackResult::kError;
        }
    };
    unit.WithOnServiceFound(std::move(init_callback));

    // When building the proxies
    auto [future, _] = this->unit.Build({});
    ASSERT_TRUE(future.Valid());

    // Then 2 proxies are constructed
    auto found_service = future.Get({});
    ASSERT_TRUE(found_service.has_value());
    EXPECT_EQ(found_service.value().NumInstances(), 1U);
}

TEST_F(MultipleProxyBuilderFixture, ConstructsServiceWhenInitCallbackIsEmpty)
{
    // Given a proxy builder for multiple proxies and that 2 service instances are immediately available

    // and given that we have registered an on_service_found init callback which is empty
    ProxyBuilderBase<FakeProxyBase>::InitCallback init_callback{};
    unit.WithOnServiceFound(std::move(init_callback));

    // When building the proxies
    auto [future, _] = this->unit.Build({});
    ASSERT_TRUE(future.Valid());

    // Then 2 proxies are constructed
    auto found_service = future.Get({});
    ASSERT_TRUE(found_service.has_value());
    EXPECT_EQ(found_service.value().NumInstances(), 2U);
}

TYPED_TEST(SingleProxyBuilderFixture, DoesNotConstructServiceWhenInitCallbackReturnsError)
{
    // Given a proxy builder and that a service is available

    // and given that we have registered an on_service_found init callback which returns an error
    auto init_callback = this->CreateInitCallbackThatReturnsError();
    this->unit.WithOnServiceFound(std::move(init_callback));

    // When building the Proxy
    auto [future, _] = this->unit.Build({});
    ASSERT_TRUE(future.Valid());

    // Then the proxy was not constructed
    auto found_service = future.Get({});
    EXPECT_FALSE(found_service.has_value());
}

TEST_F(MultipleProxyBuilderFixture, DoesNotConstructServiceWhenInitCallbackReturnsOnlyError)
{
    // Given a proxy builder for multiple proxes and that 2 service instances are immediately available

    // and given that we have registered an on_service_found init callback which always returns an error
    ProxyBuilderBase<FakeProxyBase>::InitCallback init_callback = [](FakeProxyBase&) noexcept -> InitCallbackResult {
        return InitCallbackResult::kError;
    };
    unit.WithOnServiceFound(std::move(init_callback));

    // When building the Proxies
    auto [future, _] = this->unit.Build({});
    ASSERT_TRUE(future.Valid());

    // Then no proxies were constructed
    auto found_service = future.Get({});
    ASSERT_TRUE(found_service.has_value());
    EXPECT_EQ(found_service.value().NumInstances(), 0U);
}

TEST(ProxyBuilderDeathTest, BuildCalledTwice)
{
    // Given a proxy builder that builds alternative proxies
    ProxyBuilder unit{std::make_unique<TestVariantProxyImmediateFindStrategy>(kDummyPortIdentifier)};

    // When building a proxy
    // Then it has to succeed
    auto [future, stop_action] = unit.Build({});
    ASSERT_TRUE(future.Ready());

    // When attempting to build the proxy once more
    // Then immediate termination is expected due to the strategy instance being no longer valid
    ASSERT_EXIT(score::cpp::ignore = unit.Build({}), ::testing::KilledBySignal{SIGABRT}, "");
}

TEST(ProxyBuilderTest, AlternativeProxyBuild)
{
    // Given a proxy builder that builds alternative proxies
    ProxyBuilder unit{std::make_unique<TestVariantProxyImmediateFindStrategy>(kDummyPortIdentifier)};

    // When building a proxy
    auto [future, _] = unit.Build({});

    // Then the expected variant is created
    std::variant<SingleInstanceHolder<FakeProxyBase>, SingleInstanceHolder<OtherFakeProxyBase>> variant =
        std::move(future.Get({})).value();
    EXPECT_EQ(std::get<SingleInstanceHolder<FakeProxyBase>>(variant)->What(), 42);
}

TEST_F(ProxyBuilderFixture, FindServiceGetsStoppedUponRequestStop)
{
    // Given we build a proxy and are still searching for it
    score::cpp::stop_source stop_source{};
    ASSERT_FALSE(stop_source.stop_requested());
    auto [future, _] = this->unit.Build(stop_source.get_token());
    ASSERT_EQ(future.WaitFor({}, 1ms).error(), score::concurrency::Error::kTimeout);

    // When requesting stop at the stop_source
    ASSERT_TRUE(stop_source.request_stop());

    // Then finding the service must have gotten stopped
    ExpectServiceFindIsStopped();

    // And no proxy instance must have gotten found
    EXPECT_EQ(future.WaitFor({}, 1ms).error(), score::concurrency::Error::kTimeout);
}

TEST_F(ProxyBuilderFixture, FindServiceGetsStoppedDueToStopAlreadyRequested)
{
    // Given we build a proxy together with a stop_source where stop got already requested
    score::cpp::stop_source stop_source{};
    ASSERT_TRUE(stop_source.request_stop());
    auto [future, _] = this->unit.Build(stop_source.get_token());

    // Then no proxy instance must have gotten found
    EXPECT_EQ(future.WaitFor({}, 1ms).error(), score::concurrency::Error::kTimeout);

    // And finding the service must have gotten stopped
    ExpectServiceFindIsStopped();
}

using ProxyBuilderFixtureDeathTest = ProxyBuilderFixture;
TEST_F(ProxyBuilderFixtureDeathTest, MandatoryBuildTerminatesDueToUnexpectedErrors)
{
    // Given a FakeBuilder yielding a future whose promise got already destroyed
    class FakeBuilder : public ProxyBuilderBase<FakeProxyBase>
    {
      public:
        using BuilderReturn = BuilderReturnType<SingleInstanceHolder<FakeProxyBase>>;

        FakeBuilder(const score::concurrency::Error error) : error_{error} {}

        BuilderReturn Build(std::optional<score::cpp::stop_token>) override
        {
            score::concurrency::InterruptiblePromise<SingleInstanceHolder<FakeProxyBase>> promise{};
            promise.SetError(error_);
            return BuilderReturn{promise.GetInterruptibleFuture().value(),
                                 std::unique_ptr<StopServiceDiscoveryAction>{}};
        }

      private:
        const score::concurrency::Error error_{};
    };

    // When using it wrapped by `MandatoryBuild` in conj. with a stop_token at which stop did not get requested
    score::cpp::stop_source stop_source{};
    MandatoryBuild<FakeProxyBase> build_unit{};
    ASSERT_FALSE(stop_source.stop_requested());

    // Then immediate termination is expected during `MandatoryBuild`'s method Get() upon unexpected conc. errors
    const auto perform_test_for = [&](const score::concurrency::Error error) {
        FakeBuilder builder{error};
        auto result = build_unit.Build(builder, stop_source.get_token());
        ASSERT_EXIT(score::cpp::ignore = build_unit.Get(std::move(result), {}), ::testing::KilledBySignal{SIGABRT}, "");
    };
    perform_test_for(score::concurrency::Error::kUnknown);
    perform_test_for(score::concurrency::Error::kPromiseBroken);
    perform_test_for(score::concurrency::Error::kFutureAlreadyRetrieved);
    perform_test_for(score::concurrency::Error::kPromiseAlreadySatisfied);
    perform_test_for(score::concurrency::Error::kNoState);
    perform_test_for(score::concurrency::Error::kTimeout);
}

TEST_F(ProxyBuilderFixture, MandatoryBuildYieldsEmptyResultUponRequestStop)
{
    // Given a FakeBuilder yielding a (valid) future which will never receive a value
    class FakeBuilder : public ProxyBuilderBase<FakeProxyBase>
    {
      public:
        using BuilderReturn = BuilderReturnType<SingleInstanceHolder<FakeProxyBase>>;

        BuilderReturn Build(std::optional<score::cpp::stop_token>) override
        {
            return BuilderReturn{promise_.GetInterruptibleFuture().value(),
                                 std::unique_ptr<StopServiceDiscoveryAction>{}};
        }

      private:
        score::concurrency::InterruptiblePromise<SingleInstanceHolder<FakeProxyBase>> promise_{};
    };

    // When invoking it via `MandatoryBuild` in conj. with a stop_token at which stop did NOT YET get requested
    FakeBuilder builder{};
    score::cpp::stop_source stop_source{};
    MandatoryBuild<FakeProxyBase> build_unit{};
    ASSERT_FALSE(stop_source.stop_requested());
    auto future = std::async(std::launch::async, [&] {
        auto result = build_unit.Build(builder, stop_source.get_token());
        return build_unit.Get(std::move(result), stop_source.get_token());
    });

    // Then the future must be waiting for `MandatoryBuild`'s result to be retrieved
    ASSERT_EQ(std::future_status::timeout, future.wait_for(kAsyncFindFutureTimeout));

    // When requesting stop at the stop_source now
    ASSERT_TRUE(stop_source.request_stop());

    // Then the `MandatoryBuild` must have finished yielding an empty result
    ASSERT_EQ(std::future_status::ready, future.wait_for(kAsyncFindFutureTimeout));
    EXPECT_EQ(nullptr, future.get());
}

TEST_F(ProxyBuilderFixture, OptionalBuildYieldsEmptyResultUponRequestStop)
{
    // Given a FakeBuilder yielding a (valid) future which will never receive a value
    class FakeBuilder : public ProxyBuilderBase<FakeProxyBase>
    {
      public:
        using BuilderReturn = BuilderReturnType<SingleInstanceHolder<FakeProxyBase>>;

        BuilderReturn Build(std::optional<score::cpp::stop_token>) override
        {
            return BuilderReturn{promise_.GetInterruptibleFuture().value(),
                                 std::unique_ptr<StopServiceDiscoveryAction>{}};
        }

      private:
        score::concurrency::InterruptiblePromise<SingleInstanceHolder<FakeProxyBase>> promise_{};
    };

    // When invoking it via `OptionalBuild` in conj. with a stop_token at which stop did NOT YET get requested
    FakeBuilder builder{};
    score::cpp::stop_source stop_source{};
    OptionalBuild<FakeProxyBase> build_unit{};
    ASSERT_FALSE(stop_source.stop_requested());
    auto future = std::async(std::launch::async, [&] {
        auto builder_result = build_unit.Build(builder, stop_source.get_token());
        auto result = build_unit.Get(std::move(builder_result), stop_source.get_token());
        return result.GetProxyFuture().Wait(stop_source.get_token());
    });

    // Then the future must be waiting for `OptionalBuild`'s result to be retrieved
    ASSERT_EQ(std::future_status::timeout, future.wait_for(kAsyncFindFutureTimeout));

    // When requesting stop at the stop_source now
    ASSERT_TRUE(stop_source.request_stop());

    // Then the `OptionalBuild` must have finished yielding no result
    ASSERT_EQ(std::future_status::ready, future.wait_for(kAsyncFindFutureTimeout));
    EXPECT_EQ(score::concurrency::Error::kStopRequested, future.get().error());
}

TEST_F(ProxyBuilderFixture, OptionalBuildYieldsEmptyResultDueToStopAlreadyRequested)
{
    // Given a FakeBuilder yielding a future in invalid state in case stop got already requested at the stop_token
    class FakeBuilder : public ProxyBuilderBase<FakeProxyBase>
    {
      public:
        using BuilderReturn = BuilderReturnType<SingleInstanceHolder<FakeProxyBase>>;

        BuilderReturn Build(std::optional<score::cpp::stop_token> stop_token) override
        {
            EXPECT_TRUE(stop_token->stop_requested());
            return {};
        }
    };

    // When invoking it via `OptionalBuild` in conj. with a stop_token at which stop got already requested
    FakeBuilder builder{};
    score::cpp::stop_source stop_source{};
    OptionalBuild<FakeProxyBase> build_unit{};
    ASSERT_TRUE(stop_source.request_stop());
    auto result = build_unit.Get(build_unit.Build(builder, stop_source.get_token()));
    // Then the `OptionalBuild` must have finished immediately yielding no result
    EXPECT_EQ(score::concurrency::Error::kNoState,
              result.GetProxyFuture().WaitFor({}, kAsyncFindFutureTimeout).error());
}

TEST_F(ProxyBuilderFixture, MultiInstanceBuildYieldsEmptyResultUponRequestStop)
{
    // Given a FakeBuilder yielding a (valid) future which will never receive a value
    class FakeBuilder : public ProxyBuilderBase<Multiple<FakeProxyBase>>
    {
      public:
        using BuilderReturn = BuilderReturnType<MultiInstanceHolder<FakeProxyBase>>;

        BuilderReturn Build(std::optional<score::cpp::stop_token>) override
        {
            return BuilderReturn{promise_.GetInterruptibleFuture().value(),
                                 std::unique_ptr<StopServiceDiscoveryAction>{}};
        }

      private:
        score::concurrency::InterruptiblePromise<MultiInstanceHolder<FakeProxyBase>> promise_{};
    };

    // When invoking it via `MultiInstanceBuild` in conj. with a stop_token at which stop did NOT YET get requested
    FakeBuilder builder{};
    score::cpp::stop_source stop_source{};
    MultiInstanceBuild<FakeProxyBase> build_unit{};
    ASSERT_FALSE(stop_source.stop_requested());
    auto future = std::async(std::launch::async, [&]() noexcept {
        auto result = build_unit.Build(builder, stop_source.get_token());
        return build_unit.Get(std::move(result), stop_source.get_token());
    });

    // Then the future must be waiting for `MultiInstanceBuild`'s result to be retrieved
    ASSERT_EQ(std::future_status::timeout, future.wait_for(kAsyncFindFutureTimeout));

    // When requesting stop at the stop_source now
    ASSERT_TRUE(stop_source.request_stop());

    // Then the `MultiInstanceBuild` must have finished yielding an empty container
    ASSERT_EQ(std::future_status::ready, future.wait_for(kAsyncFindFutureTimeout));
    auto proxy_data = future.get();
    EXPECT_EQ(0U, proxy_data.GetProxyInstances().NumInstances());
}

TEST_F(ProxyBuilderFixture, MultiInstanceBuildYieldsEmptyResultDueToStopAlreadyRequested)
{
    // Given a FakeBuilder yielding a future in invalid state in case stop got already requested at the stop_token
    class FakeBuilder : public ProxyBuilderBase<Multiple<FakeProxyBase>>
    {
      public:
        using BuilderReturn = BuilderReturnType<MultiInstanceHolder<FakeProxyBase>>;

        BuilderReturn Build(std::optional<score::cpp::stop_token> stop_token) override
        {
            EXPECT_TRUE(stop_token->stop_requested());
            return {};
        }
    };

    // When invoking it via `MultiInstanceBuild` in conj. with a stop_token at which stop got already requested
    FakeBuilder builder{};
    score::cpp::stop_source stop_source{};
    MultiInstanceBuild<FakeProxyBase> build_unit{};
    ASSERT_TRUE(stop_source.request_stop());
    auto build_result = build_unit.Build(builder, stop_source.get_token());
    auto result = build_unit.Get(std::move(build_result));

    // Then the `MultiInstanceBuild` must have finished immediately yielding no result
    EXPECT_EQ(0, result.GetProxyInstances().NumInstances());
}

TEST_F(ProxyBuilderFixture, CallOnceProxyBuilderCallbackReleasesItsPromiseUponDestruction)
{
    using InterruptiblePromise = score::concurrency::InterruptiblePromise<SingleInstanceHolder<FakeProxyBase>>;
    using ProxyBuilderCallback = std::unique_ptr<typename ProxyBuilderBase<FakeProxyBase>::BuilderCallback>;

    // Given a dummy strategy for finding a proxy which does nothing but storing the on_found callback
    class DummyFindProxyStrategy
    {
      public:
        void Find(ProxyBuilderCallback on_found) noexcept
        {
            on_found_ = std::move(on_found);
        }

        void StopFind() noexcept
        {
            on_found_.reset();
        }

      private:
        ProxyBuilderCallback on_found_;
    };

    // And a ProxyBuilder yielding a future created in combination with a proxy builder callback
    class ProxyBuilderForTest
    {
        using Trait = ProxySpecTraits<FakeProxyBase>;

      public:
        auto Build()
        {
            InterruptiblePromise promise;
            ProxyFuture future{promise.GetInterruptibleFuture().value()};

            strategy_.emplace();
            strategy_->Find(std::make_unique<typename ProxyBuilderBase<FakeProxyBase>::BuilderCallback>(
                std::move(promise), Trait::UserCallback{}));

            return future;
        }

        void TriggerStopFind() noexcept
        {
            strategy_->StopFind();
        }

      private:
        std::optional<DummyFindProxyStrategy> strategy_;
    };

    // When invoking the proxy builder
    ProxyBuilderForTest proxy_builder{};
    auto future = proxy_builder.Build();

    // Then the future must be waiting for the proxy builder's result to be retrieved
    ASSERT_EQ(score::concurrency::Error::kTimeout, future.WaitFor({}, kAsyncFindFutureTimeout).error());

    // When triggering the Strategy's method StopFind() now
    proxy_builder.TriggerStopFind();

    // Then the proxy builder's future must have gotten released
    ASSERT_TRUE(future.Ready());

    // And score::concurrency::Error::kStopRequested must be reported upon Get()
    EXPECT_EQ(score::concurrency::Error::kStopRequested, future.Get({}).error());
}

}  // namespace
}  // namespace score::mw::service::details
