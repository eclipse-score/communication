# `dual_qemu` ITF plugin

A pytest/ITF plugin that boots **two** QNX QEMU VMs which share a single QEMU
[`ivshmem`](https://www.qemu.org/docs/master/system/devices/ivshmem.html) region. It is a
thin extension of the upstream single-VM `qemu` plugin
(`@score_itf//score/itf/plugins/qemu`).

## What it adds over the upstream `qemu` plugin

| Aspect            | upstream `qemu`        | `dual_qemu`                                            |
| ----------------- | ---------------------- | ----------------------------------------------------- |
| Number of VMs     | 1 (`target_init`)      | 2 (`target_a`, `target_b`)                            |
| Shared memory     | none                   | one `ivshmem-plain` region backed by one host file    |
| SSH ports         | one                    | distinct host port per VM (e.g. `2222` / `2223`)      |
| Inter-VM NIC      | n/a                    | optional point-to-point socket NIC (off by default)   |

The shared region uses **`ivshmem-plain`** — a passive shared-memory PCI device with
**no MSI-X and no doorbell**. Cross-VM notifications travel over a separate
socket-based control plane, so the data plane only needs plain shared memory.

## Fixtures

| Fixture            | Scope   | Description                                              |
| ------------------ | ------- | ------------------------------------------------------- |
| `target_a`         | session | First VM (`QemuTarget`).                                 |
| `target_b`         | session | Second VM (`QemuTarget`).                                |
| `ivshmem_backend`  | session | Path of the shared host backing file.                   |
| `config`           | session | Loaded `DualQemuConfigModel` + `qemu_image`.            |

Both VMs run the upstream `pre_tests_phase` checks (ping / SSH / SFTP) before the tests
start.

### Boot reliability

Booting two QNX guests **concurrently** under KVM occasionally wedges the second guest
during device bring-up (it can hang at SMP AP / secondary-CPU startup under the `-kernel`
boot path, so its `sshd` never comes up and QEMU's SLIRP resets the harness connections).
To keep the test reliable the plugin:

- boots the VMs **sequentially** — it starts a VM, waits until SSH is *stably* reachable
  and runs `pre_tests_phase`, and only then starts the next one, so the two guests never
  initialise their devices at the same time;
- gives each VM a single core and a **distinct NIC MAC**;
- the `dual_qemu_integration_test` macro additionally marks the test `flaky = True` so
  bazel transparently retries the rare residual KVM boot hiccup.

## CLI options

| Option               | Required | Description                                        |
| -------------------- | -------- | -------------------------------------------------- |
| `--dual-qemu-config` | yes      | JSON config (see `dual_qemu_config.example.json`). |
| `--qemu-image`       | yes      | Shared QEMU IFS image booted by both VMs.          |

## Configuration

See [`dual_qemu_config.example.json`](dual_qemu_config.example.json). A config is just
**two ordinary QEMU configs** (each an upstream `QemuConfigModel`) plus an `ivshmem`
block:

```json
{
    "ivshmem": { "size": "4M" },
    "intervm_network": { "enabled": false, "host_port": 12345 },
    "vms": [ { "...": "QemuConfigModel for VM-A" },
             { "...": "QemuConfigModel for VM-B" } ]
}
```

- `ivshmem.size` — size of the shared region (e.g. `4M`).
- `ivshmem.mem_path` (optional) — explicit host backing file; when empty a temporary file
  is created for the session and removed on teardown.
- `intervm_network.enabled` — when `true`, adds a socket NIC where VM-A listens and VM-B
  connects on `host_port` (for the future socket control plane).
- Each VM's `ssh_port` and `port_forwarding` must use **distinct host ports**.

## Cross-VM lib-memory example

[`example_xvm/`](example_xvm/README.md) drives the `score::memory::shared` allocator (the
one LoLa uses) across both VMs over the ivshmem BAR. The one BAR is split into two
page-aligned lib-memory regions, so the exchange is **bidirectional**: the producer creates
a forward region holding a `Vector` (the consumer opens it), and the consumer creates a
separate reverse region holding a `Map` (the producer opens it) — each container's
`OffsetPtr`s resolve despite each VM mapping the BAR at a different base. See
[`example_xvm/README.md`](example_xvm/README.md) for the details.

## Debugging guest boot

Set `DUAL_QEMU_SERIAL_DIR` to capture each VM's serial console to
`<dir>/vm<index>_serial.log` (instead of the normal `mon:stdio`). For example:

```bash
bazel test //path/to/your:dual_qemu_test \
    --config=qnx_x86_64 --strategy=TestRunner=local \
    --test_env=DUAL_QEMU_SERIAL_DIR=/tmp/dqdbg
# then inspect /tmp/dqdbg/vm0_serial.log and /tmp/dqdbg/vm1_serial.log
```
