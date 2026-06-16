<!--
Copyright (c) 2026 Contributors to the Eclipse Foundation

See the NOTICE file(s) distributed with this work for additional
information regarding copyright ownership.

This program and the accompanying materials are made available under the
terms of the Apache License Version 2.0 which is available at
https://www.apache.org/licenses/LICENSE-2.0

SPDX-License-Identifier: Apache-2.0
-->

# COM API Example - Vehicle Monitor Application

## Overview

This example application demonstrates the COM (Communication Middleware) API capabilities using a Vehicle Monitor scenario. It showcases producer-consumer communication patterns with the Lola runtime, featuring both synchronous and asynchronous operations.

## Use Case

The application simulates a **Vehicle Monitor System** that tracks two types of events:
- **Tire Pressure Data**: Monitoring tire pressure readings
- **Exhaust Data**: Monitoring exhaust system information

The producer publishes vehicle data, while the consumer subscribes to and processes these events.

## Library Components

The `com-api-example-lib` library is organized into the following modules:

1. **Producer Module** (`producer.rs`)
   - Creates and offers vehicle data services

2. **Consumer Module** (`consumer.rs`)
   - Subscribes to vehicle events and handles them via both synchronous and asynchronous APIs

3. **Shared Enum** (`lib.rs`)
   - Defines `ExampleType`, a shared enum used for CLI parsing, consumer instantiation, and receive-mode dispatch

4. **Vehicle Monitor Module** (`vehicle_monitor.rs`)
   - Type definitions for `VehicleMonitorProducer` and `VehicleMonitorConsumer`


## Main Application

The main application (`main.rs`) demonstrates:

### Multi-threaded Architecture
- **Producer Thread**: Sends vehicle data at regular intervals
- **Consumer Thread**: Receives and processes data independently

### Async Operations
- Uses `futures::executor::block_on()` for async operations
- Supports multiple execution modes via command-line arguments

## Command-Line Options

### Main Application

```bash
bazel run //score/mw/com/example/com-api-example:com-api-example -- [OPTIONS]

Options:
  -r, --run-mode <MODE>        Which roles to start [default: both]
                               [possible values: both, producer, consumer]

  -e, --example-type <TYPE>    Execution mode [default: sync]
                               [possible values: sync, async-service-discovery,
                                async-receive, async-receive-with-timeout, streaming]

  -s, --service-instance-manifest <PATH>
                               Path to service configuration JSON
                               [default: ./score/mw/com/example/com-api-example/etc/mw_com_config.json]
```

### Examples

```bash
# Run with default sync mode (both producer and consumer)
bazel run //score/mw/com/example/com-api-example:com-api-example

# Run only the producer
bazel run //score/mw/com/example/com-api-example:com-api-example -- -r producer

# Run only the consumer
bazel run //score/mw/com/example/com-api-example:com-api-example -- -r consumer

# Test async receive with timeout/cancellation
bazel run //score/mw/com/example/com-api-example:com-api-example -- -e async-receive-with-timeout

# Run only consumer with async receive with timeout feature
bazel run //score/mw/com/example/com-api-example:com-api-example -- -r consumer -e async-receive-with-timeout

# Test streaming with custom config
bazel run //score/mw/com/example/com-api-example:com-api-example -- \
  -e streaming \
  -s ./path/to/custom/config.json
```

## Integration Tests

### Tokio crate based Integration Tests

A separate test target (`tests_using_tokio_runtime.rs`) provides comprehensive integration tests using the **Tokio async runtime**. These tests are independent of the main application and use the `com-api-gen` generated types together with the `com_api` runtime, service discovery, and subscription APIs directly.

**Why Separate?**
- Leverages Tokio's multi-threaded runtime to test concurrent scenarios and verify that subscription and other async APIs can be safely spawned on independent tasks or threads.
- Validates async COM API behavior under a real async runtime

### Running Tests

```bash
# Run all integration tests
bazel test //score/mw/com/example/com-api-example:com-api-example-tokio-integration-test

# Run specific test
bazel test //score/mw/com/example/com-api-example:com-api-example-tokio-integration-test \
  --test_filter=receive_and_send_using_multi_thread

# Run with verbose output for debugging
bazel test //score/mw/com/example/com-api-example:com-api-example-tokio-integration-test \
  --test_output=all --test_arg=--nocapture
```

## Further Reading
- [COM API Documentation](../../impl/rust/com-api/README.md)

---
