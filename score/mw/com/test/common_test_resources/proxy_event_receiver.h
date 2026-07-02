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

#include "score/concurrency/notification.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>

#include <cstdint>
#include <iostream>
#include <optional>

namespace score::mw::com::test
{

/// \brief Helper class which registers a receiveHandler on construction and allows waiting for a
/// certain number of samples to be received. It also checks that the received samples are in the expected order.
template <typename ProxyEventType, typename GetNewSamplesCallback>
class ProxyEventReceiver
{
  public:
    explicit ProxyEventReceiver(ProxyEventType& proxy_event, GetNewSamplesCallback get_new_samples_callback)
        : proxy_event_{proxy_event}, get_new_samples_callback_{std::move(get_new_samples_callback)}
    {
        auto receive_handler = [&received_sample_notification = received_sample_notification_]() {
            std::cout << "ProxyEventReceiver: Received event notification" << std::endl;
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
    ProxyEventReceiver(ProxyEventReceiver&&) = delete;
    ProxyEventReceiver& operator=(ProxyEventReceiver&&) = delete;

    /// \brief Wait for a certain number of samples to be received.
    ///
    /// \return true if the expected number of samples was received, false if the wait was interrupted by the
    /// stop_token.
    [[nodiscard]] bool WaitForSamples(const score::cpp::stop_token& stop_token,
                                      const std::size_t num_samples_to_receive)
    {
        std::size_t received_count{0U};
        while (!stop_token.stop_requested())
        {
            auto get_samples_result = proxy_event_.GetNewSamples(
                [this](SamplePtr<std::uint32_t> sample) {
                    std::invoke(get_new_samples_callback_, std::move(sample));
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

  private:
    score::concurrency::Notification received_sample_notification_{};
    std::optional<std::uint32_t> latest_value_{};
    ProxyEventType& proxy_event_;
    GetNewSamplesCallback get_new_samples_callback_;
};

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_PROXY_EVENT_RECEIVER_H
