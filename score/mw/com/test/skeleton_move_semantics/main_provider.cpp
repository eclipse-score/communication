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
#include "score/mw/com/runtime.h"
#include "score/mw/com/types.h"

#include "score/mw/com/test/common_test_resources/assert_handler.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"
#include "score/mw/com/test/skeleton_move_semantics/provider.h"
#include "score/mw/com/test/skeleton_move_semantics/test_parameters.h"

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{

const score::mw::com::InstanceSpecifier kInstanceSpecifierMovedTo =
    score::mw::com::InstanceSpecifier::Create(std::string{"test/skeleton_move_semantics/MoveEventInterfaceMovedTo"})
        .value();
const score::mw::com::InstanceSpecifier kInstanceSpecifierMovedFrom =
    score::mw::com::InstanceSpecifier::Create(std::string{"test/skeleton_move_semantics/MoveEventInterfaceMovedFrom"})
        .value();

}  // namespace

int main(int argc, const char** argv)
{
    auto test_configuration{score::mw::com::test::ReadCommandLineArguments(argc, argv)};

    score::mw::com::test::SetupAssertHandler();
    score::mw::com::runtime::InitializeRuntime(argc, argv);

    score::cpp::stop_source stop_source{};
    const bool sig_term_handler_setup_success = score::mw::com::SetupStopTokenSigTermHandler(stop_source);
    if (!sig_term_handler_setup_success)
    {
        std::cerr << "Unable to set signal handler for SIGINT and/or SIGTERM, cautiously continuing" << std::endl;
    }

    const auto num_send_iterations = [&test_configuration]() {
        if (test_configuration.scenario == score::mw::com::test::SkeletonMoveScenario::kMoveConstructNotOffered)
        {
            // In this scenario, the provider will:
            //   - Create a skeleton
            //   - move construct the skeleton
            //   - offer the service
            //   - send samples
            //   - stop offering the service
            //   - offer the service again
            //   - send samples again
            // The receiver therefore expects two send iterations.
            return 2U;
        }
        else if (test_configuration.scenario == score::mw::com::test::SkeletonMoveScenario::kMoveConstructOffered)
        {
            // In this scenario, the provider will:
            //   - Create a skeleton
            //   - offer the service
            //   - send samples
            //   - move construct the skeleton while the service is offered
            //   - send samples
            //   - stop offering the service
            //   - offer the service again
            //   - send samples again
            // The receiver therefore expects three send iterations.
            return 3U;
        }
        else if (test_configuration.scenario == score::mw::com::test::SkeletonMoveScenario::kMoveAssignNotOffered)
        {
            // In this scenario, the provider will:
            //   - Create 2 skeletons
            //   - move assign the skeleton
            //   - offer the moved-to service
            //   - send samples
            //   - stop offering the service
            //   - offer the service again
            //   - send samples again
            // The receiver therefore expects two send iterations.
            return 2U;
        }
        else if (test_configuration.scenario == score::mw::com::test::SkeletonMoveScenario::kMoveAssignOffered)
        {
            // In this scenario, the provider will:
            //   - Create 2 skeletons
            //   - offer both services
            //   - send samples in both services
            //   - move assign the skeleton
            //   - send samples from moved-to service
            //   - stop offering the moved-to service
            //   - offer the moved-to service again
            //   - send samples again
            // The receiver connected to the moved-from service expects one send iteration. The receiver connected to
            // the moved-to service expects three send iterations.
            return 3U;
        }

        score::mw::com::test::FailTest("Unknown scenario, cannot determine number of send iterations");
        return 0U;
    }();

    std::cout << "Starting provider and consumer with scenario "
              << static_cast<std::size_t>(test_configuration.scenario) << ", number of samples to send per offer "
              << test_configuration.number_of_samples_to_send_per_offer << " and number of send iterations"
              << num_send_iterations << std::endl;

    score::mw::com::test::RunProvider(
        test_configuration.scenario, test_configuration.number_of_samples_to_send_per_offer, stop_source.get_token());

    return EXIT_SUCCESS;
}
