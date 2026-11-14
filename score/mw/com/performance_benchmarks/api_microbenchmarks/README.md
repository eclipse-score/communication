<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# Micro Benchmarks for `mw::com`'s public API

## Overview

## Purpose
This module benchmarks the public interface `mw::com`. Using the Google benchmark framework.

## Available Microbenchmarks

Currently, the following microbenchmarks are available:

1. **`lola_public_api_benchmarks`** - Benchmarks `InstanceSpecifier::Create()` API
2. **`lola_get_num_new_samples_available_benchmark`** - Benchmarks the `GetNumNewSamplesAvailable()` API
3. **`lola_get_new_samples_benchmark`** - Benchmarks the `GetNewSamples()` API
4. **`lola_allocate_send_benchmark`** - Benchmarks the `Allocate()`/`Send()` APIs

> [!NOTE]
> Additional microbenchmarks for other COM API operations will be added in future updates.

## Configuration

Two config files are provided in `config/`: `mw_com_config_qm.json` (ASIL-QM) and `mw_com_config_asil_b.json`
(ASIL-B). Each defines three `serviceInstances` entries (sharing the same physical shared-memory region),
differing only in their event's `coherency` setting:

- `test/lolabenchmark_non_coherent` - `coherency` not set, defaults to `NON_COHERENT`
- `test/lolabenchmark_coherent` - `coherency` set to `COHERENT`
- `test/lolabenchmark_mixed_coherent` - `coherency` set to `MIXED` (only valid on the provider/skeleton side)

`lola_get_num_new_samples_available_benchmark` and `lola_get_new_samples_benchmark` create both a skeleton
and a proxy, so e.g. a `MIXED` provider can be combined with a `COHERENT` or `NON_COHERENT` consumer by
passing different instance specifiers for each side (see command-line arguments below).

## How-to-use

Build is supported in both host and QNX target environments.

### Command-line arguments

`lola_get_num_new_samples_available_benchmark`, `lola_get_new_samples_benchmark`, and
`lola_allocate_send_benchmark` accept the following custom command-line arguments (in addition to the usual
`--benchmark_*` flags understood by Google Benchmark):

- `--config=<path/to/mw_com_config_{qm,asil_b}.json>` - mandatory, path of the config file to load.
- `--skeleton-instance-specifier=<specifier>` - optional, instance specifier used by the skeleton
  (provider side). Defaults to `test/lolabenchmark_non_coherent`.
- `--proxy-instance-specifier=<specifier>` - optional, instance specifier used by the proxy (consumer side).
  Only parsed by `lola_get_num_new_samples_available_benchmark` and `lola_get_new_samples_benchmark`.
  Defaults to `test/lolabenchmark_non_coherent`.

`lola_public_api_benchmarks` does not accept these arguments; it only initializes the runtime with
`mw_com_config_asil_b.json`.

### Running on the host

> [!important]
> Tests should only be run on the host for quick developer feedback during development. For actual data collection host
> runs might be unreliable.
> In case of local host runs, CPU frequency scaling should be disabled. Otherwise, the execution times might be
> inconsistent between runs.


The following command can be used to run the benchmark on the host:

```bash
bazel run --config=spp_host_clang //score/mw/com/performance_benchmarks/api_microbenchmarks:lola_public_api_benchmarks --nocache_test_results --compilation_mode=opt --cxxopt=-O3
```

To run one of the benchmarks that accept command-line arguments (e.g. with a `MIXED` provider and a
`COHERENT` consumer):

```bash
bazel run --config=spp_host_clang //score/mw/com/performance_benchmarks/api_microbenchmarks:lola_get_new_samples_benchmark --nocache_test_results --compilation_mode=opt --cxxopt=-O3 -- \
  --config=$(pwd)/score/mw/com/performance_benchmarks/api_microbenchmarks/config/mw_com_config_qm.json \
  --skeleton-instance-specifier=test/lolabenchmark_mixed_coherent \
  --proxy-instance-specifier=test/lolabenchmark_coherent
```

### Running on QNX target

**Step 1: Build for QNX**
```bash
# For ARM64 QNX7 (IPN10)
bazel build --config=ipn10_qnx7 \
  //score/mw/com/performance_benchmarks/api_microbenchmarks:lola_public_api_benchmarks \
  --compilation_mode=opt \
  --cxxopt=-O3
```

**Step 2: Deploy to target**
```bash
# Copy binary to QNX device
scp bazel-bin/score/mw/com/performance_benchmarks/api_microbenchmarks/lola_public_api_benchmarks \
  root@ipnext:/tmp/

# Copy configuration file to QNX device
scp score/mw/com/performance_benchmarks/api_microbenchmarks/config/mw_com_config_asil_b.json \
  root@ipnext:/tmp/
```

**Step 3: Run on target**
```bash
# SSH to QNX device
ssh root@ipnext

#move to /tmp directory
cd /tmp

#change permission to execute
chmod +x lola_public_api_benchmarks

# Execute the benchmark
./lola_public_api_benchmarks
```

> [!NOTE]
> The `lola_get_num_new_samples_available_benchmark` can be run in a similar way on both host and QNX target by replacing the benchmark name in the commands above.
