Chapter 7: Fields - events on steroids
======================================


Up to now our service interface only exposed an *event* (the "message" event). Starting with this chapter we use a
different service interface that exposes **fields** instead: the `TirePressureService`.

A **field** can be thought of as an *event on steroids*. In addition to the pure event semantics (notification of new
values via a notifier, exactly like an event) a field always carries a **current value**. This has two immediate
consequences that we will observe in this chapter:

- On the **provider** side: a field always has a value, therefore the provider has to supply an **initial value for every
  field** *before* it may offer the service. Omitting the initial value of any field makes `OfferService()` fail.
- On the **consumer** side: because a field always carries a current value, a consumer that subscribes to a field is
  guaranteed to receive that current value right after the subscription switches to `kSubscribed` – **even if the
  provider never updates the field afterwards**. For an event this is different: there a consumer only ever receives
  samples that are sent *after* it has subscribed.

Besides its notifier, a field can also offer a request/response style getter (`Get()`) and setter (`Set()`). Those are
enabled via the `WithGetter` / `WithSetter` tags and are the topic of a later chapter. In this chapter we only use the
**notifier** part of a field (tag `WithNotifier`), so that we can focus on the initial-value semantics and reuse the
event-driven reception we already know from chapter 5.

The service interface
~~~~~~~~~~~~~~~~~~~~~


Instead of `hello_world_service.h` / `.cpp` this chapter introduces `tire_pressure_service.h` / `.cpp`. The
`TirePressureInterface` contains **four fields** of type `float` (the tire pressure in bar of each of the four wheels).
Each field is declared with the `WithNotifier` tag only:

.. literalinclude:: tire_pressure_service.h
   :language: cpp
   :lines: 30-48
   :caption: tire_pressure_service.h


A field takes its data type plus a pack of tags from `{WithGetter, WithSetter, WithNotifier}`. At least one of
`WithGetter` or `WithNotifier` must be present, otherwise the value would be invisible to consumers. Here we use
`WithNotifier`, which enables the notifier part of the proxy-side field API (`Subscribe()`, `GetNewSamples()`,
`SetReceiveHandler()`, `GetSubscriptionState()`, ...) – i.e. exactly the same subset we already used for events.

Files/artifacts used
~~~~~~~~~~~~~~~~~~~~


The `bazel` project for this chapter is located in `score/mw/com/doc/tutorial/chapter_7`. It contains the same set of
files as chapter 5, with the service interface renamed to `tire_pressure_service`:

----------------------------------------

.. list-table::
   :header-rows: 1

   * - File Name
     - Description
   * - `BUILD <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_7/BUILD>`__
     - This file contains bazel targets for this example.
   * - `consumer/consumer.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_7/consumer/consumer.cpp>`__
     - Implementation of the service consumer app. The `main()` for the consumer
   * - `consumer/consumer.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_7/consumer/consumer.h>`__
     - Header (empty - we just always want to have cpp/h pairs) of the service consumer.
   * - `consumer/mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_7/consumer/mw_com_config.json>`__
     - This file contains the configuration for `score::mw::com` for the consumer app.
   * - `consumer/logging.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_7/consumer/logging.json>`__
     - This file contains the configuration for the logging system used by `score::mw::com`
   * - `provider/provider.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_7/provider/provider.cpp>`__
     - Implementation of the service provider.
   * - `provider/provider.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_7/provider/provider.h>`__
     - Header (empty) of the service provider.
   * - `provider/logging.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_7/provider/logging.json>`__
     - This file contains the configuration for the logging system used by `score::mw::com`
   * - `provider/mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_7/provider/mw_com_config.json>`__
     - This file contains the configuration for `score::mw::com` for the provider app.
   * - `tire_pressure_service.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_7/tire_pressure_service.cpp>`__
     - This file is empty as the service interface is completely defined in the header.
   * - `tire_pressure_service.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_7/tire_pressure_service.h>`__
     - This file contains the definition of the service interface (four fields).


