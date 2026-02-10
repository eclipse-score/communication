# Support for Generic Skeletons

## Introduction

Generic skeletons are skeletons, which are not generated based on compile-time info about types used within the service
interface. Instead, they are skeletons, which can be instantiated at runtime and be connected to any service consuming
instance (proxy) just based on deployment information.

Since such generic skeletons are loosely typed (don't have the strong type info of the services event/field/service-method
data-types), they provide a different C++ API opposed to normal (strongly typed) skeletons.

The use cases for such restricted generic skeletons are typically for dynamically providing "raw data" in terms of blobs
from service providers. Interpreting the blob data semantically is obviously hard to accomplish.
If this is needed (normal use case) one should use the normal/strongly typed skeletons.


## LoLa supported feature set for Generic Skeletons

Since `LoLa`s support for fields is restricted to only the event-functionality of fields, we will in a 1st step **only**
implement event specific functionality for the `Generic Skeleton`.

This comprises the following items:
* `GenericSkeleton` class: We will provide a class `GenericSkeleton`, but opposed to the concept (linked above) **not** in
  the `ara::com`, but in the `mw::com` namespace.
* This `GenericSkeleton` class will (in relation to the concept) **only** provide a public method `GetEvents()`, which
  returns a `GenericSkeleton::EventMap` (which is a stripped down `std::map`)
* this map returned by `GetEvents()` will only contain all the events mentioned in the service deployment. Later (when
  our fields get extended with get/set method functionality).
* This `GenericSkeleton` will be based on the signatures of our current `LoLa` typed skeleton class:
  * base skeleton interface
  * skeleton event interface

There will be **no** changes done on `LoLa` configuration approach. This means the `LoLa` configuration (deployment of
service instances) is processed once at startup. The deployment information of a service instance, to which a
`GenericSkeleton` shall offer, needs to be provided in the `LoLa` configuration at application startup. This means,
that any deployment update affecting a `LoLa` application needs an application restart to become effective.

## Architecture enhancements for GenericSkeleton support

The main addition to the architecture for the interaction between a service providing instance (skeleton) and a service
consuming instance (proxy) is, that there now is some additional meta-data needed in the `lola::ServiceDataStorage`,
which resides in the shared-memory object containing the service instance specific data. This `lola::ServiceDataStorage`
is already described in the design details of our data layout in the shared memory
here

Before introduction of `GenericSkeleton` support, the skeleton side constructed in the shared-memory object for **Data** an
instance of `lola::ServiceDataStorage`, which is a map between an event-identifier and a slot-vector of event-data for
the given event-identifier.
When strongly typed skeletons offer to this shared-memory object for data, they exactly know the type (size/layout) of
this slot-vector, since the event datatype is known to them at compile-time. So they are able to:
* cast the slot-vector of event-data, which is held within the map as a `OffsetPtr<void>` to the correctly typed vector
  (`score::memory::shared::Vector<SampleType>`)
* access therefore the vector elements as `SampleType`

`GenericSkeleton`s don't know the exact type (size/layout) at compile time. However, provided with runtime meta-info (size/alignment), they must allocate and describe the slot-vector in shared memory so that consumers (Generic Proxies) can find and interpret it. So this
information has now additionally provided by the skeleton. So we extend `lola::ServiceDataStorage` with an additional
meta-data section:

I.e. beside the given map for the event data slots a second map is provided, which gives the meta-info for each event
provided by the service:
`score::memory::shared::Map<ElementFqId, EventMetaInfo> events_metainfo_`

### Meta-Info

The `lola::EventMetaInfo` is displayed in the class diagram for shared-mem data.
It contains meta-info of the events data type in the form of `lola::DataTypeMetaInfo` and an `OffsetPtr` to the
location, where the event-slot vector is stored. With this combined information a `GenericProxy`, which just knows the
deployment info (essentially the `ElementFqId` of the event) is able to access events in the event-slot vector in its raw
form (as a byte array).

The `lola::DataTypeMetaInfo` contains the following members:
* `fingerprint_` : unique fingerprint for the data type. This is a preparation/placeholder for an upcoming feature,
 which will allow doing a runtime check at the proxy side during connection to the service provider, whether provider
 and consumer side are based on the same C++ interface definition.
* `size_of_` : the result of `sizeof()` operator on the data type (in case of event/field: `SampleType`)
* `align_of_` : the result of `alignof()` operator on the data type (in case of event/field: `SampleType`)

