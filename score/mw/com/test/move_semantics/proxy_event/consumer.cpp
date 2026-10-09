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
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/proxy_container.h"
#include "score/mw/com/test/common_test_resources/proxy_event_receiver.h"
#include "score/mw/com/test/common_test_resources/proxy_event_state_change_notifier.h"
#include "score/mw/com/test/move_semantics/proxy_event/proxy_event_move_semantics_interface.h"
#include "score/mw/com/test/move_semantics/proxy_event/test_parameters.h"
#include "score/mw/com/types.h"

#include <cstdint>
#include <string>
#include <utility>

namespace score::mw::com::test
{
namespace
{

const std::string kConsumerBatchReceivedNotificationShmPath{
    "/proxy_event_move_semantics_consumer_batch_received_notification"};
const std::string kReofferTriggerNotificationShmPath{"/proxy_event_move_semantics_provider_withdraw_notification"};

void CheckIfProxyIsFullyFunctional(ProxyMoveSemanticsProxy& proxy,
                                   ProcessSynchronizer& process_synchronizer,
                                   ProcessSynchronizer& reoffer_trigger_synchronizer,
                                   const std::size_t num_samples_to_receive,
                                   const score::cpp::stop_token& stop_token)
{
    // Step 3. Register receive handler
    std::cout << "\nConsumer: Step 3 - Register receive handler" << std::endl;
    ProxyEventReceiver proxy_event_receiver{proxy.moved_event_};

    // Step 4. Register state change handler
    std::cout << "\nConsumer: Step 4 - Register state change handler" << std::endl;
    ProxyEventStateChangeNotifier proxy_event_state_change_notifier{proxy.moved_event_};

    // Step 6. Receive the first batch and notify the provider when done.
    std::cout << "\nConsumer: Step 6 - Receive first batch of samples" << std::endl;
    if (!proxy_event_state_change_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest("proxy_event_move_semantics consumer failed: WaitForStateChange was interrupted by stop_token");
    }
    if (!proxy_event_receiver.WaitForSamples(stop_token, kInitialSamplesToSend))
    {
        FailTest("proxy_event_move_semantics consumer failed: WaitForSamples was interrupted by stop_token");
    }
    process_synchronizer.Notify();

    // Step 7. Unsubscribe and subscribe again
    // std::cout << "\nConsumer: Step 7 - Unsubscribe and subscribe again" << std::endl;
    // proxy.moved_event_.Unsubscribe();
    // std::cout << "\nConsumer:Current Subscription state after calling kNotSubscribed:"
    //           << static_cast<std::uint32_t>(proxy.moved_event_.GetSubscriptionState()) << std::endl;
    // if (!proxy_event_state_change_notifier.WaitForStateChange(stop_token, SubscriptionState::kNotSubscribed))
    // {
    //     FailTest("proxy_event_move_semantics consumer failed: WaitForStateChange was interrupted by stop_token");
    // }
    // proxy.moved_event_.Subscribe(num_samples_to_receive);
    // if (!proxy_event_state_change_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    // {
    //     FailTest("proxy_event_move_semantics consumer failed: WaitForStateChange was interrupted by stop_token");
    // }

    // Step 8.  Notify provider to re-offer the service
    std::cout << "\nConsumer: Step 8 - Notify provider to re-offer the service." << std::endl;
    reoffer_trigger_synchronizer.Notify();

    // Step 9. wait for the state to change to kSubscriptionPending
    std::cout << "\nConsumer: Step 9 - Wait for the state to change to kSubscriptionPending" << std::endl;
    std::cout
        << "\nConsumer: Current Subscription state during waiting for the state to change to kSubscriptionPending:"
        << static_cast<std::uint32_t>(proxy.moved_event_.GetSubscriptionState()) << std::endl;
    if (!proxy_event_state_change_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscriptionPending))
    {
        FailTest("proxy_event_move_semantics consumer failed: WaitForStateChange was interrupted by stop_token");
    }

    // Step 10. Wait for the state to change to kSubscribed
    std::cout << "\nConsumer: Step 10 - Wait for the state to change to kSubscribed" << std::endl;
    std::cout << "\nConsumer: Current Subscription state before wait:"
              << static_cast<std::uint32_t>(proxy.moved_event_.GetSubscriptionState()) << std::endl;
    if (!proxy_event_state_change_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest("proxy_event_move_semantics consumer failed: WaitForStateChange was interrupted by stop_token");
    }
    std::cout << "Consumer: Step 10 - current subscription state after wait = "
              << static_cast<std::uint32_t>(proxy.moved_event_.GetSubscriptionState()) << std::endl;