----------------------------------------

How to run the example
~~~~~~~~~~~~~~~~~~~~~~


The steps are the same as in chapter 5. The consumer uses the **implicit** configuration loading, so it is started
**without** the `--service_instance_manifest` argument.

.. code-block:: bash

   # Build the provider and consumer targets
   bazel build //score/mw/com/doc/tutorial/chapter_7:provider-tar
   bazel build //score/mw/com/doc/tutorial/chapter_7:consumer-tar


Extract both archives (e.g. in a tmp-directory):

.. code-block:: bash

   mkdir -p /tmp/tutorial/chapter_7
   tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_7/provider-tar.tar -C /tmp/tutorial/chapter_7/
   tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_7/consumer-tar.tar -C /tmp/tutorial/chapter_7/


... then start the service-provider application in the 1st terminal:

.. code-block:: bash

   cd /tmp/tutorial/chapter_7/opt/TirePressureServer
   bin/provider_app


and the service-consumer in the 2nd terminal:

.. code-block:: bash

   cd /tmp/tutorial/chapter_7/opt/TirePressureClient
   bin/consumer_app


The provider first sets the **initial value** (`2.0` bar) of each field, offers the service and then waits 20 s
before it starts updating the fields at randomized intervals:

.. code-block:: text

   Setting initial tire pressures (before offering the service):
     tire_pressure_front_left = 2 bar
     tire_pressure_front_right = 2 bar
     tire_pressure_rear_left = 2 bar
     tire_pressure_rear_right = 2 bar
   OfferService: TirePressure service instance is now offered.
   Waiting 20 s before updating the fields ...
   Next tire pressure update in 170 ms.
   Updating tire pressures:
     tire_pressure_front_left = 2.27283 bar
     ...


The consumer subscribes to all four fields and – thanks to the field's initial-value semantics – receives
the **initial value** (`2` bar) of each field **event-driven** right after the subscription became active, i.e. even
during the 20 s in which the provider does not update anything. Afterwards it receives the randomized updates:

.. code-block:: text

   Found TirePressure service instance (instance id: { ... "instanceId": 1 ... }). Creating proxy.
   Subscribed to field 'tire_pressure_front_left'.
   Subscribed to field 'tire_pressure_front_right'.
   Subscribed to field 'tire_pressure_rear_left'.
   Subscribed to field 'tire_pressure_rear_right'.
   Waiting for event-driven field updates ...
   Field 'tire_pressure_front_left' update received (event-driven): 2 bar
   Field 'tire_pressure_front_right' update received (event-driven): 2 bar
   Field 'tire_pressure_rear_left' update received (event-driven): 2 bar
   Field 'tire_pressure_rear_right' update received (event-driven): 2 bar
   Field 'tire_pressure_front_left' update received (event-driven): 2.27283 bar
   ...


Provider application
~~~~~~~~~~~~~~~~~~~~


The provider creates a skeleton of type `AsSkeleton<TirePressureInterface>` and offers it. The essential difference to
the previous chapters (which offered an event) is that **before** the provider may offer the service, it has to supply an
**initial value for each field** via `Field::Update(const FieldType&)`. It is important to use exactly this overload of
`Update()` (the one taking the value by const-reference):

.. literalinclude:: provider/provider.cpp
   :language: cpp
   :lines: 95-95
   :caption: provider/provider.cpp


Under the hood, `Update()` behaves slightly differently depending on the offer state: as long as the service has not yet
been offered, `Update()` just **stores** the value as the field's initial value; the value is then actually published
(in case of `LoLa` binding into shared memory) the moment `OfferService()` sets everything up. After the service is
offered, `Update()` publishes the new value right away – exactly like sending an event sample.

After offering, the provider waits 20 s (to give a consumer time to observe the initial values) and then updates the
four fields in a loop. Both the **cadence** (a random delay between 100 ms and 3000 ms) and the **values** (a
random pressure between `2.0` and `2.5` bar) are randomized:

