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
#include "score/mw/com/test/generic_proxy/unsubscribe_on_destruction/provider.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/skeleton_container.h"
#include "score/mw/com/test/generic_proxy/unsubscribe_on_destruction/test_event_datatype.h"
#include "score/mw/com/types.h"

#include <score/stop_token.hpp>

#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace score::mw::com::test
{
namespace
{

const std::string kInterprocessNotificationShmPath{"/generic_proxy_unsub_on_destruction_notification"};

const InstanceSpecifier kInstanceSpecifier =
    InstanceSpecifier::Create(std::string{"test/generic_proxy/unsubscribe_on_destruction/TestEvents"}).value();

void SendSamples(TestEventSkeleton& skeleton)
{
    for (std::uint32_t value = 1U; value <= kNumberOfSamples; ++value)
    {
        const auto send_result = skeleton.test_event.Send(value);
        if (!send_result.has_value())
        {
            FailTest("Provider: Send failed: ", send_result.error());
        }
    }
    std::cout << "Provider: Sent " << kNumberOfSamples << " samples" << std::endl;
}

}  // namespace

void run_provider(const score::cpp::stop_token& stop_token)
{
    SkeletonContainer<TestEventSkeleton> skeleton_container{};
    auto process_synchronizer_result = ProcessSynchronizer::Create(kInterprocessNotificationShmPath);
    if (!(process_synchronizer_result.has_value()))
    {
        FailTest("GenericProxy unsubscribe_on_destruction provider failed: Could not create ProcessSynchronizer");
    }

    // Step 1. Create skeleton
    std::cout << "\nProvider: Step 1" << std::endl;
    skeleton_container.CreateSkeleton(kInstanceSpecifier, "unsubscribe_on_destruction");
    auto& skeleton = skeleton_container.GetSkeleton();

    // Step 2. Offer service
    std::cout << "\nProvider: Step 2" << std::endl;
    skeleton_container.OfferService("unsubscribe_on_destruction");

    // Step 3. Wait for consumer to subscribe
    std::cout << "\nProvider: Step 3" << std::endl;
    if (!process_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest(
            "GenericProxy unsubscribe_on_destruction provider failed: WaitForConsumerToSubscribe was stopped by "
            "stop_token instead of notification");
    }
    process_synchronizer_result->Reset();

    // Step 4. Send samples
    std::cout << "\nProvider: Step 4" << std::endl;
    SendSamples(skeleton);

    // Step 5. Wait for consumer to destroy its GenericProxy and finish
    std::cout << "\nProvider: Step 5" << std::endl;
    if (!process_synchronizer_result->WaitWithAbort(stop_token))
    {
        FailTest(
            "GenericProxy unsubscribe_on_destruction provider failed: WaitForConsumerToFinish was stopped by "
            "stop_token instead of notification");
    }

    std::cout << "Provider: Shutting down" << std::endl;
}

}  // namespace score::mw::com::test
