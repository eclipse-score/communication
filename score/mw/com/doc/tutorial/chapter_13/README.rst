Chapter 13: Heap-free mw::com for ASIL-B and FFI applications
==============================================================


In all previous chapters we used ``score::mw::com`` without thinking about when heap allocations
happen. The middleware creates internal data structures, shared-memory handles, and subscription
records during startup, and that was fine.

In ASIL-B and Freedom-From-Interference (FFI) applications things are different. Once the
application enters its operational loop, heap allocation can be prohibited depending on the
system's safety requirements: uncontrolled allocations can interfere with pre-allocated
resources, introduce unpredictable latency, and violate the assumptions that safety-qualified
software makes about its runtime behaviour. When that prohibition applies, the application
must split its lifecycle into two distinct phases:

- An **init phase** (before the operational loop) in which all allocating calls are made:
  creating the skeleton or proxy, calling ``OfferService()``, subscribing to events, and
  setting receive handlers. Heap allocation is expected here.
- An **operational phase** in which no heap allocation may occur. All data movement goes
  through the LoLa shared-memory ring buffers, which are pre-allocated during init and accessed
  without touching the heap.

This chapter walks through working examples that show this two-phase pattern for the most common
``score::mw::com`` use cases.

.. note::

   The examples run at ASIL-B configuration (``asil-level: "B"`` in every config file) but are
   not themselves ASIL-qualified artifacts. They are blueprints. The section
   :ref:`chapter_13_asil_guidance` at the end of this chapter lists what you need to address
   before using these patterns in production ASIL-B code.

Files/artifacts used
~~~~~~~~~~~~~~~~~~~~~


The ``bazel`` project for this chapter is located in
`score/mw/com/doc/tutorial/chapter_13 <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_13/>`__
and contains the following files:

----------------------------------------

.. list-table::
   :header-rows: 1

   * - File Name
     - Description
   * - `BUILD <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_13/BUILD>`__
     - Bazel targets for all four example groups in this chapter.
   * - `common/tire_pressure_service.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_13/common/tire_pressure_service.h>`__
     - The typed service interface (``TirePressureInterface``) shared by all four example groups. Based on chapter 7's TirePressureService, extended with one event.
   * - `heap_check/heap_check.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_13/heap_check/heap_check.h>`__
     - Header-only heap checker. Replaces ``operator new`` per-thread to abort on any heap allocation after ``forbid_heap()`` is called on that thread.
   * - `event_send_receive/provider.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_13/event_send_receive/provider.cpp>`__
     - Provider (skeleton) for the basic event send/receive example.
   * - `event_send_receive/consumer.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_13/event_send_receive/consumer.cpp>`__
     - Consumer (proxy) for the basic event send/receive example. Uses ``GetNewSamples`` polling.
   * - `event_send_receive/consumer_receive_handler.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_13/event_send_receive/consumer_receive_handler.cpp>`__
     - Consumer variant using ``SetReceiveHandler`` and ``waitWithAbort``. Not heap-free end-to-end; shown for API contrast.
   * - `event_send_receive/mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_13/event_send_receive/mw_com_config.json>`__
     - Shared ``score::mw::com`` configuration for the event_send_receive group.
   * - `event_send_receive/logging.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_13/event_send_receive/logging.json>`__
     - Logging configuration for the event_send_receive group.
   * - `event_field_update/provider.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_13/event_field_update/provider.cpp>`__
     - Provider (skeleton) for the combined event and field update example.
   * - `event_field_update/consumer.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_13/event_field_update/consumer.cpp>`__
     - Consumer (proxy) for the combined event and field update example.
   * - `event_field_update/mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_13/event_field_update/mw_com_config.json>`__
     - Shared ``score::mw::com`` configuration for the event_field_update group.
   * - `event_field_update/logging.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_13/event_field_update/logging.json>`__
     - Logging configuration for the event_field_update group.
   * - `start_find_service/provider.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_13/start_find_service/provider.cpp>`__
     - Provider (skeleton) for the async discovery example using ``StartFindService``.
   * - `start_find_service/consumer.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_13/start_find_service/consumer.cpp>`__
     - Consumer (proxy) for the async discovery example.
   * - `start_find_service/mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_13/start_find_service/mw_com_config.json>`__
     - Shared ``score::mw::com`` configuration for the start_find_service group.
   * - `start_find_service/logging.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_13/start_find_service/logging.json>`__
     - Logging configuration for the start_find_service group.
   * - `bidirectional_discovery/application_a.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_13/bidirectional_discovery/application_a.cpp>`__
     - Application A for the bidirectional example. Acts as both provider (skeleton) and consumer (proxy).
   * - `bidirectional_discovery/application_b.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_13/bidirectional_discovery/application_b.cpp>`__
     - Application B for the bidirectional example. Acts as both provider (skeleton) and consumer (proxy).
   * - `bidirectional_discovery/application_a_mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_13/bidirectional_discovery/application_a_mw_com_config.json>`__
     - ``score::mw::com`` configuration for application A.
   * - `bidirectional_discovery/application_b_mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_13/bidirectional_discovery/application_b_mw_com_config.json>`__
     - ``score::mw::com`` configuration for application B.
   * - `bidirectional_discovery/application_a_logging.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_13/bidirectional_discovery/application_a_logging.json>`__
     - Logging configuration for application A.
   * - `bidirectional_discovery/application_b_logging.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_13/bidirectional_discovery/application_b_logging.json>`__
     - Logging configuration for application B.


