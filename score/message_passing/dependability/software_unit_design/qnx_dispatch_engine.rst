QNX Dispatch Engine Unit Design
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Scope and Responsibility
~~~~~~~~~~~~~~~~~~~~~~~~

This design slice elaborates the existing ``qnx_dispatch_engine`` unit.
It describes observed QNX implementation behavior, not a new decomposition,
public contract, or safety acceptance. The selected scope is ownership,
dispatch-thread lifecycle, resource-manager adapters and native callbacks,
client endpoint/pulse handling and transport, command scheduling, and owner cleanup.

``QnxDispatchEngine`` implements ``ISharedResourceEngine`` for the common
client and provides concrete resource-manager services to the QNX server.
``DispatchThreadRunner`` and ``TimedCommandQueue`` remain implementation
classes within this engine design, not new architectural units. The queue
implementation is also used by the Unix Domain engine. ``QnxResourcePath``
and ``NonAllocatingFuture`` are supporting dependencies; platform factories
and concrete server behavior remain outside this slice.

.. _qnx-dispatch-engine-class-view:

Class View and Ownership
~~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: qnx_dispatch_engine_class_diagram.puml

The class view selects resource ownership and lifecycle/scheduling declarations
rather than the complete C++ API. The engine owns its moved ``OsResources``
adapters, two embedded runners, timer handle, client-side protocol receive
buffer ``posix_receive_buffer_``, endpoint-index storage, timed queue, and native
callback tables. The caller-supplied memory resource is borrowed and must
outlive the members using it. Each runner borrows adapters from ``OsResources``
and manages its own dispatch handle, context, side-channel connection, and thread.

The intrusive endpoint and command lists do not own their entry storage.
Consumers retain their entries and callback captures for the relevant
registration/dispatch lifetime. Owner cleanup removes registrations; it does
not prevent another submission for the same owner.
The nested ``ResourceManagerServer`` retains shared engine ownership on behalf
of a concrete server. Factories and client connections also retain shared
engine ownership; destroying one factory is not engine shutdown.

.. _qnx-dispatch-engine-buffers:

Receive Buffer Ownership
~~~~~~~~~~~~~~~~~~~~~~~~

Client and server receive storage are distinct:

* ``posix_receive_buffer_`` belongs to the engine's client transport.
  ``RegisterPosixEndpoint`` grows it to accommodate the endpoint receive bound
  plus the protocol-code byte. ``ReceiveProtocolMessage`` reads into it and
  returns a borrowed payload span after that byte; it does not allocate a
  server receive buffer.
* ``DispatchThreadRunner::StartPostInit`` calls ``dispatch_context_alloc`` and
  stores the returned native allocation in ``context_pointer_``. For the
  server runner, that allocation includes the native server receive workspace
  used by dispatch and resource-manager callbacks. ``InitServerThread``
  configures the resource-manager buffer attributes before this allocation.
  ``Stop`` frees the context only after joining that runner's worker.
* Queued replies and notifications in the concrete server connection are a
  third kind of storage, owned by the server unit. They are neither the
  client receive vector nor the native dispatch receive workspace.

.. _qnx-dispatch-engine-resource-manager:

Resource-Manager Adapters
~~~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: qnx_dispatch_engine_resource_manager_class_diagram.puml
   :caption: Native connection handling: :ref:`qnx-dispatch-engine-native-callbacks`.

The engine's nested abstract classes bridge native QNX handles and concrete
server behavior. They are internal implementation adapters, not additional
public interfaces or architectural units.

``ResourceManagerServer`` privately embeds ``extended_dev_attr_t``, retains a
shared engine, and initializes ``resmgr_id_`` to ``-1``. ``Start`` forwards to
``StartServer`` on its caller's thread. Under the server runner mutex, the
engine attaches the path with its connect/I/O tables and the server as the
native handle. An attachment failure returns its OS error. Success records
the attachment ID, then initializes the native attributes with
``S_IFNAM | 0666``. ``Stop`` forwards to ``StopServer``; a non-``-1`` attachment
is detached under the same mutex with ``_RESMGR_DETACH_CLOSE``, and the ID is
reset without checking the detach result. The base destructor has no implicit
``Stop`` call. ``ProcessConnect`` is supplied by the concrete server.

