# SOME/IP Configuration Guide

This directory contains configuration examples for the LG SOME/IP daemon and
applications. The configuration format is implemented by the parser in
[`src/someip/config/Configuration.cpp`](../../src/someip/config/Configuration.cpp)
and the field names are centralized in
[`src/someip/config/ConfigConstant.h`](../../src/someip/config/ConfigConstant.h).

The main examples are:

- [`lgsomeip_config.json`](lgsomeip_config.json): request/response and event sample.
- [`request_e2e.json`](request_e2e.json): E2E checker configuration.
- [`response_e2e.json`](response_e2e.json): E2E protector configuration.
- [`lgsomeip_config.template.json`](lgsomeip_config.template.json): minimal starting
  point for a new configuration.

## Quick Start

Copy the template and replace the example identifiers, addresses, ports, and
application names:

```sh
cp examples/config/lgsomeip_config.template.json examples/config/my_lgsomeip_config.json
build/daemon/someip-daemon examples/config/my_lgsomeip_config.json
```

An empty configuration path uses `LGSOMEIP_CONFIGURATION` when it is set, then
falls back to `/etc/lgsomeip_config.json`. An explicit path is recommended during
development. Native LG SOME/IP applications can use the environment override
without requiring a system-wide file:

```sh
LGSOMEIP_CONFIGURATION=examples/config/lgsomeip_config.json \
  build/examples/someip/simple-method/someip-response
```

This behavior is defined in [`Configuration.cpp`](../../src/someip/config/Configuration.cpp).

## Format Conventions

Use the following conventions to match the current parser:

- Service, instance, method, event, eventgroup, and application IDs are quoted
  hexadecimal strings such as `"0x1001"`.
- Ports, timing values, payload sizes, offsets, and lengths should be quoted
  decimal strings. Service endpoint ports (`unreliable`, `reliable.port`, and
  service or eventgroup `multicast.port`) and service `vlan_qos` must be
  strings; other numeric fields, such as Service Discovery timings,
  `update-cycle`, `threshold`, and E2E values, also accept unsigned JSON numbers.
- Boolean values may be JSON booleans or the strings `"true"` and `"false"`.
  The examples use strings for consistency with the existing configuration files.
- IPv4 addresses use dotted decimal notation. IPv6 configurations use an IPv6
  `unicast` address and an IPv6 Service Discovery `multicast` address. Setting
  `"iptype": "IPv6"` is recommended; if `iptype` is omitted, an address
  containing `:` selects IPv6.
- Unknown keys are ignored. The loader rejects invalid JSON and checks the
  types of known keys, but it does not validate against a JSON schema.

## Top-Level Configuration

| Key | Required | Meaning |
|---|---|---|
| `unicast` | Yes for an explicit file | Local unicast IPv4 or IPv6 address. |
| `iptype` | Recommended | `IPv4` or `IPv6`. If omitted, an address containing `:` selects IPv6; otherwise IPv4 is used. |
| `someiptpmaxpayloadsize` | No | SOME/IP-TP maximum payload size. The implementation default is `65536`. |
| `logging` | No | Controls the logger level and optional console/file sinks. |
| `applications` | Yes | Application name-to-client-ID mappings. |
| `services` | No at parser level | Service and event configuration. Provide it for a useful daemon/application configuration. |
| `service-discovery` | Required for a valid explicit file | SOME/IP-SD multicast address, port, protocol, and timing values. |
| `e2e` | No | E2E protection settings. |

The address and top-level parsing are implemented in
[`Configuration.cpp`](../../src/someip/config/Configuration.cpp).

## Logging

The `logging` object supports the following fields:

```json
"logging": {
  "level": "info",
  "console": "true",
  "file": {
    "enable": "false",
    "path": "/tmp/lgsomeip.log"
  }
}
```

The constructor defaults to `info` with console logging enabled and file logging
disabled. Supported levels are `off`, `fatal`, `error`, `warn` (or `warning`),
`info`, `debug`, and `verbose` (or `trace`). Level names are case-insensitive;
the legacy `kInfo`-style names are also accepted. Boolean values may be JSON
booleans or the strings `"true"` and `"false"`. The DLT backend is selected at
build time with `-DENABLE_DLT=ON`; the `dlt` JSON key is not used to select it.

