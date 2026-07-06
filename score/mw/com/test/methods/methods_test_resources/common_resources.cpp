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
#include "score/mw/com/test/methods/methods_test_resources/common_resources.h"

#include "score/mw/com/test/common_test_resources/command_line_parser.h"

namespace score::mw::com::test
{

std::string ParseServiceInstanceManifest(int argc, const char** argv)
{
    const std::string service_instance_manifest_name = "service_instance_manifest";

    auto args = ParseCommandLineArguments(argc, argv, {{service_instance_manifest_name, ""}});
    return GetValue<std::string>(args, service_instance_manifest_name);
}

}  // namespace score::mw::com::test
