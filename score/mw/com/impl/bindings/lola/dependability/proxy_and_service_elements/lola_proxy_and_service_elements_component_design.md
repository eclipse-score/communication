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
