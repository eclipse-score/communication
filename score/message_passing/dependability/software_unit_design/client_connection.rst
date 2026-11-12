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

Helper Ownership
~~~~~~~~~~~~~~~~

``ClientConnection`` retains the engine, public state and callbacks, endpoint
registration, connection attempts, and the embedded engine command entries.
Its send APIs delegate to ``ClientSendQueue``; input delegates reply handling;
stop orchestration delegates admission and retirement to ``ClientConnectionLifetime``.
The two helpers remain implementation classes in the existing client unit.

``ClientConnectionLifetime`` owns the descriptor, the shared mutex, operation count,
and retirement readiness. It references the connection's existing atomic state
and stop reason rather than creating another lifecycle state machine.
``ClientSendQueue`` owns fixed command storage, FIFO/pool membership, sequencing,
reply reservations, and the condition variable for the current blocking wait.
It alone has friend access to the lifetime helper's private lock interface.
Admission and queue reservation therefore stay in one locked transaction.

Transport and scheduling are fixed-capacity actor callables. Transport executes
through an admitted ``ClientConnectionLifetime::Operation`` outside the shared mutex.
Drain and retirement scheduling actors run under that mutex and must enqueue
work without reentering the helpers or invoking finalization inline. The
connection adapters retain the existing owner-keyed engine command entries.
No new dynamically allocated resource or scheduling thread is introduced.

``ClientConnectionLifetime::ExecuteTransaction`` is a lifetime-only extension point.
It admits one scoped operation and invokes an expected-result actor with the
live descriptor outside the mutex. It never inspects queue ordering, storage,
or the ordinary reply slot. A future explicitly selected native blocking
``SendWaitReply`` may use it; no engine transaction capability or user option
is implemented by this extraction. QNX can support that future optimization,
while the current Unix Domain transport cannot supply the same transaction.

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
mutex. Member destruction then invokes ``ClientSendQueue`` destruction, which
clears free-pool intrusive membership before destroying its owned storage.
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
  ``kStopping`` and dispatches finalization like ``Stop``: ``SwitchToStopState``
  runs directly in callback context, otherwise ``disconnection_command_`` is
  queued at ``TimePoint{}`` with owner ``this``. A synchronous first-open failure
  therefore does not run the shared finalizer on its user thread. The engine
  may complete cleanup before or after ``Start`` returns; observing
  ``kStopping`` at that return is not guaranteed. If another stop path already
  selected a reason, the failure path preserves it without duplicate cleanup.
  Neither outcome schedules another retry. A later explicit ``Restart`` is
  distinct from automatic retry.
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
  :caption: Queue helper: :ref:`client-send-queue-send-one-way`.

``ClientConnection::Send`` forwards the message and returns the helper result.
It does not acquire the send mutex or inspect reply reservations.

.. _client-send-queue-send-one-way:

Queue One-Way Send
~~~~~~~~~~~~~~~~~~

.. uml:: client_send_queue_send_activity.puml
  :caption: Admission: :ref:`client-connection-queue-admission`; operation lifetime: :ref:`client-connection-lifetime-operation`.

``Send`` checks message size before readiness, returning ``EMSGSIZE`` or
``EINVAL`` respectively. Readiness and the selected stop reason are checked
under ``lifetime_.mutex_`` to serialize admission with stopping. When neither
``fully_ordered`` nor ``truly_async`` is configured, a pending request does not
force this default one-way path into the library queue.
These steps now execute in ``ClientSendQueue::Send`` after facade delegation.

A ``truly_async`` send, or a ``fully_ordered`` send behind an engaged slot,
uses ``TryQueue``, which
owns queued admission and the guarded scheduling attempt. It returns ``false``
when the combined send pool has no free command; the API reports ``ENOBUFS``.
Successful admission copies the message into connection-owned storage and
returns before later transport dispatch.

Direct admission registers an active operation. A fully ordered direct send
also reserves an empty slot marker so concurrent ordered/request admission
queues instead of passing it. ``SendTransport`` releases the
mutex for IPC and reacquires it before returning. The sender releases only its
own marker, attempts queue arming, and finishes its active operation under the
mutex. The descriptor stays alive through that bookkeeping.

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
  :caption: Queue helper: :ref:`client-send-queue-send-with-callback`.

