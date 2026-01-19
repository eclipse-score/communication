# Micro Benchmarks for `mw::com`'s public API

## Overview

## Purpose
This module benchmarks the public interface `mw::com`. Using the Google benchmark framework.

## Available Microbenchmarks

Currently, the following microbenchmarks are available:

1. **`lola_public_api_benchmarks`** - Benchmarks `InstanceSpecifier::Create()` API
2. **`lola_get_num_new_samples_available_benchmark`** - Benchmarks the `GetNumNewSamplesAvailable()` API

> [!NOTE]
> Additional microbenchmarks for other COM API operations will be added in future updates.

## How-to-use

Build is supported in both host and QNX target environments.

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

### Running on QNX target
> [!TIP]
> Before building, update the config file path in the C++ source to use just `mw_com_config.json` instead of an absolute path for easier deployment.

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
scp score/mw/com/performance_benchmarks/api_microbenchmarks/config/mw_com_config.json \
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
