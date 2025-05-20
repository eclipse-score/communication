/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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

#include "score/mw/com/test/partial_restart/provider_restart/consumer.h"

#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/consumer_resources.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/common_test_resources/generic_trace_api_test_resources.h"
#include "score/mw/com/test/partial_restart/consumer_handle_notification_data.h"
#include "score/mw/com/test/partial_restart/test_datatype.h"

#include "score/mw/com/runtime.h"
#include "score/mw/com/types.h"

#include <iostream>
#include <mutex>

namespace bmw::mw::com::test
{

    void DoConsumerActionsWithProxy(CheckPointControl &check_point_control,
                                    HandleNotificationData &handle_notification_data,
                                    amp::stop_token test_stop_token,
                                    const ConsumerParameters &) noexcept
    {
        // ********************************************************************************
        // Step (2) - Create Proxy for found service
        // ********************************************************************************
        auto proxy_wrapper_result =
            CreateProxy<TestServiceProxy>("Consumer", *handle_notification_data.handle, check_point_control);
        if (!(proxy_wrapper_result.has_value()))
        {
            return;
        }
        auto &lola_proxy = proxy_wrapper_result.value();

        // ********************************************************************************
        // Step (3) - Subscribe to the event
        // ********************************************************************************
        const std::size_t max_sample_count{5U};
        auto subscribe_result = SubscribeProxyEvent<decltype(lola_proxy.simple_event_)>(
            "Consumer", lola_proxy.simple_event_, max_sample_count, check_point_control);
        if (!(subscribe_result.has_value()))
        {
            return;
        }

        // ********************************************************************************
        // Step (4) - Register EventReceiveHandler for the event.
        // ********************************************************************************
        concurrency::Notification event_received{};
        auto set_receive_handler_result = SetBasicNotifierReceiveHandler<decltype(lola_proxy.simple_event_)>(
            "Consumer", lola_proxy.simple_event_, event_received, check_point_control);
        if (!(set_receive_handler_result.has_value()))
        {
            return;
        }

        // ********************************************************************************
        // Step (5) -  max_sample_count events
        // ********************************************************************************
        std::size_t num_samples_received{0};
        std::vector<bmw::mw::com::SamplePtr<SimpleEventDatatype>> sample_ptrs;
        std::vector<SimpleEventDatatype> events;
        while (num_samples_received < max_sample_count)
        {
            std::cout << "Consumer: Waiting for sample" << std::endl;
            if (!event_received.waitWithAbort(test_stop_token))
            {
                std::cerr << "Consumer: Event reception aborted via stop-token!" << std::endl;
                check_point_control.ErrorOccurred();
                return;
            }
            std::cout << "Consumer: Calling GetNewSamples" << std::endl;
            auto get_new_samples_result = lola_proxy.simple_event_.GetNewSamples(
                [&sample_ptrs, &events](SamplePtr<SimpleEventDatatype> sample) noexcept
                {
                    std::cerr << "Consumer: Received sample from GetNewSamples: member_1 (" << sample->member_1
                              << ") / member_2 (" << sample->member_2 << ")" << std::endl;
                    events.emplace_back(*sample);
                    sample_ptrs.emplace_back(std::move(sample));
                },
                max_sample_count);
            if (!(get_new_samples_result.has_value()))
            {
                std::cerr << "Consumer: GetNewSamples failed with error: " << get_new_samples_result.error() << std::endl;
                check_point_control.ErrorOccurred();
                return;
            }
            num_samples_received += get_new_samples_result.value();
            event_received.reset();
        }
        // ********************************************************************************
        // Step (6) - Notify to Controller, that checkpoint (1) has been reached
        // ********************************************************************************
        std::cerr << "Consumer: Expected number of samples received - checkpoint (1) reached!" << std::endl;
        check_point_control.CheckPointReached(1);

        // ********************************************************************************
        // Step (7) - wait for controller to trigger further steps
        // ********************************************************************************
        auto wait_for_child_proceed_result = WaitForChildProceed(check_point_control, test_stop_token);
        if (wait_for_child_proceed_result != CheckPointControl::ProceedInstruction::PROCEED_NEXT_CHECKPOINT)
        {
            std::cerr << "Consumer: Expected to get notification to continue to next checkpoint but got: "
                      << static_cast<int>(wait_for_child_proceed_result) << std::endl;
            check_point_control.ErrorOccurred();
            return;
        }

        // ********************************************************************************
        // Step (8) - Supervise event-subscription state. Expecting it to switch to
        //            subscription-pending. If detected notify controller, that
        //            checkpoint (2) has been reached.
        // ********************************************************************************
        auto subscription_state = lola_proxy.simple_event_.GetSubscriptionState();
        std::cerr << "Consumer: Now waiting for event switch to kSubscriptionPending!" << std::endl;

        // In step (10) we have a poll-loop based on the event-subscription-state.
        // This is the nice way to do it, but poll-loop only works, if the state persists long enough!
        // in our provider kill-restart sequence it will not work as during restart the old offer gets withdrawn and
        // almost immediately renewed. So the consumer might not see the very short time the state goes to
        // kSubscriptionPending! So in this case, we have to resort back to direct events from the async StartFindService
        // search. Later we could do it more nicely based on the t be implemented event-subscription state-change handler.

        // So currently we directly check notifications of the start-find-service callbacks - the same approach, we use
        // in the ITFs, where we have NO proxy instance at the consumer side ...
        std::unique_lock lock{handle_notification_data.mutex};
        handle_notification_data.condition_variable.wait(
            lock, [&handle_notification_data]
            { return handle_notification_data.handle == nullptr; });
        lock.unlock();

        std::cerr << "Consumer: Event switched to kSubscriptionPending - checkpoint (2) reached!" << std::endl;
        check_point_control.CheckPointReached(2);

        // ********************************************************************************
        // Step (9) - wait for controller notification to trigger further steps or finish.
        // ********************************************************************************
        wait_for_child_proceed_result = WaitForChildProceed(check_point_control, test_stop_token);
        if (wait_for_child_proceed_result != CheckPointControl::ProceedInstruction::PROCEED_NEXT_CHECKPOINT)
        {
            std::cerr << "Consumer: Expected to get notification to continue to next checkpoint but got: "
                      << static_cast<int>(wait_for_child_proceed_result) << std::endl;
            check_point_control.ErrorOccurred();
            return;
        }

        // ********************************************************************************
        // Step (10) - Supervise event-subscription state. Expecting it to switch to
        //            subscribed again. If detected notify controller, that
        //            checkpoint (3) has been reached.
        // ********************************************************************************
        subscription_state = lola_proxy.simple_event_.GetSubscriptionState();
        while (subscription_state != SubscriptionState::kSubscribed)
        {
            if (test_stop_token.stop_requested())
            {
                std::cerr << "Consumer: Wait for event switch to kSubscribed aborted via stop-token!" << std::endl;
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{50U});
            subscription_state = lola_proxy.simple_event_.GetSubscriptionState();
        }
        check_point_control.CheckPointReached(3);

        // ********************************************************************************
        // Step (11) - wait for controller notification to trigger further steps or finish.
        // ********************************************************************************
        wait_for_child_proceed_result = WaitForChildProceed(check_point_control, test_stop_token);
        if (wait_for_child_proceed_result != CheckPointControl::ProceedInstruction::PROCEED_NEXT_CHECKPOINT)
        {
            std::cerr << "Consumer: Expected to get notification to continue to next checkpoint but got: "
                      << static_cast<int>(wait_for_child_proceed_result) << std::endl;
            check_point_control.ErrorOccurred();
            return;
        }

        // ********************************************************************************
        // Step (12) - Check the stored data pointed by SamplePtrs for integrity.
        // ********************************************************************************
        for (std::size_t i = 0; i < max_sample_count; i++)
        {
            auto event = *sample_ptrs.at(i);
            if ((event.member_1 != events.at(i).member_1) || (event.member_2 != events.at(i).member_2))
            {
                std::cerr << "Consumer: Data integrity check failed.\n";
                check_point_control.ErrorOccurred();
                return;
            }
        }

        // ********************************************************************************
        // Step (13) - Clear all stored SamplePtrs.
        // ********************************************************************************
        sample_ptrs.clear();

        // ********************************************************************************
        // Step (14) - Repeat Step (5) again
        // ********************************************************************************
        num_samples_received = 0;
        while (num_samples_received < max_sample_count)
        {
            if (!event_received.waitWithAbort(test_stop_token))
            {
                std::cerr << "Consumer: Event reception aborted via stop-token!" << std::endl;
                check_point_control.ErrorOccurred();
                return;
            }
            auto get_new_samples_result = lola_proxy.simple_event_.GetNewSamples(
                [&sample_ptrs](SamplePtr<SimpleEventDatatype> sample) noexcept
                {
                    std::cerr << "Consumer: Received sample from GetNewSamples: member_1 (" << sample->member_1
                              << ") / member_2 (" << sample->member_2 << ")" << std::endl;
                    sample_ptrs.emplace_back(std::move(sample));
                },
                max_sample_count);
            if (!(get_new_samples_result.has_value()))
            {
                std::cerr << "Consumer: GetNewSamples failed with error: " << get_new_samples_result.error() << std::endl;
                check_point_control.ErrorOccurred();
                return;
            }
            num_samples_received += get_new_samples_result.value();
            event_received.reset();
        }

        // ********************************************************************************
        // Step (15) - Notify controller, that checkpoint (4) has been reached.
        // ********************************************************************************
        check_point_control.CheckPointReached(4);

        // ********************************************************************************
        // Step (16) - wait for controller notification to finish.
        // ********************************************************************************
        wait_for_child_proceed_result = WaitForChildProceed(check_point_control, test_stop_token);
        if (wait_for_child_proceed_result != CheckPointControl::ProceedInstruction::FINISH_ACTIONS)
        {
            std::cerr << "Consumer: Expected to get notification to finish but got: "
                      << static_cast<int>(wait_for_child_proceed_result) << std::endl;
            check_point_control.ErrorOccurred();
            return;
        }
        std::cerr << "Consumer: Finishing Actions!" << std::endl;
    }