``ClientConnection::SendWithCallback`` transfers the callback to the queue
helper and returns its result without duplicating synchronization.

.. _client-send-queue-send-with-callback:

Queue Reply Request
~~~~~~~~~~~~~~~~~~~

.. uml:: client_send_queue_send_with_callback_activity.puml
  :caption: Admission: :ref:`client-connection-queue-admission`; operation lifetime: :ref:`client-connection-lifetime-operation`.

``SendWithCallback`` checks message size and readiness in the same order as
``Send``, checking readiness and the stop reason under ``lifetime_.mutex_``.
``truly_async`` or an engaged reply/drain
slot selects ``TryQueue``, including its guarded scheduling attempt.
Otherwise it registers an active operation and installs the callback and its
sender owner before direct ``REQUEST`` transport releases the mutex.
``fully_ordered`` does not select a different request admission path.
The queue helper owns these steps; the facade forwards the callback.

Local rejection returns ``EMSGSIZE``, ``EINVAL``, or ``ENOBUFS``. Direct transport
failure returns its error without invoking the callback if the sender still
owns the slot; it removes that reservation and rearms queued progress. If input
or stopping already took completion, the method returns acceptance instead of
a second local rejection and leaves any later callback untouched. An owned
callback remaining after successful transport stays pending, but its temporary
sender owner is cleared before the caller's callback parameter leaves scope.
After acceptance, the callback is held in either queued command storage or the
reply slot. Later queue transport, input processing, and stopping determine
reply/error delivery; this view does not imply receipt merely from API success.

.. _client-connection-send-wait-reply:

Sending and Waiting for a Reply
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: client_connection_send_wait_reply_activity.puml
  :caption: Queue helper: :ref:`client-send-queue-send-wait-reply`.

The facade rejects callback context with ``EAGAIN`` before delegating to the
current queue-backed implementation. A future native transaction path remains
separate from this dispatch.

.. _client-send-queue-send-wait-reply:

Queue Reply Wait
~~~~~~~~~~~~~~~~

.. uml:: client_send_queue_send_wait_reply_activity.puml
  :caption: Admission: :ref:`client-connection-queue-admission`; rearming: :ref:`client-connection-queue-arming`; operation lifetime: :ref:`client-connection-lifetime-operation`.

``SendWaitReply`` first rejects engine callback context with ``EAGAIN``. It then
delegates to ``ClientSendQueue::SendWaitReply``. Size and readiness checks
follow, so that callback-context rejection takes
precedence over ``EMSGSIZE`` or ``EINVAL``. The method creates a local result,
``NonAllocatingFuture``, and completion callback, then takes ``lifetime_.mutex_``.
It rechecks readiness and the stop reason under that mutex and registers an
active operation before attempting queued or direct admission.

An engaged reply/drain slot causes queued admission and can return ``ENOBUFS``.
With a free slot, the method installs its completion callback, releases the
mutex around direct ``REQUEST`` transport, then reacquires it. This direct
choice is independent of ``truly_async`` and ``fully_ordered``. Other callers
can admit queued work while transport is in progress because the slot remains
engaged even though the mutex is released.

If direct transport fails while this sender still owns the callback, the method
resets its reply slot and owner under the reacquired mutex, then calls
``ArmIfNeeded``. That helper only schedules progress
for a nonempty queue with a free slot. The method returns the transport error
without waiting; its local completion callback need not be invoked for that
direct error. This release-before-schedule sequence resolves the former
free-slot precondition mismatch. If completion already transferred to input or
stopping, the sender instead waits for that completion and cannot reset a later
request's slot. Failed queued admission also finishes its registered operation
before returning ``ENOBUFS``.

After successful direct transport or queued admission, the method releases the
mutex and waits on the local future's ready predicate. A callback result that
arrives before ``Wait`` is handled by that predicate. The completion callback
passes through an error, reports ``EMSGSIZE`` if the received reply exceeds the
caller-provided buffer, or copies the reply into that buffer and returns a span
trimmed to the copied size. Each result marks the future ready and notifies the
shared condition variable. The wait releases ``lifetime_.mutex_`` while blocked;
it does not apply an overall request/reply deadline.
After the future becomes ready, the caller reacquires the mutex, finishes the
active operation, and releases the mutex before returning its result. Stopping
cancels the future, so retirement does not require a successful application
reply; it does require this continuation to finish using the future and mutex.

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

