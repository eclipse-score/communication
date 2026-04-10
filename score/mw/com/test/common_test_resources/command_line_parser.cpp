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

#include "score/mw/com/test/common_test_resources/command_line_parser.h"
#include <vector>

namespace score::mw::com::test
{
auto ParseCommandLineArguments(int argc,
                               const char** argv,
                               const std::vector<std::pair<std::string, std::string>>& param_names)
    -> CommandLineArgsMapType
{
    // uid is needed internally by sctftestrunner

    namespace po = boost::program_options;
    po::options_description options;
    options.add_options()("help,h", "display the help message");
    for (const auto& [param_name, param_doc] : param_names)
    {
        options.add_options()(
            param_name.data(), po::value<std::string>(), ("specify " + std::string{param_doc}).data());
    }

    po::variables_map args;
    const auto parsed_args =
        po::command_line_parser{argc, argv}
            .options(options)
            .allow_unregistered()
            .style(po::command_line_style::unix_style | po::command_line_style::allow_long_disguise)
            .run();
    po::store(parsed_args, args);

    if (args.count("help") > 0U)
    {
        std::cerr << options << '\n';
        return args;
    }

    po::notify(args);

    return args;
}

}  // namespace score::mw::com::test
