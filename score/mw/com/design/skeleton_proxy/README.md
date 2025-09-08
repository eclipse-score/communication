# Skeleton/Proxy Binding architecture

## Introduction

The following structural view shows, how the separation of generic/binding independent part of a proxy/skeleton from
its flexible/variable technical binding implementation is achieved. **Note**: It does **only** reflect the common use
case of strongly typed proxies. The special case of "generic proxies" is described in
[design extension for generic proxies](generic_proxy/README.md#) to not bloat this class diagram even more:

<a name="classdiagram"></a>

<img alt="SKELETON_BINDING_MODEL" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/design/skeleton_proxy/skeleton_binding_model.puml">

<img alt="PROXY_BINDING_MODEL" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/design/skeleton_proxy/proxy_binding_model.puml">


The overall structure foresees proxies (`DummyProxy`) and skeletons (`DummySkeleton`), which are generated from IDL.
Both inherit from a respective base class, where otherwise redundant code that can be reused by any proxy or skeleton is
shifted to.
A concrete instance of a proxy or skeleton is always bound to a specific technical binding. Nevertheless, we want to have
a clear separation between binding independent code and binding specific code. Thus, the interfaces `SkeletonBinding`
and `ProxyBinding` are introduced, which are implemented binding specific respectively. Based on the provided
`InstanceIdentifier`, the proxy and skeletons need to decide which binding to construct. This decision is encapsulated
within the `.*BindingFactory`.

One central strategy is to **_generate as less code as possible from the IDL_** (arxml), which has then to resort to a
generic C++ code base (of course applying C++ templating mechanisms).

In the case of our shared memory implementation (also called `LoLa`), it is necessary to access an underlying, not yet
further specified shared memory region which is specific to each communication instance. A problem can arise if the same
process includes a skeleton and proxy side of the same service instance. In this case we have to ensure that the shared
memory region is not mapped twice into the process space. In order to do that, the
`memory::shared::MemoryResourceRegistry` will take care to check which paths have already been created and will return
or create the necessary resources if necessary.

The details, if and when a shared memory object created by a communication instance gets cleaned up, can be read in the
[detailed design of partial restart functionality](../partial_restart/README.md#partial-restart-specific-extensions-to-skeletonpreparestopoffer)

### Copy/Move support for binding independent and dependent proxies/skeletons

### Copy

Proxies and skeletons are **not** copyable! Neither on the binding independent nor binding specific level.
This is "by design" as proxy and skeleton instances are heavyweight, and it semantically makes **no sense** to create a
copy of such an instance:

- what would it mean to have a copy of a skeleton instance!? To which instance would a remote proxy talk to then? If the
  user updates an event on the 1st instance, but **not** on the copy, what does this mean for the remote proxy? Which
  update does it see?
- also on the proxy side copies are semantically _problematic_! Proxies and their resource-usage (`max-number-of-samples`
  used in subscribe calls) are reflected in the configuration done by the provider (in `LoLa` binding this regarding
  slot allocation resources in shared-memory). Doing simple copies would completely violate this approach!
  Note, that `LoLa`/`mw::com` allows to explicitly create multiple proxy instances for the same service instance. These
  are **not** copies, but explicitly separate instances, which happen to interact with the same service instance.

### Move

Proxies and skeletons are movable on the binding independent level! This makes completely sense from the user API
perspective! Restricting this/disallowing this on the user facing binding independent level would just lead to a bad
API experience.

However, on the binding dependent level, we **don't** support `move` for proxies and skeletons! The main reasons:

- it isn't needed as a proxy/skeleton instance on binding level is created once and then owned by the binding
  independent proxy/skeleton via unique-ptr (see further discussion of `pImpl` idiom below).
- not supporting it makes our life much easier as we then can store references to binding specific proxy/skeleton
  instances without taking care to update such references after a move.

### Skeleton creation

In order to perform any service discovery related operations on the skeleton side it is necessary to instantiate the
skeleton by the user. This has no restrictions of any kind. For this purpose, the user would create an instance of a
generated skeleton (`DummySkeleton`).

The concrete (technical) binding to be used is defined within the deployment information, which gets evaluated at
runtime! The transition from the binding independent part to binding dependent part is done via the `pImpl` idiom.
So `SkeletonBase`, which is the base class for the (generated) skeleton, contains a pointer to the binding specific
implementation. All (technical) bindings have to implement `SkeletonBinding`.

During construction of `SkeletonBase` the `pImpl` gets initialized from the information contained in the given
`InstanceIdentifier` (or `InstanceSpecifier`, which gets resolved into an `InstanceIdentifier`). The resolution of the
correct binding and the construction of the correct subclass of `SkeletonBinding` is performed by `SkeletonBindingFactory`.

However, the generated skeleton is a composite, potentially containing  (depending on its interface description) a number
of different event (or also field) members. For those composite members the same `pImpl` pattern is applied.
So `SkeletonEvent` is the binding independent class template (template arg is the data type of the event), which holds
the `pImpl`, which points to an abstract class `SkeletonEventBinding`, which has to be implemented by each (technical)
binding. The mechanism to initialize the `pImpl` is conceptually the same as within the `SkeletonBase`.

The following sequence shows the instantiation of a service class up to its service offering based on our `LoLa`
(shared-mem) binding:

<img alt="SKELETON_CREATE_OFFER_SEQ" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/design/skeleton_proxy/skeleton_create_offer_seq.puml">

#### Binding independent level Registration of skeleton events/fields at their parent skeleton

Due to our architectural constraints, the `impl::SkeletonBase` (the base class of the generated skeleton/`DummySkeleton`)
doesn't "know" its event/field children. Because events/fields are the members of the generated skeleton and not the
`impl::SkeletonBase`. But for various functionalities, we require the `impl::SkeletonBase` to have access to its
events/fields.
This is solved by a mechanism, where the event/field members of the generated skeleton class register themselves at
their parent skeleton, which they got by reference during their creation/`ctor` call. So in their `ctor` they are
calling `impl::SkeletonBase::RegisterEvent()` resp. `impl::SkeletonBase::RegisterField()`, with their own reference and
name. So after creation of a generated skeleton, its base class (`impl::SkeletonBase`) has all the event/field members
stored in `protected` members `events_` and `fields_`, so that in the future these could be even made `public`
accessible to the generated (user facing) skeleton. Since these user facing skeletons have discrete event/field
members anyhow, there is currently no need for such a generalized event/field access.

So while we have this relation/accessibility of events/fields from the "parent" skeleton on the binding **independent**
level, we **don't** have a symmetric setup on the binding **dependent** level. The binding specific implementation of
`SkeletonBinding`, where `impl::SkeletonBase` is dispatching to via the `pImpl` idiom doesn't "know" its corresponding
binding specific events and fields, which would be represented **both** as `SkeletonEventBinding` (as our field is a
composite, which dispatches to `impl::SkeletonEventBase`, which then &ndash; also via `pImpl` idiom &ndash; dispatches
to the binding specific `SkeletonEventBinding` [see here](#../todo)).

This symmetry isn't currently needed as we don't have any use case yet, where on the binding dependent level our
`lola::Skeleton` (implementing `SkeletonBinding`) would need to call functionality on its events/fields!
I.e. all our use cases, where a skeleton has to delegate some functionality to its "children" (events/fields), happens
currently solely on the binding independent level.

**_Note_**: If this would need to change in the future, we would only store `SkeletonEventBinding` instances within an
implementation of `SkeletonBinding`, since we do not have (yet) a field specific class on binding level! To detect,
whether we have to deal with a "pure" event or the "event part" of a field, we would resort to checking the related
`ElementFqId`, which contains this differentiation.

#### LoLa binding level Registration of skeleton events/fields at their parent skeleton

Despite the last paragraph in the previous chapter, we are doing still "some registration" in our `LoLa`/shared-memory
binding from `lola::SkeletonEvent` at its parent `lola::Skeleton`!

This registration differs from the registration done on the binding independent layer explained above:

1. Registration does **NOT** take place at construction time of `lola::SkeletonEvent`, but during
   `lola::SkeletonEvent::PrepareOffer`. I.e. in the context of `impl::Skeleton::OfferService()`.
2. During this call to `lola::Skeleton::Register()` the `lola::Skeleton` does **NOT** store any references/links to the
  `lola::SkeletonEvent`, which is doing this call.

Instead, this registration approach via the `lola::Skeleton::Register()` is done, to allow the `lola::SkeletonEvent` to
set up its specific storage in shared memory! Since the `lola::Skeleton` is the owner/creator of the whole shared-memory
object, where all the events/fields/(later also methods) belonging to this `lola::Skeleton` are stored, the access to
the shared-memory object has to be done through the parent `lola::Skeleton` instance. As shown in the 2nd part of the
sequence diagram in [chapter for skeleton creation](#skeleton-creation), the `lola::SkeletonEvent`s are enriching the
event maps prepared by the `lola::Skeleton` in `lola::Skeleton::PrepareOffer()` (and stored within shared-memory) with
their event specific storage. This job has to be done by/shifted to the `lola::SkeletonEvent` as it needs the event/
field type information, which only the `lola::SkeletonEvent` has (not the `lola::Skeleton`!). This is also the reason,
that `lola::Skeleton::Register()` is a method template. The `lola::SkeletonEvent` calls the method template
instantiation with its event/field type.

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

The implementation of the latter one contains some indirection &ndash; again due to our architectural constraints, which
are the same at proxy and skeleton side and have been laid out
[here on the skeleton side](#binding-independent-level-registration-of-skeleton-eventsfields-at-their-parent-skeleton).

The indirection is that for `ProxyBase::AreBindingsValid()` to work, the binding independent `impl::ProxyEvent`s set
the member of its semantic parent `ProxyBase::are_service_element_bindings_valid_` to `false` during their construction,
if they detect, that they don't have a valid underlying `pImpl` member of type `ProxyEventBindingBase` to dispatch to.
This is an indication, that the binding specific implementation of the `ProxyEvent` couldn't be successfully created by
the proxy side factories.

So, the binding independent `impl::ProxyBase` doesn't "know" its semantically aggregated `ProxyEvent`s.
Although the `ProxyEvent`s get a reference to their parent `ProxyBase` during construction (and therefore "know" their
parent in the context of their `ctor`), they don't store this reference (as it isn't really needed currently and
would require additional logic to update this reference if the parent `impl::ProxyBase` gets moved).
Instead, they only set once (see above) the `ProxyBase::are_service_element_bindings_valid_` member variable of their
parent during construction as this is currently the only feedback needed between proxy and its proxy events on binding
independent level.

#### Binding level Registration of proxy events/fields at their parent proxy

On the binding **specific** level things look different!
I.e. the binding specific proxy needs to interact with its dependent/child proxy events of type `ProxyEventBindingBase`
(at least the `LoLa`/shared-memory specific does, so we introduced it on the `ProxyBinding` interface level) .

Therefore, `impl::ProxyBinding` has a method `RegisterEventBinding` (and `UnregisterEventBinding`) and our `LoLa`
binding specific proxy `lola::Proxy`, which implements `impl::ProxyBinding`, has a member `event_bindings_` (a map
containing its child proxy events), which it populates in its implementation of `RegisterEventBinding` with its
dependent proxy events.

The call to `RegisterEventBinding` happens during creation of a binding independent `impl::ProxyEvent` via an `RAII`
style member `event_binding_registration_guard_` of type `EventBindingRegistrationGuard`, which the `impl::ProxyEvent`
owns.
This `EventBindingRegistrationGuard` takes over two functionalities:

- setting `ProxyBase::are_service_element_bindings_valid_` member of its parent ProxyBase instance (see previous chapter)
- calling `RegisterEventBinding` on the underlying `ProxyBinding` of the given `ProxyBase` on construction of the
  `EventBindingRegistrationGuard` and calling `UnregisterEventBinding` on destruction. Since
  `event_binding_registration_guard_` is owned by the `impl::ProxyEvent`, the registration / unregistration is done on
  construction / destruction of an `impl::ProxyEvent`.

So **after** construction of user facing generated proxy class instance (`DummyProxy`), we have the following structure
in place:

1. Only an instance of the generated proxy class gets returned from the call to `<DummyProxy>::Create()` in case its
   binding on proxy level (its `pImpl` target) implementing `ProxyBinding` could be constructed and also for **all** its
   aggregated events/fields their related `ProxyEventBinding`s (`pImpl` targets) could be constructed.
2. The `ProxyBinding` (`ProxyBase::proxy_binding_`) has complete access to all its child events/fields as
   `ProxyBinding::RegisterEventBinding` has been called for all contained events/fields.


#### Extract type agnostic code

The [class diagram](#classdiagram) also shows, that our `LoLa` proxy event binding implementation (`lola::ProxyEvent`)
aggregates an object of type `lola::ProxyEventCommon`, to which it dispatches all its `SampleType` agnostic method
calls, it has to implement to fulfill its interface `ProxyEventBindingBase`.
The reason for this architectural decision is described in the [design extension for generic proxies](./generic_proxy/README.md)

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
