Client Connection Unit Design
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Scope and Responsibility
~~~~~~~~~~~~~~~~~~~~~~~~

This pilot elaborates the existing ``client_connection`` unit without changing
its architectural boundaries or public API. ``ClientConnection`` is built in
``message_passing_client_connection`` and resides in
``score::message_passing::detail``.

``ClientConnection`` manages one client's connection lifecycle and send/reply
coordination through an injected ``ISharedResourceEngine``. The concrete engine
provides transport, endpoint registration, and command execution.
The engine's client-related scheduling and command queue internals belong in
engine unit designs, not this view; sharing a build target does not determine
design ownership. Platform factories, servers, and the ``NonAllocatingFuture``
helper are also outside this unit's design scope.

Class View
~~~~~~~~~~

.. uml:: client_connection_class_diagram.puml

The view selects lifecycle declarations, synchronization members, and queue
storage. The public send/reply signatures remain defined by
``IClientConnection`` and the architectural public API view. This is not yet a
complete class specification. The dynamic inventory below makes operation and
continuation coverage explicit without claiming complete behavioral validation.

.. _client-connection-object-lifetime:

Object Lifetime
~~~~~~~~~~~~~~~

.. uml:: client_connection_object_lifetime_activity.puml
  :caption: Stopped callback lifetime: :ref:`client-connection-switch-to-stop-state`.

Construction initializes the stopped, not-yet-started lifecycle and the fixed
combined send storage before any ``Start`` call. It retains the engine, copies
the identifier/configuration, and sets the receive-size bound to the larger of
the configured reply and notification bounds. This view does not qualify
allocation-failure handling or establish allocation-free execution everywhere.

Destruction first synchronizes with the callback context's recursive finalizer
mutex, then clears free-pool intrusive membership before member destruction.
The active-state branch requests ``Stop`` and waits in 10 ms intervals for
``kStopped`` before taking that same mutex. This is observed fallback cleanup,
not supported active-state destruction or a bounded stop-completion guarantee.
The public stopped-state and callback-context destruction preconditions remain
in force. The separate shared finalizer explains supported stopped-callback
destruction; other shared engine owners can outlive this connection.

.. _client-connection-establishment:

Lifecycle and Connection Establishment
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: client_connection_connect_activity.puml
  :caption: Terminal-error cleanup: :ref:`client-connection-switch-to-stop-state`.

The terminal-error branch references
:ref:`client-connection-switch-to-stop-state`; its caller context and the
establishment review gaps below remain distinct from that shared procedure.

Construction leaves the connection in ``kStopped`` with stop reason ``kInit``
and descriptor ``-1``. ``Start`` installs the state and notification callbacks,
then calls ``DoRestart``. ``Restart`` returns without action unless the state is
``kStopped``; when accepted, it calls ``DoRestart`` without replacing callbacks.
``Start`` does not perform that state check in the current implementation.

``DoRestart`` resets the stop reason to ``kNone`` and publishes ``kStarting``
through ``ProcessStateChange``. The starting-state callback runs synchronously
before the retry delay is reset or connection work is scheduled. The delay is
reset to 50 ms for each start or accepted restart. With ``sync_first_connect``,
the first ``TryConnect`` executes directly in the calling thread; otherwise it
is queued through ``connection_timer_`` at ``TimePoint{}``, with owner ``this``.

The dynamic connection-establishment activity view records this local control
flow and the subsequent ``TryConnect`` outcomes. Its loop represents separate
engine-scheduled invocations, not a blocking retry loop inside ``TryConnect``.

* ``EAGAIN``, ``ECONNREFUSED``, and ``ENOENT`` schedule another invocation at
  ``FromNow(retry_delay)``. The command uses the current delay; the stored delay
  for a subsequent retry increases by one third of its value, rounded up to
  whole milliseconds. The update is computed with checked 64-bit arithmetic
  and applied only if the new delay would not exceed 5,000 ms. Otherwise the
  stored delay is left unchanged; it is not clamped to 5,000 ms. This bounds
  the requested retry delay, not the retry count, total connection time, or
  engine scheduling latency. There is no overall connection deadline in this
  method.
* ``EACCES`` selects ``kPermission``. Every other non-retry open error selects
  ``kIoError``. If ``TrySetStopReason`` accepts the reason, the method publishes
  ``kStopping`` and calls ``SwitchToStopState``; otherwise it preserves the
  reason already selected by another stop path. Neither branch schedules
  another retry. A later explicit ``Restart`` is distinct from automatic retry.
* A successful open stores the descriptor and initializes the embedded endpoint
  with its owner, receive-size bound, and callbacks. Registration is immediate
  on the engine callback thread; otherwise registration itself is queued using
  ``connection_timer_``. The method then publishes ``kReady``. In the queued
  case, publishing readiness does not mean that the registration command has
  already executed.