----------------------------------------

The two-phase model
~~~~~~~~~~~~~~~~~~~


The boundary between the init phase and the operational phase is enforced by a header-only
**heap checker** (``heap_check/heap_check.h``). It replaces the per-thread ``operator new``
with a version that calls ``std::abort()`` if called after the application has entered the
operational phase. The check is **per-thread**: only the thread that called ``forbid_heap()``
is constrained. Library background threads, including the LoLa worker threads, are never
affected.

.. literalinclude:: heap_check/heap_check.h
   :language: cpp
   :lines: 25-46
   :caption: heap_check/heap_check.h — phase flag and control functions

The replacement ``operator new`` itself reads the thread-local flag and calls ``std::abort()``
on any allocation after ``forbid_heap()`` has been called on that thread:

.. literalinclude:: heap_check/heap_check.h
   :language: cpp
   :lines: 48-61
   :caption: heap_check/heap_check.h — operator new replacement

.. note::

   Include ``heap_check.h`` in **exactly one translation unit** per binary (the translation unit that defines main()).
   Including it in multiple translation units would produce duplicate ``operator new``
   definitions, which is an ODR violation.

Before the application enters cleanup or shutdown, it must call ``heap_check::allow_heap()``
to re-enable allocation on the calling thread. This lets ``Unsubscribe()``,
``StopOfferService()``, and destructors run normally.

The service interface used in the examples
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


All four example groups share a single typed service interface defined in
``common/tire_pressure_service.h``. It is based on the ``TirePressureService`` introduced in
chapter 7, extended with one event (``tire_pressure_update``) to demonstrate heap-free event
send and receive alongside heap-free field updates. The four per-wheel pressure fields from
chapter 7 are retained with the ``WithNotifier`` tag, which enables ``Subscribe()`` and
``GetNewSamples()`` on the proxy side. The examples use ``tire_pressure_front_left`` as the
representative field for field-update demonstrations.

The typed API uses ``AsSkeleton<TirePressureInterface>`` and ``AsProxy<TirePressureInterface>``
to produce the concrete skeleton and proxy types, the same pattern that generated code would use
in a production system:

.. literalinclude:: common/tire_pressure_service.h
   :language: cpp
   :lines: 25-59
   :caption: common/tire_pressure_service.h

Which API calls allocate
~~~~~~~~~~~~~~~~~~~~~~~~


The table below shows which ``score::mw::com`` calls touch the heap and which do not. The
table aggregates API usage across all example binaries in this chapter. All allocating calls
must be completed **before** ``heap_check::forbid_heap()`` is called. Calls marked
"heap-free" are safe to use in the operational loop.