``ResourceManagerConnection`` privately embeds ``iofunc_ocb_t`` and initializes
its receive ID and select/ping events to zero. ``AttachConnection`` forwards
the supplied connection OCB and server attributes to ``iofunc_ocb_attach``;
it does not allocate a concrete connection. ``GetServer`` follows the native
OCB attribute binding through ``OcbToServer``, rather than retaining another
shared engine pointer. ``OcbToConnection`` and ``ResmgrHandleToServer`` recover
the other corresponding adapters at native callback entry points.

The concrete server connection supplies ``ProcessInput``,
``ProcessReadRequest``, ``HasSomethingToRead``, and ``ProcessDisconnect``.
The engine calls these private virtual hooks as the adapter's friend;
connection allocation, reply/notification storage, application callbacks, and
concrete connection lifetime remain in the server unit.

.. _qnx-dispatch-engine-lifecycle:

Engine Construction and Destruction
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: qnx_dispatch_engine_lifecycle_activity.puml
   :caption: Runner procedures: :ref:`qnx-dispatch-engine-runner-lifecycle`.

Construction configures the native resource-manager callback tables before
starting the client runner and then the server runner. Despite their names,
``InitClientThread`` and ``InitServerThread`` execute synchronously in the
constructing thread, inside the respective runner's startup lock. The former
attaches timer, select, and connection-death pulse callbacks and creates the
monotonic timer. The latter configures resource-manager dispatch buffering
through a null-path attachment. Checked setup failures log and terminate;
there is no recoverable initialization result.

Destruction explicitly stops and joins the server runner first. If the client
dispatch handle exists, it destroys the timer before stopping and joining the
client runner. Member destruction follows these explicit operations. Timer
destruction and runner teardown OSAL results are not checked. This view does
not establish a finite destruction deadline or permission to destroy the
engine from one of its own callback threads.

.. _qnx-dispatch-engine-runner-lifecycle:

Runner Caller-Side Lifecycle
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: qnx_dispatch_engine_runner_activity.puml
   :caption: Independently executing worker: :ref:`qnx-dispatch-engine-runner-dispatch`.

Every action in this view runs on the ``Start`` or ``Stop`` caller, not on the
worker thread. There are separate client and server runner instances. ``Start``
holds
``thread_mutex_`` across the duplicate-start check, dispatch/side-channel
setup, caller-supplied initialization, context allocation, and worker creation.
Creating ``std::thread`` launches the separate worker activity; it is not an
inline call to ``RunOnThread``. The caller restores its signal mask, releases
the startup mutex, and returns without waiting for dispatch work.

Startup temporarily blocks SIGTERM in the creating thread so the worker
inherits the blocked mask, then restores the creator's old mask. The signal
setup calls have unchecked results. Dispatch setup failures handled by
``ValueOrTerminate`` or ``IfUnexpectedTerminate`` log and terminate.

``Stop`` sends the quit pulse and joins the independently executing worker.
Only after that join does the caller detach the side channel, destroy the
dispatch handle, free the context and its receive workspace, and clear
``dispatch_pointer_``. It does not take
the startup mutex or impose a timeout. Duplicate startup while active is
guarded; this view does not establish concurrent stopping or restart after stopping.

.. _qnx-dispatch-engine-runner-dispatch:

Runner Worker-Thread Dispatch
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: qnx_dispatch_engine_dispatch_activity.puml
   :caption: Caller-side lifecycle: :ref:`qnx-dispatch-engine-runner-lifecycle`; server request handlers: :ref:`qnx-dispatch-engine-native-callbacks`; client pulse handlers: :ref:`qnx-dispatch-engine-client-pulses`.

This view begins in the newly created thread's lambda. The worker acquires and
releases ``thread_mutex_`` before entering ``RunOnThread``; startup's lock makes
the assignment of ``thread_`` precede dispatch. The loop retries a failed
``dispatch_block`` and invokes ``dispatch_handler`` after a successful block,
using that runner's ``context_pointer_``. The handler result is unchecked.

