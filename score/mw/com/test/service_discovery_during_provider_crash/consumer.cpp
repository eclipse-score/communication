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

#include "score/mw/com/test/service_discovery_during_provider_crash/consumer.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/test/common_test_resources/check_point_control.h"
#include "score/mw/com/test/common_test_resources/consumer_resources.h"

#include "score/mw/com/test/common_test_resources/general_resources.h"
#include "score/mw/com/test/service_discovery_during_provider_crash/test_datatype.h"

#include <iostream>
#include <random>

namespace score::mw::com::test
{
namespace
{
const auto kProxyInstanceSpecifier =
    InstanceSpecifier::Create(std::string{"test/service_discovery_during_provider_crash"}).value();
}  // namespace

void DoConsumerActions(score::mw::com::test::CheckPointControl& check_point_control,
                       score::cpp::stop_token test_stop_token,
                       int argc,
                       const char** argv)
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
    // Step (C.1) - Get Ready
    // ********************************************************************************

    std::cout << "Consumer Step (C.1): Ready to call StartFindService." << std::endl;
    check_point_control.CheckPointReached(1);
    if (WaitForChildProceed(check_point_control, test_stop_token) !=
        CheckPointControl::ProceedInstruction::PROCEED_NEXT_CHECKPOINT)
    {
        std::cerr << "Consumer Step (C.1): Incorrect instruction received." << std::endl;
        check_point_control.ErrorOccurred();
        return;
    }

    // ***********************************************************************************
    // Step (C.2) - Start an async FindService search.
    //              Once a Service is found, wait in the callback for a random amount
    //              of nanosecond to give the controller time to do bad things to the
    //              provider while the consumer is still in the callback.
    // ***********************************************************************************

    auto find_service_callback = [&check_point_control](auto service_handle_container,
                                                        auto find_service_handle) noexcept {
        std::cerr << "Consumer Step (C.2): find service handler called" << std::endl;
        if (service_handle_container.size() != 1)
        {
            std::cerr
                << "Consumer Step (C.2): Error - StartFindService() is expected to find 1 service instance but found: "
                << service_handle_container.size() << std::endl;
            check_point_control.ErrorOccurred();
            return;
        }

        std::cerr << "Consumer Step (C.2): FindServiceHandler handler done - found one service instance." << std::endl;
        std::cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;

        std::cout << "Consumer Step (C.2): Generated a random number." << std::endl;
        auto a_quick_pause = get_random_time();
        std::this_thread::sleep_for(a_quick_pause);

        std::cout << "Consumer Step (C.2): StopFindService will be called." << std::endl;

        if (auto result = TestServiceProxy::StopFindService(find_service_handle); !result.has_value())
        {
            std::cout << "Consumer Step (C.2): Error Occurred during StopFindService." << std::endl;
            std::cout << result.error() << std::endl;
            check_point_control.ErrorOccurred();
        }
        else
        {
            std::cout << "Consumer Step (C.2): StopFindService was called." << std::endl;
        }
    };

    std::cout << "Consumer Step (C.2): Call StartFindService" << std::endl;
    auto find_service_handle_result = StartFindService<TestServiceProxy>(
        "Consumer Step (C.2)", find_service_callback, kProxyInstanceSpecifier, check_point_control);
    if (!find_service_handle_result.has_value())
    {
        return;
    }

    if (WaitForChildProceed(check_point_control, test_stop_token) !=
        CheckPointControl::ProceedInstruction::FINISH_ACTIONS)
    {
        std::cerr << "Consumer Step (C.2): Received proceed-trigger from controller, but expected finish-trigger!"
                  << std::endl;
        check_point_control.ErrorOccurred();
        return;
    }
    std::cout << "Consumer: Finishing actions!" << std::endl;
}
}  // namespace score::mw::com::test