+----------------------------------------------------+----------------------------+
| API call                                           | Heap behaviour             |
+====================================================+============================+
| ``TirePressureSkeleton::Create()``                 | Allocates                  |
+----------------------------------------------------+----------------------------+
| ``skeleton.tire_pressure_front_left.Update()``     | Allocates (init only)      |
| (init phase)                                       |                            |
+----------------------------------------------------+----------------------------+
| ``skeleton.OfferService()``                        | Allocates                  |
+----------------------------------------------------+----------------------------+
| ``skeleton.tire_pressure_update.Allocate()``       | **Heap-free**              |
+----------------------------------------------------+----------------------------+
| ``skeleton.tire_pressure_update.Send()``           | **Heap-free** (see note)   |
+----------------------------------------------------+----------------------------+
| ``skeleton.tire_pressure_front_left.Update()``     | **Heap-free**              |
| (operational phase)                                |                            |
+----------------------------------------------------+----------------------------+
| ``TirePressureProxy::FindService()``               | Allocates                  |
+----------------------------------------------------+----------------------------+
| ``TirePressureProxy::StartFindService()``          | Allocates                  |
+----------------------------------------------------+----------------------------+
| ``TirePressureProxy::StopFindService()``           | May allocate               |
+----------------------------------------------------+----------------------------+
| ``TirePressureProxy::Create()``                    | Allocates                  |
+----------------------------------------------------+----------------------------+
| ``proxy.tire_pressure_update.Subscribe()``         | Allocates                  |
+----------------------------------------------------+----------------------------+
| ``proxy.tire_pressure_update.SetReceiveHandler()`` | Allocates                  |
+----------------------------------------------------+----------------------------+
| ``proxy.tire_pressure_update.GetNewSamples()``     | **Heap-free**              |
+----------------------------------------------------+----------------------------+
| ``proxy.tire_pressure_update``                     | **Heap-free**              |
| ``.GetSubscriptionState()``                        |                            |
+----------------------------------------------------+----------------------------+
| ``Notification::waitForWithAbort()``               | **Heap-free**              |
+----------------------------------------------------+----------------------------+
| ``Notification::waitWithAbort()``                  | **Heap-free**              |
+----------------------------------------------------+----------------------------+
| ``Notification::notify()`` / ``reset()``           | **Heap-free**              |
+----------------------------------------------------+----------------------------+
| ``stop_source::get_token()``                       | **Heap-free**              |
+----------------------------------------------------+----------------------------+

.. note::

   ``skeleton.tire_pressure_update.Send()`` is heap-free in normal operation. When tracing is
   enabled, the runtime allocates a temporary ``std::string`` inside
   ``TracingRuntime::ConvertToTracingServiceInstanceElement()`` for an ``unordered_map`` lookup.
   This is a known limitation tracked in an internal ticket. If tracing must be active during
   the heap-free operational phase, treat ``Send()`` as allocating.