.. uml:: client_send_queue_try_queue_activity.puml
  :caption: Guarded scheduling after admission: :ref:`client-connection-queue-arming`.

``TryQueue`` is called with ``lifetime_.mutex_`` already held. With no free
command it returns ``false``; the API caller reports ``ENOBUFS``. Otherwise it
removes the free-pool front, copies the message into that command's storage,
moves the callback, and appends the command to the send-queue tail. The storage
remains owned by ``storage_`` throughout this intrusive-list transfer.

Successful admission then calls ``ArmIfNeeded`` with the same lock
held. An occupied reply/drain slot is preserved; a free slot permits reserving
and scheduling queue progress. The helper does not validate lifecycle or
message size and does not send the message immediately. It stores the supplied
callback without a non-empty check; the dispatcher selects ``REQUEST`` for a
non-empty callback and ``SEND`` for an empty one. This is the observed dispatch
discriminator, not an additional callback API guarantee.

.. _client-connection-queue-arming:

Arming Queued Dispatch
~~~~~~~~~~~~~~~~~~~~~~

.. uml:: client_send_queue_arm_activity.puml
  :caption: Engine callback dispatch: :ref:`client-connection-send-queue-dispatch`.

``ArmIfNeeded`` requires ``lifetime_.mutex_`` to be held and does not release
it. The name describes deterministic conditional scheduling, not successful
arming on every return. Its early returns have different meanings:

* ``queue_`` is empty: there are no pending IPC messages to send, so no
  engine command is added.
* A stop reason is selected: no new drain is scheduled after stopping admission.
* ``waiting_for_reply_`` is engaged: an existing drain or request already owns
  progress. An empty callback marker represents a queued drain command or active
  queue processing or an active direct ordered send; a non-empty callback
  belongs to a request's completion. The
  helper preserves either state without scheduling another drain.

The two queues are distinct. ``queue_`` is the connection's FIFO of pending
IPC messages and callbacks. The engine's client-thread command queue schedules
execution of ``async_send_command_``, whose callback processes that FIFO. Slot
engagement is not a direct test of engine command-queue membership.

Only a nonempty message queue with no selected stop reason and a free slot
causes the helper to install an
empty callback marker, then enqueue ``async_send_command_`` at ``TimePoint{}``
with owner ``this``. The marker reserves queue processing, not a reply callback.
Repeated admission cannot overwrite a pending callback or schedule another drain
while an existing drain or request owns progress.

When the engine invokes the stored command callback, that callback acquires
``lifetime_.mutex_`` with a unique lock and calls
:ref:`client-connection-send-queue-dispatch`. The arming helper itself returns
without executing that continuation and without releasing the caller's lock.
Stopping removes connection-owned engine work through the shared finalizer.

``TryQueue`` attempts arming after every successful admission, and
direct ordered sends and direct-request error recovery attempt it after
releasing their own slot.
The helper decides whether work is needed rather than relying on duplicated
caller branches. The arming helper does not itself establish sender ownership;
its callers preserve that identity and the retirement fence separately.

.. _client-connection-send-queue-dispatch:

Queued Send Dispatch
~~~~~~~~~~~~~~~~~~~~

.. uml:: client_send_queue_drain_activity.puml
  :caption: Scoped operation: :ref:`client-connection-lifetime-operation`.

``DrainUnderLock`` receives an already-held ``lifetime_.mutex_`` lock.
It rejects dispatch after stopping has been selected and registers one active
operation for the complete drain continuation. The loop checks the stop reason
before taking another queued command.
It removes commands from the queue front, releasing the lock around transport
and request-error callbacks. A detached command remains reserved during
transport, then returns to the front of the free pool under the reacquired lock.
Admission order is FIFO; free-command reuse is LIFO.

A non-empty command callback selects ``REQUEST``. The callback moves into the
shared reply slot before transport, with the local callback object identifying
its sender. A successful send returns from draining with any still-pending
callback engaged, leaving later commands queued until reply processing
resumes progress. The command storage is already free at this point: the
pending callback, not the message-storage command, owns reply completion.

