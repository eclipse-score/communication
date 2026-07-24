/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

// CROSS-VM demo: Lib memory (score::memory::shared) over the ivshmem BAR,
// with TWO separate shared-memory regions carved out of the one BAR -- each VM CREATES its own
// region and OPENS the peer's -- so the exchange is fully bidirectional.
//
//   lib_memory_ivshmem_xvm --role producer   // VM-A: Create() the forward region, read reverse
//   lib_memory_ivshmem_xvm --role consumer   // VM-B: Open() the forward region, Create() reverse
//
// BAR layout (the single physical BAR split into page-aligned sub-ranges):
//
//   | forward region (A)      | reverse region (B)      | handshake |
//   | paddr+0,   size=SA      | paddr+SA,  size=SB      | last page |
//     producer Create()s        consumer Create()s
//     writes Vector<uint32_t>    writes Map<uint32_t,uint32_t>
//     consumer Open()s + writes  producer Open()s + writes
//
// Each region is INITIALISED by its creator; the cross-write phase additionally lets each VM
// write into the peer's region to prove full bidirectional write access over the BAR.
//
// Why a handshake is still needed (the two hard cross-VM problems):
//   1. Separate shm namespaces. Each QNX guest has its OWN shm object namespace: a name on
//      VM-A and on VM-B are DIFFERENT objects. What makes them the same memory is that BOTH
//      are bound to the SAME BAR physical sub-range via shm_ctl(SHMCTL_PHYS). So the reader
//      must FIRST bind its own name to that sub-range (via the provider) and only THEN call
//      SharedMemoryFactory::Open() -- Open() itself does a plain shm_open() and never consults
//      the typed-memory provider.
//   2. Init ordering. A reader must not Open()/parse a region until its creator has finished
//      Create(). ivshmem-plain has no doorbell, so we rendezvous by polling a small handshake
//      header placed in the LAST page of the BAR (outside both lib-memory arenas).
//
// QNX-only (uses pci-server + mmap_device_memory + shm_ctl(SHMCTL_PHYS)).

#include "quality/integration_testing/environments/dual_qemu/example_xvm/bar_discovery.h"
#include "quality/integration_testing/environments/dual_qemu/example_xvm/bar_typed_memory.h"

#include "score/memory/shared/i_shared_memory_resource.h"
#include "score/memory/shared/managed_memory_resource.h"
#include "score/memory/shared/map.h"
#include "score/memory/shared/shared_memory_factory.h"
#include "score/memory/shared/vector.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>
#include <tuple>

#if defined(__QNXNTO__)
#include <sys/mman.h>  // mmap_device_memory / munmap_device_memory
#include <unistd.h>    // getpid
#endif

namespace
{
using score::memory::shared::ISharedMemoryResource;
using score::memory::shared::ManagedMemoryResource;
using score::memory::shared::SharedMemoryFactory;

// Process exit codes (0 == OK).
constexpr int kExitDiscoverFailed = 1;    // no ivshmem BAR
constexpr int kExitBadArgs = 2;           // missing/invalid --role
constexpr int kExitMapFailed = 3;         // could not raw-map the BAR for the handshake
constexpr int kExitCreateFailed = 4;      // producer: Create returned nullptr
constexpr int kExitNotTypedMemory = 5;    // producer: shm_ctl(SHMCTL_PHYS) rejected
constexpr int kExitOpenFailed = 6;        // consumer: bind or Open failed
constexpr int kExitVerifyFailed = 7;      // consumer: content mismatch
constexpr int kExitHandshakeTimeout = 8;  // peer did not rendezvous in time

constexpr std::uint32_t kMagic = 0x00C0FFEEU;
constexpr std::uint32_t kReadyMagic = 0x0AD10ADEU;  // a region has been created + filled

constexpr char kFwdName[] = "/lola_ivshmem_fwd";  // region A: producer -> consumer (Vector)
constexpr char kRevName[] = "/lola_ivshmem_rev";  // region B: consumer -> producer (Map)
constexpr std::size_t kReserve = 4096U;           // lib-memory user bytes per region
constexpr std::uint64_t kPageSize = 4096U;        // shm_ctl(SHMCTL_PHYS) needs page alignment
constexpr std::size_t kHandshakeReserve = 4096U;  // last page of BAR reserved for the handshake
constexpr int kPollTimeoutMs = 30000;             // rendezvous timeout
constexpr int kPollStepMs = 10;

// LoLa-style shared layout: a single "storage" root object constructed WITH the memory
// resource, holding a shared-memory container whose elements are allocated through the
// resource's MemoryResourceProxy -- exactly how lola::ServiceDataStorage owns its Map/Vector
// members. The container's internal OffsetPtrs resolve on the peer VM despite the different
// mapping base. `nonce` links this Create to the handshake so the reader can reject stale
// BAR contents from a previous run.
using ValueVector = score::memory::shared::Vector<std::uint32_t>;
using ResponseMap = score::memory::shared::Map<std::uint32_t, std::uint32_t>;

struct ForwardRoot
{
    explicit ForwardRoot(ManagedMemoryResource& resource) noexcept : magic{0U}, nonce{0U}, values{resource} {}

