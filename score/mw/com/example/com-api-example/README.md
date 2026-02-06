# COM-API-EXAMPLE

## Building

### Standard Build (Host Platform)

```bash
bazel build //examples/com-api-example:com-api-example
```

### QNX Cross-Compilation

```bash
bazel build --config=x86_64-qnx //examples/com-api-example:com-api-example
```

## Running

After building, the binary will be in `bazel-bin/examples/com-api-example/com-api-example`.

### Quick Start

To see the COM API in action, run the example from the repo root:

```bash
./bazel-bin/examples/com-api-example/com-api-example
```

The application demonstrates a producer-consumer pattern where:

- A `VehicleOfferedProducer` publishes tire pressure data
- A `VehicleConsumer` subscribes to and receives tire pressure updates
- Five samples are sent with incrementing tire pressure values (5.0, 6.0, 7.0, 8.0, 9.0)
- Each sample is read back and validated

You should see output showing tire data being sent and received, demonstrating the complete publish-subscribe workflow.

### Running Tests

```bash
bazel test //examples/com-api-example:com-api-example-test
```

The test suite includes:

- **Integration test**: Basic producer-consumer workflow with synchronous operations
- **Async test**: Multi-threaded async producer-consumer using Tokio runtime

## Configuration

The communication behavior is configured via `examples/com-api-example/etc/config.json`:

- Service interface definitions (`VehicleInterface`)
- Event definitions for data types (`Tire`)
- Instance-specific configuration
- Shared memory settings and transport configuration

## Project Structure

```text
examples/
├── com-api-example/
│   ├── basic-consumer-producer.rs    # Main example and integration tests
│   ├── BUILD                          # Bazel build configuration
│   ├── etc/
│   │   └── config.json                # Communication configuration
│   └── com-api-gen/
│       ├── com_api_gen.rs             # Generated Rust bindings
│       ├── vehicle_gen.cpp            # Generated C++ implementation
│       └── vehicle_gen.h              # Generated C++ headers
```

## Code Structure

The example demonstrates several key patterns:

### VehicleMonitor Struct

A composite structure that combines:

- `VehicleConsumer`: Subscribes to vehicle data
- `VehicleOfferedProducer`: Publishes vehicle data
- `Subscription`: Active subscription to tire data

### Producer-Consumer Pattern

```rust
// Create producer
let producer = create_producer(runtime, service_id.clone());

// Create consumer
let consumer = create_consumer(runtime, service_id);

// Create monitor combining both
let monitor = VehicleMonitor::new(consumer, producer).unwrap();

// Write data
monitor.write_tire_data(Tire { pressure: 5.0 }).unwrap();

// Read data
let tire_data = monitor.read_tire_data().unwrap();
```

### Async Multi-Threading (Test)

The async test demonstrates:

- Concurrent producer and consumer tasks using Tokio
- Asynchronous data sending at 2ms intervals
- Asynchronous data processing with buffering
- Proper lifecycle management (offer/unoffer)

## Troubleshooting

### Build Warnings

You may see deprecation warnings during compilation related to the COM API. These are intentional warnings from the S-CORE libraries and do not prevent successful builds.

### QNX Builds

QNX cross-compilation requires:

- QNX SDP installation and license
- Proper credential setup (see `.github/workflows/build_and_test_qnx.yml` for CI example)

### Configuration Path Issues

The example expects the configuration file at:

```text
score/mw/com/example/com-api-example/etc/config.json
```

Ensure this path is accessible from your workspace root when running the binary.
