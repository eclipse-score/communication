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
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/fields/set_and_notifier/fields_test_resources/initial_only_field.h"
#include "score/mw/com/test/fields/set_and_notifier/fields_test_resources/set_enabled_field.h"
#include "score/mw/com/test/fields/set_and_notifier/fields_test_resources/test_constants.h"
#include "score/mw/com/types.h"

#include <score/optional.hpp>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <iostream>
#include <mutex>
#include <string>
#include <utility>

namespace score::mw::com::test
{
namespace
{

constexpr auto kMaxNumSamples{1U};
const std::string kInterprocessNotificationShmPath{"/fields_set_and_notifier_interprocess_notification"};
const std::string kNotifierConsumerReadyShmPath{"/fields_notifier_consumer_ready"};

template <typename FieldType>
bool WaitForSubscription(FieldType& field, std::size_t retries, const std::chrono::milliseconds retry_backoff_time)
{
    std::mutex mutex;
    std::condition_variable cv;
    bool subscribed{false};

    const auto handler_result = field.SetSubscriptionStateChangeHandler(
        [&mutex, &cv, &subscribed](score::mw::com::impl::SubscriptionState new_state) noexcept -> bool {
            if (new_state == score::mw::com::impl::SubscriptionState::kSubscribed)
            {
                {
                    std::lock_guard<std::mutex> lock{mutex};
                    subscribed = true;
                }
                cv.notify_one();
                return false;
            }
            return true;
        });

    if (!handler_result.has_value())
    {
        return false;
    }

    // subscription may have become kSubscribed before the handler was registered
    if (field.GetSubscriptionState() == score::mw::com::impl::SubscriptionState::kSubscribed)
    {
        std::ignore = field.UnsetSubscriptionStateChangeHandler();
        return true;
    }

    const auto total_wait_time = retry_backoff_time * retries;
    std::unique_lock<std::mutex> lock{mutex};
    const bool success = cv.wait_for(lock, total_wait_time, [&subscribed] {
        return subscribed;
    });
    if (!success)
    {
        std::ignore = field.UnsetSubscriptionStateChangeHandler();
    }
    return success;
}

template <typename FieldType>
bool WaitForValue(FieldType& field,
                  const std::int32_t expected_value,
                  std::size_t retries,
                  const std::chrono::milliseconds retry_backoff_time)
{
    struct SyncState
    {
        std::mutex mutex{};
        std::condition_variable cv{};
        bool value_received{false};
    };
    SyncState sync_state;

    const auto handler_result = field.SetReceiveHandler([&field, &sync_state, expected_value]() noexcept {
        score::cpp::optional<std::int32_t> received;
        std::ignore = field.GetNewSamples(
            [&received](const auto& sample_ptr) noexcept {
                received = *sample_ptr;
            },
            kMaxNumSamples);
        if (received.has_value() && received.value() == expected_value)
        {
            {
                std::lock_guard<std::mutex> lock{sync_state.mutex};
                sync_state.value_received = true;
            }
            sync_state.cv.notify_one();
        }
    });

    if (!handler_result.has_value())
    {
        return false;
    }

    // Handle race: value may already be in the buffer before the handler was registered
    {
        score::cpp::optional<std::int32_t> existing;
        std::ignore = field.GetNewSamples(
            [&existing](const auto& sample_ptr) noexcept {
                existing = *sample_ptr;
            },
            kMaxNumSamples);
        if (existing.has_value() && existing.value() == expected_value)
        {
            std::ignore = field.UnsetReceiveHandler();
            return true;
        }
    }

    const auto total_wait_time = retry_backoff_time * retries;
    bool success;
    {
        std::unique_lock<std::mutex> lock{sync_state.mutex};
        success = sync_state.cv.wait_for(lock, total_wait_time, [&sync_state] {
            return sync_state.value_received;
        });
    }
    std::ignore = field.UnsetReceiveHandler();
    return success;
}

}  // namespace

void run_notifier_consumer(const std::size_t num_retries, const std::chrono::milliseconds retry_backoff_time)
{
    auto consumer_ready_synchronizer_result = ProcessSynchronizer::Create(kNotifierConsumerReadyShmPath);
    if (!consumer_ready_synchronizer_result.has_value())
    {
        FailTest("Consumer: Could not create consumer ready ProcessSynchronizer");
    }
    auto& consumer_ready_synchronizer = consumer_ready_synchronizer_result.value();

    auto done_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!done_synchronizer_result.has_value())
    {
        FailTest("Consumer: Could not create done ProcessSynchronizer");
    }
    ExitFunctionGuard done_guard{[&done_synchronizer_result]() {
        done_synchronizer_result->Notify();
    }};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1" << std::endl;
    auto instance_specifier_result = InstanceSpecifier::Create(std::string{kInstanceSpecifierString});
    if (!instance_specifier_result.has_value())
    {
        FailTest("Consumer: Unable to create instance specifier");
    }
    ProxyContainer<InitialOnlyProxy> proxy_container{};
    proxy_container.CreateProxy(std::move(instance_specifier_result).value(), "set_and_notifier");
    auto& proxy = proxy_container.GetProxy();

