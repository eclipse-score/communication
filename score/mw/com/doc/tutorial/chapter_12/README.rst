Chapter 12: Heap-free mw::com for ASIL-B and FFI applications
==============================================================


In all previous chapters we used ``score::mw::com`` without thinking about when heap allocations
happen. The middleware creates internal data structures, shared-memory handles, and subscription
records during startup, and that was fine.

In ASIL-B and Freedom-From-Interference (FFI) applications things are different. Once the
application enters its operational loop, heap allocation must stop: uncontrolled allocations can
interfere with pre-allocated resources, introduce unpredictable latency, and violate the
assumptions that safety-qualified software makes about its runtime behaviour. The application
must therefore split its lifecycle into two distinct phases:

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
   :ref:`chapter_12_asil_guidance` at the end of this chapter lists what you need to address
   before using these patterns in production ASIL-B code.

Files/artifacts used
~~~~~~~~~~~~~~~~~~~~~


The ``bazel`` project for this chapter is located in ``score/mw/com/doc/tutorial/chapter_12``
and contains the following files:

----------------------------------------

.. list-table::
   :header-rows: 1

   * - File Name
     - Description
   * - `BUILD <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/BUILD>`__
     - Bazel targets for all four example groups in this chapter.
   * - `common/service_interface.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/common/service_interface.h>`__
     - The single typed service interface (``SensorInterface``) shared by all four example groups.
   * - `heap_check/heap_check.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/heap_check/heap_check.h>`__
     - Header-only heap checker. Replaces ``operator new`` process-globally to abort on any heap allocation after ``forbid_heap()`` is called.
   * - `event_send_receive/provider.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/event_send_receive/provider.cpp>`__
     - Provider (skeleton) for the basic event send/receive example.
   * - `event_send_receive/consumer.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/event_send_receive/consumer.cpp>`__
     - Consumer (proxy) for the basic event send/receive example.
   * - `event_send_receive/mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/event_send_receive/mw_com_config.json>`__
     - Shared ``score::mw::com`` configuration for the event_send_receive pair.
   * - `event_send_receive/logging.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/event_send_receive/logging.json>`__
     - Logging configuration for the event_send_receive pair.
   * - `event_field_update/provider.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/event_field_update/provider.cpp>`__
     - Provider (skeleton) for the combined event and field update example.
   * - `event_field_update/consumer.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/event_field_update/consumer.cpp>`__
     - Consumer (proxy) for the combined event and field update example.
   * - `event_field_update/mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/event_field_update/mw_com_config.json>`__
     - Shared ``score::mw::com`` configuration for the event_field_update pair.
   * - `event_field_update/logging.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/event_field_update/logging.json>`__
     - Logging configuration for the event_field_update pair.
   * - `start_find_service/provider.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/start_find_service/provider.cpp>`__
     - Provider (skeleton) for the async discovery example using ``StartFindService``.
   * - `start_find_service/consumer.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/start_find_service/consumer.cpp>`__
     - Consumer (proxy) for the async discovery example.
   * - `start_find_service/mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/start_find_service/mw_com_config.json>`__
     - Shared ``score::mw::com`` configuration for the start_find_service pair.
   * - `start_find_service/logging.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/start_find_service/logging.json>`__
     - Logging configuration for the start_find_service pair.
   * - `bidirectional_discovery/application_a.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/bidirectional_discovery/application_a.cpp>`__
     - Application A for the bidirectional example. Acts as both provider (skeleton) and consumer (proxy).
   * - `bidirectional_discovery/application_b.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/bidirectional_discovery/application_b.cpp>`__
     - Application B for the bidirectional example. Acts as both provider (skeleton) and consumer (proxy).
   * - `bidirectional_discovery/application_a_mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/bidirectional_discovery/application_a_mw_com_config.json>`__
     - ``score::mw::com`` configuration for application A.
   * - `bidirectional_discovery/application_b_mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/bidirectional_discovery/application_b_mw_com_config.json>`__
     - ``score::mw::com`` configuration for application B.
   * - `bidirectional_discovery/application_a_logging.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/bidirectional_discovery/application_a_logging.json>`__
     - Logging configuration for application A.
   * - `bidirectional_discovery/application_b_logging.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/bidirectional_discovery/application_b_logging.json>`__
     - Logging configuration for application B.


