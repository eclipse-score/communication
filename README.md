# Communication Module (LoLa)

[![Eclipse Score](https://img.shields.io/badge/Eclipse-Score-orange.svg)](https://eclipse-score.github.io/score/main/modules/communication/index.html)

- A high-performance, safety-critical communication middleware implementation based on the Adaptive AUTOSAR Communication Management specification. 
- This module provides zero-copy, shared-memory based inter-process communication (IPC) in embedded systems.

## Overview

The Communication Module (also known as **LoLa** - Low Latency) is an open-source implementation that provides:

- **High-Performance Intra-ECU IPC**: Zero-copy shared-memory communication for minimal latency within ECUs
- **AUTOSAR Compliance**: Partial implementation of Adaptive AUTOSAR Communication Management (ara::com)
- **Event-Driven Architecture**: Publisher/subscriber pattern with skeleton/proxy framework
- **Service Discovery**: Flag file-based service registration and lookup mechanism
- **Safety-Critical**: Designed for automotive safety standards (ASIL-B qualified)
- **Multi-Threading Support**: Thread-safe operations with atomic data structures
- **Memory Management**: Custom allocators optimized for shared memory usage
- **Tracing Infrastructure**: Zero-copy, binding-agnostic communication tracing support
- **Multi-Platform**: Supports Linux and QNX operating systems

## Architecture

The module consists of two main components:

### 1. LoLa/mw::com (High-Level Middleware)
- **Service Discovery**: Automatic service registration and discovery [`score/mw/com/impl/find_service_handler.h`](score/mw/com/impl/find_service_handler.h)
- **Event/Field Communication**: Publish-subscribe messaging patterns [`score/mw/com/impl/generic_proxy_event.h`](score/mw/com/impl/generic_proxy_event.h)
- **Method Invocation**: Remote procedure call (RPC) support [`score/mw/com/impl/generic_proxy.h`](score/mw/com/impl/generic_proxy.h)
- **Quality of Service**: ASIL-B and QM (Quality Management) support [`score/mw/com/types.h`](score/mw/com/types.h)
- **Zero-Copy**: Shared-memory based data exchange [`score/mw/com/message_passing/`](score/mw/com/message_passing/)

### 2. Message Passing (Low-Level Foundation)
- **Asynchronous Communication**: Non-blocking message exchange [`score/mw/com/message_passing/design/`](score/mw/com/message_passing/design/)
- **Multi-Channel**: Multiple senders to single receiver communication (unidirectional n-to-1) [`score/mw/com/message_passing/`](score/mw/com/message_passing/)
- **OS Abstraction**: POSIX and QNX-specific implementations [`score/mw/com/message_passing/mqueue/`](score/mw/com/message_passing/mqueue/) | [`score/mw/com/message_passing/qnx/`](score/mw/com/message_passing/qnx/)
- **Message Types**: Support for short messages (~8 bytes payload) and medium messages (~16 bytes payload) [`score/mw/com/message_passing/design/`](score/mw/com/message_passing/design/)

## System Flow Diagram

### Intra-ECU communication

<img src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/main/score/mw/com/doc/assumptions/lola_flowdiagram.puml">

```
Flow Steps:
1. Publisher registers service with unique identifier
2. Subscriber searches for services by identifier  
3. Service discovery matches publisher and subscriber
4. Publisher sends data to shared memory (zero-copy)
5. Subscriber receives notification of new data
6. Data transferred via direct shared memory access
7. OS provides underlying memory management and synchronization
```
> **Note**: Inter-ECU communication via [SOME/IP](https://github.com/eclipse-score/score/issues/914) is under architectural planning. The block diagram will be updated post-implementation.

## Communication Patterns

### Pattern 1: Simple Event Publishing
```
For example:
Sensor App ──► [Temperature Data] ──► Dashboard App
                (30ms intervals)      (Real-time display)
```

### Pattern 2: Multi-Subscriber Broadcasting  
```
For example:
Camera App ──► [Video Frame] ──┬──► Display App
                               ├──► Recording App  
                               └──► AI Processing App
```

## Getting Started

### Prerequisites
- **C++ Compiler**: GCC 12+ with C++17 support
- **Build System**: Bazel 6.0+
- **Operating System**: Linux (Ubuntu 24.04+) or QNX
- **Dependencies**: GoogleTest, Google Benchmark

### DevContainer Setup(Recommended)

>**Note**:
> This repository offers a [DevContainer](https://containers.dev/).
> For setting this up and enabling code completion read [eclipse-score/devcontainer/README.md#inside-the-container](https://github.com/eclipse-score/devcontainer/blob/main/README.md#inside-the-container).

>**Note**:
> If you are using Docker on Windows **without `WSL2`** in between, you have to select the alternative container `eclipse-s-core-docker-on-windows`.

### Building the Project

```bash
# Clone the repository
git clone <repository-url>
cd communication

# Build all targets
bazel build //...

# Run tests
bazel test //...

# Build specific component
bazel build //score/mw/com:all
```

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
- [Examples](score/mw/com/example/) - Code examples and tutorials

### For Developers
- [Architecture Guide](score/mw/com/design/README.md) - System architecture overview
- [Service Discovery Design](score/mw/com/design/service_discovery/README.md) - Service discovery implementation
- [Message Passing Design](score/mw/com/message_passing/design/README.md) - Low-level messaging details
- [Safety Requirements](score/mw/com/doc/assumptions/README.md) - Safety assumptions and requirements

## Contributing

We welcome contributions! See our [Contributing Guide](CONTRIBUTING.md) for details.

## Support

### Community
- **Issues**: Report bugs and request features via [GitHub Issues](https://github.com/eclipse-score/communication/issues)
- **Discussions**: Join community [discussions](https://github.com/eclipse-score/communication/discussions) on the Eclipse forums
- **Documentation**: Comprehensive docs in the [`design/`](score/mw/com/design/) and [`doc/`](score/mw/com/doc/) directories

---

**Note**: This is an open-source project under the Eclipse Foundation. It implements automotive-grade communication middleware suitable for safety-critical applications.