    void DoConsumerActionsWithOutProxy(CheckPointControl & /*check_point_control*/,
                                       HandleNotificationData & /*handle_notification_data*/,
                                       amp::stop_token /*test_stop_token*/,
                                       const ConsumerParameters & /*test_params*/) noexcept
    {
        // not yet implemented.
    }

    /// \brief Implements Actions/Steps done by the Consumer process in the Partial Restart ITF
    /// \param check_point_control communication/sync object to interact with the parent/controller process.
    /// \param create_and_run_proxy
    /// \param argc
    /// \param argv
    void DoConsumerActions(CheckPointControl &check_point_control,
                           amp::stop_token test_stop_token,
                           int argc,
                           const char **argv,
                           ConsumerParameters test_params) noexcept
    {
        // we also set up IPC-Tracing mocks for the consumer side, although we technically don't do tracing on the proxy
        // side. But we are sharing ONE mw_com_config.json between producer and consumer (which has IPC tracing enabled).
        // The alternative would have been to apply different mw_com_config.json configs for both provider/consumer
        // processes ...
        GenericTraceApiMockContext trace_api_mock_context{};
        trace_api_mock_context.typed_memory_mock = std::make_shared<TypedMemoryMock>();
        SetupGenericTraceApiMocking(trace_api_mock_context);

        // Initialize mw::com runtime explicitly, if we were called with cmd-line args from main/parent
        if (argc > 0 && argv != nullptr)
        {
            std::cerr
                << "Consumer: Initializing LoLa/mw::com runtime from cmd-line args handed over by parent/controller ..."
                << std::endl;
            mw::com::runtime::InitializeRuntime(argc, argv);
            std::cerr << "Consumer: Initializing LoLa/mw::com runtime done." << std::endl;
        }

        HandleNotificationData handle_notification_data{};

        // setup Proxy::StartFindService once. This async service discovery search will be active for the whole runtime
        // of the consumer process - among all starts/kills of the service provider processes. It serves as our indicator,
        // whether the service instance has been successfully (re) started.

        // ********************************************************************************
        // Step (1) - Start an async FindService Search
        // ********************************************************************************
        auto instance_specifier_result = bmw::mw::com::InstanceSpecifier::Create("partial_restart/small_but_great");
        if (!instance_specifier_result.has_value())
        {
            std::cerr << "Consumer: Could not create instance specifier due to error "
                      << std::move(instance_specifier_result).error() << ", terminating!\n";
            check_point_control.ErrorOccurred();
            return;
        }
        auto find_service_callback = [&check_point_control, &handle_notification_data](auto service_handle_container,
                                                                                       auto) mutable noexcept
        {
            std::cerr << "Consumer: find service handler called" << std::endl;
            if (service_handle_container.size() > 1)
            {
                std::cerr << "Consumer: Error - StartFindService() did find more than 1 service instance!" << std::endl;
                check_point_control.ErrorOccurred();
                return;
            }
            else if (service_handle_container.size() == 1)
            {
                std::lock_guard lock{handle_notification_data.mutex};
                handle_notification_data.handle =
                    std::make_unique<bmw::mw::com::test::TestServiceProxy::HandleType>(service_handle_container[0]);
                handle_notification_data.condition_variable.notify_all();
                std::cerr << "Consumer: FindServiceHandler handler done - found one service instance." << std::endl;
            }
            else // service container size == 0 -> initial empty find-result or service disappeared
            {
                std::cerr << "Consumer: find service handler called with 0 instances." << std::endl;
                std::lock_guard lock{handle_notification_data.mutex};
                const auto handle_previously_existed = (handle_notification_data.handle != nullptr);
                handle_notification_data.handle.reset(nullptr);
                if (handle_previously_existed)
                {
                    std::cerr << "Consumer: FindServiceHandler handler done - service instance disappeared." << std::endl;
                    handle_notification_data.condition_variable.notify_all();
                }
            }
        };

        auto find_service_handle_result = StartFindService<TestServiceProxy>(
            "Consumer", find_service_callback, instance_specifier_result.value(), check_point_control);
        if (!find_service_handle_result.has_value())
        {
            return;
        }

        // Wait until Service Discovery returns a valid handle to create the Proxy
        std::unique_lock lock{handle_notification_data.mutex};
        handle_notification_data.condition_variable.wait(
            lock, [&handle_notification_data]
            { return handle_notification_data.handle != nullptr; });
        lock.unlock();

        if (test_params.create_and_run_proxy)
        {
            // ********************************************************************************
            // Consumer sequence for
            // ITF 1 - Provider normal restart - connected Proxy
            // AND
            // ITF 3 - Provider crash restart - connected Proxy
            // ITF 5 - Consumer normal restart
            // ITF 6 - Consumer crash restart
            // ********************************************************************************
            DoConsumerActionsWithProxy(check_point_control, handle_notification_data, test_stop_token, test_params);
        }
        else
        {
            // ********************************************************************************
            // Consumer sequence for
            // ITF 2 - Provider normal restart - **without** connected Proxy
            // AND
            // ITF 4 - Provider crash restart - **without** connected Proxy
            // ********************************************************************************
            DoConsumerActionsWithOutProxy(check_point_control, handle_notification_data, test_stop_token, test_params);
        }
        std::cerr << "Consumer: Finishing Actions." << std::endl;
    }

} // namespace bmw::mw::com::test
