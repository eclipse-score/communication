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

#include "score/mw/com/test/loading_add_on_configuration/test_constants.h"

#include "score/mw/com/test/loading_add_on_configuration/provider.h"
#include "types/example_interface.h"

#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/process_synchronizer.h"
#include "score/mw/com/test/common_test_resources/skeleton_container.h"

#include <iostream>

namespace score::mw::com::test
{

void run_provider(const score::cpp::stop_token& stop_token,
                  const score::mw::com::InstanceSpecifier& instance_specifier,
                  ProcessSynchronizer& done_synchronizer,
                  ProcessSynchronizer& provider_ready_synchronizer,
                  const std::vector<std::uint32_t>& samples)
{
    const auto cycle_time = std::chrono::milliseconds(score::mw::com::test::kCycleTimeMs);

    // Step 1. Create skeleton
    std::cout << "\nProvider: Step 1 - Create skeleton" << std::endl;
    SkeletonContainer<ExampleInterfaceSkeleton> skeleton_container{};
    skeleton_container.CreateSkeleton(instance_specifier, "provider");

    auto& service = skeleton_container.GetSkeleton();

    // Step 2. Offer service
    std::cout << "\nProvider: Step 2 - Offer service" << std::endl;
    skeleton_container.OfferService("provider");

    // Step 3. Signal consumer that we are ready
    std::cout << "\nProvider: Step 3 - Informing consumer that we are ready" << std::endl;
    provider_ready_synchronizer.Notify();

    // Step 4. Send data
    int sample_counter = 0;
    for (std::size_t cycle = 0U;
         (cycle < score::mw::com::test::kTotalNumValuesToSend || score::mw::com::test::kTotalNumValuesToSend == 0U) &&
         !stop_token.stop_requested();
         ++cycle)
    {
        {
            const auto send_result = service.example_event.Send(samples[sample_counter++]);
            if (!send_result.has_value())
            {
                FailTest("Unable to send data. Exiting.");
            }
        }
        if (sample_counter >= score::mw::com::test::kTotalNumValuesToSend)
        {
            sample_counter = 0;
        }
        std::this_thread::sleep_for(cycle_time);
    }

    // Step 5. Wait until consumer signals done
    std::cout << "\nProvider: Step 5 - Wait for consumer done notification" << std::endl;
    if (!done_synchronizer.WaitWithAbort(stop_token))
    {
        FailTest("Provider: WaitWithAbort (done) was stopped by stop_token instead of notification");
    }
    // Reset synchronizer for subsequent calls
    done_synchronizer.Reset();
}

}  // namespace score::mw::com::test
