/*******************************************************************************
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

#include "score/mw/com/test/common_test_resources/sctf_test_runner.h"
#include "score/mw/com/test/field_initial_value/test_datatype.h"
#include "score/mw/com/types.h"

#include <score/optional.hpp>

#include <chrono>
#include <cstddef>
#include <future>
#include <iostream>
#include <thread>

namespace score::mw::com::test
{

namespace
{

constexpr auto kMaxNumSamples{1U};

int run_client(const std::size_t num_retries, const std::chrono::milliseconds retry_backoff_time)
{
    using score::mw::com::test::TestDataProxy;

    auto instance_specifier_result = InstanceSpecifier::Create(std::string{kInstanceSpecifierString});
    if (!instance_specifier_result.has_value())
    {
        std::cerr << "Unable to create instance specifier, terminating\n";
        return -7;
    }
    auto instance_specifier = std::move(instance_specifier_result).value();

    std::promise<std::vector<TestDataProxy::HandleType>> service_discovery_promise{};
    auto service_discovery_future = service_discovery_promise.get_future();
    auto lola_proxy_handles_result = TestDataProxy::StartFindService(
        [moved_service_discovery_promise = std::move(service_discovery_promise)](auto handles, auto handle) mutable {
            moved_service_discovery_promise.set_value(handles);
            TestDataProxy::StopFindService(handle);
        },
        std::move(instance_specifier));

    if (!lola_proxy_handles_result.has_value())
    {
        std::cerr << "Unable to get handles, terminating\n";
        return -1;
    }

    auto lola_proxy_handles = service_discovery_future.get();
    if (lola_proxy_handles.empty())
    {
        std::cerr << "Unable to find lola service, terminating\n";
        return -2;
    }

    auto lola_proxy_result = TestDataProxy::Create(lola_proxy_handles[0]);
    if (!lola_proxy_result.has_value())
    {
        std::cerr << "Unable to create lola proxy, terminating\n";
        return -3;
    }
    auto& lola_proxy = lola_proxy_result.value();
    score::cpp::optional<std::int32_t> received_value;

    lola_proxy.test_field.Subscribe(kMaxNumSamples);
    std::size_t retries = num_retries;
    while (lola_proxy.test_field.GetSubscriptionState() != score::mw::com::impl::SubscriptionState::kSubscribed)
    {
        std::this_thread::sleep_for(retry_backoff_time);
        retries--;
        if (retries <= 0)
        {
            std::cerr << "Subscription failed!\n";
            return -4;
        }
    }
    lola_proxy.test_field.GetNewSamples(
        [&received_value](const auto& sample_ptr) noexcept {
            received_value = *sample_ptr;
        },
        kMaxNumSamples);

    lola_proxy.test_field.Unsubscribe();

    if (!received_value.has_value())
    {
        std::cerr << "Lola didn't receive a sample!\n";
        return -5;
    }

    if (received_value.value() != kTestValue)
    {
        std::cerr << "Expecting:" << kTestValue << " Received:" << received_value.value() << "!\n ";
        return -6;
    }

    return 0;
}

}  // namespace

}  // namespace score::mw::com::test

int main(int argc, const char** argv)
{
    using Parameters = score::mw::com::test::SctfTestRunner::RunParameters::Parameters;

    const std::vector<Parameters> allowed_parameters{Parameters::NUM_RETRIES, Parameters::RETRY_BACKOFF_TIME};
    score::mw::com::test::SctfTestRunner test_runner(argc, argv, allowed_parameters);
    const auto& run_parameters = test_runner.GetRunParameters();
    const auto num_retries = run_parameters.GetNumRetries();
    const auto retry_backoff_time = run_parameters.GetRetryBackoffTime();
    const auto stop_token = test_runner.GetStopToken();

    return score::mw::com::test::run_client(num_retries, retry_backoff_time);
}
