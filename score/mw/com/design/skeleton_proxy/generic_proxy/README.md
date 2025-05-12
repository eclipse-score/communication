# Support for Generic Proxies

## Introduction

Generic proxies are proxies, which are not generated based on compile-time info about types used within the service
interface. Instead, they are proxies, which can be instantiated at runtime and be connected to any service providing
instance (skeleton) just based on deployment information.

Since such generic proxies are loosely typed (don't have the strong type info of the services event/field/service-method
data-types), they provide a different C++ API opposed to normal (strongly typed) proxies.

The use cases for such restricted generic proxies are typically for dynamically extracting "raw data" in terms of blobs
from service providers to dump it out/log it. Interpreting the blob data semantically is obviously hard to accomplish.
If this is needed (normal use case) one should use the normal/strongly typed proxies.

Details about the concept of generic proxies can be found
[here](../../../../../docs/features/ipc/lola/mw_com_dii/README.md)

## LoLa supported feature set for Generic Proxies

Since `LoLa`s support for fields is restricted to only the event-functionality of fields, we will in a 1st step **only**
implement event specific functionality for the `Generic Proxy`. 

This comprises the following items:
* `GenericProxy` class: We will provide a class `GenericProxy`, but opposed to the concept (linked above) **not** in 
  the `ara::com`, but in the `mw::com` namespace.
* This `GenericProxy` class will (in relation to the concept) **only** provide a public method `GetEvents()`, which
  returns a `GenericProxy::EventMap` (which is a stripped down `std::map`)
* this map returned by `GetEvents()` will only contain all the events mentioned in the service deployment. Later (when
  our fields get extended with get/set method functionality), this will change
  (broken_link_j/Ticket-156027)
* This `GenericProxy` will be based on the signatures of our current `LoLa` typed proxy class:
  * [base proxy interface](broken_link_g/swh/ddad_platform/blob/master/aas/mw/com/impl/proxy_base.h)
  * [proxy event interface](broken_link_g/swh/ddad_platform/blob/master/aas/mw/com/impl/proxy_event.h)

There will be **no** changes done on `LoLa` configuration approach. This means the `LoLa` configuration (deployment of
service instances) is processed once at startup. The deployment information of a service instance, to which a
`GenericProxy` shall connect to, needs to be provided in the `LoLa` configuration at application startup. This means,
that any deployment update affecting a `LoLa` application needs an application restart to become effective.

## Architecture enhancements for GenericProxy support