These are observed implementation paths through the injected engine, not
additional public guarantees or a separate server health-check mechanism.
The view does not model all interleavings with ``Stop`` or reentrant state
callbacks. In particular, ``DoRestart`` continues after the starting callback,
and the successful-open branch does not recheck the stop reason before
publishing ``kReady``. The public allowance for ``Stop`` within the starting-state
callback must therefore not be treated as verified by this establishment view.

.. _client-connection-send-one-way:

Sending Without a Reply
~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: client_connection_send_activity.puml
  :caption: Queue helpers: :ref:`client-connection-queue-admission` and :ref:`client-connection-queue-arming`.

``Send`` checks message size before readiness, returning ``EMSGSIZE`` or
``EINVAL`` respectively. When neither ``fully_ordered`` nor ``truly_async`` is
configured, it calls transport directly without taking ``send_mutex_`` or
inspecting the shared reply/drain slot. A pending request therefore does not
force this default one-way path into the library queue.

The other paths take ``send_mutex_``. An engaged ``waiting_for_reply_`` causes
queued admission. With a free slot, ``truly_async`` still queues and calls
``ArmSendQueueUnderLock``; otherwise the ordered send calls transport while
holding the mutex. Queued admission returns ``ENOBUFS`` when the combined send
pool has no free command. Successful queued admission copies the message into
connection-owned storage and returns before later transport dispatch.

Both direct paths return the transport result. Queued success reports local
acceptance, not peer receipt; later one-way transport errors have no reply
callback through which to report them. The shared-slot and queue helpers are
described in :ref:`client-connection-queue-admission`,
:ref:`client-connection-queue-arming`, and
:ref:`client-connection-send-queue-dispatch`.
This admission view does not establish all concurrent send/stop interleavings
or a whole-path non-blocking guarantee.

.. _client-connection-send-with-callback:

Sending With a Reply Callback
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: client_connection_send_with_callback_activity.puml
  :caption: Queued requests: :ref:`client-connection-queue-admission` and :ref:`client-connection-queue-arming`.

``SendWithCallback`` checks message size and readiness in the same order as
``Send``, then takes ``send_mutex_``. An engaged reply/drain slot causes queued
admission. With a free slot, ``truly_async`` selects queuing and arms dispatch;
otherwise the method sends ``REQUEST`` directly under the mutex and installs
the reply callback in ``waiting_for_reply_`` only after successful transport.
``fully_ordered`` does not select a different request admission path.

Local rejection returns ``EMSGSIZE``, ``EINVAL``, or ``ENOBUFS``. Direct transport
failure returns its error without installing or invoking the reply callback.
After acceptance, the callback is held in either queued command storage or the
reply slot. Later queue transport, input processing, and stopping determine
reply/error delivery; this view does not imply receipt merely from API success.

.. _client-connection-send-wait-reply:

Sending and Waiting for a Reply
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: client_connection_send_wait_reply_activity.puml
  :caption: Queued admission: :ref:`client-connection-queue-admission`; direct-error rearming: :ref:`client-connection-queue-arming`.

``SendWaitReply`` first rejects engine callback context with ``EAGAIN``. Message
size and readiness checks follow, so that callback-context rejection takes
precedence over ``EMSGSIZE`` or ``EINVAL``. The method creates a local result,
``NonAllocatingFuture``, and completion callback, then takes ``send_mutex_``.

An engaged reply/drain slot causes queued admission and can return ``ENOBUFS``.
With a free slot, the method installs its completion callback, releases the
mutex around direct ``REQUEST`` transport, then reacquires it. This direct
choice is independent of ``truly_async`` and ``fully_ordered``. Other callers
can admit queued work while transport is in progress because the slot remains
engaged even though the mutex is released.

If direct transport fails, an empty send queue causes the slot to be reset;
otherwise the implementation calls ``ArmSendQueueUnderLock`` to request queue
progress. The method returns the transport error without waiting. The nonempty-
queue branch reaches that helper without clearing the reply slot at this call
site; its consistency with the helper's free-slot debug precondition remains a
queue-design review gap, not a runtime fix in this increment.

After successful direct transport or queued admission, the method releases the
mutex and waits on the local future's ready predicate. A callback result that
arrives before ``Wait`` is handled by that predicate. The completion callback
passes through an error, reports ``EMSGSIZE`` if the received reply exceeds the
caller-provided buffer, or copies the reply into that buffer and returns a span
trimmed to the copied size. Each result marks the future ready and notifies the
shared condition variable. The wait releases ``send_mutex_`` while blocked;
it does not apply an overall request/reply deadline.

