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

#include "score/mw/service/backend/mw_com/single_instantiation_strategy.h"

#include "score/mw/service/backend/mw_com/proxy_stub.h"

#include "score/result/result.h"
#include "score/concurrency/future/interruptible_promise.h"
#include "score/mw/com/com_error_domain.h"
#include "score/mw/com/types.h"

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

using Strategy = backend::mw_com::SingleInstantiationStrategy<Proxy, MyFakeProxy, MwComProxy, PortIdentifier>;

class MwComSingleInstantiationStrategyFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        ON_CALL(proxy_mock, StartFindService(_, _)).WillByDefault(Return(kDummyFindServiceHandle));

        ON_CALL(proxy_mock, StopFindService(_)).WillByDefault(Invoke([this] {
            stop_find_service_invocations++;
            return score::Result<void>{};
        }));
    }

    MwComSingleInstantiationStrategyFixture& GivenAnMwComSingleInstantiationStrategy()
    {
        score::cpp::ignore = strategy.emplace();
        return *this;
    }

    MwComSingleInstantiationStrategyFixture& WhichHasStartedFind()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT(strategy.has_value());
        auto on_found_builder_callback = CreateBuilderCallbackWithMockedUserCallback();
        strategy.value().Find(std::move(on_found_builder_callback));
        return *this;
    }

    std::unique_ptr<typename ProxyBuilderBase<Proxy>::BuilderCallback> CreateBuilderCallbackWithMockedUserCallback()
    {
        score::concurrency::InterruptiblePromise<SingleInstanceHolder<Proxy>> promise{};
        score::cpp::callback<void(Proxy&)> user_callback = [this](Proxy& dummy_receiver) {
            mock_user_callback.AsStdFunction()(dummy_receiver);
        };
        auto on_found_builder_callback = std::make_unique<typename ProxyBuilderBase<Proxy>::BuilderCallback>(
            std::move(promise), std::move(user_callback));
        return on_found_builder_callback;
    }

    backend::mw_com::FakeProxyMockGuard<MwComProxy> fake_proxy_mock_guard{};
    MwComProxy::Mock& proxy_mock{MwComProxy::GetMockInstance()};

    std::optional<Strategy> strategy{};

    MockFunction<void(Proxy&)> mock_user_callback{};
    std::uint32_t stop_find_service_invocations{0U};
};

TEST_F(MwComSingleInstantiationStrategyFixture, CanGetPortIdentifierWhenConstructingWithPortIdentifierTemplateArgument)
{
    // Given a default-constructed Strategy instance whereby a `PortIdentifier` type got provided as template argument
    backend::mw_com::SingleInstantiationStrategy<Proxy, MyFakeProxy, MwComProxy, PortIdentifier> local_strategy{};

    // When getting the PortIdentifier
    const auto& port_identifier = local_strategy.GetPortIdentifier();

    // Then its port identifier must have gotten determined via the `PortIdentifier` type defined above
    EXPECT_EQ(port_identifier, PortIdentifier::Get());
}

TEST_F(MwComSingleInstantiationStrategyFixture,
       CanGetPortIdentifierWhenConstructingWithPortIdentifierConstructorArgument)
{
    // Given a Strategy instance where no `PortIdentifier` type got provided as template argument but was provided to
    // the constructor
    backend::mw_com::SingleInstantiationStrategy<Proxy, MyFakeProxy, MwComProxy> local_strategy{kPortIdentifierString};

    // When getting the PortIdentifier
    const auto& port_identifier = local_strategy.GetPortIdentifier();

    // Then its port identifier must be equal to the one provided as constructor argument
    EXPECT_EQ(port_identifier, kPortIdentifierString);
}

TEST_F(MwComSingleInstantiationStrategyFixture, StartAndStopFindServiceAreNotCalledOnConstruction)
{
    // Expecting that Start/StopFindService will never be called
    EXPECT_CALL(proxy_mock, StartFindService(_, _)).Times(0);
    EXPECT_CALL(proxy_mock, StopFindService(_)).Times(0);

    // and expecting that the user callback will not be called
    EXPECT_CALL(mock_user_callback, Call(_)).Times(0);

    // When creating a backend::mw_com::SingleInstantiationStrategy without calling Find()
    GivenAnMwComSingleInstantiationStrategy();
}

TEST_F(MwComSingleInstantiationStrategyFixture, CallingFindWillCallStartFindService)
{
    // Expecting that StartFindService will be called
    EXPECT_CALL(proxy_mock, StartFindService(_, kInstanceSpecifier)).Times(1);

    // Given a backend::mw_com::SingleInstantiationStrategy
    GivenAnMwComSingleInstantiationStrategy();

    // When calling Find()
    WhichHasStartedFind();
}

TEST_F(MwComSingleInstantiationStrategyFixture, CallingStopFindWillCallStopFindService)
{
    // Expecting that StopFindService will be called
    EXPECT_CALL(proxy_mock, StopFindService(_)).Times(1);

    // Given a backend::mw_com::SingleInstantiationStrategy which has started Find()
    GivenAnMwComSingleInstantiationStrategy().WhichHasStartedFind();

    // When calling StopFind()
    // Then StopFindService will be called
    EXPECT_EQ(stop_find_service_invocations, 0U);
    strategy->StopFind();
    EXPECT_EQ(stop_find_service_invocations, 1U);
}

