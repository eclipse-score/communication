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
#ifndef SCORE_MW_COM_PERFORMANCE_BENCHMARKS_MACRO_BENCHMARK_CONFIG_PARSER_H
#define SCORE_MW_COM_PERFORMANCE_BENCHMARKS_MACRO_BENCHMARK_CONFIG_PARSER_H

#include "score/json/json_parser.h"
#include "score/mw/com/performance_benchmarks/macro_benchmark/common_resources.h"
#include "score/mw/log/logging.h"

#include <string_view>

namespace score::mw::com::test
{

enum struct ServiceFinderMode : unsigned int
{
    POLLING = 0,
    ASYNC
};

enum struct DurationUnit : unsigned int
{
    ms = 0,
    s,
    sample_count
};

struct ClientConfig
{
    int read_cycle_time_ms;
    unsigned int number_of_clients;
    unsigned int max_num_samples;
    ServiceFinderMode service_finder_mode;
    struct RunTimeLimit
    {
        unsigned int duration;
        DurationUnit unit;
    };
    std::optional<RunTimeLimit> run_time_limit;
};

struct ServiceConfig
{
    unsigned int send_cycle_time_ms;
    unsigned int number_of_clients;
};

ClientConfig ParseClientConfig(std::string_view path, std::string_view log_context);
ServiceConfig ParseServiceConfig(std::string_view path, std::string_view log_context);

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_PERFORMANCE_BENCHMARKS_MACRO_BENCHMARK_CONFIG_PARSER_H
