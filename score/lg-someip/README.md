# LG SOME/IP

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
New applications should use the `lgsomeip` API, provided by the
`//score/lg-someip:lgsomeip` target:

```
cc_binary(
    name = "your_application",
    srcs = ["your_application.cpp"],
    deps = ["//score/lg-someip:lgsomeip"],
)
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

Existing applications can continue to use the `vsomeip` interface, provided by
the `//score/lg-someip:vsomeip-if` target.

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

See [docs/lgsomeip-software-design.md](docs/lgsomeip-software-design.md) for the detailed software design.

## Requirements

LG SOME/IP is built with [Bazel](https://bazel.build/) as part of the
`score_communication` workspace and uses the toolchains configured there. See
the repository [CONTRIBUTING.md](../../CONTRIBUTING.md) for the Bazel setup.
RapidJSON and GoogleTest are fetched by Bazel from the Bazel Central Registry.

TLS/DTLS secure connections (OpenSSL) and the COVESA DLT logging backend are
not supported by the Bazel build yet.

## Build

Run the Bazel commands from the root of the `score_communication` workspace:

```sh
bazel build //score/lg-someip/...
```

The binaries are written below `bazel-bin/score/lg-someip/`. The main targets are:

| Target | Description |
|---|---|
| `//score/lg-someip:lgsomeip` | Library with the `lgsomeip` API. |
| `//score/lg-someip:vsomeip-if` | Library with the `vsomeip` compatibility API. |
| `//score/lg-someip:someip-daemon` | Routing daemon. |
| `//score/lg-someip/tools/dump-someipservices` | Diagnostic tool. |
| `//score/lg-someip/examples/someip:all` | Examples using the `lgsomeip` API. |
| `//score/lg-someip/third_party/vsomeip_interface:all` | Examples using the `vsomeip` API. |

The library and daemon targets are visible within `//score/...`.

### QNX

QNX is not supported by the Bazel build yet. See
[docs/qnx-build-and-run-guide.md](docs/qnx-build-and-run-guide.md).

## Build Options

The following options are Bazel flags. They are all disabled by default and
apply to all lg-someip targets:

| Flag | Description |
|---|---|
| `--//score/lg-someip:enable_someip_tp` | Enable SOME/IP Transport Protocol support. |
| `--//score/lg-someip:enable_someip_packet_filtering` | Enable SOME/IP packet filtering. |
| `--//score/lg-someip:enable_someip_delivery_statistics` | Enable packet delivery statistics. |
| `--//score/lg-someip:enable_someip_ipc` | Enable extended SOME/IP IPC routing support. |

For example:

```sh
bazel build //score/lg-someip/... --//score/lg-someip:enable_someip_ipc=True
```

Known limitations: with `enable_someip_tp` the library does not compile, and
with `enable_someip_delivery_statistics` the `unittest_someip_unit` test does
not compile.

QNX message passing for local IPC is used automatically when building for QNX.
The SOME/IP application management entry ID (`APPLICATION_MGMT_ENTRY`, `0xF0`)
and the maximum SOME/IP UDP payload size (`SOMEIP_UDP_MAX_PAYLOAD_SIZE`, `1400`)
are defined in [lgsomeip.bzl](lgsomeip.bzl).

## Run

The commands below use the sample configuration and are run from the root of
the `score_communication` workspace after building. See
[`examples/config/README.md`](examples/config/README.md) for the configuration
format, field meanings, source-code references, and a copyable template.

Start the daemon in one terminal:

```sh
bazel-bin/score/lg-someip/daemon/someip-daemon score/lg-someip/examples/config/lgsomeip_config.json
```

Then start each sample application in its own terminal, because each one runs
until it is stopped with Ctrl+C:

```sh
VSOMEIP_CONFIGURATION=score/lg-someip/examples/config/lgsomeip_config.json \
  VSOMEIP_APPLICATION_NAME=notify \
  bazel-bin/score/lg-someip/third_party/vsomeip_interface/notify-sample
```

```sh
VSOMEIP_CONFIGURATION=score/lg-someip/examples/config/lgsomeip_config.json \
  VSOMEIP_APPLICATION_NAME=subscribe-1 \
  bazel-bin/score/lg-someip/third_party/vsomeip_interface/subscribe-sample
```

## Tests

Build and run all unit, component, and integration tests:

```sh
bazel test //score/lg-someip/...
```

| Level | Targets in `//score/lg-someip/tests` |
|---|---|
| Unit | `unittest_osabstraction_unit`, `unittest_someip_unit`, `unittest_serialization` |
| Component | `unittest_osabstraction_component`, `unittest_someip_component` |
| Integration | `unittest_osabstraction_integration`, `unittest_someip_integration` |

Run a single test and show its output with:

```sh
bazel test //score/lg-someip/tests:unittest_someip_unit --test_output=all
```

The test logs are written to `bazel-testlogs/score/lg-someip/tests/`. Bazel
runs each test in a sandbox with a private `/tmp` and `/dev/shm` and without
network access.

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