    // step 11. Wait for the newSamples
    std::cout << "\nConsumer: Step 11 - Wait for the new samples" << std::endl;
    std::cout << "\nConsumer: expected second batch of samples: " << kSamplesToSendAfterReOffer.size() << std::endl;
    if (!proxy_event_receiver.WaitForSamples(stop_token, kSamplesToSendAfterReOffer))
    {
        FailTest("proxy_event_move_semantics consumer failed: WaitForSamples was interrupted by stop_token");
    }
    std::cout << "\nConsumer: Received the second batch of samples successfully" << std::endl;
}

void RunConsumerMoveConstructProxyBeforeSubscribe(ProcessSynchronizer& process_synchronizer,
                                                  ProcessSynchronizer& reoffer_trigger_synchronizer,
                                                  const std::size_t num_samples_to_receive,
                                                  const score::cpp::stop_token& stop_token)
{
    ProxyContainer<ProxyMoveSemanticsProxy> proxy_container{};

    // Step 1. Find service and create original proxy
    std::cout << "\nConsumer: Step 1 - Find service and create proxy" << std::endl;
    proxy_container.CreateProxy(kInstanceSpecifierMovedTo, "proxy_event_move_semantics");

    // Step 2. Move construct proxy before subscribe
    std::cout << "\nConsumer: Step 2 - Move construct proxy before subscribe" << std::endl;
    auto original_proxy = std::move(proxy_container).Extract();

    // Step 5. Subscribe
    std::cout << "\nConsumer: Step 5 - Subscribe" << std::endl;
    auto subscribe_result = original_proxy.moved_event_.Subscribe(num_samples_to_receive);
    if (!subscribe_result.has_value())
    {
        FailTest("proxy_event_move_semantics consumer failed: Subscribe failed: ", subscribe_result.error());
    }

    CheckIfProxyIsFullyFunctional(
        original_proxy, process_synchronizer, reoffer_trigger_synchronizer, num_samples_to_receive, stop_token);

    // Step 12. Notify to provider we're done.
    std::cout << "\nConsumer: Step 12 - Notify provider we're done." << std::endl;
    process_synchronizer.Notify();
    std::cout << "Consumer: Done with all iterations, exiting" << std::endl;
}

void RunConsumerMoveConstructProxyWhileSubscribed(ProcessSynchronizer& process_synchronizer,
                                                  ProcessSynchronizer& reoffer_trigger_synchronizer,
                                                  const std::size_t num_samples_to_receive,
                                                  const score::cpp::stop_token& stop_token)
{
    ProxyContainer<ProxyMoveSemanticsProxy> proxy_container{};

    // Step 1. Find service and create original proxy
    std::cout << "\nConsumer: Step 1 - Find service and create original proxy" << std::endl;
    proxy_container.CreateProxy(kInstanceSpecifierMovedTo, "proxy_event_move_semantics");
    auto& original_proxy = proxy_container.GetProxy();

    // Step 2. Subscribe on original proxy
    std::cout << "\nConsumer: Step 2 - Subscribe on original proxy" << std::endl;
    auto subscribe_result = original_proxy.moved_event_.Subscribe(num_samples_to_receive);
    if (!subscribe_result.has_value())
    {
        FailTest("proxy_event_move_semantics consumer failed: Subscribe failed: ", subscribe_result.error());
    }

    // Step 5. Move construct while subscribed. The receive and state-change handlers are preserved on the moved-to
    // proxy, so no re-registration is needed.
    std::cout << "\nConsumer: Step 5 - Move construct while subscribed" << std::endl;
    auto moved_proxy = std::move(proxy_container).Extract();

    CheckIfProxyIsFullyFunctional(
        moved_proxy, process_synchronizer, reoffer_trigger_synchronizer, num_samples_to_receive, stop_token);
}

void RunConsumerMoveAssignProxyBeforeSubscribe(ProcessSynchronizer& process_synchronizer,
                                               ProcessSynchronizer& reoffer_trigger_synchronizer,
                                               const std::size_t num_samples_to_receive,
                                               const score::cpp::stop_token& stop_token)
{
    ProxyContainer<ProxyMoveSemanticsProxy> moved_from_proxy_container{};
    ProxyContainer<ProxyMoveSemanticsProxy> moved_to_proxy_container{};

    // Step 1. Find service and create original proxy
    std::cout << "\nConsumer: Step 1 - Find service and create original proxy" << std::endl;
    moved_from_proxy_container.CreateProxy(kInstanceSpecifierMovedTo, "proxy_event_move_semantics");
    std::cout << "\nConsumer: Step 2 - Find service and create proxy" << std::endl;
    moved_to_proxy_container.CreateProxy(kInstanceSpecifierMovedFrom, "proxy_event_move_semantics");

    auto& moved_from_proxy = moved_from_proxy_container.GetProxy();
    auto& moved_to_proxy = moved_to_proxy_container.GetProxy();

    // Step 3. Move assign proxy = move(original proxy) before subscribe
    std::cout << "\nConsumer: Step 3 - Move assign proxy = move(original proxy) before subscribe" << std::endl;
    moved_to_proxy = std::move(moved_from_proxy_container).Extract();

    // Step 5. Subscribe
    std::cout << "\nConsumer: Step 5 - Subscribe on proxy" << std::endl;
    auto subscribe_result = moved_to_proxy.moved_event_.Subscribe(num_samples_to_receive);
    if (!subscribe_result.has_value())
    {
        FailTest("proxy_event_move_semantics consumer failed: Subscribe failed: ", subscribe_result.error());
    }

    CheckIfProxyIsFullyFunctional(
        moved_to_proxy, process_synchronizer, reoffer_trigger_synchronizer, num_samples_to_receive, stop_token);

    // Step 12. Notify to provider we're done.
    std::cout << "\nConsumer: Step 12 - Notify provider we're done." << std::endl;
    process_synchronizer.Notify();
    std::cout << "Consumer: Done with all iterations, exiting" << std::endl;
}

void RunConsumerMoveAssignProxyWhileSubscribed(ProcessSynchronizer& process_synchronizer,
                                               ProcessSynchronizer& reoffer_trigger_synchronizer,
                                               const std::size_t num_samples_to_receive,
                                               const score::cpp::stop_token& stop_token)
{
    ProxyContainer<ProxyMoveSemanticsProxy> moved_from_proxy_container{};
    ProxyContainer<ProxyMoveSemanticsProxy> moved_to_proxy_container{};

    // Step 1. Create two proxies
    std::cout << "\nConsumer: Step 1 - Find service and create active proxy" << std::endl;
    moved_from_proxy_container.CreateProxy(kInstanceSpecifierMovedTo, "proxy_event_move_semantics");
    std::cout << "\nConsumer: Step 2 - Find service and create passive proxy" << std::endl;
    moved_to_proxy_container.CreateProxy(kInstanceSpecifierMovedFrom, "proxy_event_move_semantics");

    auto& moved_from_proxy = moved_from_proxy_container.GetProxy();
    auto& moved_to_proxy = moved_to_proxy_container.GetProxy();

    // Step 3. Subscribe
    std::cout << "\nConsumer: Step 3 - Subscribe active proxy" << std::endl;
    auto subscribe_result = moved_from_proxy.moved_event_.Subscribe(num_samples_to_receive);
    if (!subscribe_result.has_value())
    {
        FailTest("proxy_event_move_semantics consumer failed: Subscribe failed: ", subscribe_result.error());
    }

    // Step 6. Move assign while active. The receive and state-change handlers are preserved on the moved-to proxy, so
    // no re-registration is needed.
    std::cout << "\nConsumer: Step 6 - Move assign while active" << std::endl;
    moved_to_proxy = std::move(moved_from_proxy_container).Extract();

    CheckIfProxyIsFullyFunctional(
        moved_to_proxy, process_synchronizer, reoffer_trigger_synchronizer, num_samples_to_receive, stop_token);
}
}  // namespace

