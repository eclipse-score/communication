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

#include "score/mw/com/test/partial_restart/consumer_restart/consumer.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/consumer_resources.h"
#include "score/mw/com/test/partial_restart/consumer_handle_notification_data.h"

#include "score/concurrency/notification.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"

#include <iostream>

namespace score::mw::com::test
{

namespace
{

const auto kProxyInstanceSpecifier = InstanceSpecifier::Create(std::string{"partial_restart/small_but_great"}).value();
const std::chrono::seconds kMaxHandleNotificationWaitTime{15U};

void DoIdleAActionsWhileWaitingForKill()
{
    const std::chrono::milliseconds sleep_duration{1000U};
    while (true)
    {
        std::this_thread::sleep_for(sleep_duration);
        std::cerr << "Consumer: Still waiting for getting killed ..." << std::endl;
    }
}

}  // namespace

void DoConsumerActions(score::mw::com::test::CheckPointControl& check_point_control,
                       score::cpp::stop_token test_stop_token,
                       int argc,
                       const char** argv,
                       const ConsumerParameters& /* consumer_parameters */)
{
    // Initialize mw::com runtime explicitly, if we were called with cmd-line args from main/parent
    if (argc > 0 && argv != nullptr)
    {
        std::cerr
            << "Consumer: Initializing LoLa/mw::com runtime from cmd-line args handed over by parent/controller ..."
            << std::endl;
        mw::com::runtime::InitializeRuntime(argc, argv);
        std::cerr << "Consumer: Initializing LoLa/mw::com runtime done." << std::endl;
    }

    // ********************************************************************************
    // Step (C.1) - Start an async FindService Search
    // ********************************************************************************
    std::cout << "Consumer Step (C.1): Call StartFindService" << std::endl;
    HandleNotificationData handle_notification_data{};
    auto find_service_callback = [&check_point_control, &handle_notification_data](auto service_handle_container,
                                                                                   auto find_service_handle) noexcept {
        std::cerr << "Consumer Step (C.1): find service handler called" << std::endl;
        if (service_handle_container.size() != 1)
        {
            std::cerr
                << "Consumer Step (C.1): Error - StartFindService() is expected to find 1 service instance but found: "
                << service_handle_container.size() << std::endl;
            check_point_control.ErrorOccurred();
            return;
        }
        std::lock_guard lock{handle_notification_data.mutex};
        handle_notification_data.handle = std::make_unique<TestServiceProxy::HandleType>(service_handle_container[0]);
        handle_notification_data.condition_variable.notify_all();
        std::cerr << "Consumer Step (C.1): FindServiceHandler handler done - found one service instance." << std::endl;

        TestServiceProxy::StopFindService(find_service_handle);
    };

    auto find_service_handle_result = StartFindService<TestServiceProxy>(
        "Consumer Step (C.1)", find_service_callback, kProxyInstanceSpecifier, check_point_control);
    if (!find_service_handle_result.has_value())
    {
        return;
    }

    // ********************************************************************************
    // Step (C.2) - Wait for FindServiceHandler to be called. Call StopFindService in handler
    // ********************************************************************************
    std::cout << "Consumer Step (C.2): Wait for FindServiceHandler to be called" << std::endl;
    std::unique_lock lock{handle_notification_data.mutex};
    const auto wait_result = handle_notification_data.condition_variable.wait_for(
        lock, kMaxHandleNotificationWaitTime, [&handle_notification_data] {
            return handle_notification_data.handle != nullptr;
        });
    lock.unlock();
    if (!wait_result)
    {
        std::cerr << "Consumer: Did not receive handle in time!" << std::endl;
        check_point_control.ErrorOccurred();
        return;
    }

    // ********************************************************************************
    // Step (C.3) - Create a Proxy for the found service
    // ********************************************************************************
    std::cout << "Consumer Step (C.3): Create a Proxy for found service" << std::endl;
    auto proxy_wrapper_result =
        CreateProxy<TestServiceProxy>("Consumer Step (C.3)", *handle_notification_data.handle, check_point_control);
    if (!(proxy_wrapper_result.has_value()))
    {
        return;
    }
    auto& proxy = proxy_wrapper_result.value();
    std::cerr << "Consumer Step (C.3): Created a proxy" << std::endl;

    // ********************************************************************************
    // Step (C.4) - Subscribe to Event
    // ********************************************************************************
    std::cout << "Consumer Step (C.4): Subscribe to Event" << std::endl;
    const std::size_t max_sample_count{5U};
    auto subscribe_result = SubscribeProxyEvent<decltype(proxy.simple_event_)>(
        "Consumer Step (C.4)", proxy.simple_event_, max_sample_count, check_point_control);
    if (!(subscribe_result.has_value()))
    {
        return;
    }

    // ********************************************************************************
    // Step (C.5) - Register EventReceiveHandler
    // ********************************************************************************
    std::cout << "Consumer Step (C.5): Registering EventReceiveHandler" << std::endl;
    concurrency::Notification event_received{};
    auto set_receive_handler_result = SetBasicNotifierReceiveHandler<decltype(proxy.simple_event_)>(
        "Consumer", proxy.simple_event_, event_received, check_point_control);
    if (!(set_receive_handler_result.has_value()))
    {
        return;
    }

    // ********************************************************************************
    // Step (C.6) - Wait until N events have been received
    // ********************************************************************************
    std::size_t num_samples_received{0};
    const std::size_t desired_samples_received{10U};
    while (num_samples_received < desired_samples_received)
    {
        std::cout << "Consumer Step (C.6): Waiting for sample" << std::endl;
        if (!event_received.waitWithAbort(test_stop_token))
        {
            std::cerr << "Consumer Step (C.6): Event reception aborted via stop-token!" << std::endl;
            check_point_control.ErrorOccurred();
            return;
        }
        std::cout << "Consumer Step (C.6): Calling GetNewSamples" << std::endl;
        auto get_new_samples_result = proxy.simple_event_.GetNewSamples(
            [&check_point_control](SamplePtr<SimpleEventDatatype> sample) noexcept {
                std::cout << "Consumer Step (C.6): Received sample from GetNewSamples: member_1 (" << sample->member_1
                          << ") / member_2 (" << sample->member_2 << ")" << std::endl;
                if (sample->member_1 != sample->member_2)
                {
                    std::cerr
                        << "Consumer: GetNewSamples received corrupted data. Expected that member_1 == member_2 : "
                        << std::endl;
                    check_point_control.ErrorOccurred();
                }
            },
            max_sample_count);
        if (!(get_new_samples_result.has_value()))
        {
            std::cerr << "Consumer Step (C.6): GetNewSamples failed with error: " << get_new_samples_result.error()
                      << std::endl;
            check_point_control.ErrorOccurred();
            return;
        }
        num_samples_received += get_new_samples_result.value();
        event_received.reset();
        std::cout << "Consumer Step (C.6): Reset event received notifier" << std::endl;
    }

    // ********************************************************************************
    // Step (C.7) - Notify to Controller, that checkpoint (1) has been reached
    // ********************************************************************************
    std::cout << "Consumer Step (C.7): Notify controller that checkpoint 1 has been reached" << std::endl;
    check_point_control.CheckPointReached(1);

    // ********************************************************************************
    // Step (C.8) - Wait for finish trigger or termination by controller
    // ********************************************************************************
    std::cout << "Consumer Step (C.8): Wait for proceed trigger from Controller to indicate that consumer can finish "
                 "or termination."
              << std::endl;
    auto proceed_instruction = WaitForChildProceed(check_point_control, test_stop_token);
    if (proceed_instruction == CheckPointControl::ProceedInstruction::FINISH_ACTIONS)
    {
        std::cout << "Consumer: Finishing actions!" << std::endl;
    }
    else if (proceed_instruction == CheckPointControl::ProceedInstruction::WAIT_FOR_KILL)
    {
        std::cout << "Consumer: Waiting until being killed!" << std::endl;
        std::cout << std::flush;
        check_point_control.SetChildWaitingForKill();
        DoIdleAActionsWhileWaitingForKill();
    }
    else
    {
        std::cerr << "Consumer Step (C.8): Received proceed-trigger from controller, but expected finish-trigger!"
                  << std::endl;
        check_point_control.ErrorOccurred();
    }
}

}  // namespace score::mw::com::test
