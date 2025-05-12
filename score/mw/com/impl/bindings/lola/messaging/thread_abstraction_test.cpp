/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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
#include "score/mw/com/impl/bindings/lola/messaging/thread_abstraction.h"
#include "score/mw/com/impl/bindings/lola/messaging/thread_abstraction_mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <thread>

namespace score::mw::com::impl::lola
{

class ThreadHWConcurrencyMockGuard
{
  public:
    ThreadHWConcurrencyMockGuard()
    {
        ThreadHWConcurrency::injectMock(&mock_);
    }
    ~ThreadHWConcurrencyMockGuard()
    {
        ThreadHWConcurrency::injectMock(nullptr);
    }

    ThreadHWConcurrencyMock mock_;
};

TEST(ThreadHWConcurrencyTest, UsesInjectedMockToDetermineHardwareConcurrency)
{
    // When injected with mock
    ThreadHWConcurrencyMockGuard guard;
    EXPECT_CALL(guard.mock_, hardware_concurrency()).WillOnce(::testing::Return(8U));

    // Then expect the same value in return
    EXPECT_EQ(ThreadHWConcurrency::hardware_concurrency(), 8U);
}

TEST(ThreadHWConcurrencyTest, DefaultsToStdThreadWhenNoMockIsInjected)
{
    EXPECT_EQ(ThreadHWConcurrency::hardware_concurrency(), std::thread::hardware_concurrency());
}

}  // namespace score::mw::com::impl::lola
