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

#include "score/concurrency/notification.h"

#include "score/mw/service/backend/mw_com/proxy_holder.h"
#include "score/mw/service/backend/mw_com/proxy_stub.h"
#include "score/mw/com/com_error_domain.h"
#include "score/mw/com/types.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <optional>
#include <thread>
#include <utility>

namespace score::mw::service::backend::mw_com::internal
{
namespace
{

using ::testing::_;
using ::testing::Exactly;
using ::testing::Invoke;
using ::testing::Return;

using MwComProxy = mw::service::backend::mw_com::FakeProxy;

const auto kInstanceSpecifier = score::mw::com::InstanceSpecifier::Create("SOME/IDENTIFIER").value();
const auto kDummyFindServiceHandle = MwComProxy::CreateFindServiceHandle(10U);
const auto kDummyFindServiceHandle2 = MwComProxy::CreateFindServiceHandle(20U);
const auto kDummyFindServiceHandle3 = MwComProxy::CreateFindServiceHandle(30U);

constexpr typename MwComProxy::HandleType kProxyHandle1{1};
constexpr typename MwComProxy::HandleType kProxyHandle2{11};
constexpr typename MwComProxy::HandleType kProxyHandle3{21};
constexpr typename MwComProxy::HandleType kProxyHandle4{31};
constexpr typename MwComProxy::HandleType kProxyHandle5{41};
constexpr typename MwComProxy::HandleType kProxyHandle6{51};

class ProxyHolderFixture : public ::testing::Test
{
  public:
    void SetUp() override
    {
        ON_CALL(MwComProxy::GetMockInstance(), StartFindService(_, _))
            .WillByDefault(Invoke([](auto&& /*callback*/, auto&& /*instance_specifier*/) {
                return kDummyFindServiceHandle;
            }));

        ON_CALL(MwComProxy::GetMockInstance(), StopFindService(_)).WillByDefault(Invoke([this] {
            stop_find_service_invocations++;
            return score::Result<void>{};
        }));
    }

    ProxyHolderFixture& GivenAnMwComProxyHolder()
    {
        std::string instance_specifier_string{kInstanceSpecifier.ToString()};
        proxy_holder = ProxyHolder<MwComProxy>::CreateFor(std::move(instance_specifier_string));
        return *this;
    }

    backend::mw_com::FakeProxyMockGuard<MwComProxy> fake_proxy_mock_guard{};
    std::shared_ptr<ProxyHolder<MwComProxy>> proxy_holder{nullptr};