On request transport failure, the command is recycled before a still-owned
callback is invoked outside the mutex. Input or stopping may already have taken
completion; in that case dispatch does not invoke it again or reset a later
slot. Otherwise the optional slot remains engaged while that moved
callback executes, so reentrant request admission queues rather than starting
another direct request. After reacquiring the mutex, draining resets the slot
only if the same sender still owns it. An empty command callback instead selects ``SEND``; transport
errors are ignored, the command is recycled, and the temporary empty marker is
reset before draining continues. Neither transport-error branch itself claims
stopping or changes the connection state.

The helper finishes its active operation after transport, recycling, and error
callback return, then returns with the caller's lock held. An empty queue at entry leaves
the incoming slot unchanged; there is no unconditional final slot reset. The
engine's queued command and reply-input handling are the two callers of this
procedure. The queued command is described in
:ref:`client-connection-queue-arming`; the reply-dispatch boundary is described
in :ref:`client-connection-input-processing`.

.. _client-connection-input-processing:

Processing Received Input
~~~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: client_connection_process_input_event_activity.puml
  :caption: Reply helper: :ref:`client-send-queue-handle-reply`.

``ProcessInputEvent`` receives a code and message span through the engine.
``EAGAIN`` is treated as a spurious wake-up and returns ``kNone``. ``EPIPE``
returns ``kClosedByPeer``; every other receive error returns ``kIoError``.
These results classify input failure without themselves selecting the stop
reason, publishing a stopping state, or requesting endpoint cleanup.

For ``REPLY``, the facade delegates to ``sends_.HandleReply(message)`` and maps
a rejected reply to ``kIoError``. It does not manipulate the helper's mutex or
reply reservation itself. Notification and unknown-code handling remain local.

For ``NOTIFY``, a non-empty notification callback receives the message span;
otherwise the notification is ignored without requesting a stop. Unknown
message codes return ``kIoError``. Successful input processing returns ``kNone``.
The endpoint wrapper owns the resulting stop decision.

.. _client-send-queue-handle-reply:

Queue Reply Handling
~~~~~~~~~~~~~~~~~~~~

.. uml:: client_send_queue_handle_reply_activity.puml
  :caption: Queue progress: :ref:`client-connection-send-queue-dispatch`.

``ClientSendQueue::HandleReply`` takes ``lifetime_.mutex_``. An unoccupied slot
or empty callback marker returns ``false``. Otherwise it moves the callback locally,
resets the slot and sender owner, and
calls :ref:`client-connection-send-queue-dispatch` under the same unique lock.
Only after that procedure returns does it release the mutex and invoke the
completed request's callback. Dispatch can send another queued request and
occupy the slot before the earlier callback runs; this is not callback delivery
followed by queue draining. The local callback receives the existing message
span. ``SendWaitReply`` performs its own caller-buffer bound check and copy in
the completion callback described above.

The guard checks both engagement and callback non-emptiness. An empty drain or
ordered-send marker is not a valid pending request; its reply follows the same
IO-error stopping policy as a reply with a disengaged slot. The deterministic
marker test qualifies this common guard, not native backend scheduling.

.. _client-send-queue-cancellation:

Queue Cancellation
~~~~~~~~~~~~~~~~~~

.. uml:: client_send_queue_cancel_activity.puml
  :caption: Lifetime completion: :ref:`client-connection-lifetime-retirement`.

``CancelPending`` removes the pending reservation and recycles queued commands
under the shared mutex, releasing it for non-empty callbacks receiving ``EPIPE``.
It neither closes the descriptor nor publishes stopped. The facade has already
closed admission and marks cancellation complete only after this method returns.
Detached active command storage remains reserved until its transport terminates.

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
``TrySetStopReason`` takes ``lifetime_.mutex_`` around reason selection. Admission
checks both readiness and that reason under the same mutex, so a winning stop
closes send admission before the later stopping-state callback is invoked.

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
  :caption: Queue cancellation: :ref:`client-send-queue-cancellation`; descriptor retirement: :ref:`client-connection-lifetime-retirement`.

``SwitchToStopState`` expects ``kStopping``, a selected non-``kNone`` stop reason,
and engine callback context; the implementation asserts these conditions in
debug builds. The procedure itself neither selects nor resets the reason.
These entry conditions do not establish that every caller interleaving satisfies
them, including the existing establishment review gaps.