    std::uint32_t magic;
    std::uint32_t nonce;
    ValueVector values;  // producer -> consumer
};

struct ReverseRoot
{
    explicit ReverseRoot(ManagedMemoryResource& resource) noexcept : magic{0U}, nonce{0U}, responses{resource} {}

    std::uint32_t magic;
    std::uint32_t nonce;
    ResponseMap responses;  // consumer -> producer
};

// Page-aligned split of the one BAR into two lib-memory regions plus the handshake page.
struct BarLayout
{
    std::uint64_t fwd_paddr;
    std::uint64_t fwd_size;
    std::uint64_t rev_paddr;
    std::uint64_t rev_size;
};

BarLayout ComputeLayout(std::uint64_t paddr, std::uint64_t size) noexcept
{
    // Reserve the last page for the handshake, then split the rest in two page-aligned halves.
    const std::uint64_t usable = size - kHandshakeReserve;
    const std::uint64_t fwd_size = (usable / 2U) & ~(kPageSize - 1U);
    BarLayout layout{};
    layout.fwd_paddr = paddr;
    layout.fwd_size = fwd_size;
    layout.rev_paddr = paddr + fwd_size;  // page-aligned: paddr and fwd_size are both page-aligned
    layout.rev_size = usable - fwd_size;
    return layout;
}

// Handshake header placed at the END of the BAR (raw-mapped), never touched by lib memory.
// volatile because it is written by one VM and polled by the other with no cache-coherency
// help beyond ivshmem-plain's shared host RAM.
struct Handshake
{
    volatile std::uint32_t fwd_ready;           // producer -> consumer: region A created + filled
    volatile std::uint32_t rev_ready;           // consumer -> producer: region B created + filled
    volatile std::uint32_t nonce;               // producer's run nonce, echoed into both roots
    volatile std::uint32_t cross_write_done_a;  // consumer wrote into region A (producer's region)
    volatile std::uint32_t cross_write_done_b;  // producer wrote into region B (consumer's region)
};

void SleepMs(int ms) noexcept
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = static_cast<long>(ms % 1000) * 1000000L;
    std::ignore = nanosleep(&ts, nullptr);
}

// Raw-maps the BAR physical memory so we can reach the handshake header. Discovery has
// already requested I/O privilege and enabled the device's memory decoding.
void* MapBarRaw(std::uint64_t paddr, std::uint64_t size) noexcept
{
#if defined(__QNXNTO__)
    void* const addr = ::mmap_device_memory(nullptr, size, PROT_READ | PROT_WRITE, 0, static_cast<off_t>(paddr));
    return (addr == MAP_FAILED) ? nullptr : addr;
#else
    std::ignore = paddr;
    std::ignore = size;
    return nullptr;
#endif
}

Handshake* HandshakeAt(void* raw, std::uint64_t size) noexcept
{
    // Last page of the BAR; well past the lib-memory arena (control block + kReserve).
    return reinterpret_cast<Handshake*>(static_cast<char*>(raw) + (size - kHandshakeReserve));
}

// Polls *word until it equals `expected` or the timeout elapses. Returns true on match.
bool WaitForWord(volatile std::uint32_t* word, std::uint32_t expected, int timeout_ms) noexcept
{
    for (int waited = 0; waited <= timeout_ms; waited += kPollStepMs)
    {
        if (*word == expected)
        {
            return true;
        }
        SleepMs(kPollStepMs);
    }
    return false;
}

