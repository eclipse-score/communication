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

#include "score/concurrency/future/interruptible_promise.h"
#include "score/language/safecpp/scoped_function/scope.h"

#include "score/mw/service/proxy_future.h"

#include "gtest/gtest.h"

#include <chrono>
#include <optional>

namespace score::mw::service::test
{
namespace
{

using namespace std::chrono_literals;

class ProxyFutureFixture : public ::testing::Test
{
  public:
    ProxyFutureFixture& GivenAProxyFuture()
    {
        score::cpp::ignore = proxy_future.emplace(promise.GetInterruptibleFuture().value());
        return *this;
    }

    ProxyFutureFixture& GivenAProxyFutureWithValue()
    {
        GivenAProxyFuture();
        score::cpp::ignore = promise.SetValue();
        return *this;
    }

    score::concurrency::InterruptiblePromise<void> promise{};
    std::optional<ProxyFuture<void>> proxy_future{};
};

TEST_F(ProxyFutureFixture, CanConstruct)
{
    // When creating a ProxyFuture
    GivenAProxyFuture();

    // Then it must be valid
    EXPECT_TRUE(proxy_future->Valid());
}

TEST_F(ProxyFutureFixture, CanDestroy)
{
    // When creating a ProxyFuture
    GivenAProxyFuture();

    // Then destruction must work without any issues
    EXPECT_NO_THROW(proxy_future.reset());
}

// Note: We don't create extensive tests for Valid/Ready/Wait/Get since this functionality is tested in
// InterruptibleFuture (which ProxyFuture inherits from). We test the initial conditions and basic API surface.
TEST_F(ProxyFutureFixture, ProxyFutureIsValidButNotReadyOnCreation)
{
    // When creating a ProxyFuture
    GivenAProxyFuture();

    // Then the future is valid but not ready
    EXPECT_TRUE(proxy_future->Valid());
    EXPECT_FALSE(proxy_future->Ready());
}

TEST_F(ProxyFutureFixture, ProxyFutureBecomesReadyWhenPromiseIsSet)
{
    // Given a ProxyFuture with its promise already satisfied
    GivenAProxyFutureWithValue();

    // Then the future must be ready
    EXPECT_TRUE(proxy_future->Valid());
    EXPECT_TRUE(proxy_future->Ready());
}

TEST_F(ProxyFutureFixture, CanGetValueWhenReady)
{
    // Given a ProxyFuture with its promise already satisfied
    GivenAProxyFutureWithValue();

    // When getting the value
    auto result = proxy_future->Get({});

    // Then it must contain a value
    EXPECT_TRUE(result.has_value());
}

TEST_F(ProxyFutureFixture, WaitForReturnsEmptyOptionalWhenNotReady)
{
    // Given a ProxyFuture that is not ready
    GivenAProxyFuture();

    // When waiting with timeout
    auto result = proxy_future->WaitFor({}, 10ms);

    // Then it must return an empty optional
    EXPECT_FALSE(result.has_value());
}

TEST_F(ProxyFutureFixture, WaitForReturnsValueWhenReady)
{
    // Given a ProxyFuture with its promise already satisfied
    GivenAProxyFutureWithValue();

    // When waiting with timeout
    auto result = proxy_future->WaitFor({}, 10ms);

    // Then it must contain a value (WaitFor returns optional<Result<void>>, and for void, Result is empty)
    EXPECT_TRUE(result.has_value());
}

TEST_F(ProxyFutureFixture, CanMoveConstruct)
{
    // Given a ProxyFuture
    GivenAProxyFuture();

    // When move-constructing another ProxyFuture instance
    ProxyFuture<void> move_constructed{std::move(proxy_future).value()};

    // Then the new instance must be valid
    EXPECT_TRUE(move_constructed.Valid());
    EXPECT_FALSE(move_constructed.Ready());
}

TEST_F(ProxyFutureFixture, CanMoveAssign)
{
    // Given a ProxyFuture
    GivenAProxyFuture();

    // When move-assigning to another ProxyFuture instance
    ProxyFuture<void> move_assigned{
        std::make_unique<score::concurrency::InterruptiblePromise<void>>()->GetInterruptibleFuture().value()};
    move_assigned = std::move(proxy_future).value();

    // Then the new instance must be valid
    EXPECT_TRUE(move_assigned.Valid());
    EXPECT_FALSE(move_assigned.Ready());
}

TEST_F(ProxyFutureFixture, CanConstructFromInterruptibleFuture)
{
    // Given an InterruptibleFuture
    auto interruptible_future = promise.GetInterruptibleFuture().value();

    // When constructing a ProxyFuture from it
    ProxyFuture<void> interruptible_proxy_future{std::move(interruptible_future)};

    // Then the ProxyFuture must be valid
    EXPECT_TRUE(interruptible_proxy_future.Valid());
}

TEST_F(ProxyFutureFixture, CanAttachMoveOnlyScopedContinuation)
{
    // Given a proxy future and a scope that bounds continuation execution
    GivenAProxyFuture();
    score::safecpp::Scope<> scope{};
    bool callback_called{false};

    // When attaching a move-only scoped continuation and fulfilling the promise
    auto registration_result = proxy_future->Then(score::safecpp::MoveOnlyScopedFunction<void(score::Result<void>&)>{
        scope, [&callback_called](score::Result<void>& result) {
            if (result.has_value())
            {
                callback_called = true;
            }
        }});
    score::cpp::ignore = promise.SetValue();

    // Then registration succeeds and the continuation is invoked
    EXPECT_TRUE(registration_result.has_value());
    EXPECT_TRUE(callback_called);
}

}  // namespace
}  // namespace score::mw::service::test
