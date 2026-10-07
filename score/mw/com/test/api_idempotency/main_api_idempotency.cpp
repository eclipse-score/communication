/********************************************************************************
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
 ********************************************************************************/
#include "score/mw/com/test/api_idempotency/api_idempotency_datatype.h"

#include "score/mw/com/com_error_domain.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/test/common_test_resources/assert_handler.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/service_instance_manifest_parser.h"
#include "score/mw/com/test/common_test_resources/skeleton_container.h"
#include "score/mw/com/types.h"
#include "score/result/result.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace score::mw::com::test
{
namespace
{

using namespace std::chrono_literals;

const std::string kFailureMessagePrefix{"api_idempotency"};

void CheckResult(const score::Result<void>& result, const std::string_view what)
{
    if (!result.has_value())
    {
        FailTest(kFailureMessagePrefix, " ", what, " failed: ", result.error());
    }
}

template <typename Predicate>
bool WaitUntil(Predicate predicate, const std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (predicate())
        {
            return true;
        }
        std::this_thread::sleep_for(10ms);
    }
    return predicate();
}

ServiceHandleContainer<HandleType> WaitForServiceAvailability(const InstanceSpecifier& instance_specifier,
                                                              const bool expected_available)
{
    ServiceHandleContainer<HandleType> service_handles;
    const bool success = WaitUntil(
        [&instance_specifier, expected_available, &service_handles]() {
            const auto find_result = ApiIdempotencyProxy::FindService(instance_specifier);
            if (!find_result.has_value())
            {
                return false;
            }
            const bool available = !find_result.value().empty();
            if (available != expected_available)
            {
                return false;
            }
            service_handles = find_result.value();
            return true;
        },
        5s);
    if (!success)
    {
        FailTest(kFailureMessagePrefix, " Timed out waiting for service availability to become ", expected_available);
    }
    return service_handles;
}

void ReceiveSamples(ApiIdempotencyProxy& proxy,
                    const std::vector<ApiIdempotencySample>& expected,
                    const std::size_t max_samples)
{
    std::vector<ApiIdempotencySample> received;
    const bool success = WaitUntil(
        [&]() {
            while (received.size() < expected.size())
            {
                const auto get_result = proxy.test_event.GetNewSamples(
                    [&received](SamplePtr<ApiIdempotencySample> sample) noexcept {
                        received.push_back(*sample);
                    },
                    max_samples);
                if (!get_result.has_value())
                {
                    FailTest(kFailureMessagePrefix, " GetNewSamples failed: ", get_result.error());
                    return false;
                }
                if (get_result.value() == 0U)
                {
                    return false;
                }
            }
            return true;
        },
        5s);
    if (!success)
    {
        FailTest(kFailureMessagePrefix, " Timed out waiting for samples");
    }
    if (received != expected)
    {
        FailTest(kFailureMessagePrefix, " Received samples do not match expected values");
    }
}

}  // namespace

