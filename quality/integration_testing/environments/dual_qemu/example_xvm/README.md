# Cross-VM lib-memory example

This is the payload demo for the [`dual_qemu`](../README.md) plugin: it drives the
`score::memory::shared` allocator (the one LoLa uses) across the two VMs over the ivshmem
BAR. The single physical BAR is split into **two page-aligned lib-memory regions** plus a
handshake page — each VM *creates* its own region and *opens* the peer's, even though each VM
maps the BAR at a **different** virtual base address. The data in each region is a LoLa-style
storage root: a single object constructed *with* the memory resource, holding a shared-memory
container whose elements are allocated through the resource's `MemoryResourceProxy`. Its
internal `OffsetPtr`s resolve on the peer VM despite the different base — which is exactly the
property that makes the LoLa data plane work across VMs.

The exchange is **bidirectional**, using two different LoLa container types in two separate
regions. Each region is *initialised* by its creator; a subsequent **cross-write phase** then
has each VM write into the peer's region to prove full bidirectional write access over the BAR:

| Direction              | Region / shm name    | Container                                        | Writer(s)                  |
| ---------------------- | -------------------- | ------------------------------------------------ | -------------------------- |
| producer → consumer    | region A / `/fwd`    | `score::memory::shared::Vector<uint32_t>`        | producer (init), consumer (cross-write) |
| consumer → producer    | region B / `/rev`    | `score::memory::shared::Map<uint32_t,uint32_t>`  | consumer (init), producer (cross-write) |

```
BAR base ─────────────────────────────────────────────► BAR end
| region A (forward)      | region B (reverse)      | handshake |
| paddr+0,   size=SA      | paddr+SA,  size=SB      | last page |
  producer Create()s        consumer Create()s
  consumer Open()s + writes  producer Open()s + writes
```

The single binary `lib_memory_ivshmem_xvm` takes a `--role`:

- `--role producer` (VM-A) — `Create("/fwd")` at the BAR base and `push_back` two values into
  its shared `Vector`; then (after the consumer signals its region is ready) bind + `Open("/rev")`
  and verify the `Map` the consumer created; finally writes an extra entry into the consumer's
  region B (`emplace(3, 400)`) to demonstrate cross-VM write access.
- `--role consumer` (VM-B) — binds + `Open("/fwd")`, verifies the magic, the run `nonce`, and
  the vector sum (`42 + 43 == 85`); then `Create("/rev")` in the second sub-range and `emplace`
  two entries into its own `Map`; finally writes an extra value into the producer's region A
  (`push_back(100)`) to demonstrate cross-VM write access.

Both print `verified` on success.

> Two cross-VM hurdles the example has to clear:
> 1. **Separate shm namespaces** — the same name is a *different* object in each guest, so the
>    reader must first bind its name to the region's BAR physical sub-range (`shm_ctl(SHMCTL_PHYS)`
>    via a custom `TypedMemory` provider) *before* calling `Open()`.
> 2. **Init ordering** — ivshmem-plain has no doorbell, so the two roles rendezvous by polling
>    a small handshake header placed in the **last page** of the BAR, outside both lib-memory
>    arenas. Start both roles roughly together; each waits for the other.

## Files

| File                                | Purpose                                                              |
| ----------------------------------- | -------------------------------------------------------------------- |
| `lib_memory_ivshmem_xvm.cpp`        | The producer/consumer binary (handshake + Create/Open + verify).     |
| `bar_discovery.{h,cpp}`             | Finds the ivshmem PCI device and its shared-memory BAR.              |
| `bar_typed_memory.{h,cpp}`          | `TypedMemory` provider binding a shm name to the BAR phys address.  |
| `lib_memory_ivshmem_xvm_test.py`    | Runs both roles concurrently and asserts both report `verified`.     |
| `BUILD`                             | Builds + packages the binary and wires `dual_qemu_integration_test`. |

## Running it as a test

Run with streamed (or `all`) output so both VMs' lines are shown:

```bash
bazel test //quality/integration_testing/environments/dual_qemu/example_xvm:test_lib_memory_ivshmem_xvm \
    --config=qnx_x86_64 \
    --test_output=streamed
```