int RunProducer(const BarLayout& layout, Handshake* hs) noexcept
{
    // Reset the handshake for this run before anyone can observe a READY.
    hs->rev_ready = 0U;
    hs->nonce = 0U;
    hs->fwd_ready = 0U;
    hs->cross_write_done_a = 0U;
    hs->cross_write_done_b = 0U;
    std::atomic_thread_fence(std::memory_order_release);

    std::uint32_t nonce = 0U;
#if defined(__QNXNTO__)
    nonce = static_cast<std::uint32_t>(getpid());
#endif
    nonce = (nonce << 8) ^ static_cast<std::uint32_t>(std::time(nullptr));
    if (nonce == 0U)
    {
        nonce = 1U;
    }

    // Forward region (A): Create() our own region at the BAR base and fill its Vector. This VM
    // is the INITIALISING writer of region A (the consumer also writes into it in the cross-write phase).
    SharedMemoryFactory::SetTypedMemoryProvider(
        std::make_shared<qemu_shared_ivshmem::BarTypedMemory>(layout.fwd_paddr, layout.fwd_size));

    auto fwd = SharedMemoryFactory::Create(
        kFwdName,
        [nonce](std::shared_ptr<ISharedMemoryResource> res) {
            ForwardRoot* const root = res->construct<ForwardRoot>(*res);
            root->values.reserve(2U);
            root->values.push_back(42U);
            root->values.push_back(43U);
            root->magic = kMagic;
            root->nonce = nonce;
        },
        kReserve,
        SharedMemoryFactory::WorldWritable{},
        /*prefer_typed_memory=*/true);

    if (fwd == nullptr)
    {
        std::fprintf(stderr, "producer: Create(/fwd) failed\n");
        return kExitCreateFailed;
    }
    if (!fwd->IsShmInTypedMemory())
    {
        std::fprintf(stderr, "producer: /fwd NOT in typed memory (shm_ctl(SHMCTL_PHYS) rejected)\n");
        return kExitNotTypedMemory;
    }

    // Publish: make the forward data visible before announcing it is ready.
    std::atomic_thread_fence(std::memory_order_release);
    hs->nonce = nonce;
    hs->fwd_ready = kReadyMagic;
    std::fprintf(
        stderr, "producer: published forward Vector region (nonce=0x%08x), waiting for reverse region\n", nonce);

    // Wait for the consumer to Create() + fill region B.
    if (!WaitForWord(&hs->rev_ready, kReadyMagic, kPollTimeoutMs))
    {
        std::fprintf(stderr, "producer: timed out waiting for consumer reverse region\n");
        return kExitHandshakeTimeout;
    }
    std::atomic_thread_fence(std::memory_order_acquire);

    // Reverse region (B): bind our own name to region B's sub-range, then Open() and read the
    // Map the consumer created. Its OffsetPtr node links resolve here despite the different base.
    auto rev_provider = std::make_shared<qemu_shared_ivshmem::BarTypedMemory>(layout.rev_paddr, layout.rev_size);
    const auto bind = rev_provider->AllocateNamedTypedMemory(
        static_cast<std::size_t>(layout.rev_size), kRevName, SharedMemoryFactory::WorldWritable{});
    if (!bind.has_value())
    {
        std::fprintf(stderr, "producer: failed to bind /rev to region B\n");
        return kExitOpenFailed;
    }

    auto rev = SharedMemoryFactory::Open(kRevName, /*is_read_write=*/true);
    if (rev == nullptr)
    {
        std::fprintf(stderr, "producer: Open(/rev) failed\n");
        return kExitOpenFailed;
    }

    const ReverseRoot* const root = static_cast<const ReverseRoot*>(rev->getUsableBaseAddress());
    std::uint32_t response_sum = 0U;
    std::size_t response_count = 0U;
    for (const auto& entry : root->responses)
    {
        response_sum += entry.second;
        ++response_count;
    }
    if (root->magic != kMagic || root->nonce != nonce || response_count != 2U || response_sum != 300U)
    {
        std::fprintf(stderr,
                     "producer: reverse Map verify FAILED (magic=0x%08x nonce=0x%08x/exp0x%08x count=%zu sum=%u)\n",
                     root->magic,
                     root->nonce,
                     nonce,
                     response_count,
                     response_sum);
        return kExitVerifyFailed;
    }

    std::fprintf(stderr, "producer: resolved consumer reverse Map (count=%zu sum=%u)\n", response_count, response_sum);

    // ---- Cross-write phase: producer writes INTO region B (consumer's region) ----
    // Open region B read-write (already opened above), then emplace a new entry into the
    // consumer's Map. This proves BOTH VMs can write to the SAME shared memory location.
    ReverseRoot* const rev_root_rw = static_cast<ReverseRoot*>(rev->getUsableBaseAddress());
    rev_root_rw->responses.emplace(3U, 400U);
    std::atomic_thread_fence(std::memory_order_release);
    hs->cross_write_done_b = kReadyMagic;
    std::fprintf(stderr, "producer: wrote entry {3,400} into consumer's region B\n");

    // Wait for consumer to write into region A (our region).
    if (!WaitForWord(&hs->cross_write_done_a, kReadyMagic, kPollTimeoutMs))
    {
        std::fprintf(stderr, "producer: timed out waiting for consumer cross-write into region A\n");
        return kExitHandshakeTimeout;
    }
    std::atomic_thread_fence(std::memory_order_acquire);

    // Verify consumer's cross-write into region A (our Vector).
    const ForwardRoot* const fwd_root = static_cast<const ForwardRoot*>(fwd->getUsableBaseAddress());
    if (fwd_root->values.size() != 3U)
    {
        std::fprintf(stderr,
                     "producer: cross-write verify FAILED (expected 3 values, got %zu)\n",
                     static_cast<std::size_t>(fwd_root->values.size()));
        return kExitVerifyFailed;
    }
    const std::uint32_t cross_sum = fwd_root->values[0] + fwd_root->values[1] + fwd_root->values[2];
    if (cross_sum != 185U)  // 42 + 43 + 100
    {
        std::fprintf(stderr, "producer: cross-write verify FAILED (expected sum=185, got %u)\n", cross_sum);
        return kExitVerifyFailed;
    }
    std::fprintf(stderr,
                 "producer: verified consumer cross-wrote into region A (values=[%u,%u,%u] sum=%u)\n",
                 fwd_root->values[0],
                 fwd_root->values[1],
                 fwd_root->values[2],
                 cross_sum);
    std::printf("verified\n");
    return 0;
}

