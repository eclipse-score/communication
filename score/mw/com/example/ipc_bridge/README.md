# IPC Bridge Example

Demonstrates inter-process communication using shared memory with both C++ and Rust implementations.

## Overview

The example sends `MapApiLanesStamped` samples from a sender/skeleton process to a receiver/proxy process. Configuration can be provided in JSON or FlatBuffer binary format.

## C++ Example

**Receiver (Terminal 1):**
```bash
bazel run //score/mw/com/example/ipc_bridge:ipc_bridge_cpp -- --mode recv -n 5 -t 200 --service_instance_manifest score/mw/com/example/ipc_bridge/etc/mw_com_config.json
```

**Sender (Terminal 2):**
```bash
bazel run //score/mw/com/example/ipc_bridge:ipc_bridge_cpp -- --mode send -n 5 -t 200 --service_instance_manifest score/mw/com/example/ipc_bridge/etc/mw_com_config.json
```

## Rust Example

**Receiver (Terminal 1):**
```bash
bazel run //score/mw/com/example/ipc_bridge:ipc_bridge_rs -- --mode recv --service-instance-manifest score/mw/com/example/ipc_bridge/etc/mw_com_config.json
```

**Sender (Terminal 2):**
```bash
bazel run //score/mw/com/example/ipc_bridge:ipc_bridge_rs -- --mode send --service-instance-manifest score/mw/com/example/ipc_bridge/etc/mw_com_config.json
```

## Using FlatBuffer Configuration

Replace `.json` with `.bin` in the service instance manifest argument e.g.:

## C++ Example

**Receiver (Terminal 1):**
```bash
bazel run //score/mw/com/example/ipc_bridge:ipc_bridge_cpp -- --mode recv -n 5 -t 200 --service_instance_manifest score/mw/com/example/ipc_bridge/etc/mw_com_config.bin
```

**Sender (Terminal 2):**
```bash
bazel run //score/mw/com/example/ipc_bridge:ipc_bridge_cpp -- --mode send -n 5 -t 200 --service_instance_manifest score/mw/com/example/ipc_bridge/etc/mw_com_config.bin
```

The configuration format is automatically detected by file extension. The `.bin` file is generated from the `.json` source during build
