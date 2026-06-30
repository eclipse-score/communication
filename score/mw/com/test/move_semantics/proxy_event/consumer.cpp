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
#include "score/mw/com/test/move_semantics/proxy_event/consumer.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"
#include "score/mw/com/test/move_semantics/proxy_event/test_event_datatype.h"
#include "score/mw/com/types.h"

#include <cstdint>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/proxy_event_move_semantics_interprocess_notification"};

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

template <typename ProxyEventType>
class ProxyEventReceiver
{
  public:
    explicit ProxyEventReceiver(ProxyEventType& proxy_event) : proxy_event_{proxy_event}
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
                [this](SamplePtr<std::uint32_t> sample) {
                    GetNewSamplesCallback(std::move(sample));
                },
                num_samples_to_receive);
            if (!get_samples_result.has_value())
            {
                FailTest("proxy_event_move_semantics consumer failed: GetNewSamples failed: ",
                         get_samples_result.error());
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
    void GetNewSamplesCallback(SamplePtr<std::uint32_t> sample)
    {
        if (sample == nullptr)
        {
            FailTest("proxy_event_move_semantics consumer failed: received null sample");
        }
        const std::uint32_t expected_value = latest_value_.has_value() ? latest_value_.value() + 1U : 1U;
        if (*sample != expected_value)
        {
            FailTest("proxy_event_move_semantics consumer failed: received value ",
                     *sample,
                     " does not match expected value ",
                     expected_value);
        }
        latest_value_ = *sample;
    }

    score::concurrency::Notification received_sample_notification_{};
    std::optional<std::uint32_t> latest_value_{};
    ProxyEventType& proxy_event_;
};

void RunConsumerMoveConstructProxyBeforeSubscribe(const InstanceSpecifier& instance_specifier,
                                                  const std::size_t num_samples_to_receive,
                                                  const std::size_t num_send_iterations,
                                                  const score::cpp::stop_token& stop_token)
{
    const auto name = filesystem::Path{instance_specifier.ToString()}.Filename().Native();
    auto process_synchronizer_result =
        ProcessSynchronizer::Create(kInterprocessNotificationShmPath + std::string{name});
    if (!process_synchronizer_result.has_value())
    {
        FailTest("proxy_event_move_semantics consumer failed: could not create ready synchronizer");
    }

    ExitFunctionGuard done_guard{[&process_synchronizer_result]() {
        process_synchronizer_result->Notify();
    }};

    ProxyContainer<ProxyMoveSemanticsProxy> proxy_container{};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1 - Find service and create proxy" << std::endl;
    proxy_container.CreateProxy(kInstanceSpecifierMovedTo, "proxy_event_move_semantics");

    // Step 2. Move construct proxy before subscribe
    std::cout << "\nConsumer: Step 2 - Move construct proxy before subscribe" << std::endl;
    auto moved_proxy = proxy_container.Extract();

    // Step 3. Register receive handler
    std::cout << "\nConsumer: Step 3 - Register receive handler" << std::endl;
    ProxyEventReceiver proxy_event_receiver{moved_proxy.moved_event_};

    // Step 4. Register state change handler
    std::cout << "\nConsumer: Step 4 - Register state change handler" << std::endl;
    ProxyStateChangeNotifier proxy_state_change_notifier{moved_proxy.moved_event_};

    // Step 5. Subscribe
    std::cout << "\nConsumer: Step 5 - Subscribe" << std::endl;
    auto subscribe_result = moved_proxy.moved_event_.Subscribe(num_samples_to_receive);
    if (!subscribe_result.has_value())
    {
        FailTest("proxy_event_move_semantics consumer failed: Subscribe failed: ", subscribe_result.error());
    }

    // Step 6. Wait for provider to send values and notify
    std::cout << "\nConsumer: Step 6 - Wait for provider to send values and notify" << std::endl;
    for (std::size_t iteration = 0U; iteration < num_send_iterations; ++iteration)
    {
        std::cout << "\nConsumer: Iteration " << (iteration + 1U) << " of " << num_send_iterations << std::endl;
        proxy_state_change_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed);

        proxy_event_receiver.WaitForSamples(stop_token, num_samples_to_receive);
        process_synchronizer_result->Notify();
    }

    std::cout << "Consumer: Done with all iterations, exiting" << std::endl;
}

}  // namespace

void RunConsumer(const ProxyMoveScenario& scenario,
                 const InstanceSpecifier& instance_specifier,
                 const std::size_t num_samples_to_receive,
                 const std::size_t num_send_iterations,
                 const score::cpp::stop_token& stop_token)
{
    if (scenario == ProxyMoveScenario::kMoveConstructBeforeSubscribe)
    {
        RunConsumerMoveConstructProxyBeforeSubscribe(
            instance_specifier, num_samples_to_receive, num_send_iterations, stop_token);
        return;
    }

    FailTest("Unknown proxy move scenario in consumer");
}

}  // namespace score::mw::com::test
