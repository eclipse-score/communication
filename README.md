<!---- 
*******************************************************************************
Copyright (c) 2026 Contributors to the Eclipse Foundation

See the NOTICE file(s) distributed with this work for additional
information regarding copyright ownership.

This program and the accompanying materials are made available under the
terms of the Apache License Version 2.0 which is available at
https://www.apache.org/licenses/LICENSE-2.0

SPDX-License-Identifier: Apache-2.0
*******************************************************************************
-->

# Communication Module: mw::com (with multiple bindings including LoLa), memory::shared, and message_passing

[![Eclipse Score](https://img.shields.io/badge/Eclipse-Score-orange.svg)](https://eclipse-score.github.io/score/main/modules/communication/index.html)

High-performance, safety-critical communication middleware for automotive safety standards up to ASIL-B with:

- **Multi-Threading Support**: Thread-safe operations with atomic data structures
- **Multi-Platform**: Supports Linux and QNX operating systems

## Overview

The module consists of three components (also known as safety elements out of context (SEooC)).

### 1. LoLa/mw::com (High-Level Middleware)

The mw::com (also known as **LoLa** - Low Latency) is an open-source implementation that provides:

This provides zero-copy, shared-memory based inter-process communication (IPC) in embedded systems.

- **Service Discovery**: Automatic service registration and discovery
- **Event/Field Communication**: Publish-subscribe messaging patterns
- **Method Invocation**: Remote procedure call (RPC) support
- **Quality of Service**: ASIL-B and QM (Quality Management) support
- **Zero-Copy**: Shared-memory based data exchange for minimal latency within ECUs
- **Tracing Infrastructure**: Zero-copy, binding-agnostic communication tracing support

### 2. Message Passing (Low-Level Foundation)

- **Asynchronous Communication**: Non-blocking message exchange
- **Multi-Channel**: Multiple senders to single receiver communication (unidirectional n-to-1)
- **OS Abstraction**: POSIX and QNX-specific implementations

### 3. Shared memory abstraction (Low-Level Foundation)

## Getting Started

### Prerequisites

- **C++ Compiler**: GCC 12+ or Clang with C++17 support
- **Build System**: 8.6 <= Bazel < 9.x (only with Bzlmod)
- **Operating System**: Linux (Ubuntu 24.04+) or QNX
- **Virtualization**: Docker with user permissions or rootless mode

### DevContainer Setup(Recommended)

>**Note**:
> This repository offers a [DevContainer](https://containers.dev/).
> For setting this up and enabling code completion read [eclipse-score/devcontainer/README.md#inside-the-container](https://github.com/eclipse-score/devcontainer/blob/main/README.md#inside-the-container).

>**Note**:
> If you are using Docker on Windows **without `WSL2`** in between, you have to select the alternative container `eclipse-s-core-docker-on-windows`.

### Building the Project

See [CONTRIBUTING.md](CONTRIBUTING.md).

## Project Structure

```
communication/
├── score/mw/com/  # Main communication middleware (design,code,tests,examples)
├── third_party/   # External dependencies
├── bazel/         # Build configuration
└── BUILD          # Root build file
```

## Documentation

### For Users

- [User Guide](score/mw/com/README.md) - Getting started with the API
- [API Reference](score/mw/com/design/README.md) - Detailed API documentation
- [Tutorial](https://eclipse-score.github.io/communication/latest/score/mw/com/doc/tutorial/README.html) - Guided user-facing API walkthrough

### For Developers

- [Architecture Guide](score/mw/com/design/README.md) - System architecture overview
- [Service Discovery Design](score/mw/com/design/service_discovery/README.md) - Service discovery implementation
- [Message Passing Design](score/mw/com/message_passing/design/README.md) - Low-level messaging details
- [Safety Requirements](score/mw/com/doc/assumptions/README.md) - Safety assumptions and requirements

## Contributing

We welcome contributions! See our [Contributing Guide](CONTRIBUTING.md) for details.

## Release model

### Versioning

This repository follows [Semantic Versioning](https://semver.org/).

### Cadence

As of today, there is no fixed cadance on how often we publish a new release. If you need a new release, or a bug fix of a previously published release without new changes, please create an issue.

### Distribution

Releases are available in the [GitHub releases page of this repository](https://github.com/eclipse-score/communication/releases) and published [as a Bazel module in the Eclipse S-CORE Bazel registry](https://eclipse-score.github.io/bazel_registry_ui/modules/score_communication).

## Safety qualification

The safety qualification of the different SEooCs is not provided but we strive to provide the necesary safety artifacts required for a safety qualification.
We do that by making use of [S-CORE tooling](https://eclipse-score.github.io/tooling/latest/).

The artifacts can be found in:

- [Quality reports](https://eclipse-score.github.io/communication/latest/quality_reports.html)
- Individual components documentation
  - [mw::com](https://eclipse-score.github.io/communication/latest/docs/sphinx/mw_com_index/index.html)
  - [message passing](https://eclipse-score.github.io/communication/latest/docs/sphinx/dependable_element_message_passing_index/index.html)
  - [shared memory](https://eclipse-score.github.io/communication/latest/docs/sphinx/dependable_element_memory_shared_index/index.html)

## Support

### Community

- **Issues**: Report bugs and request features via [GitHub Issues](https://github.com/eclipse-score/communication/issues)
- **Discussions**: Join community [discussions](https://github.com/eclipse-score/communication/discussions) on the Eclipse forums
- **Documentation**: Comprehensive docs in the [`design/`](score/mw/com/design/) and [`doc/`](score/mw/com/doc/) directories

### Flaky tests

Non-deterministic behavior is not desired in safety critical systems. The first step to prevent and fix such behavior, is to be able to detect it.

Because of that, the tests of this project are run every night multiple times to detect such behavior. Any thest that sometimes passes and sometimes failes is considered a flaky tests and [an issue is created automatically](https://github.com/eclipse-score/communication/issues?q=is%3Aissue+state%3Aopen+label%3Aflaky-test).

A test migh be flaky for different reasons. The non-deterministic behavior could come from the software but also from the test itself or the infrastructure. For this reason, if you suspect non-deterministic behavior in the software comming from this project, please create a separate issue even if it might be already reported by one of the automatically created tickets. The analysis of user created issues about non-deterministic behavior will be prioritzed over the automatically created issues.

---

**Note**: This is an open-source project under the Eclipse Foundation. It implements automotive-grade communication middleware suitable for safety-critical applications.
