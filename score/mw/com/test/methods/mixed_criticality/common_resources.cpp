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

#include "score/mw/com/test/methods/mixed_criticality/common_resources.h"
#include "score/mw/com/test/common_test_resources/command_line_parser.h"
#include "score/mw/com/test/common_test_resources/fail_test.h"

namespace score::mw::com::test
{

auto GetServiceInstanceManifestPath(int argc, const char** argv) -> std::string
{
    std::string service_instance_manifest_name = "service-instance-manifest";

    auto args = ParseCommandLineArguments(argc, argv, {{service_instance_manifest_name, ""}});

    auto get_value_or_crash = [&args](const std::string& name) -> std::string {
        return GetValueIfProvided<std::string>(args, name)
            .or_else([&name](auto&&) -> score::Result<std::string> {
                FailTest("Parser: could not parse ", name, " from command line arguments!");
                return score::Result<std::string>{};
            })
            .value();
    };

    return get_value_or_crash(service_instance_manifest_name);
}

}  // namespace score::mw::com::test
