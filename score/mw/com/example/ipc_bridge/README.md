# IPC_BRIDGE

## Building

### Standard Build (Host Platform)

```bash
bazel build //examples/ipc_bridge:sample_sender_receiver
```

### QNX Cross-Compilation

```bash
bazel build --config=x86_64-qnx //examples/ipc_bridge:sample_sender_receiver
```

## Running

After building the binary will be in `bazel-bin/examples/ipc_bridge/ipc_bridge_cpp`.

### Quick Start (Two Terminals)

To see the IPC communication in action, open two terminals in the repo root:

**Terminal 1 - Start Skeleton (Publisher):**

```bash
./bazel-bin/examples/ipc_bridge/ipc_bridge_cpp \
  --mode skeleton \
  --cycle-time 1000 \
  --num-cycles 10 \
  --service_instance_manifest examples/ipc_bridge/etc/mw_com_config.json
```

**Terminal 2 - Start Proxy (Subscriber):**

```bash
./bazel-bin/examples/ipc_bridge/ipc_bridge_cpp \
  --mode proxy \
  --cycle-time 500 \
  --num-cycles 20 \
  --service_instance_manifest examples/ipc_bridge/etc/mw_com_config.json
```

You should see the proxy discover the skeleton service, subscribe, and receive `MapApiLanesStamped` samples. The proxy validates data integrity and ordering for each received sample.

### Start Skeleton (Publisher)

```bash
./bazel-bin/examples/ipc_bridge/ipc_bridge_cpp \
  --mode skeleton \
  --cycle-time 1000 \
  --num-cycles 10 \
  --service_instance_manifest examples/ipc_bridge/etc/mw_com_config.json
```

### Start Proxy (Subscriber)

```bash
./bazel-bin/src/scrample \
  --mode proxy \
  --cycle-time 500 \
  --num-cycles 20 \
  --service_instance_manifest examples/ipc_bridge/etc/mw_com_config.json
```

### Command-Line Options

| Option | Description | Required |
| ------ | ----------- | -------- |
| `--mode, -m` | Operation mode: `skeleton`/`send` or `proxy`/`recv` | Yes |
| `--cycle-time, -t` | Cycle time in milliseconds for sending/polling | Yes |
| `--num-cycles, -n` | Number of cycles to execute (0 = infinite) | Yes |
| `--service_instance_manifest, -s` | Path to communication config JSON | Optional |
| `--disable-hash-check, -d` | Skip sample hash validation in proxy mode | Optional |

## Configuration

The communication behavior is configured via `examples/ipc_bridge/etc/mw_com_config.json`:

- Service type definitions and bindings
- Event definitions with IDs
- Instance-specific configuration (shared memory settings, subscriber limits)
- ASIL level and process ID restrictions

## Project Structure

``` text
examples/
├── ipc_bridge/
│   ├── main.cpp                  # Entry point and CLI argument parsing
│   ├── sample_sender_receiver.cpp # Core skeleton/proxy logic
│   ├── datatype.h                # Data type definitions
│   ├── assert_handler.cpp        # Custom assertion handling
│   └── etc/
│       ├── mw_com_config.json    # Communication configuration
│       └── logging.json          # Logging configuration
```

## Troubleshooting

### Runtime Warnings

When running the application, you may see:

```text
mw::log initialization error: Error No logging configuration files could be found.
Fallback to console logging.
```

This is expected and harmless. The application falls back to console logging when the optional logging configuration isn't found at the expected system location.

### Build Warnings

You may see deprecation warnings during compilation related to:

- `string_view` null-termination checks
- `InstanceSpecifier::Create()` API deprecations

These are intentional warnings from the S-CORE libraries and do not prevent successful builds. They are addressed in the `.bazelrc` configuration with `-Wno-error=deprecated-declarations`.

### QNX Builds

QNX cross-compilation requires:

- QNX SDP installation and license
- Proper credential setup (see `.github/workflows/build_and_test_qnx.yml` for CI example)
