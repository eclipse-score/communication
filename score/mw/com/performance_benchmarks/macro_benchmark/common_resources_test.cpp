/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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
#include "score/mw/com/performance_benchmarks/macro_benchmark/common_resources.h"

#include <score/stop_token.hpp>

#include <gtest/gtest.h>
#include <string_view>

namespace score::mw::com::test
{
namespace
{
constexpr std::string_view kDummyLogContext{"DLC"};

/*
 * using ::testing::_;
 * using ::testing::Return;
 */

TEST(CommonResources, GetStopTokenAndSetUpSigTermHandlerTest)
{
    // given a stop source
    score::cpp::stop_source stop_source;
    // Then GetStopTokenAndSetUpSigTermHandler returns true
    ASSERT_TRUE(GetStopTokenAndSetUpSigTermHandler(stop_source));
}

TEST(CommonResources, ParseCommandLineArgsTwoArgsTest)
{
    std::string_view config_path{"path/to/config.json"};
    std::string_view service_instance_manifest_path{"path/to/service_instance_manifest.json"};

    // given  two positional arguments
    constexpr int argc = 3;
    const char* argv[argc]{"prog_name", config_path.data(), service_instance_manifest_path.data()};

    // Then ParseCommandLineArgs returns those arguments packaged in the TestCommanLineArguments struct
    auto parsed_args = ParseCommandLineArgs(argc, argv, kDummyLogContext);
    ASSERT_EQ(parsed_args.value().config_path, config_path);
    ASSERT_EQ(parsed_args.value().service_instance_manifest, service_instance_manifest_path);
}

TEST(CommonResources, ParseCommandLineArgsNoArgsTest)
{
    // given no positional arguments
    constexpr int argc = 1;
    const char* argv[argc]{"prog_name"};

    // Then ParseCommandLineArgs will return an empty optional
    auto parsed_args = ParseCommandLineArgs(argc, argv, kDummyLogContext);
    ASSERT_FALSE(parsed_args.has_value());
}

TEST(CommonResources, ParseCommandLineArgsInsufficientArgsTest)
{
    std::string_view config_path{"path/to/config.json"};

    // given only one positional arguments
    constexpr int argc = 2;
    const char* argv[argc]{"prog_name", config_path.data()};

    // Then ParseCommandLineArgs will return an empty optional
    auto parsed_args = ParseCommandLineArgs(argc, argv, kDummyLogContext);
    ASSERT_FALSE(parsed_args.has_value());
}

}  // namespace
}  // namespace score::mw::com::test