Dispatch invokes registered callbacks on this worker. In both runners, the
quit-pulse callback sets ``quit_flag_``; the loop subsequently exits and the
worker returns, allowing the ``Stop`` caller's join to complete. Client-runner
callbacks handle timer, select, and connection-death pulses. Server-runner
callbacks handle resource-manager requests through the engine's native tables.
This worker-dispatch path does not move callbacks to the thread that called
``Start`` or ``Stop``.

.. _qnx-dispatch-engine-native-callbacks:

Native Resource-Manager Callbacks
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: qnx_dispatch_engine_connection_callbacks_activity.puml
   :caption: Worker context: :ref:`qnx-dispatch-engine-runner-dispatch`; adapter definitions: :ref:`qnx-dispatch-engine-resource-manager`.

``SetupResourceManagerCallbacks`` initializes native default tables and
overrides the following five entries. The server attachment passes those tables
to native dispatch; callbacks receive the server dispatch context's message
workspace, not ``posix_receive_buffer_``.

.. list-table:: Native Callback Routing and Guards
   :header-rows: 1
   :widths: 24 76

   * - Table entry and handler
     - Observed engine behavior
   * - ``connect_funcs_.open`` / ``io_open``
     - Recover the server from its native handle and lock its attributes.
       An ``iofunc_open`` error unlocks and returns immediately. Otherwise call
       ``ProcessConnect`` under that lock, unlock, and return its result.
       The concrete callback can use ``AttachConnection`` to bind an OCB.
   * - ``io_funcs_.write`` / ``io_write``
     - Propagate ``iofunc_write_verify`` errors; reject unsupported xtype
       with ``ENOSYS``, an absent protocol-code byte with ``EBADMSG``, and a
       payload exceeding the received message portion with ``EMSGSIZE``.
       Pass the code and borrowed payload to ``ProcessInput``. Its boolean
       result is ignored; the handler acknowledges the byte count with ``EOK``.
   * - ``io_funcs_.read`` / ``io_read``
     - Propagate ``iofunc_read_verify`` errors and reject unsupported xtype
       with ``ENOSYS``. A zero-byte request returns zero read bytes and
       ``_RESMGR_NPARTS(0)``. Other requests delegate to ``ProcessReadRequest``
       and return its result.
   * - ``io_funcs_.msg`` / ``io_msg``
     - Accept only the private sticky-select manager ID, otherwise ``ENOSYS``.
       Fetch the event-registration message with ``resmgr_msgget``; log and
       return a fetch error or ``EBADMSG`` for a short message. Store receive
       ID and select/ping events, then deliver the select event if
       ``HasSomethingToRead`` reports pending data. Delivery is unchecked.
   * - ``io_funcs_.close_ocb`` / ``io_close_ocb``
     - Attempt a select-event delivery, lock attributes, detach the OCB,
       unlock, and call ``ProcessDisconnect`` before returning ``EOK``.
       Event delivery, attribute-lock/unlock, and detach results are unchecked.

The open/close activity elaborates connection dispatch. The other three
handlers' guards and delegation are described in the table rather than
separate activities. An engine-level callback return does not imply a new
application delivery or disconnection guarantee.

.. _qnx-dispatch-engine-endpoint-registration:

Client Endpoint Registration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: qnx_dispatch_engine_register_endpoint_activity.puml
   :caption: Pulse delivery: :ref:`qnx-dispatch-engine-client-pulses`; shared removal: :ref:`qnx-dispatch-engine-endpoint-removal`.

``RegisterPosixEndpoint`` asserts ``IsOnCallbackThread``; it does not queue
itself when called elsewhere. That predicate accepts either engine worker,
not only the client worker, and does not serialize the workers with each other.
The method grows the shared client receive vector when needed, adds the
borrowed endpoint to ``posix_endpoint_list_``, and assigns it a poll slot.
It reuses the first null slot and increments its 16-bit nonce, or appends a
slot with nonce zero. The ``UINT32_MAX`` new-index guard returns before native
registration, without removing the already-inserted intrusive-list entry.

