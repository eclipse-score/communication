Chapter 9: Fields - Get() and Set()
===================================


In :doc:`chapter 7 <../chapter_7/README>` we introduced **fields** – events "on steroids" that always carry a
*current value*. There, however, we only used the **notifier** part of a field (tag ``WithNotifier``): the consumer
*subscribed* to the field and received value updates event-driven via ``GetNewSamples()``.

A field can, in addition to (or instead of) its notifier, offer a request/response style **getter** (``Get()``) and
**setter** (``Set()``). These are enabled via the ``WithGetter`` / ``WithSetter`` tags. We deferred them to this chapter
because – semantically – **Get() and Set() on a field are just methods**, and methods were only introduced in
:doc:`chapter 8 <../chapter_8/README>`.

This chapter takes the ``TirePressureService`` of chapter 7 as its basis, renames it to ``TirePressureExtendedService``
(``tire_pressure_extended_service.h`` / ``.cpp``) and extends it to demonstrate ``Get()`` and ``Set()``.

.. warning::

   **Current limitation (please read first):** The provider-side handler that serves a field **getter** is not yet
   auto-registered by the middleware (see the ``TODO: Remove this skip once the field Get handler is auto-registered in
   SkeletonField.`` in ``score/mw/com/impl/bindings/lola/skeleton.cpp``). As a consequence, while the code of this
   chapter **builds and links** cleanly, a provider will currently **abort** as soon as a consumer subscribes to a field
   ``Get()``. ``Set()`` (i.e. ``WithSetter``) already works end-to-end; the ``WithGetter``/``Get()`` path does not yet.
   This chapter is therefore written to be *complete and correct against the public API*, so that it runs as soon as the
   middleware gap is closed. The "How to run the example" section below reflects this.

Get() vs. the notifier - why does a getter exist at all?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


At first glance a getter looks redundant: with ``WithNotifier`` a consumer already receives every field value and can
read the latest one via ``GetNewSamples()``. So why would one want ``Get()``?

The key is to understand what the **notifier** actually *requires*: whenever the provider updates the field value, that
update has to be made **visible** to a subscribed consumer, i.e. the latest value has to end up in some buffer **local
to the subscriber**, because that is exactly where ``GetNewSamples()`` looks. What "local to the subscriber" means,
however, is **binding-specific**:

- **Setup 1 – network binding (separate ECUs):** On every field update the provider has to **send the new value over
  the network** to the subscriber node, where it is stored in a local buffer that ``GetNewSamples()`` then reads.
- **Setup 2 – IPC binding with per-process buffers (same ECU, different processes):** On every field update the
  provider has to **copy the new value into the buffer of the subscriber's process/address space**, from where
  ``GetNewSamples()`` reads it.
- **Setup 3 – IPC binding with shared-memory buffers (same ECU, different processes):** Here provider and subscriber
  **share the very same buffer** for the field samples. There is *nothing extra* for the provider to do after updating
  the value – enabling ``WithNotifier`` is essentially a **no-op** on the provider side.

Now assume the provider updates the field at a **very high rate**. In Setup 1 (and, to a lesser extent, Setup 2)
enabling ``WithNotifier`` then creates real cost: CPU load in both cases and additional network traffic in Setup 1. If a
consumer does **not** need every single update instantly – e.g. because it only uses the latest value in some rarely
scheduled activity – keeping ``WithNotifier`` active would just **waste resources**. Such a consumer would instead
**disable the notifier**, enable **WithGetter** and call ``Get()`` **just in time**, exactly when it needs the current
value.

In Setup 3, on the other hand, where ``WithNotifier`` carries no added cost, a consumer could simply keep the notifier
and always access the latest sample via ``GetNewSamples()``.

From an academic point of view it is somewhat unfortunate when a developer, who implements solely against the (binding
agnostic) public API of ``score::mw::com``, lets **binding-specific behaviour** drive the design of the service
interface. Deciding to use ``WithNotifier`` and *not* ``WithGetter`` by assuming that the application will always run in
a Setup 3 situation is problematic: at least in theory, the person **deploying** the application (who may be different
from the one designing the interface) could change the configuration so that it is *not* a Setup 3 situation anymore.