The local future and the referenced reply buffer stay in scope until completion
on this waiting path. The design describes only the helper's caller-facing
boundary; the ``NonAllocatingFuture`` implementation remains outside this unit.
Queue progress is elaborated in :ref:`client-connection-send-queue-dispatch`.
Input/reply handling is elaborated in :ref:`client-connection-input-processing`
and :ref:`client-connection-endpoint-callbacks`. The admission views do not
establish every cancellation or reentrant callback interleaving.

.. _client-connection-queue-admission:

Queue Admission
~~~~~~~~~~~~~~~

.. uml:: client_connection_try_queue_message_activity.puml

``TryQueueMessage`` is called with ``send_mutex_`` already held. With no free
command it returns ``false``; the API caller reports ``ENOBUFS``. Otherwise it
removes the free-pool front, copies the message into that command's storage,
moves the callback, and appends the command to the send-queue tail. The storage
remains owned by ``send_storage_`` throughout this intrusive-list transfer.

The helper does not validate state or message size, occupy the reply slot, send
the message, or schedule dispatch. Its caller owns those decisions. It stores
the supplied callback without a non-empty check; the dispatcher selects
``REQUEST`` for a non-empty callback and ``SEND`` for an empty one. This is the
observed dispatch discriminator, not an additional callback API guarantee.

.. _client-connection-queue-arming:

Arming Queued Dispatch
~~~~~~~~~~~~~~~~~~~~~~

.. uml:: client_connection_arm_send_queue_activity.puml
  :caption: Engine callback dispatch: :ref:`client-connection-send-queue-dispatch`.

``ArmSendQueueUnderLock`` expects the caller's send lock to be held and asserts
a disengaged reply slot in debug builds. It installs an empty callback marker
inside the optional slot, then enqueues the embedded ``async_send_command_`` at
``TimePoint{}`` with owner ``this``. The marker is a dispatch reservation, not a
request already sent to the peer. Other admission paths seeing the engaged slot
queue without attempting another arming of the same command entry.

When the engine invokes the stored command callback, that callback acquires
``send_mutex_`` with a unique lock and calls
:ref:`client-connection-send-queue-dispatch`. The arming helper itself returns
without executing that continuation and without releasing the caller's lock.
Stopping removes connection-owned engine work through the shared finalizer.

Ordinary asynchronous admission reaches arming only after observing a free slot
and queuing a command. The ``SendWaitReply`` direct-error recovery path instead
reaches it while its completion callback remains installed at the call site.
The existing precondition mismatch is still open; this design does not add a
reset or relax the assertion to hide it.

.. _client-connection-send-queue-dispatch:

Queued Send Dispatch
~~~~~~~~~~~~~~~~~~~~

.. uml:: client_connection_process_send_queue_activity.puml

``ProcessSendQueueUnderLock`` receives an already-held ``send_mutex_`` lock.
It removes commands from the queue front, releasing the lock around transport
and request-error callbacks. A detached command remains reserved during
transport, then returns to the front of the free pool under the reacquired lock.
Admission order is FIFO; free-command reuse is LIFO.

A non-empty command callback selects ``REQUEST``. The callback moves into the
shared reply slot before transport. A successful send returns from draining
with that slot engaged, leaving later commands queued until reply processing
resumes progress. The command storage is already free at this point: the
pending callback, not the message-storage command, owns reply completion.

On request transport failure, the command is recycled before its callback is
invoked outside the mutex. The optional slot remains engaged while that moved
callback executes, so reentrant request admission queues rather than starting
another direct request. After reacquiring the mutex, draining resets the slot
and continues. An empty command callback instead selects ``SEND``; transport
errors are ignored, the command is recycled, and the temporary empty marker is
reset before draining continues. Neither transport-error branch itself claims
stopping or changes the connection state.

The helper returns with the caller's lock held. An empty queue at entry leaves
the incoming slot unchanged; there is no unconditional final slot reset. The
engine's queued command and reply-input handling are the two callers of this
procedure. The queued command is described in
:ref:`client-connection-queue-arming`; the reply-dispatch boundary is described
in :ref:`client-connection-input-processing`.

.. _client-connection-input-processing:

Processing Received Input
~~~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: client_connection_process_input_event_activity.puml
  :caption: Reply queue progress: :ref:`client-connection-send-queue-dispatch`.

``ProcessInputEvent`` receives a code and message span through the engine.
``EAGAIN`` is treated as a spurious wake-up and returns ``kNone``. ``EPIPE``
returns ``kClosedByPeer``; every other receive error returns ``kIoError``.
These results classify input failure without themselves selecting the stop
reason, publishing a stopping state, or requesting endpoint cleanup.

