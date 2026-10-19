# LoLa Proxy and Service Elements Unit Design

## Binding level Registration of proxy events/fields at their parent proxy

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

## Proxy auto-reconnect functionality

According to our requirements (namely requirement `SCR-29682823`), we need to support a functionality, which is known as
`proxy auto-reconnect`.
We decided for now to implement this feature at the (`LoLa`) binding specific level on the proxy side. Technically it
would be easy to shift it to the binding independent `impl::Proxy` level as the interfaces being used by the
functionality are already very generic.

The `proxy auto-reconnect` function is solved within our architecture with a couple of mechanisms:

### Automated Start/Stop FindService

Our `lola::Proxy` has been extended with a `FindServiceGuard` member, which applies an `RAII` pattern:

- on construction, it retrieves the `ServiceDiscovery` via `impl::Runtime::getInstance().GetServiceDiscovery()` and
  then starts an asynchronous service search via
  `StartFindService(FindServiceHandler<HandleType>, EnrichedInstanceIdentifier)`.
- the value of `EnrichedInstanceIdentifier` exactly represents the `InstanceIdentifier`, the enclosing `lola::Proxy`
  instance was constructed from. So this search exactly monitors the availability of the remote service instance, the
  `lola::Proxy` instance communicates with.
- on destruction of `FindServiceGuard` member, it stops the **asynchronous** search again via `StopFindService()`.

### Reaction on service becoming unavailable

When the registered `FindServiceHandler` gets called by the `ServiceDiscovery`, because the searched/monitored service
instance has gone, it sets the `is_service_instance_available_` member variable of the `lola::Proxy` instance to `false`
and notifies all child proxy events about the fact, that the service instance is now unavailable.

This again leads to a call to `lola::SubscriptionStateMachine::StopOfferEvent()`, which updates the event instance
specific state machine. The subscription state switches to `SUBSCRIPTION_PENDING_STATE`.

### Reaction on service becoming available (again)

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

### State machine

Event subscription and the (potentially) asynchronous subscription state change callbacks for a `ProxyEvent` instance
are handled by the `lola::SubscriptionStateMachine`.
The "events" , which drive the state machine transitions are:

- `SubscribeEvent()` - Subscribe-call done by the user
- `UnsubscribeEvent()` Unsubscribe-call done by the user (or implicitly by proxy destruction)
- `StopOfferEvent()` - service discovery induced notification about service instance disappearance
- `ReOfferEvent()` - service discovery induced notification about service instance reappearance

These events can potentially happen concurrently. It is the job of the state machine to synchronize correctly and deduce
a valid state.

How the service discovery related notifications get forwarded to the `ProxyEvent`/its `lola::SubscriptionStateMachine`
instance has been described [here](../skeleton_proxy/README.md#proxy-auto-reconnect-functionality).

The structure and transitions of the state machine are shown in:

<img alt="PROXY_EVENT_STATE_MACHINE" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/impl/bindings/lola/dependability/proxy_and_service_elements/proxy_event_state_machine.puml">

The structural model of the state machine design is as follows:

<img alt="PROXY_EVENT_STATE_MACHINE_MODEL" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/impl/bindings/lola/dependability/proxy_and_service_elements/proxy_event_state_machine_model.puml">

### User access to the subscription state

The user can access the subscription state of a `ProxyEvent` or `ProxyField` instance via method `GetSubscriptionState()`.
This method dispatches to the binding (`ProxyEventBindingBase::GetSubscriptionState()`), which in the case of the `LoLa`
binding dispatches to `lola::SubscriptionStateMachine` (method `GetCurrentState()`) shown above.

Additionally a user can register a callback, which gets called as soon as the subscription state changes. This is done
via method `ProxyEvent::SetSubscriptionStateChangeHandler`. This method dispatches to the binding
(`ProxyEventBindingBase::SetSubscriptionStateChangeHandler()`), which in the case of the `LoLa` binding dispatches to
`lola::SubscriptionStateMachine` (method `SetSubscriptionStateChangeHandler()`) shown above. The
`lola::SubscriptionStateMachine` stores the user provided callback and calls it as soon as the subscription state changes.

The call to the user-provided handler happens under the same lock, which protects the state change and the state change
notification. Thus, there are two important implications:
1. The user provided handler gets called synchronously within the state change call. This means, that the state change
   call only returns after the user provided handler has returned.
2. Since the user provided handler gets called under the lock, it must not do any call, which could lead to a deadlock. E.g.
   it must not call `ProxyEvent::Unsubscribe` or `ProxyEvent::Subscribe` again, as these calls also acquire the same
   lock.

The implications/recommendations are given in the public API description of the `SubscriptionStateChangeHandler`.
We explicitly allow, that the user unsets/unregisters the `SubscriptionStateChangeHandler` from within the handler itself.
To avoid any potential deadlock, we do **not** allow to unset the `SubscriptionStateChangeHandler` via the
`UnsetSubscriptionStateChangeHandler()`! Instead the `SubscriptionStateChangeHandler` signature is designed to control
the unsetting via its return value. I.e. if the handler returns `false`, it will be automatically unset after it has been
called. This avoids introduction of recursive mutexes/locking on the existing mutex of the state machine, which would
add a lot of complexity and potential for deadlocks. The solution via the return code is very lightweight: After the state
machine called the handler, and before it releases the lock, it checks the return value. If it is `false`, it just resets
&ndash; still under state-machine mutex lock &ndash; the internal `std::optional`, which holds the handler.


## GenericProxy

### Architecture enhancements for GenericProxy support

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

#### Meta-Info

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

#### initialization of `lola::EventMetaInfo`

The map containing the meta-information gets initialized by the skeleton instance together with the setup/creation of
the vectors containing the sample slots in
```
template <typename SampleType>
std::pair<EventDataStorage<SampleType>*, EventDataControlComposite<>> lola::Skeleton::Register(ElementFqId element_fq_id,
   std::size_t number_of_slots)
```

This happens during initial offering-phase of a service instance, before potential consumers/proxies are able to access
the shared-memory object.

### GenericProxy class

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