**Bottom line:** there is no real *semantic* difference between "``WithNotifier`` + read via ``GetNewSamples()``" and
"``WithGetter`` + read via ``Get()``". It is rather a question of **efficiency**, which depends on the field update rate
and the concrete binding being used.

The service interface
~~~~~~~~~~~~~~~~~~~~~~


The interface is based on chapter 7's ``TirePressureInterface``, renamed to ``TirePressureExtendedInterface``. Compared
to chapter 7 the following changed:

- The four per-wheel fields (``tire_pressure_front_left``, ...) are kept, but their tag changed from ``WithNotifier`` to
  **WithGetter**.
- A fifth field ``tire_pressure_thresholds`` was added. Its data type is a ``struct TirePressureThreshold`` with two
  ``float`` members ``lower_threshold`` and ``upper_threshold``. It is declared with **WithGetter and WithSetter**.
- An event ``tire_pressure_warning`` was added, whose data type is an ``enum`` identifying a specific tire.

The event data type is a plain enum with a fixed underlying type (so it stays trivially copyable):

.. literalinclude:: tire_pressure_extended_service.h
   :language: cpp
   :lines: 28-34
   :caption: tire_pressure_extended_service.h


The value type of the new field is a small, trivially copyable struct (a field getter/setter transports the value as a
method argument/return value through shared memory, so it must be trivially copyable – exactly the same constraint we
saw for method arguments in chapter 8):

.. literalinclude:: tire_pressure_extended_service.h
   :language: cpp
   :lines: 39-43
   :caption: tire_pressure_extended_service.h


The interface itself:

.. literalinclude:: tire_pressure_extended_service.h
   :language: cpp
   :lines: 57-82
   :caption: tire_pressure_extended_service.h


Files/artifacts used
~~~~~~~~~~~~~~~~~~~~~


The ``bazel`` project for this chapter is located in ``score/mw/com/doc/tutorial/chapter_9``. It follows the same
structure as the previous chapters, with the service interface renamed to ``tire_pressure_extended_service``:

----------------------------------------

.. list-table::
   :header-rows: 1

   * - File Name
     - Description
   * - `BUILD <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_9/BUILD>`__
     - This file contains bazel targets for this example.
   * - `consumer/consumer.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_9/consumer/consumer.cpp>`__
     - Implementation of the service consumer app. The `main()` for the consumer
   * - `consumer/consumer.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_9/consumer/consumer.h>`__
     - Header (empty - we just always want to have cpp/h pairs) of the service consumer.
   * - `consumer/mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_9/consumer/mw_com_config.json>`__
     - This file contains the configuration for `score::mw::com` for the consumer app.
   * - `consumer/logging.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_9/consumer/logging.json>`__
     - This file contains the configuration for the logging system used by `score::mw::com`
   * - `provider/provider.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_9/provider/provider.cpp>`__
     - Implementation of the service provider.
   * - `provider/provider.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_9/provider/provider.h>`__
     - Header (empty) of the service provider.
   * - `provider/logging.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_9/provider/logging.json>`__
     - This file contains the configuration for the logging system used by `score::mw::com`
   * - `provider/mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_9/provider/mw_com_config.json>`__
     - This file contains the configuration for `score::mw::com` for the provider app.
   * - `tire_pressure_extended_service.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_9/tire_pressure_extended_service.cpp>`__
     - This file is empty as the service interface is completely defined in the header.
   * - `tire_pressure_extended_service.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_9/tire_pressure_extended_service.h>`__
     - This file contains the definition of the service interface (fields + event).


----------------------------------------

Configuration
~~~~~~~~~~~~~


The service type binding lists the five **fields** (each with a ``fieldId``) and the new **event** (with an
``eventId``). Note that a field getter/setter does **not** need its own ``methodId``: internally the get/set methods are
derived from the ``fieldId`` of the field they belong to, so the configuration of a field is unchanged whether or not it
has a getter/setter.

.. literalinclude:: provider/mw_com_config.json
   :language: json
   :lines: 13-40
   :caption: provider/mw_com_config.json


