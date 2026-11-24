<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# Communication Middleware (mw::com)

## Overview

`LoLa`/`mw::com`: A high-level, service-oriented communication middleware for local interprocess / intraprocess communication.

## General introduction

`mw::com` and `LoLa` are have slightly different meanings. The former refers to the binding independent public API which is used by applications. It is 
also the namespace (`mw::com` = middleware communication). The latter refers to the specific shared memory binding (`LoLa` = Low Latency).

Low latency in our `mw::com` implementation is realized by basing our underlying `technical binding` on zero-copy
shared-memory based communication.
`mw::com` defines a user-level API and its behavior. **How** this public API is realized within its lower layers
(`technical binding`) is independent of the API itself.
Thus, `mw::com` can support several different `technical binding`s (e.g. one for local and one for network communication).

## Feature flags

Optional features are hidden behind Bazel feature flags defined in [flags/BUILD](flags/BUILD). Each flag is a
`bool_flag` that defaults to `False` and has a corresponding `--config=<name>` in the repository's `.bazelrc`.
If a flag is disabled, the sources of the feature are not compiled and linked, and the preprocessor define is not set.
`--config=activate_all_experimental_features` enables all of them at once. The CI tests the defaults and this config,
see [CI.md](../../../CI.md#feature-flags).

| Feature                       | Flag                                                       | Config                  | Preprocessor define                         |
|-------------------------------|------------------------------------------------------------|-------------------------|---------------------------------------------|
| FlatBuffers configuration     | `//score/mw/com/flags:experimental_enable_flatbuffers_configuration` | `--config=flatbuffers` | `SCORE_MW_COM_FLATBUFFERS_CONFIGURATION_ENABLED` |
| SOME/IP binding               | `//score/mw/com/flags:experimental_enable_someip_binding`  | `--config=someip`       | `SCORE_MW_COM_SOMEIP_BINDING_ENABLED`       |

## Documentation for users

If you are an adaptive application developer, and you want to use `mw::com` to do local
interprocess communication, you will find the user documentation here: LINK TODO.

Documentation of `mw::com`s public API with usage examples can be found in [doc/user_facing_API_examples.md](doc/user_facing_API_examples.md).
If you want to see an example app, which uses LoLa to establish shared-memory communication between a service and a
client you can take a look at our macro benchmark app which resides in [performance_benchmarks/macro_benchmark](performance_benchmarks/macro_benchmark).

