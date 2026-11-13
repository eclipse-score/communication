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
#include "score/mw/com/runtime_configuration.h"
#include "score/mw/com/test/common_test_resources/assert_handler.h"
#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"
#include "score/mw/com/test/skeleton_reoffer/consumer.h"
#include "score/mw/com/test/skeleton_reoffer/provider.h"
#include "score/mw/com/test/skeleton_reoffer/test_parameters.h"

#include <score/stop_token.hpp>

#include <cstdlib>
#include <future>
#include <iostream>

int main(int argc, const char** argv)
{
    const auto test_parameters = score::mw::com::test::skeleton_reoffer::ReadCommandLineArguments(argc, argv);

    score::mw::com::test::SetupAssertHandler();
    score::mw::com::runtime::InitializeRuntime(
        score::mw::com::runtime::RuntimeConfiguration{test_parameters.service_instance_manifest});

    score::cpp::stop_source stop_source{};
    const bool sig_term_handler_setup_success = score::mw::com::SetupStopTokenSigTermHandler(stop_source);
    if (!sig_term_handler_setup_success)
    {
        std::cerr << "Unable to set signal handler for SIGINT and/or SIGTERM, cautiously continuing\n";
    }

    auto provider_future = std::async(std::launch::async,
                                      score::mw::com::test::skeleton_reoffer::run_provider,
                                      test_parameters.scenario,
                                      stop_source.get_token());
    auto consumer_future = std::async(std::launch::async,
                                      score::mw::com::test::skeleton_reoffer::run_consumer,
                                      test_parameters.scenario,
                                      stop_source.get_token());

    provider_future.get();
    consumer_future.get();

    return EXIT_SUCCESS;
}
