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
#include "score/mw/com/performance_benchmarks/api_microbenchmarks/benchmark_cli_args.h"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace score::mw::com::test
{

namespace
{

constexpr std::string_view kConfigFlagPrefix{"--config="};
constexpr std::string_view kSkeletonInstanceSpecifierFlagPrefix{"--skeleton-instance-specifier="};
constexpr std::string_view kProxyInstanceSpecifierFlagPrefix{"--proxy-instance-specifier="};

/// \brief If arg starts with flag_prefix, assigns the remainder of arg (the flag's value) to out and
/// returns true. Otherwise leaves out untouched and returns false.
bool ConsumeFlag(const std::string_view arg, const std::string_view flag_prefix, std::string& out)
{
    if (arg.substr(0, flag_prefix.size()) == flag_prefix)
    {
        out = std::string{arg.substr(flag_prefix.size())};
        return true;
    }
    return false;
}

void PrintUsageAndExit(const char* const program_name)
{
    std::cerr << "Usage: " << program_name
              << " --config=<path/to/mw_com_config.json> [--skeleton-instance-specifier=<specifier>] "
                 "[--proxy-instance-specifier=<specifier>] [google-benchmark flags...]\n";
    std::exit(EXIT_FAILURE);
}

}  // namespace

BenchmarkCliArgs ParseBenchmarkCliArgs(int& argc, char** argv)
{
    BenchmarkCliArgs args{};
    bool config_path_set{false};

    int write_index{1};  // argv[0] (program name) is always kept.
    for (int read_index = 1; read_index < argc; ++read_index)
    {
        const std::string_view arg{argv[read_index]};
        if (ConsumeFlag(arg, kConfigFlagPrefix, args.config_path))
        {
            config_path_set = true;
            continue;
        }
        if (ConsumeFlag(arg, kSkeletonInstanceSpecifierFlagPrefix, args.skeleton_instance_specifier))
        {
            continue;
        }
        if (ConsumeFlag(arg, kProxyInstanceSpecifierFlagPrefix, args.proxy_instance_specifier))
        {
            continue;
        }
        // Not one of ours - keep it (e.g. a --benchmark_* flag meant for google-benchmark's own Initialize()).
        argv[write_index] = argv[read_index];
        ++write_index;
    }
    argc = write_index;

    if (!config_path_set)
    {
        PrintUsageAndExit(argv[0]);
    }

    return args;
}

}  // namespace score::mw::com::test
