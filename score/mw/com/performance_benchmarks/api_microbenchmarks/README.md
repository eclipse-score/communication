# Micro Benchmarks for `mw::com`'s public API

## Overview

## Purpose
This module benchmarks the public interface `mw::com`. Using the Google benchmark framework.

## How-to-use

Currently, only the host build is supported, but we are looking into an easy way of running these benchmarks on QEMU and
on the target hardware.

### Running on the host

> [!important]
> Tests should only be run on the host for quick developer feedback during development. For actual data collection host
> runs might be unreliable.
> In case of local host runs, CPU frequency scaling should be disabled. Otherwise, the execution times might be
> inconsistent between runs.


The following command can be used to run the tests on the host:

```bash
bazel run --config=spp_host_clang //score/mw/com/performance_benchmarks/api_microbenchmarks:lola_public_api_benchmarks --nocache_test_results --compilation_mode=opt --cxxopt=-O3
```


### Run on target

tbd
