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
#include "score/mw/com/test/common_test_resources/proxy_event_consumer.h"
#include "score/mw/com/test/common_test_resources/proxy_event_state_change_notifier.h"
#include "score/mw/com/test/methods/methods_test_resources/process_synchronizer.h"
#include "score/mw/com/test/move_semantics/proxy_event/test_event_datatype.h"
#include "score/mw/com/types.h"

#include <cstdint>
#include <optional>
#include <utility>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/proxy_event_move_semantics_interprocess_notification"};

void RunConsumerMoveConstructProxyBeforeSubscribe(ProcessSynchronizer& process_synchronizer,
                                                  const std::size_t num_samples_to_receive,
                                                  const std::size_t num_send_iterations,
                                                  const score::cpp::stop_token& stop_token)
{
    ProxyContainer<ProxyMoveSemanticsProxy> proxy_container{};

    // Step 1. Find service and create proxy
    std::cout << "\nConsumer: Step 1 - Find service and create proxy" << std::endl;
    proxy_container.CreateProxy(kInstanceSpecifierMovedTo, "proxy_event_move_semantics");

    // Step 2. Move construct proxy before subscribe
    std::cout << "\nConsumer: Step 2 - Move construct proxy before subscribe" << std::endl;
    auto moved_proxy = proxy_container.Extract();

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

    // Step 6. Wait for provider to send the first batch of values and notify
    std::cout << "\nConsumer: Step 6 - Receive first batch of samples" << std::endl;
    ReceiveAndNotify(proxy_event_receiver,
                     proxy_state_change_notifier,
                     process_synchronizer,
                     num_samples_to_receive,
                     num_send_iterations - 1U,
                     stop_token);

    // Step 7. Unsubscribe and subscribe again across the provider's re-offer
    std::cout << "\nConsumer: Step 7 - Unsubscribe and subscribe again" << std::endl;
    ResubscribeAcrossReoffer(moved_proxy.moved_event_,
                             proxy_state_change_notifier,
                             process_synchronizer,
                             num_samples_to_receive,
                             stop_token);
    // A fresh subscription replays the buffered samples, so accept the next sample as the new baseline.
    latest_value.reset();

    // Step 8. Receive the remaining batch after re-subscribing
    std::cout << "\nConsumer: Step 8 - Receive samples again after re-subscribing" << std::endl;
    ReceiveAndNotify(proxy_event_receiver,
                     proxy_state_change_notifier,
                     process_synchronizer,
                     num_samples_to_receive,
                     1U,
                     stop_token);

    std::cout << "Consumer: Done with all iterations, exiting" << std::endl;
}

void RunConsumerMoveConstructProxyWhileSubscribed(ProcessSynchronizer& process_synchronizer,
                                                  const std::size_t num_samples_to_receive,
                                                  const std::size_t num_send_iterations,
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

    // Step 3. Register handlers on original proxy and receive first batch
    std::cout << "\nConsumer: Step 3 - Register handlers on original proxy" << std::endl;
    {
        std::optional<std::uint32_t> latest_value{0U};
        ProxyEventReceiver proxy_event_receiver{
            original_proxy.moved_event_,
            MakeSampleSequenceCallback(latest_value, "proxy_event_move_semantics consumer failed:")};
        ProxyEventStateChangeNotifier proxy_state_change_notifier{original_proxy.moved_event_};

        std::cout << "\nConsumer: Step 4 - Receive first batch of samples" << std::endl;
        ReceiveAndNotify(proxy_event_receiver,
                         proxy_state_change_notifier,
                         process_synchronizer,
                         num_samples_to_receive,
                         1U,
                         stop_token);
    }

    // Step 5. Move construct while subscribed
    std::cout << "\nConsumer: Step 5 - Move construct while subscribed" << std::endl;
    auto moved_proxy = proxy_container.Extract();

    // Step 6. Register handlers on moved-to proxy and continue receiving
    std::cout << "\nConsumer: Step 6 - Register handlers on moved-to proxy" << std::endl;
    std::optional<std::uint32_t> latest_value{};
    ProxyEventReceiver proxy_event_receiver{
        moved_proxy.moved_event_,
        MakeSampleSequenceCallback(latest_value, "proxy_event_move_semantics consumer failed:")};
    ProxyEventStateChangeNotifier proxy_state_change_notifier{moved_proxy.moved_event_};

    // Step 7. Continue receiving on the moved-to proxy while still offered
    std::cout << "\nConsumer: Step 7 - Continue receiving on moved-to proxy" << std::endl;
    ReceiveAndNotify(proxy_event_receiver,
                     proxy_state_change_notifier,
                     process_synchronizer,
                     num_samples_to_receive,
                     num_send_iterations - 2U,
                     stop_token);

    // Step 8. Unsubscribe and subscribe again across the provider's re-offer
    std::cout << "\nConsumer: Step 8 - Unsubscribe and subscribe again" << std::endl;
    ResubscribeAcrossReoffer(moved_proxy.moved_event_,
                             proxy_state_change_notifier,
                             process_synchronizer,
                             num_samples_to_receive,
                             stop_token);
    // A fresh subscription replays the buffered samples, so accept the next sample as the new baseline.
    latest_value.reset();

    // Step 9. Receive the remaining batch after re-subscribing
    std::cout << "\nConsumer: Step 9 - Receive samples again after re-subscribing" << std::endl;
    ReceiveAndNotify(proxy_event_receiver,
                     proxy_state_change_notifier,
                     process_synchronizer,
                     num_samples_to_receive,
                     1U,
                     stop_token);

    std::cout << "Consumer: Done with all iterations, exiting" << std::endl;
}