int RunConsumer(const BarLayout& layout, Handshake* hs) noexcept
{
    // Wait for the producer to Create() + fill region A.
    if (!WaitForWord(&hs->fwd_ready, kReadyMagic, kPollTimeoutMs))
    {
        std::fprintf(stderr, "consumer: timed out waiting for producer forward region\n");
        return kExitHandshakeTimeout;
    }
    std::atomic_thread_fence(std::memory_order_acquire);
    const std::uint32_t expected_nonce = hs->nonce;

    // Forward region (A): bind our own name to region A's sub-range, then Open() and verify the
    // producer's Vector. Open() does a plain shm_open(), so this VM must bind the name first.
    auto fwd_provider = std::make_shared<qemu_shared_ivshmem::BarTypedMemory>(layout.fwd_paddr, layout.fwd_size);
    const auto bind_fwd = fwd_provider->AllocateNamedTypedMemory(
        static_cast<std::size_t>(layout.fwd_size), kFwdName, SharedMemoryFactory::WorldWritable{});
    if (!bind_fwd.has_value())
    {
        std::fprintf(stderr, "consumer: failed to bind /fwd to region A\n");
        return kExitOpenFailed;
    }

    auto fwd = SharedMemoryFactory::Open(kFwdName, /*is_read_write=*/true);
    if (fwd == nullptr)
    {
        std::fprintf(stderr, "consumer: Open(/fwd) failed\n");
        return kExitOpenFailed;
    }

    ForwardRoot* const froot = static_cast<ForwardRoot*>(fwd->getUsableBaseAddress());
    std::uint32_t sum = 0U;
    for (const std::uint32_t value : froot->values)
    {
        sum += value;
    }
    if (froot->magic != kMagic || froot->nonce != expected_nonce || sum != 85U)
    {
        std::fprintf(stderr,
                     "consumer: forward verify FAILED (magic=0x%08x nonce=0x%08x/exp0x%08x sum=%u)\n",
                     froot->magic,
                     froot->nonce,
                     expected_nonce,
                     sum);
        return kExitVerifyFailed;
    }

    // Reverse region (B): Create() our OWN region and fill its Map. This VM is the INITIALISING
    // writer of region B; the producer also writes into it in the cross-write phase. Each emplace
    // allocates a tree node from region B's own arena.
    SharedMemoryFactory::SetTypedMemoryProvider(
        std::make_shared<qemu_shared_ivshmem::BarTypedMemory>(layout.rev_paddr, layout.rev_size));

    auto rev = SharedMemoryFactory::Create(
        kRevName,
        [expected_nonce](std::shared_ptr<ISharedMemoryResource> res) {
            ReverseRoot* const root = res->construct<ReverseRoot>(*res);
            root->responses.emplace(1U, 100U);
            root->responses.emplace(2U, 200U);
            root->magic = kMagic;
            root->nonce = expected_nonce;
        },
        kReserve,
        SharedMemoryFactory::WorldWritable{},
        /*prefer_typed_memory=*/true);

    if (rev == nullptr)
    {
        std::fprintf(stderr, "consumer: Create(/rev) failed\n");
        return kExitCreateFailed;
    }
    if (!rev->IsShmInTypedMemory())
    {
        std::fprintf(stderr, "consumer: /rev NOT in typed memory (shm_ctl(SHMCTL_PHYS) rejected)\n");
        return kExitNotTypedMemory;
    }

    // Publish: make the reverse Map visible before announcing it is ready.
    std::atomic_thread_fence(std::memory_order_release);
    hs->rev_ready = kReadyMagic;
    std::fprintf(stderr, "consumer: verified forward Vector (sum=%u), published reverse Map region\n", sum);

    // ---- Cross-write phase: consumer writes INTO region A (producer's region) ----
    // The producer's Vector lives in region A. We (consumer) push_back a new value into it.
    // This proves BOTH VMs can write to the SAME shared memory location.
    ForwardRoot* const fwd_root_rw = static_cast<ForwardRoot*>(fwd->getUsableBaseAddress());
    fwd_root_rw->values.push_back(100U);
    std::atomic_thread_fence(std::memory_order_release);
    hs->cross_write_done_a = kReadyMagic;
    std::fprintf(stderr, "consumer: wrote value 100 into producer's region A Vector\n");

    // Wait for producer to write into region B (our region).
    if (!WaitForWord(&hs->cross_write_done_b, kReadyMagic, kPollTimeoutMs))
    {
        std::fprintf(stderr, "consumer: timed out waiting for producer cross-write into region B\n");
        return kExitHandshakeTimeout;
    }
    std::atomic_thread_fence(std::memory_order_acquire);

    // Verify producer's cross-write into region B (our Map).
    const ReverseRoot* const rev_root = static_cast<const ReverseRoot*>(rev->getUsableBaseAddress());
    std::uint32_t final_sum = 0U;
    std::size_t final_count = 0U;
    for (const auto& entry : rev_root->responses)
    {
        final_sum += entry.second;
        ++final_count;
    }
    if (final_count != 3U || final_sum != 700U)  // 100 + 200 + 400
    {
        std::fprintf(stderr,
                     "consumer: cross-write verify FAILED (expected count=3 sum=700, got count=%zu sum=%u)\n",
                     final_count,
                     final_sum);
        return kExitVerifyFailed;
    }
    std::fprintf(
        stderr, "consumer: verified producer cross-wrote into region B (count=%zu sum=%u)\n", final_count, final_sum);
    std::printf("verified\n");
    return 0;
}

