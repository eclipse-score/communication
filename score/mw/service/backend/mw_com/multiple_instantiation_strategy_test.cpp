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

#include "score/mw/service/backend/mw_com/multiple_instantiation_strategy.h"

#include "score/mw/service/backend/mw_com/proxy_stub.h"

#include "score/concurrency/future/interruptible_promise.h"
#include "score/mw/com/types.h"
#include "score/result/result.h"

#include <score/assert.hpp>
#include <score/utility.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace score::mw::service
{
namespace
{

using namespace ::testing;

using MwComProxy = mw::service::backend::mw_com::FakeProxy;

constexpr auto kPortIdentifierString = "Some/Instance/Specifier";
const auto kInstanceSpecifier = score::mw::com::InstanceSpecifier::Create(kPortIdentifierString).value();
const auto kDummyFindServiceHandle = MwComProxy::CreateFindServiceHandle(10U);
constexpr typename MwComProxy::HandleType kDummyProxyHandle1{1};
constexpr typename MwComProxy::HandleType kDummyProxyHandle2{2};

class Proxy
{
  public:
    virtual ~Proxy() = default;
};

class MyFakeProxy : public Proxy
{
  public:
    explicit MyFakeProxy(std::unique_ptr<MwComProxy>) {}
};

struct PortIdentifier
{
    constexpr static auto Get()
    {
        return kPortIdentifierString;
    }
};

using Strategy = backend::mw_com::MultipleInstantiationStrategy<Proxy, MyFakeProxy, MwComProxy, PortIdentifier>;

class MultipleInstantiationStrategyFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        ON_CALL(proxy_mock, StartFindService(_, _))
            .WillByDefault(Invoke([](auto&& /*callback*/, auto&& /*instance_specifier*/) {
                return kDummyFindServiceHandle;
            }));

        ON_CALL(proxy_mock, StopFindService(kDummyFindServiceHandle)).WillByDefault(Invoke([this] {
            stop_find_service_invocations++;
            return score::Result<void>{};
        }));
    }

    MultipleInstantiationStrategyFixture& GivenAMultipleInstantiationStrategy()
    {
        score::cpp::ignore = strategy.emplace();
        return *this;
    }

    MultipleInstantiationStrategyFixture& WhichHasStartedFind()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT(strategy.has_value());
        auto on_found_builder_callback = CreateBuilderCallbackWithMockedUserCallback();
        strategy.value().Find(std::move(on_found_builder_callback));
        return *this;
    }

    std::unique_ptr<typename ProxyBuilderBase<Multiple<Proxy>>::BuilderCallback>
    CreateBuilderCallbackWithMockedUserCallback()
    {
        score::concurrency::InterruptiblePromise<MultiInstanceHolder<Proxy>> promise{};
        score::cpp::callback<void(Proxy&)> user_callback = [this](Proxy& dummy_receiver) {
            mock_user_callback.AsStdFunction()(dummy_receiver);
        };
        auto on_found_builder_callback = std::make_unique<typename ProxyBuilderBase<Multiple<Proxy>>::BuilderCallback>(
            std::move(promise), std::move(user_callback));
        return on_found_builder_callback;
    }

    backend::mw_com::FakeProxyMockGuard<MwComProxy> fake_proxy_mock_guard{};
    MwComProxy::Mock& proxy_mock{MwComProxy::GetMockInstance()};

    std::optional<Strategy> strategy{};

    MockFunction<void(Proxy&)> mock_user_callback{};
    std::uint32_t stop_find_service_invocations{0U};
};

TEST_F(MultipleInstantiationStrategyFixture, CanGetPortIdentifierWhenConstructingWithPortIdentifierTemplateArgument)
{
    // Given a default-constructed Strategy instance whereby a `PortIdentifier` type got provided as template argument
    backend::mw_com::MultipleInstantiationStrategy<Proxy, MyFakeProxy, MwComProxy, PortIdentifier> local_strategy{};

    // When getting the PortIdentifier
    const auto& port_identifier = local_strategy.GetPortIdentifier();

    // Then its port identifier must have gotten determined via the `PortIdentifier` type defined above
    EXPECT_EQ(port_identifier, PortIdentifier::Get());
}

TEST_F(MultipleInstantiationStrategyFixture, CanGetPortIdentifierWhenConstructingWithPortIdentifierConstructorArgument)
{
    // Given a Strategy instance where no `PortIdentifier` type was provided as template argument but was provided to
    // the constructor
    backend::mw_com::MultipleInstantiationStrategy<Proxy, MyFakeProxy, MwComProxy> local_strategy{
        kPortIdentifierString};

    // When getting the PortIdentifier
    const auto& port_identifier = local_strategy.GetPortIdentifier();

    // Then its port identifier must be equal to the one provided as constructor argument
    EXPECT_EQ(port_identifier, kPortIdentifierString);
}

TEST_F(MultipleInstantiationStrategyFixture, StartAndStopFindServiceAreNotCalledOnConstruction)
{
    // Expecting that Start/StopFindService will never be called
    EXPECT_CALL(proxy_mock, StartFindService(_, _)).Times(0);
    EXPECT_CALL(proxy_mock, StopFindService(_)).Times(0);

    // and expecting that the user callback that's registered with Find() is never called
    EXPECT_CALL(mock_user_callback, Call(_)).Times(0);

    // When creating a MultipleInstantiationStrategy without calling Find()
    GivenAMultipleInstantiationStrategy();
}

