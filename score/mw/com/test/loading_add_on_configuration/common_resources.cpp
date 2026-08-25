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
#include "score/mw/com/test/loading_add_on_configuration/common_resources.h"

#include "score/mw/com/test/common_test_resources/command_line_parser.h"

namespace score::mw::com::test
{

TestConfig ParseConfig(int argc, const char** argv)
{
    constexpr auto kServiceInstanceManifestArg = "service-instance-manifest";

    const std::vector<std::pair<std::string, std::string>> parameter_description_pairs{
        {kServiceInstanceManifestArg, "Path to the service instance manifest"},
    };

    const auto args = ParseCommandLineArguments(argc, argv, parameter_description_pairs);

    const auto manifest_result = GetValueIfProvided<std::string>(args, kServiceInstanceManifestArg);
    if (!manifest_result.has_value())
    {
        FailTest("Missing or invalid --", kServiceInstanceManifestArg, " argument");
    }

    return TestConfig{manifest_result.value()};
}

}  // namespace score::mw::com::test
