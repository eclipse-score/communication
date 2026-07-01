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
#ifndef SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_PROXY_CONTAINER_H
#define SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_PROXY_CONTAINER_H

#include "score/concurrency/notification.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/types.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace score::mw::com::test
{

template <typename Proxy>
class ProxyContainer
{
  public:
    void CreateProxy(InstanceSpecifier instance_specifier, const std::string& failure_message_prefix);

    Proxy& GetProxy()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(proxy_ != nullptr,
                                                    "Proxy was not successfully created! Cannot get it!");
        return *proxy_;
    }

    Proxy&& Extract()
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(proxy_ != nullptr,
                                                    "Proxy was not successfully created! Cannot extract it!");
        return std::move(*proxy_);
    }

  private:
    std::unique_ptr<typename Proxy::HandleType> handle_{nullptr};
    std::mutex proxy_creation_mutex_{};
    std::condition_variable proxy_creation_condition_variable_{};

    std::unique_ptr<Proxy> proxy_{nullptr};
};

template <typename ProxyEventType>
class ProxyStateChangeNotifier
{
  public:
    explicit ProxyStateChangeNotifier(ProxyEventType& proxy_event) : proxy_event_{proxy_event}
    {
        auto state_change_handler = [this](const SubscriptionState new_state) -> bool {
            std::cout << "Service state changed, new state: " << static_cast<std::uint32_t>(new_state) << std::endl;
            std::lock_guard lock(mutex_);
            last_seen_state_ = new_state;
            condition_variable_.notify_all();
            return true;
        };
        proxy_event_.SetSubscriptionStateChangeHandler(state_change_handler);
    }

    ~ProxyStateChangeNotifier()
    {
        proxy_event_.UnsetSubscriptionStateChangeHandler();
    }

    ProxyStateChangeNotifier(const ProxyStateChangeNotifier&) = delete;
    ProxyStateChangeNotifier& operator=(const ProxyStateChangeNotifier&) = delete;
    ProxyStateChangeNotifier(const ProxyStateChangeNotifier&&) = delete;
    ProxyStateChangeNotifier& operator=(const ProxyStateChangeNotifier&&) = delete;

    bool WaitForStateChange(const score::cpp::stop_token& stop_token, SubscriptionState desired_state)
    {
        std::unique_lock lock(mutex_);
        return condition_variable_.wait(lock, stop_token, [this, desired_state]() {
            return last_seen_state_.has_value() && last_seen_state_.value() == desired_state;
        });
    }

  private:
    std::mutex mutex_{};
    concurrency::InterruptibleConditionalVariable condition_variable_{};
    std::optional<SubscriptionState> last_seen_state_{};
    ProxyEventType& proxy_event_;
};

template <typename ProxyEventType, typename SampleType = std::uint32_t>
class ProxyEventReceiver
{
  public:
    ProxyEventReceiver(ProxyEventType& proxy_event, std::string failure_message_prefix)
        : failure_message_prefix_{std::move(failure_message_prefix)}, proxy_event_{proxy_event}
    {
        auto receive_handler = [&received_sample_notification = received_sample_notification_]() {
            std::cout << "Received event notification" << std::endl;
            received_sample_notification.notify();
        };
        proxy_event_.SetReceiveHandler(receive_handler);
    }

    ~ProxyEventReceiver()
    {
        proxy_event_.UnsetReceiveHandler();
    }

    ProxyEventReceiver(const ProxyEventReceiver&) = delete;
    ProxyEventReceiver& operator=(const ProxyEventReceiver&) = delete;
    ProxyEventReceiver(const ProxyEventReceiver&&) = delete;
    ProxyEventReceiver& operator=(const ProxyEventReceiver&&) = delete;

