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

#include "score/mw/service/utils/shared_optional_proxy_holder/shared_optional_proxy_holder.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "score/concurrency/future/interruptible_promise.h"
#include "score/language/safecpp/scoped_function/scope.h"

#include <score/assert_support.hpp>
#include <score/jthread.hpp>
#include <score/stop_token.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

namespace score::mw::service::utils
{
namespace
{
using score::mw::service::ProxyFuture;
using namespace std::chrono_literals;

// Mock proxy interface for testing
class Proxy
{
  public:
    virtual ~Proxy() = default;
    virtual int GetValue() const = 0;
};

class ProxyMock : public Proxy
{
  public:
    explicit ProxyMock(int value) : value_{value} {}
    int GetValue() const override
    {
        return value_;
    }

  private:
    int value_;
};

}  // namespace

class SharedOptionalProxyHolderTest : public ::testing::Test
{
  protected:
    score::safecpp::Scope<> scope_;
};

TEST_F(SharedOptionalProxyHolderTest, InitiallyNotAvailable)
{
    // Given a promise/future pair
    score::concurrency::InterruptiblePromise<std::unique_ptr<Proxy>> promise{};
    ProxyFuture<std::unique_ptr<Proxy>> future{promise.GetInterruptibleFuture().value()};

    // When creating a holder without resolving the future
    auto holder = SharedOptionalProxyHolder<Proxy>(std::move(future));

    // Then the proxy should not be available
    EXPECT_FALSE(holder.IsAvailable());
}

TEST_F(SharedOptionalProxyHolderTest, AvailableAfterFutureResolves)
{
    // Given a holder with an unresolved future
    score::concurrency::InterruptiblePromise<std::unique_ptr<Proxy>> promise{};
    ProxyFuture<std::unique_ptr<Proxy>> future{promise.GetInterruptibleFuture().value()};
    auto holder = SharedOptionalProxyHolder<Proxy>(std::move(future));

    // When the promise is resolved
    promise.SetValue(std::make_unique<ProxyMock>(42));

    // Then the proxy should be available
    EXPECT_TRUE(holder.IsAvailable());
}

TEST_F(SharedOptionalProxyHolderTest, GetReturnsCorrectProxy)
{
    // Given a holder with a resolved future containing value 42
    score::concurrency::InterruptiblePromise<std::unique_ptr<Proxy>> promise{};
    ProxyFuture<std::unique_ptr<Proxy>> future{promise.GetInterruptibleFuture().value()};
    auto holder = SharedOptionalProxyHolder<Proxy>(std::move(future));
    promise.SetValue(std::make_unique<ProxyMock>(42));

    // When calling Get()
    ASSERT_TRUE(holder.IsAvailable());
    auto& proxy = holder.Get();

    // Then it should return the proxy with the correct value
    EXPECT_EQ(42, proxy.GetValue());
}

TEST_F(SharedOptionalProxyHolderTest, CallbackInvokedWhenProxyAvailable)
{
    // Given a holder with a callback to track invocation
    score::concurrency::InterruptiblePromise<std::unique_ptr<Proxy>> promise{};
    ProxyFuture<std::unique_ptr<Proxy>> future{promise.GetInterruptibleFuture().value()};
    std::atomic<bool> callback_invoked{false};

    score::safecpp::MoveOnlyScopedFunction<void(score::Result<std::unique_ptr<Proxy>>&)> callback{
        scope_, [&callback_invoked](score::Result<std::unique_ptr<Proxy>>& result) {
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(123, result.value()->GetValue());
            callback_invoked.store(true);
        }};
    auto holder = SharedOptionalProxyHolder<Proxy>(std::move(future), std::move(callback));

    EXPECT_FALSE(callback_invoked.load());

    // When the promise is resolved
    promise.SetValue(std::make_unique<ProxyMock>(123));

    // Then the callback should be invoked and proxy should be available
    EXPECT_TRUE(callback_invoked.load());
    EXPECT_TRUE(holder.IsAvailable());
    EXPECT_EQ(123, holder.Get().GetValue());
}

TEST_F(SharedOptionalProxyHolderTest, NoCallbackDoesNotCrash)
{
    // Given a holder without a callback
    score::concurrency::InterruptiblePromise<std::unique_ptr<Proxy>> promise{};
    ProxyFuture<std::unique_ptr<Proxy>> future{promise.GetInterruptibleFuture().value()};
    auto holder = SharedOptionalProxyHolder<Proxy>(std::move(future));

    // When the promise is resolved
    EXPECT_NO_THROW(promise.SetValue(std::make_unique<ProxyMock>(99)));

    // Then the proxy should be available without crashing
    EXPECT_TRUE(holder.IsAvailable());
}

TEST_F(SharedOptionalProxyHolderTest, ShareableAcrossComponents)
{
    // Given a holder with a resolved proxy
    score::concurrency::InterruptiblePromise<std::unique_ptr<Proxy>> promise{};
    ProxyFuture<std::unique_ptr<Proxy>> future{promise.GetInterruptibleFuture().value()};
    auto holder = SharedOptionalProxyHolder<Proxy>(std::move(future));
    promise.SetValue(std::make_unique<ProxyMock>(777));

    // When the holder is copied to multiple components
    auto component1_holder = holder;
    auto component2_holder = holder;
    auto component3_holder = holder;

    // Then all components should see the same proxy with the same value
    EXPECT_TRUE(component1_holder.IsAvailable());
    EXPECT_TRUE(component2_holder.IsAvailable());
    EXPECT_TRUE(component3_holder.IsAvailable());

    EXPECT_EQ(777, component1_holder.Get().GetValue());
    EXPECT_EQ(777, component2_holder.Get().GetValue());
    EXPECT_EQ(777, component3_holder.Get().GetValue());
}

TEST_F(SharedOptionalProxyHolderTest, ConcurrentAccessIsThreadSafe)
{
    // Given a holder with a resolved proxy
    score::concurrency::InterruptiblePromise<std::unique_ptr<Proxy>> promise{};
    ProxyFuture<std::unique_ptr<Proxy>> future{promise.GetInterruptibleFuture().value()};
    auto holder = SharedOptionalProxyHolder<Proxy>(std::move(future));
    promise.SetValue(std::make_unique<ProxyMock>(888));

    std::atomic<int> successful_accesses{0};
    std::vector<score::cpp::jthread> threads;

    // When multiple threads access the holder concurrently
    for (int i = 0; i < 10; ++i)
    {
        threads.emplace_back([holder, &successful_accesses](const score::cpp::stop_token stop_token) {
            while (!stop_token.stop_requested())
            {
                if (holder.IsAvailable())
                {
                    auto& proxy = holder.Get();
                    if (proxy.GetValue() == 888)
                    {
                        successful_accesses.fetch_add(1);
                    }
                }
            }
        });
    }

    // Wait for sufficient successful accesses
    for (std::size_t num_attempts = 0; num_attempts < 3'000'000; ++num_attempts)
    {
        if (successful_accesses.load() > 1'000)
        {
            break;
        }
    }

    threads.clear();

    // Then all threads should have successfully accessed the proxy without data races
    EXPECT_GT(successful_accesses.load(), 1'000U);
}

TEST_F(SharedOptionalProxyHolderTest, ProxyNotAvailableWhenFutureFails)
{
    // Given a holder with a callback to track invocation
    score::concurrency::InterruptiblePromise<std::unique_ptr<Proxy>> promise{};
    ProxyFuture<std::unique_ptr<Proxy>> future{promise.GetInterruptibleFuture().value()};
    std::atomic<bool> callback_invoked{false};
    std::atomic<bool> callback_received_error{false};

    score::safecpp::MoveOnlyScopedFunction<void(score::Result<std::unique_ptr<Proxy>>&)> callback{
        scope_, [&callback_invoked, &callback_received_error](score::Result<std::unique_ptr<Proxy>>& result) {
            callback_invoked.store(true);
            callback_received_error.store(!result.has_value());
        }};
    auto holder = SharedOptionalProxyHolder<Proxy>(std::move(future), std::move(callback));

    // When the promise fails with an error
    promise.SetError(score::concurrency::Error{});

    // Then the proxy should not be available but callback should be invoked with error
    EXPECT_FALSE(holder.IsAvailable());
    EXPECT_TRUE(callback_invoked.load());
    EXPECT_TRUE(callback_received_error.load());
}

TEST_F(SharedOptionalProxyHolderTest, MultipleAccessesToGetReturnSameProxy)
{
    // Given a holder with a resolved future
    score::concurrency::InterruptiblePromise<std::unique_ptr<Proxy>> promise{};
    ProxyFuture<std::unique_ptr<Proxy>> future{promise.GetInterruptibleFuture().value()};
    auto holder = SharedOptionalProxyHolder<Proxy>(std::move(future));
    promise.SetValue(std::make_unique<ProxyMock>(555));

    // When calling Get() multiple times
    ASSERT_TRUE(holder.IsAvailable());
    auto& proxy1 = holder.Get();
    auto& proxy2 = holder.Get();
    auto& proxy3 = holder.Get();

    // Then all references should point to the same proxy
    EXPECT_EQ(&proxy1, &proxy2);
    EXPECT_EQ(&proxy2, &proxy3);
    EXPECT_EQ(555, proxy1.GetValue());
}

TEST_F(SharedOptionalProxyHolderTest, OperatorArrowReturnsPointerToProxy)
{
    // Given a holder with a resolved future
    score::concurrency::InterruptiblePromise<std::unique_ptr<Proxy>> promise{};
    ProxyFuture<std::unique_ptr<Proxy>> future{promise.GetInterruptibleFuture().value()};
    auto holder = SharedOptionalProxyHolder<Proxy>(std::move(future));
    promise.SetValue(std::make_unique<ProxyMock>(555));

    // When using operator-> to access the proxy
    ASSERT_TRUE(holder.IsAvailable());
    auto* proxy_via_arrow_operator = holder.operator->();
    auto* proxy_get = &holder.Get();

    // Then it should return a pointer to the proxy
    EXPECT_EQ(proxy_via_arrow_operator, proxy_get);

    // And calling a method via operator-> should return the same result
    EXPECT_EQ(holder.Get().GetValue(), proxy_via_arrow_operator->GetValue());
}

TEST_F(SharedOptionalProxyHolderTest, StarOperatorReturnsReferenceToProxy)
{
    // Given a holder with a resolved future
    score::concurrency::InterruptiblePromise<std::unique_ptr<Proxy>> promise{};
    ProxyFuture<std::unique_ptr<Proxy>> future{promise.GetInterruptibleFuture().value()};
    auto holder = SharedOptionalProxyHolder<Proxy>(std::move(future));
    promise.SetValue(std::make_unique<ProxyMock>(555));

    // When using operator* to access the proxy
    ASSERT_TRUE(holder.IsAvailable());
    auto& proxy_via_star_operator = *holder;

    // Then it should point to the same proxy as Get()
    EXPECT_EQ(&proxy_via_star_operator, &holder.Get());

    // And calling a method via operator* should return the same result
    EXPECT_EQ(holder.Get().GetValue(), (*holder).GetValue());
}

TEST_F(SharedOptionalProxyHolderTest, GetAssertsWhenProxyNotAvailable)
{
    // Given a holder without a resolved proxy
    score::concurrency::InterruptiblePromise<std::unique_ptr<Proxy>> promise{};
    ProxyFuture<std::unique_ptr<Proxy>> future{promise.GetInterruptibleFuture().value()};
    auto holder = SharedOptionalProxyHolder<Proxy>(std::move(future));

    // When calling Get() without proxy being available
    ASSERT_FALSE(holder.IsAvailable());

    // Then contract violation should be triggered
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = holder.Get());
}

TEST_F(SharedOptionalProxyHolderTest, ConstructorAssertsWhenThenFails)
{
    // Given a default-initialized (invalid) ProxyFuture
    ProxyFuture<std::unique_ptr<Proxy>> invalid_future{};

    // When attempting to create a holder with the invalid future
    // Then contract violation should be triggered in constructor
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(auto holder = SharedOptionalProxyHolder<Proxy>(std::move(invalid_future)));
}

TEST_F(SharedOptionalProxyHolderTest, ProxyBecomesAvailableAfterCopy)
{
    // Given a holder and a copy of the holder with an unresolved future
    score::concurrency::InterruptiblePromise<std::unique_ptr<Proxy>> promise{};
    ProxyFuture<std::unique_ptr<Proxy>> future{promise.GetInterruptibleFuture().value()};

    auto holder = SharedOptionalProxyHolder<Proxy>(std::move(future));
    auto holder_copy = holder;

    // Initially both holders should not have the proxy available
    EXPECT_FALSE(holder.IsAvailable());
    EXPECT_FALSE(holder_copy.IsAvailable());

    // When the promise is resolved
    promise.SetValue(std::make_unique<ProxyMock>(42));

    // Then both holders must be available
    EXPECT_TRUE(holder.IsAvailable());
    EXPECT_TRUE(holder_copy.IsAvailable());

    // And both holders must point to the same proxy
    EXPECT_EQ(&holder.Get(), &holder_copy.Get());
}

}  // namespace score::mw::service::utils