----------------------------------------

The two-phase model
~~~~~~~~~~~~~~~~~~~


The boundary between the init phase and the operational phase is enforced by a header-only
**heap checker** (``heap_check/heap_check.h``). It replaces the process-global ``operator new``
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

   Include ``heap_check.h`` in **exactly one translation unit** per binary (the ``main.cpp``).
   Including it in multiple translation units would produce duplicate ``operator new``
   definitions, which is an ODR violation.

Before the application enters cleanup or shutdown, it must call ``heap_check::allow_heap()``
to re-enable allocation on the calling thread. This lets ``Unsubscribe()``,
``StopOfferService()``, and destructors run normally.

The service interface used in the examples
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


All four example groups share a single typed service interface defined in
``common/service_interface.h``. It provides one event (``reading``, carrying a small
``SensorReading`` struct) and one field (``calibration_status``, an unsigned integer). The
typed API uses ``AsSkeleton<SensorInterface>`` and ``AsProxy<SensorInterface>`` to produce the
concrete skeleton and proxy types, the same pattern that generated code would use in a
production system:

.. literalinclude:: common/service_interface.h
   :language: cpp
   :lines: 23-49
   :caption: common/service_interface.h

Which API calls allocate
~~~~~~~~~~~~~~~~~~~~~~~~


The table below shows which ``score::mw::com`` calls touch the heap and which do not. All
allocating calls must be completed **before** ``heap_check::forbid_heap()`` is called. Calls
marked "heap-free" are safe to use in the operational loop.

+-------------------------------------------+-------------------------+-----------------------------------+
| API call                                  | Heap behaviour          | Basis                             |
+===========================================+=========================+===================================+
| ``SensorSkeleton::Create()``              | Allocates               | Binding setup, shared_ptr chains  |
+-------------------------------------------+-------------------------+-----------------------------------+
| ``sk.calibration_status.Update()``        | Allocates (init only)   | ``make_unique<FieldType>`` before |
| (before ``OfferService()``)               |                         | ``OfferService()``                |
+-------------------------------------------+-------------------------+-----------------------------------+
| ``sk.OfferService()``                     | Allocates               | SHM setup, PrepareOffer checks    |
+-------------------------------------------+-------------------------+-----------------------------------+
| ``sk.reading.Allocate()``                 | **Heap-free**           | Returns ``SampleAllocateePtr<T>`` |
| (after ``OfferService()``)                |                         | from LoLa SHM ring buffer         |
+-------------------------------------------+-------------------------+-----------------------------------+
| ``sk.reading.Send()``                     | **Heap-free**           | Writes directly to SHM            |
+-------------------------------------------+-------------------------+-----------------------------------+
| ``sk.calibration_status.Update()``        | **Heap-free**           | After ``OfferService()``:         |
| (after ``OfferService()``)                |                         | ``UpdateImpl()`` → SHM write      |
+-------------------------------------------+-------------------------+-----------------------------------+
| ``SensorProxy::FindService()``            | Allocates               | ``ServiceHandleContainer``        |
+-------------------------------------------+-------------------------+-----------------------------------+
| ``SensorProxy::StartFindService()``       | Allocates               | ``make_shared<FindServiceHandler>``|
+-------------------------------------------+-------------------------+-----------------------------------+
| ``SensorProxy::StopFindService()``        | May allocate            | Place before ``forbid_heap()``    |
+-------------------------------------------+-------------------------+-----------------------------------+
| ``SensorProxy::Create()``                 | Allocates               | Binding setup, shared_ptr chains  |
+-------------------------------------------+-------------------------+-----------------------------------+
| ``proxy.reading.Subscribe()``             | Allocates               | Binding structures                |
+-------------------------------------------+-------------------------+-----------------------------------+
| ``proxy.reading.SetReceiveHandler()``     | Allocates               | ``make_shared<ScopedEventReceive``|
|                                           |                         | ``Handler>`` on proxy side;       |
|                                           |                         | additionally causes skeleton bg   |
|                                           |                         | thread to allocate on the         |
|                                           |                         | ``RegisterEventNotification``     |
|                                           |                         | path; see note below              |
+-------------------------------------------+-------------------------+-----------------------------------+
| ``proxy.reading.GetNewSamples()``         | **Heap-free**           | SHM ring buffer access            |
+-------------------------------------------+-------------------------+-----------------------------------+
| ``proxy.reading.GetSubscriptionState()``  | **Heap-free**           | Atomic read                       |
+-------------------------------------------+-------------------------+-----------------------------------+
| ``Notification::waitForWithAbort()``      | **Heap-free**           | ``std::unique_lock`` on           |
|                                           |                         | member mutex                      |
+-------------------------------------------+-------------------------+-----------------------------------+
| ``Notification::notify()`` / ``reset()``  | **Heap-free**           | ``std::lock_guard`` on            |
|                                           |                         | member mutex                      |
+-------------------------------------------+-------------------------+-----------------------------------+
| ``stop_source::get_token()``              | **Heap-free**           | Atomic shared_ptr ref-count copy  |
+-------------------------------------------+-------------------------+-----------------------------------+

