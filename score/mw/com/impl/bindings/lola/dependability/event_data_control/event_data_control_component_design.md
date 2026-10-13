
## Event related datastructures in LoLa binding

Here we provide insight, how event communication is realized within our `LoLa` (shared memory based) binding. The
following class diagram shows LoLa specific binding implementation and its relationship with binding independent parts:

<img alt="EVENT_LOLA_MODEL" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/design/events_fields/event_lola_model.puml">

The referenced class `SubscriptionStateMachine` within this model is further detailed (including its dependent entities)
further [below](#event-subscription).

We use two different event related data structures, which get placed in shared memory:

1. `EventDataStorage<SampleType>`: Basically a vector (aka storage slots) of the raw event data of SampleType.
2. `EventDataControl`: Control structures, which maintain a specific state for each storage slot.

The reason for dividing this event related information into separate data structures stems from our need to support
safety critical setups! While `EventDataStorage<SampleType>` needs only be writeable by the skeleton instance, we can
put it into one single shared memory object, which gets mapped read-only by all subscribing proxies, independent,
whether they are ASIL or QM qualified. For `EventDataControl` things are different: This data structure has
to be r/w for all participants (skeleton, QM proxies, ASIL proxies). Therefore, we have to partition it into a different
shared memory object, which is r/w accessible by **all** participants. Moreover - in specific setups, where we have an
ASIL provider and both - ASIL and QM consumers, we need two instances of `EventDataControl`. One for the
ASIL participants and one for the QM participants to guarantee freedom from interference.

There is an intrinsic relation between `EventDataControl` and `EventDataStorage<SampleType>`, namely between
their members `event_slots` and `state_slots`! Both are vectors of the exact same size! While `event_slots[i]` contains
the data of a specific event sample, `state_slots[i]` contains state information for exactly the same slot.

### Sizing of `event_slots` and `state_slots` vectors

How many `storage slots` are needed to hold event sample data within the `EventDataStorage<SampleType>` instance and its
corresponding `EventDataControl` instance(s), depends on the number of subscribers (proxies) and the
value/number for parameter `maxSamples` they specified in their subscription.

The concrete formula is:
`num storage Slots` = 1 + Sum of maxSamples of all subscribers

Since the presence of consumers can change dynamically during runtime (they can show up and subscribe and unsubscribe
again), the number of needed `storage slots` is fluctuating. Since a fully dynamic number of `storage slots` (
allocation/deallocation)
during runtime is detrimental to very efficient lock-free data structures and algorithms, we are going to use, we will
**pre-allocate** a configurable maximum number of `storage slots` in `EventDataControl` and
`EventDataStorage<SampleType>` at the time, where they get created by the service instance.

Whether the pre-allocation wasn't sufficient, would be detected during runtime, when a new consumer tries to subscribe
to the event with a certain value of parameter `maxSamples`, which can't be fulfilled by the pre-allocation. The
subscription of the consumer would fail. This can then be fixed in integration by extending the configuration for max
pre-allocation of `storage slots`.

**Note**: The calculation of `num storage Slots` is up to the user. The number of slots actually allocated is exactly equal
to the value specified in the configuration. Therefore, if the user wants to use the formula above, they have to do it themselves
and set the result of that calculation in the configuration.

### Lock-free/non-blocking access to control data

Since `EventDataControl` instances get concurrently accessed in a r/w manner from skeleton and proxy
instances, the central member`state_slots` has been designed for concurrent lock-free access based on atomics.
Essentially we have the following concurrent r/w accesses here:

* Skeleton allocates/sends or simply sends a new event sample.
* Proxy searches' event(s) with timestamp > refTimestamp and increments slot refcount.
* Proxy dereferences refcount of a sample/slot.

The `state_slots` uses `std::atomic<EventSlotStatus::value_type>`. So `EventSlotStatus::value_type` is designed to be lock-free
on 64-Bit platforms!
I.e.`std::atomic<EventSlotStatus::value_type>::is_always_lock_free` has to return `true`. Otherwise, our concept would not work well!
The member `timestamp` in `EventSlotStatus` is overloaded, since there are specific values, which are not
interpreted as real timestamps, but as markers, whether this slot has not yet any valid sample data (data invalid) or
whether it is currently blocked in a write-state as the slot has been currently handed out to skeleton application layer
in the form of an `score::mw::com::SampleAllocateePtr`, where the application is currently busy creating event data using
the zero-copy path of the `mw::com` event API.

Based on this control data design, the concurrent functionalities mentioned above can be implemented lock-free, just
using
`std::atomic::store`, `std::atomic::load`, `std::atomic::compare_exchange`.

Note: We intentionally left out ordering/sorting of those `state_slots` vectors! Although at first glance it would be
useful (skeleton allocation and proxy event search would benefit from an ordering by timestamp), it could **not** be
done lock-free<sup id="lockfreeref">[1](#lockfree)</sup>. So we plan to optimize more aggressively regarding space
consumption and compactness of the
`state_slots` vector! The elements of `state_slots` (std::atomic<EventSlotStatusType>) are roughly 5 bytes in size. A
typical cache line of 64 Byte could hold roughly 12 elements. We do not expect `state_slots` vectors with more than 50
elements. Typically, much less! This means that we should in our case have extreme cache locality! Just using/scanning a
few cache lines anyhow, which should be blistering fast (will be benchmarked).

#### Slot access in mixed-criticality setup

The basic lock-free algo for a skeleton instance to search for a "free" slot in the context of a Send/Allocate call is:

Go through the elements in the `state_slots` vector and check, whether the slot is either empty (invalid timestamp) or
whether it has a ref_count of 0 and has the lowest/oldest timestamp of all slots with ref_count 0. If it has found such
a slot, it "occupies" it with a compare_exchange() operation (to shield against ref_count changes, which have happened
in between) by setting the timestamp to the special "blocked" value. If the `compare_exchange` detects a change, the
search has to start again.

Things get a bit more complex in mixed-criticality setups. Here the skeleton instance has to apply the "free" slot
search in two different `EventDataControl` instances. One in ASIL-QM shared-mem-object and one in ASIL-B
shared-mem-object. The adaption of the shown algo is as follows:

- the skeleton starts with the algo from above in the ASIL-B `EventDataControl` first.
- If it has found/blocked a slot successfully, it tries to acquire the same slot/index in the ASIL-QM
  `EventDataControl`. I.e. the corresponding slot needs to have also ref_count 0!
- If it can't acquire the slot in the QM section successfully, it has to unblock the slot in the ASIL-B section and
  restart from scratch.
- It should have a max. number of retry cycles. If after this cycle number no slot could be blocked, this is a sign,
  that the QM consumers did corrupt the QM section. In this case the ASIL skeleton should ignore/dump the QM section and
  just fall back only on searching/blocking in the ASIL section.

The cost in memory for typical generic lock-free queues/vectors, which rely on copying and versioning far outweighs the
benefit. [↩](#lockfreeref)