On the **consumer** side there is one important difference to a pure-notifier field (chapter 7): because
``Get()``/``Set()`` are methods, and – just like the regular methods of chapter 8 – a consumer that wants to *call* them
has to list the corresponding fields in its own service instance. The optional ``useGetIfAvailable`` /
``useSetIfAvailable`` flags declare that the consumer intends to use the getter/setter of the field:

.. literalinclude:: consumer/mw_com_config.json
   :language: json
   :lines: 58-80
   :caption: consumer/mw_com_config.json


.. note::

   The ``useGetIfAvailable`` / ``useSetIfAvailable`` flags are the field-accessor counterpart of the method ``use``
   property described in :doc:`chapter 8 <../chapter_8/README>`: they are pure **resource-tweaking** hints, only
   evaluated on the proxy/consumer side, and – just like the method ``use`` property – they **default to ``true``** (the
   getter/setter is enabled by default). Enabling a getter/setter makes the proxy set up the resources for it (for the
   LoLa binding: the shared-memory of the corresponding get/set request/response queue); if a consumer is not going to
   read or write a field via ``Get()``/``Set()`` at all, set the corresponding flag to ``false`` to spare that
   shared-memory. So they share the exact same default polarity as a method's ``use``: enabled by default, *opt out* via
   ``useGetIfAvailable: false`` / ``useSetIfAvailable: false``.

Provider application
~~~~~~~~~~~~~~~~~~~~


The provider is based on chapter 7's provider. The essential additions are:

**1. A set-handler for tire_pressure_thresholds.** Because ``Set()`` on a field is a method, its handler has to be
registered (via ``RegisterSetHandler()``) **before** the service is offered. The handler receives the
consumer-requested value **by (mutable) reference**, validates/clamps it in place and thereby determines the value that
actually becomes the new field value (the middleware updates the field with whatever the handler leaves behind). The
handler clamps ``lower_threshold`` to ``[1.5, 2.5]`` and ``upper_threshold`` to ``[2.5, 3.5]`` bar, logging an error
whenever it has to adjust a value:

.. literalinclude:: provider/provider.cpp
   :language: cpp
   :lines: 133-148
   :caption: provider/provider.cpp


Note that there is **no** explicit "get-handler": the getter of a field always returns the field's **current value**,
so the provider only has to keep the field value up to date via ``Update()`` (exactly as in chapter 7). As with every
field, the provider must supply an **initial value for each field** before offering the service:

.. literalinclude:: provider/provider.cpp
   :language: cpp
   :lines: 153-169
   :caption: provider/provider.cpp


**2. Cyclic updates and warnings.** As in chapter 7 the provider updates the four tire pressures at a randomized
cadence, but now the random pressures are drawn from ``[1.2, 4.0]`` bar. After each update cycle it reads the current
threshold band and, for every tire whose new pressure has **left** the band, sends a ``tire_pressure_warning`` event
carrying the affected tire (several warnings may be sent after a single cycle):

.. literalinclude:: provider/provider.cpp
   :language: cpp
   :lines: 201-234
   :caption: provider/provider.cpp


Consumer application
~~~~~~~~~~~~~~~~~~~~


The consumer is based on chapter 7's consumer, but its reception model is inverted: it does **not** subscribe to the
tire pressure fields anymore (they no longer have a notifier). Instead it:

- subscribes to the ``tire_pressure_warning`` event (with ``max_sample_count`` 4, one per tire) and registers a receive
  handler for it,
- and, whenever a warning arrives, actively reads the values it needs via the field **getters**.

Registering the receive handler and subscribing looks exactly like the event reception of chapter 5 / the field notifier
of chapter 7:

.. literalinclude:: consumer/consumer.cpp
   :language: cpp
   :lines: 242-260
   :caption: consumer/consumer.cpp


The receive handler drains all new warning samples via ``GetNewSamples()`` and reacts to each of them:

.. literalinclude:: consumer/consumer.cpp
   :language: cpp
   :lines: 187-199
   :caption: consumer/consumer.cpp


