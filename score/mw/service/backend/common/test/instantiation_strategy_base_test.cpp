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

#include "score/mw/service/backend/common/instantiation_strategy_base.h"

#include <gtest/gtest.h>

#include <score/assert_support.hpp>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace score::mw::service::backend::common::test
{
namespace
{

class TestBackendProxy final
{
};

struct ProxyHolderState
{
    std::atomic_uint32_t stop_find_calls{0U};
    std::function<void()> on_start_find_service{};
};

class TestProxyHolder final
{
  public:
    explicit TestProxyHolder(std::shared_ptr<ProxyHolderState> state) : state_{std::move(state)} {}

    template <typename Callback>
    void StartFindService(Callback&& callback)
    {
        static_cast<void>(callback);
        if (state_->on_start_find_service != nullptr)
        {
            state_->on_start_find_service();
        }
    }

    void StopFindService() noexcept
    {
        ++state_->stop_find_calls;
    }

    std::vector<std::unique_ptr<TestBackendProxy>> ExtractProxies()
    {
        return {};
    }

  private:
    std::shared_ptr<ProxyHolderState> state_;
};

std::shared_ptr<ProxyHolderState> gProxyHolderState{};

struct TestProxyCreator
{
    std::shared_ptr<TestProxyHolder> operator()(std::string /*port_identifier*/) const
    {
        return std::make_shared<TestProxyHolder>(gProxyHolderState);
    }
};

class TestInstantiationStrategy final : public InstantiationStrategyBase<TestBackendProxy, TestProxyCreator>
{
    using Base = InstantiationStrategyBase<TestBackendProxy, TestProxyCreator>;

  public:
    explicit TestInstantiationStrategy(std::string port_identifier) : Base(std::move(port_identifier)) {}

    void StartFindForTest()
    {
        Base::StartFind(
            [](std::vector<std::unique_ptr<TestBackendProxy>> /*found_proxies*/, std::string_view /*port_identifier*/) {
                return Base::ShallStopFindService::kNo;
            });
    }

    void StopFindForTest() noexcept
    {
        Base::StopFind();
    }
};

class InstantiationStrategyBaseTest : public ::testing::Test
{
  public:
    void SetUp() override
    {
        gProxyHolderState = std::make_shared<ProxyHolderState>();
    }

    void TearDown() override
    {
        gProxyHolderState.reset();
    }
};

TEST_F(InstantiationStrategyBaseTest, StartFindTwiceWithoutStopTriggersPrecondition)
{
    TestInstantiationStrategy strategy{"dummy_port"};

    strategy.StartFindForTest();

    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(strategy.StartFindForTest());
}

TEST_F(InstantiationStrategyBaseTest, StopFindDuringStartFindStopsCreatedProxyHolder)
{
    TestInstantiationStrategy strategy{"dummy_port"};

    gProxyHolderState->on_start_find_service = [&strategy]() {
        strategy.StopFindForTest();
    };

    strategy.StartFindForTest();

    EXPECT_TRUE(strategy.IsStopped());
    EXPECT_EQ(gProxyHolderState->stop_find_calls.load(), 2U);
}

}  // namespace
}  // namespace score::mw::service::backend::common::test
