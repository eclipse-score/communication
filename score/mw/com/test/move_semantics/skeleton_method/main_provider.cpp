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
#include "score/mw/com/test/move_semantics/skeleton_method/provider.h"
#include "score/mw/com/test/move_semantics/skeleton_method/test_parameters.h"

#include <iostream>

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

    std::cout << "Starting provider with scenario " << static_cast<std::size_t>(test_configuration.scenario)
              << std::endl;

    score::mw::com::test::RunProvider(test_configuration.scenario, stop_source.get_token());

    return EXIT_SUCCESS;
}