For ``REPLY``, the method takes ``send_mutex_``. An unoccupied reply slot returns
``kIoError``. Otherwise it moves the callback locally, resets the slot, and
calls :ref:`client-connection-send-queue-dispatch` under the same unique lock.
Only after that procedure returns does it release the mutex and invoke the
completed request's callback. Dispatch can send another queued request and
occupy the slot before the earlier callback runs; this is not callback delivery
followed by queue draining. The local callback receives the existing message
span. ``SendWaitReply`` performs its own caller-buffer bound check and copy in
the completion callback described above.

The guard checks optional engagement rather than callback non-emptiness. An
engaged empty queue-draining marker satisfies it, and the method has no empty
callback check before invocation. This distinction remains an input review
gap; the view does not add a protocol-error guard that is absent from the code.

For ``NOTIFY``, a non-empty notification callback receives the message span;
otherwise the notification is ignored without requesting a stop. Unknown
message codes return ``kIoError``. Successful reply or notification processing
returns ``kNone``. The endpoint wrapper owns the resulting stop decision.

.. _client-connection-endpoint-callbacks:

Registered Endpoint Callbacks
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: client_connection_endpoint_callbacks_activity.puml
  :caption: Input classification: :ref:`client-connection-input-processing`; disconnect cleanup: :ref:`client-connection-switch-to-stop-state`.

``TryConnect`` installs input, ping, and disconnect callbacks on the embedded
endpoint; the output callback is empty. The input callback first calls
:ref:`client-connection-input-processing`. A ``kNone`` result requests no stop
transition. For a non-``kNone`` result, only a successful ``TrySetStopReason``
publishes ``kStopping`` and unregisters the endpoint. If an API ``Stop`` has
already claimed stopping, the input path preserves that reason and leaves its
cleanup request in place. This is the engine side of the shared ownership gate.

The registered disconnect callback calls
:ref:`client-connection-switch-to-stop-state`; the input wrapper does not also
call it directly after unregistration. The disconnect lambda neither selects
a stop reason nor repeats the ownership gate. It relies on the shared helper's
asserted stopping-state, selected-reason, and callback-context entry conditions.
The engine determines when disconnect dispatch occurs; this view does not
introduce an immediate-dispatch guarantee or invent a new shutdown stop reason.

Ping delivery invokes a non-empty ``notify_callback_`` with an empty span and
otherwise does nothing. It performs no protocol receive and is distinct from
``NOTIFY`` processing, which forwards the received span. User notification
callbacks may themselves request ``Stop`` under the public contract; returning
from a successful input or ping callback therefore does not imply that the
connection still has its earlier state. All callback-combination lifetime
questions remain separate from this local callback-dispatch view.

.. _client-connection-stop-initiation:

Stopping and Finalization
~~~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: client_connection_stop_activity.puml
  :caption: Finalization: :ref:`client-connection-switch-to-stop-state`.

The stop-reason check is the ownership gate for stopping the current connection
lifecycle. It rejects a stop when no lifecycle is active, or when another path
has already claimed stopping, such as a communication failure detected by the
engine. API ``Stop`` and engine input-failure handling can compete; the winner
owns the transition and dispatches cleanup, while the loser does not duplicate
that work. This is the intended single-finalization mechanism, not a second
state machine or an independent cleanup operation for each caller.

``Stop`` attempts to select ``kUserRequested`` through ``TrySetStopReason``.
Only the first transition from ``kNone`` is accepted. A rejected attempt does
not publish a state or schedule cleanup; this also makes ``Stop`` on a newly
constructed connection a no-op, preserving ``kInit``.

This gate does not test whether a descriptor is open. ``DoRestart`` clears the
reason before establishment, so ``kStarting`` belongs to an active lifecycle
that can be stopped even before a descriptor is acquired. Initial or previously
stopped instances retain a non-``kNone`` reason until the next accepted start.

The winning caller publishes ``kStopping`` before dispatching cleanup. On the
engine callback thread it calls ``SwitchToStopState`` directly. Other callers
enqueue ``disconnection_command_`` at ``TimePoint{}``, with owner ``this``.
``Stop`` therefore requests cleanup rather than waiting for completion.

The endpoint input callback follows the corresponding engine-triggered path:
``ProcessInputEvent`` returns a reason, and a successful ``TrySetStopReason``
publishes ``kStopping`` and unregisters the endpoint. Its disconnect callback
then invokes :ref:`client-connection-switch-to-stop-state`. If an API stop has
already claimed stopping, input-failure handling leaves that caller's cleanup
in place rather than requesting it again.

