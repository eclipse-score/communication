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

#include <charconv>
#include <score/optional.hpp>

#include "score/result/result.h"
#include <string>
#include <type_traits>
#include <vector>

namespace score::mw::com::test
{

using CommandLineArgsMapType = std::unordered_map<std::string, std::string>;

/// \brief Get a value represented as a string, from an arguments map and parse it into the appropriate type
///
/// \tparam ReturnType Target type of the parsed argument. Must be std::string, integral or floating point type.
/// \param args Map of provided command line arguments, where the key is the argument name and the value is the argument
/// value as a string.
/// \param arg_string The name of the argument to retrieve and parse from the map.
/// \return The parsed argument value in case of success, or an error if the argument was not provided or could not be
/// parsed.
template <typename ReturnType>
auto GetValueIfProvided(const CommandLineArgsMapType& args, const std::string& arg_string) -> Result<ReturnType>
{
    if (args.count(arg_string) == 0U)
    {
        std::string error_msg = "could not find the requested parameter: " + arg_string;
        return score::MakeUnexpected<ReturnType>(
            MakeError(TestErrorCode::kParsingCommandLineArgumentFailed, error_msg));
    }

    std::string value_str = args.at(arg_string);

    if constexpr (std::is_same_v<ReturnType, std::string>)
    {
        return value_str;
    }
    else if constexpr (std::is_same_v<ReturnType, bool>)
    {
        if (value_str == "false" || value_str == "0")
        {
            return false;
        }
        if (value_str == "true" || value_str == "1")
        {
            return true;
        }
        return score::MakeUnexpected<ReturnType>(
            MakeError(TestErrorCode::kParsingCommandLineArgumentFailed,
                      "Failed during parsing of: " + arg_string + " . Provided value " + value_str +
                          "could not be parsed into bool! Boolean fields can only have one of the following values: "
                          "true, 1, false, 0 \n"));
    }
    else if constexpr (std::is_integral_v<ReturnType>)
    {

        ReturnType parsed_value{};
        // This pointer arithmetic is required by the from_chars API. This will always point to a valid segment of the
        // string data since we get the size of the string directly from the string object.
        auto conversion_result =
            std::from_chars<ReturnType>(value_str.data(), value_str.data() + value_str.size(), parsed_value);

        if (conversion_result.ec == std::errc::invalid_argument)
        {
            return score::MakeUnexpected<ReturnType>(MakeError(TestErrorCode::kParsingCommandLineArgumentFailed,
                                                               "Failed during parsing of: " + arg_string +
                                                                   " . Provided value " + value_str +
                                                                   "could not be parsed into the required type!\n"));
        }

        if (conversion_result.ec == std::errc::result_out_of_range)
        {
            return score::MakeUnexpected<ReturnType>(MakeError(TestErrorCode::kParsingCommandLineArgumentFailed,
                                                               "Failed during parsing of: " + arg_string +
                                                                   " . Provided value " + value_str +
                                                                   "Is Larger than requested type!\n"));
        }

        if (conversion_result.ec != std::errc{})
        {
            return score::MakeUnexpected<ReturnType>(
                MakeError(TestErrorCode::kParsingCommandLineArgumentFailed,
                          "Failed during parsing of: " + arg_string + " . Provided value " + value_str +
                              "could not be parsed into the required type for an unknown reason!\n"));
        }

        return parsed_value;
    }
    else
    {
        static_assert(false, "Return type can only be a string, bool or an integral type!");
    }
}

/// \brief A generic command line parser which can parse any kind of arguments and return an argument value map.
///
/// Every argument is parsed as a string and stored in the map with the argument name as key.
/// `GetValueIfProvided` can then be used to retrieve the argument value from the map and parse it into the appropriate
/// type.
/// The function will print the help message if the "help" argument is provided.
///
/// \param argc Number of command line arguments. Directly passed from the main function parameters.
/// \param argv Array of command line argument strings. Directly passed from the main function parameters.
/// \param param_names Vector of pairs of argument names and their documentation, which are used to specify which
/// arguments to parse.
/// \return A map of provided argument names and their corresponding values as strings.
auto ParseCommandLineArguments(int argc,
                               const char** argv,
                               const std::vector<std::pair<std::string, std::string>>& param_names)
    -> CommandLineArgsMapType;

}  // namespace score::mw::com::test
#endif  // SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_COMMAND_LINE_PARSER_H
