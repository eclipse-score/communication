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

#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/consumer_resources.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"
#include "score/mw/com/test/partial_restart/consumer_handle_notification_data.h"
#include "score/mw/com/test/partial_restart/test_datatype.h"

#include <string>

namespace bmw::mw::com::test
{

    namespace
    {

        const auto kMaxNumSamples{10U};
        constexpr auto kInstanceSpecifierString{"partial_restart/small_but_great"};
        const auto kInstanceSpecifier = bmw::mw::com::InstanceSpecifier::Create(kInstanceSpecifierString).value();
        const std::chrono::seconds kMaxHandleNotificationWaitTime{15U};
        // uid 1312, 1313 is reserved for use. See /display/User+Management
        const uid_t kUidFirstConsumer{1312};
        const uid_t kUidSecondConsumer{1313};

        bool StartFindServiceAndWait(const std::string &tag,
                                     HandleNotificationData &handle_notification_data,
                                     CheckPointControl &check_point_control)
        {
            //***************************************************
            // start find service
            //***************************************************
            std::cout << "Consumer Step: Call StartFindService" << std::endl;
            auto find_service_callback = [&tag, &check_point_control, &handle_notification_data](
                                             auto service_handle_container, auto find_service_handle) noexcept
            {
                std::cerr << tag << ": find service handler called" << std::endl;
                if (service_handle_container.size() != 1)
                {
                    std::cerr << tag
                              << ": Error - StartFindService() is expected to find 1 service instance "
                                 "but found: "
                              << service_handle_container.size() << std::endl;
                    check_point_control.ErrorOccurred();
                    return;
                }
                std::lock_guard lock{handle_notification_data.mutex};
                handle_notification_data.handle = std::make_unique<TestServiceProxy::HandleType>(service_handle_container[0]);
                handle_notification_data.condition_variable.notify_all();
                std::cerr << tag << ": FindServiceHandler handler done - found one service instance." << std::endl;

                TestServiceProxy::StopFindService(find_service_handle);
            };

            auto find_service_handle_result =
                StartFindService<TestServiceProxy>("Consumer", find_service_callback, kInstanceSpecifier, check_point_control);
            if (!find_service_handle_result.has_value())
            {
                bmw::mw::log::LogError() << "Unable to get handle from specifier " << find_service_handle_result.error()
                                         << ", bailing!\n";
                return false;
            }

            //***************************************************
            // Wait for FindServiceHandler to be called. Call StopFindService in handler
            //***************************************************
            std::cout << tag << ": Wait for FindServiceHandler to be called" << std::endl;
            std::unique_lock lock{handle_notification_data.mutex};
            const auto wait_result = handle_notification_data.condition_variable.wait_for(
                lock, kMaxHandleNotificationWaitTime, [&handle_notification_data]
                { return handle_notification_data.handle != nullptr; });
            lock.unlock();
            if (!wait_result)
            {
                std::cerr << tag << ": Did not receive handle in time!" << std::endl;
                check_point_control.ErrorOccurred();
                return false;
            }

            return true;
        }

    } // namespace

    void PerformFirstConsumerActions(CheckPointControl &check_point_control, amp::stop_token stop_token)
    {
        //***************************************************
        // Step (1)- setuid
        //***************************************************
        // LoLa requires that processes shall have distinct UIDs.
        // After fork. Parent and child proccess will have same UID.
        // setuid is used to make child proccess UID distinct.
        const auto ret_setuid = setuid(kUidFirstConsumer);
        if (ret_setuid != 0)
        {
            std::cerr << "set uid fails: " << errno << std::strerror(errno) << "\n";
            check_point_control.ErrorOccurred();
            return;
        }
        //***************************************************************************
        // Step (2)- start find service and wait till it is found.
        //***************************************************************************
        HandleNotificationData handle_notification_data{};
        if (!StartFindServiceAndWait("First Consumer Step (2)", handle_notification_data, check_point_control))
        {
            return;
        }

        //***************************************************
        // Step (3)- create proxy
        //***************************************************
        std::cout << "First Consumer Step (3): Create a Proxy for found service" << std::endl;
        auto proxy_wrapper_result =
            CreateProxy<TestServiceProxy>("First Consumer Step (3)", *handle_notification_data.handle, check_point_control);
        if (!(proxy_wrapper_result.has_value()))
        {
            return;
        }
        auto &proxy = proxy_wrapper_result.value();

        //***************************************************
        // Step (4)- subscribe
        //***************************************************
        std::cout << "First Consumer Step (4): Subscribe to Event" << std::endl;
        auto subscribe_result = SubscribeProxyEvent<decltype(proxy.simple_event_)>(
            "First Consumer Step (4)", proxy.simple_event_, kMaxNumSamples, check_point_control);
        if (!(subscribe_result.has_value()))
        {
            return;
        }

        //***************************************************
        // Step (5)- get configured number of new samples
        //***************************************************
        std::size_t samples_received{0U};
        while (samples_received < kMaxNumSamples)
        {
            auto get_new_samples_result = proxy.simple_event_.GetNewSamples(
                [](auto) noexcept
                { std::cout << "First Consumer Step (5): obtained sample.\n"; }, kMaxNumSamples);
            if (!get_new_samples_result.has_value())
            {
                std::cout << "First Consumer Step (5): Failed to get new samples: " << get_new_samples_result.error()
                          << "\n";
                check_point_control.ErrorOccurred();
                return;
            }
            samples_received += get_new_samples_result.value();
        }

        //***************************************************
        // Step (6)- ACK check point 1
        //***************************************************
        std::cout << "First Consumer Step (6): check point 1 reached.\n";
        check_point_control.CheckPointReached(1);

        //***************************************************
        // Step (7)- wait for parent command to proceed
        //***************************************************
        std::cout << "First Consumer Step (7): waiting for parent command to proceed\n";
        const auto proceed_instruction = WaitForChildProceed(check_point_control, stop_token);
        std::cout << "First Consumer Step (7): received parent command to proceed\n";

        if (proceed_instruction != CheckPointControl::ProceedInstruction::FINISH_ACTIONS)
        {
            std::cerr << "Provider Step (P.3): Unexpected proceed instruction received: "
                      << static_cast<int>(proceed_instruction) << std::endl;
            check_point_control.ErrorOccurred();
            return;
        }
    }