The stop activity retains this caller-specific dispatch choice and references
the shared finalization procedure. The endpoint callback view references the
same procedure for failure or peer-termination cleanup, retaining its own
trigger, reason selection, and disconnect-dispatch boundary. Named calls and caption references
provide navigation, not automatic interprocedural validation of the activities.
The pinned SVG-object renderer cannot resolve a bare page-fragment link inside
an activity image, so the captions use Sphinx references instead.

.. _client-connection-switch-to-stop-state:

Shared Stop Finalization
~~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: client_connection_switch_to_stop_state_activity.puml

``SwitchToStopState`` expects ``kStopping``, a selected non-``kNone`` stop reason,
and engine callback context; the implementation asserts these conditions in
debug builds. The procedure itself neither selects nor resets the reason.
These entry conditions do not establish that every caller interleaving satisfies
them, including the existing establishment review gaps.

``SwitchToStopState`` disables the endpoint's disconnect callback and cleans up
engine registrations owned by the connection before closing an open descriptor.
Under ``send_mutex_`` it removes the pending reply slot and returns queued send
commands to the free pool. Non-empty pending or queued reply callbacks receive
``EPIPE`` outside that mutex. An empty queue-draining marker is simply removed.
After releasing the send mutex, it publishes ``kStopped`` through
``ProcessStateChangeToStopped``; the callback-context lifetime mechanism below
permits destruction or restart from that stopped-state callback.

Callback Reentrancy and Competing Stops
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Selecting a stop reason and publishing ``kStopping`` are separate atomic
operations. ``GetState`` and ``GetStopReason`` do not provide a combined snapshot.
The successful compare/exchange selects the caller that publishes the stopping
state and dispatches cleanup. While the stop reason remains non-``kNone``,
additional ``Stop`` calls do neither. ``Restart`` resets that reason for a new
connection cycle; the selection argument must not be extended across restart.

``ConcurrentStopRequestsFinalizeOnce`` releases two application threads through
a shared start gate before any queued cleanup runs. After both threads return,
the connection is ``kStopping`` with ``kUserRequested``, one stopping callback,
and one queued cleanup. Executing that cleanup produces one stopped callback;
further ``Stop`` calls do not repeat either callback or cleanup. This is focused
evidence for competing user stop requests, not proof of all stop/failure/restart
interleavings or arbitrary concurrent API use.

When the ready-state callback runs on the engine callback thread and calls
``Stop``, cleanup executes inline. ``ReadyStateCallbackCanStopConnection``
observes callback entries in the order ``kStarting``, ``kReady``, ``kStopping``,
``kStopped`` and confirms that ``Stop`` has reached ``kStopped`` before returning
to the ready callback. The stopping and stopped callbacks are therefore nested
within the ready callback, not deferred until it returns. This must not be
generalized to a ready callback running outside engine callback context.

``StoppedStateCallbackCanRestartConnection`` exercises ``Restart`` from the
first stopped-state callback. It enters ``kStarting`` and clears the stop reason
before that callback returns, then connects again and stops a second time.
The old finalizer still holds its local context reference and recursive mutex
while the nested restart invokes the starting callback. ``kStopped`` need not
remain observable after a stopped-state callback that immediately restarts.

``StoppedStateCallbackKeepsOwnedCaptureAliveDuringDestruction`` destroys the
connection from a stopped-state callback. A weak observer confirms that an
owned callback capture remains alive after connection destruction, then expires
after the callback returns and the finalizer releases its context reference.
The test keeps the callback within the library's fixed capture capacity; this
is not a guarantee of unrestricted callback capture size.

These cases do not qualify ``Stop`` from a starting-state callback, stopping
while an open attempt is in progress, or every combination of nested stopping,
restart, and destruction. In particular, proving destruction from a stopped
callback does not by itself establish capture lifetime for an outer callback
that triggered stopping. Those combinations remain review and test gaps.

Ownership and Resource Bounds
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* ``engine_`` retains shared ownership of the injected engine. Destroying the
  originating factory does not by itself end that engine's lifetime.
* Construction copies the service identifier and allocates send storage using
  the engine's memory resource. The shared storage has
  ``max_queued_sends + max_async_replies`` command objects, each reserving
  ``max_send_size`` bytes for its message. These configuration values feed a
  combined pool, not two independently enforced pools in this implementation.
* ``send_storage_`` owns the fixed set of command objects and their message
  storage. The intrusive ``send_pool_`` and ``send_queue_`` are complementary
  free/queued lists over that storage, not separate allocations:
  ``send_pool_`` contains commands available for admission, while
  ``send_queue_`` contains commands reserved for accepted sends awaiting
  transport. Admission moves one command from the free pool to the queue;
  queued admission fails with ``ENOBUFS`` when no free command remains.
