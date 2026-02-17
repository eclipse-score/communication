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

#include "score/mw/com/test/unsubscribe_deadlock/unsubscribe_deadlock_application.h"

#include "score/concurrency/notification.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/test/common_test_resources/assert_handler.h"
#include "score/mw/com/test/common_test_resources/big_datatype.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"
#include "score/mw/com/types.h"

#include <score/jthread.hpp>
#include <score/stop_token.hpp>

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <thread>

const score::mw::com::test::MapApiLanesStamped kEvent_sample{};

void CreateAndOfferSkeleton(const score::mw::com::InstanceSpecifier& instance_specifier, std::atomic_bool& success_flag)
{
    for (std::size_t j = 0; j < 10; ++j)
    {
        auto bigdata_result = score::mw::com::test::BigDataSkeleton::Create(instance_specifier);
        if (!bigdata_result.has_value())
        {
            success_flag = false;
            std::cerr << "Could not create skeleton with instance specifier" << instance_specifier.ToString()
                      << "in index " << j << "of loop, terminating." << std::endl;
            break;
        }
        const auto offer_service_result = bigdata_result->OfferService();
        if (!offer_service_result.has_value())
        {
            success_flag = false;
            std::cerr << "Could not offer service for skeleton with instance specifier" << instance_specifier.ToString()
                      << "in index " << j << "of loop, terminating." << std::endl;
            break;
        }
    }
}

score::Result<score::mw::com::test::BigDataProxy> CreateProxy(const score::mw::com::InstanceSpecifier& instance_specifier)
{
    std::promise<std::vector<score::mw::com::test::BigDataProxy::HandleType>> service_discovery_promise{};
    auto service_discovery_future = service_discovery_promise.get_future();
    auto handles_result = score::mw::com::test::BigDataProxy::StartFindService(
        [moved_service_discovery_promise = std::move(service_discovery_promise)](auto handles, auto handle) mutable {
            moved_service_discovery_promise.set_value(handles);
            score::mw::com::test::BigDataProxy::StopFindService(handle);
        },
        std::move(instance_specifier));
    if (!handles_result.has_value())
    {
        std::cerr << "Error finding service for instance specifier" << instance_specifier.ToString() << ": "
                  << handles_result.error().Message() << ", terminating." << std::endl;
        return score::MakeUnexpected<score::mw::com::test::BigDataProxy>(handles_result.error());
    }

    const auto handles = service_discovery_future.get();
    if (handles.empty())
    {
        std::cerr << "NO instance found for instance specifier" << instance_specifier.ToString()
                  << " although service instance has been successfully offered! Terminating!" << std::endl;
        return score::MakeUnexpected<score::mw::com::test::BigDataProxy>(
            score::mw::com::impl::MakeError(score::mw::com::impl::ComErrc::kServiceNotAvailable));
    }

    return score::mw::com::test::BigDataProxy::Create(handles.front());
}

/**
 * Test that checks that there is no deadlock in the following situation:
 * - Proxy side has an EventReceiveHandler set for a given event.
 * - the EventReceiveHandler calls GetSubscriptionState()
 * - Proxy side calls Unsubscribe for the given event, while concurrently the EventReceiveHandler is being called.
 *
 * Background: We had a deadlock in this situation before, because of the following sequence:
 * 1. Thread/context, which calls Unsubscribe() takes a LOCK on subscription state machine mutex.
 * 2. before this thread is able to acquire a WRITE-LOCK on the mutex, which protects the receive-handler map ...
 * 3. in another thread an event-update-notification takes place and triggers a user-provided EventReceiveHandler
 * 4. the execution of this EventReceiveHandler happens under READ-LOCK of receive-handler map.
 * 5. EventReceiveHandler calls GetSubscriptionState(), which tries to acquire a LOCK on subscription state machine
 *    mutex, but the LOCK is already held by thread/context, which calls Unsubscribe() see 1.
 * Summary: Thread/context, which calls Unsubscribe() holds LOCK of subscription state machine mutex and wants
 * to acquire WRITE-LOCK on the mutex, which protects the receive-handler map. But this lock is already held by
 * thread handling event-update-notification see 3, which can not proceed either, because it waits on the lock of
 * subscription state machine mutex -> DEADLOCK
 */
