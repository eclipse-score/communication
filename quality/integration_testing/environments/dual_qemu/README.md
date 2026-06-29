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

## Files

| File                              | Purpose                                                        |
| --------------------------------- | -------------------------------------------------------------- |
| `__init__.py`                     | `pytest_addoption` + fixtures (`target_a`/`target_b`/...).      |
| `config.py`                       | `DualQemuConfigModel` (two `QemuConfigModel` + `ivshmem`).      |
| `ivshmem_qemu.py`                 | QEMU launcher that appends the `ivshmem-plain` device.         |
| `dual_qemu_process.py`            | Process wrapper around `IvshmemQemu`.                           |
| `BUILD`                           | `py_library` + `py_itf_plugin` (`:dual_qemu_plugin`).          |
| `dual_qemu_config.example.json`   | Sample two-VM configuration.                                   |
| `example/run_example_manually.sh` | Launch both VMs by hand for manual SSH / experimentation.      |

## Using it from a test

Wire `@//quality/integration_testing/environments/dual_qemu:dual_qemu_plugin` as the
plugin of a `py_itf_test`-based target (instead of the single-VM `qemu_plugin`) and pass
`--dual-qemu-config` / `--qemu-image`. The easiest way is the
`dual_qemu_integration_test` macro in
`//quality/integration_testing:integration_testing.bzl`, which builds the shared IFS image
and wires everything for you.

## Simple C++ example

[`example/`](example) contains the smallest possible cross-VM **data-plane** demo. It
shows the property that makes the LoLa data plane work across VMs: a pointer stored
*inside* the shared region must resolve on the other VM even though each VM maps the
region at a **different** virtual base address. Instead of a flat string, the example
stores a singly-linked list whose links are self-relative byte offsets — *offset pointers*,
just like `score::memory::shared::OffsetPtr`. A raw `T*` could not survive the trip; an
offset pointer can.

- [`example/qemu_shared_ivshmem.cpp`](example/qemu_shared_ivshmem.cpp) — one C++ binary. In a single
  run each VM writes an offset-pointer linked list into its own slot and then reads +
  verifies the *peer's* slot by walking the offset pointers. The region has two slots:
  `slot[0]` for VM-A (`--id A`) and `slot[1]` for VM-B (`--id B`). Because the read phase
  waits for the peer, the two VMs must run concurrently (you can still split the phases
  with `--write` / `--read` to run them by hand). On QNX it reaches the region through
  `pci-server` by mapping the ivshmem device's shared-memory BAR
  directly — no `/dev/ivshmem0` driver needed. No mw::com involved.
- [`example/qemu_shared_ivshmem_test.py`](example/qemu_shared_ivshmem_test.py) — launches the two
  single-run commands concurrently (one per VM) so they rendezvous, and asserts both VMs
  verify the other's list.
- [`example/BUILD`](example/BUILD) — builds the binary, packages it, and runs it via
  `dual_qemu_integration_test`.

### How to see both VMs' results

Run the test with streamed (or `all`) output so the per-VM `print` lines are shown:

```bash
bazel test //quality/integration_testing/environments/dual_qemu/example:test_qemu_shared_ivshmem \
    --config=qnx_x86_64 \
    --test_output=streamed
```

Each VM prints its write and read lines, for example:

```text
[VM-A] VM-A wrote 8-node offset-pointer list into slot 0 (region mapped at 0x...)
VM-A reading peer slot 1 (region mapped at 0x...): 8192 8193 ...
VM-A verified 8 nodes via offset pointers OK (rc=0)
[VM-B] VM-B wrote 8-node offset-pointer list into slot 1 (region mapped at 0x...)
VM-B reading peer slot 0 (region mapped at 0x...): 4096 4097 ...
VM-B verified 8 nodes via offset pointers OK (rc=0)
```

The two `region mapped at` addresses differ between the VMs, which is exactly why the test
is meaningful: each VM resolves the *other* VM's linked list correctly despite the
different base. The test asserts both VMs report `verified`.

> Note: on QNX the example maps the ivshmem PCI device's shared-memory BAR via `pci-server`,
> so it needs no `/dev/ivshmem0` guest driver. QNX SDP ships no ivshmem resmgr, so the BAR
> is mapped in-process via `mmap_device_memory()`.

## Running it manually

[`example/run_example_manually.sh`](example/run_example_manually.sh) boots the same two ivshmem-sharing VMs by
hand so you can SSH in and drive the example yourself.

1. Build the IFS image (contains the `qemu_shared_ivshmem` app + `sshd`):

   ```bash
   bazel build --config=qnx_x86_64 \
     //quality/integration_testing/environments/dual_qemu/example:_init_ifs_test_qemu_shared_ivshmem
   ```

2. Start both VMs (from the repo root):

   ```bash
   ./quality/integration_testing/environments/dual_qemu/example/run_example_manually.sh
   ```

3. In two other terminals, SSH in (the `root` password is empty):

   ```bash
   ssh -p 2222 root@localhost      # VM-A
   ssh -p 2223 root@localhost      # VM-B
   ```

4. Each VM writes its own offset-pointer list and reads + verifies the peer's in a single
   run. Start both roughly together (each one waits for the other):

   ```bash
   # on VM-A:
   /opt/qemu_shared_ivshmem/bin/qemu_shared_ivshmem --id A
   # on VM-B:
   /opt/qemu_shared_ivshmem/bin/qemu_shared_ivshmem --id B
   ```

5. Stop the VMs:

   ```bash
   ./quality/integration_testing/environments/dual_qemu/example/run_example_manually.sh stop
   ```

The script honours environment overrides such as `IMG`, `SHM_PATH`, `SHM_SIZE`, `RAM`,
`CORES`, `CPU`, `SSH_PORT_A`, `SSH_PORT_B` and `RUN_DIR`; serial logs are written under
`RUN_DIR` (default `/tmp/dual_qemu`).

## Debugging guest boot

Set `DUAL_QEMU_SERIAL_DIR` to capture each VM's serial console to
`<dir>/vm<index>_serial.log` (instead of the normal `mon:stdio`). For example:

```bash
bazel test //quality/integration_testing/environments/dual_qemu/example:test_qemu_shared_ivshmem \
    --config=qnx_x86_64 --strategy=TestRunner=local \
    --test_env=DUAL_QEMU_SERIAL_DIR=/tmp/dqdbg
# then inspect /tmp/dqdbg/vm0_serial.log and /tmp/dqdbg/vm1_serial.log
```

