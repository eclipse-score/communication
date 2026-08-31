# Skeleton/Proxy Binding architecture

## Introduction

The following structural view shows, how the separation of generic/binding independent part of a proxy/skeleton from
its flexible/variable technical binding implementation is achieved. **Note**: It does **only** reflect the common use
case of strongly typed proxies and skeletons. The special case of "generic proxies" and "generic skeletons" are described in
[design extension for generic proxies](generic_proxy/README.md#) and [design extension for generic skeletons](generic_skeleton/README.md#) to not bloat this class diagram even more:

<a name="classdiagram"></a>

<img alt="SKELETON_BINDING_MODEL" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/design/skeleton_proxy/skeleton_binding_model.puml">

<img alt="PROXY_BINDING_MODEL" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/design/skeleton_proxy/proxy_binding_model.puml">

### Skeleton creation


The following sequence shows the instantiation of a service class up to its service offering based on our `LoLa`
(shared-mem) binding:

<img alt="SKELETON_CREATE_OFFER_SEQ" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/design/skeleton_proxy/skeleton_create_offer_seq.puml">

#### Binding independent level Registration of skeleton events/fields/methods at their parent skeleton




### Proxy creation
The mechanism regarding binding abstraction resp. runtime binding selection on proxy side is almost identical to the
skeleton side. A small difference is, that the starting point of a proxy instance creation is a handle (`HandleType`).
This handle is returned by the service discovery and includes (depending on the technical binding) maybe even more
information than the InstanceIdentifier used as starting point on the skeleton side. So here the analog to the
`SkeletonBase`,`SkeletonBinding`and `SkeletonBindingFactory` are `ProxyBase`, `ProxyBinding`/`ProxyBindingFactory`.

So the user creates instances of generated proxy classes (referred to as `DummyProxy` in our class diagrams)
(see [Introduction](#introduction)) via static `<DummyProxy>::Create(HandleType)`. These instances inherit from
`ProxyBase`, which follows the same architectural `pImpl` paradigm as the skeleton side by dispatching to a binding
specific implementation of `ProxyBinding`.

`ProxyBase`/`ProxyBinding` do not contain any public/user facing instance methods beside the rather "technical"
`GetHandle()` method, simply because `ara::com` does not currently define functionality/APIs on proxy instance level.
On `ProxyBase` level we have two types of methods:

- public static/class methods for finding/stop finding service instances. These static methods dispatch to a binding
  specific implementation of service discovery (which gets retrieved via `Runtime::getInstance().GetServiceDiscovery()`)
- internal/implementation specific method `AreBindingsValid()`, which is a method being called **after** construction of
  a generated proxy instance, to detect, whether the instance can be successfully returned from
  `static Result<generated proxy class> <generated proxy class>::Create()` or not.

Similarly to the skeleton side, the `impl::ProxyBase` doesn't "know" its event/field/method children. Therefore,
the `impl::ProxyEventBase` registers itself with the `impl::ProxyBase` in the same way as `impl::SkeletonEventBase`
(as explained
[here on the skeleton side](#binding-independent-level-registration-of-skeleton-eventsfields-at-their-parent-skeleton). 
It will then call `impl::ProxyBase::GetConstructionResult()` and will return a valid proxy to the user if none of the 
bindings received an error from the binding factory when trying to construct the binding.

#### Binding level Registration of proxy events/fields at their parent proxy

On the binding **specific** level things look similar:
the binding specific proxy needs to interact with its dependent/child proxy events of type `ProxyEventBindingBase`
(at least the `LoLa`/shared-memory specific does, so we introduced it on the `ProxyBinding` interface level) .

Therefore, when each `lola::ProxyEvent` gets created (as a member of the generated proxy class), it registers itself 
at its parent `lola::Proxy` via `lola::Proxy::RegisterEvent()`. The `lola::Proxy` stores the reference to each 
child `lola::ProxyEvent`s in a map which it can later use.

So **after** construction of user facing generated proxy class instance (`DummyProxy`), we have the following structure
in place:

1. Only an instance of the generated proxy class gets returned from the call to `<DummyProxy>::Create()` in case its
   binding on proxy level (its `pImpl` target) implementing `ProxyBinding` could be constructed and also for **all** its
   aggregated events/fields their related `ProxyEventBinding`s (`pImpl` targets) could be constructed. If this is not 
   the case, then an error will be returned from `<DummyProxy>::Create()`.
2. The `ProxyBinding` (`ProxyBase::proxy_binding_`) has complete access to all its child events/fields as
   `lola::Proxy::RegisterEvent()` has been called for all contained events/fields.


#### Extract type agnostic code

The [class diagram](#classdiagram) also shows, that our `LoLa` proxy event binding implementation (`lola::ProxyEvent`)
aggregates an object of type `lola::ProxyEventCommon`, to which it dispatches all its `SampleType` agnostic method
calls, it has to implement to fulfill its interface `ProxyEventBindingBase`.
The reason for this architectural decision is described in the [design extension for generic proxies](./generic_proxy/README.md)

Similarly, on the skeleton side, `lola::SkeletonEvent` aggregates an object of type `lola::SkeletonEventCommon`. This class encapsulates all `SampleTyp`e agnostic logic (such as interaction with `lola::Skeleton` for offering services, timestamp management, and notification handling). This allows both strongly typed skeletons and generic skeletons to share the same core implementation logic.

### Proxy auto-reconnect functionality

According to our requirements (namely requirement `SCR-29682823`), we need to support a functionality, which is known as
`proxy auto-reconnect`.
We decided for now to implement this feature at the (`LoLa`) binding specific level on the proxy side. Technically it
would be easy to shift it to the binding independent `impl::Proxy` level as the interfaces being used by the
functionality are already very generic.

The `proxy auto-reconnect` function is solved within our architecture with a couple of mechanisms:

#### Automated Start/Stop FindService

Our `lola::Proxy` has been extended with a `FindServiceGuard` member, which applies an `RAII` pattern:

- on construction, it retrieves the `ServiceDiscovery` via `impl::Runtime::getInstance().GetServiceDiscovery()` and
  then starts an asynchronous service search via
  `StartFindService(FindServiceHandler<HandleType>, EnrichedInstanceIdentifier)`.
- the value of `EnrichedInstanceIdentifier` exactly represents the `InstanceIdentifier`, the enclosing `lola::Proxy`
  instance was constructed from. So this search exactly monitors the availability of the remote service instance, the
  `lola::Proxy` instance communicates with.
- on destruction of `FindServiceGuard` member, it stops the **asynchronous** search again via `StopFindService()`.

#### Reaction on service becoming unavailable

When the registered `FindServiceHandler` gets called by the `ServiceDiscovery`, because the searched/monitored service
instance has gone, it sets the `is_service_instance_available_` member variable of the `lola::Proxy` instance to `false`
and notifies all child proxy events about the fact, that the service instance is now unavailable.

This again leads to a call to `lola::SubscriptionStateMachine::StopOfferEvent()`, which updates the event instance
specific state machine. The subscription state switches to `SUBSCRIPTION_PENDING_STATE`.

#### Reaction on service becoming available (again)

When the registered `FindServiceHandler` gets called by the `ServiceDiscovery`, because the searched/monitored service
instance has shown up again, it sets the `is_service_instance_available_` member variable of the `lola::Proxy` instance
to `true` and notifies all child proxy events about the fact, that the service instance is now available.

This again leads to a call to `lola::SubscriptionStateMachine::ReOfferEvent()`, which does the following:

- it updates the state-machine with the current/new `PID` of the restarted provider
- it (re) subscribes to the (remote) event with the same sample-count then before.
- it re-registers an eventually previously registered `EventUpdateNotificationHandler` via
  `lola::Runtime::GetLolaMessaging().ReregisterEventNotification()`. Since the `EventUpdateNotificationHandler`s are
  move-only functions, this specific function expects, that the handler is already existing/registered within the `LoLa`
  messaging subsystem and only the remote `LoLa` node of the service provider has to be triggered to notify on updates.
- it transitions to `SUBSCRIBED_STATE`