int main(int argc, const char** argv)
{
    score::mw::com::test::SetupAssertHandler();
    score::cpp::stop_source stop_source;
    const bool sig_term_handler_setup_success = score::mw::com::SetupStopTokenSigTermHandler(stop_source);
    if (!sig_term_handler_setup_success)
    {
        std::cerr << "Unable to set signal handler for SIGINT and/or SIGTERM, cautiously continuing\n";
        return EXIT_FAILURE;
    }

    score::mw::com::runtime::InitializeRuntime(argc, argv);

    const auto instance_specifier_result =
        score::mw::com::InstanceSpecifier::Create(std::string{"score/cp60/MapApiLanesStamped"});
    if (!instance_specifier_result.has_value())
    {
        std::cerr << "Invalid instance specifier, terminating." << std::endl;
        return EXIT_FAILURE;
    }
    const auto& instance_specifier = instance_specifier_result.value();

    // Create skeleton and offer its service ...
    auto bigdata_result = score::mw::com::test::BigDataSkeleton::Create(instance_specifier);
    if (!bigdata_result.has_value())
    {
        std::cerr << "Could not create skeleton with instance specifier" << instance_specifier.ToString()
                  << ", terminating." << std::endl;
        return EXIT_FAILURE;
    }
    auto& skeleton = bigdata_result.value();
    const auto offer_service_result = skeleton.OfferService();
    if (!offer_service_result.has_value())
    {
        std::cerr << "Could not offer service for skeleton with instance specifier" << instance_specifier.ToString()
                  << ", terminating." << std::endl;
        return EXIT_FAILURE;
    }

    // Create proxy in the same process for the given service instance offered above
    auto proxy_result = CreateProxy(instance_specifier);
    if (!proxy_result.has_value())
    {
        std::cerr << "Could not find/create proxy, terminating." << std::endl;
        return EXIT_FAILURE;
    }

    // Subscribe to the event and register an EventReceiveHandler
    auto& proxy = proxy_result.value();
    auto subscribe_result = proxy.map_api_lanes_stamped_.Subscribe(1);
    if (!subscribe_result.has_value())
    {
        std::cerr << "Proxy error subscribing to event: " << subscribe_result.error() << ", terminating." << std::endl;
        return EXIT_FAILURE;
    }

    score::concurrency::Notification call_get_subscription_state_notification{};
    proxy.map_api_lanes_stamped_.SetReceiveHandler([&proxy, &call_get_subscription_state_notification, &stop_source]() {
        std::cout << "Proxy received event! Waiting for Notification to call GetSubscriptionState()" << std::endl;
        const bool call_get_subscription_state =
            call_get_subscription_state_notification.waitWithAbort(stop_source.get_token());
        if (call_get_subscription_state)
        {
            std::cout << "Proxy calling GetSubscriptionState()" << std::endl;
            proxy.map_api_lanes_stamped_.GetSubscriptionState();
            std::cout << "Proxy GetSubscriptionState() returned" << std::endl;
        }
        else
        {
            std::cerr << "Waiting for Notification to call GetSubscriptionState() has been aborted!" << std::endl;
        }
    });

    // Sending an event update here triggers the event-receive-handler registered above, which will acquire the
    // read-lock on receive-handler map!
    skeleton.map_api_lanes_stamped_.Send(kEvent_sample);

    // Make a deferred notification to call_get_subscription_state_notification, where the registered
    // event-receive-handler waits for! It will get woken up by this notification and then call GetSubscriptionState(),
    // which tries to acquire subscription-state-machine lock! We make it deferred, so that the call to Unsubscribe()
    // below is overtaking and acquiring the subscription-state-machine lock first!
    using namespace std::chrono_literals;
    score::cpp::jthread notify_thread{[&call_get_subscription_state_notification]() {
        std::this_thread::sleep_for(5000ms);
        call_get_subscription_state_notification.notify();
    }};

    score::cpp::stop_callback stop_callback{stop_source.get_token(), [&notify_thread]() noexcept -> void {
                                         notify_thread.native_handle();
                                     }};

    // call to Unsubscribe() will acquiring the subscription-state-machine lock and then get blocked on the
    // read-lock on receive-handler map ...
    std::this_thread::sleep_for(2000ms);
    std::cout << "Proxy calling Unsubscribe()" << std::endl;
    proxy.map_api_lanes_stamped_.Unsubscribe();
    std::cout << "Proxy Unsubscribe returned" << std::endl;

    return EXIT_SUCCESS;
}
