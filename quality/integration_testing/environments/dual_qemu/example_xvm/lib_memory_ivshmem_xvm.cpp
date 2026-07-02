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

// Stage-2 CROSS-VM demo: PRODUCTION lib memory (score::memory::shared) over the ivshmem BAR,
// with one VM CREATING and the other VM OPENING the same region.
//
//   lib_memory_ivshmem_xvm --role producer   // VM-A: Create()+init the OffsetPtr list
//   lib_memory_ivshmem_xvm --role consumer   // VM-B: Open() the peer's region and verify
//
// This is the piece Stage 1 (single-VM lib_memory_ivshmem) does not prove: that a SECOND VM
// can attach to the SAME physical BAR through the lib-memory allocator and resolve the
// producer's OffsetPtr graph, even though the two VMs map the BAR at different virtual bases.
//
// Why a handshake is needed (the two hard cross-VM problems):
//   1. Separate shm namespaces. Each QNX guest has its OWN shm object namespace: the name
//      "/lola_ivshmem" on VM-A and on VM-B are DIFFERENT objects. What makes them the same
//      memory is that BOTH are bound to the SAME BAR physical address via
//      shm_ctl(SHMCTL_PHYS). So the consumer must FIRST bind its own name to the BAR (via the
//      provider) and only THEN call SharedMemoryFactory::Open() -- Open() itself does a plain
//      shm_open() and never consults the typed-memory provider.
//   2. Init ordering. The consumer must not Open()/parse the control block until the producer
//      has finished Create() (which writes the control block + the list). ivshmem-plain has no
//      doorbell, so we rendezvous by polling a small handshake header placed in the LAST page
//      of the BAR -- outside the lib-memory arena (which grows from the BAR base upward and
//      uses only the control block + kReserve bytes).
//
// Protocol (best-effort; see caveats at the bottom):
//   producer: zero handshake -> Create()+init (embeds a run `nonce` in Root) -> publish
//             {nonce, READY} -> wait for consumer ACK -> exit.
//   consumer: wait for READY -> bind name->BAR -> Open() -> verify magic + nonce + list sum
//             -> publish ACK -> exit.
//
// QNX-only (uses pci-server + mmap_device_memory + shm_ctl(SHMCTL_PHYS)).

#include "quality/integration_testing/environments/dual_qemu/example_xvm/bar_discovery.h"
#include "quality/integration_testing/environments/dual_qemu/example_xvm/bar_typed_memory.h"

#include "score/memory/shared/i_shared_memory_resource.h"
#include "score/memory/shared/offset_ptr.h"
#include "score/memory/shared/shared_memory_factory.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>

#if defined(__QNXNTO__)
#include <sys/mman.h>  // mmap_device_memory / munmap_device_memory
#include <unistd.h>    // getpid
#endif

namespace
{
using score::memory::shared::ISharedMemoryResource;
using score::memory::shared::OffsetPtr;
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
constexpr std::uint32_t kReadyMagic = 0x0AD10ADEU;  // producer -> consumer: data ready
constexpr std::uint32_t kAckMagic = 0x0AC0AC0AU;    // consumer -> producer: verified

constexpr char kShmName[] = "/lola_ivshmem";
constexpr std::size_t kReserve = 4096U;           // lib-memory user bytes (arena from BAR base)
constexpr std::size_t kHandshakeReserve = 4096U;  // last page of BAR reserved for the handshake
constexpr int kPollTimeoutMs = 30000;             // rendezvous timeout
constexpr int kPollStepMs = 10;

// A minimal shared data structure using PRODUCTION lib-memory OffsetPtr, so it resolves on
// the consumer VM despite the different mapping base. `nonce` links this Create to the
// handshake so the consumer can reject stale BAR contents from a previous run.
struct Node
{
    std::uint32_t value;
    OffsetPtr<Node> next;
};

struct Root
{
    std::uint32_t magic;
    std::uint32_t nonce;
    OffsetPtr<Node> head;
};

// Handshake header placed at the END of the BAR (raw-mapped), never touched by lib memory.
// volatile because it is written by one VM and polled by the other with no cache-coherency
// help beyond ivshmem-plain's shared host RAM.
struct Handshake
{
    volatile std::uint32_t ready;  // producer -> consumer
    volatile std::uint32_t nonce;  // producer's run nonce, echoed in Root.nonce
    volatile std::uint32_t ack;    // consumer -> producer
};

void SleepMs(int ms) noexcept
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = static_cast<long>(ms % 1000) * 1000000L;
    (void)nanosleep(&ts, nullptr);
}