.. note::

   ``SetReceiveHandler`` cannot be used in a fully heap-free provider-consumer pair today. When
   the proxy registers a receive handler, the LoLa runtime's skeleton-side background thread
   creates a ``std::make_shared<MoveOnlyScopedFunction<...>>`` to record the registration. If
   the provider has already called ``forbid_heap()``, that allocation triggers the abort. A TODO
   for a pre-allocated pool exists in the LoLa source. The heap-free examples therefore use
   ``GetNewSamples()`` polling instead.

Provider application: sending events without heap allocation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


The ``event_send_receive/provider.cpp`` example shows the complete provider lifecycle. During
the **init phase**, the skeleton is created, the field is given an initial value with
``Update()`` (required by the framework before ``OfferService()`` is called), and
``OfferService()`` is called. All of these calls allocate heap memory and finish before
``forbid_heap()``:

.. literalinclude:: event_send_receive/provider.cpp
   :language: cpp
   :lines: 36-54
   :caption: event_send_receive/provider.cpp — init phase

Once ``heap_check::forbid_heap()`` is called, the application enters the **operational phase**.
The provider loop calls ``sk.reading.Allocate()`` on every iteration. This call does not touch
the heap: it acquires a slot from the LoLa shared-memory ring buffer and returns a
``SampleAllocateePtr<SensorReading>``, an RAII handle to that slot. The provider fills the
struct in place and calls ``Send(std::move(sample))`` to publish it. Both calls stay on the
SHM ring buffer; no ``operator new`` is involved:

.. literalinclude:: event_send_receive/provider.cpp
   :language: cpp
   :lines: 52-87
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
   :lines: 46-79
   :caption: event_send_receive/consumer.cpp — init phase (discovery, create, subscribe)

After ``forbid_heap()``, the operational loop calls ``proxy.reading.GetNewSamples()``, which
reads directly from the LoLa SHM ring buffer. The callback lambda receives a
``SamplePtr<const SensorReading>``, a SHM handle that gives zero-copy access to the data the
provider placed there:

.. literalinclude:: event_send_receive/consumer.cpp
   :language: cpp
   :lines: 77-107
   :caption: event_send_receive/consumer.cpp — operational phase and cleanup

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
   :lines: 52-95
   :caption: event_field_update/provider.cpp — operational phase with event and field update

Async service discovery with StartFindService
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


The ``FindService()`` polling pattern used in ``event_send_receive/consumer.cpp`` has a problem
in scenarios where two applications must discover each other at the same time. If application A
polls ``FindService()`` waiting for B to appear, and B polls ``FindService()`` waiting for A to
appear, and neither calls ``OfferService()`` until it has found the other, both applications
block forever. That is a deadlock.

``StartFindService()`` avoids this by separating the search from the wait. The application
calls ``StartFindService()`` immediately (non-blocking), which registers a callback that fires
when the service appears. The main thread then blocks with a **timeout** on a
``Notification``. Because the wait has a deadline, the application cannot deadlock even if the
other side never appears.