TEST_F(MwComSingleInstantiationStrategyFixture, StopFindWillBeCalledOnDestruction)
{
    // Expecting that StopFindService will be called
    EXPECT_CALL(proxy_mock, StopFindService(_)).Times(1);

    // Given a backend::mw_com::SingleInstantiationStrategy which has started Find()
    GivenAnMwComSingleInstantiationStrategy().WhichHasStartedFind();

    // When destroying the backend::mw_com::SingleInstantiationStrategy
    // Then StopFindService will only be called after destruction
    EXPECT_EQ(stop_find_service_invocations, 0U);
    strategy.reset();
    EXPECT_EQ(stop_find_service_invocations, 1U);
}

TEST_F(MwComSingleInstantiationStrategyFixture, UserCallbackNotInvokedWhenServiceIsSearchedButNotFound)
{
    // Expecting that the user callback that's registered with Find() is never called
    EXPECT_CALL(mock_user_callback, Call(_)).Times(0);

    // Given a backend::mw_com::SingleInstantiationStrategy
    GivenAnMwComSingleInstantiationStrategy();

    // When calling Find() but no service is found
    WhichHasStartedFind();
}

TEST_F(MwComSingleInstantiationStrategyFixture, UserCallbackIsInvokedWhenServiceIsSearchedAndFound)
{
    // Expecting that the user callback that's registered with Find() is called once
    EXPECT_CALL(mock_user_callback, Call(_)).Times(1);

    // Given that StartFindService is called and the callback is called with a handle of a found service
    ON_CALL(proxy_mock, StartFindService(_, kInstanceSpecifier))
        .WillByDefault(Invoke([](auto&& callback, auto&& /*instance_specifier*/) {
            callback({kDummyProxyHandle1}, kDummyFindServiceHandle);
            return kDummyFindServiceHandle;
        }));

    // and given a backend::mw_com::SingleInstantiationStrategy
    GivenAnMwComSingleInstantiationStrategy();

    // When calling Find()
    WhichHasStartedFind();
}

TEST_F(MwComSingleInstantiationStrategyFixture, StopFindServiceCalledWhenServiceIsSearchedAndFound)
{
    // Given that StartFindService is called and the callback is called with a handle of a found service
    ON_CALL(proxy_mock, StartFindService(_, kInstanceSpecifier))
        .WillByDefault(Invoke([](auto&& callback, auto&& /*instance_specifier*/) {
            callback({kDummyProxyHandle1}, kDummyFindServiceHandle);
            return kDummyFindServiceHandle;
        }));

    // and given a backend::mw_com::SingleInstantiationStrategy
    GivenAnMwComSingleInstantiationStrategy();

    // When calling Find()
    WhichHasStartedFind();

    // Then StopFindService will be called after the service instance is found
    EXPECT_EQ(stop_find_service_invocations, 1U);
}

TEST_F(MwComSingleInstantiationStrategyFixture, UserCallbackIsInvokedOnceWhenMultipleInstancesFound)
{
    // Expecting that the user callback that's registered with Find() is called once
    EXPECT_CALL(mock_user_callback, Call(_)).Times(1);

    // Given that StartFindService is called and the callback is called with multiple handles of a found service
    ON_CALL(proxy_mock, StartFindService(_, kInstanceSpecifier))
        .WillByDefault(Invoke([](auto&& callback, auto&& /*instance_specifier*/) {
            callback({kDummyProxyHandle1, kDummyProxyHandle2}, kDummyFindServiceHandle);
            return kDummyFindServiceHandle;
        }));

    // Given a backend::mw_com::SingleInstantiationStrategy
    GivenAnMwComSingleInstantiationStrategy();

    // When calling Find()
    WhichHasStartedFind();
}

TEST_F(MwComSingleInstantiationStrategyFixture,
       UserCallbackAndStopFindServiceAreNotInvokedWhenServiceFoundWithNoInstances)
{
    // Expecting that the user callback that's registered with Find() is never called
    EXPECT_CALL(mock_user_callback, Call(_)).Times(0);

    // Given that StartFindService is called and the callback is called with no handles of a found service
    ON_CALL(proxy_mock, StartFindService(_, kInstanceSpecifier))
        .WillByDefault(Invoke([](auto&& callback, auto&& /*instance_specifier*/) {
            callback({}, kDummyFindServiceHandle);
            return kDummyFindServiceHandle;
        }));

    // and given a backend::mw_com::SingleInstantiationStrategy
    GivenAnMwComSingleInstantiationStrategy();

    // When calling Find()
    WhichHasStartedFind();

    // Then StopFindService will not be invoked
    EXPECT_EQ(stop_find_service_invocations, 0U);
}

TEST_F(MwComSingleInstantiationStrategyFixture,
       UserCallbackAndStopFindServiceAreNotInvokedWhenServiceFoundButProxyCreationFails)
{
    // Expecting that the user callback that's registered with Find() is never called
    EXPECT_CALL(mock_user_callback, Call(_)).Times(0);

    // Given that StartFindService is called and the callback is called with a handle of a found service
    ON_CALL(proxy_mock, StartFindService(_, kInstanceSpecifier))
        .WillByDefault(Invoke([](auto&& callback, auto&& /*instance_specifier*/) {
            callback({kDummyProxyHandle1}, kDummyFindServiceHandle);
            return kDummyFindServiceHandle;
        }));

    // and given that an error will be returned when trying to create an score::mw::com::Proxy
    MwComProxy::InjectCreateError(MakeUnexpected(score::mw::com::ComErrc::kBindingFailure));

    // and given a backend::mw_com::SingleInstantiationStrategy
    GivenAnMwComSingleInstantiationStrategy();

    // When calling Find()
    WhichHasStartedFind();

    // Then StopFindService will not be invoked
    EXPECT_EQ(stop_find_service_invocations, 0U);
}

}  // namespace
}  // namespace score::mw::service
