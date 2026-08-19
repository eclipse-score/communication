<!----
*******************************************************************************
Copyright (c) 2026 Contributors to the Eclipse Foundation

See the NOTICE file(s) distributed with this work for additional
information regarding copyright ownership.

This program and the accompanying materials are made available under the
terms of the Apache License Version 2.0 which is available at
https://www.apache.org/licenses/LICENSE-2.0

SPDX-License-Identifier: Apache-2.0
*******************************************************************************
-->

# Gateway Application — Sample Runtime Configuration

This directory bundles a complete, runnable pair of configurations for `gateway_application_bin`: one set for a
**Source Gateway** process and one for a **Destination Gateway** process (see [Source vs. Destination
roles](../../README.md#architecture) in the gateway README). Together they demonstrate an inter-domain forwarding
setup for the two sample services shipped with the `mw::com` tutorial (`HelloWorldService`) and a vehicle interface
example (`VehicleInterface`).

It also holds `logging.json`, the `mw::log` configuration applied to the gateway process itself.

## Files

| File | Role |
|---|---|
| `mw_com_config_source_gateway.json` / `mw_com_config_destination_gateway.json` | Standard `mw::com` deployment config for each side — `serviceTypes` and `serviceInstances` (binding, ASIL level, sample slots, allowed consumers). Passed as the **first** `gateway_application_bin` argument (`<mw_com_config-path>`). |
| `mw_com_gateway_config_source_gateway.json` / `mw_com_gateway_config_destination_gateway.json` | Gateway-specific config — which services this side forwards (`forwarded-services`) or expects to receive (`expected-received-services`), and which transport layer to use. Passed as the **second** `gateway_application_bin` argument (`<gateway-config-path>`). |
| `mw_com_gateway_sample_transport_config_source_gateway.json` / `..._destination_gateway.json` | Config for the bundled `sample_hypervisor` transport (see [transport_layer/sample](../../transport_layer/sample)) — remote IP, local/remote ports, request timeout. Not passed on the command line; referenced by the `transport-layer.config-path` field of the matching `mw_com_gateway_config_*.json` file above. |
| `logging.json` | `mw::log` config for the gateway process (log level, console output). Auto-discovered by `mw::log` from `<binary-dir>/../etc/logging.json`, or overridden via the `MW_LOG_CONFIG_FILE` environment variable — see the `score/mw/log` module's own README for the full discovery order. |

For the general schema of each config file, see [../configuration/](../configuration) (gateway config) and
[../../transport_layer/sample/configuration/](../../transport_layer/sample/configuration) (transport config).

## How the files link together

Each `mw_com_gateway_config_*_gateway.json` references its transport config via an **absolute path**:

```json
"transport-layer": {
    "id": "sample_hypervisor",
    "config-path": "/etc/mw_com_gateway_sample_transport_config_destination_gateway.json"
}
```

`GatewayConfigParser` passes this path through unmodified to `TransportFactory::Create()` — it is **not** resolved
relative to the gateway config file's own location. To run a side locally, either:

- deploy that side's `mw_com_gateway_sample_transport_config_*_gateway.json` to the literal path referenced
  (`/etc/mw_com_gateway_sample_transport_config_*_gateway.json` on the target/container), or
- copy this file and edit `config-path` to wherever you placed the transport config.

## Running locally

Each side is a separate `gateway_application_bin` process, given its own `mw_com_config` and `gateway_config`:

```sh
# Destination side
gateway_application_bin \
    etc/mw_com_config_destination_gateway.json \
    etc/mw_com_gateway_config_destination_gateway.json

# Source side
gateway_application_bin \
    etc/mw_com_config_source_gateway.json \
    etc/mw_com_gateway_config_source_gateway.json
```

The sample transport connects the two sides over TCP (`hypervisor-socket` in the transport config); update
`remote-ip` in the `_source_gateway` / `_destination_gateway` sample transport files if the two sides are not on
`localhost`/the default addresses shown.
