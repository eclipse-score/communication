# LG SOME/IP

[![CI](https://github.com/LGE-OSS/lg-someip/actions/workflows/ci.yml/badge.svg)](https://github.com/LGE-OSS/lg-someip/actions/workflows/ci.yml)

LG SOME/IP is a C++ implementation of the [SOME/IP](https://some-ip.com/)
service-oriented communication protocol and its Service Discovery (SOME/IP-SD),
together with a routing daemon. It can be used directly by an application or
through a compatible service-binding layer, and it ships a vsomeip-compatible
interface for drop-in interoperability.

The protocol behavior and wire format are aligned with the [Open SOME/IP
Specification, Version 25-12](https://github.com/some-ip-com/open-someip-spec/).
The specification is an external reference and is not bundled with this
project.

## Features

- SOME/IP and SOME/IP-SD protocol stack (request/response, events, fields).
- Service Discovery with a standalone routing daemon (`someip-daemon`).
- End-to-end (E2E) payload protection (Profile 01 and a custom profile).
- OS abstraction over POSIX sockets (TCP/UDP) with Linux and QNX support.
- vsomeip-compatible public interface for existing vsomeip applications.

## Public API

LG SOME/IP provides its own `lgsomeip` API and a `vsomeip` compatibility API.
New applications should use the `lgsomeip` API, installed under
`include/lgsomeip`:

```
find_package(lgsomeip CONFIG REQUIRED)
target_link_libraries(your_application PRIVATE lgsomeip::api)
```

The API is declared in `<lgsomeip/LgsomeipApi.h>` and provides application lifecycle,
service and event management, message callbacks, message sending, and
notifications. `lgsomeip::api::Runtime::instance()` creates application
objects.

`Application::start()` returns immediately after beginning registration with
the daemon; it does not block. Register an `ApplicationStateHandler` before
calling `start()`, and call `offer_service()`/`request_service()` from inside
that handler once the application reports it is registered. Call `join()`
afterwards (or otherwise keep the process alive) to process messages for the
application's lifetime.

Existing applications can continue to use the installed `vsomeip` interface:

```
find_package(vsomeip CONFIG REQUIRED)
target_link_libraries(your_application PRIVATE vsomeip)
```

`ApplicationManager`, the `lgsomeip` message implementation, OS abstraction,
serializer, and bundled E2E headers remain internal implementation details.

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for the C++ formatting, static
analysis, and validation workflow.

## Architecture

The stack is split into two cooperating roles that share the same endpoint,
socket, and message primitives:

- A **daemon process** (`someip-daemon`) performs Service Discovery and routes SOME/IP
  traffic between local applications and remote ECUs.
- Each **application process** links the library and connects to the daemon over
  local IPC (Unix domain sockets on Linux, message passing on QNX).

See [docs/lgsomeip.md](docs/lgsomeip.md) for the detailed software design.

## Requirements

- A C++14-capable compiler
- CMake >= 3.18
- [RapidJSON](https://github.com/Tencent/rapidjson) (configuration parsing)
- [GoogleTest](https://github.com/google/googletest) (to build the unit tests)
- [OpenSSL](https://www.openssl.org/) (optional; required only for the `ENABLE_TLS` secure-connection build)
- [COVESA DLT](https://github.com/COVESA/dlt-daemon) development files and `pkg-config` (optional; required for `ENABLE_DLT` when no DLT CMake package is available)

If RapidJSON is not available, CMake fetches a pinned revision during
configuration. When `ENABLE_TLS=ON`, OpenSSL must be provided by the build
environment; use `OPENSSL_ROOT_DIR` or install the system development package.
The project does not download an obsolete OpenSSL fallback. Provide
`RAPIDJSON_INCLUDE_DIR` and `OPENSSL_ROOT_DIR`, or preinstall the dependencies,
for an offline or controlled build.

## Build

```
mkdir build
cd build
cmake .. && make
sudo make install
```

## Build Options

Boolean options use the same `-D<OPTION>=ON|OFF` form. They are declared in
the root `CMakeLists.txt` and are applied consistently to all project targets.

| Option | Default | Description |
|---|---:|---|
| `ENABLE_TLS` | `OFF` | Build TLS/DTLS support using OpenSSL. |
| `ENABLE_DLT` | `OFF` | Use COVESA DLT instead of console logging. |
| `ENABLE_SOMEIP_TP` | `OFF` | Enable SOME/IP Transport Protocol support. |
| `ENABLE_SOMEIP_PACKET_FILTERING` | `OFF` | Enable SOME/IP packet filtering. |
| `ENABLE_SOMEIP_DELIVERY_STATISTICS` | `OFF` | Enable packet delivery statistics. |
| `ENABLE_QNX_MESSAGE_PASSING` | QNX only | Use QNX message passing for local IPC. |
| `ENABLE_SOMEIP_IPC` | `OFF` | Enable extended SOME/IP IPC routing support. |
| `ENABLE_TEST_BUILD` | `OFF` | Build unit, component, and integration tests plus coverage instrumentation. Requires the standard CTest option `BUILD_TESTING=ON`. |
| `ENABLE_EXAMPLES` | `ON` | Build Linux example applications. This option is ignored on QNX. |
| `ENABLE_TOOLS` | `ON` | Build diagnostic tools. |

The numeric settings use the same root CMake cache with
`-D<SETTING>=<VALUE>`:

| Setting | Default | Description |
|---|---:|---|
| `APPLICATION_MGMT_ENTRY` | `0xF0` | SOME/IP application management entry ID. |
| `SOMEIP_UDP_MAX_PAYLOAD_SIZE` | `1400` | Maximum SOME/IP UDP payload size in bytes. |

For example:

```
cmake .. -DENABLE_TLS=ON -DENABLE_SOMEIP_TP=ON \
  -DSOMEIP_UDP_MAX_PAYLOAD_SIZE=1400
```

## Run

The commands below use the sample configuration. See
[`examples/config/README.md`](examples/config/README.md) for the configuration
format, field meanings, source-code references, and a copyable template.

Start the daemon in one terminal:

```sh
cd build
daemon/someip-daemon ../examples/config/lgsomeip_config.json
```

Then start each sample application in its own terminal, because each one runs
until it is stopped with Ctrl+C:

```sh
cd build
VSOMEIP_CONFIGURATION=../examples/config/lgsomeip_config.json \
  VSOMEIP_APPLICATION_NAME=notify third_party/vsomeip_interface/examples/notify-sample
```

```sh
cd build
VSOMEIP_CONFIGURATION=../examples/config/lgsomeip_config.json \
  VSOMEIP_APPLICATION_NAME=subscribe-1 third_party/vsomeip_interface/examples/subscribe-sample
```

## Tests

The Linux test script configures a separate `build-tests` directory by default,
enables both `ENABLE_TEST_BUILD` and CTest's `BUILD_TESTING`, builds the unit,
component, and integration targets, and runs them through CTest:

```
./tests/run_unit_tests_linux.sh
```

Pass a different build directory as the first argument when needed. Coverage
artifacts are written to `tests/test_report` only when `lcov` and `genhtml` are
available.

CTest labels can be used to run one test level from the test-enabled `build-tests` directory:

```sh
ctest --test-dir build-tests --label-regex '^unit$'
ctest --test-dir build-tests --label-regex '^component$'
ctest --test-dir build-tests --label-regex '^integration$'
```

Unit tests isolate value, message, serialization, and utility behavior.
Component tests exercise host or filesystem-backed components such as
configuration parsing and network-device discovery. Integration tests exercise
real sockets, multiplexers, the daemon, and application lifecycle behavior.

## License

LG-owned and original LG SOME/IP code is licensed under the **Apache License,
Version 2.0**. See [LICENSE](LICENSE) for the full text.

Portions of this project (the `third_party/vsomeip_interface/` and
`third_party/e2e_protection/` components) are derived from the GENIVI/COVESA
vsomeip project and remain under their original
MPL-2.0 terms, Copyright (C) 2014-2017 Bayerische Motoren Werke
Aktiengesellschaft (BMW AG).

Third-party dependencies and their licenses are listed in
[THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).
