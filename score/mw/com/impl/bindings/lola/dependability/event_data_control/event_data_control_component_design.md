# LoLa Event Data Control Unit Design

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

## Layout of Shared Memory Objects

### Introduction

This section shall provide a compact overview over the entire shared-memory layout. I.e. the data structures stored
within shared-memory by `Lola`. Other parts of the design documentation e.g.
[event and field description part](../events_fields/README.md#introduction) already describe/hint in their class
diagrams via the stereotype `<<SharedMemory>>` in the following form:

![Stereotype](stereotype.svg)

whether some classes (instances of it) are partially placed within shared-memory. We use the same stereotype here, but
give a complete overview.

According to our
[architecture concept](../../../../docs/features/ipc/lola/ipnext_README.md)
LoLa uses two different types of shared-memory objects per service instance:

* shared-memory object for **Data**
* shared-memory object for **Control**

The **specific** content of those shared-memory objects will be described below. But first the **communality** of those two
shared-memory types are described.

### Shared-Memory Object as Memory Resource

All shared-memory objects are represented by `score::memory::shared::SharedMemoryResource` and as such they inherit from
`ManagedMemoryResource` and `score::cpp::pmr::memory_resource`. Therefore, they provide the mechanisms needed by `C++17`
allocator/pmr aware (STL) containers to serve as a memory resource providing memory allocation. To fulfill this task
`SharedMemoryResource` uses a control block (`SharedMemoryResource::ControlBlock`), which is located at the beginning of
the address space of a shared-memory object/`SharedMemoryResource`. It provides the bookkeeping mechanism for the memory
available to the "user" of the `SharedMemoryResource` (noting, where the first user available memory starts, how many
bytes are already allocated/left).

The following class diagram provides an overview of the classes, whose instances are placed within shared-memory and
also a rough indication how the provider side (in the form of `LoLa` skeleton) and the consumer side (in the form of
`LoLa` proxy/proxy-event) interact with them.

<img alt="SHARED_MEM_LAYOUT_CLASSDIAGRAM" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/impl/bindings/lola/dependability/event_data_control/shared_mem_layout_classdiagram.puml">

This object diagram below (showing a concrete instantiation of the class diagram above for some example service instance)
depicts more clearly the relation between the `SharedMemoryResource` instances representing a shared-memory object and the
anchor/root elements placed within shared-memory:

<img alt="SHARED_MEM_LAYOUT_OBJECTDIAGRAM" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/impl/bindings/lola/dependability/event_data_control/shared_mem_layout_objectdiagram.puml">

So - the first object/element created within the shared-memory object for **Data** (represented by `SharedMemoryResource`
instance `shmResource_storage_SI_1`) is the instance `serviceDataStorage_SI_1` of class `ServiceDataStorage` and
therefore the corresponding `ControlBlock::start` points to `serviceDataStorage_SI_1`.

Accordingly, the first object/element created within the shared-memory object for the QM **Control** (represented by
`SharedMemoryResource` instance `shmResource_control_qm_SI_1`) is the instance `serviceDataControl_qm_SI_1` of class
`ServiceDataControl` and therefore the corresponding `ControlBlock::start` points to `serviceDataControl_qm_SI_1`.

In the object diagram example we used a service instance with additional ASIL-B support, so besides a QM
`SharedMemoryResource` we also do have an ASIL-B `SharedMemoryResource`.

### Shared-Memory Object for Data

The anchor/root element (i.e. the location, where `SharedMemoryResource::ControlBlock::start` points to), which gets
placed into the `SharedMemoryResource` for **Data** is an instance of `lola::ServiceDataStorage`. It gets created by a
`LoLa` skeleton instance during `lola::Skeleton::PrepareOffer()` within the initialization callback of the
newly created shared-memory object.
This anchor element of the `SharedMemoryResource` contains two members:
* `events_`: a fixed-capacity, associative container (`lola::LinearSearchMap`) for the data storage (slots) of every
  event the service provides.
  Note, that the storage slots, which are effectively vectors of `SampleType`s:
  ```
  template <typename SampleType>
  using EventDataStorage = score::memory::shared::Vector<SampleType>
  ```
  are stored here as "type-erased" `score::memory::shared::OffsetPtr<void>`, because every slot-vector has a different
  type and only later when the skeleton events register themselves dynamically or access that storage they are cast to
  the effective/concrete type.

* `events_metainfo_`: a fixed-capacity, associative container (`lola::LinearSearchMap`) for meta information of every
  event the service provides
* `skeleton_pid_`: PID of the provider/skeleton process.

> **Note on container choice / shared-memory sizing:** `ServiceDataStorage` deliberately does **not** use dynamically
> allocating map-types (like `std::map` / `boost::interprocess::map`) anymore. Instead it uses `lola::LinearSearchMap`,
> a flat, fixed-capacity container built on `score::containers::NonRelocatableVector`. Its capacity (the number of
> service-elements = events + fields) is known up-front, so the container performs exactly one allocation of a
> deterministic size on construction. This makes the total size of the **Data** shared-memory object analytically
> computable via `SkeletonMemoryManager::CalculateDataShmResourceStorageSize()` (selected through the
> `ShmSizeCalculationMode::kAnalysis` mode), avoiding the otherwise necessary (and costly) heap-based "simulation
> run".

### Shared-Memory Object for Control

The anchor/root element (i.e. the location, where `SharedMemoryResource::ControlBlock::start` points to), which gets
placed into the `SharedMemoryResource` for **Control** is an instance of `lola::ServiceDataControl`. It also gets
created by a `LoLa` skeleton instance during `lola::Skeleton::PrepareOffer()` within the initialization
callback of the newly created shared-memory object.
This anchor element of the `SharedMemoryResource` contains the members:
* `event_controls_`: a fixed-capacity `lola::LinearSearchMap` containing an `EventControl` entry for every event the
  service provides. Its capacity equals the number of offered service elements (events + fields), which is known
  up-front, so the required shared-memory size can be computed analytically (no simulation run). Each `EventControl`
  aggregates the event's data control slots (`EventDataControl`), subscription control (`EventSubscriptionControl`), and
  the transaction log set (`TransactionLogSet`).
* `application_id_pid_mapping_`: a mapping from application identifiers to process IDs, used by proxy instances to
  detect crashes and register their current PID.

### Local Views (Performance Optimization)

Accessing data structures inside shared-memory objects via `OffsetPtr` has a runtime cost: every dereference of an
`OffsetPtr` involves a pointer arithmetic computation relative to the base address of the shared-memory segment. In
hot-path operations (e.g. every `Send()`, every `GetNewSamples()` call), this overhead accumulates.

To eliminate this cost, **local view** classes are introduced. They are created once per Skeleton event / Proxy instance
at offer/subscription time by resolving the `OffsetPtr`s to ordinary raw pointers (or `span`s). During runtime the
hot-path code uses these local views exclusively.

The following local view classes exist:

| Local View | Side | Wraps |
|---|---|---|
| `ProviderEventDataControlLocalView` | Provider (Skeleton) | `EventDataControl` – provides `AllocateNextSlot`, `EventReady`, `Discard` |
| `ConsumerEventDataControlLocalView` | Consumer (Proxy / IPC-Tracing) | `EventDataControl` – provides `ReferenceNextEvent`, `DereferenceEvent`, `GetNumNewEvents` |
| `ConsumerEventControlLocalView` | Consumer (Proxy) | `EventControl` – aggregates `ConsumerEventDataControlLocalView`, a reference to `EventSubscriptionControl`, and a reference to `TransactionLogSet` |
| `ProxyServiceDataControlLocalView` | Consumer (Proxy) | `ServiceDataControl` – provides a `Map<ElementFqId, ConsumerEventControlLocalView>` built once at proxy construction |
| `TransactionLogLocalView` | Consumer | `TransactionLog` – provides transaction recording via raw pointers |

On the **skeleton side**, `SkeletonEventCommon::PrepareOfferCommon()` creates and stores `ProviderEventDataControlLocalView`
(one per quality level) and `ConsumerEventDataControlLocalView` (used by IPC-tracing) when the event is offered.
`EventDataControlComposite` (used by `lola::SkeletonEvent` for slot allocation) holds references to
`ProviderEventDataControlLocalView` instances rather than raw `EventDataControl` pointers.

On the **consumer side**, `ProxyServiceDataControlLocalView` is built at `lola::Proxy` construction time. It iterates
the `ServiceDataControl::event_controls_` map (resolving `OffsetPtr`s once) and stores a
`ConsumerEventControlLocalView` per event. `ProxyEventCommon` obtains its `ConsumerEventControlLocalView` reference
from `Proxy::GetEventControlLocal()` at construction. `SlotCollector` holds a reference to
`ConsumerEventDataControlLocalView` (obtained from `ConsumerEventControlLocalView::data_control`) and uses it in
`GetNewSamples()` for efficient slot scanning.
