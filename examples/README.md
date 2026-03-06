# Examples

This directory contains aliases to example applications demonstrating the S-CORE Communication middleware API.

## Available Examples

- **[COM-API-Example](#com-api-example)**: Rust-based producer-consumer pattern demonstrating the Communication API with tire pressure data
- **[IPC Bridge](#ipc-bridge)**: C++/Rust application demonstrating IPC communication with skeleton/proxy pattern

## COM-API-Example

### Standard Build (Host Platform)

```bash
bazel build //examples/com-api-example:com-api-example
```

### QNX Cross-Compilation

```bash
bazel build --config=x86_64-qnx //examples/com-api-example:com-api-example
```

### Running

After building, the binary will be in `bazel-bin/score/mw/com/example/com-api-example/com-api-example`.

### Quick Start

To see the COM API in action, run the example from the repo root:

```bash
./bazel-bin/score/mw/com/example/com-api-example/com-api-example
```

The application demonstrates a producer-consumer pattern where:

- A `VehicleOfferedProducer` publishes tire pressure data
- A `VehicleConsumer` subscribes to and receives tire pressure updates
- Five samples are sent with incrementing tire pressure values (5.0, 6.0, 7.0, 8.0, 9.0)
- Each sample is read back and validated

You should see output showing tire data being sent and received, demonstrating the complete publish-subscribe workflow.

### Code Structure

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

## IPC Bridge

### Standard Build (Host Platform)

```bash
bazel build //examples:ipc_bridge_cpp
```

### QNX Cross-Compilation

```bash
bazel build --config=x86_64-qnx //examples:ipc_bridge_cpp
```

### Running

After building the binary will be in `bazel-bin/score/mw/com/example/ipc_bridge/ipc_bridge_cpp`.

#### Quick Start (Two Terminals)

To see the IPC communication in action, open two terminals in the repo root:

**Terminal 1 - Start Skeleton (Publisher):**

```bash
./bazel-bin/score/mw/com/example/ipc_bridge/ipc_bridge_cpp \
  --mode skeleton \
  --cycle-time 1000 \
  --num-cycles 10 \
  --service_instance_manifest score/mw/com/example/ipc_bridge/etc/mw_com_config.json
```

**Terminal 2 - Start Proxy (Subscriber):**

```bash
./bazel-bin/score/mw/com/example/ipc_bridge/ipc_bridge_cpp \
  --mode proxy \
  --cycle-time 500 \
  --num-cycles 20 \
  --service_instance_manifest score/mw/com/example/ipc_bridge/etc/mw_com_config.json
```

You should see the proxy discover the skeleton service, subscribe, and receive `MapApiLanesStamped` samples. The proxy validates data integrity and ordering for each received sample.

#### Command-Line Options

| Option | Description | Required |
| ------ | ----------- | -------- |
| `--mode, -m` | Operation mode: `skeleton`/`send` or `proxy`/`recv` | Yes |
| `--cycle-time, -t` | Cycle time in milliseconds for sending/polling | Yes |
| `--num-cycles, -n` | Number of cycles to execute (0 = infinite) | Yes |
| `--service_instance_manifest, -s` | Path to communication config JSON | Optional |
| `--disable-hash-check, -d` | Skip sample hash validation in proxy mode | Optional |

## Configuration

The communication behavior is configured via `score/mw/com/example/ipc_bridge/etc/mw_com_config.json`:

- Service type definitions and bindings
- Event definitions with IDs
- Instance-specific configuration (shared memory settings, subscriber limits)
- ASIL level and process ID restrictions