* The lists are disjoint, but a command removed for dispatch temporarily
  belongs to neither list and remains in use until the transport call finishes.
  Queue draining then returns it to the free pool; stopping returns commands
  after removing them from the queue. A command can become free before its
  reply arrives, because the pending reply callback is held separately in
  ``waiting_for_reply_``.
* ``waiting_for_reply_`` serializes request/reply progress. An engaged optional
  can also contain an empty callback as a marker for scheduled queue draining;
  it does not always represent a request already sent to the server.
* The connection embeds its endpoint and command entries. Engine registrations
  use the connection as their owner key, and stopping calls
  ``CleanUpOwner(this)`` before closing the descriptor and publishing
  ``kStopped``.

Preallocated send storage does not establish that construction, callback
captures, or every subsequent operation is allocation-free. Allocation failure
handling and whole-path memory bounds are not qualified by this pilot.

Synchronization and Callback Lifetime
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``state_`` and ``stop_reason_`` are atomic. A compare/exchange on the stop reason
selects the first accepted stop reason when user-requested stopping and
background failures compete. This does not make arbitrary concurrent use of
every API operation safe.

``send_mutex_`` protects the send pool, queue, and reply slot. Queue draining
releases it around transport calls and reply callbacks, then reacquires it to
update queue state. ``SendWaitReply`` uses the same mutex and
``send_condition_`` with ``NonAllocatingFuture``; it rejects invocation from the
engine callback thread with ``EAGAIN`` rather than blocking that thread.

``CallbackContext`` keeps the state needed during stopped-state callback
invocation alive even if that callback destroys ``ClientConnection``.
``ProcessStateChangeToStopped`` takes a local shared reference to the context
and locks its recursive ``finalize_mutex`` before publishing ``kStopped`` and
invoking ``state_callback``.

If the callback destroys the connection, destruction of ``callback_context_``
releases the connection's reference, but the dispatcher's local reference still
owns the context. The executing callback object, including its owned captures,
and the mutex guarding that invocation therefore survive until the callback
returns and the lock guard unlocks. Only then is the local shared reference
released. The dispatcher does not access connection members after the callback.
This survival of callback state, rather than restart itself, is the reason for
the separate reference-counted object.

Destruction synchronizes with finalization through the same mutex. Its recursive
nature permits the destructor to take it from inside the stopped-state callback
without deadlocking on the dispatcher's existing lock.

The public lifetime preconditions still apply: destruction is only supported
in ``kStopped``, and destruction from a callback is only supported from the
stopped-state callback. The destructor's stop-and-wait fallback is observed
cleanup behavior, not permission to violate those preconditions. Referenced
callback captures must remain valid until stopping has completed.

The starting-state callback executes synchronously during ``Start`` or
``Restart``. Other callback contexts follow ``IClientConnection``; this design
does not introduce a uniform background-thread guarantee.

.. _client-connection-dynamic-coverage:

Dynamic Operation Inventory
~~~~~~~~~~~~~~~~~~~~~~~~~~~

This inventory accounts for each implemented operation in ``ClientConnection``
and its embedded ``SendCommand``, together with the registered continuations
below. Deleted copy/move constructors and assignments have no execution paths.
Coverage here means an explicit activity or behavioral description, not a test
coverage percentage, release acceptance, or proof of every interleaving.

