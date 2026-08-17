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
#ifndef SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_PROXY_EVENT_STATE_CHANGE_NOTIFIER_H
#define SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_PROXY_EVENT_STATE_CHANGE_NOTIFIER_H

#include "score/concurrency/interruptible_interprocess_condition_variable.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <thread>

namespace score::mw::com::test
{

/// \brief Helper class which registers a SubscriptionStateChangeHandler on construction and allows waiting for a
/// specific state change to occur.
template <typename ProxyEventType>
class ProxyEventStateChangeNotifier
{
  public:
    explicit ProxyEventStateChangeNotifier(ProxyEventType& proxy_event) : proxy_event_{proxy_event}
    {
        auto state_change_handler = [this](const SubscriptionState new_state) -> bool {
            std::cout << "ProxyEventStateChangeNotifier: Service state changed, new state: "
                      << static_cast<std::uint32_t>(new_state) << std::endl;
            std::lock_guard lock(mutex_);
            last_seen_state_ = new_state;
            condition_variable_.notify_all();
            return true;
        };
        const auto registration_result = proxy_event_.get().SetSubscriptionStateChangeHandler(state_change_handler);
        if (!registration_result.has_value())
        {
            FailTest("ProxyEventStateChangeNotifier: Failed to register state change handler: ",
                     registration_result.error());
        }
    }

    ~ProxyEventStateChangeNotifier()
    {
        proxy_event_.get().UnsetSubscriptionStateChangeHandler();
    }

    /// \brief Rebind this notifier to a different event after a proxy move.
    ///
    /// Does NOT re-register the subscription state change handler — the handler was already transferred to the
    /// new event by the proxy move. Only updates the internal reference so that subsequent GetSubscriptionState
    /// calls target the correct event.
    void Reattach(ProxyEventType& new_event) noexcept
    {
        proxy_event_ = std::ref(new_event);
    }

    ProxyEventStateChangeNotifier(const ProxyEventStateChangeNotifier&) = delete;
    ProxyEventStateChangeNotifier& operator=(const ProxyEventStateChangeNotifier&) = delete;
    ProxyEventStateChangeNotifier(ProxyEventStateChangeNotifier&&) = delete;
    ProxyEventStateChangeNotifier& operator=(ProxyEventStateChangeNotifier&&) = delete;

    /// \brief Waits for the subscription state to be in the desired state (will return immediately if already in the
    /// desired state).
    ///
    /// Can be called multiple times.
    /// \return true if the desired state was reached, false if the wait was interrupted by the stop_token.
    [[nodiscard]] bool WaitForStateChange(const score::cpp::stop_token& stop_token, SubscriptionState desired_state)
    {
        // Poll every 10 ms, checking both the callback-recorded state and the live
        // GetSubscriptionState(). Relying solely on the callback is unreliable: under LoLa SHM the
        // state-change callback is sometimes never delivered after an unsubscribe/re-subscribe cycle.
        //
        // IMPORTANT: GetSubscriptionState() is intentionally called WITHOUT holding mutex_.
        // LoLa's background thread can hold an internal LoLa lock while invoking the state-change
        // callback (which acquires mutex_). If we called GetSubscriptionState() (which may also
        // acquire the same internal LoLa lock) while holding mutex_, we would get an ABBA deadlock.
        while (!stop_token.stop_requested())
        {
            {
                std::lock_guard lock(mutex_);
                if (last_seen_state_.has_value() && last_seen_state_.value() == desired_state)
                {
                    last_seen_state_.reset();
                    return true;
                }
            }
            // Check the live state outside the lock to avoid the ABBA deadlock described above.
            if (proxy_event_.get().GetSubscriptionState() == desired_state)
            {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }
        return false;
    }

  private:
    std::mutex mutex_{};
    concurrency::InterruptibleConditionalVariable condition_variable_{};
    std::optional<SubscriptionState> last_seen_state_{};
    std::reference_wrapper<ProxyEventType> proxy_event_;
};

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_PROXY_EVENT_STATE_CHANGE_NOTIFIER_H