void RunConsumerMoveAssignProxyBeforeSubscribe(ProcessSynchronizer& process_synchronizer,
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

    // Step 6. Wait for provider to send the first batch of values and notify
    std::cout << "\nConsumer: Step 6 - Receive first batch of samples" << std::endl;
    ReceiveAndNotify(proxy_event_receiver,
                     proxy_state_change_notifier,
                     process_synchronizer,
                     num_samples_to_receive,
                     num_send_iterations - 1U,
                     stop_token);

    // Step 7. Unsubscribe and subscribe again across the provider's re-offer
    std::cout << "\nConsumer: Step 7 - Unsubscribe and subscribe again" << std::endl;
    ResubscribeAcrossReoffer(moved_to_proxy.moved_event_,
                             proxy_state_change_notifier,
                             process_synchronizer,
                             num_samples_to_receive,
                             stop_token);
    // A fresh subscription replays the buffered samples, so accept the next sample as the new baseline.
    latest_value.reset();

    // Step 8. Receive the remaining batch after re-subscribing
    std::cout << "\nConsumer: Step 8 - Receive samples again after re-subscribing" << std::endl;
    ReceiveAndNotify(proxy_event_receiver,
                     proxy_state_change_notifier,
                     process_synchronizer,
                     num_samples_to_receive,
                     1U,
                     stop_token);

    std::cout << "Consumer: Done with all iterations, exiting" << std::endl;
}

void RunConsumerMoveAssignProxyWhileSubscribed(ProcessSynchronizer& process_synchronizer,
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

    {
        std::optional<std::uint32_t> latest_value{0U};
        ProxyEventReceiver proxy_event_receiver{
            active_proxy.moved_event_,
            MakeSampleSequenceCallback(latest_value, "proxy_event_move_semantics consumer failed:")};
        ProxyEventStateChangeNotifier proxy_state_change_notifier{active_proxy.moved_event_};

        std::cout << "\nConsumer: Step 4 - Receive first batch on active proxy" << std::endl;
        ReceiveAndNotify(proxy_event_receiver,
                         proxy_state_change_notifier,
                         process_synchronizer,
                         num_samples_to_receive,
                         1U,
                         stop_token);
    }

    // Step 5. Move assign while active
    std::cout << "\nConsumer: Step 5 - Move assign while active" << std::endl;
    passive_proxy = std::move(active_proxy);

    // Step 6. Continue receiving on the moved-to proxy while still offered
    std::cout << "\nConsumer: Step 6 - Continue receiving on moved-to proxy" << std::endl;
    std::optional<std::uint32_t> latest_value{};
    ProxyEventReceiver proxy_event_receiver{
        passive_proxy.moved_event_,
        MakeSampleSequenceCallback(latest_value, "proxy_event_move_semantics consumer failed:")};
    ProxyEventStateChangeNotifier proxy_state_change_notifier{passive_proxy.moved_event_};

    ReceiveAndNotify(proxy_event_receiver,
                     proxy_state_change_notifier,
                     process_synchronizer,
                     num_samples_to_receive,
                     num_send_iterations - 2U,
                     stop_token);

    // Step 7. Unsubscribe and subscribe again across the provider's re-offer
    std::cout << "\nConsumer: Step 7 - Unsubscribe and subscribe again" << std::endl;
    ResubscribeAcrossReoffer(passive_proxy.moved_event_,
                             proxy_state_change_notifier,
                             process_synchronizer,
                             num_samples_to_receive,
                             stop_token);
    // A fresh subscription replays the buffered samples, so accept the next sample as the new baseline.
    latest_value.reset();

    // Step 8. Receive the remaining batch after re-subscribing
    std::cout << "\nConsumer: Step 8 - Receive samples again after re-subscribing" << std::endl;
    ReceiveAndNotify(proxy_event_receiver,
                     proxy_state_change_notifier,
                     process_synchronizer,
                     num_samples_to_receive,
                     1U,
                     stop_token);

    std::cout << "Consumer: Done with all iterations, exiting" << std::endl;
}

}  // namespace

void RunConsumer(const ProxyMoveScenario& scenario,
                 const InstanceSpecifier& instance_specifier,
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

    auto& process_synchronizer = *process_synchronizer_result;

    switch (scenario)
    {
        case ProxyMoveScenario::kMoveConstructBeforeSubscribe:
        {
            RunConsumerMoveConstructProxyBeforeSubscribe(
                process_synchronizer, num_samples_to_receive, num_send_iterations, stop_token);
            break;
        }
        case ProxyMoveScenario::kMoveConstructWhileSubscribed:
        {
            RunConsumerMoveConstructProxyWhileSubscribed(
                process_synchronizer, num_samples_to_receive, num_send_iterations, stop_token);
            break;
        }
        case ProxyMoveScenario::kMoveAssignBeforeSubscribe:
        {
            RunConsumerMoveAssignProxyBeforeSubscribe(
                process_synchronizer, num_samples_to_receive, num_send_iterations, stop_token);
            break;
        }
        case ProxyMoveScenario::kMoveAssignWhileSubscribed:
        {
            RunConsumerMoveAssignProxyWhileSubscribed(
                process_synchronizer, num_samples_to_receive, num_send_iterations, stop_token);
            break;
        }
        case ProxyMoveScenario::kNumberOfScenarios:
            [[fallthrough]];
        default:
            FailTest("Unknown proxy move scenario in consumer");
    }
}

}  // namespace score::mw::com::test