On its initial entry, ``SwitchToStopState`` disables the endpoint's disconnect
callback and cleans up engine registrations owned by the connection, with the
shared mutex released around ``CleanUpOwner``. It then calls
``ClientSendQueue::CancelPending`` to remove the reply reservation and recycle
queued commands. Non-empty callbacks receive ``EPIPE`` outside the mutex.
``ClientConnectionLifetime::CancellationFinished`` marks retirement pending only
after these cancellations finish.
If registered send operations remain, the procedure returns in ``kStopping``
without closing the descriptor. The last terminating operation schedules
``disconnection_command_``; its entry skips the already-completed owner cleanup
and cancellation phase. ``FinishStopState`` calls
``ClientConnectionLifetime::RetireDescriptor`` to detach the descriptor under the
mutex, closes it outside the mutex, then publishes ``kStopped``. The callback-context mechanism below
permits destruction or restart from that stopped-state callback.

.. _client-connection-send-retirement:
.. _client-connection-lifetime-operation:

Send Retirement
~~~~~~~~~~~~~~~

.. uml:: client_connection_lifetime_operation_activity.puml
  :caption: Descriptor retirement: :ref:`client-connection-lifetime-retirement`.

``ClientConnectionLifetime::Operation`` admits work under the shared mutex only when
the public state is ready and no stop reason is selected. Its non-copyable,
non-movable scope registers one active operation, reacquires the lock on scope
exit if necessary, and decrements the count exactly once. Manual counter
balancing is no longer scattered through the facade or queue error branches.
Direct sends, the entire blocking-wait continuation, and complete queue-drain
continuations retain an operation through their last use of borrowed resources.

``Operation::Execute`` releases the owned unique lock, invokes an actor with
the live descriptor, and reacquires the lock before returning its result.
``ClientSendQueue::SendTransport`` supplies the transport actor. Descriptor
retirement requires zero operations, so the descriptor cannot be closed or
replaced while an admitted actor or its continuation still uses it. The
operation, not the integer descriptor argument, is the lifetime protection.

``reply_slot_owner_`` identifies a live local ``ReplySlotToken`` while its sender
can still process a transport result. Its identity is independent of callback
moves; it is not an owned callback or a dereferenced stack pointer. Input and
stop cancellation clear it when taking the callback. A sender may reset only
its own slot; when a callback outlives a successful direct or queued send, the
temporary owner is cleared before that local token leaves scope. Blocking
requests keep the token alive through their reply wait. Empty ordered-send
markers use the same ownership rule, while scheduled drain markers have no
active sender owner.

``ClientConnectionLifetime::Operation::~Operation`` decrements the counter under the held mutex.
The last operation schedules the embedded disconnection command only if the
initial cancellation phase is complete. Otherwise that initial phase observes
the zero count itself and finalizes directly. ``RetireDescriptor`` detaches the
descriptor and clears the pending flag under the mutex. ``ClientConnection``
closes that returned descriptor outside the mutex and publishes stopped.
No caller polls or waits
for quiescence inside ``Restart``; a restart during ``kStopping`` is rejected,
including with ``sync_first_connect``.

This is a descriptor-safety fence, not an IPC interruption mechanism. QNX
transport or ``close`` can remain blocked; signal or ``SIGEV_UNBLOCK`` handling
is not implemented or qualified here. A stuck operation can keep the connection
in ``kStopping`` indefinitely. The establishment-stop and nested outer-callback
lifetime findings are separate from the admitted-send retirement boundary.

.. _client-connection-lifetime-retirement:

Descriptor Retirement
~~~~~~~~~~~~~~~~~~~~~

.. uml:: client_connection_lifetime_retirement_activity.puml
  :caption: Scoped operation lifetime: :ref:`client-connection-lifetime-operation`.

This helper view separates stop-reason selection, descriptor installation,
retirement readiness, cancellation completion, and quiescent detachment.
Each method owns its mutex boundary. The facade receives a detached descriptor
only after the mutex is released, then closes it before publishing stopped.
The include-only ``client_connection_lifetime.cpp`` compiles the header without
preceding includes in the client library on both supported toolchains.

.. _client-connection-lifetime-transaction:

Native Transaction Hook
~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: client_connection_lifetime_transaction_activity.puml
  :caption: Scoped operation lifetime: :ref:`client-connection-lifetime-operation`.

