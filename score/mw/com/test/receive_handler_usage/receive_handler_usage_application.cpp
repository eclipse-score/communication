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

#include "score/mw/com/test/receive_handler_usage/receive_handler_usage_application.h"

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
#include <iostream>

namespace
{

const score::mw::com::test::MapApiLanesStamped kEvent_sample{};
const std::size_t kSampleSubscriptionCount{1};
enum class ReceiveHandlerStatus : std::uint8_t
{
    PENDING = 0,
    FINISHED_OK,
    FINISHED_ERROR
};
struct ReceiveHandlerCtrl
{
    score::concurrency::Notification finished_notification;
    ReceiveHandlerStatus status{ReceiveHandlerStatus::PENDING};
};

score::Result<score::mw::com::test::BigDataSkeleton> CreateAndOfferSkeleton(
    const score::mw::com::InstanceSpecifier& instance_specifier)
{

    auto bigdata_result = score::mw::com::test::BigDataSkeleton::Create(instance_specifier);
    if (!bigdata_result.has_value())
    {
        std::cerr << "Could not create skeleton with instance specifier" << instance_specifier.ToString() << std::endl;
        return bigdata_result;
    }
    const auto offer_service_result = bigdata_result->OfferService();
    if (!offer_service_result.has_value())
    {
        std::cerr << "Could not offer service for skeleton with instance specifier" << instance_specifier.ToString()
                  << std::endl;
        score::MakeUnexpected<score::mw::com::test::BigDataSkeleton>(
            score::mw::com::impl::MakeError(score::mw::com::impl::ComErrc::kServiceNotOffered));
    }
    return bigdata_result;
}

score::Result<score::mw::com::test::BigDataProxy> CreateProxy(const score::mw::com::InstanceSpecifier& instance_specifier)
{
    auto handles_result = score::mw::com::test::BigDataProxy::FindService(instance_specifier);
    if (!handles_result.has_value())
    {
        std::cerr << "Error finding service for instance specifier" << instance_specifier.ToString() << ": "
                  << handles_result.error().Message() << ", terminating." << std::endl;
        return score::MakeUnexpected<score::mw::com::test::BigDataProxy>(handles_result.error());
    }

    if (handles_result.value().empty())
    {
        std::cerr << "NO instance found for instance specifier" << instance_specifier.ToString()
                  << " although service instance has been successfully offered! Terminating!" << std::endl;
        return score::MakeUnexpected<score::mw::com::test::BigDataProxy>(
            score::mw::com::impl::MakeError(score::mw::com::impl::ComErrc::kServiceNotAvailable));
    }

    return score::mw::com::test::BigDataProxy::Create(handles_result.value().front());
}

bool ReceiveHandlerActions(score::mw::com::test::BigDataProxy& proxy)
{
    bool success{true};
    std::cout << "EventReceiveHandler being called." << std::endl;

    std::cout << "Calling GetSubscriptionState()" << std::endl;
    score::cpp::ignore = proxy.map_api_lanes_stamped_.GetSubscriptionState();
    std::cout << "GetSubscriptionState() returned successfully." << std::endl;

    std::cout << "Proxy calling GetFreeSampleCount()" << std::endl;
    auto free_sample_count = proxy.map_api_lanes_stamped_.GetFreeSampleCount();
    if (free_sample_count != kSampleSubscriptionCount)
    {
        std::cerr << "GetFreeSampleCount() returned: " << free_sample_count
                  << ", but expected: " << kSampleSubscriptionCount << std::endl;
        success = false;
    }
    else
    {
        std::cout << "Proxy GetSubscriptionState() returned successfully." << std::endl;
    }

    std::cout << "Proxy calling GetNumNewSamplesAvailable()" << std::endl;
    auto num_new_samples = proxy.map_api_lanes_stamped_.GetNumNewSamplesAvailable();
    if (!num_new_samples.has_value())
    {
        std::cerr << "GetNumNewSamplesAvailable() returned error: " << num_new_samples.error() << std::endl;
        success = false;
    }
    else
    {
        std::cout << "Proxy GetNumNewSamplesAvailable() returned successfully." << std::endl;
    }

    std::cout << "Proxy calling GetNewSamples()" << std::endl;
    auto samples_received = proxy.map_api_lanes_stamped_.GetNewSamples(
        [&success](score::mw::com::SamplePtr<score::mw::com::test::MapApiLanesStamped> sample) {
            if (!sample)
            {
                std::cerr << "GetNewSamples() provided invalid sample in callback." << std::endl;
                success = false;
            }
        },
        kSampleSubscriptionCount);
    if (!samples_received.has_value())
    {
        std::cerr << "GetNewSamples() returned error: " << samples_received.error() << std::endl;
        success = false;
    }
    else
    {
        std::cout << "Proxy GetNewSamples() returned successfully." << std::endl;
    }

    std::cout << "Proxy calling UnsetReceiveHandler()" << std::endl;
    auto unset_result = proxy.map_api_lanes_stamped_.UnsetReceiveHandler();
    if (!unset_result.has_value())
    {
        std::cerr << "UnsetReceiveHandler() returned error: " << unset_result.error() << std::endl;
        success = false;
    }
    else
    {
        std::cout << "Proxy UnsetReceiveHandler() returned successfully." << std::endl;
    }

    std::cout << "Proxy calling Unsubscribe()" << std::endl;
    proxy.map_api_lanes_stamped_.Unsubscribe();

    return success;
}

}  // namespace