### initialization of `lola::EventMetaInfo`

The map containing the meta-information gets initialized by the skeleton instance together with the setup/creation of
the vectors containing the sample slots via `RegisterGeneric` (or the internal `CreateEventDataFromOpenedSharedMemory` helper).
```cpp
std::pair<score::memory::shared::OffsetPtr<void>,EventDataControlComposite> 
lola::Skeleton::RegisterGeneric(const ElementFqId element_fq_id,
                                const SkeletonEventProperties& element_properties,
                                const size_t sample_size,
                                const size_t sample_alignment)
```
This happens during initial offering-phase of a service instance, before potential consumers/proxies are able to access
the shared-memory object. `GenericSkeleton`s will utilize this mechanism to register their events.

## GenericSkeleton class

As shown in the class diagram, below, the `mw::com::impl::GenericSkeleton` is **not** a generated (from some interface
specification) class (see Sys-Req-13525992), derived from
`impl::SkeletonBase` (like `DummySkeleton` example) 

### Creation and Configuration
Runtime instantiation is handled via the static `GenericSkeleton::Create` method, which accepts `GenericSkeletonCreateParams`. This structure allows users to define the service elements dynamically without compile-time type information.

```cpp
struct EventInfo {
    std::string_view name;
    DataTypeMetaInfo data_type_meta_info; // contains size and alignment
};

struct GenericSkeletonCreateParams {
    score::cpp::span<const EventInfo> events{};
};
```

The `GenericSkeleton`contains one member `events_`, which is a map of
`impl::GenericSkeletonEvent` in the form of `mw::com::EventMap`.

<img alt="GENERIC_SKELETON_MODEL" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/design/skeleton_proxy/generic_skeleton/generic_skeleton_model.puml">

Classes drawn with yellow background are extensions of the class diagram to support `GenericSkeleton` functionality beside
"normal" skeletons.
While there is a new `impl::GenericSkeleton` in the binding **independent** area, we currently do not need to reflect it in
the binding specific part as `impl::SkeletonBinding` (and therefore `lola::Skeleton` implementing `impl::SkeletonBinding`)
doesn't need any extensions for `GenericSkeleton` functionality.
On the level below the skeleton (skeleton-event level) this looks different. In the binding independent part we introduced the
non-templated class `impl::GenericSkeletonEvent` beside the existing class template `impl::SkeletonEvent<SampleType>`.
Both derive from `impl::SkeletonEventBase`, which contains all the `SampleType` agnostic/independent signatures/
implementations a skeleton-event needs to have.
The methods `Send()` and `Allocate()` are `SampleType` specific (via their arguments, which depend on the `SampleType`).
Therefore, they aren't contained in `impl::SkeletonEventBase` but specifically implemented in `impl::GenericSkeletonEvent` and
`impl::SkeletonEvent<SampleType>`.

We have a similar setup on the binding side: `impl::GenericSkeletonEventBinding` has been introduced beside
`impl::SkeletonEventBinding<SampleType>` as abstract classes/interface definitions for "normal", respectively generic
skeleton-event bindings.
Their common abstract base class `impl::SkeletonEventBindingBase` aggregates all interfaces, which are `SampleType`
agnostic and are common to both skeleton-event bindings.

On `LoLa` implementation level for the skeleton-event binding, we added `lola::GenericSkeletonEvent` beside
`lola::SkeletonEvent<SampleType>`.
To factor out common code/implementations for all signatures, which are `SampleType` agnostic and common to both
`lola::GenericSkeletonEvent` and `lola::SkeletonEvent<SampleType>`
the class `lola::SkeletonEventCommon` has been
introduced, which implements this common code. I.e. instances of `lola::GenericSkeletonEvent` and `lola::SkeletonEvent<SampleType>`
create such an instance and dispatch the corresponding methods to it. We decided to go for this composite/dispatch
approach instead of introducing a common base class to `lola::GenericSkeletonEvent` and `lola::SkeletonEvent<SampleType>`,
which provides/implements those signatures as this would have meant, that `lola::GenericSkeletonEvent` and
`lola::SkeletonEvent<SampleType>` had multi-inheritance (from this base class and their corresponding interface)!
Even if this hadn't been problematic (in terms of potential diamond pattern), we would have to explicitly argue/analyse
it due to the `ASIL-B` nature of our code base and the general avoidance pattern of multi-inheritance.