``ExecuteTransaction`` uses admission and descriptor protection without FIFO
or ordinary reply-slot sequencing. Its expected-result actor runs outside the
mutex. This is a compatibility hook for a future explicitly selected blocking
native transaction, not an implemented engine capability or configuration option.

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
* ``storage_`` owns the fixed set of command objects and their message
  storage. The intrusive ``pool_`` and ``queue_`` are complementary
  free/queued lists over that storage, not separate allocations:
  ``pool_`` contains commands available for admission, while
  ``queue_`` contains commands reserved for accepted sends awaiting
  transport. Admission moves one command from the free pool to the queue;
  queued admission fails with ``ENOBUFS`` when no free command remains.
* The lists are disjoint, but a command removed for dispatch temporarily
  belongs to neither list and remains in use until the transport call finishes.
  Queue draining then returns it to the free pool; stopping returns commands
  after removing them from the queue. A command can become free before its
  reply arrives, because the pending reply callback is held separately in
  ``waiting_for_reply_``.
* ``waiting_for_reply_`` serializes request/reply progress. An engaged optional
  can also contain an empty callback as a marker for scheduled/active queue
  draining or a direct ordered send;
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

``lifetime_.mutex_`` protects the send pool, queue, and reply slot. Queue draining
and all direct send paths release it for transport. Descriptor closure and
reply callbacks also run outside this mutex. Admission and stop-reason selection
share the mutex; atomic state observations remain independent of that compound
admission transaction. :ref:`client-connection-send-retirement` describes the
counter, slot owner, and finalization boundary. ``SendWaitReply`` uses the same mutex and
``reply_condition_`` with ``NonAllocatingFuture``; it rejects invocation from the
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

This inventory accounts for the facade and extracted helpers in the existing
``client_connection`` unit, together with the registered continuations
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
   * - ``ClientSendQueue::SendCommand::SendCommand``
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
     - :ref:`client-connection-switch-to-stop-state`: initial owner cleanup/cancellation, deferred or immediate finalization.
   * - ``ClientConnectionLifetime::Operation::~Operation``
     - :ref:`client-connection-send-retirement`: decrement the operation count and schedule finalization on the last pending completion.
   * - ``FinishStopState``
     - :ref:`client-connection-send-retirement`: detach the descriptor under lock, close outside it, and publish stopped.
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
   * - ``ClientSendQueue::TryQueue``
     - :ref:`client-connection-queue-admission`: free-pool reservation, FIFO admission, and guarded scheduling attempt.
   * - ``ClientSendQueue::ArmIfNeeded``
     - :ref:`client-connection-queue-arming`: distinguish no messages from owned progress, then conditionally schedule a client-thread drain command.
   * - ``ClientSendQueue::DrainUnderLock``
     - :ref:`client-connection-send-queue-dispatch`: transport, recycling, slot transitions, and error callbacks.
   * - ``ClientSendQueue::SendTransport``
     - :ref:`client-connection-send-retirement`: release the owned unique lock for IPC, then reacquire it before returning.
   * - ``ProcessInputEvent``
     - :ref:`client-connection-input-processing`: receive classification and reply/notification dispatch.
   * - ``IsInCallback``
     - Delegate to ``engine_->IsOnCallbackThread()``; this helper does not create a thread or change connection state.
   * - ``GetState``
     - Return one atomic ``state_`` load; no engine work or explicit mutex acquisition.
   * - ``GetStopReason``
     - Return one atomic ``stop_reason_`` load; independent of a ``GetState`` observation.
   * - ``ScheduleSendDrain`` / ``ScheduleStopFinalization``
     - Facade actor adapters: enqueue the embedded drain or disconnection command with owner ``this``.
   * - ``ClientSendQueue`` / ``~ClientSendQueue``
     - Allocate and reserve combined storage during construction; clear free-list membership during destruction.
   * - ``ClientSendQueue::Send`` / ``SendWithCallback`` / ``SendWaitReply``
     - Execute the three send views after facade validation of callback context for the blocking API.
   * - ``ClientSendQueue::HandleReply`` / ``Drain`` / ``CancelPending``
     - Lock privately for reply transfer, scheduled draining, or cancellation; transport and user callback invocation release that lock.
   * - ``ClientConnectionLifetime`` / ``InstallDescriptor``
     - Initialize stopped resource state; install a new descriptor under lock with no active operations.
   * - ``ClientConnectionLifetime::IsAccepting`` / ``TrySetStopReason``
     - Inspect the existing atomic state/reason; serialize stop selection with operation admission.
   * - ``ClientConnectionLifetime::RetirementPending`` / ``CancellationFinished`` / ``RetireDescriptor``
     - Inspect retirement state, mark cancellation complete, and detach a quiescent descriptor under the shared mutex.
   * - ``ClientConnectionLifetime::Lock`` / ``Mutex``
     - Private synchronization access shared only with the queue helper.
   * - ``ClientConnectionLifetime::Operation`` / ``operator bool`` / ``Execute``
     - Admit and register one scope, report admission, or execute an actor outside the lock while retaining that registration.
   * - ``ClientConnectionLifetime::ExecuteTransaction``
     - Lifetime-only expected-result actor execution; rejected admission returns ``EINVAL`` without invoking the actor.

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
     - Engine-scheduled initial stop cleanup, terminal open failure, or finalization after the last admitted send terminates.
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
   * - Helper transport and scheduling actors
     - Fixed-capacity facade captures bridge transport, drain scheduling, and quiescent-finalization scheduling; their mutex contracts are stated above.

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
     - Shared finalization asserts callback context. With a callback, publication and invocation hold a locally retained context's recursive mutex; ``lifetime_.mutex_`` is released first.
   * - Successful reply callback
     - Input delivery after queue progress, outside ``lifetime_.mutex_``; a new request may already occupy the slot.
   * - Queued-request error callback
     - Queue dispatch outside ``lifetime_.mutex_``; a still-owned callback is moved locally, with its reservation retained until return or cancellation.
   * - Stop-cancellation reply callback
     - Shared finalization outside ``lifetime_.mutex_``; pending slot removed or queued command recycled before invocation.
   * - Notification/ping callback
     - Input or ping delivery; no send/finalizer mutex is acquired by the invocation itself. User reentrant calls have their own paths.

