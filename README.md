# Communication Module (LoLa)

[![Eclipse Score](https://img.shields.io/badge/Eclipse-Score-orange.svg)](https://eclipse-score.github.io/score/main/modules/communication/index.html)

A high-performance, safety-critical communication middleware implementation based on the Adaptive AUTOSAR Communication Management specification. This module provides zero-copy, shared-memory based inter-process communication (IPC) for intra-ECU communication and for inter-ECU communication using Some/IP(future implementation) in automotive and embedded systems.

## Overview

The Communication Module (also known as **LoLa** - Low Latency) is an open-source implementation that provides:

- **High-Performance Intra-ECU IPC**: Zero-copy shared-memory communication for minimal latency within ECUs
- **AUTOSAR Compliance**: Partial implementation of Adaptive AUTOSAR Communication Management (ara::com)
- **Safety-Critical**: Designed for automotive safety standards (ASIL-B qualified)
- **Multi-Platform**: Supports Linux and QNX operating systems  
- **Multi-Binding Architecture**: Currently implements LoLa (intra-ECU) with architectural preparation for SOME/IP (inter-ECU)

## Architecture

The module consists of two main components:

### 1. LoLa/mw::com (High-Level Middleware)
- **Service Discovery**: Automatic service registration and discovery
- **Event/Field Communication**: Publish-subscribe messaging patterns  
- **Method Invocation**: Remote procedure call (RPC) support
- **Quality of Service**: ASIL-B and QM (Quality Management) support
- **Zero-Copy**: Shared-memory based data exchange

### 2. Message Passing (Low-Level Foundation)
- **Asynchronous Communication**: Non-blocking message exchange
- **Multi-Channel**: Multiple senders to single receiver communication (unidirectional n-to-1)
- **OS Abstraction**: POSIX and QNX-specific implementations
- **Message Types**: Support for short messages (~8 bytes payload) and medium messages (~16 bytes payload)

## System Flow Diagram

### Intra-ECU communication
```
┌─────────────────────────────────────────────────────────────────────────┐
│                          Application Layer                              │
├─────────────────────────────┬───────────────────────────────────────────┤
│        Publisher App        │            Subscriber App                 │
│                             │                                           │
│  ┌─────────────────────┐    │    ┌─────────────────────┐                │
│  │   Data Producer     │    │    │   Data Consumer     │                │
│  │  (e.g., Sensor)     │    │    │ (e.g., Dashboard)   │                │
│  └─────────┬───────────┘    │    └─────────┬───────────┘                │
└───────────┼────────────────────────────────┼────────────────────────────┘
            │                                │                             
            │ 1. OfferService()              │ 2. FindService()           
            ▼                                ▼                             
┌─────────────────────────────────────────────────────────────────────────┐
│                         LoLa Middleware                                 │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                  Service Discovery                              │    │
│  │  • Register services    • Find available services               │    │
│  │  • Manage connections   • Handle service lifecycle              │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                    │                                    │
│                              3. Service Match                           │
│                                    ▼                                    │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                    Event System                                 │    │
│  │  ┌─────────────┐           ┌─────────────┐                      │    │
│  │  │  Publisher  │ 4. Send() │ Subscriber  │ 5. Receive()         │    │
│  │  │   Skeleton  │◄─────────►│   Proxy     │                      │    │
│  │  └─────────────┘           └─────────────┘                      │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                    │                                    │
│                            6. Zero-Copy Transfer                        │
│                                    ▼                                    │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │                  Shared Memory Manager                          │    │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐              │    │
│  │  │   Memory    │  │   Memory    │  │   Memory    │              │    │
│  │  │   Pool 1    │  │   Pool 2    │  │   Pool N    │              │    │
│  │  └─────────────┘  └─────────────┘  └─────────────┘              │    │
│  └─────────────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────────┘
                                    │                                     
                            7. Direct Memory Access                       
                                    ▼                                     
┌─────────────────────────────────────────────────────────────────────────┐
│                        Operating System                                 │
│                      (Linux / QNX Neutrino)                             |
└─────────────────────────────────────────────────────────────────────────┘

Flow Steps:
1. Publisher registers service with unique identifier
2. Subscriber searches for services by identifier  
3. Service discovery matches publisher and subscriber
4. Publisher sends data to shared memory (zero-copy)
5. Subscriber receives notification of new data
6. Data transferred via direct shared memory access
7. OS provides underlying memory management and synchronization
```
> **Note**: Inter-ECU communication via SOME/IP is under architectural planning. The block diagram will be updated post-implementation.TODO

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

## Features

### **Intra-ECU Communication (LoLa - Fully Implemented)**

- **Zero-Copy Shared Memory**: Ultra-low latency communication within ECUs using shared memory
- **AUTOSAR ara::com API**: Partial implementation of Adaptive AUTOSAR Communication Management
- **Event-Driven Architecture**: Publisher/subscriber pattern with skeleton/proxy framework
- **Service Discovery**: Flag file-based service registration and lookup mechanism
- **Safety-Critical Design**: ASIL-B qualified implementation with deterministic behavior
- **Multi-Threading Support**: Thread-safe operations with atomic data structures
- **Memory Management**: Custom allocators optimized for shared memory usage
- **Tracing Infrastructure**: Zero-copy, binding-agnostic communication tracing support

### **Inter-ECU Communication (SOME/IP - Architectural Preparation)**

> **Note**: Inter-ECU communication via SOME/IP is under architectural preparation phase. The features will be updated after implementation.TODO


## Getting Started

### Prerequisites
- **C++ Compiler**: GCC 12+ with C++17 support
- **Build System**: Bazel 6.0+
- **Operating System**: Linux (Ubuntu 24.04+) or QNX
- **Dependencies**: GoogleTest, Google Benchmark

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
├── score/mw/com/                    # Main communication middleware
│   ├── design/                      # Architecture documentation
│   ├── impl/                        # Implementation details
│   │   ├── bindings/lola/          # LoLa binding implementation
│   │   │   ├── service_discovery/  # Service discovery client
│   │   │   ├── messaging/          # Message passing facade
│   │   │   └── test/               # Component tests
│   │   └── configuration/          # Runtime configuration
│   ├── message_passing/             # Low-level messaging
│   │   ├── design/                 # Message passing architecture
│   │   ├── qnx/                    # QNX-specific implementation
│   │   └── mqueue/                 # POSIX message queue implementation
│   ├── doc/                        # Requirements and assumptions
│   └── example/                    # Usage examples
├── third_party/                    # External dependencies
├── bazel/                          # Build configuration
└── BUILD                          # Root build file
```


## Testing

The project includes comprehensive testing:

```bash
# Run all tests
bazel test //...

# Run specific test suites
bazel test //score/mw/com/impl:all
bazel test //score/mw/com/message_passing:all

# Run with coverage
bazel coverage //...
```

Test categories:
- **Unit Tests**: Component-level testing
- **Integration Tests**: Cross-component interaction
- **Performance Tests**: Latency and throughput benchmarks
- **Safety Tests**: ASIL-B compliance verification


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

We welcome contributions to the Communication Module! Please read our contribution guidelines:

1. **Fork** the repository
2. **Create** a feature branch (`git checkout -b feature/amazing-feature`)
3. **Follow** coding standards (C++17, Google style guide)
4. **Add** tests for new functionality
5. **Ensure** all tests pass (`bazel test //...`)
6. **Commit** changes (`git commit -m 'Add amazing feature'`)
7. **Push** to branch (`git push origin feature/amazing-feature`)
8. **Open** a Pull Request

### Development Setup
```bash
# Setup development environment
bazel build //...
bazel test //...

# Format code
clang-format -i $(find . -name "*.cpp" -o -name "*.h")

# Lint code
bazel build //... --config=clang-tidy
```

## Project Information

- **Project Home**: [Eclipse Score](https://projects.eclipse.org/projects/automotive.score)
- **Maintainer**: Eclipse Foundation
- **Category**: Automotive Software
- **Language**: C++17
- **Build System**: Bazel
- **Platforms**: Linux, QNX

## Support

### Community
- **Issues**: Report bugs and request features via GitHub Issues
- **Discussions**: Join community discussions on the Eclipse forums
- **Documentation**: Comprehensive docs in the `design/` and `doc/` directories

---

**Note**: This is an open-source project under the Eclipse Foundation. It implements automotive-grade communication middleware suitable for safety-critical applications.