    void PerformSecondConsumerActions(CheckPointControl &check_point_control,
                                      amp::stop_token stop_token,
                                      const size_t create_proxy_and_receive_M_times)
    {
        //***************************************************
        // Step (1)- setuid
        //***************************************************
        const auto ret_setuid = setuid(kUidSecondConsumer);
        // LoLa requires that processes shall have distinct UIDs.
        // After fork. Parent and child proccess will have same UID.
        // setuid is used to make child proccess UID distinct.
        if (ret_setuid != 0)
        {
            std::cerr << "Second Consumer Step (1): set uid fails: " << errno << std::strerror(errno) << "\n";
            check_point_control.ErrorOccurred();
            return;
        }
        //*********************************************************
        // Step (2)- start find service and wait till it is found.
        //*********************************************************
        HandleNotificationData handle_notification_data{};
        if (!StartFindServiceAndWait("Second Consumer Step (2)", handle_notification_data, check_point_control))
        {
            return;
        }

        std::uint8_t check_point_number{1};
        for (size_t i = 0; i < create_proxy_and_receive_M_times; ++i)
        {
            //***************************************************
            // Step (3)- create proxy
            //***************************************************
            std::cout << "Second Consumer Step (3): Create a Proxy for found service. Iteration:" << i << std::endl;
            auto proxy_wrapper_result = CreateProxy<TestServiceProxy>(
                "Second Consumer Step (3)", *handle_notification_data.handle, check_point_control);
            if (!(proxy_wrapper_result.has_value()))
            {
                return;
            }
            auto &proxy = proxy_wrapper_result.value();

            //***************************************************
            // Step (4)- subscribe
            //***************************************************
            std::cout << "Second Consumer Step (4): Subscribe to Event" << std::endl;
            auto subscribe_result = SubscribeProxyEvent<decltype(proxy.simple_event_)>(
                "Second Consumer Step (4)", proxy.simple_event_, kMaxNumSamples, check_point_control);
            if (!(subscribe_result.has_value()))
            {
                return;
            }

            //***************************************************
            // Step (5)- get configured number of new samples
            //***************************************************
            std::size_t samples_received{0U};
            while (samples_received < kMaxNumSamples)
            {
                auto get_new_samples_result = proxy.simple_event_.GetNewSamples(
                    [](auto) noexcept
                    { std::cout << "Second Consumer Step (5): obtained sample.\n"; }, kMaxNumSamples);
                if (!get_new_samples_result.has_value())
                {
                    std::cout << "Second Consumer Step (5): Failed to get new samples: " << get_new_samples_result.error()
                              << "\n";
                    check_point_control.ErrorOccurred();
                    return;
                }
                samples_received += get_new_samples_result.value();
            }

            //***************************************************
            // Step (6)- unsubscribe
            //***************************************************
            proxy.simple_event_.Unsubscribe();

            //*****************************************************
            // Step (7)- ACK check point M. Where M ranges from 1 to a specific number.
            //*****************************************************
            std::cout << "Second Consumer Step (7): check point " << static_cast<int>(check_point_number) << " reached.\n";
            check_point_control.CheckPointReached(check_point_number);
            check_point_number++;

            //***************************************************
            // Step (8)- wait for controller command to proceed
            //***************************************************
            std::cout << "Second Consumer Step (8): waiting for parent command to proceed\n";
            const auto proceed_instruction = WaitForChildProceed(check_point_control, stop_token);
            std::cout << "Second Consumer Step (8): received parent command\n";

            if (proceed_instruction != CheckPointControl::ProceedInstruction::PROCEED_NEXT_CHECKPOINT)
            {
                std::cerr << "Second Consumer Step (8): Unexpected instruction received: "
                          << static_cast<int>(proceed_instruction) << std::endl;
                check_point_control.ErrorOccurred();
                return;
            }

            check_point_control.ResetProceedNotifications();
        }

        //***************************************************
        // Step (9)- wait for controller command to finish
        //***************************************************
        std::cout << "Second Consumer Step (9): waiting for parent command to finish\n";
        const auto proceed_instruction = WaitForChildProceed(check_point_control, stop_token);
        std::cout << "Second Consumer Step (9): received parent command\n";
        if (proceed_instruction != CheckPointControl::ProceedInstruction::FINISH_ACTIONS)
        {
            std::cerr << "Second Consumer Step (9): Unexpected instruction received: "
                      << static_cast<int>(proceed_instruction) << std::endl;
            check_point_control.ErrorOccurred();
            return;
        }
    }

} // namespace bmw::mw::com::test