.. list-table:: Implemented Operations
   :header-rows: 1
   :widths: 34 66

   * - Operation
     - Dynamic home or behavior
   * - ``ClientConnection``
     - :ref:`client-connection-object-lifetime`: ownership, configuration, pool initialization.
   * - ``~ClientConnection``
     - :ref:`client-connection-object-lifetime`: finalizer synchronization and observed fallback.
   * - ``SendCommand::SendCommand``
     - Object-lifetime initialization: initialize the intrusive base, allocator-aware message vector, and empty callback.
   * - ``Start``
     - :ref:`client-connection-establishment`: install callbacks, then enter ``DoRestart``.
   * - ``Restart``
     - :ref:`client-connection-establishment`: stopped-state guard and retained callbacks.
   * - ``DoRestart``
     - :ref:`client-connection-establishment`: reason reset, starting publication, first-attempt scheduling.
   * - ``TryConnect``
     - :ref:`client-connection-establishment`: retry policy, fatal errors, endpoint setup, readiness.
   * - ``Stop``
     - :ref:`client-connection-stop-initiation`: claim stopping and choose direct or queued cleanup.
   * - ``TrySetStopReason``
     - Stop-initiation ownership gate: load the reason, reject non-``kNone``, otherwise compare/exchange once.
   * - ``SwitchToStopState``
     - :ref:`client-connection-switch-to-stop-state`: owner cleanup, descriptor release, cancellation, finalization.
   * - ``ProcessStateChange``
     - :ref:`client-connection-execution-context`: publish non-stopped states and call the installed callback; delegate stopped publication.
   * - ``ProcessStateChangeToStopped``
     - Shared finalization: atomic publication alone if no callback; otherwise local context ownership and recursive mutex.
   * - ``Send``
     - :ref:`client-connection-send-one-way`: validation and direct/queued one-way admission.
   * - ``SendWithCallback``
     - :ref:`client-connection-send-with-callback`: request admission and callback storage.
   * - ``SendWaitReply``
     - :ref:`client-connection-send-wait-reply`: callback-thread rejection, request admission, completion wait/copy.
   * - ``TryQueueMessage``
     - :ref:`client-connection-queue-admission`: free-pool reservation and FIFO admission.
   * - ``ArmSendQueueUnderLock``
     - :ref:`client-connection-queue-arming`: empty marker and scheduled dispatch continuation.
   * - ``ProcessSendQueueUnderLock``
     - :ref:`client-connection-send-queue-dispatch`: transport, recycling, slot transitions, and error callbacks.
   * - ``ProcessInputEvent``
     - :ref:`client-connection-input-processing`: receive classification and reply/notification dispatch.
   * - ``IsInCallback``
     - Delegate to ``engine_->IsOnCallbackThread()``; this helper does not create a thread or change connection state.
   * - ``GetState``
     - Return one atomic ``state_`` load; no engine work or explicit mutex acquisition.
   * - ``GetStopReason``
     - Return one atomic ``stop_reason_`` load; independent of a ``GetState`` observation.

.. list-table:: Stored and Registered Continuations
   :header-rows: 1
   :widths: 34 66

   * - Continuation
     - Entry and behavior
   * - ``connection_timer_`` connect callback
     - Engine-scheduled ``TryConnect`` for initial or retry attempts; establishment view.
   * - ``connection_timer_`` registration callback
     - Engine-scheduled endpoint registration after a direct successful open; establishment view.
   * - ``disconnection_command_``
     - Engine-scheduled shared finalization after an off-callback-thread API stop.
   * - ``async_send_command_``
     - Acquire a unique send lock and drain the queue; arming and dispatch views.
   * - Endpoint ``input``
     - Process input, then gate fatal-result stopping and unregister; endpoint view.
   * - Endpoint ``ping``
     - Invoke the installed notification callback with an empty span, or ignore; endpoint view.
   * - Endpoint ``disconnect``
     - Invoke shared finalization under its stopping/context preconditions; endpoint view.
   * - Endpoint ``output``
     - Empty: no client output-handler body is installed.
   * - ``SendWaitReply`` completion callback
     - Store error or bounded reply copy, mark the local future ready, and notify the shared condition variable.

.. _client-connection-execution-context:

Execution Context and Lock Boundaries
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

API methods execute in their calling thread. Engine-scheduled command and
endpoint delivery are engine-owned boundaries; the common interface exposes
``IsOnCallbackThread`` rather than establishing a universal thread guarantee.
The contexts below describe local invocation and locks acquired by this code,
not every lock or frame inherited through a reentrant caller.

.. list-table:: User Callback Invocation
   :header-rows: 1
   :widths: 28 72

   * - Callback
     - Context and local synchronization
   * - State ``kStarting``
     - Synchronously in accepted start/restart, before connection scheduling. ``ProcessStateChange`` acquires no send/finalizer mutex for this state.
   * - State ``kReady``
     - In ``TryConnect`` after direct registration or registration scheduling; caller thread for a direct attempt, otherwise engine delivery.
   * - State ``kStopping``
     - In the winning API stop or failure path before cleanup dispatch; non-stopped publication takes no local finalizer mutex.
   * - State ``kStopped``
     - Shared finalization asserts callback context. With a callback, publication and invocation hold a locally retained context's recursive mutex; ``send_mutex_`` is released first.
   * - Successful reply callback
     - Input delivery after queue progress, outside ``send_mutex_``; a new request may already occupy the slot.
   * - Queued-request error callback
     - Queue dispatch outside ``send_mutex_``, with the optional slot kept engaged until callback return.
   * - Stop-cancellation reply callback
     - Shared finalization outside ``send_mutex_``; pending slot removed or queued command recycled before invocation.
   * - Notification/ping callback
     - Input or ping delivery; no send/finalizer mutex is acquired by the invocation itself. User reentrant calls have their own paths.

