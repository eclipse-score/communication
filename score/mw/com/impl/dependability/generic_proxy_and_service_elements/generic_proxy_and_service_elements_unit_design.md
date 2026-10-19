# Generic Proxy And Service Elements Unit Design

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
* `GenericProxy` class: We will provide a class `GenericProxy` in the `mw::com` namespace.
* This `GenericProxy` class will (in relation to the concept) **only** provide a public method `GetEvents()`, which
  returns a `GenericProxy::EventMapView` (which is a read-only view to a `std::map`)
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

## GenericProxy class

As shown in the class diagram, below, the `mw::com::impl::GenericProxy` is **not** a generated (from some interface
specification) class (see [Sys-Req-13525992](broken_link_c/issue/13525992)), derived from
`impl::ProxyBase` (like `DummyProxy` example) and contains one member `events_`, which is a map of
`impl::GenericProxyEvent` in the form of `mw::com::EventMap`.

<img alt="GENERIC_PROXY_MODEL" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/impl/dependability/generic_proxy_and_service_elements/generic_proxy_model.puml">

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