TEST_F(MultipleInstantiationStrategyFixture, CallingFindWillCallStartFindService)
{
    // Expecting that StartFindService will be called
    EXPECT_CALL(proxy_mock, StartFindService(_, kInstanceSpecifier)).Times(1);

    // Given a MultipleInstantiationStrategy
    GivenAMultipleInstantiationStrategy();

    // When calling Find()
    WhichHasStartedFind();
}

TEST_F(MultipleInstantiationStrategyFixture, CallingStopFindWillCallStopFindService)
{
    // Expecting that StopFindService will be called
    EXPECT_CALL(proxy_mock, StopFindService(_)).Times(1);

    // Given a MultipleInstantiationStrategy which has started Find()
    GivenAMultipleInstantiationStrategy().WhichHasStartedFind();

    // When calling StopFind()
    EXPECT_EQ(stop_find_service_invocations, 0U);
    strategy->StopFind();

    // Then StopFindService will be called
    EXPECT_EQ(stop_find_service_invocations, 1U);
}

TEST_F(MultipleInstantiationStrategyFixture, StopFindWillBeCalledOnDestruction)
{
    // Expecting that StopFindService will be called
    EXPECT_CALL(proxy_mock, StopFindService(_)).Times(1);

    // Given a MultipleInstantiationStrategy which has started Find()
    GivenAMultipleInstantiationStrategy().WhichHasStartedFind();

    // When destroying the MultipleInstantiationStrategy
    EXPECT_EQ(stop_find_service_invocations, 0U);
    strategy.reset();

    // Then StopFindService will only be called after destruction
    EXPECT_EQ(stop_find_service_invocations, 1U);
}

TEST_F(MultipleInstantiationStrategyFixture, UserCallbackNotInvokedWhenServiceIsSearchedButNotFound)
{
    // Expecting that the user callback that's registered with Find() is never called
    EXPECT_CALL(mock_user_callback, Call(_)).Times(0);

    // Given a MultipleInstantiationStrategy
    GivenAMultipleInstantiationStrategy();

    // When calling Find() but no service is found
    WhichHasStartedFind();
}

TEST_F(MultipleInstantiationStrategyFixture, UserCallbackIsInvokedWhenServiceIsSearchedAndFound)
{
    // Expecting that the user callback that's registered with Find() is called once
    EXPECT_CALL(mock_user_callback, Call(_)).Times(1);

    // Given that StartFindService is called and the callback is called with a handle of a found service
    ON_CALL(proxy_mock, StartFindService(_, kInstanceSpecifier))
        .WillByDefault(Invoke([](auto&& callback, auto&& /*instance_specifier*/) {
            callback({kDummyProxyHandle1}, kDummyFindServiceHandle);
            return kDummyFindServiceHandle;
        }));

    // and given a MultipleInstantiationStrategy
    GivenAMultipleInstantiationStrategy();

    // When calling Find()
    WhichHasStartedFind();
}

TEST_F(MultipleInstantiationStrategyFixture, StopFindServiceNotCalledWhenServiceIsSearchedAndFound)
{
    // Given that StartFindService is called and the callback is called with a handle of a found service
    ON_CALL(proxy_mock, StartFindService(_, kInstanceSpecifier))
        .WillByDefault(Invoke([](auto&& callback, auto&& /*instance_specifier*/) {
            callback({kDummyProxyHandle1}, kDummyFindServiceHandle);
            return kDummyFindServiceHandle;
        }));

    // and given a MultipleInstantiationStrategy
    GivenAMultipleInstantiationStrategy();

    // When calling Find()
    WhichHasStartedFind();

    // Then StopFindService will not be called after the service instance is found
    EXPECT_EQ(stop_find_service_invocations, 0U);
}

TEST_F(MultipleInstantiationStrategyFixture, UserCallbackIsInvokedOncePerInstanceWhenMultipleInstancesFound)
{
    // Expecting that the user callback that's registered with Find() is called once per handle provided to the
    // StartFindServiceHandler
    EXPECT_CALL(mock_user_callback, Call(_)).Times(2);

    // Given that StartFindService is called and the callback is called with multiple handles of a found service
    ON_CALL(proxy_mock, StartFindService(_, kInstanceSpecifier))
        .WillByDefault(Invoke([](auto&& callback, auto&& /*instance_specifier*/) {
            callback({kDummyProxyHandle1, kDummyProxyHandle2}, kDummyFindServiceHandle);
            return kDummyFindServiceHandle;
        }));

    // and given a MultipleInstantiationStrategy
    GivenAMultipleInstantiationStrategy();

    // When calling Find()
    WhichHasStartedFind();
}

TEST_F(MultipleInstantiationStrategyFixture, UserCallbackNotInvokedWhenServiceFoundWithNoInstances)
{
    // Expecting that the user callback that's registered with Find() is never called
    EXPECT_CALL(mock_user_callback, Call(_)).Times(0);

    // Given that StartFindService is called and the callback is called with no handles of a found service
    ON_CALL(proxy_mock, StartFindService(_, kInstanceSpecifier))
        .WillByDefault(Invoke([](auto&& callback, auto&& /*instance_specifier*/) {
            callback({}, kDummyFindServiceHandle);
            return kDummyFindServiceHandle;
        }));

    // Given a MultipleInstantiationStrategy
    GivenAMultipleInstantiationStrategy();

    // When calling Find()
    WhichHasStartedFind();
}

}  // namespace
}  // namespace score::mw::service