``ProcessStateChange`` publishes a non-stopped atomic state before calling the
installed callback, or delegates ``kStopped`` to the shared finalizer. Empty
state callbacks suppress invocation, not publication. ``GetState`` and
``GetStopReason`` are separate observations and do not produce a combined
snapshot; these descriptions do not claim lock-free atomics.

An API stop may finish cleanup inline on callback context or return after
queuing it elsewhere. The common engine interface explicitly describes
``CleanUpOwner`` as blocking; mutex acquisition, transport, and the synchronous
reply wait also have dependency or scheduling latency. No diagram turn order
creates a whole-path non-blocking guarantee or a finite completion deadline.

Callback nesting is observable: ready-callback stopping can invoke stopping and
stopped callbacks before the ready callback returns; stopped-callback restart
can invoke starting before the old finalizer releases its context. The local
stopped context does not by itself qualify lifetime of an outer state or
notification callback, its owned captures, or a continuation using the destroyed
connection. Those composition questions remain open, not new lifetime permissions.

The inventory closes the missing-operation and registered-entry descriptions
for the current dynamic baseline. The known establishment-stop, direct-error
rearming, empty-marker reply, and nested callback-lifetime findings remain
explicit review limits. Description completeness must not be read as their
resolution or as acceptance of the selected static class view.

Evidence and Review Gaps
~~~~~~~~~~~~~~~~~~~~~~~~

The design is grounded in ``client_connection.h``, ``client_connection.cpp``,
and ``i_client_connection.h``. Existing ``client_connection_test`` cases exercise
the connection through ``SharedResourceEngineMock``. The four focused callback
and competing-stop tests trace to ``ClientConnectionMaintainsStateMachine`` and
``ClientConnectionStateCallbackInvocation``. These are partial test links, not
a complete requirement-coverage claim or native QNX qualification.

Existing lifecycle cases cover initial state, stopping during an open attempt,
retryable and terminal open errors, eventual success after repeated retries,
and synchronous versus queued first connection. The explicit-stop cases cover
direct callback-thread cleanup and queued cleanup. ``ConnectedRestartdDoesNothing``
checks the non-stopped restart guard. These tests do not assert the exact retry
delay sequence. The focused cases above add selected callback-reentrancy and
competing-stop evidence, but do not qualify every lifecycle interleaving.

Existing send cases exercise size/readiness rejection, direct and ordered
one-way paths, truly asynchronous admission, and pool exhaustion. The
``SendWithCallback`` and ``SendWaitReply`` cases exercise direct errors, queued
admission/errors, cancellation, callback-thread rejection, and reply-buffer
bounds. They support selected paths in the send views, not every configuration
combination or concurrent send/reply/stop ordering.

Focused queue cases exercise repeated one-way draining, request transport-error
callbacks, command reservation during an unlocked send, direct-error rearming,
and stop/restart pool reuse. They support selected queue paths, not every
reentrant transport-error callback or assertion-enabled configuration. The
known rearm-precondition mismatch remains a separate finding.

Existing input cases exercise spurious ``EAGAIN``, peer closure, other receive
errors, unknown codes, a reply with a disengaged slot, notification delivery or
ignore behavior, and ping delivery or ignore behavior. The reply-buffer case
also exercises a non-empty completion callback. These are selected input paths,
not coverage of the engaged-empty-marker case, every queue-before-callback
ordering, or all API-stop/input-failure races.

With tooling 3.0.2, the unit build succeeds and class validation runs, reporting
type-representation findings. The view retains declaration-level names;
extraction expands or qualifies some field types differently. These findings are
non-blocking under the unit's development maturity, not zero-finding acceptance.

``SwitchToStopState`` removes each command from ``send_queue_`` before
returning it to ``send_pool_``. These lists share a single intrusive hook, so
inserting a still-linked command into the pool aborts. A client regression
fills the combined pool with messages and a queued reply callback, checks
``ENOBUFS`` at the bound, and verifies exactly-once ``EPIPE`` cancellation on
stop. After ``Restart``, it fills and stops the same pool again, checking that
queued capacity was restored.

This focused case does not establish all possible concurrent stop/restart
interleavings or whole-path memory guarantees.

Class consistency checks cover only the selected declarations. The dynamic
activity views elaborate connection establishment, the three send API paths,
queue admission/arming/dispatch, explicit stopping, and shared finalization,
plus input classification and endpoint reply/notify/ping/failure continuations.
They are not automatic behavioral comparisons with C++. The remaining
establishment and callback-combination gaps still need completeness review.
The direct-error rearm precondition and engagement-only reply guard remain
explicitly open.
Development maturity permits findings, including incomplete C++ extraction, so
a successful build alone is not design acceptance. Native QNX implementation
analysis remains subject to the existing tooling limitation; the compatibility
guards are unchanged.