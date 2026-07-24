Chapter 4: Subscription state
=============================


In the previous chapters the consumer simply subscribed to the `message` event and then started polling for new samples.
It silently assumed, that the subscription was – and stayed – successful. In reality the **subscription
state** of an event is dynamic: it changes over time, e.g. when the provider (temporarily) stops offering the service.

In this chapter we look at the subscription state of an event and the **two ways** a consumer can deal with it:

1. **Polling** the current subscription state via `GetSubscriptionState()` (typically directly after `Subscribe()`).
2. **Registering** a subscription-state-change handler via `SetSubscriptionStateChangeHandler()` and reacting to state
   changes as they happen.

The `SubscriptionState` (from `score/mw/com/types.h`) is an enum with three values:

.. list-table::
   :header-rows: 1

   * - Value
     - Meaning
   * - `kNotSubscribed`
     - Not subscribed (e.g. `Subscribe()` was not (yet) called or `Unsubscribe()` was called).
   * - `kSubscribed`
     - Subscribed and the provider currently offers the service – samples can flow.
   * - `kSubscriptionPending`
     - Subscribed, but the provider currently does **not** offer the service. The subscription stays "armed" and will automatically become `kSubscribed` again once the provider offers the service again.


To be able to **show both variations in a single sample application**, the consumer creates **two** proxy instances for
the **same** found service instance:

- **proxy_1** uses the **polling** variation.
- **proxy_2** uses the **state-change handler** variation.

.. note::

   **Note:** Creating **two proxies in the same application for the very same (provided) service instance** is a rather
   "pathological" use case. But `score::mw::com` supports it, and we deliberately take advantage of this here to keep the
   sample minimal: it frees us from writing a second consumer application just to show the second variation. In a real
   application you would normally have exactly **one** proxy per service instance per consumer.

To make the subscription state actually change at runtime, the provider in this chapter periodically **stops offering**
the service (via `StopOfferService()`) and, after a short pause, **offers it again** (via `OfferService()`).

Files/artifacts used
~~~~~~~~~~~~~~~~~~~~


The `bazel` project for this chapter is located in `score/mw/com/doc/tutorial/chapter_4`. It contains the same set of
files as chapter 3 (just adapted in content):

----------------------------------------

.. list-table::
   :header-rows: 1

   * - File Name
     - Description
   * - `BUILD <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_4/BUILD>`__
     - This file contains bazel targets for this example.
   * - `consumer/consumer.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_4/consumer/consumer.cpp>`__
     - Implementation of the service consumer app. The `main()` for the consumer
   * - `consumer/consumer.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_4/consumer/consumer.h>`__
     - Header (empty - we just always want to have cpp/h pairs) of the service consumer.
   * - `consumer/mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_4/consumer/mw_com_config.json>`__
     - This file contains the configuration for `score::mw::com` for the consumer app.
   * - `consumer/logging.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_4/consumer/logging.json>`__
     - This file contains the configuration for the logging system used by `score::mw::com`
   * - `provider/provider.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_4/provider/provider.cpp>`__
     - Implementation of the service provider.
   * - `provider/provider.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_4/provider/provider.h>`__
     - Header (empty) of the service provider.
   * - `provider/logging.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_4/provider/logging.json>`__
     - This file contains the configuration for the logging system used by `score::mw::com`
   * - `provider/mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_4/provider/mw_com_config.json>`__
     - This file contains the configuration for `score::mw::com` for the provider app.
   * - `hello_world_service.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_4/hello_world_service.cpp>`__
     - This file is empty as the service interface is completely defined in the header.
   * - `hello_world_service.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_4/hello_world_service.h>`__
     - This file contains the definition of the service interface.


----------------------------------------

The service interface (`hello_world_service.h`) is **unchanged** compared to chapters 2 and 3. All changes in this
chapter are in the provider/consumer application code and in the configuration files.

How to run the example
~~~~~~~~~~~~~~~~~~~~~~


The steps are the same as in chapter 3. As in chapter 3, the consumer uses the **implicit** configuration loading, so it
is started **without** the `--service_instance_manifest` argument.

.. code-block:: bash

   # Build the provider and consumer targets
   bazel build //score/mw/com/doc/tutorial/chapter_4:provider-tar
   bazel build //score/mw/com/doc/tutorial/chapter_4:consumer-tar


Extract both archives (e.g. in a tmp-directory):

.. code-block:: bash

   mkdir -p /tmp/tutorial/chapter_4
   tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_4/provider-tar.tar -C /tmp/tutorial/chapter_4/
   tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_4/consumer-tar.tar -C /tmp/tutorial/chapter_4/


... then start the service-provider application in the 1st terminal:

.. code-block:: bash

   cd /tmp/tutorial/chapter_4/opt/HelloWorldServer
   bin/provider


and the service-consumer in the 2nd terminal:

.. code-block:: bash

   cd /tmp/tutorial/chapter_4/opt/HelloWorldClient
   bin/consumer


When both run, you will observe how the provider alternates between offering and stopping to offer the service:

.. code-block:: text

   OfferService: HelloWorld service instance is now offered.
   ...
   StopOfferService: HelloWorld service instance is no longer offered. Pausing event updates.
   ...
   OfferService: HelloWorld service instance is offered again. Resuming event updates.


... and how this is reflected on the consumer side in the subscription state of both proxies (see below for the details):

.. code-block:: text

   [proxy_1 poll] Subscription state directly after Subscribe(): kSubscribed
   [proxy_1 poll] State is kSubscribed -> will start receiving samples cyclically.
   [proxy_2 handler] Subscription state changed to: kSubscribed
   [proxy_1 poll] Sample received: Hello World #3
   [proxy_2 handler] Sample received: Hello World #3
   ...
   [proxy_2 handler] Subscription state changed to: kSubscriptionPending
   [proxy_1 poll] Sample received: Hello World #15
   [proxy_2 handler] State is not kSubscribed -> skipping GetNewSamples().
   ...
   [proxy_2 handler] Subscription state changed to: kSubscribed
   [proxy_2 handler] Sample received: Hello World #15
   ...


Provider application
~~~~~~~~~~~~~~~~~~~~


Compared to chapter 3, the provider in this chapter offers only a **single** service instance again (as in chapters 1
and 2). The interesting new aspect is that it periodically toggles between offering and not offering the service.

Two durations control the cycle:

.. literalinclude:: provider/provider.cpp
   :language: cpp
   :lines: 32-33
   :caption: provider/provider.cpp


In its main loop, the provider keeps track of whether it currently offers the service (`offered`) and when the next
phase change is due. On each phase change it either stops or (re-)starts offering:

.. literalinclude:: provider/provider.cpp
   :language: cpp
   :lines: 107-121
   :caption: provider/provider.cpp


Event updates (`SendSample()`) are only published while the service is currently offered:

.. literalinclude:: provider/provider.cpp
   :language: cpp
   :lines: 132-133
   :caption: provider/provider.cpp


Note: `StopOfferService()` does **not** invalidate the consumer's proxy or its subscription. The subscription stays
"armed"; only its state changes to `kSubscriptionPending`. When the provider offers the service again, the state
automatically goes back to `kSubscribed`. This functionality/behavior is called "proxy auto reconnect" and there is a
specific chapter in the design documentation that explains it in detail
(see `here <https://github.com/eclipse-score/communication/blob/main/score/mw/com/design/skeleton_proxy/README.md#proxy-auto-reconnect-functionality>`__).

Consumer application
~~~~~~~~~~~~~~~~~~~~


The consumer uses `StartFindService()` for asynchronous discovery (as introduced in chapter 3), but this time it does an
**explicit** instance search: the `InstanceSpecifier` "MyHelloWorldServiceInstance" is mapped in the consumer
configuration **with** an explicit `instanceId` (1). Hence, the search matches exactly the one instance the provider
offers – not a "find-any" search.

Once the instance is found, the find-service handler creates the **two proxies** for the same handle. All state shared
between the (asynchronously called) find-service handler and the main loop is bundled into a single `SharedState` struct,
so that the handler lambda only needs to capture a single pointer (the `FindServiceHandler` callback has a limited
capture capacity):

.. literalinclude:: consumer/consumer.cpp
   :language: cpp
   :lines: 113-138
   :caption: consumer/consumer.cpp


Since we only ever act on the **first** matching instance, we can **stop the search directly from within the
find-service handler**, as soon as we are called. This also nicely demonstrates that a service-instance search may be
stopped from within a `StartFindService()` callback (the middleware explicitly supports this – internally it uses a
recursive lock so that `StopFindService()` may be called from the handler).

Note that we do **not** need to guard against an empty `handles` container here: the **first** invocation of a
find-service handler is guaranteed to carry at least one matching handle. An empty set is only ever reported *later on*,
when a previously found instance vanishes – but since we stop the discovery right away, this handler is only ever
called once, with a non-empty set:

.. literalinclude:: consumer/consumer.cpp
   :language: cpp
   :lines: 180-180
   :caption: consumer/consumer.cpp


If setting up the proxies (creating, subscribing, registering the handler) unexpectedly fails, the handler does **not**
just log and return: such errors are practically non-recoverable and would otherwise leave the main loop spinning
forever without any proxies. Instead, it records a `fatal_error`, which the main loop observes and reacts to by
terminating:

.. literalinclude:: consumer/consumer.cpp
   :language: cpp
   :lines: 189-192
   :caption: consumer/consumer.cpp


.. literalinclude:: consumer/consumer.cpp
   :language: cpp
   :lines: 279-282
   :caption: consumer/consumer.cpp


Variation 1: proxy_1 – polling the subscription state
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


After subscribing, proxy_1 polls the subscription state **exactly once** via `GetSubscriptionState()`. Only if the
`Subscribe()` succeeded **and** the state is already `kSubscribed`, it enables cyclic reception:

.. literalinclude:: consumer/consumer.cpp
   :language: cpp
   :lines: 198-214
   :caption: consumer/consumer.cpp


From then on, the main loop keeps calling `GetNewSamples()` on proxy_1 **unconditionally**, regardless of the later
subscription state:

.. literalinclude:: consumer/consumer.cpp
   :language: cpp
   :lines: 297-299
   :caption: consumer/consumer.cpp


This nicely shows that a proxy **may** call `GetNewSamples()` even in state `kSubscriptionPending` (as long as it was
`kSubscribed` at least once): during the provider's stop-offer phases proxy_1 simply does not get new data, but the call
itself remains valid.

Variation 2: proxy_2 – a subscription-state-change handler
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


For proxy_2 we register a subscription-state-change handler **before** subscribing, so that we observe all state
transitions. The handler is invoked on an internal middleware thread; it must be short and must **not** call any method
on the same event instance (this could deadlock). Have a look at the documentation of
`SubscriptionStateChangeHandler <https://github.com/eclipse-score/communication/blob/main/score/mw/com/impl/subscription_state_change_handler.h>`__ for its details. Therefore, it merely
records the new state in an atomic and returns `true` to stay registered:

.. literalinclude:: consumer/consumer.cpp
   :language: cpp
   :lines: 237-252
   :caption: consumer/consumer.cpp


The main loop then calls `GetNewSamples()` on proxy_2 **only while** the last observed state was `kSubscribed`. When the
provider stops offering, the handler reports `kSubscriptionPending` and proxy_2 stops calling `GetNewSamples()` until the
state returns to `kSubscribed`:

.. literalinclude:: consumer/consumer.cpp
   :language: cpp
   :lines: 305-312
   :caption: consumer/consumer.cpp


Configuration
~~~~~~~~~~~~~


The provider configuration (`provider/mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_4/provider/mw_com_config.json>`__) offers a single
instance again, mapped to the `instanceSpecifier` "MyHelloWorldServiceInstance" with `instanceId` 1. The `message` event
allows up to 3 subscribers (`maxSubscribers`), which is important here: the single consumer creates **two** proxies for
the same instance, i.e. it produces **two** subscriptions on the same event.

The consumer configuration (`consumer/mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_4/consumer/mw_com_config.json>`__) maps the
`instanceSpecifier` "MyHelloWorldServiceInstance" to the `HelloWorldService` type **with** an explicit `instanceId` (1).
This is what makes the `StartFindService()` search an **explicit** instance search (as opposed to the "find-any" search
of chapter 3, which omitted the `instanceId`).

Summary
~~~~~~~


A short summary of what we have learned in this chapter:

- Every event has a dynamic **subscription state** (`kNotSubscribed`, `kSubscribed`, `kSubscriptionPending`), which a
  consumer can either **poll** via `GetSubscriptionState()` or observe via a handler registered with
  `SetSubscriptionStateChangeHandler()`.
- The subscription-state-change handler runs on an internal middleware thread. It must be short and must **not** call any
  method on the same event instance (deadlock risk). Returning `true` keeps it registered, returning `false` unregisters
  it.
- A proxy **may** call `GetNewSamples()` even in state `kSubscriptionPending` (once it has been `kSubscribed` at least
  once). It simply will not receive new data while the provider does not offer the service.
- A `StopOfferService()` on the provider side makes the subscribed events of consumers transition to
  `kSubscriptionPending`.
- A renewed `OfferService()` on the provider side makes those events transition back to `kSubscribed` – the
  subscription is automatically re-established, without the consumer having to re-subscribe.
- `score::mw::com` even supports creating **multiple proxies for the very same service instance within one application**.
  This is unusual in practice, but handy here to demonstrate both subscription-state strategies in a single sample app.
- A service-instance search started with `StartFindService()` may be **stopped from within its own callback** (via
  `StopFindService()`) – e.g. as soon as the (first) instance of interest has been found. The middleware supports
  this explicitly (it uses a recursive lock internally).
- The **first** invocation of a find-service handler always carries at least one matching handle. An empty container is
  only ever reported on *subsequent* invocations, when a previously found instance vanishes. So when you stop the search
  on the first invocation, you do not need to guard against an empty container.