The ``start_find_service/consumer.cpp`` example shows this pattern in full.
``StartFindService()`` itself allocates (a ``make_shared<FindServiceHandler>`` internally) and
must be called in the init phase. ``Proxy::Create()`` is called inside the discovery callback,
which may run on a middleware worker thread. The per-thread nature of the heap checker means
the worker thread's flag is always ``false``, so worker threads are never constrained. However,
``forbid_heap()`` on the main thread must not be called until the callback has completed and
the proxy has been created, because ``Create()`` allocates:

.. literalinclude:: start_find_service/consumer.cpp
   :language: cpp
   :lines: 48-94
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

- ``Proxy::Create()`` allocates heap memory and must therefore complete **before**
  ``forbid_heap()`` is called.
- ``Proxy::Create()`` requires the provider to have already called ``OfferService()``, because
  ``OfferService()`` is the call that creates the shared-memory region the proxy attaches to.
  A proxy cannot be created against a service that has not yet been offered.

Each application therefore calls ``OfferService()`` first, making its shared-memory region
available immediately. It then calls ``StartFindService()`` and waits with a bounded timeout
for the other application's service to appear and for ``Proxy::Create()`` to complete inside
the discovery callback. Only after that wait returns with a valid proxy does the application
call ``forbid_heap()``.

A useful side effect of this ordering is that neither application can deadlock: because each
offers its service before waiting, the other application can always proceed with
``Proxy::Create()`` regardless of which one reaches ``OfferService()`` first.

The ``bidirectional_discovery/application_a.cpp`` example shows this sequence:

.. literalinclude:: bidirectional_discovery/application_a.cpp
   :language: cpp
   :lines: 59-120
   :caption: bidirectional_discovery/application_a.cpp — OfferService before StartFindService, then wait

In the operational phase, application A simultaneously sends on its own skeleton event and
polls the remote proxy's event in the same loop, both heap-free:

.. literalinclude:: bidirectional_discovery/application_a.cpp
   :language: cpp
   :lines: 118-157
   :caption: bidirectional_discovery/application_a.cpp — operational loop: send and receive without heap

.. _chapter_12_asil_guidance:

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
(``score/mw/com/doc/tutorial/chapter_12/``) and are built as part of the main workspace. Each
group pairs a provider (skeleton) binary with a consumer (proxy) binary that must run
concurrently. Start the provider first in one terminal and the consumer in a second terminal.
Both binaries exit with code 0 on success. An ``std::abort()`` during the run indicates a
heap allocation in the operational phase.

The four example groups and their pairing:

- ``event_send_receive`` — basic event send/receive. Start ``provider`` first.
- ``event_field_update`` — event and field update together. Start ``provider`` first.
- ``start_find_service`` — async discovery via ``StartFindService``. Start ``provider`` first
  (or simultaneously; the consumer uses a 5-second discovery timeout).
- ``bidirectional_discovery`` — both applications offer and consume simultaneously. Start in
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
  process-globally and aborts on any allocation after ``forbid_heap()``. The check is
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
  the skeleton side.
- **Async service discovery** with ``StartFindService()`` avoids the ``FindService()``
  polling deadlock. The discovery callback may run on a middleware worker thread;
  ``Proxy::Create()`` (which allocates) runs there safely because the per-thread heap check
  does not constrain worker threads. The main thread must **wait for the callback to
  complete** (with a timeout) before calling ``forbid_heap()``.
- **Bidirectional discovery** between two applications that each act as provider and consumer
  requires ``OfferService()`` before ``StartFindService()``. This satisfies both mandatory
  constraints: ``Proxy::Create()`` allocates and must finish before ``forbid_heap()``, and
  ``Proxy::Create()`` can only succeed after the provider has called ``OfferService()``. A
  structural consequence is that both applications can start in any order without deadlock.
- Before calling ``forbid_heap()``: ``SensorSkeleton::Create()``, ``OfferService()``, the
  initial ``Field::Update()``, ``FindService()``/``StartFindService()``,
  ``Proxy::Create()``, ``Subscribe()``.
- After the operational loop: call ``allow_heap()`` unconditionally, then
  ``Unsubscribe()``, ``StopOfferService()``, and let destructors run normally.
- Adapting to ASIL-B production requires formal MISRA deviation records for
  ``heap_check.h``, safety-monitor notification before ``std::abort()``, per-thread
  happens-before analysis, watchdog supervision on all blocking waits, and single-TU
  compilation of the operator new replacement.
