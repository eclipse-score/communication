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
#include "score/mw/com/test/move_semantics/proxy_event/test_parameters.h"

#include "score/mw/com/test/common_test_resources/command_line_parser.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"

namespace score::mw::com::test
{

CombinedTestConfiguration ReadCommandLineArguments(int argc, const char** argv)
{
    auto args = ParseCommandLineArguments(argc, argv, {{kScenario, ""}, {kServiceInstanceManifest, ""}});

    const auto scenario_index = GetValue<std::size_t>(args, kScenario);
    if (scenario_index >= static_cast<std::size_t>(ProxyMoveScenario::kNumberOfScenarios))
    {
        FailTest("Consumer: ",
                 kScenario,
                 " value ",
                 scenario_index,
                 " is out of range. Valid values are between 0 and ",
                 static_cast<std::uint8_t>(ProxyMoveScenario::kNumberOfScenarios) - 1,
                 ".");
    }
    const auto scenario = static_cast<ProxyMoveScenario>(scenario_index);

    auto service_instance_manifest = GetValue<std::string>(args, kServiceInstanceManifest);

    return {scenario, service_instance_manifest};
}

std::size_t GetNumberOfSendIterations(const ProxyMoveScenario scenario)
{
    if (scenario == ProxyMoveScenario::kMoveConstructBeforeSubscribe)
    {
        // In this scenario, the consumer will:
        //   - Find service and create proxy
        //   - move construct the proxy before subscribe
        //   - subscribe and receive samples
        //   - continue receiving samples after provider re-offers
        // The receiver therefore expects two receive iterations.
        return 2U;
    }

    FailTest("Unknown scenario, cannot determine number of send iterations");
    return 0U;
}

}  // namespace score::mw::com::test