enum class Role
{
    kUnknown,
    kProducer,
    kConsumer
};

Role ParseRole(int argc, char** argv) noexcept
{
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--role") == 0 && (i + 1) < argc)
        {
            const char* const v = argv[i + 1];
            if (std::strcmp(v, "producer") == 0)
            {
                return Role::kProducer;
            }
            if (std::strcmp(v, "consumer") == 0)
            {
                return Role::kConsumer;
            }
        }
    }
    return Role::kUnknown;
}
}  // namespace

int main(int argc, char** argv)
{
    const Role role = ParseRole(argc, argv);
    if (role == Role::kUnknown)
    {
        std::fprintf(stderr, "usage: %s --role <producer|consumer>\n", argv[0]);
        return kExitBadArgs;
    }

    std::uint64_t paddr = 0U;
    std::uint64_t size = 0U;
    if (!qemu_shared_ivshmem::DiscoverIvshmemBar(paddr, size))
    {
        return kExitDiscoverFailed;
    }
    // Need room for two lib-memory regions (control block + kReserve each) plus the handshake.
    if (size <= (kHandshakeReserve + 2U * (kReserve + kPageSize)))
    {
        std::fprintf(stderr, "ivshmem BAR too small: 0x%llx\n", static_cast<unsigned long long>(size));
        return kExitMapFailed;
    }
    const BarLayout layout = ComputeLayout(paddr, size);

    void* const raw = MapBarRaw(paddr, size);
    if (raw == nullptr)
    {
        std::fprintf(stderr, "failed to raw-map the ivshmem BAR for the handshake\n");
        return kExitMapFailed;
    }
    Handshake* const hs = HandshakeAt(raw, size);

    return (role == Role::kProducer) ? RunProducer(layout, hs) : RunConsumer(layout, hs);
}
