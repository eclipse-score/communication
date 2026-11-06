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
complete class or behavioral specification.

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

Evidence and Review Gaps
~~~~~~~~~~~~~~~~~~~~~~~~

The design is grounded in ``client_connection.h``, ``client_connection.cpp``,
and ``i_client_connection.h``. Existing ``client_connection_test`` cases exercise
the connection through ``SharedResourceEngineMock``. No new requirement-to-test
coverage is claimed.

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

Class consistency checks cover only the selected declarations. Dynamic
lifecycle, retry, send/reply, and failure paths are follow-up design increments,
not behavior proven by this static view. Development maturity permits findings,
including incomplete C++ extraction, so a successful build alone is not design
acceptance. Native QNX implementation analysis remains subject to the existing
tooling limitation; the compatibility guards are unchanged.