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

#include "score/mw/com/test/subscribe_handler/subscribe_handler_application.h"

#include "score/os/utils/interprocess/interprocess_notification.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/test/common_test_resources/sample_sender_receiver.h"
#include "score/mw/com/test/common_test_resources/sctf_test_runner.h"
#include "score/mw/com/test/common_test_resources/shared_memory_object_creator.h"
#include "score/mw/com/test/common_test_resources/shared_memory_object_guard.h"

#include "score/os/errno.h"

#include <future>
#include <string>
#include <thread>
#include <vector>

namespace
{

const std::string kInterprocessNotificationShmPath{"/lock"};

}

/**
 * Integration test to ensure that if a proxy / proxy event is destroyed before a subscription state callback is called,
 * the subscription is revoked and the program doesn't crash.
 */
int main(int argc, const char** argv)
{
    using Parameters = score::mw::com::test::SctfTestRunner::RunParameters::Parameters;

    const std::vector<Parameters> allowed_parameters{Parameters::MODE};
    score::mw::com::test::SctfTestRunner test_runner(argc, argv, allowed_parameters);
    const auto& run_parameters = test_runner.GetRunParameters();
    const auto stop_token = test_runner.GetStopToken();
    const auto mode = run_parameters.GetMode();

    score::mw::com::test::EventSenderReceiver event_sender_receiver;
    const auto instance_specifier_result =
        score::mw::com::InstanceSpecifier::Create(std::string{"score/cp60/MapApiLanesStamped"});
    if (!instance_specifier_result.has_value())
    {
        std::cerr << "Invalid instance specifier, terminating." << std::endl;

        return EXIT_FAILURE;
    }
    const auto& instance_specifier = instance_specifier_result.value();

    if (mode == "send" || mode == "skeleton")
    {
        auto interprocess_notification_result =
            score::mw::com::test::SharedMemoryObjectCreator<score::os::InterprocessNotification>::CreateOrOpenObject(
                kInterprocessNotificationShmPath);
        if (!interprocess_notification_result.has_value())
        {
            std::stringstream ss;
            ss << "Creating or opening interprocess notification object on skeleton side failed:"
               << interprocess_notification_result.error().ToString();
            std::cout << ss.str() << std::endl;
            return EXIT_FAILURE;
        }

        const score::mw::com::test::SharedMemoryObjectGuard<score::os::InterprocessNotification>
            interprocess_notification_guard{interprocess_notification_result.value()};
        return event_sender_receiver.RunAsSkeletonWaitForProxy(
            instance_specifier, interprocess_notification_result.value().GetObject(), stop_token);
    }
    else if (mode == "recv" || mode == "proxy")
    {
        auto interprocess_notification_result =
            score::mw::com::test::SharedMemoryObjectCreator<score::os::InterprocessNotification>::CreateOrOpenObject(
                kInterprocessNotificationShmPath);
        if (!interprocess_notification_result.has_value())
        {
            std::stringstream ss;
            ss << "Creating or opening interprocess notification object on proxy side failed:"
               << interprocess_notification_result.error().ToString();
            std::cout << ss.str() << std::endl;
            return EXIT_FAILURE;
        }

        const score::mw::com::test::SharedMemoryObjectGuard<score::os::InterprocessNotification>
            interprocess_notification_guard{interprocess_notification_result.value()};
        return event_sender_receiver.RunAsProxyCheckSubscribeHandler(
            instance_specifier, interprocess_notification_result.value().GetObject(), stop_token);
    }
    else
    {
        std::cerr << "Unknown mode " << mode << ", terminating." << std::endl;
        return EXIT_FAILURE;
    }
}