The main addition to the architecture for the interaction between a service providing instance (skeleton) and a service
consuming instance (proxy) is, that there now is some additional meta-data needed in the `lola::ServiceDataStorage`,
which resides in the shared-memory object containing the service instance specific data. This `lola::ServiceDataStorage`
is already described in the design details of our data layout in the shared memory
[here](../../shared_mem_layout/README.md#shared-memory-object-for-data)

Before introduction of `GenericProxy` support, the skeleton side constructed in the shared-memory object for **Data** an
instance of `lola::ServiceDataStorage`, which is a map between an event-identifier and a slot-vector of event-data for
the given event-identifier.
When strongly typed proxies connect to this shared-memory object for data, they exactly know the type (size/layout) of
this slot-vector, since the event datatype is known to them at compile-time. So they are able to:
* cast the slot-vector of event-data, which is held within the map as a `OffsetPtr<void>` to the correctly typed vector
  (`score::memory::shared::Vector<SampleType>`)
* access therefore the vector elements as `SampleType`

`GenericProxy`s don't know the exact type (size/layout)
but for later accessing this slot-vector,
they need to know at least the exact location in memory, where the slot-vector starts and the exact slot-size
[Sys-Req-13526377](broken_link_c/issue/13526377). So this
information has now additionally provided by the skeleton. So we extend `lola::ServiceDataStorage` with an additional
meta-data section:

I.e. beside the given map for the event data slots a second map is provided, which gives the meta-info for each event
provided by the service:
`score::memory::shared::Map<ElementFqId, EventMetaInfo> events_metainfo_`

### Meta-Info

The `lola::EventMetaInfo` is displayed in the [class diagram for shared-mem data](../../shared_mem_layout/README.md#shared-memory-object-as-memory-resource).
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
the vectors containing the sample slots in
```
template <typename SampleType>
std::pair<EventDataStorage<SampleType>*, EventDataControlComposite> lola::Skeleton::Register(ElementFqId event_fqn,
   std::size_t number_of_slots)
```

This happens during initial offering-phase of a service instance, before potential consumers/proxies are able to access
the shared-memory object.

## GenericProxy class

As shown in the class diagram, below, the `mw::com::impl::GenericProxy` is **not** a generated (from some interface
specification) class (see [Sys-Req-13525992](broken_link_c/issue/13525992)), derived from
`impl::ProxyBase` (like `DummyProxy` example) and contains one member `events_`, which is a map of
`impl::GenericProxyEvent` in the form of `mw::com::EventMap`.

![Generic Proxy Extension](broken_link_k/swh/ddad_score/mw/com/design/skeleton_proxy/generic_proxy/generic_proxy_model.uxf?ref=18c835c8d7b01056dd48f257c14f435795a48b7d)

Classes drawn with yellow background are extensions of the class diagram to support `GenericProxy` functionality beside
"normal" proxies.
While there is a new `impl::GenericProxy` in the binding **independent** area, we currently do not need to reflect it in
the binding specific part as `impl::ProxyBinding` (and therefore `lola::Proxy` implementing `impl::ProxyBinding`)
doesn't need any extensions for `GenericProxy` functionality.
On the level below the proxy(proxy-event level) this looks different. In the binding independent part we introduced the
non-templated class `impl::GenericProxyEvent` beside the existing class template `impl::ProxyEvent>SampleType>`.
Both derive from `impl::ProxyEventBase`, which contains all the `SampleType` agnostic/independent signatures/
implementations a proxy-event needs to have.
The method `GetNewSamples()` is `SampleType` specific (via its callback argument, which depends on the `SampleType`).
Therefore, it isn't contained in `impl::ProxyEventBase` but specifically implemented in `impl::GenericProxyEvent` and
`impl::ProxyEvent>SampleType>`. 

We have a similar setup on the binding side: `impl::GenericProxyEventBinding` has been introduced beside
`impl::ProxyEventBinding>SampleType>` as abstract classes/interface definitions for "normal", respectively generic
proxy-event bindings.
Their common abstract base class `impl::ProxyEventBindingBase` aggregates all interfaces, which are `SampleType`
agnostic and are common to both proxy-event bindings.

On `LoLa` implementation level for the proxy-event binding, we added `lola::GenericProxyEvent` beside
`lola::ProxyEvent<SampleType>`.
To factor out common code/implementations for all signatures, which are `SampleType` agnostic and common to both
&ndash; `lola::GenericProxyEvent` and `lola::ProxyEvent<SampleType>` &ndash; the class `lola::ProxyEventCommon` has been
introduced, which implements this common code. I.e. instances of `lola::GenericProxyEvent` and `lola::ProxyEvent<SampleType>`
create such an instance and dispatch the corresponding methods to it. We decided to go for this composite/dispatch
approach instead of introducing a common base class to `lola::GenericProxyEvent` and `lola::ProxyEvent<SampleType>`,
which provides/implements those signatures as this would have meant, that `lola::GenericProxyEvent` and
`lola::ProxyEvent<SampleType>` had multi-inheritance (from this base class and their corresponding interface)!
Even if this hadn't been problematic (in terms of potential diamond pattern), we would have to explicitly argue/analyse
it due to the `ASIL-B` nature of our code base and the general avoidance pattern of multi-inheritance.
To make sure, that the dispatch to `lola::ProxyEventCommon` is free from any overhead, we should take into account
making all the methods of `lola::ProxyEventCommon` inline.

