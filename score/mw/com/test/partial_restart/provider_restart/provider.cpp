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

#include "score/mw/com/test/partial_restart/provider_restart/provider.h"

#include "score/concurrency/notification.h"
#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/common_test_resources/generic_trace_api_test_resources.h"
#include "score/mw/com/test/common_test_resources/provider_resources.h"
#include "score/mw/com/test/partial_restart/test_datatype.h"

#include "score/mw/com/runtime.h"
#include "score/mw/com/types.h"

#include <chrono>
#include <iostream>
#include <thread>

namespace bmw::mw::com::test
{
    namespace
    {

        const std::chrono::milliseconds kSampleSendCycleTime{40};

        /// \brief cyclic event-send-thread used by producer
        class CyclicEventSender
        {
        public:
            CyclicEventSender(TestServiceSkeleton &skeleton, GenericTraceApiMockContext &trace_api_mock_context)
                : skeleton_{skeleton}, trace_api_mock_context_{trace_api_mock_context}
            {
            }

            // Not copyable as our thread member isn't copyable (right so)
            CyclicEventSender(const CyclicEventSender &other) = delete;
            CyclicEventSender &operator=(const CyclicEventSender &rhs) = delete;

            void Start()
            {
                AMP_ASSERT_PRD_MESSAGE(cyclic_send_thread_.joinable() == false,
                                       "cyclic event sender thread is already active!");
                cyclic_send_thread_ = std::thread{&CyclicEventSender::CyclicSendActivity, this, stop_source_.get_token()};
            }

            void Stop()
            {
                if (cyclic_send_thread_.joinable())
                {
                    stop_source_.request_stop();
                    cyclic_send_thread_.join();
                }
            }

            ~CyclicEventSender() { Stop(); }

        private:
            void CyclicSendActivity(amp::stop_token stop_token) noexcept
            {
                SimpleEventDatatype event_data{1U, 42U};
                while (!stop_token.stop_requested())
                {
                    // Provider sends an event update (which leads - since IPC-Tracing is enabled - to a transaction-log update)
                    auto result = skeleton_.simple_event_.Send(event_data);
                    event_data.member_1++;
                    event_data.member_2++;
                    if (!result.has_value())
                    {
                        std::cerr << "Provider: Sending of event failed: " << result.error().Message().data() << std::endl;
                    }

                    std::cout << "Provider: Sent data: (" << event_data.member_1 << ", " << event_data.member_2 << ")"
                              << std::endl;
                    std::this_thread::sleep_for(kSampleSendCycleTime);

                    // After some sleep, we simulate a Trace-Done-Callback from the mocked GenericTraceAPI ... as our skeleton
                    // should have called Trace() within Send() and now expects this callback to free the slot again ...
                    if (!trace_api_mock_context_.stored_trace_done_cb)
                    {
                        std::cerr << "Provider: No TraceDoneCB was registered although IPC tracing should be enabled in the "
                                     "config, exiting cyclic sender thread!"
                                  << std::endl;
                        break;
                    }
                    else
                    {
                        if (trace_api_mock_context_.last_trace_context_id.has_value())
                        {
                            trace_api_mock_context_.stored_trace_done_cb(trace_api_mock_context_.last_trace_context_id.value());
                            trace_api_mock_context_.last_trace_context_id = amp::nullopt;
                        }
                    }
                }
            };

            TestServiceSkeleton &skeleton_;
            GenericTraceApiMockContext &trace_api_mock_context_;
            amp::stop_source stop_source_{};
            std::thread cyclic_send_thread_;
        };

    } // namespace