.. note::

   ``SetReceiveHandler`` cannot be used in a fully heap-free provider-consumer pair today. On
   the consumer side, calling ``SetReceiveHandler`` creates a
   ``std::make_shared<ScopedEventReceiveHandler>`` once in the init phase, which is correctly
   placed before ``forbid_heap()``. The problem is on the provider side: when the consumer
   sends its registration message at runtime, the LoLa messaging layer inserts the consumer's
   node identifier into an STL container that uses a default allocator
   (``message_passing_service_instance.cpp``, marked ``// TODO: PMR``). If the provider has
   already called ``forbid_heap()``, that insertion triggers the abort. The heap-free examples
   therefore use ``GetNewSamples()`` polling instead. See the async receive handler section
   below for the full API demonstration.

Provider application: sending events without heap allocation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


The ``event_send_receive/provider.cpp`` example shows the complete provider lifecycle. During
the **init phase**, the skeleton is created, the field is given an initial value with
``Update()`` (required by the framework before ``OfferService()`` is called), and
``OfferService()`` is called. All of these calls allocate heap memory and finish before
``forbid_heap()``:

.. literalinclude:: event_send_receive/provider.cpp
   :language: cpp
   :lines: 39-63
   :caption: event_send_receive/provider.cpp — init phase

Once ``heap_check::forbid_heap()`` is called, the application enters the **operational phase**.
The provider loop calls ``skeleton.tire_pressure_update.Allocate()`` on every iteration. This
call does not touch the heap: it acquires a slot from the LoLa shared-memory ring buffer and
returns a ``SampleAllocateePtr<float>``, an RAII handle to that slot. The provider fills the
value in place and calls ``Send(std::move(sample))`` to publish it. Both calls stay on the
SHM ring buffer; no ``operator new`` is involved:

.. literalinclude:: event_send_receive/provider.cpp
   :language: cpp
   :lines: 69-101
   :caption: event_send_receive/provider.cpp — operational phase and cleanup

Note the ``heap_check::allow_heap()`` call on each early-return error path inside the
operational loop, and again unconditionally before the cleanup calls at the end. This
re-enables allocation on the calling thread so that ``StopOfferService()`` and the skeleton
destructor can run normally.

Consumer application: receiving events without heap allocation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


The ``event_send_receive/consumer.cpp`` example shows the corresponding consumer. The **init
phase** polls ``FindService()`` until the provider's service appears, creates the proxy, and
subscribes to the event. All of these calls allocate and finish before ``forbid_heap()``:

.. literalinclude:: event_send_receive/consumer.cpp
   :language: cpp
   :lines: 49-84
   :caption: event_send_receive/consumer.cpp — init phase (discovery, create, subscribe)

After ``forbid_heap()``, the operational loop calls
``proxy.tire_pressure_update.GetNewSamples()``, which reads directly from the LoLa SHM ring
buffer. The callback lambda receives a ``SamplePtr<float>``, a SHM handle that gives
zero-copy access to the data the provider placed there:

.. literalinclude:: event_send_receive/consumer.cpp
   :language: cpp
   :lines: 82-109
   :caption: event_send_receive/consumer.cpp — operational phase and cleanup

Async receive handler (not heap-free)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


The ``event_send_receive/consumer_receive_handler.cpp`` binary shows the async receive API:
``SetReceiveHandler`` registers a callback that fires on the middleware thread whenever a new
sample arrives, and ``waitWithAbort`` blocks the application thread until the callback signals
it via a ``Notification``.

This binary is **not heap-free end-to-end**. ``SetReceiveHandler`` allocates on the proxy
side (``make_shared<ScopedEventReceiveHandler>``) and also causes the skeleton background
thread to allocate when it registers the notification. The binary therefore never calls
``forbid_heap()``. It is included here to make the full async API available as a reference.
For a heap-free consumer, use ``GetNewSamples()`` polling as shown in
``event_send_receive/consumer.cpp``.

.. literalinclude:: event_send_receive/consumer_receive_handler.cpp
   :language: cpp
   :lines: 82-117
   :caption: event_send_receive/consumer_receive_handler.cpp — handler registration and receive loop

Fields in the operational phase
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


A ``SkeletonField`` requires a call to ``Update()`` with an initial value **before**
``OfferService()`` is called. The framework's ``PrepareOffer()`` checks for this initial value
and returns an error if it is absent. This init-phase ``Update()`` internally stores the value
via ``std::make_unique<FieldType>``, which is heap allocation, correctly placed before
``forbid_heap()``.

After ``OfferService()`` the picture changes: ``Update()`` routes through the event channel
directly to the LoLa shared-memory binding. From that point on, calling ``Update()`` in the
operational loop does not allocate. The ``event_field_update/provider.cpp`` example shows both
an event send and a field update in the same heap-free loop:

.. literalinclude:: event_field_update/provider.cpp
   :language: cpp
   :lines: 69-109
   :caption: event_field_update/provider.cpp — operational phase with event and field update

Async service discovery with StartFindService
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


The ``start_find_service`` example shows how ``StartFindService()`` fits into the heap-free
model. For the general API, the deadlock-avoidance rationale, and ``StopFindService()``, see
:doc:`chapter 3 <../chapter_3/README>`.

The allocation-relevant rules for this chapter are:

- ``StartFindService()`` itself allocates (a ``make_shared<FindServiceHandler>`` internally)
  and must be called in the init phase.
- ``Proxy::Create()`` is called inside the discovery callback, which runs on a middleware
  worker thread. The per-thread heap checker does not constrain worker threads, so
  ``Create()`` may allocate freely there.
- The main thread must wait for the callback to complete before calling ``forbid_heap()``,
  because ``Create()`` allocates heap memory that must be in place before the operational
  phase begins.

The ``start_find_service/consumer.cpp`` example shows this:

.. literalinclude:: start_find_service/consumer.cpp
   :language: cpp
   :lines: 51-98
   :caption: start_find_service/consumer.cpp — StartFindService with bounded wait before forbid_heap()

The key rule is: **wait for the discovery callback to complete before calling**
``forbid_heap()``. Here ``proxy_ready.waitForWithAbort(kDiscoveryTimeout, ...)`` provides
both the wait and the timeout. Only after that call returns with a valid proxy does the
example call ``forbid_heap()`` and enter the operational loop.

Bidirectional discovery
~~~~~~~~~~~~~~~~~~~~~~~


The ``bidirectional_discovery`` example takes this one step further: both applications act as
a provider and a consumer at the same time. Each must offer its own service **and** create a
proxy against the other's service, all before entering the heap-free operational phase.

Two constraints together determine the required init-phase ordering:

- ``OfferService()`` registers the service in the service-discovery registry. A side that has
  not called ``OfferService()`` is not visible to service discovery, so ``FindService()`` and
  the ``StartFindService()`` callback cannot find it and ``Proxy::Create()`` cannot proceed.
  ``OfferService()`` also creates the shared-memory region that the proxy attaches to.
- ``Proxy::Create()`` allocates heap memory and must therefore complete **before**
  ``forbid_heap()`` is called.

Each application therefore calls ``OfferService()`` first, making its service visible in the
service-discovery registry immediately. It then calls ``StartFindService()`` and waits with a
bounded timeout for the other application's service to appear and for ``Proxy::Create()`` to
complete inside the discovery callback. Only after that wait returns with a valid proxy does
the application call ``forbid_heap()``.

A structural consequence of this ordering is that neither application can deadlock: because
each offers its service before waiting, the other application can always proceed with
``Proxy::Create()`` regardless of which one reaches ``OfferService()`` first.

The ``bidirectional_discovery/application_a.cpp`` example shows this sequence:

.. literalinclude:: bidirectional_discovery/application_a.cpp
   :language: cpp
   :lines: 59-141
   :caption: bidirectional_discovery/application_a.cpp — OfferService before StartFindService, then wait

In the operational phase, application A simultaneously sends on its own skeleton event and
polls the remote proxy's event in the same loop, both heap-free:

.. literalinclude:: bidirectional_discovery/application_a.cpp
   :language: cpp
   :lines: 143-187
   :caption: bidirectional_discovery/application_a.cpp — operational loop: send and receive without heap

.. _chapter_13_asil_guidance:

ASIL-B and FFI guidance
~~~~~~~~~~~~~~~~~~~~~~~


The examples are correctly structured blueprints for ASIL-B production code. Before copying
these patterns into a safety-qualified context, however, there are several things you still
need to take care of:

- **MISRA deviations.** ``heap_check.h`` requires four project-level deviation records: a
  mutable ``thread_local`` variable, ``std::malloc``/``std::free`` inside the operator new
  replacement, ``throw std::bad_alloc``, and ``std::abort()`` as the fail-fast reaction
  (covered by the project's pre-existing deviation for Rule 18-5-2). Register these formally
  before use.
- **Safety monitor notification.** The ``std::abort()`` in ``operator new`` terminates the
  process immediately. In a production ASIL-B system, the application should notify the safety
  monitor or FMEA-defined error handler before (or instead of) aborting.
- **Multi-threaded phase boundaries.** The thread-local flag does not need synchronisation
  between threads. The examples are correct because only the main thread calls
  ``forbid_heap()``. If your code has multiple application threads each managing their own
  phase boundary, analyse the happens-before relationship for each thread separately.
- **Cleanup paths.** After ``allow_heap()`` is called, all cleanup code runs with heap
  permitted. If your shutdown re-enters a heap-free state, bracket each re-entry explicitly
  with ``forbid_heap()``/``allow_heap()`` pairs.
- **Watchdog supervision.** The ``waitForWithAbort()`` call in the discovery phase already
  carries a 5-second application-level timeout. Any blocking call in an ASIL-B operational
  loop needs an equivalent deadline; relying on ``std::stop_token`` cancellation alone is not
  enough for watchdog compliance.
- **Single-TU compilation.** The header-only inclusion pattern is convenient for examples but
  risks ODR violations if included in more than one translation unit. In production code,
  compile the operator new replacements into a single dedicated translation unit and treat it
  as a separately audited object.

How to run the examples
~~~~~~~~~~~~~~~~~~~~~~~


The example source files live inside this chapter directory
(``score/mw/com/doc/tutorial/chapter_13/``) and are built as part of the main workspace. Each
group pairs a provider (skeleton) binary with a consumer (proxy) binary that must run
concurrently. Start the provider first in one terminal and the consumer in a second terminal.
Both binaries exit with code 0 on success. An ``std::abort()`` during the run indicates a
heap allocation in the operational phase.

Build the tar archives and extract them into a temporary directory. The
``event_send_receive`` group is shown below as a representative example:

.. code-block:: bash

   bazel build //score/mw/com/doc/tutorial/chapter_13:event_send_receive_provider-tar
   bazel build //score/mw/com/doc/tutorial/chapter_13:event_send_receive_consumer-tar

   mkdir -p /tmp/tutorial/chapter_13
   tar -xf bazel-bin/score/mw/com/doc/tutorial/chapter_13/event_send_receive_provider-tar.tar -C /tmp/tutorial/chapter_13/
   tar -xf bazel-bin/score/mw/com/doc/tutorial/chapter_13/event_send_receive_consumer-tar.tar -C /tmp/tutorial/chapter_13/

Then start the provider in the first terminal:

.. code-block:: bash

   cd /tmp/tutorial/chapter_13/opt/HeapFreeEventSendReceiveProvider
   bin/event_send_receive_provider

And the consumer in the second terminal:

.. code-block:: bash

   cd /tmp/tutorial/chapter_13/opt/HeapFreeEventSendReceiveConsumer
   bin/event_send_receive_consumer

.. note::

   The location from which you start each binary matters. ``score::mw::com`` resolves its
   configuration at ``./etc/mw_com_config.json`` relative to the working directory. The tar
   archive places the configuration at ``opt/<AppName>/etc/mw_com_config.json``, so run each
   binary from its ``opt/<AppName>`` directory as shown above.

The table below lists the tar target, extracted directory, and binary name for each example in
this chapter. Replace the names in the commands above to run a different group.

.. list-table::
   :header-rows: 1

   * - Example
     - Tar target
     - Extracted directory
     - Binary
   * - ``event_send_receive`` provider
     - ``:event_send_receive_provider-tar``
     - ``opt/HeapFreeEventSendReceiveProvider``
     - ``bin/event_send_receive_provider``
   * - ``event_send_receive`` consumer
     - ``:event_send_receive_consumer-tar``
     - ``opt/HeapFreeEventSendReceiveConsumer``
     - ``bin/event_send_receive_consumer``
   * - ``event_field_update`` provider
     - ``:event_field_update_provider-tar``
     - ``opt/HeapFreeEventFieldUpdateProvider``
     - ``bin/event_field_update_provider``
   * - ``event_field_update`` consumer
     - ``:event_field_update_consumer-tar``
     - ``opt/HeapFreeEventFieldUpdateConsumer``
     - ``bin/event_field_update_consumer``
   * - ``start_find_service`` provider
     - ``:start_find_service_provider-tar``
     - ``opt/HeapFreeStartFindServiceProvider``
     - ``bin/start_find_service_provider``
   * - ``start_find_service`` consumer
     - ``:start_find_service_consumer-tar``
     - ``opt/HeapFreeStartFindServiceConsumer``
     - ``bin/start_find_service_consumer``
   * - ``bidirectional_discovery`` application A
     - ``:bidirectional_discovery_application_a-tar``
     - ``opt/HeapFreeBidirectionalDiscoveryA``
     - ``bin/bidirectional_discovery_application_a``
   * - ``bidirectional_discovery`` application B
     - ``:bidirectional_discovery_application_b-tar``
     - ``opt/HeapFreeBidirectionalDiscoveryB``
     - ``bin/bidirectional_discovery_application_b``

The four example groups and their pairing:

- ``event_send_receive``: basic event send and receive. Start the provider first. The
  ``consumer_receive_handler`` variant demonstrates the async receive-handler API but is not
  heap-free.
- ``event_field_update``: event and field update together. Start the provider first.
- ``start_find_service``: async discovery via ``StartFindService``. Start the provider first
  (or simultaneously; the consumer uses a 5-second discovery timeout).
- ``bidirectional_discovery``: both applications offer and consume simultaneously. Start in
  any order.

Before re-running, clear stale shared-memory objects:

.. code-block:: bash

   rm -f /dev/shm/lola-*

Summary
~~~~~~~


A short summary of what we have learned in this chapter:

- ASIL-B and FFI operational code must be **heap-free** after initialisation. Use the
  two-phase model: all allocating API calls go in the **init phase**, before
  ``heap_check::forbid_heap()``. The operational loop runs with heap forbidden and uses only
  shared-memory paths.
- The **heap checker** (``heap_check/heap_check.h``) replaces ``operator new``
  per-thread and aborts on any allocation after ``forbid_heap()``. The check is
  **per-thread**, so library background threads are unaffected.
- **Provider events** use ``Allocate()`` + ``Send(std::move(sample))`` in the operational
  loop. ``Allocate()`` returns a ``SampleAllocateePtr<T>`` backed by the LoLa SHM ring
  buffer, with no heap involved.
- **Provider fields** require an initial ``Update()`` call before ``OfferService()``
  (allocates). After ``OfferService()``, ``Update()`` writes directly to SHM and is
  heap-free.
- **Consumer events** use ``Subscribe()`` in the init phase (allocates) and
  ``GetNewSamples()`` in the operational loop (heap-free, SHM access).
  ``SetReceiveHandler()`` is not heap-free end-to-end today due to a framework limitation on
  the skeleton side. See ``consumer_receive_handler.cpp`` for the async receive API.
- **Async service discovery** with ``StartFindService()`` (see also chapter 3) must be called
  in the init phase. The discovery callback runs on a middleware worker thread and may
  allocate freely. The main thread must **wait for the callback to complete** (with a
  timeout) before calling ``forbid_heap()``.
- **Bidirectional discovery** between two applications requires ``OfferService()`` before
  ``StartFindService()``. This ensures both sides are visible in service discovery so each
  can create a proxy against the other, with all allocations completing before
  ``forbid_heap()``.
- Before calling ``forbid_heap()``: ``TirePressureSkeleton::Create()``, ``OfferService()``,
  the initial ``Field::Update()``, ``FindService()``/``StartFindService()``,
  ``Proxy::Create()``, ``Subscribe()``.
- After the operational loop: call ``allow_heap()`` unconditionally, then
  ``Unsubscribe()``, ``StopOfferService()``, and let destructors run normally.
- Adapting to ASIL-B production requires formal MISRA deviation records for
  ``heap_check.h``, safety-monitor notification before ``std::abort()``, per-thread
  happens-before analysis, watchdog supervision on all blocking waits, and single-TU
  compilation of the operator new replacement.
