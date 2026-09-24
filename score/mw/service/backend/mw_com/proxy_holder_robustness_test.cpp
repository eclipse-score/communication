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

#include "score/mw/service/backend/mw_com/proxy_stub.h"
#include "score/mw/service/backend/mw_com/single_instantiation_strategy.h"
#include "score/mw/service/details/proxy_spec_traits.h"
#include "score/mw/service/proxy_data.h"
#include "score/mw/service/proxy_needs.h"
#include "score/mw/service/proxy_needs_factory.h"
#include "score/language/safecpp/scoped_function/move_only_scoped_function.h"

#include "score/concurrency/future/interruptible_promise.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>
#include <score/utility.hpp>

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace score::mw::service::backend::common::test
{
namespace
{

using namespace ::testing;

using MyProxy = mw_com::FakeProxy;

const auto kDummyFindServiceHandle = MyProxy::CreateFindServiceHandle(10U);
constexpr typename MyProxy::HandleType kProxyHandle1{1};

class Receiver
{
  public:
    Receiver() = default;
    virtual ~Receiver() = default;
};

class ReceiverImpl : public Receiver
{
  public:
    ReceiverImpl(std::unique_ptr<MyProxy> my_proxy) : my_proxy_{std::move(my_proxy)} {}

  private:
    std::unique_ptr<MyProxy> my_proxy_;
};

using SingleProxySpec = mw::service::ProxyNeeds<mw::service::Optional<Receiver>>;

class ProxyHolderRobustnessFixture : public ::testing::Test
{
  public:
    ProxyHolderRobustnessFixture()
    {
        ON_CALL(MyProxy::GetMockInstance(), StartFindService(_, _))
            .WillByDefault(Invoke([this](auto&& callback, auto&& /*instance_specifier*/) {
                find_service_handler = std::move(callback);
                return kDummyFindServiceHandle;
            }));
    }

    SingleProxySpec CreateSingleProxySpec(std::string instance_specifier)
    {
        using SingleProxyStrategy =
            mw::service::backend::mw_com::SingleInstantiationStrategy<Receiver, ReceiverImpl, MyProxy>;

        auto strategy = std::make_unique<SingleProxyStrategy>(std::move(instance_specifier));
        return mw::service::ProxyNeedsFactory<SingleProxySpec>::Create(std::move(strategy));
    }

    OptionalProxyData<Receiver> GetOptionalProxyData()
    {
        auto single_proxy_spec = CreateSingleProxySpec("dummy");
        auto requested_proxies = single_proxy_spec.InitiateServiceDiscovery(stop_source.get_token());
        auto proxy_container = requested_proxies.GetProxyContainer();

        return proxy_container.template Extract<mw::service::Optional<Receiver>>();
    }

    backend::mw_com::FakeProxyMockGuard<MyProxy> fake_proxy_mock_guard{};
    score::cpp::stop_source stop_source{};
    std::optional<score::mw::com::FindServiceHandler<MyProxy::HandleType>> find_service_handler{};
};

TEST_F(ProxyHolderRobustnessFixture, ProxyHolderIsNotDestroyedWhenFindServiceHandlerDestroysProxyFuture)
{
    // Given an OptionalProxyData wrapping a SingleInstanceHolder future together with its service discovery
    auto optional_proxy_data = GetOptionalProxyData();

    // and given a promise which is created by a user of mw::service whose value is set when the ProxyFuture is
    // fulfilled via a continuation. The ownership of the OptionalProxyData is moved to the future connected to the new
    // promise so that the lifetime of service discovery can be linked to the new future that will be used in
    // application code (i.e. to avoid that service discovery is stopped as soon as the ProxyFuture is destroyed).
    ::score::concurrency::InterruptiblePromise<void> promise{};
    auto future = promise.GetInterruptibleFuture().value();
    score::safecpp::Scope<> scope{};

    score::cpp::ignore = optional_proxy_data.GetProxyFuture().Then(
        score::safecpp::MoveOnlyScopedFunction<void(score::Result<std::unique_ptr<Receiver>>&)>{
            scope, [promise = std::move(promise)](auto&) mutable {
                score::cpp::ignore = promise.SetValue();
            }});
    future.Then(score::safecpp::MoveOnlyScopedFunction<void(score::Result<void>&)>{
        stop_source.get_token(), [optional_proxy_data = std::move(optional_proxy_data)](auto&) {}});

    // When calling the FindServiceHandler (mimicking the behaviour when a proxy instance is found by mw::com's Service
    // Discovery)
    std::invoke(find_service_handler.value(),
                score::mw::com::ServiceHandleContainer<MyProxy::HandleType>{kProxyHandle1},
                kDummyFindServiceHandle);

    // Then we should not crash (this test should be verified with sanitizer configs as well as --config=memcheck!)
}

}  // namespace
}  // namespace score::mw::service::backend::common::test