The private sticky-select request contains new-data and ping events. Each
event targets the client runner's side channel with ``kSelectPulseCode``.
The pulse value encodes a 32-bit slot index in bits 0..31, a 16-bit signal in
bits 32..47, and a 16-bit slot nonce in bits 48..63.
``PackSelectPulseValue`` and ``UnpackSelectPulseValue`` implement this layout.
The locally constructed signals are ``kSignalNewData`` (0) and
``kSignalPing`` (1); they are distinct from the native pulse code.

The engine attempts ``MsgRegisterEvent`` for both events and then ``MsgSend``
of the event-registration request. The server-side private-message handler
is described in :ref:`qnx-dispatch-engine-native-callbacks`. Event-registration
and send failures are logged, but do not roll back list/slot state or prevent
the remaining attempts. This ``void`` operation reports no failure result and
does not close the descriptor.

.. _qnx-dispatch-engine-endpoint-removal:

Client Endpoint Removal
~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: qnx_dispatch_engine_unselect_endpoint_activity.puml
   :caption: Owner-cleanup caller: :ref:`qnx-dispatch-engine-cleanup`.

Public ``UnregisterPosixEndpoint`` asserts engine callback context, erases the
supplied endpoint's intrusive-list entry, and calls ``UnselectEndpoint``.
Owner cleanup also calls that helper for its matching entries. The helper
does not perform the callers' list removal itself.

``UnselectEndpoint`` finds the poll slot by endpoint address, saves its index
and nonce, and clears its endpoint pointer before native unregistration.
It rebuilds both signal events from that saved identity and calls
``MsgUnregisterEvent``, ignoring each result. Removal retains the nonce;
later reuse increments it. A non-empty disconnect callback is invoked even
when no poll slot was found. This helper's not-found branch does not make
the public wrapper's intrusive-list erase an idempotent operation.
Neither method directly closes the descriptor; callback and connection
lifecycle behavior remain separate.

.. _qnx-dispatch-engine-client-pulses:

Client Pulse Dispatch
~~~~~~~~~~~~~~~~~~~~~

.. uml:: qnx_dispatch_engine_client_pulses_activity.puml
   :caption: Registration and pulse encoding: :ref:`qnx-dispatch-engine-endpoint-registration`; input transport: :ref:`qnx-dispatch-engine-client-transport`.

Both pulse callbacks are attached to the client runner and execute on its
worker thread. ``SelectPulseCallback`` decodes the index, signal and nonce.
An out-of-range index, a nonce mismatch or a null endpoint drops the pulse.
For a valid slot, the ping signal invokes ``ping``; every other signal uses
``input``. The latter branch relies on local registration constructing only
new-data and ping values, rather than an additional signal-validation check.
The callback returns zero after either delivery or rejection.

``CoidDeathPulseCallback`` reads a connection ID from the pulse integer
value, not the packed select-event layout. A successful ``ConnectServerInfo``
returning that same ID identifies an obsolete death pulse, which is logged
and ignored. A failed lookup or a different returned ID instead leads to a
search for a non-null poll endpoint with the matching descriptor. If found,
the engine logs that path and invokes its ``input`` callback; otherwise no
endpoint callback runs. It returns zero in either case.

The death-pulse handler does not itself call ``disconnect``, unregister an
endpoint, or close a descriptor. The consumer's input/receive-error and
connection-lifecycle handling determine subsequent cleanup.

.. _qnx-dispatch-engine-client-transport:

Client Transport Behavior
~~~~~~~~~~~~~~~~~~~~~~~~~

These methods execute in their callers. They do not add a worker handoff,
readiness/size validation, a timeout, or transport interruption.