/**
 * Test that checks that certain mw::com/LoLa APIs can be called from an user provided EventReceiveHandler successfully.
 * I.e., without returning errors or leading to a deadlock.
 * Specifically, that there are no deadlocks/errors in the following situation:
 * - Proxy side has an EventReceiveHandler set for a given event/field.
 * - EventReceiveHandler implementation does one of the following API calls:
 *   - UnsetReceiveHandler() -> unregistering the EventReceiveHandler currently being executed
 *   - Unsubscribe() -> unsubscribing from the event/field for which the EventReceiveHandler is currently executed
 *   - GetSubscriptionState() -> querying the current subscription state of the event/field for which the
 *     EventReceiveHandler is currently executed.
 *   - GetFreeSampleCount()
 *   - GetNumNewSamplesAvailable()
 *   - GetNewSamples()
 * - Proxy side ReceiveHandler gets triggered (by an event-update-notification.
 *
 * In a nutshell: The APIs listed above, shall be supported to be used from within an EventReceiveHandler!
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

    // This allows us more flexibility as we can hand over "-service_instance_manifest /path/to/mw_com_config.json
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
    auto skeleton_result = CreateAndOfferSkeleton(instance_specifier);
    if (!skeleton_result.has_value())
    {
        return EXIT_FAILURE;
    }
    auto& skeleton = skeleton_result.value();

    // Create proxy in the same process for the given service instance offered above
    auto proxy_result = CreateProxy(instance_specifier);
    if (!proxy_result.has_value())
    {
        std::cerr << "Could not find/create proxy, terminating." << std::endl;
        return EXIT_FAILURE;
    }

    // Subscribe to the event and register an EventReceiveHandler
    auto& proxy = proxy_result.value();
    const auto subscribe_result = proxy.map_api_lanes_stamped_.Subscribe(kSampleSubscriptionCount);
    if (!subscribe_result.has_value())
    {
        std::cerr << "Proxy error subscribing to event: " << subscribe_result.error() << ", terminating." << std::endl;
        return EXIT_FAILURE;
    }

    ReceiveHandlerCtrl receive_handler_ctrl{};
    proxy.map_api_lanes_stamped_.SetReceiveHandler([&proxy, &receive_handler_ctrl]() {
        auto result = ReceiveHandlerActions(proxy);
        receive_handler_ctrl.status = result ? ReceiveHandlerStatus::FINISHED_OK : ReceiveHandlerStatus::FINISHED_ERROR;
        receive_handler_ctrl.finished_notification.notify();
    });

    // Sending an event update here triggers the event-receive-handler registered above
    // We do this in a separate thread as the Send() call might theoretically be calling the
    // EventReceiveHandler synchronously as skeleton/proxy are in the same process here. Then we would already block/
    // deadlock in the Send() call, which we want to avoid. Note, that in our current implementation, we are ALWAYS
    // detaching EventReceiveHandler calls from local skeleton event-updates via a thread pool, but this implementation
    // decision might change, and we do not want to depend on it here.
    using namespace std::chrono_literals;
    score::cpp::jthread send_thread{[&skeleton]() noexcept {
        skeleton.map_api_lanes_stamped_.Send(kEvent_sample);
    }};

    // Wait max 5 seconds for the EventReceiveHandler to finish
    std::cout << "Waiting for EventReceiveHandler to finish." << std::endl;
    const auto notification_received =
        receive_handler_ctrl.finished_notification.waitForWithAbort(5000ms, stop_source.get_token());

    if (notification_received)
    {
        std::cout << "EventReceiveHandler finished with: ";
        if (receive_handler_ctrl.status == ReceiveHandlerStatus::FINISHED_OK)
        {
            std::cout << "SUCCESS!" << std::endl;
            return EXIT_SUCCESS;
        }
        else
        {
            std::cout << "ERROR!" << std::endl;
            // Doing a hard terminate here as a normal return would try to call the destructors in main thread, but in
            // case we are deadlocking in EventReceiveHandler, there is also a high likelihood, that our proxy dtor
            // might be blocked on the deadlock.
            std::terminate();
        }
    }
    else
    {
        std::cout << "ERROR: EventReceiveHandler didn't finish" << std::endl << std::flush;
        // Reason for terminate() instead of "normal return" -> see above!
        std::terminate();
    }
}
