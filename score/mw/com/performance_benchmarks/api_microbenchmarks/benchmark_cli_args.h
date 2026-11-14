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
#ifndef SCORE_MW_COM_PERFORMANCE_BENCHMARKS_MICRO_BENCHMARK_BENCHMARK_CLI_ARGS_H
#define SCORE_MW_COM_PERFORMANCE_BENCHMARKS_MICRO_BENCHMARK_BENCHMARK_CLI_ARGS_H

#include <string>

namespace score::mw::com::test
{

/// \brief Command-line configuration shared by the mw::com LoLa micro benchmarks that need to create a
/// skeleton (and, for some benchmarks, also a proxy) against a configurable mw_com config file / instance
/// specifier(s), e.g. to compare CoherencyType::kNonCoherent / kCoherent / kMixed configurations without
/// having to recompile the benchmark binary for each variant.
struct BenchmarkCliArgs
{
    /// \brief Path of the mw_com configuration JSON file to load.
    std::string config_path;
    /// \brief Instance specifier used by the benchmark's skeleton (provider side).
    std::string skeleton_instance_specifier{"test/lolabenchmark_non_coherent"};
    /// \brief Instance specifier used by the benchmark's proxy (consumer side). Only relevant for
    /// benchmarks that create both a skeleton and a proxy (e.g. GetNewSamples()/GetNumNewSamplesAvailable()
    /// benchmarks); ignored by benchmarks that only create a skeleton (e.g. the Allocate()/Send() benchmark).
    std::string proxy_instance_specifier{"test/lolabenchmark_non_coherent"};
};

/// \brief Parses the mw::com-benchmark-specific command line arguments
/// (`--config=<path>`, `--skeleton-instance-specifier=<specifier>`, `--proxy-instance-specifier=<specifier>`)
/// out of `argv`, removing them in place.
///
/// \details Must be called before `benchmark::Initialize()`: it compacts `argv`/`argc` down to the
/// arguments it did not recognize (e.g. google-benchmark's own `--benchmark_*` flags), so that
/// `benchmark::Initialize()` only sees arguments it understands afterwards. `--config` is mandatory;
/// if it is missing, this function prints a usage message to stderr and terminates the process. The two
/// instance-specifier arguments are optional and default to "test/lolabenchmark_non_coherent" if not given.
///
/// \param argc Number of entries in argv; updated in place to reflect the remaining (non mw::com-benchmark)
/// arguments.
/// \param argv Command line arguments (argv[0] is the program name and is always kept); updated in place.
/// \return The parsed arguments.
BenchmarkCliArgs ParseBenchmarkCliArgs(int& argc, char** argv);

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_PERFORMANCE_BENCHMARKS_MICRO_BENCHMARK_BENCHMARK_CLI_ARGS_H