.. list-table:: Transport Operations
   :header-rows: 1
   :widths: 30 70

   * - Operation
     - Observed behavior
   * - ``TryOpenClientConnection``
     - Format the identifier with ``QnxResourcePath`` and open it read/write
       with close-on-exec. Return the OS descriptor or open error. Path-format
       preconditions belong to the supporting resource-path implementation.
   * - ``CloseClientConnection``
     - Call ``close`` on the supplied descriptor and ignore its result.
       This method does not remove endpoint registrations.
   * - ``SendProtocolMessage``
     - Supply ``writev`` with two vectors: the one-byte code and the borrowed
       payload. Propagate an OS error; any successful ``writev`` result is
       returned as success without comparing its byte count.
   * - ``ReceiveProtocolMessage``
     - Read into ``posix_receive_buffer_``. Propagate an OS error, map a
       non-positive successful result to ``EPIPE``, otherwise extract the
       first byte as the code and return a span over the remaining bytes.
       A code-only result therefore has an empty payload.

The receive span borrows the engine's client buffer. A later read can overwrite
its content, and buffer growth can invalidate its address. No payload copy or
independently owned result is introduced. Common-client validation, queueing,
and lifetime policy are outside these thin transport operations.

.. _qnx-dispatch-engine-scheduling:

Command Scheduling and Timer Dispatch
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: qnx_dispatch_engine_commands_activity.puml
   :caption: Owner cancellation: :ref:`qnx-dispatch-engine-cleanup`.

``EnqueueCommand`` registers a timed entry, then sends a timer pulse to the
client runner. Registration assigns the borrowed entry's deadline, owner key,
and callback before locking the queue for insertion. Entries are inserted
before the first strictly later deadline, retaining FIFO order at equal
deadlines. ``RegisterImmediateEntry`` uses the same operation with
``TimePoint{}``. This describes use of an available entry, not permission to
register an already-linked entry or concurrently rewrite its callback.

``TimerPulseCallback`` invokes ``ProcessTimerQueue`` on the client thread.
``ProcessQueue`` uses one ``now`` snapshot for that invocation. It unlinks a
due entry, moves its callback to a local, unlocks, invokes the callback with
that snapshot, and destroys the local callback before relocking. This permits
callback rescheduling of the same unlinked entry. Both callback invocation and
capture destruction occur outside the queue mutex.

A non-due front entry returns its deadline. The engine computes
``duration_cast<nanoseconds>(then - Clock::now()).count() + 1``, casts it to
``uint64_t``, and requests a one-shot timer. No clamping or timer-error recovery
is added by this design. An empty queue returns ``TimePoint{}``, and the engine
neither rearms nor explicitly disarms the timer on that path. Pulse-send and
timer-set results are unchecked. The queue ordering and rearm sequence do not
add an exact wakeup-latency or bounded command-completion guarantee.

.. _qnx-dispatch-engine-cleanup:

Owner Cleanup and Acknowledgement
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: qnx_dispatch_engine_cleanup_activity.puml
   :caption: Queued continuation dispatch: :ref:`qnx-dispatch-engine-scheduling`; endpoint removal: :ref:`qnx-dispatch-engine-endpoint-removal`.

The engine's ``CleanUpOwner(nullptr)`` is a no-op. On either recognized engine
callback thread, a non-null owner's cleanup runs inline. In another thread,
the method creates a stack command and ``NonAllocatingFuture``, queues an
immediate continuation with that owner, wakes the client runner, and waits.
The continuation performs ``ProcessCleanup`` before marking the future ready.
The future uses the client runner mutex and ``cleanup_ready_condition_``;
its predicate also handles readiness preceding the wait.

``ProcessCleanup`` first removes matching endpoints and calls
``UnselectEndpoint`` for each, as described in
:ref:`qnx-dispatch-engine-endpoint-removal`. It then cleans matching queued
commands. Each matching command is
unlinked before its callback is reset outside the queue mutex; captures are
destroyed without invoking the command. The queue helper treats a null key
like any other key, unlike the engine's null-owner early return.

The external-thread return acknowledges this cleanup continuation. There is
no timeout and no admission gate preventing concurrent or reentrant work for
the same owner. This is not a general quiescence proof, an interruption of
blocked QNX transport, or a bounded shutdown guarantee.

.. _qnx-dispatch-engine-execution-context:

