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
#include "score/mw/com/test/move_semantics/proxy_event/test_event_datatype.h"
#include "score/mw/com/types.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/proxy_event_move_semantics_interprocess_notification"};
const std::string kProviderWithdrawNotificationShmPath{"/proxy_event_move_semantics_provider_withdraw_notification"};

auto MakeSampleSequenceCallback(std::optional<std::uint32_t>& latest_value, const char* failure_message_prefix)
{
    return [&latest_value, failure_message_prefix](SamplePtr<std::uint32_t> sample) {
        if (sample == nullptr)
        {
            FailTest(failure_message_prefix, " received null sample");
        }
        // After a reset latest_value is empty, so the next sample is accepted as the new baseline.
        const std::uint32_t expected_value = latest_value.has_value() ? latest_value.value() + 1U : *sample;
        if (*sample != expected_value)
        {
            FailTest(
                failure_message_prefix, " received value ", *sample, " does not match expected value ", expected_value);
        }
        latest_value = *sample;
    };
}

void RunConsumerMoveConstructProxyBeforeSubscribe(ProcessSynchronizer& process_synchronizer,
                                                  ProcessSynchronizer& provider_withdraw_synchronizer,
                                                  const std::size_t num_samples_to_receive,
                                                  const std::size_t num_send_iterations,
                                                  const score::cpp::stop_token& stop_token)
{
    ProxyContainer<ProxyMoveSemanticsProxy> proxy_container{};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1 - Find service and create proxy" << std::endl;
    proxy_container.CreateProxy(kInstanceSpecifierMovedTo, "proxy_event_move_semantics");
    auto original_proxy = proxy_container.Extract();

    // Step 2. Move construct proxy before subscribe
    std::cout << "\nConsumer: Step 2 - Move construct proxy before subscribe" << std::endl;
    auto moved_proxy = std::move(original_proxy);

    // Step 3. Register receive handler
    std::cout << "\nConsumer: Step 3 - Register receive handler" << std::endl;
    std::optional<std::uint32_t> latest_value{0U};
    ProxyEventReceiver proxy_event_receiver{
        moved_proxy.moved_event_,
        MakeSampleSequenceCallback(latest_value, "proxy_event_move_semantics consumer failed:")};

    // Step 4. Register state change handler
    std::cout << "\nConsumer: Step 4 - Register state change handler" << std::endl;
    ProxyEventStateChangeNotifier proxy_state_change_notifier{moved_proxy.moved_event_};

    // Step 5. Subscribe
    std::cout << "\nConsumer: Step 5 - Subscribe" << std::endl;
    auto subscribe_result = moved_proxy.moved_event_.Subscribe(num_samples_to_receive);
    if (!subscribe_result.has_value())
    {
        FailTest("proxy_event_move_semantics consumer failed: Subscribe failed: ", subscribe_result.error());
    }
    // Notify provider that subscription and handler registration are complete so it can start sending.
    // Without this the provider might send the entire first batch before Subscribe() is called; LoLa SHM
    // does not deliver pre-subscription samples, so GetNewSamples would return 0 forever.
    process_synchronizer.Notify();

    // Step 6. Receive batches of samples and notify the provider after each one
    std::cout << "\nConsumer: Step 6 - Receive first batches of samples" << std::endl;
    for (std::size_t iteration = 0U; iteration < num_send_iterations - 1U; ++iteration)
    {
        std::cout << "\nConsumer: Iteration " << (iteration + 1U) << " of " << (num_send_iterations - 1U) << std::endl;
        if (!proxy_state_change_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
        {
            FailTest("proxy_event_move_semantics consumer failed: WaitForStateChange was interrupted by stop_token");
        }
        if (!proxy_event_receiver.WaitForSamples(stop_token, num_samples_to_receive))
        {
            FailTest("proxy_event_move_semantics consumer failed: WaitForSamples was interrupted by stop_token");
        }
        process_synchronizer.Notify();
    }

    // Step 7. Unsubscribe and subscribe again across the provider's re-offer
    std::cout << "\nConsumer: Step 7 - Unsubscribe and subscribe again" << std::endl;
    std::cout << "\nConsumer: Waiting for provider to withdraw its offer" << std::endl;
    if (!provider_withdraw_synchronizer.WaitWithAbort(stop_token))
    {
        FailTest(
            "proxy_event_move_semantics consumer failed: Waiting for provider withdrawal was interrupted by "
            "stop_token");
    }
    provider_withdraw_synchronizer.Reset();
    moved_proxy.moved_event_.Unsubscribe();
    const auto resubscribe_result = moved_proxy.moved_event_.Subscribe(num_samples_to_receive);
    if (!resubscribe_result.has_value())
    {
        FailTest("proxy_event_move_semantics consumer failed: Re-subscribe failed: ", resubscribe_result.error());
    }
    // Tell the provider we have re-subscribed so that it can re-offer the service.
    process_synchronizer.Notify();
    // A fresh subscription replays the buffered samples, so accept the next sample as the new baseline.
    latest_value.reset();

    // Step 8. Receive the remaining batch after re-subscribing
    std::cout << "\nConsumer: Step 8 - Receive samples again after re-subscribing" << std::endl;
    if (!proxy_state_change_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest("proxy_event_move_semantics consumer failed: WaitForStateChange was interrupted by stop_token");
    }
    if (!proxy_event_receiver.WaitForSamples(stop_token, num_samples_to_receive))
    {
        FailTest("proxy_event_move_semantics consumer failed: WaitForSamples was interrupted by stop_token");
    }
    process_synchronizer.Notify();

    std::cout << "Consumer: Done with all iterations, exiting" << std::endl;
}

void RunConsumerMoveConstructProxyWhileSubscribed(ProcessSynchronizer& process_synchronizer,
                                                  ProcessSynchronizer& provider_withdraw_synchronizer,
                                                  const std::size_t num_samples_to_receive,
                                                  const std::size_t num_send_iterations,
                                                  const score::cpp::stop_token& stop_token)
{
    ProxyContainer<ProxyMoveSemanticsProxy> proxy_container{};

    // Declare the move destination before the monitoring helpers so that it is destroyed after them.
    // This guarantees the helpers' destructors (which call Unset on moved_proxy->moved_event_) run while
    // the moved proxy is still alive.
    std::optional<ProxyMoveSemanticsProxy> moved_proxy{};

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

    // Step 3. Register handlers on original proxy. The handlers must not be destroyed before the move and must
    // not be re-created afterwards, so that we verify they are preserved on the moved-to proxy.
    std::cout << "\nConsumer: Step 3 - Register handlers on original proxy" << std::endl;
    std::optional<std::uint32_t> latest_value{0U};
    ProxyEventReceiver proxy_event_receiver{
        original_proxy.moved_event_,
        MakeSampleSequenceCallback(latest_value, "proxy_event_move_semantics consumer failed:")};
    ProxyEventStateChangeNotifier proxy_state_change_notifier{original_proxy.moved_event_};
    // Notify provider that subscription and handler registration are complete so it can start sending.
    // Without this the provider might send the entire first batch before Subscribe() is called; LoLa SHM
    // does not deliver pre-subscription samples, so GetNewSamples would return 0 forever.
    process_synchronizer.Notify();

    // Step 4. Receive first batch of samples on original proxy
    std::cout << "\nConsumer: Step 4 - Receive first batch of samples" << std::endl;
    if (!proxy_state_change_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest("proxy_event_move_semantics consumer failed: WaitForStateChange was interrupted by stop_token");
    }
    if (!proxy_event_receiver.WaitForSamples(stop_token, num_samples_to_receive))
    {
        FailTest("proxy_event_move_semantics consumer failed: WaitForSamples was interrupted by stop_token");
    }
    process_synchronizer.Notify();

    // Step 5. Move construct while subscribed. The receive and state-change handlers are preserved on the moved-to
    // proxy, so no re-registration is needed.
    std::cout << "\nConsumer: Step 5 - Move construct while subscribed" << std::endl;
    auto proxy_before_move = proxy_container.Extract();
    moved_proxy.emplace(std::move(proxy_before_move));

    // Reattach monitoring helpers to the moved-to event. The handlers (lambdas) were transferred by the move;
    // Reattach only updates the internal reference so that GetNewSamples / GetSubscriptionState are called on
    // the correct event object without re-registering any handler.
    proxy_event_receiver.Reattach(moved_proxy->moved_event_);
    proxy_state_change_notifier.Reattach(moved_proxy->moved_event_);

    // Step 6. Continue receiving on the moved-to proxy - the handlers survived the move
    std::cout << "\nConsumer: Step 6 - Continue receiving on moved-to proxy" << std::endl;
    for (std::size_t iteration = 0U; iteration < num_send_iterations - 2U; ++iteration)
    {
        std::cout << "\nConsumer: Iteration " << (iteration + 1U) << " of " << (num_send_iterations - 2U) << std::endl;
        if (!proxy_state_change_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
        {
            FailTest("proxy_event_move_semantics consumer failed: WaitForStateChange was interrupted by stop_token");
        }
        if (!proxy_event_receiver.WaitForSamples(stop_token, num_samples_to_receive))
        {
            FailTest("proxy_event_move_semantics consumer failed: WaitForSamples was interrupted by stop_token");
        }
        process_synchronizer.Notify();
    }

    // Step 7. Unsubscribe and subscribe again across the provider's re-offer
    std::cout << "\nConsumer: Step 7 - Unsubscribe and subscribe again" << std::endl;
    std::cout << "\nConsumer: Waiting for provider to withdraw its offer" << std::endl;
    if (!provider_withdraw_synchronizer.WaitWithAbort(stop_token))
    {
        FailTest(
            "proxy_event_move_semantics consumer failed: Waiting for provider withdrawal was interrupted by "
            "stop_token");
    }
    provider_withdraw_synchronizer.Reset();
    moved_proxy->moved_event_.Unsubscribe();
    const auto resubscribe_result = moved_proxy->moved_event_.Subscribe(num_samples_to_receive);
    if (!resubscribe_result.has_value())
    {
        FailTest("proxy_event_move_semantics consumer failed: Re-subscribe failed: ", resubscribe_result.error());
    }
    // Tell the provider we have re-subscribed so that it can re-offer the service.
    process_synchronizer.Notify();
    // A fresh subscription replays the buffered samples, so accept the next sample as the new baseline.
    latest_value.reset();

    // Step 8. Receive the remaining batch after re-subscribing
    std::cout << "\nConsumer: Step 8 - Receive samples again after re-subscribing" << std::endl;
    if (!proxy_state_change_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest("proxy_event_move_semantics consumer failed: WaitForStateChange was interrupted by stop_token");
    }
    if (!proxy_event_receiver.WaitForSamples(stop_token, num_samples_to_receive))
    {
        FailTest("proxy_event_move_semantics consumer failed: WaitForSamples was interrupted by stop_token");
    }
    process_synchronizer.Notify();

    std::cout << "Consumer: Done with all iterations, exiting" << std::endl;
}

void RunConsumerMoveAssignProxyBeforeSubscribe(ProcessSynchronizer& process_synchronizer,
                                               ProcessSynchronizer& provider_withdraw_synchronizer,
                                               const std::size_t num_samples_to_receive,
                                               const std::size_t num_send_iterations,
                                               const score::cpp::stop_token& stop_token)
{
    ProxyContainer<ProxyMoveSemanticsProxy> moved_from_proxy_container{};
    ProxyContainer<ProxyMoveSemanticsProxy> moved_to_proxy_container{};

    // Step 1. Create original proxy and proxy
    std::cout << "\nConsumer: Step 1 - Find service and create original proxy" << std::endl;
    moved_from_proxy_container.CreateProxy(kInstanceSpecifierMovedTo, "proxy_event_move_semantics");
    std::cout << "\nConsumer: Step 2 - Find service and create proxy" << std::endl;
    moved_to_proxy_container.CreateProxy(kInstanceSpecifierMovedFrom, "proxy_event_move_semantics");

    auto moved_from_proxy = moved_from_proxy_container.Extract();
    auto moved_to_proxy = moved_to_proxy_container.Extract();

    // Step 3. Move assign proxy = move(original proxy) before subscribe
    std::cout << "\nConsumer: Step 3 - Move assign proxy = move(original proxy) before subscribe" << std::endl;
    moved_to_proxy = std::move(moved_from_proxy);

    // Step 4. Register handlers and subscribe on proxy
    std::cout << "\nConsumer: Step 4 - Register handlers on proxy" << std::endl;
    std::optional<std::uint32_t> latest_value{0U};
    ProxyEventReceiver proxy_event_receiver{
        moved_to_proxy.moved_event_,
        MakeSampleSequenceCallback(latest_value, "proxy_event_move_semantics consumer failed:")};
    ProxyEventStateChangeNotifier proxy_state_change_notifier{moved_to_proxy.moved_event_};

    std::cout << "\nConsumer: Step 5 - Subscribe on proxy" << std::endl;
    auto subscribe_result = moved_to_proxy.moved_event_.Subscribe(num_samples_to_receive);
    if (!subscribe_result.has_value())
    {
        FailTest("proxy_event_move_semantics consumer failed: Subscribe failed: ", subscribe_result.error());
    }
    // Notify provider that subscription and handler registration are complete so it can start sending.
    // Without this the provider might send the entire first batch before Subscribe() is called; LoLa SHM
    // does not deliver pre-subscription samples, so GetNewSamples would return 0 forever.
    process_synchronizer.Notify();

    // Step 6. Receive batches of samples and notify the provider after each one
    std::cout << "\nConsumer: Step 6 - Receive first batches of samples" << std::endl;
    for (std::size_t iteration = 0U; iteration < num_send_iterations - 1U; ++iteration)
    {
        std::cout << "\nConsumer: Iteration " << (iteration + 1U) << " of " << (num_send_iterations - 1U) << std::endl;
        if (!proxy_state_change_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
        {
            FailTest("proxy_event_move_semantics consumer failed: WaitForStateChange was interrupted by stop_token");
        }
        if (!proxy_event_receiver.WaitForSamples(stop_token, num_samples_to_receive))
        {
            FailTest("proxy_event_move_semantics consumer failed: WaitForSamples was interrupted by stop_token");
        }
        process_synchronizer.Notify();
    }

    // Step 7. Unsubscribe and subscribe again across the provider's re-offer
    std::cout << "\nConsumer: Step 7 - Unsubscribe and subscribe again" << std::endl;
    std::cout << "\nConsumer: Waiting for provider to withdraw its offer" << std::endl;
    if (!provider_withdraw_synchronizer.WaitWithAbort(stop_token))
    {
        FailTest(
            "proxy_event_move_semantics consumer failed: Waiting for provider withdrawal was interrupted by "
            "stop_token");
    }
    provider_withdraw_synchronizer.Reset();
    moved_to_proxy.moved_event_.Unsubscribe();
    const auto resubscribe_result = moved_to_proxy.moved_event_.Subscribe(num_samples_to_receive);
    if (!resubscribe_result.has_value())
    {
        FailTest("proxy_event_move_semantics consumer failed: Re-subscribe failed: ", resubscribe_result.error());
    }
    // Tell the provider we have re-subscribed so that it can re-offer the service.
    process_synchronizer.Notify();
    // A fresh subscription replays the buffered samples, so accept the next sample as the new baseline.
    latest_value.reset();

    // Step 8. Receive the remaining batch after re-subscribing
    std::cout << "\nConsumer: Step 8 - Receive samples again after re-subscribing" << std::endl;
    if (!proxy_state_change_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest("proxy_event_move_semantics consumer failed: WaitForStateChange was interrupted by stop_token");
    }
    if (!proxy_event_receiver.WaitForSamples(stop_token, num_samples_to_receive))
    {
        FailTest("proxy_event_move_semantics consumer failed: WaitForSamples was interrupted by stop_token");
    }
    process_synchronizer.Notify();

    std::cout << "Consumer: Done with all iterations, exiting" << std::endl;
}

void RunConsumerMoveAssignProxyWhileSubscribed(ProcessSynchronizer& process_synchronizer,
                                               ProcessSynchronizer& provider_withdraw_synchronizer,
                                               const std::size_t num_samples_to_receive,
                                               const std::size_t num_send_iterations,
                                               const score::cpp::stop_token& stop_token)
{
    ProxyContainer<ProxyMoveSemanticsProxy> active_proxy_container{};
    ProxyContainer<ProxyMoveSemanticsProxy> passive_proxy_container{};

    // Step 1. Create two proxies
    std::cout << "\nConsumer: Step 1 - Find service and create active proxy" << std::endl;
    active_proxy_container.CreateProxy(kInstanceSpecifierMovedTo, "proxy_event_move_semantics");
    std::cout << "\nConsumer: Step 2 - Find service and create passive proxy" << std::endl;
    passive_proxy_container.CreateProxy(kInstanceSpecifierMovedFrom, "proxy_event_move_semantics");

    auto active_proxy = active_proxy_container.Extract();
    auto passive_proxy = passive_proxy_container.Extract();

    // Step 3. Subscribe active proxy and receive first batch
    std::cout << "\nConsumer: Step 3 - Subscribe active proxy" << std::endl;
    auto subscribe_result = active_proxy.moved_event_.Subscribe(num_samples_to_receive);
    if (!subscribe_result.has_value())
    {
        FailTest("proxy_event_move_semantics consumer failed: Subscribe failed: ", subscribe_result.error());
    }

    // Step 4. Register handlers on active proxy. The handlers must not be destroyed before the move and must
    // not be re-created afterwards, so that we verify they are preserved on the moved-to proxy.
    std::cout << "\nConsumer: Step 4 - Register handlers on active proxy" << std::endl;
    std::optional<std::uint32_t> latest_value{0U};
    ProxyEventReceiver proxy_event_receiver{
        active_proxy.moved_event_,
        MakeSampleSequenceCallback(latest_value, "proxy_event_move_semantics consumer failed:")};
    ProxyEventStateChangeNotifier proxy_state_change_notifier{active_proxy.moved_event_};
    // Notify provider that subscription and handler registration are complete so it can start sending.
    // Without this the provider might send the entire first batch before Subscribe() is called; LoLa SHM
    // does not deliver pre-subscription samples, so GetNewSamples would return 0 forever.
    process_synchronizer.Notify();

    std::cout << "\nConsumer: Step 5 - Receive first batch on active proxy" << std::endl;
    if (!proxy_state_change_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest("proxy_event_move_semantics consumer failed: WaitForStateChange was interrupted by stop_token");
    }
    if (!proxy_event_receiver.WaitForSamples(stop_token, num_samples_to_receive))
    {
        FailTest("proxy_event_move_semantics consumer failed: WaitForSamples was interrupted by stop_token");
    }
    process_synchronizer.Notify();

    // Step 6. Move assign while active. The receive and state-change handlers are preserved on the moved-to
    // proxy, so no re-registration is needed.
    std::cout << "\nConsumer: Step 6 - Move assign while active" << std::endl;
    passive_proxy = std::move(active_proxy);

    // Reattach monitoring helpers to the moved-to event. The handlers (lambdas) were transferred by the move;
    // Reattach only updates the internal reference so that GetNewSamples / GetSubscriptionState are called on
    // the correct event object without re-registering any handler.
    proxy_event_receiver.Reattach(passive_proxy.moved_event_);
    proxy_state_change_notifier.Reattach(passive_proxy.moved_event_);

    // Step 7. Continue receiving on the moved-to proxy - the handlers survived the move
    std::cout << "\nConsumer: Step 7 - Continue receiving on moved-to proxy" << std::endl;
    for (std::size_t iteration = 0U; iteration < num_send_iterations - 2U; ++iteration)
    {
        std::cout << "\nConsumer: Iteration " << (iteration + 1U) << " of " << (num_send_iterations - 2U) << std::endl;
        if (!proxy_state_change_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
        {
            FailTest("proxy_event_move_semantics consumer failed: WaitForStateChange was interrupted by stop_token");
        }
        if (!proxy_event_receiver.WaitForSamples(stop_token, num_samples_to_receive))
        {
            FailTest("proxy_event_move_semantics consumer failed: WaitForSamples was interrupted by stop_token");
        }
        process_synchronizer.Notify();
    }

    // Step 8. Unsubscribe and subscribe again across the provider's re-offer
    std::cout << "\nConsumer: Step 8 - Unsubscribe and subscribe again" << std::endl;
    std::cout << "\nConsumer: Waiting for provider to withdraw its offer" << std::endl;
    if (!provider_withdraw_synchronizer.WaitWithAbort(stop_token))
    {
        FailTest(
            "proxy_event_move_semantics consumer failed: Waiting for provider withdrawal was interrupted by "
            "stop_token");
    }
    provider_withdraw_synchronizer.Reset();
    passive_proxy.moved_event_.Unsubscribe();
    const auto resubscribe_result = passive_proxy.moved_event_.Subscribe(num_samples_to_receive);
    if (!resubscribe_result.has_value())
    {
        FailTest("proxy_event_move_semantics consumer failed: Re-subscribe failed: ", resubscribe_result.error());
    }
    // Tell the provider we have re-subscribed so that it can re-offer the service.
    process_synchronizer.Notify();
    // A fresh subscription replays the buffered samples, so accept the next sample as the new baseline.
    latest_value.reset();

    // Step 9. Receive the remaining batch after re-subscribing
    std::cout << "\nConsumer: Step 9 - Receive samples again after re-subscribing" << std::endl;
    if (!proxy_state_change_notifier.WaitForStateChange(stop_token, SubscriptionState::kSubscribed))
    {
        FailTest("proxy_event_move_semantics consumer failed: WaitForStateChange was interrupted by stop_token");
    }
    if (!proxy_event_receiver.WaitForSamples(stop_token, num_samples_to_receive))
    {
        FailTest("proxy_event_move_semantics consumer failed: WaitForSamples was interrupted by stop_token");
    }
    process_synchronizer.Notify();

    std::cout << "Consumer: Done with all iterations, exiting" << std::endl;
}

}  // namespace

void RunConsumer(const ProxyMoveScenario& scenario,
                 const std::size_t num_samples_to_receive,
                 const std::size_t num_send_iterations,
                 const score::cpp::stop_token& stop_token)
{
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!process_synchronizer_result.has_value())
    {
        FailTest("proxy_event_move_semantics consumer failed: could not create ready synchronizer");
    }
    auto provider_withdraw_synchronizer_result = ProcessSynchronizer::Create(kProviderWithdrawNotificationShmPath);
    if (!provider_withdraw_synchronizer_result.has_value())
    {
        FailTest("proxy_event_move_semantics consumer failed: could not create provider withdrawal synchronizer");
    }

    ExitFunctionGuard done_guard{[&process_synchronizer_result]() {
        process_synchronizer_result->Notify();
    }};

    auto& process_synchronizer = *process_synchronizer_result;
    auto& provider_withdraw_synchronizer = *provider_withdraw_synchronizer_result;
    process_synchronizer.Reset();
    provider_withdraw_synchronizer.Reset();

    switch (scenario)
    {
        case ProxyMoveScenario::kMoveConstructBeforeSubscribe:
        {
            RunConsumerMoveConstructProxyBeforeSubscribe(process_synchronizer,
                                                         provider_withdraw_synchronizer,
                                                         num_samples_to_receive,
                                                         num_send_iterations,
                                                         stop_token);
            break;
        }
        case ProxyMoveScenario::kMoveConstructWhileSubscribed:
        {
            RunConsumerMoveConstructProxyWhileSubscribed(process_synchronizer,
                                                         provider_withdraw_synchronizer,
                                                         num_samples_to_receive,
                                                         num_send_iterations,
                                                         stop_token);
            break;
        }
        case ProxyMoveScenario::kMoveAssignBeforeSubscribe:
        {
            RunConsumerMoveAssignProxyBeforeSubscribe(process_synchronizer,
                                                      provider_withdraw_synchronizer,
                                                      num_samples_to_receive,
                                                      num_send_iterations,
                                                      stop_token);
            break;
        }
        case ProxyMoveScenario::kMoveAssignWhileSubscribed:
        {
            RunConsumerMoveAssignProxyWhileSubscribed(process_synchronizer,
                                                      provider_withdraw_synchronizer,
                                                      num_samples_to_receive,
                                                      num_send_iterations,
                                                      stop_token);
            break;
        }
        case ProxyMoveScenario::kNumberOfScenarios:
            [[fallthrough]];
        default:
            FailTest("Unknown proxy move scenario in consumer");
    }
}

}  // namespace score::mw::com::test