For each warning the consumer reads – via ``Get()`` – the **current pressure** of the affected tire and the **current
threshold band**, logs both, and then adjusts the threshold band: if the pressure was **below** the lower threshold it
lowers ``lower_threshold`` by ``0.1`` bar; if it was **above** the upper threshold it raises ``upper_threshold`` by
``0.1`` bar. The new band is written back via ``Set()``, whose return value carries the value the provider actually
accepted (the provider may have clamped it):

.. literalinclude:: consumer/consumer.cpp
   :language: cpp
   :lines: 133-183
   :caption: consumer/consumer.cpp


Note that reading a field via ``Get()`` is completely independent of a subscription: ``Get()`` is a request/response
method call, so the consumer never subscribes to the tire pressure fields at all.

How to run the example
~~~~~~~~~~~~~~~~~~~~~~


.. warning::

   As noted at the top of this chapter, the field getter path is not yet functional in the current middleware. The
   targets **build** successfully, but running provider + consumer will currently make the **provider abort** as soon
   as the consumer subscribes to a field ``Get()``. The instructions below are provided for completeness and will work
   unchanged once the middleware auto-registers the field get handler.

The steps are the same as in the previous chapters. The consumer uses the **implicit** configuration loading, so it is
started **without** the ``--service_instance_manifest`` argument.

.. code-block:: bash

   # Build the provider and consumer targets
   bazel build //score/mw/com/doc/tutorial/chapter_9:provider-tar
   bazel build //score/mw/com/doc/tutorial/chapter_9:consumer-tar


Extract both archives (e.g. in a tmp-directory):

.. code-block:: bash

   mkdir -p /tmp/tutorial/chapter_9
   tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_9/provider-tar.tar -C /tmp/tutorial/chapter_9/
   tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_9/consumer-tar.tar -C /tmp/tutorial/chapter_9/


... then start the service-provider application in the 1st terminal:

.. code-block:: bash

   cd /tmp/tutorial/chapter_9/opt/TirePressureExtendedServer
   bin/provider


and the service-consumer in the 2nd terminal:

.. code-block:: bash

   cd /tmp/tutorial/chapter_9/opt/TirePressureExtendedClient
   bin/consumer


Summary
~~~~~~~


A short summary of what we have learned in this chapter:

- A field can, besides (or instead of) its notifier, offer a request/response style **getter** (``Get()``, tag
  ``WithGetter``) and **setter** (``Set()``, tag ``WithSetter``). Semantically, ``Get()``/``Set()`` on a field are just
  **methods** – which is why they are covered only after chapter 8.
- There is **no semantic difference** between "``WithNotifier`` + ``GetNewSamples()``" and "``WithGetter`` + ``Get()``";
  the choice is a matter of **efficiency**, which depends on the field update rate and the concrete binding (network /
  IPC with per-process buffers / IPC with shared memory). A high-rate field that a consumer only samples occasionally is
  a typical case for preferring the getter over the notifier.
- On the **provider** side, a **setter** is served by a handler registered via ``RegisterSetHandler()`` **before**
  ``OfferService()``; the handler validates/adjusts the requested value in place and the middleware then adopts it as
  the new field value. A **getter** needs no handler: it always returns the field's current value, which the provider
  keeps up to date via ``Update()`` (and must initialize before offering, as for any field).
- On the **consumer** side, ``field.Get()`` and ``field.Set(value)`` return a ``score::Result`` wrapping a
  ``MethodReturnTypePtr``, dereferenced to access the value. Reading via ``Get()`` is independent of any subscription.
- A field getter/setter is **not** configured as a separate method: its method id is derived from the field's
  ``fieldId``. However – just like a regular method (chapter 8) – a consumer that wants to call ``Get()``/``Set()`` must
  list the corresponding fields in its **own** service instance configuration (optionally with ``useGetIfAvailable`` /
  ``useSetIfAvailable``).
- **Current limitation:** the middleware does not yet auto-register the provider-side handler that serves a field
  getter, so the ``WithGetter``/``Get()`` path is not runnable end-to-end at the time of writing, even though it
  compiles and links.
