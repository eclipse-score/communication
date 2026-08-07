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
#ifndef SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_PROXY_EVENT_RECEIVER_H
#define SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_PROXY_EVENT_RECEIVER_H

#include "score/mw/com/test/common_test_resources/fail_test.h"

#include "score/concurrency/notification.h"

#include <score/stop_token.hpp>

#include <functional>
#include <iostream>
#include <type_traits>
#include <utility>
#include <variant>

namespace score::mw::com::test
{

/// \brief Helper class which registers a receiveHandler on construction and allows waiting for a
/// certain number of samples to be received. It also checks that the received samples are in the expected order.
template <typename ProxyOrFieldType, typename StoredCallback = std::monostate>
class ProxyEventReceiver
{
  public:
    explicit ProxyEventReceiver(ProxyOrFieldType& proxy_event_or_field)
        : received_sample_notification_{}, proxy_event_or_field_{proxy_event_or_field}, stored_callback_{}
    {
        auto receive_handler = [&received_sample_notification = received_sample_notification_]() {
            std::cout << "ProxyEventReceiver: Received event notification" << std::endl;
            received_sample_notification.notify();
        };
        proxy_event_or_field_.get().SetReceiveHandler(receive_handler);
    }

    explicit ProxyEventReceiver(ProxyOrFieldType& proxy_event_or_field, StoredCallback callback)
        : received_sample_notification_{},
          proxy_event_or_field_{proxy_event_or_field},
          stored_callback_{std::move(callback)}
    {
        auto receive_handler = [&received_sample_notification = received_sample_notification_]() {
            std::cout << "ProxyEventReceiver: Received event notification" << std::endl;
            received_sample_notification.notify();
        };
        proxy_event_or_field_.get().SetReceiveHandler(receive_handler);
    }

    ~ProxyEventReceiver()
    {
        proxy_event_or_field_.get().UnsetReceiveHandler();
    }

    ProxyEventReceiver(const ProxyEventReceiver&) = delete;
    ProxyEventReceiver& operator=(const ProxyEventReceiver&) = delete;
    ProxyEventReceiver(ProxyEventReceiver&&) = delete;
    ProxyEventReceiver& operator=(ProxyEventReceiver&&) = delete;

    /// \brief Reattach the receiver to a new event object (e.g. after a proxy move).
    /// Updates the internal reference so that GetNewSamples and UnsetReceiveHandler
    /// are called on the new event without re-registering the receive handler.
    void Reattach(ProxyOrFieldType& new_event)
    {
        proxy_event_or_field_ = std::ref(new_event);
    }

    /// \brief Wait for a certain number of samples to be received using the stored callback.
    ///
    /// Only available when constructed with a callback argument.
    ///
    /// \return true if the expected number of samples was received, false if the wait was interrupted by the
    /// stop_token.
    [[nodiscard]] bool WaitForSamples(const score::cpp::stop_token& stop_token,
                                      const std::size_t num_samples_to_receive)
    {
        static_assert(!std::is_same<StoredCallback, std::monostate>::value,
                      "WaitForSamples(stop_token, num_samples) requires a callback to be passed at construction");
        return WaitForSamples(stop_token, num_samples_to_receive, stored_callback_);
    }

    /// \brief Wait for a certain number of samples to be received.
    ///
    /// \return true if the expected number of samples was received, false if the wait was interrupted by the
    /// stop_token.
    template <typename GetNewSamplesCallback>
    [[nodiscard]] bool WaitForSamples(const score::cpp::stop_token& stop_token,
                                      const std::size_t num_samples_to_receive,
                                      GetNewSamplesCallback&& get_new_samples_callback)
    {
        std::size_t received_count{0U};
        while (!stop_token.stop_requested())
        {
            auto get_samples_result = proxy_event_or_field_.get().GetNewSamples(
                [&get_new_samples_callback](auto sample) {
                    std::invoke(get_new_samples_callback, std::move(sample));
                },
                num_samples_to_receive);
            if (!get_samples_result.has_value())
            {
                FailTest("ProxyEventReceiver failed: GetNewSamples failed: ", get_samples_result.error());
            }

            received_count += get_samples_result.value();
            std::cout << "ProxyEventReceiver: Received " << get_samples_result.value() << " samples. " << received_count
                      << " samples so far" << std::endl;

            if (received_count == num_samples_to_receive)
            {
                break;
            }

            const auto notification_received = received_sample_notification_.waitWithAbort(stop_token);
            if (!notification_received)
            {
                std::cout << "ProxyEventReceiver: Wait for samples was interrupted by stop_token." << std::endl;
                return false;
            }

            received_sample_notification_.reset();
        }

        if (received_count != num_samples_to_receive)
        {
            std::cout << "ProxyEventReceiver: Expected to receive " << num_samples_to_receive
                      << " samples, but only received " << received_count << " samples." << std::endl;
            return false;
        }

        std::cout << "ProxyEventReceiver: Done receiving samples, received " << received_count << " samples in total"
                  << std::endl;
        return true;
    }

    /// \brief Wait for a certain set of events to be received in the expected order.
    ///
    /// \return true if the expected samples were received, false if the wait was interrupted by the
    /// stop_token.
    template <typename SampleType>
    [[nodiscard]] bool WaitForSamples(const score::cpp::stop_token& stop_token,
                                      const std::vector<SampleType>& expected_samples)
    {
        const auto number_of_expected_samples = expected_samples.size();
        std::size_t sample_idx{0U};
        auto get_new_samples_callback = [&expected_samples, &sample_idx](const auto& sample_ptr) {
            const auto& received_value = *sample_ptr;
            const auto expected_value = expected_samples[sample_idx];
            if (received_value != expected_value)
            {
                FailTest("ProxyEventReceiver: Received unexpected value. Expected: ",
                         expected_value,
                         ". Received: ",
                         received_value);
            }
            ++sample_idx;
        };
        return WaitForSamples(stop_token, number_of_expected_samples, std::move(get_new_samples_callback));
    }

  private:
    score::concurrency::Notification received_sample_notification_;
    std::reference_wrapper<ProxyOrFieldType> proxy_event_or_field_;
    StoredCallback stored_callback_;
};

template <typename ProxyOrFieldType>
ProxyEventReceiver(ProxyOrFieldType&) -> ProxyEventReceiver<ProxyOrFieldType>;

template <typename ProxyOrFieldType, typename CallbackType>
ProxyEventReceiver(ProxyOrFieldType&, CallbackType) -> ProxyEventReceiver<ProxyOrFieldType, CallbackType>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_PROXY_EVENT_RECEIVER_H