void RunApiIdempotencyTest()
{
    const auto instance_specifier_result =
        InstanceSpecifier::Create(std::string{kApiIdempotencyInstanceSpecifierString});
    if (!instance_specifier_result.has_value())
    {
        FailTest(kFailureMessagePrefix, " Could not create instance specifier");
    }
    const auto instance_specifier = instance_specifier_result.value();

    SkeletonContainer<ApiIdempotencySkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(instance_specifier, kFailureMessagePrefix);
    auto& skeleton = skeleton_container.GetSkeleton();

    CheckResult(skeleton.OfferService(), "first OfferService");
    const auto offered_service_handles = WaitForServiceAvailability(instance_specifier, true);
    if (offered_service_handles.size() != 1U)
    {
        FailTest(kFailureMessagePrefix, " Expected the single configured service instance after first offer");
    }

    CheckResult(skeleton.OfferService(), "duplicate OfferService");
    if (WaitForServiceAvailability(instance_specifier, true) != offered_service_handles)
    {
        FailTest(kFailureMessagePrefix, " Duplicate OfferService changed the discovered service handles");
    }

    std::mutex discovery_mutex;
    std::vector<std::pair<FindServiceHandle, ServiceHandleContainer<HandleType>>> discovery_results;
    const auto handler = [&discovery_mutex, &discovery_results](ServiceHandleContainer<HandleType> handles,
                                                                FindServiceHandle find_service_handle) noexcept {
        std::lock_guard<std::mutex> lock{discovery_mutex};
        const auto existing = std::find_if(
            discovery_results.begin(), discovery_results.end(), [&find_service_handle](const auto& discovery) {
                return discovery.first == find_service_handle;
            });
        if (existing == discovery_results.end())
        {
            discovery_results.emplace_back(find_service_handle, std::move(handles));
        }
        else
        {
            existing->second = std::move(handles);
        }
    };

    std::vector<FindServiceHandle> find_service_handles;
    for (std::size_t i = 0U; i < 3U; ++i)
    {
        const auto start_find_result = ApiIdempotencyProxy::StartFindService(handler, instance_specifier);
        if (!start_find_result.has_value())
        {
            FailTest(kFailureMessagePrefix, " StartFindService failed: ", start_find_result.error());
        }
        find_service_handles.push_back(start_find_result.value());
    }

    const bool all_discoveries_match = WaitUntil(
        [&]() {
            std::lock_guard<std::mutex> lock{discovery_mutex};
            for (const auto& find_service_handle : find_service_handles)
            {
                const auto discovery = std::find_if(
                    discovery_results.begin(), discovery_results.end(), [&find_service_handle](const auto& result) {
                        return result.first == find_service_handle;
                    });
                if (discovery == discovery_results.end() || discovery->second != offered_service_handles)
                {
                    return false;
                }
            }
            return true;
        },
        5s);
    if (!all_discoveries_match)
    {
        FailTest(kFailureMessagePrefix, " Repeated StartFindService calls did not discover the unchanged service");
    }
    if (WaitForServiceAvailability(instance_specifier, true) != offered_service_handles)
    {
        FailTest(kFailureMessagePrefix, " Repeated StartFindService changed the discovered service handles");
    }

    for (const auto handle : find_service_handles)
    {
        CheckResult(ApiIdempotencyProxy::StopFindService(handle), "StopFindService");
    }

    {
        const auto find_result = ApiIdempotencyProxy::FindService(instance_specifier);
        if (!find_result.has_value() || find_result.value().empty())
        {
            FailTest(kFailureMessagePrefix, " Service was not discoverable after duplicate offers");
        }

        auto proxy_result = ApiIdempotencyProxy::Create(find_result.value().front());
        if (!proxy_result.has_value())
        {
            FailTest(kFailureMessagePrefix, " Proxy creation failed: ", proxy_result.error());
        }
        auto proxy = std::move(proxy_result.value());

        constexpr std::size_t kMaxSamples = 3U;

        CheckResult(proxy.test_event.Subscribe(kMaxSamples), "first Subscribe");
        if (!WaitUntil(
                [&proxy]() {
                    return proxy.test_event.GetSubscriptionState() == SubscriptionState::kSubscribed;
                },
                5s))
        {
            FailTest(kFailureMessagePrefix, " First subscription did not reach kSubscribed");
        }

        CheckResult(proxy.test_event.Subscribe(kMaxSamples), "duplicate Subscribe");
        if (proxy.test_event.GetSubscriptionState() != SubscriptionState::kSubscribed)
        {
            FailTest(kFailureMessagePrefix, " Duplicate Subscribe changed the subscribed state");
        }

        const std::vector<ApiIdempotencySample> first_expected_values{101, 102, 103};
        for (const auto value : first_expected_values)
        {
            CheckResult(skeleton.test_event.Send(value), "sending first event sample");
        }
        ReceiveSamples(proxy, first_expected_values, kMaxSamples);

        for (std::size_t i = 0U; i < 2U; ++i)
        {
            proxy.test_event.Unsubscribe();
            const auto get_after_unsubscribe_result =
                proxy.test_event.GetNewSamples([](SamplePtr<ApiIdempotencySample>) noexcept {}, 1U);
            if (get_after_unsubscribe_result.has_value() ||
                (get_after_unsubscribe_result.error() != ComErrc::kNotSubscribed))
            {
                FailTest(kFailureMessagePrefix, " Event was still subscribed after Unsubscribe");
            }
        }

        CheckResult(proxy.test_event.Subscribe(kMaxSamples), "re-subscribe");
        if (!WaitUntil(
                [&proxy]() {
                    return proxy.test_event.GetSubscriptionState() == SubscriptionState::kSubscribed;
                },
                5s))
        {
            FailTest(kFailureMessagePrefix, " Re-subscription did not reach kSubscribed");
        }

        const std::vector<ApiIdempotencySample> second_expected_values{201, 202, 203};
        for (const auto value : second_expected_values)
        {
            CheckResult(skeleton.test_event.Send(value), "sending second event sample");
        }
        ReceiveSamples(proxy, second_expected_values, kMaxSamples);

        proxy.test_event.Unsubscribe();
    }

    skeleton.StopOfferService();
    WaitForServiceAvailability(instance_specifier, false);
    skeleton.StopOfferService();
    WaitForServiceAvailability(instance_specifier, false);

    CheckResult(skeleton.OfferService(), "OfferService after duplicate stops");
    WaitForServiceAvailability(instance_specifier, true);
    skeleton.StopOfferService();
    WaitForServiceAvailability(instance_specifier, false);
}

}  // namespace score::mw::com::test

int main(int argc, const char** argv)
{
    const auto service_instance_manifest_path = score::mw::com::test::ParseServiceInstanceManifest(argc, argv);

    score::mw::com::test::SetupAssertHandler();
    score::mw::com::runtime::InitializeRuntime(
        score::mw::com::runtime::RuntimeConfiguration{service_instance_manifest_path});

    score::mw::com::test::RunApiIdempotencyTest();

    return EXIT_SUCCESS;
}