void RunConsumer(const ProxyMoveScenario& scenario,
                 const std::size_t num_samples_to_receive,
                 const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kConsumerBatchReceivedNotificationShmPath);
    if (!process_synchronizer_result.has_value())
    {
        FailTest("proxy_event_move_semantics consumer failed: could not create ready synchronizer");
    }

    ExitFunctionGuard done_guard{[&process_synchronizer_result]() {
        process_synchronizer_result->Notify();
    }};

    auto reoffer_trigger_synchronizer_result = ProcessSynchronizer::Create(kReofferTriggerNotificationShmPath);
    if (!reoffer_trigger_synchronizer_result.has_value())
    {
        FailTest("proxy_event_move_semantics consumer failed: could not create reoffer trigger synchronizer");
    }

    auto& process_synchronizer = *process_synchronizer_result;
    auto& reoffer_trigger_synchronizer = *reoffer_trigger_synchronizer_result;

    switch (scenario)
    {
        case ProxyMoveScenario::kMoveConstructBeforeSubscribe:
        {
            RunConsumerMoveConstructProxyBeforeSubscribe(
                process_synchronizer, reoffer_trigger_synchronizer, num_samples_to_receive, stop_token);
            break;
        }
        case ProxyMoveScenario::kMoveConstructWhileSubscribed:
        {
            RunConsumerMoveConstructProxyWhileSubscribed(
                process_synchronizer, reoffer_trigger_synchronizer, num_samples_to_receive, stop_token);
            break;
        }
        case ProxyMoveScenario::kMoveAssignBeforeSubscribe:
        {
            RunConsumerMoveAssignProxyBeforeSubscribe(
                process_synchronizer, reoffer_trigger_synchronizer, num_samples_to_receive, stop_token);
            break;
        }
        case ProxyMoveScenario::kMoveAssignWhileSubscribed:
        {
            RunConsumerMoveAssignProxyWhileSubscribed(
                process_synchronizer, reoffer_trigger_synchronizer, num_samples_to_receive, stop_token);
            break;
        }
        case ProxyMoveScenario::kNumberOfScenarios:
            [[fallthrough]];
        default:
            FailTest("Unknown proxy move scenario in consumer");
    }
}

}  // namespace score::mw::com::test
