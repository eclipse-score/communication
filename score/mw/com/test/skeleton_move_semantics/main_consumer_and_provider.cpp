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

#include "score/mw/com/test/common_test_resources/assert_handler.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"
#include "score/mw/com/test/skeleton_move_semantics/consumer.h"
#include "score/mw/com/test/skeleton_move_semantics/provider.h"
#include "score/mw/com/test/skeleton_move_semantics/test_parameters.h"

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

    const auto num_send_iterations = score::mw::com::test::GetNumberOfSendIterations(test_configuration.scenario);

    std::cout << "Starting provider and consumer with scenario "
              << static_cast<std::size_t>(test_configuration.scenario) << ", number of samples to send per offer "
              << score::mw::com::test::kNumberOfSamplesToSendPerOffer << " and number of send iterations"
              << num_send_iterations << std::endl;

    auto provider_future = std::async(score::mw::com::test::RunProvider,
                                      test_configuration.scenario,
                                      score::mw::com::test::kNumberOfSamplesToSendPerOffer,
                                      stop_source.get_token());
    auto consumer_future = std::async(score::mw::com::test::RunConsumer,
                                      score::mw::com::test::kInstanceSpecifierMovedTo,
                                      score::mw::com::test::kNumberOfSamplesToSendPerOffer,
                                      num_send_iterations,
                                      stop_source.get_token());

    if (test_configuration.scenario == score::mw::com::test::SkeletonMoveScenario::kMoveAssignOffered)
    {
        const std::size_t num_moved_to_send_iterations{1U};
        auto moved_from_consumer_future = std::async(score::mw::com::test::RunConsumer,
                                                     score::mw::com::test::kInstanceSpecifierMovedFrom,
                                                     score::mw::com::test::kNumberOfSamplesToSendPerOffer,
                                                     num_moved_to_send_iterations,
                                                     stop_source.get_token());
        moved_from_consumer_future.get();
    }

    provider_future.get();
    consumer_future.get();

    return EXIT_SUCCESS;
}