    std::uint32_t stop_find_service_invocations{0U};
};

TEST_F(ProxyHolderFixture, StartFindServiceDispatchesToMwComStartFindService)
{
    // Expecting that the actual proxy's method StartFindService() will get invoked exactly once
    EXPECT_CALL(MwComProxy::GetMockInstance(), StartFindService(_, kInstanceSpecifier)).Times(1);

    // Given a ProxyHolder instance
    GivenAnMwComProxyHolder();

    // When calling StartFindService()
    proxy_holder->StartFindService();
}

TEST_F(ProxyHolderFixture, StopFindServiceDispatchesToMwComStartFindServiceWhenServiceSearchWasStarted)
{
    // Expecting that the actual proxy's method StopFindService() will get invoked exactly once with the find service
    // handle returned by StartFindService
    EXPECT_CALL(MwComProxy::GetMockInstance(), StopFindService(kDummyFindServiceHandle)).Times(1);

    // Given a ProxyHolder instance where the service search has been started
    GivenAnMwComProxyHolder();
    proxy_holder->StartFindService();

    // When calling StopFindService()
    proxy_holder->StopFindService();
}

TEST_F(ProxyHolderFixture, StopFindServiceIsCalledOnDestructionWhenServiceWasOffered)
{
    // Expecting that the actual proxy's method StopFindService() will get invoked exactly once with the find service
    // handle returned by StartFindService
    EXPECT_CALL(MwComProxy::GetMockInstance(), StopFindService(kDummyFindServiceHandle)).Times(1);

    // Given a ProxyHolder instance where the service search has been started
    GivenAnMwComProxyHolder();
    proxy_holder->StartFindService();

    // When destroying the ProxyHolder instance
    proxy_holder.reset();
}

TEST_F(ProxyHolderFixture, StopFindServiceIsNotCalledOnDestructionIfServiceWasNotOffered)
{
    // Expecting that the actual proxy's method StopFindService() will never get invoked
    EXPECT_CALL(MwComProxy::GetMockInstance(), StopFindService(_)).Times(Exactly(0));

    // Given a ProxyHolder instance where the service search was never started
    GivenAnMwComProxyHolder();

    // When destroying the ProxyHolder instance
    proxy_holder.reset();
}

TEST_F(ProxyHolderFixture, StartFindServiceFindsMultipleProxies)
{
    // Expecting that the actual proxy's method StartFindService() will get invoked exactly once and the callback will
    // be called with 3 proxy handles (but only 2 unique)
    EXPECT_CALL(MwComProxy::GetMockInstance(), StartFindService(_, kInstanceSpecifier))
        .WillOnce(Invoke([](auto&& callback, auto&& /*instance_specifier*/) {
            callback({kProxyHandle1, kProxyHandle2, kProxyHandle1}, kDummyFindServiceHandle);
            return kDummyFindServiceHandle;
        }));

    GivenAnMwComProxyHolder();

    // When calling StartFindService
    proxy_holder->StartFindService();

    // Then ProxyHolder should contain exactly two proxies
    const auto found_proxies = proxy_holder->ExtractProxies();
    EXPECT_EQ(found_proxies.size(), 2U);
}

TEST_F(ProxyHolderFixture, ProxyHolderContainsNullptrWhenProxyCreationFails)
{
    // Expecting that the actual proxy's method StartFindService() will get invoked exactly once and the callback will
    // be called with 1 proxy handle
    EXPECT_CALL(MwComProxy::GetMockInstance(), StartFindService(_, kInstanceSpecifier))
        .WillOnce(Invoke([](auto&& callback, auto&& /*instance_specifier*/) {
            callback({kProxyHandle1}, kDummyFindServiceHandle);
            return kDummyFindServiceHandle;
        }));

    GivenAnMwComProxyHolder();

    // Given that an error will be returned when trying to create an score::mw::com::Proxy
    MwComProxy::InjectCreateError(MakeUnexpected(score::mw::com::ComErrc::kBindingFailure));

    // When calling StartFindService
    proxy_holder->StartFindService();

    // Then ProxyHolder should contain a single instance which is a nullptr
    const auto found_proxies = proxy_holder->ExtractProxies();
    ASSERT_EQ(found_proxies.size(), 1U);
    EXPECT_EQ(found_proxies.front(), nullptr);
}

TEST_F(ProxyHolderFixture, OnFoundCallbackCalledWithContainerContainingNullptrWhenProxyCreationFails)
{
    bool callback_got_invoked{false};
    typename ProxyHolder<MwComProxy>::OnServiceFoundCallback on_found_callback =
        [&callback_got_invoked](ProxyHolder<MwComProxy>& mw_com_proxy_holder) {
            callback_got_invoked = true;
            // Expecting that the callback will be called with a proxy holder containing a single instance which is a
            // nullptr.
            const auto found_proxies = mw_com_proxy_holder.ExtractProxies();
            ASSERT_EQ(found_proxies.size(), 1U);
            EXPECT_EQ(found_proxies.front(), nullptr);
        };

    // and expecting that the actual proxy's method StartFindService() will get invoked exactly once and the callback
    // will be called with 1 proxy handle
    EXPECT_CALL(MwComProxy::GetMockInstance(), StartFindService(_, kInstanceSpecifier))
        .WillOnce(Invoke([](auto&& callback, auto&& /*instance_specifier*/) {
            callback({kProxyHandle1}, kDummyFindServiceHandle);
            return kDummyFindServiceHandle;
        }));

    GivenAnMwComProxyHolder();

    // Given that an error will be returned when trying to create an score::mw::com::Proxy
    MwComProxy::InjectCreateError(MakeUnexpected(score::mw::com::ComErrc::kBindingFailure));

    // When calling StartFindService
    proxy_holder->StartFindService(std::move(on_found_callback));

    // Then the callback should have been invoked.
    EXPECT_TRUE(callback_got_invoked);
}

TEST_F(ProxyHolderFixture, StartFindServiceGetsInvokedDuringStartFindService)
{
    ::testing::InSequence in_sequence{};

    // Given an OnServiceFoundCallback which calls StartFindService
    typename ProxyHolder<MwComProxy>::OnServiceFoundCallback on_found_callback =
        [](ProxyHolder<MwComProxy>& mw_com_proxy_holder) {
            mw_com_proxy_holder.StartFindService();
        };

    // Expecting that upon invocation of the MwComProxy's method StartFindService(),
    // ProxyHolder::StartFindService() shall get invoked once more by the FindServiceHandler (-> recursive
    // invocation)
    EXPECT_CALL(MwComProxy::GetMockInstance(), StartFindService(_, kInstanceSpecifier))
        .WillOnce(Invoke([](auto&& callback, auto&& /*instance_specifier*/) {
            callback({kProxyHandle1}, kDummyFindServiceHandle);
            return kDummyFindServiceHandle;
        }));

    // and expecting that StopFindService will be called by the invocation of ProxyHolder::StartFindService() in the
    // FindServiceHandler
    EXPECT_CALL(MwComProxy::GetMockInstance(), StopFindService(kDummyFindServiceHandle));

    // and expecting that StartFindService will be called by the invocation of ProxyHolder::StartFindService() in
    // the FindServiceHandler
    EXPECT_CALL(MwComProxy::GetMockInstance(), StartFindService(_, kInstanceSpecifier))
        .WillOnce(Return(kDummyFindServiceHandle2));

    // and expecting that StopFindService will be called again with the FindServiceHandle from the second
    // StartFindService call on destruction of ProxyHolder
    EXPECT_CALL(MwComProxy::GetMockInstance(), StopFindService(kDummyFindServiceHandle2));

    GivenAnMwComProxyHolder();

    // When calling StartFindService()
    proxy_holder->StartFindService(std::move(on_found_callback));

    // Then StopFindService should be called once due to the invocation of ProxyHolder::StartFindService() in the
    // FindServiceHandler
    EXPECT_EQ(stop_find_service_invocations, 1U);
}

TEST_F(ProxyHolderFixture, StopFindServiceGetsInvokedDuringStartFindService)
{
    // Given an OnServiceFoundCallback which calls StopFindService and counts invocations
    std::size_t num_user_callback_invocations{0};
    typename ProxyHolder<MwComProxy>::OnServiceFoundCallback on_found_callback =
        [&num_user_callback_invocations](ProxyHolder<MwComProxy>& mw_com_proxy_holder) noexcept {
            ++num_user_callback_invocations;
            mw_com_proxy_holder.StopFindService();
        };

    // Expecting that the actual proxy's method StartFindService() will get invoked once which will invoke the callback
    EXPECT_CALL(MwComProxy::GetMockInstance(), StartFindService(_, kInstanceSpecifier))
        .WillOnce(Invoke([](auto&& callback, auto&& /*instance_specifier*/) {
            callback({kProxyHandle1}, kDummyFindServiceHandle);
            return kDummyFindServiceHandle;
        }));

    // and expecting that StopFindService will be called twice: once in the callback of the FindServiceHandler and again
    // on destruction of ProxyHolder
    EXPECT_CALL(MwComProxy::GetMockInstance(), StopFindService(kDummyFindServiceHandle)).Times(2);

    GivenAnMwComProxyHolder();

    // When calling StartFindService() together with providing a callback in which we explicitly call StopFindService()
    proxy_holder->StartFindService(std::move(on_found_callback));

    // Then the actual proxy's methods StopFindService() was invoked once
    EXPECT_EQ(stop_find_service_invocations, 1U);

    // And the user callback must have gotten invoked as well
    EXPECT_EQ(num_user_callback_invocations, 1U);
}

TEST_F(ProxyHolderFixture, StartFindServiceInvokesUserProvidedCallbackAsynchronously)
{
    bool start_find_service_got_invoked{false};

    // Given an OnServiceFoundCallback which calls StopFindService and counts invocations
    std::size_t num_user_callback_invocations{0};
    typename ProxyHolder<MwComProxy>::OnServiceFoundCallback on_found_callback =
        [&num_user_callback_invocations](ProxyHolder<MwComProxy>& mw_com_proxy_holder) noexcept {
            ++num_user_callback_invocations;
            mw_com_proxy_holder.StopFindService();
        };

    // Expecting that the actual proxy's method StartFindService() will get invoked exactly once which will invoke the
    // callback asynchronously
    std::future<void> service_found_callback_is_running_future{};
    EXPECT_CALL(MwComProxy::GetMockInstance(), StartFindService(_, kInstanceSpecifier))
        .Times(1)
        .WillOnce(Invoke([&start_find_service_got_invoked, &service_found_callback_is_running_future](
                             auto&& callback, auto&& /*instance_specifier*/) {
            service_found_callback_is_running_future =
                std::async(std::launch::async, [callback = std::move(callback)]() {
                    callback({kProxyHandle1}, kDummyFindServiceHandle);
                });
            start_find_service_got_invoked = true;
            return kDummyFindServiceHandle;
        }));

    // Expecting that the actual proxy's method StopFindService() will get invoked exactly once when StopFindService is
    // called asynchronously (this call will block on the mutex until StartFindService finishes, so the
    // find_service_handle_ will be cleared by the ProxyHolder::StopFindService call and therefore StopFindService
    // will not be called on destruction of ProxyHolder).
    EXPECT_CALL(MwComProxy::GetMockInstance(), StopFindService(kDummyFindServiceHandle));

    GivenAnMwComProxyHolder();

    // When calling StartFindService() together with providing a callback in which we explicitly call StopFindService()
    proxy_holder->StartFindService(std::move(on_found_callback));

    // Then the actual proxy's method StartFindService() must have gotten invoked
    ASSERT_TRUE(service_found_callback_is_running_future.valid());
    EXPECT_TRUE(start_find_service_got_invoked);

    // When waiting for the invocation of the on service found callback to finish
    service_found_callback_is_running_future.wait();

    // Then the actual proxy's method StopFindService() must have gotten invoked
    EXPECT_EQ(stop_find_service_invocations, 1U);

    // And the user callback must have gotten invoked as well
    EXPECT_EQ(num_user_callback_invocations, 1U);
}

TEST_F(ProxyHolderFixture, StartFindServiceFindsProxiesWhileUserProvidedCallbackIsRunning)
{
    // Expecting that the actual proxy's method StartFindService() will get invoked once which saves the callback
    score::mw::com::FindServiceHandler<typename MwComProxy::HandleType> mw_com_service_found_callback{};
    bool was_callback_set{false};
    EXPECT_CALL(MwComProxy::GetMockInstance(), StartFindService(_, kInstanceSpecifier))
        .WillOnce(
            Invoke([&mw_com_service_found_callback, &was_callback_set](auto&& callback, auto&& /*instance_specifier*/) {
                mw_com_service_found_callback = std::move(callback);
                was_callback_set = true;
                return kDummyFindServiceHandle;
            }));

    // and expecting that the actual proxy's method StopFindService() will get invoked exactly once on destruction of
    // ProxyHolder
    EXPECT_CALL(MwComProxy::GetMockInstance(), StopFindService(kDummyFindServiceHandle));

    // Given an OnServiceFoundCallback which calls the score::mw::com::FindServiceHandler 3 times which the same handles
    std::size_t num_user_callback_invocations{0};
    typename ProxyHolder<MwComProxy>::OnServiceFoundCallback on_found_callback =
        [&num_user_callback_invocations, &was_callback_set, &mw_com_service_found_callback](
            ProxyHolder<MwComProxy>&) noexcept {
            if (++num_user_callback_invocations >= 3U)
            {
                return;  // to prevent endless recursion
            }
            // Then the actual proxy's method StartFindService() must have gotten invoked
            ASSERT_TRUE(was_callback_set);
            mw_com_service_found_callback({kProxyHandle1, kProxyHandle2, kProxyHandle3}, kDummyFindServiceHandle);
        };

    GivenAnMwComProxyHolder();

    // When calling StartFindService() together with providing a callback in which we simulate that further proxies got
    // found while such callback is currently running
    proxy_holder->StartFindService(std::move(on_found_callback));

    // When invoking the ara service found callback now
    mw_com_service_found_callback({kProxyHandle4, kProxyHandle5, kProxyHandle6}, kDummyFindServiceHandle);

    // Then exactly 6 proxies must have gotten found
    EXPECT_EQ(proxy_holder->ExtractProxies().size(), 6U);
}

TEST_F(ProxyHolderFixture, StartFindServiceGetsInvokedWithinUserProvidedCallbackSynchronously)
{
    // Given an OnServiceFoundCallback which will recursively call StartFindService 3 times
    std::uint16_t num_user_callback_invocations{0};
    typename ProxyHolder<MwComProxy>::OnServiceFoundCallback nested_on_found_callback =
        [&num_user_callback_invocations](auto& proxy_holder_outer) noexcept {
            EXPECT_EQ(proxy_holder_outer.ExtractProxies().size(), 1U);
            ++num_user_callback_invocations;
            proxy_holder_outer.StartFindService([&num_user_callback_invocations](auto& proxy_holder_middle) noexcept {
                EXPECT_EQ(proxy_holder_middle.ExtractProxies().size(), 1U);
                ++num_user_callback_invocations;
                proxy_holder_middle.StartFindService(
                    [&num_user_callback_invocations](auto& proxy_holder_inner) noexcept {
                        EXPECT_EQ(proxy_holder_inner.ExtractProxies().size(), 1U);
                        ++num_user_callback_invocations;
                    });
            });
        };

    // Expecting that a sequence of StartFindService()/StopFindService() invocations takes place with each
    // StartFindService calling the callback with a unique handle and returning a unique FindServiceHandle. Each
    // StopFindService is expected to be called with a unique FindServiceHandle
    ::testing::InSequence in_sequence{};

    std::atomic<std::uint16_t> num_start_find_service_invocations{0};
    EXPECT_CALL(MwComProxy::GetMockInstance(), StartFindService(_, kInstanceSpecifier))
        .WillOnce(Invoke([&num_start_find_service_invocations](auto&& callback, auto&& /*instance_specifier*/) {
            ++num_start_find_service_invocations;
            callback({kProxyHandle1}, kDummyFindServiceHandle);
            return kDummyFindServiceHandle;
        }));
    EXPECT_CALL(MwComProxy::GetMockInstance(), StopFindService(kDummyFindServiceHandle)).Times(1);

    EXPECT_CALL(MwComProxy::GetMockInstance(), StartFindService(_, kInstanceSpecifier))
        .WillOnce(Invoke([&num_start_find_service_invocations](auto&& callback, auto&& /*instance_specifier*/) {
            ++num_start_find_service_invocations;
            callback({kProxyHandle2}, kDummyFindServiceHandle2);
            return kDummyFindServiceHandle2;
        }));
    EXPECT_CALL(MwComProxy::GetMockInstance(), StopFindService(kDummyFindServiceHandle2)).Times(1);

    EXPECT_CALL(MwComProxy::GetMockInstance(), StartFindService(_, kInstanceSpecifier))
        .WillOnce(Invoke([&num_start_find_service_invocations](auto&& callback, auto&& /*instance_specifier*/) {
            ++num_start_find_service_invocations;
            callback({kProxyHandle3}, kDummyFindServiceHandle3);
            return kDummyFindServiceHandle3;
        }));
    EXPECT_CALL(MwComProxy::GetMockInstance(), StopFindService(kDummyFindServiceHandle3)).Times(1);

    GivenAnMwComProxyHolder();

    // When calling StartFindService() together with a callback in which we call StartFindService() recursively
    proxy_holder->StartFindService(std::move(nested_on_found_callback));

    // Then the actual proxy's methods StartFindService() will be called 3 times (once for the original StartFindService
    // invocation and again for each recursive call in the callback)
    EXPECT_EQ(num_start_find_service_invocations.load(std::memory_order_relaxed), 3U);

    // And StopFindService will be called before each call to StartFindService within the callback
    EXPECT_EQ(stop_find_service_invocations, 2U);

    // And the user callback must have gotten invoked three times
    EXPECT_EQ(num_user_callback_invocations, 3U);
}

TEST_F(ProxyHolderFixture, StartFindServiceGetsInvokedWithinUserProvidedCallbackAsynchronously)
{
    using OnServiceFoundCallback = typename ProxyHolder<MwComProxy>::OnServiceFoundCallback;

    // Given an OnServiceFoundCallback which will recursively call StartFindService 3 times
    std::uint16_t num_user_callback_invocations{0};
    score::concurrency::Notification innermost_callback_got_invoked;
    OnServiceFoundCallback nested_on_found_callback =
        [&num_user_callback_invocations, &innermost_callback_got_invoked](auto& proxy_holder_outer) noexcept {
            EXPECT_EQ(proxy_holder_outer.ExtractProxies().size(), 1U);
            ++num_user_callback_invocations;
            proxy_holder_outer.StartFindService([&](auto& proxy_holder_middle) noexcept {
                EXPECT_EQ(proxy_holder_middle.ExtractProxies().size(), 1U);
                ++num_user_callback_invocations;
                proxy_holder_middle.StartFindService([&](auto& proxy_holder_inner) noexcept {
                    EXPECT_EQ(proxy_holder_inner.ExtractProxies().size(), 1U);
                    ++num_user_callback_invocations;
                    innermost_callback_got_invoked.notify();
                });
            });
        };

    // Expecting that a sequence of StartFindService()/StopFindService() invocations takes place
    ::testing::InSequence in_sequence{};

    std::atomic<std::uint16_t> num_start_find_service_invocations{0};
    std::future<void> service_found_callback_is_running_future1{};
    EXPECT_CALL(MwComProxy::GetMockInstance(), StartFindService(_, kInstanceSpecifier))
        .WillOnce(Invoke([&](auto&& callback, auto&& /*instance_specifier*/) {
            service_found_callback_is_running_future1 =
                std::async(std::launch::async, [&, on_found_callback = std::move(callback)]() {
                    num_start_find_service_invocations.fetch_add(1U, std::memory_order_relaxed);
                    on_found_callback({kProxyHandle1}, kDummyFindServiceHandle);
                });
            return kDummyFindServiceHandle;
        }));
    EXPECT_CALL(MwComProxy::GetMockInstance(), StopFindService(kDummyFindServiceHandle)).Times(1);

    std::future<void> service_found_callback_is_running_future2{};
    EXPECT_CALL(MwComProxy::GetMockInstance(), StartFindService(_, kInstanceSpecifier))
        .WillOnce(Invoke([&](auto&& callback, auto&& /*instance_specifier*/) {
            service_found_callback_is_running_future2 =
                std::async(std::launch::async, [&, on_found_callback = std::move(callback)]() {
                    num_start_find_service_invocations.fetch_add(1U, std::memory_order_relaxed);
                    on_found_callback({kProxyHandle2}, kDummyFindServiceHandle2);
                });
            return kDummyFindServiceHandle2;
        }));
    EXPECT_CALL(MwComProxy::GetMockInstance(), StopFindService(kDummyFindServiceHandle2)).Times(1);

    std::future<void> service_found_callback_is_running_future3{};
    EXPECT_CALL(MwComProxy::GetMockInstance(), StartFindService(_, kInstanceSpecifier))
        .WillOnce(Invoke([&](auto&& callback, auto&& /*instance_specifier*/) {
            service_found_callback_is_running_future3 =
                std::async(std::launch::async, [&, on_found_callback = std::move(callback)]() {
                    num_start_find_service_invocations.fetch_add(1U, std::memory_order_relaxed);
                    on_found_callback({kProxyHandle3}, kDummyFindServiceHandle3);
                });
            return kDummyFindServiceHandle3;
        }));
    EXPECT_CALL(MwComProxy::GetMockInstance(), StopFindService(kDummyFindServiceHandle3)).Times(1);

    GivenAnMwComProxyHolder();

    // When calling StartFindService() together with a callback in which we call StartFindService() recursively
    proxy_holder->StartFindService(std::move(nested_on_found_callback));

    // Then, after some time, the innermost user callback must have gotten invoked
    innermost_callback_got_invoked.waitForWithAbort(std::chrono::seconds{3}, {});
    service_found_callback_is_running_future3.wait();
    service_found_callback_is_running_future2.wait();
    service_found_callback_is_running_future1.wait();

    // Then the actual proxy's methods StartFindService() will be called 3 times (once for the original StartFindService
    // invocation and again for each recursive call in the callback)
    EXPECT_EQ(num_start_find_service_invocations.load(std::memory_order_relaxed), 3U);

    // And StopFindService will be called before each call to StartFindService within the callback
    EXPECT_EQ(stop_find_service_invocations, 2U);

    // And the user callback must have gotten invoked three times
    EXPECT_EQ(num_user_callback_invocations, 3U);
}

TEST_F(ProxyHolderFixture, FindAndExtractMultiThreaded)
{
    constexpr auto kNumberOfProxiesToFind{500U};

    std::future<void> producer{};
    score::concurrency::Notification producer_is_ready{};
    score::concurrency::Notification consumer_is_ready{};
    score::concurrency::Notification perform_actual_test{};

    // Expecting that the actual proxy's method StartFindService() will get invoked and continuously finds proxies
    EXPECT_CALL(MwComProxy::GetMockInstance(), StartFindService(_, kInstanceSpecifier))
        .Times(1)
        .WillOnce(Invoke([&](auto&& callback, auto&& /*instance_specifier*/) {
            producer = std::async(std::launch::async, [&, callback = std::move(callback)]() {
                score::mw::com::ServiceHandleContainer<typename MwComProxy::HandleType> proxies{};
                producer_is_ready.notify();
                perform_actual_test.waitWithAbort({});
                for (std::uint16_t counter{0}; counter < kNumberOfProxiesToFind; ++counter)
                {
                    proxies.emplace_back(typename MwComProxy::HandleType{counter});
                    callback(proxies, kDummyFindServiceHandle);
                }
            });

            return kDummyFindServiceHandle;
        }));

    GivenAnMwComProxyHolder();

    proxy_holder->StartFindService();

    // When extracting the found proxies in parallel to above logic where proxies are getting found continuously
    std::size_t number_of_found_proxies{};
    std::future<void> consumer = std::async(std::launch::async, [&]() noexcept {
        consumer_is_ready.notify();
        perform_actual_test.waitWithAbort({});
        while (number_of_found_proxies != kNumberOfProxiesToFind)
        {
            const auto found_proxies = proxy_holder->ExtractProxies();
            number_of_found_proxies += found_proxies.size();
        }
    });

    producer_is_ready.waitWithAbort({});
    consumer_is_ready.waitWithAbort({});
    perform_actual_test.notify();

    producer.wait();
    consumer.wait();

    // Then the expected number of proxies must have gotten found
    EXPECT_EQ(number_of_found_proxies, kNumberOfProxiesToFind);
}

}  // namespace
}  // namespace score::mw::service::backend::mw_com::internal
