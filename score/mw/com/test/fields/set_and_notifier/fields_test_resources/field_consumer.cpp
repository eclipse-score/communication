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

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/fields/set_and_notifier/fields_test_resources/test_constants.h"
#include "score/mw/com/test/fields/set_and_notifier/fields_test_resources/test_datatype.h"
#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"
#include "score/mw/com/types.h"

#include <score/optional.hpp>

#include <chrono>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

namespace score::mw::com::test
{
namespace
{

constexpr auto kMaxNumSamples{1U};
const std::string kInterprocessNotificationShmPath{"/fields_set_and_notifier_interprocess_notification"};

template <typename... Args>
void NotifyAndFail(ProcessSynchronizer& process_synchronizer, Args&&... args)
{
    process_synchronizer.Notify();
    FailTest(std::forward<Args>(args)...);
}

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

void run_notifier_consumer(const std::size_t num_retries, const std::chrono::milliseconds retry_backoff_time)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!process_synchronizer_result.has_value())
    {
        FailTest("Consumer: Could not create ProcessSynchronizer");
    }
    auto& process_synchronizer = process_synchronizer_result.value();

    auto instance_specifier_result = InstanceSpecifier::Create(std::string{kInstanceSpecifierString});
    if (!instance_specifier_result.has_value())
    {
        NotifyAndFail(process_synchronizer, "Consumer: Unable to create instance specifier");
    }
    auto instance_specifier = std::move(instance_specifier_result).value();

    ProxyContainer<InitialOnlyProxy> proxy_container{};
    proxy_container.CreateProxy(std::move(instance_specifier), "set_and_notifier");

    auto& proxy = proxy_container.GetProxy();
    std::ignore = proxy.test_field.Subscribe(kMaxNumSamples);
    if (!WaitForSubscription(proxy.test_field, num_retries, retry_backoff_time))
    {
        NotifyAndFail(process_synchronizer, "Consumer: Subscription failed in notifier scenario");
    }

    const bool initial_value_received = PollForValue(proxy.test_field, kInitialValue, num_retries, retry_backoff_time);
    proxy.test_field.Unsubscribe();

    if (!initial_value_received)
    {
        NotifyAndFail(process_synchronizer,
                      "Consumer: Did not receive expected initial value ",
                      kInitialValue,
                      " in notifier scenario");
    }

    process_synchronizer.Notify();
}

void run_set_consumer(const std::size_t num_retries, const std::chrono::milliseconds retry_backoff_time)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!process_synchronizer_result.has_value())
    {
        FailTest("Consumer: Could not create ProcessSynchronizer");
    }
    auto& process_synchronizer = process_synchronizer_result.value();

    auto instance_specifier_result = InstanceSpecifier::Create(std::string{kInstanceSpecifierString});
    if (!instance_specifier_result.has_value())
    {
        NotifyAndFail(process_synchronizer, "Consumer: Unable to create instance specifier");
    }
    auto instance_specifier = std::move(instance_specifier_result).value();

    ProxyContainer<SetEnabledProxy> proxy_container{};
    proxy_container.CreateProxy(std::move(instance_specifier), "set_and_notifier");

    auto& proxy = proxy_container.GetProxy();
    std::ignore = proxy.test_field.Subscribe(kMaxNumSamples);
    if (!WaitForSubscription(proxy.test_field, num_retries, retry_backoff_time))
    {
        NotifyAndFail(process_synchronizer, "Consumer: Subscription failed in set scenario");
    }

    const bool initial_value_received = PollForValue(proxy.test_field, kInitialValue, num_retries, retry_backoff_time);
    if (!initial_value_received)
    {
        NotifyAndFail(
            process_synchronizer, "Consumer: Did not receive initial value ", kInitialValue, " in set scenario");
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
        NotifyAndFail(process_synchronizer, "Consumer: Valid Set call failed: ", valid_set_error);
    }

    if (valid_accepted_value != kSetValidValue)
    {
        NotifyAndFail(process_synchronizer,
                      "Consumer: Valid Set accepted value mismatch. Expected ",
                      kSetValidValue,
                      " but got ",
                      valid_accepted_value);
    }

    const bool valid_value_received = PollForValue(proxy.test_field, kSetValidValue, num_retries, retry_backoff_time);
    if (!valid_value_received)
    {
        NotifyAndFail(
            process_synchronizer, "Consumer: Did not receive valid set value ", kSetValidValue, " after Set call");
    }

    std::int32_t requested_value = kSetRequestValue;
    std::int32_t accepted_value{0};
    std::string invalid_set_error;
    if (!SetWithRetries(
            proxy.test_field, requested_value, accepted_value, num_retries, retry_backoff_time, invalid_set_error))
    {
        NotifyAndFail(process_synchronizer, "Consumer: Set call failed: ", invalid_set_error);
    }

    if (accepted_value != kSetAcceptedValue)
    {
        NotifyAndFail(process_synchronizer,
                      "Consumer: Set accepted value mismatch. Expected ",
                      kSetAcceptedValue,
                      " but got ",
                      accepted_value);
    }

    const bool clamped_value_received =
        PollForValue(proxy.test_field, kSetAcceptedValue, num_retries, retry_backoff_time);
    proxy.test_field.Unsubscribe();

    if (!clamped_value_received)
    {
        NotifyAndFail(
            process_synchronizer, "Consumer: Did not receive clamped value ", kSetAcceptedValue, " after Set call");
    }

    process_synchronizer.Notify();
}

}  // namespace

void run_consumer(const std::size_t num_retries,
                  const std::chrono::milliseconds retry_backoff_time,
                  const std::string& mode)
{
    if (mode == "notifier")
    {
        run_notifier_consumer(num_retries, retry_backoff_time);
        return;
    }
    if (mode == "set")
    {
        run_set_consumer(num_retries, retry_backoff_time);
        return;
    }

    // TODO: Add "get" mode consumer scenario coverage once getter-enabled field variant is introduced.
    FailTest("Consumer: Unsupported mode: ", mode);
}

}  // namespace score::mw::com::test