    void WaitForSamples(const score::cpp::stop_token& stop_token, const std::size_t num_samples_to_receive)
    {
        std::size_t received_count{0U};
        while (!stop_token.stop_requested())
        {
            auto get_samples_result = proxy_event_.GetNewSamples(
                [this](SamplePtr<SampleType> sample) {
                    GetNewSamplesCallback(std::move(sample));
                },
                num_samples_to_receive);
            if (!get_samples_result.has_value())
            {
                FailTest(failure_message_prefix_, " GetNewSamples failed: ", get_samples_result.error());
            }

            received_count += get_samples_result.value();
            std::cout << "Received " << get_samples_result.value() << " samples. " << received_count
                      << " samples so far" << std::endl;

            if (received_count == num_samples_to_receive)
            {
                break;
            }

            const auto notification_received = received_sample_notification_.waitWithAbort(stop_token);
            if (!notification_received)
            {
                continue;
            }

            received_sample_notification_.reset();
        }
        std::cout << "\nConsumer: Done receiving samples, received " << received_count << " samples in total\n";
    }

  private:
    void GetNewSamplesCallback(SamplePtr<SampleType> sample)
    {
        if (sample == nullptr)
        {
            FailTest(failure_message_prefix_, " received null sample");
        }
        const SampleType expected_value = latest_value_.has_value() ? latest_value_.value() + 1U : 1U;
        if (*sample != expected_value)
        {
            FailTest(failure_message_prefix_,
                     " received value ",
                     *sample,
                     " does not match expected value ",
                     expected_value);
        }
        latest_value_ = *sample;
    }

    std::string failure_message_prefix_;
    concurrency::Notification received_sample_notification_{};
    std::optional<SampleType> latest_value_{};
    ProxyEventType& proxy_event_;
};

template <typename Proxy>
void ProxyContainer<Proxy>::CreateProxy(InstanceSpecifier instance_specifier, const std::string& failure_message_prefix)
{
    bool callback_called{false};
    auto find_service_callback = [this, &callback_called](auto service_handle_container,
                                                          auto find_service_handle) mutable noexcept {
        std::cout << "Consumer: find service handler called" << std::endl;
        std::lock_guard lock{proxy_creation_mutex_};
        if (service_handle_container.size() != 1)
        {
            std::cerr << "Consumer: service handle container should contain 1 handle but contains: "
                      << service_handle_container.size() << std::endl;
            callback_called = true;
            proxy_creation_condition_variable_.notify_all();
            return;
        }
        handle_ = std::make_unique<typename Proxy::HandleType>(service_handle_container[0]);
        score::cpp::ignore = Proxy::StopFindService(find_service_handle);
        callback_called = true;
        proxy_creation_condition_variable_.notify_all();
    };

    auto start_find_service_result = Proxy::StartFindService(find_service_callback, instance_specifier);
    if (!start_find_service_result.has_value())
    {
        FailTest(failure_message_prefix, " Consumer: StartFindService() failed: ", start_find_service_result.error());
    }
    std::cout << "Consumer: StartFindService called" << std::endl;

    // Wait for the find service handler to be called (with timeout to prevent indefinite blocking)
    constexpr auto kServiceDiscoveryTimeout = std::chrono::seconds(10);
    std::unique_lock proxy_creation_lock{proxy_creation_mutex_};
    const bool service_found =
        proxy_creation_condition_variable_.wait_for(proxy_creation_lock, kServiceDiscoveryTimeout, [&callback_called] {
            return callback_called;
        });
    if (!service_found || handle_ == nullptr)
    {
        FailTest(failure_message_prefix, " Consumer: StartFindService() failed to get handle");
    }

    auto proxy_result = Proxy::Create(*handle_);
    proxy_creation_lock.unlock();
    if (!proxy_result.has_value())
    {
        FailTest(failure_message_prefix, " Consumer: Unable to construct proxy: ", proxy_result.error());
    }
    proxy_ = std::make_unique<Proxy>(std::move(proxy_result).value());

    std::cout << "Consumer: Proxy created successfully" << std::endl;
}

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_PROXY_CONTAINER_H
