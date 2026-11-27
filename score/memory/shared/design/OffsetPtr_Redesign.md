# OffsetPtr Bounds-Checking Redesign

## Motivation

We currently have the following issue with OffsetPtr related bounds-checking, which has been already discussed shortly.
The issue came up, when doing FMEA/FTA for `lib/memory/shared`.
Controlling, whether bounds-checking shall be done or not "process globally" (via a global variable) has severe safety
issues!
I.e. several (unrelated) parts within an application can have/see different needs, whether boundschecking shall be done
or not!
Since bounce-checking is costly, you don't want to turn it on generally, if you only need it in "some areas".
With the current approach, we need "scary" AoU, which tells you, when you are allowed to call `EnableOffsetPtrBoundsChecking()`
and when not and what the impact might be!

What do I mean with "some areas"?
Imho we want to decide, whether we do bounds-checking or not, based on the shared-memory-region, where an `OffsetPtr`
instance is located!
`lib/memory/shared` now becomes a general purpose SEooC. So the user-base will get bigger (not only `LoLa`) and I do expect,
that users will have specific shared-memory-regions, where bounds-checking is needed/wanted and others where it is not!

### When is bounds-checking needed

Bounds-checking is **generally** needed in the following situation:

> A shared-memory-region with accessing users having mixed-criticality AND QM-users having write-access to it

In any other case, there is **no** bounds-checking needed:

For `LoLa` specifically this means:

- DATA-segment provided by an ASIL-B application: **No bounds-checking**.
- DATA-segment provided by a QM application: **Bounds-checking**. Unless we can rule out ASIL-B consumers (then we would
  have NO mixed criticality).
- ASIL-B CONTROL-segment: **No bounds-checking**.
- QM CONTROL-segment provided by an ASIL-B application: **Bounds-checking**.
- QM CONTROL-segment provided by a QM application: **Bounds-checking**. Unless  we can rule out ASIL-B consumers (then
  we would have NO mixed criticality).

## Solution Proposal

### Separate OffsetPtr types

The existing `OffsetPtr<T>` gets refactored to:

```
enum class BoundsChecking { Disabled, Enabled };

template <typename T, BoundsChecking B = BoundsChecking::Disabled>
OffsetPtr
```

This already has severe benefits:

- An `OffsetPtr<T, BoundsChecking::Disabled>` has NO `memory_bounds_` member (for the copy-out-of-shm logic)! This
  already reduces the size of such an `OffsetPtr` (compared to the template instantiation with `BoundsChecking::Enabled`)
  by 16 Byte!
- The whole `OffsetPtr` internal logic, which always/everywhere has an `if(global-bounce-check-flag)`, which is a runtime
  check/access is then `constexpr`.

### PolymorphicOffsetPtrAllocator adaption

Consequently, the `PolymorphicOffsetPtrAllocator<T>` also gets refactored to:

```
template <typename T, BoundsChecking B = BoundsChecking::Disabled>
PolymorphicOffsetPtrAllocator
```

as we need to decide, whether we want to have a PolymorphicOffsetPtrAllocator, which creates OffsetPtr<T, BoundsChecking::Disabled> or OffsetPtr<T, BoundsChecking::Enabled>
So - for PolymorphicOffsetPtrAllocator we just have this signature-change - no other changes/adaptions needed.

### Adaptions to LoLa

Now - since `OffsetPtr`/`PolymorphicOffsetPtrAllocator` class templates do have this additional template arg
`BoundsChecking`, also `LoLa` types dealing with these, need adaptions.
For example the data types `LoLa` places "top-level" into shared memory:
- `ServiceDataControl`
- `ServiceDataStorage`

need to get templated with template arg BoundsChecking and then need to "forward" it to their children/members.

### Optional SharedMemoryResource extension

The decision, whether to use `OffsetPtr<T, BoundsChecking::Disabled>` or `OffsetPtr<T, BoundsChecking::Enabled>` within
a shared-memory-object (`SharedMemoryResource`) will be taken by the creator.
He knows the context of the usage and what other users/consumers of this shared-memory-object will come up. This
knowledge he already uses, when setting access-permissions for the shared-memory-object.

However, the consumers might not know it prematurely. I.e. they may be fine with both types of `OffsetPtr`.
If you think of `LoLa`, there would be such a case:
You have a `LoLa` consumer/Proxy, which is QM. He wants to consume a service S, where it doesn't care, whether it is
provided by an ASIL-B or a QM provider!
But an ASIL-B provider of S would use (see above) `OffsetPtr<T, BoundsChecking::Disabled>` in the DATA-segment, whereas
a QM provider would use  `OffsetPtr<T, BoundsChecking::Enabled>`.
The `LoLa` consumer/Proxy needs to now - when opening the shm-objects of S, whether to expect/interpret the top-level
`ServiceDataControl`/`ServiceDataStorage` as template instantiations with `BoundsChecking::Disabled` or `BoundsChecking::Enabled`
There are two ways in this concrete example:

1. The proxy knows that upfront in construction. I.e. we hand over in the ctor, whether the provider is QM/ASIL-B and
   then the proxy deduces, what `OffsetPtr` type will be used in DATA/CTRL sections and
   then takes it into account, when doing the static cast from `getUsableBaseAddress()` of the `SharedMemoryResource`.
   Currently, we do NOT have this knowledge in LoLa! We would need to e.g. widen our service-discovery API, etc.
   And this approach would not be very "generic".

2. Have a more generic approach! This is my proposal: We put an information in the CONTROL-BLOCK of the
   `SharedMemoryResource`, which indicates, whether this shared-memory resource was created with bounds-checking
   in mind or not and thus all `OffsetPtrs` will be one with `BoundsChecking::Enabled` or `BoundsChecking::Disabled`
   I like this approach, because it is a facility on `lib/memory/shared` level! Not every user of this lib needs to then
   think about a mechanism, how to exchange this important info on their own.
    