    // Step 2. Subscribe to field and wait for subscription
    std::cout << "\nConsumer: Step 2" << std::endl;
    std::ignore = proxy.test_field.Subscribe(kMaxNumSamples);
    if (!WaitForSubscription(proxy.test_field, num_retries, retry_backoff_time))
    {
        FailTest("Consumer: Subscription failed in notifier scenario");
    }

    // Step 3. Verify initial value received
    std::cout << "\nConsumer: Step 3" << std::endl;
    const bool initial_value_received = WaitForValue(proxy.test_field, kInitialValue, num_retries, retry_backoff_time);
    if (!initial_value_received)
    {
        FailTest("Consumer: Did not receive expected initial value ", kInitialValue, " in notifier scenario");
    }

    // Step 4. Notify provider that consumer is ready
    std::cout << "\nConsumer: Step 4" << std::endl;
    consumer_ready_synchronizer.Notify();

    // Step 5. Verify updated value received after provider publishes update
    std::cout << "\nConsumer: Step 5" << std::endl;
    const bool updated_value_received = WaitForValue(proxy.test_field, kUpdatedValue, num_retries, retry_backoff_time);
    proxy.test_field.Unsubscribe();

    if (!updated_value_received)
    {
        FailTest("Consumer: Did not receive expected updated value ", kUpdatedValue, " in notifier scenario");
    }
}

void run_set_and_notifier_consumer(const std::size_t num_retries, const std::chrono::milliseconds retry_backoff_time)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!process_synchronizer_result.has_value())
    {
        FailTest("Consumer: Could not create ProcessSynchronizer");
    }
    ExitFunctionGuard process_synchronizer_guard{[&process_synchronizer_result]() {
        process_synchronizer_result->Notify();
    }};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1" << std::endl;
    auto instance_specifier_result = InstanceSpecifier::Create(std::string{kInstanceSpecifierString});
    if (!instance_specifier_result.has_value())
    {
        FailTest("Consumer: Unable to create instance specifier");
    }
    ProxyContainer<SetEnabledProxy> proxy_container{};
    proxy_container.CreateProxy(std::move(instance_specifier_result).value(), "set_and_notifier");
    auto& proxy = proxy_container.GetProxy();

    // Step 2. Subscribe to field and wait for subscription
    std::cout << "\nConsumer: Step 2" << std::endl;
    std::ignore = proxy.test_field.Subscribe(kMaxNumSamples);
    if (!WaitForSubscription(proxy.test_field, num_retries, retry_backoff_time))
    {
        FailTest("Consumer: Subscription failed in set scenario");
    }

    // Step 3. Verify initial value received
    std::cout << "\nConsumer: Step 3" << std::endl;
    const bool initial_value_received = WaitForValue(proxy.test_field, kInitialValue, num_retries, retry_backoff_time);
    if (!initial_value_received)
    {
        FailTest("Consumer: Did not receive initial value ", kInitialValue, " in set scenario");
    }

    // Step 4. Set new field value and verify accepted value matches expected transformed value
    std::cout << "\nConsumer: Step 4" << std::endl;
    const auto set_result = proxy.test_field.Set(kSetRequestValue);
    if (!set_result.has_value())
    {
        FailTest("Consumer: Set call failed: ", set_result.error());
    }
    const std::int32_t accepted_value = *(set_result.value());

    if (accepted_value != kSetTransformedValue)
    {
        FailTest("Consumer: Set accepted value mismatch. Expected ", kSetTransformedValue, " but got ", accepted_value);
    }

    // Step 5. Verify transformed value received via field notification
    std::cout << "\nConsumer: Step 5" << std::endl;
    const bool transformed_value_received =
        WaitForValue(proxy.test_field, kSetTransformedValue, num_retries, retry_backoff_time);
    proxy.test_field.Unsubscribe();

    if (!transformed_value_received)
    {
        FailTest("Consumer: Did not receive transformed value ", kSetTransformedValue, " after Set call");
    }
}

void run_consumer(const std::size_t num_retries,
                  const std::chrono::milliseconds retry_backoff_time,
                  const std::string& mode)
{
    if (mode == "notifier")
    {
        run_notifier_consumer(num_retries, retry_backoff_time);
        return;
    }
    if (mode == "set_and_notifier")
    {
        run_set_and_notifier_consumer(num_retries, retry_backoff_time);
        return;
    }

    // TODO: Add "get" mode consumer scenario coverage once getter-enabled field variant is introduced.
    FailTest("Consumer: Unsupported mode: ", mode);
}
}  // namespace score::mw::com::test