.. literalinclude:: provider/provider.cpp
   :language: cpp
   :lines: 105-110
   :caption: provider/provider.cpp


Consumer application
~~~~~~~~~~~~~~~~~~~~


The consumer finds the `TirePressureService` instance via `StartFindService()` – exactly the same pattern as in
chapter 5: it creates the proxy from the found handle and stops the discovery right away from within the find-service
handler. It then, for **each** of the four fields, registers an `EventReceiveHandler` via `SetReceiveHandler()`
**before** subscribing. Because all four fields have identical type
(`ProxyField<float, WithNotifier>`), the set-up is factored into a small helper:

.. literalinclude:: consumer/consumer.cpp
   :language: cpp
   :lines: 100-129
   :caption: consumer/consumer.cpp


.. literalinclude:: consumer/consumer.cpp
   :language: cpp
   :lines: 183-187
   :caption: consumer/consumer.cpp


The important observation is: it is **expected** that each field-receive handler gets called (and `GetNewSamples()` finds
at least one new sample) **right after** the subscription became active – because of the **initial-value semantics**
of a field. A field, in contrast to an event, always has a current value; therefore the receive handler fires after a
successful subscription and switch to `kSubscribed`, delivering that current (here: the initial `2.0` bar) value. Only
afterwards, whenever the provider updates a field, the corresponding handler fires again.

As in chapter 5, the consumer is **purely event-driven**: there is no polling loop. The main thread just blocks on a
condition variable until a termination signal requests the shut down, and any unrecoverable set-up error terminates the
application immediately via `std::exit(EXIT_FAILURE)`.

.. note::

   **A note on the receive handler's capture size:** `EventReceiveHandler` is a `score::cpp::callback<void(void)>` with a
   fixed inline storage (32 bytes). We therefore capture the field by pointer (`&field`, 8 bytes) and the field name as a
   string literal (`const char*`, 8 bytes) instead of, say, a `std::string` (which alone would already exceed the
   available storage).

Configuration
~~~~~~~~~~~~~


The configuration now describes **fields** instead of events. In the service type binding each field gets a `fieldName`
and a unique `fieldId`; in the service instance each field gets its `numberOfSampleSlots` and `maxSubscribers`
(exactly analogous to how events were configured before). The relevant part of the provider
(`provider/mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_7/provider/mw_com_config.json>`__) looks like this:

.. literalinclude:: provider/mw_com_config.json
   :language: json
   :lines: 9-32
   :caption: provider/mw_com_config.json


The consumer (`consumer/mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_7/consumer/mw_com_config.json>`__) maps its `instanceSpecifier`
`MyTirePressureServiceInstance` to that service type **with** the explicit `instanceId` 1, so that the
`StartFindService()` call finds exactly the one instance the provider offers.

Summary
~~~~~~~


A short summary of what we have learned in this chapter:

- A **field** is like an *event on steroids*: it has the same notifier semantics as an event, but in addition always
  carries a **current value**. On the proxy side its notifier API (`Subscribe()`, `GetNewSamples()`,
  `SetReceiveHandler()`, ...) is identical to that of an event.
- A field is declared via `Trait::template Field<DataType, Tags...>` with tags from `{WithGetter, WithSetter,
  WithNotifier}`. At least one of `WithGetter` / `WithNotifier` must be present. In this chapter we used `WithNotifier`.
- The provider must supply an **initial value for every field** via `Field::Update(const FieldType&)` **before** calling
  `OfferService()`; otherwise offering the service fails.
- Because a field always carries a current value, a consumer that subscribes to a field is guaranteed to receive that
  current value **right after the subscription switched to `kSubscribed`** – even if the provider never updates the
  field afterwards. This is the essential difference to an event, and it is why the field-receive handlers in this
  chapter fire once directly after subscription (delivering the initial value) and again on every provider update.