// Raw-maps the BAR physical memory so we can reach the handshake header. Discovery has
// already requested I/O privilege and enabled the device's memory decoding.
void* MapBarRaw(std::uint64_t paddr, std::uint64_t size) noexcept
{
#if defined(__QNXNTO__)
    void* const addr = ::mmap_device_memory(nullptr, size, PROT_READ | PROT_WRITE, 0, static_cast<off_t>(paddr));
    return (addr == MAP_FAILED) ? nullptr : addr;
#else
    (void)paddr;
    (void)size;
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

// After Open(), the producer's first construct<Root>() sits at the usable base address.
// Align up defensively in case the arena start is padded to alignof(Root).
Root* LocateRoot(const std::shared_ptr<ISharedMemoryResource>& resource) noexcept
{
    auto addr = reinterpret_cast<std::uintptr_t>(resource->getUsableBaseAddress());
    addr = (addr + (alignof(Root) - 1U)) & ~(static_cast<std::uintptr_t>(alignof(Root)) - 1U);
    return reinterpret_cast<Root*>(addr);
}

int RunProducer(std::uint64_t paddr, std::uint64_t size, Handshake* hs) noexcept
{
    // Reset the handshake for this run before anyone can observe READY.
    hs->ack = 0U;
    hs->nonce = 0U;
    hs->ready = 0U;
    std::atomic_thread_fence(std::memory_order_release);

    SharedMemoryFactory::SetTypedMemoryProvider(std::make_shared<qemu_shared_ivshmem::BarTypedMemory>(paddr, size));

    std::uint32_t nonce = 0U;
#if defined(__QNXNTO__)
    nonce = static_cast<std::uint32_t>(getpid());
#endif
    nonce = (nonce << 8) ^ static_cast<std::uint32_t>(std::time(nullptr));
    if (nonce == 0U)
    {
        nonce = 1U;
    }

    auto resource = SharedMemoryFactory::Create(
        kShmName,
        [nonce](std::shared_ptr<ISharedMemoryResource> res) {
            Root* const root = res->construct<Root>();
            Node* const second = res->construct<Node>();
            second->value = 43U;
            second->next = nullptr;
            Node* const first = res->construct<Node>();
            first->value = 42U;
            first->next = second;
            root->magic = kMagic;
            root->nonce = nonce;
            root->head = first;
        },
        kReserve,
        SharedMemoryFactory::WorldWritable{},
        /*prefer_typed_memory=*/true);

    if (resource == nullptr)
    {
        std::fprintf(stderr, "producer: Create failed\n");
        return kExitCreateFailed;
    }
    if (!resource->IsShmInTypedMemory())
    {
        std::fprintf(stderr, "producer: NOT in typed memory (shm_ctl(SHMCTL_PHYS) rejected)\n");
        return kExitNotTypedMemory;
    }

    // Publish: make the data visible before announcing READY.
    std::atomic_thread_fence(std::memory_order_release);
    hs->nonce = nonce;
    hs->ready = kReadyMagic;
    std::fprintf(stderr, "producer: published list (nonce=0x%08x), waiting for consumer ACK\n", nonce);

    if (!WaitForWord(&hs->ack, kAckMagic, kPollTimeoutMs))
    {
        std::fprintf(stderr, "producer: timed out waiting for consumer ACK\n");
        return kExitHandshakeTimeout;
    }

    std::fprintf(stderr, "producer: consumer acknowledged\n");
    std::printf("verified\n");
    return 0;
}

int RunConsumer(std::uint64_t paddr, std::uint64_t size, Handshake* hs) noexcept
{
    if (!WaitForWord(&hs->ready, kReadyMagic, kPollTimeoutMs))
    {
        std::fprintf(stderr, "consumer: timed out waiting for producer READY\n");
        return kExitHandshakeTimeout;
    }
    std::atomic_thread_fence(std::memory_order_acquire);
    const std::uint32_t expected_nonce = hs->nonce;

    auto provider = std::make_shared<qemu_shared_ivshmem::BarTypedMemory>(paddr, size);
    SharedMemoryFactory::SetTypedMemoryProvider(provider);

    // Open() does a plain shm_open(), so this VM must FIRST bind its own name to the BAR.
    const auto bind = provider->AllocateNamedTypedMemory(
        static_cast<std::size_t>(size), kShmName, SharedMemoryFactory::WorldWritable{});
    if (!bind.has_value())
    {
        std::fprintf(stderr, "consumer: failed to bind name to BAR\n");
        return kExitOpenFailed;
    }

    auto resource = SharedMemoryFactory::Open(kShmName, /*is_read_write=*/true);
    if (resource == nullptr)
    {
        std::fprintf(stderr, "consumer: Open failed\n");
        return kExitOpenFailed;
    }

    const Root* const root = LocateRoot(resource);
    std::uint32_t sum = 0U;
    for (const Node* n = root->head.get(); n != nullptr; n = n->next.get())
    {
        sum += n->value;
    }
    if (root->magic != kMagic || root->nonce != expected_nonce || sum != 85U)
    {
        std::fprintf(stderr,
                     "consumer: verify FAILED (magic=0x%08x nonce=0x%08x/exp0x%08x sum=%u)\n",
                     root->magic,
                     root->nonce,
                     expected_nonce,
                     sum);
        return kExitVerifyFailed;
    }

    std::atomic_thread_fence(std::memory_order_release);
    hs->ack = kAckMagic;
    std::fprintf(stderr, "consumer: resolved producer OffsetPtr list (sum=%u), ACK sent\n", sum);
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
    if (size <= (kHandshakeReserve + kReserve))
    {
        std::fprintf(stderr, "ivshmem BAR too small: 0x%llx\n", static_cast<unsigned long long>(size));
        return kExitMapFailed;
    }

    void* const raw = MapBarRaw(paddr, size);
    if (raw == nullptr)
    {
        std::fprintf(stderr, "failed to raw-map the ivshmem BAR for the handshake\n");
        return kExitMapFailed;
    }
    Handshake* const hs = HandshakeAt(raw, size);

    return (role == Role::kProducer) ? RunProducer(paddr, size, hs) : RunConsumer(paddr, size, hs);
}
