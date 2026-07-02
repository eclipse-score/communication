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

#ifndef QUALITY_INTEGRATION_TESTING_ENVIRONMENTS_DUAL_QEMU_EXAMPLE_OFFSET_LIST_H
#define QUALITY_INTEGRATION_TESTING_ENVIRONMENTS_DUAL_QEMU_EXAMPLE_OFFSET_LIST_H

// Self-relative ("offset pointer") singly-linked list that lives ENTIRELY inside a shared
// region, plus the read/write helpers used by the cross-VM example. This is the transport-
// independent core: it only touches the mapped region, so it works regardless of how the
// region was mapped or at which virtual base each VM mapped it. That is the LoLa data-plane
// property -- a pointer stored in shared memory resolves on the other VM even though the
// bases differ -- modelled exactly like score::memory::shared::OffsetPtr.

#include <cstdint>
#include <cstdio>
#include <ctime>

namespace qemu_shared_ivshmem
{

inline constexpr std::uint32_t kMagic = 0x53484D31U;                // "SHM1": published once a slot is fully written
inline constexpr std::uint32_t kNodeCount = 8U;                     // nodes per linked list
inline constexpr std::uint32_t kBaseValue[2] = {0x1000U, 0x2000U};  // per-VM base so values differ

// A list node whose link is a self-relative byte offset, not a raw Node* (which would be a
// VM-local address, invalid on the peer).
struct Node
{
    std::uint32_t value;
    std::int64_t next_off;  // offset from &next_off to the next Node; 0 marks the end of the list
};

// One slot per VM: the owning VM writes it, the peer walks it.
struct Slot
{
    std::uint32_t magic;       // published LAST with a release store once the list is complete
    std::uint32_t node_count;  // number of nodes in the list
    std::int64_t head_off;     // offset from &head_off to the first Node (an offset pointer)
    Node nodes[kNodeCount];    // node storage, also inside the shared region
};

// slot[0] = VM-A, slot[1] = VM-B; both VMs agree on this layout.
struct Region
{
    Slot slot[2];
};

// Store a self-relative offset: byte distance from the field to target (0 == null).
inline void StoreOffset(std::int64_t& field, const void* target)
{
    field = (target == nullptr)
                ? 0
                : reinterpret_cast<const std::uint8_t*>(target) - reinterpret_cast<std::uint8_t*>(&field);
}

// Resolve an offset pointer back into an address in THIS process's mapping.
inline void* LoadOffset(const std::int64_t& field)
{
    if (field == 0)
    {
        return nullptr;
    }
    const std::uint8_t* const base = reinterpret_cast<const std::uint8_t*>(&field);
    return const_cast<std::uint8_t*>(base) + field;
}

// Wait (up to timeout_ms) until the peer publishes its slot via an acquire/release magic.
inline bool WaitForMagic(const std::uint32_t& magic, std::uint32_t timeout_ms)
{
    constexpr std::uint32_t kStepMs = 20U;
    for (std::uint32_t waited = 0U;; waited += kStepMs)
    {
        if (__atomic_load_n(&magic, __ATOMIC_ACQUIRE) == kMagic)
        {
            return true;
        }
        if (waited >= timeout_ms)
        {
            return false;
        }
        const struct timespec ts{0, static_cast<long>(kStepMs) * 1000000L};
        (void)::nanosleep(&ts, nullptr);
    }
}

// Build this VM's list inside its own slot and publish it. Every node and link lives in the
// shared region, so the peer can walk the list at its own mapping base.
inline void WriteOwnSlot(Region& region, int id, const char* name, const void* base)
{
    Slot& slot = region.slot[id];
    for (std::uint32_t i = 0U; i < kNodeCount; ++i)
    {
        slot.nodes[i].value = kBaseValue[id] + i;
        StoreOffset(slot.nodes[i].next_off, (i + 1U < kNodeCount) ? &slot.nodes[i + 1U] : nullptr);
    }
    slot.node_count = kNodeCount;
    StoreOffset(slot.head_off, &slot.nodes[0]);
    // Release store: makes the nodes + offsets above visible to a peer's acquire load.
    __atomic_store_n(&slot.magic, kMagic, __ATOMIC_RELEASE);
    std::printf(
        "%s wrote %u-node offset-pointer list into slot %d (region mapped at %p)\n", name, kNodeCount, id, base);
}

// Wait for the peer to publish, then walk its list through offset pointers and verify.
// Returns a process exit code (0 = OK).
inline int ReadPeerSlot(Region& region, int id, const char* name, const void* base)
{
    const int peer = 1 - id;
    Slot& slot = region.slot[peer];

    constexpr std::uint32_t kWaitMs = 15000U;
    if (!WaitForMagic(slot.magic, kWaitMs))
    {
        std::fprintf(stderr, "%s: peer slot %d not published within %ums\n", name, peer, kWaitMs);
        return 3;
    }

    // head_off/next_off are self-relative, so this resolves even though we mapped the region
    // at a DIFFERENT base than the writer.
    std::printf("%s reading peer slot %d (region mapped at %p):", name, peer, base);
    std::uint32_t seen = 0U;
    bool ok = true;
    for (auto* node = static_cast<Node*>(LoadOffset(slot.head_off));
         node != nullptr && seen <= kNodeCount;  // bound also guards against a cyclic list
         node = static_cast<Node*>(LoadOffset(node->next_off)))
    {
        std::printf(" %u", node->value);
        ok = ok && (node->value == kBaseValue[peer] + seen);
        ++seen;
    }
    std::printf("\n");

    if (!ok || seen != slot.node_count)
    {
        std::fprintf(stderr, "%s: list verification FAILED (seen=%u, node_count=%u)\n", name, seen, slot.node_count);
        return 4;
    }
    std::printf("%s verified %u nodes via offset pointers OK\n", name, seen);
    return 0;
}

}  // namespace qemu_shared_ivshmem

#endif  // QUALITY_INTEGRATION_TESTING_ENVIRONMENTS_DUAL_QEMU_EXAMPLE_OFFSET_LIST_H