Execution Context and Lock Boundaries
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table:: Contexts in This Slice
   :header-rows: 1
   :widths: 32 68

   * - Entry or resource
     - Observed context and synchronization
   * - Construction and runner initialization
     - Constructing caller; each runner's startup mutex covers its own setup.
   * - Timer queue processing
     - Client dispatch thread; the queue mutex protects list operations, not
       execution or destruction of callbacks.
   * - Client select/death pulse delivery
     - Client dispatch thread; endpoint callbacks execute in that call.
       A death pulse requests input rather than directly disconnecting.
   * - Native resource-manager request dispatch
     - Server dispatch thread, using its allocated dispatch context and native
       receive workspace. Concrete adapter hooks execute in that same call.
   * - ``IsOnCallbackThread``
     - Recognizes either runner. This does not serialize the client and server
       threads with each other.
   * - Owner cleanup
     - Inline on either recognized callback thread, otherwise dispatched to
       the client thread with a blocking acknowledgement.
   * - Client runner mutex
     - Used for startup and future readiness/wait synchronization; not held
       throughout the dispatch loop.
   * - Server runner mutex
     - Also guards ``StartServer``/``StopServer`` attachment operations for
       the QNX dispatch attachment thread-safety workaround.
   * - Endpoint indexes and client receive buffer
     - Not protected by the timed queue's mutex. Their calling-context and
       cross-thread interleavings need separate review.
   * - Native dispatch contexts
     - Allocated during caller-side startup, used by their respective workers,
       and freed by the stopping caller only after the join.
   * - Runner stopping
     - Executes in its caller and joins the worker; callback-thread
       destruction and finite completion are not established.

.. _qnx-dispatch-engine-operation-inventory:

Operation Inventory and Deliberate Limits
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table:: Implementation Coverage
   :header-rows: 1
   :widths: 48 52

   * - Operations
     - Coverage in this slice
   * - Engine construction/destruction, ``InitClientThread``,
       ``InitServerThread``, ``SetupResourceManagerCallbacks``
     - Lifecycle activity and the setup descriptions above.
   * - Runner construction/destruction, ``Start``, ``StartPreInit``,
       ``StartPostInit``, ``RunOnThread``, ``Stop``, ``SendQuitPulse``,
       ``QuitPulseCallback``
     - Separate caller-side lifecycle and worker-dispatch activities.
   * - ``EnqueueCommand``, ``WakeUpTimerQueue``, ``TimerPulseCallback``,
       ``ProcessTimerQueue``; queue registration and processing
     - Scheduling activity, including the expanded queue algorithm.
   * - ``CleanUpOwner``, ``ProcessCleanup``; queue owner cleanup
     - Cleanup activity and acknowledgement/lock descriptions.
   * - Engine resource/logger accessors, ``GetDefaultOsResources``,
       ``IsOnCallbackThread``; runner identity/handle/mutex accessors
     - Ownership and context inventory; no separate activity. Default
       adapter acquisition is not an allocation-free execution claim.
   * - ``TryOpenClientConnection``, ``CloseClientConnection``,
       ``SendProtocolMessage``, ``ReceiveProtocolMessage``, resource paths
     - Transport behavior table, error mapping, framing, and borrowed receive
       storage described without separate thin-wrapper activities.
   * - ``RegisterPosixEndpoint``, ``UnregisterPosixEndpoint``,
       ``UnselectEndpoint``, ``SelectPulseCallback``,
       ``CoidDeathPulseCallback``, pulse packing/unpacking
     - Registration, shared removal and client pulse activities; slot/nonce
       identity, event encoding, stale-pulse checks, callback contexts and
       peer-death input dispatch described.
   * - ``StartServer``, ``StopServer``, ``AttachConnection``, resource-manager
       server/connection adapters and conversion helpers
     - Adapter class view, initialization, attachment/detachment, shared-engine
       ownership, native OCB binding, and concrete-server delegation described.
   * - ``io_open``, ``io_write``, ``io_read``, ``io_msg``, ``io_close_ocb``
     - Native callback routing and guard table; open/close activity on the
       server worker. Read/write/private-message paths have prose coverage.

The inventory describes implemented control flow and resource boundaries.
It does not add thread serialization, callback-lifetime permissions, transport
interruption, or a finite completion deadline.