## Applications

Each application needs a name and a hexadecimal client ID:

```json
"applications": [
    {
        "name": "request-1",
        "id": "0x0121"
    }
]
```

The application name is used by the runtime to look up the client ID. Names and
IDs must be unique; duplicates are rejected when the configuration is loaded.
Names may contain only letters, digits, `-`, `_`, and `.` (up to 128
characters); the daemon rejects application registrations with other names.
The parser reads this array in
[`Configuration.cpp`](../../src/someip/config/Configuration.cpp).

## Services

Each service entry is identified by `service` and `instance`:

```json
{
    "service": "0x1001",
    "instance": "0x0001",
    "name": "example-service",
    "major_version": "0x01",
    "minor_version": "0x00000001",
    "is-provider": "true",
    "unreliable": "30509",
    "reliable": {
        "port": "30509",
        "enable-magic-cookies": "false"
    },
    "events": [],
    "eventgroups": []
}
```

The service fields are parsed by [`ServiceInfo.cpp`](../../src/someip/config/ServiceInfo.cpp):

| Key | Meaning |
|---|---|
| `service` | Required hexadecimal service ID. |
| `instance` | Required hexadecimal service-instance ID. |
| `name` | Optional service name. The current default is `no-name`. |
| `major_version` | Optional hexadecimal major interface version. |
| `minor_version` | Optional hexadecimal minor version. |
| `minimum_minor_version` | Optional hexadecimal minimum compatible minor version. |
| `is-provider` | Whether the local application provides the service. |
| `unreliable` | UDP port for unreliable SOME/IP traffic. |
| `reliable.port` | TCP port for reliable SOME/IP traffic. |
| `reliable.enable-magic-cookies` | Enables SOME/IP TCP magic cookies for this service. |
| `vlan_qos` | Optional VLAN/QoS priority. Use a quoted decimal value. |
| `someiptp` | Optional array of hexadecimal method/event IDs transported with SOME/IP-TP. |
| `secure-connection` | Enables secure transport behavior for the service when TLS support is built. |
| `multicast` | Service-level multicast address and port. |
| `events` | Event definitions offered by the service. |
| `eventgroups` | Eventgroup definitions and their event membership. |

Provide the reliable endpoint, unreliable endpoint, or both according to the
transport behavior required by the service.

### Events

```json
"events": [
    {
        "event": "0x8777",
        "is_field": "false",
        "update-cycle": "2000"
    }
]
```

- `event` is the hexadecimal event ID.
- `is_field` marks a field notifier when true.
- `update-cycle` is the cyclic update interval in milliseconds. Zero means no
  configured cycle in the current model.

Reliability is derived from the service's `reliable` and `unreliable` endpoints;
`is_reliable` is not a supported configuration key and should not be added to
event objects.

### Eventgroups

```json
"eventgroups": [
    {
        "eventgroup": "0x4455",
        "events": ["0x8777", "0x8778"],
        "is_multicast": "false",
        "threshold": "0"
    }
]
```

- `eventgroup` is the hexadecimal eventgroup ID.
- `events` lists hexadecimal event IDs belonging to the group.
- `multicast` may define an eventgroup-specific multicast `address` and `port`.
- `is_multicast` explicitly selects multicast behavior.
- `threshold` controls adaptive multicast behavior. A threshold of zero forces
  unicast in the current implementation.

The event and eventgroup model is implemented by
[`ConfigEvent.h`](../../src/someip/config/ConfigEvent.h),
[`ConfigEventgroup.h`](../../src/someip/config/ConfigEventgroup.h), and
[`ServiceInfo.cpp`](../../src/someip/config/ServiceInfo.cpp).

## SOME/IP Service Discovery

A valid explicit configuration should include:

```json
"service-discovery": {
    "enable": "true",
    "multicast": "224.244.224.245",
    "port": "30490",
    "protocol": "udp",
    "initial_delay_min": "10",
    "initial_delay_max": "100",
    "repetitions_base_delay": "200",
    "repetitions_max": "3",
    "ttl": "3",
    "cyclic_offer_delay": "2000",
    "request_response_delay": "1500"
}
```

