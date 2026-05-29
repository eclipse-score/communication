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

#include "score/mw/com/test/fields/set_and_notifier/fields_test_resources/field_consumer.h"

#include "score/mw/com/test/fields/set_and_notifier/fields_test_resources/test_datatype.h"
#include "score/mw/com/types.h"

#include <score/optional.hpp>

#include <chrono>
#include <cstddef>
#include <future>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace score::mw::com::test
{
namespace
{

constexpr auto kMaxNumSamples{1U};

template <typename FieldType>
bool WaitForSubscription(FieldType& field, std::size_t retries, const std::chrono::milliseconds retry_backoff_time)
{
    while (field.GetSubscriptionState() != score::mw::com::impl::SubscriptionState::kSubscribed)
    {
        std::this_thread::sleep_for(retry_backoff_time);
        if (retries == 0)
        {
            return false;
        }
        --retries;
    }
    return true;
}

template <typename FieldType>
bool PollForValue(FieldType& field,
                  const std::int32_t expected_value,
                  std::size_t retries,
                  const std::chrono::milliseconds retry_backoff_time)
{
    while (retries > 0)
    {
        score::cpp::optional<std::int32_t> received_value;
        const auto samples_result = field.GetNewSamples(
            [&received_value](const auto& sample_ptr) noexcept {
                received_value = *sample_ptr;
            },
            kMaxNumSamples);

        if (samples_result.has_value() && received_value.has_value() && received_value.value() == expected_value)
        {
            return true;
        }

        std::this_thread::sleep_for(retry_backoff_time);
        --retries;
    }

    return false;
}

template <typename FieldType>
bool SetWithRetries(FieldType& field,
                    std::int32_t& requested_value,
                    std::int32_t& accepted_value,
                    std::size_t retries,
                    const std::chrono::milliseconds retry_backoff_time,
                    std::string& last_error)
{
    while (true)
    {
        const auto set_result = field.Set(requested_value);
        if (set_result.has_value())
        {
            accepted_value = *(set_result.value());
            return true;
        }

        std::ostringstream error_stream;
        error_stream << set_result.error();
        last_error = error_stream.str();

        if (retries == 0)
        {
            return false;
        }

        --retries;
        std::this_thread::sleep_for(retry_backoff_time);
    }
}

int run_notifier_client(const std::size_t num_retries, const std::chrono::milliseconds retry_backoff_time)
{
    auto instance_specifier_result = InstanceSpecifier::Create(std::string{kInstanceSpecifierString});
    if (!instance_specifier_result.has_value())
    {
        std::cerr << "Unable to create instance specifier, terminating\n";
        return -7;
    }
    auto instance_specifier = std::move(instance_specifier_result).value();

    std::promise<std::vector<InitialOnlyProxy::HandleType>> service_discovery_promise{};
    auto service_discovery_future = service_discovery_promise.get_future();
    auto handles_result = InitialOnlyProxy::StartFindService(
        [moved_service_discovery_promise = std::move(service_discovery_promise)](auto handles, auto handle) mutable {
            moved_service_discovery_promise.set_value(handles);
            score::cpp::ignore = InitialOnlyProxy::StopFindService(handle);
        },
        std::move(instance_specifier));
    if (!handles_result.has_value())
    {
        std::cerr << "Unable to get handles, terminating\n";
        return -1;
    }

    auto handles = service_discovery_future.get();
    if (handles.empty())
    {
        std::cerr << "Unable to find lola service, terminating\n";
        return -2;
    }

    auto proxy_result = InitialOnlyProxy::Create(handles[0]);
    if (!proxy_result.has_value())
    {
        std::cerr << "Unable to create InitialOnlyProxy: " << proxy_result.error() << "\n";
        return -3;
    }

    auto& proxy = proxy_result.value();
    std::ignore = proxy.test_field.Subscribe(kMaxNumSamples);
    if (!WaitForSubscription(proxy.test_field, num_retries, retry_backoff_time))
    {
        std::cerr << "Subscription failed in notifier scenario.\n";
        return -4;
    }

    const bool initial_value_received = PollForValue(proxy.test_field, kInitialValue, num_retries, retry_backoff_time);
    proxy.test_field.Unsubscribe();

    if (!initial_value_received)
    {
        std::cerr << "Did not receive expected initial value " << kInitialValue << " in notifier scenario.\n";
        return -5;
    }

    return 0;
}

int run_set_client(const std::size_t num_retries, const std::chrono::milliseconds retry_backoff_time)
{
    auto instance_specifier_result = InstanceSpecifier::Create(std::string{kInstanceSpecifierString});
    if (!instance_specifier_result.has_value())
    {
        std::cerr << "Unable to create instance specifier, terminating\n";
        return -27;
    }
    auto instance_specifier = std::move(instance_specifier_result).value();

    std::promise<std::vector<SetEnabledProxy::HandleType>> service_discovery_promise{};
    auto service_discovery_future = service_discovery_promise.get_future();
    auto handles_result = SetEnabledProxy::StartFindService(
        [moved_service_discovery_promise = std::move(service_discovery_promise)](auto handles, auto handle) mutable {
            moved_service_discovery_promise.set_value(handles);
            score::cpp::ignore = SetEnabledProxy::StopFindService(handle);
        },
        std::move(instance_specifier));
    if (!handles_result.has_value())
    {
        std::cerr << "Unable to get handles, terminating\n";
        return -21;
    }

    auto handles = service_discovery_future.get();
    if (handles.empty())
    {
        std::cerr << "Unable to find lola service, terminating\n";
        return -22;
    }

    auto proxy_result = SetEnabledProxy::Create(handles[0]);
    if (!proxy_result.has_value())
    {
        std::cerr << "Unable to create SetEnabledProxy: " << proxy_result.error() << "\n";
        return -13;
    }

    auto& proxy = proxy_result.value();
    std::ignore = proxy.test_field.Subscribe(kMaxNumSamples);
    if (!WaitForSubscription(proxy.test_field, num_retries, retry_backoff_time))
    {
        std::cerr << "Subscription failed in set scenario.\n";
        return -14;
    }

    const bool initial_value_received = PollForValue(proxy.test_field, kInitialValue, num_retries, retry_backoff_time);
    if (!initial_value_received)
    {
        std::cerr << "Did not receive initial value " << kInitialValue << " in set scenario.\n";
        return -18;
    }

    std::int32_t valid_requested_value = kSetValidValue;
    std::int32_t valid_accepted_value{0};
    std::string valid_set_error;
    if (!SetWithRetries(proxy.test_field,
                        valid_requested_value,
                        valid_accepted_value,
                        num_retries,
                        retry_backoff_time,
                        valid_set_error))
    {
        std::cerr << "Valid Set call failed: " << valid_set_error << "\n";
        return -19;
    }

    if (valid_accepted_value != kSetValidValue)
    {
        std::cerr << "Valid Set accepted value mismatch. Expected " << kSetValidValue << " but got "
                  << valid_accepted_value << "\n";
        return -20;
    }

    const bool valid_value_received = PollForValue(proxy.test_field, kSetValidValue, num_retries, retry_backoff_time);
    if (!valid_value_received)
    {
        std::cerr << "Did not receive valid set value " << kSetValidValue << " after Set call.\n";
        return -21;
    }

    std::int32_t requested_value = kSetRequestValue;
    std::int32_t accepted_value{0};
    std::string invalid_set_error;
    if (!SetWithRetries(
            proxy.test_field, requested_value, accepted_value, num_retries, retry_backoff_time, invalid_set_error))
    {
        std::cerr << "Set call failed: " << invalid_set_error << "\n";
        return -22;
    }

    if (accepted_value != kSetAcceptedValue)
    {
        std::cerr << "Set accepted value mismatch. Expected " << kSetAcceptedValue << " but got " << accepted_value
                  << "\n";
        return -23;
    }

    const bool clamped_value_received =
        PollForValue(proxy.test_field, kSetAcceptedValue, num_retries, retry_backoff_time);
    proxy.test_field.Unsubscribe();

    if (!clamped_value_received)
    {
        std::cerr << "Did not receive clamped value " << kSetAcceptedValue << " after Set call.\n";
        return -24;
    }

    return 0;
}

}  // namespace

int run_client(const std::size_t num_retries,
               const std::chrono::milliseconds retry_backoff_time,
               const std::string& mode)
{
    if (mode == "notifier")
    {
        return run_notifier_client(num_retries, retry_backoff_time);
    }
    if (mode == "set")
    {
        return run_set_client(num_retries, retry_backoff_time);
    }

    // TODO: Add "get" mode client scenario coverage once getter-enabled field variant is introduced.
    std::cerr << "Unsupported mode passed to client: " << mode << "\n";
    return -1;
}

}  // namespace score::mw::com::test