    void DoProviderActions(CheckPointControl &check_point_control,
                           amp::stop_token test_stop_token,
                           int argc,
                           const char **argv) noexcept
    {
        // We enabled IPC Tracing in our mw_com_config.json. Since we don't want the full DMA-TraceLibrary functionality
        // integrated in this test (although it is an integration/ITF test), we mock it accordingly.
        GenericTraceApiMockContext trace_api_mock_context{};
        trace_api_mock_context.typed_memory_mock = std::make_shared<TypedMemoryMock>();
        std::cerr << "Provider: Setting up GenericTraceAPI mocking ..." << std::endl;
        SetupGenericTraceApiMocking(trace_api_mock_context);
        std::cerr << "Provider: Setting up GenericTraceAPI mocking done." << std::endl;

        if (argc > 0 && argv != nullptr)
        {
            std::cerr
                << "Provider: Initializing LoLa/mw::com runtime from cmd-line args handed over by parent/controller ..."
                << std::endl;
            mw::com::runtime::InitializeRuntime(argc, argv);
            std::cerr << "Provider: Initializing LoLa/mw::com runtime done." << std::endl;
        }

        // ********************************************************************************
        // Step (1) - Create service instance/skeleton
        // ********************************************************************************

        constexpr auto instance_specifier_string{"partial_restart/small_but_great"};
        auto skeleton_wrapper_result =
            CreateSkeleton<TestServiceSkeleton>("Provider", instance_specifier_string, check_point_control);
        if (!(skeleton_wrapper_result.has_value()))
        {
            return;
        }
        auto &service_instance = skeleton_wrapper_result.value();

        // ********************************************************************************
        // Step (2) - Offer Service. `Checkpoint` (1) is reached when service is offered - notify to `Controller`.
        // ********************************************************************************
        // before offering (which takes some time), we check, whether we shall already stop ...
        if (test_stop_token.stop_requested())
        {
            return;
        }

        auto offer_service_result = OfferService<TestServiceSkeleton>("Provider", service_instance, check_point_control);
        if (!(offer_service_result.has_value()))
        {
            return;
        }
        std::cerr << "Provider: Service instance is offered." << std::endl;
        check_point_control.CheckPointReached(1);

        // ********************************************************************************
        // Step (3) - Start sending cyclic events
        // ********************************************************************************
        // before Starting Cyclic Sending (which takes some time), we check, whether we shall already stop ...
        if (test_stop_token.stop_requested())
        {
            std::cerr << "Provider: Stop requested. Exiting" << std::endl;
            return;
        }
        auto cyclic_event_sender = CyclicEventSender{service_instance, trace_api_mock_context};
        cyclic_event_sender.Start();

        // ********************************************************************************
        // Step (4) - Wait for proceed trigger from Controller
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
        // Step (5) - Stop sending events. Calls StopOffer on the service instance (skeleton)
        // ********************************************************************************

        // we stop our cyclic sender thread first! This is "essential" as sending event updates after stop-offering the
        // service instance will lead to all types of errors.
        std::cerr << "Provider: Stopping cyclic event sending." << std::endl;
        cyclic_event_sender.Stop();
        // and then stop offer our service instance
        std::cerr << "Provider: Stopping service offering." << std::endl;
        service_instance.StopOfferService();

        // ********************************************************************************
        // Step (6) - Checkpoint(2) reached - notify controller
        // ********************************************************************************
        std::cerr << "Provider: Notifying controller, that checkpoint(2) has been reached." << std::endl;
        check_point_control.CheckPointReached(2);

        // ********************************************************************************
        // Step (7) - Wait for Controller trigger to terminate.
        // ********************************************************************************
        wait_for_child_proceed_result = WaitForChildProceed(check_point_control, test_stop_token);
        if (wait_for_child_proceed_result != CheckPointControl::ProceedInstruction::FINISH_ACTIONS)
        {
            std::cerr << "Provider: Expected to get notification to finish but got: "
                      << static_cast<int>(wait_for_child_proceed_result) << std::endl;
            check_point_control.ErrorOccurred();
            return;
        }
        std::cerr << "Provider: Finishing Actions!" << std::endl;
    }

} // namespace bmw::mw::com::test