``ProcessStateChange`` publishes a non-stopped atomic state before calling the
installed callback, or delegates ``kStopped`` to the shared finalizer. Empty
state callbacks suppress invocation, not publication. ``GetState`` and
``GetStopReason`` are separate observations and do not produce a combined
snapshot; these descriptions do not claim lock-free atomics.

API stopping and terminal-open failure may finish cleanup inline on callback
context or queue it elsewhere. Queued cleanup can finish before or after the
initiating call returns; completion and callback order are stronger evidence
than a transient ``kStopping`` snapshot. The common engine interface explicitly describes
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
for the current dynamic baseline. The known establishment-stop and nested
callback-lifetime findings remain explicit review limits. Send retirement and
the non-empty reply guard now have focused common-client regressions, not
complete native scheduling or liveness qualification. Description completeness
is not acceptance of the selected static class view.

Evidence and Review Gaps
~~~~~~~~~~~~~~~~~~~~~~~~

The design is grounded in ``client_connection.h``, ``client_connection.cpp``,
``client_connection_lifetime.h``, ``client_send_queue.h``, ``client_send_queue.cpp``,
and ``i_client_connection.h``. Existing ``client_connection_test`` cases exercise
the connection through ``SharedResourceEngineMock``. The four focused callback
and competing-stop tests trace to ``ClientConnectionMaintainsStateMachine`` and
``ClientConnectionStateCallbackInvocation``. These are partial test links, not
a complete requirement-coverage claim or native QNX qualification.
The helper-only cases have moved out of that facade fixture into dedicated
``client_connection_lifetime_test`` and ``client_send_queue_test`` targets.
The former has 8 actor-driven cases for admission, first-stop ownership,
descriptor propagation, successful/failed retirement, nested operation release,
and descriptor reuse after quiescence. The latter has 14 cases for validation,
direct transport, FIFO copying and reuse, ordered reply progress, local/queued
errors, empty markers, cancellation reentrancy, blocking replies and buffer
bounds, cancellation during delayed transport, and native-style transaction
bypass without consuming the ordinary reply. These implementation tests use
Given/When/Then comments, not Lobster IDs or additional SEooC units. All 22
helper cases and the 58 facade cases pass in normal and debug builds. QNX test
sources compile, but native execution and engine transactions remain unqualified.

