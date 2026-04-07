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

#ifndef SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_COMMAND_LINE_PARSER_H
#define SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_COMMAND_LINE_PARSER_H

#include "score/mw/com/test/common_test_resources/test_error_domain.h"
#include <boost/program_options.hpp>

#include <score/optional.hpp>

#include "score/result/result.h"
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

namespace score::mw::com::test
{

using CommandLineArgsMapType = boost::program_options::variables_map;

template <typename ParsedType>
auto GetValueIfProvided(const boost::program_options::variables_map& args, const std::string& arg_string)
    -> Result<ParsedType>
{
    std::string error_msg = "could nod find the requested parameter: " + arg_string;
    if constexpr (std::is_same_v<ParsedType, std::string>)
    {
        return (args.count(arg_string) > 0U) ? args[arg_string].as<std::string>()
                                             : score::MakeUnexpected<ParsedType>(MakeError(
                                                   TestErrorCode::kParsingCommandLineArgumentFailed, error_msg));
    }
    if constexpr (std::is_integral_v<ParsedType>)
    {
        return (args.count(arg_string) > 0U) ? static_cast<ParsedType>(std::stoull(args[arg_string].as<std::string>()))
                                             : score::MakeUnexpected<ParsedType>(MakeError(
                                                   TestErrorCode::kParsingCommandLineArgumentFailed, error_msg));
    }
    if constexpr (std::is_floating_point_v<ParsedType>)
    {
        return (args.count(arg_string) > 0U) ? static_cast<ParsedType>(std::stold(args[arg_string].as<std::string>()))
                                             : score::MakeUnexpected<ParsedType>(MakeError(
                                                   TestErrorCode::kParsingCommandLineArgumentFailed, error_msg));
    }

    return score::MakeUnexpected<ParsedType>(
        MakeError(TestErrorCode::kParsingCommandLineArgumentFailed,
                  "Only std::string, integral and floatin point types can be parsed as command line arguments!\n"));
}

auto ParseCommandLineArguments(int argc,
                               const char** argv,
                               const std::vector<std::pair<std::string, std::string>>& param_names)
    -> boost::program_options::variables_map;

}  // namespace score::mw::com::test
#endif  // SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_COMMAND_LINE_PARSER_H