The fields are parsed by [`ConfigurationSD.cpp`](../../src/someip/config/ConfigurationSD.cpp):

| Key | Meaning |
|---|---|
| `enable` | Must be `true`. Service Discovery cannot currently be disabled: with `false`, the remaining SD fields are not loaded and the configuration is rejected. |
| `multicast` | SOME/IP-SD multicast address. |
| `port` | SOME/IP-SD UDP/TCP port. |
| `protocol` | Transport protocol, normally `udp`. |
| `vlan_qos` | Optional VLAN/QoS priority. |
| `initial_delay_min` / `initial_delay_max` | Initial wait range in milliseconds. |
| `repetitions_base_delay` | Base delay for the repetition phase in milliseconds. |
| `repetitions_max` | Maximum repetition count. |
| `ttl` | Service-discovery entry lifetime in seconds. |
| `cyclic_offer_delay` | Main-phase offer interval in milliseconds. |
| `request_response_delay` | Delay for multicast-triggered responses in milliseconds. |

Quoted decimal strings are used in the examples for consistency. The current
parser also accepts unsigned JSON numbers for these Service Discovery fields.

For an explicit configuration, keep a valid multicast address even while
experimenting with Service Discovery settings: post-parse address validation in
[`Configuration.cpp`](../../src/someip/config/Configuration.cpp) validates the
configured address and SD multicast address.

## E2E Protection

E2E settings are kept in a separate top-level object:

```json
"e2e": {
    "e2e_enabled": "true",
    "protected": [
        {
            "data_id": "28",
            "service_id": "0x1001",
            "event_id": "0x0001",
            "variant": "protector",
            "profile": "CRC8",
            "crc_offset": "0",
            "counter_offset": "8",
            "data_id_mode": "0",
            "data_id_nibble_offset": "0",
            "data_length": "120"
        }
    ]
}
```

The two checked-in examples show the usual roles:

- [`request_e2e.json`](request_e2e.json) uses `variant: "checker"`.
- [`response_e2e.json`](response_e2e.json) uses `variant: "protector"`.

The current runtime accepts `protector`, `checker`, or `both` variants and the
`CRC8` or `CRC32` profiles. The fields are parsed by
[`ConfigE2E.cpp`](../../src/someip/config/ConfigE2E.cpp) and applied by
[`PacketRouterHost.cpp`](../../src/someip/packetrouter/PacketRouterHost.cpp).

`data_id`, offsets, and `data_length` are decimal values in the current parser;
`service_id` and `event_id` are hexadecimal IDs.

## Feature-Specific Configuration

- `someiptp` is meaningful when the project is built with
  `ENABLE_SOMEIP_TP=ON`.
- `secure-connection` requires a TLS-enabled build and the corresponding secure
  endpoint support.
- When `ENABLE_SOMEIP_PACKET_FILTERING=ON`, an additional file is read from
  `/etc/someip_packet_filter_config.json`. Its `services` array contains a
  `service` ID and an `events` array with `id` and `protected_interval_ms`.

## Source of Truth

When the guide and a sample diverge, use these implementation files as the
source of truth:

- [`ConfigConstant.h`](../../src/someip/config/ConfigConstant.h): key names.
- [`Configuration.cpp`](../../src/someip/config/Configuration.cpp): root parsing,
  defaults, and validation.
- [`ServiceInfo.cpp`](../../src/someip/config/ServiceInfo.cpp): service, endpoint,
  event, and eventgroup parsing.
- [`ConfigurationSD.cpp`](../../src/someip/config/ConfigurationSD.cpp): Service
  Discovery parsing and defaults.
- [`ConfigE2E.cpp`](../../src/someip/config/ConfigE2E.cpp): E2E parsing and required
  fields.
- [`tests/component/someip/config/ConfigurationTest.cpp`](../../tests/component/someip/config/ConfigurationTest.cpp):
  configuration behavior covered by tests.

After changing a configuration, validate the JSON syntax and start the daemon
with the explicit file path. Configuration errors are reported during runtime
initialization rather than by a generated JSON schema.