Existing lifecycle cases cover initial state, stopping during an open attempt,
retryable and terminal open errors, eventual success after repeated retries,
and synchronous versus queued first connection. The explicit-stop cases cover
direct callback-thread cleanup and queued cleanup. ``ConnectedRestartdDoesNothing``
checks the non-stopped restart guard. These tests do not assert the exact retry
delay sequence. The focused cases above add selected callback-reentrancy and
competing-stop evidence, but do not qualify every lifecycle interleaving.

``TryingToSyncConnectOnceStoppingOnHardError`` checks terminal errors with queued
cleanup and ordered starting/stopping/stopped callbacks. It observes final
completion after executing the captured engine command, not ``kStopping``
immediately after ``Start``. ``SyncConnectFailureMayFinalizeBeforeStartReturns``
runs that command on a worker thread before ``Start`` returns and checks engine
context, cleanup, and the same callback sequence. Both pass with debug assertions
enabled; they do not remove the separate establishment-cancellation findings.

Existing send cases exercise size/readiness rejection, direct and ordered
one-way paths, truly asynchronous admission, and pool exhaustion. The
``SendWithCallback`` and ``SendWaitReply`` cases exercise direct errors, queued
admission/errors, cancellation, callback-thread rejection, and reply-buffer
bounds. They support selected paths in the send views, not every configuration
combination or concurrent send/reply/stop ordering.

Focused queue cases exercise repeated one-way draining, request transport-error
callbacks, command reservation during an unlocked send, direct-error rearming,
and stop/restart pool reuse. ``QueuedAdmissionsArmOnceAndPreservePendingReply``
checks that admissions behind a scheduled drain or pending request do not
schedule another command or overwrite its callback, and that idle admission
can arm again after draining. Direct-error empty-queue, one-way queue, and
queued-request callback controls also pass with Futurecpp debug assertions
explicitly enabled. These are selected paths, not every reentrant callback or
lifecycle interleaving. ``SendWaitReplyStopDefersCloseAndRestartUntilTransportReturns``
checks the direct-request fence and synchronous-first restart rejection.
``SendStopDefersCloseUntilTransportReturns`` covers both direct one-way modes.
``DirectSendsAllowQueuedAdmissionWhileTransportIsDelayed`` holds direct ordered
and callback IPC behind explicit promises and checks that another thread can
admit queued work before release. ``SendWithCallbackStopCompletesOnceBeforeTransportReturns``
covers successful and failed late transport returns after cancellation.
``QueuedRequestStopPreservesActiveSendBufferAndPoolCapacity`` checks detached
storage lifetime and stop/restart reuse. ``SendWithCallbackEarlyReplyPreservesLaterRequestOnLateTransportError``
checks callback-owned capture survival and exactly-once later cancellation.
``CloseRunsOutsideSendMutexAndBeforeStoppedPublication`` checks responsive
admission rejection during close and publication only afterward. The obsolete
disabled restart-during-active-transport diagnostic has been replaced by these
positive assertions; no disabled client cases remain.

Existing input cases exercise spurious ``EAGAIN``, peer closure, other receive
errors, unknown codes, a reply with a disengaged slot, notification delivery or
ignore behavior, and ping delivery or ignore behavior. The reply-buffer case
also exercises a non-empty completion callback. These are selected input paths,
not every queue-before-callback ordering or all API-stop/input-failure races.
``ReplyDuringDrainMarkerStopsWithoutInvokingEmptyCallback`` explicitly leaves
the armed command pending and qualifies the engaged-empty marker's IO-error
stopping behavior without invoking an empty callback.

With tooling 3.0.2, the unit build succeeds and class validation runs, reporting
12 type-representation findings, including expansion of the new sender-owner
callback pointer in addition to the previous 11. The view retains declaration-level names;
extraction expands or qualifies some field types differently. These findings are
non-blocking under the unit's development maturity, not zero-finding acceptance.

``SwitchToStopState`` removes each command from ``queue_`` before
returning it to ``pool_``. These lists share a single intrusive hook, so
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
The admitted-send retirement fence and non-empty reply guard have focused
regressions; the direct-error rearm precondition is resolved by guarded
scheduling and owner-aware release-before-schedule recovery. Native QNX IPC
interruption and finite stopping latency remain unqualified.
Development maturity permits findings, including incomplete C++ extraction, so
a successful build alone is not design acceptance. Native QNX implementation
analysis remains subject to the existing tooling limitation; the compatibility
guards are unchanged.