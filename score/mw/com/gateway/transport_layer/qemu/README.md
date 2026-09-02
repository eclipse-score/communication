<!--
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

# QEMU Transport Layer

Inter-VM communication and shared memory exchange for LoLa gateway using **ivshmem** (Inter-VM Shared Memory BAR) for zero-copy data transfer between QEMU virtual machines. Extends the [sample transport layer](../sample/README.md) with ivshmem-based memory sharing.

## What It Provides

- **Message Communication** — Service provisioning and lifecycle events via TCP over QEMU's intervm network (reuses sample transport layer)
- **Shared Memory** — Zero-copy data exchange through ivshmem BAR mapping
- **Cross-VM Coordination** — Shared directory for consistent memory region access

## Main Components

- **QemuHypervisorTransport** — Orchestrates message and memory layers
- **IvshmemTypedMemoryProvider** — Manages named shared memory backed by ivshmem BAR

## Configuration

See [`configuration/example/mw_com_gateway_qemu_transport_config.json`](./configuration/example/mw_com_gateway_qemu_transport_config.json)

The config contains the usual `hypervisor-socket` block plus an optional `ivshmem` block.
`ivshmem.preferred-bar-num` selects which PCI BAR to map for the shared-memory region and
defaults to BAR2 for QEMU `ivshmem-plain`.

## How It Works

**Offering a service (Source VM):**
- Allocate CTRL and DATA shared memory regions via ivshmem BAR ([`IvshmemTypedMemoryProvider::AllocateNamedTypedMemory`](./ivshmem/ivshmem_typed_memory_provider.h))
- Register allocation offsets in the shared directory (visible to both VMs)
- Send service availability notification to remote gateway over TCP
- Remote VMs can now discover and map to the same physical memory regions

**Consuming a service (Destination VM):**
- Receive service notification and look up memory offsets in the shared directory
- Bind local shared memory objects to those discovered offsets in ivshmem BAR ([`IvshmemTypedMemoryProvider::AllocateNamedTypedMemoryAtOffset`](./ivshmem/ivshmem_typed_memory_provider.h))
- Both VMs now access the exact same physical memory

See [`QemuHypervisorTransport`](./qemu_hypervisor_transport.h) for the orchestration logic.

## Supported Platforms

- **QNX 7.1+** — Full ivshmem support
- **Linux** — Message layer only
