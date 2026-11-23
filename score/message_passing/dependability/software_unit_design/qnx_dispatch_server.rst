QNX Dispatch Server Unit Design
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Scope and Responsibility
~~~~~~~~~~~~~~~~~~~~~~~~

The existing ``qnx_dispatch_server`` unit contains
``score::message_passing::detail::QnxDispatchServer``, its nested
``ServerConnection``, and the connection's private ``SendMessage`` storage
element. The factory remains a separate unit even though its implementation
shares a C++ library with the server.

The server implements listening and application admission; each accepted
connection implements request dispatch and outgoing reply/notification storage.
The views describe the current implementation rather than adding public
contracts or new architectural boundaries. Public interfaces remain
``IServer`` and ``IServerConnection``.

Native attachment, OCB adapters and dispatch-loop resources belong to
:ref:`qnx-dispatch-engine-resource-manager` and
:ref:`qnx-dispatch-engine-runner-dispatch`. Those definitions are referenced,
not duplicated as server-owned workers or native receive buffers.

.. _qnx-dispatch-server-ownership:

Server Ownership
~~~~~~~~~~~~~~~~

.. uml:: qnx_dispatch_server_class_diagram.puml
   :caption: Per-connection storage: :ref:`qnx-dispatch-server-connection-storage`.

The class views select owned state and native hooks rather than repeat every
public-interface declaration. The server's native base retains a shared
engine and its attachment ID. Construction copies the service identifier
using that engine's memory resource, copies protocol size limits and
``ServerConfig``, and initializes empty application callbacks.

There is no server-owned connection collection in this class. Pending
connections have a local unique owner in ``ProcessConnect``; acceptance moves
that ownership into the connection itself. Native OCB binding supplies the
connection's later server lookup. Destroying a factory or stopping one listener
does not itself mean that the shared engine is destroyed.

.. _qnx-dispatch-server-connection-storage:

Connection Storage and Lifetime
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: qnx_dispatch_server_connection_class_diagram.puml
   :caption: Admission: :ref:`qnx-dispatch-server-acceptance`; destruction: :ref:`qnx-dispatch-server-destruction`.

Construction copies ``ClientIdentity`` and zero-initializes the native
connection base, leaving ``user_data_`` and ``self_`` empty. It uses the engine
memory resource through the supplied server reference to construct:

* One embedded ``reply_message_``, with code ``REPLY`` and payload capacity
  reserved from ``max_reply_size``.
* ``max_queued_notifies`` notification elements in ``notify_storage_``, each
  with code ``NOTIFY`` and reserved ``max_notify_size`` payload capacity.
* An intrusive free pool containing those notification elements and an empty
  outgoing FIFO.

These are outgoing buffers, distinct from the native receive workspace in
the engine's server dispatch context. The shared FIFO can contain the single
reply element and queued notification elements. The notification pool never
contains the reply element; free and queued notification membership are
disjoint and use the same intrusive hook.

``AcceptConnection`` first moves the admission result into ``user_data_`` and
then moves the local unique owner into ``self_``. ``ProcessDisconnect`` breaks
that self-ownership with ``self_.reset()``. A callback receives a borrowed
connection interface, not ownership of that object.

``UserData`` is a variant of ``void*``, ``std::uintptr_t`` and an owning handler
pointer. Message and disconnect routing inspect its current alternative on
each call; ``GetUserData`` returns a mutable reference, not a stored mode flag
or a copy. It dereferences the optional without an engagement check.
``GetClientIdentity`` returns a const reference to the stored identity.

.. _qnx-dispatch-server-listening:

Listening Lifecycle
~~~~~~~~~~~~~~~~~~~~

.. uml:: qnx_dispatch_server_listening_activity.puml
   :caption: Native admission: :ref:`qnx-dispatch-server-acceptance`.

``StartListening`` replaces all four callback members before formatting the
identifier as a ``QnxResourcePath`` and delegating attachment to the native
base. Resource-path preconditions are not converted into a new expected-error
result by this method. The method returns the engine's attachment result;
there is no additional listening-state flag or callback-validation branch.

``StopListening`` delegates to the base's ``Stop``. The server destructor also
calls that base procedure directly, before member destruction. Neither path
explicitly resets callback members; they are replaced by another start or
destroyed with the server object.

The public ``IServer`` contract requires referenced callback state to remain
valid until stopping or destruction returns. It permits stopping to block and
forbids calling ``StopListening`` from server callbacks. These caller-side
operations are distinct from native worker dispatch; no finite completion
deadline is introduced here.

.. _qnx-dispatch-server-acceptance:

Native Connection Acceptance
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: qnx_dispatch_server_connect_activity.puml
   :caption: Early notification admission: :ref:`qnx-dispatch-server-notify`; rejection destruction: :ref:`qnx-dispatch-server-destruction`.

In the ordinary native request path, the engine's ``io_open`` calls
``ProcessConnect`` on its server worker with native attributes locked.
``ConnectClientInfo`` uses the request's server connection ID. Failure logs an
error and returns ``EINVAL``. The stored identity uses the request PID and the
returned effective UID/GID.

The server allocates and constructs a pending connection, then invokes
``connect_callback_`` before attaching its OCB. The connection's identity and
outgoing storage already exist, but accepted ``UserData`` and self-ownership
do not. In particular, queued notification admission with inactive events
does not require native server lookup.

An application admission error is logged and mapped to ``EACCES``, rather than
returning the callback's original OS error. Successful application admission
is followed by ``AttachConnection``. An attachment error is logged and
returned unchanged. In either failure branch, the local owner destroys the
pending connection. Only successful native attachment is followed by
``AcceptConnection`` and ``EOK``.

The code adds no allocation or application-callback recovery branch around
this ``noexcept`` procedure. Native binding-dependent access and optional
``UserData`` engagement remain distinct from receiving the interface argument
during admission.

.. _qnx-dispatch-server-input:

Request and One-Way Input
~~~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: qnx_dispatch_server_input_activity.puml
   :caption: Replies: :ref:`qnx-dispatch-server-reply`; notifications: :ref:`qnx-dispatch-server-notify`.

``ProcessInput`` receives the code and a borrowed payload from native engine
dispatch. ``REQUEST`` selects ``OnMessageSentWithReply`` on a stored handler,
or ``sent_with_reply_callback_`` on the server. ``SEND`` selects
``OnMessageSent`` or ``sent_callback_``. Routing depends on the variant type;
it adds no null-handler or empty-callable guard.

The result is the selected expected-result's ``has_value()``. An unknown code
returns false. Neither branch itself requests disconnection or sends a reply.
The engine's current ``io_write`` discards this boolean before acknowledging
the native write, so false is not an implemented connection-drop mechanism.
An application request callback returning success is also distinct from
calling ``Reply``.

The payload is not copied into a server-owned incoming queue. This class
stores ``max_request_size_`` from ``max_send_size`` but does not use it to
check incoming payloads.

.. _qnx-dispatch-server-reply:

Reply Admission
~~~~~~~~~~~~~~~~

.. uml:: qnx_dispatch_server_reply_activity.puml
   :caption: Native read/drain: :ref:`qnx-dispatch-server-read`.

``Reply`` recovers the server through native binding and rejects a payload
above ``max_reply_size_`` with ``EMSGSIZE``. It copies into the embedded reply
buffer before acquiring ``send_mutex_``. Under the mutex it appends that reply
element to the FIFO and attempts select-event delivery, without checking the
event's active state or the delivery result.

There is one reply element, not a reply pool or per-request record allocation.
This method adds no already-linked check before appending it. Success reports
local queuing, not confirmed event delivery or client receipt.

.. _qnx-dispatch-server-notify:

Notification Admission
~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: qnx_dispatch_server_notify_activity.puml
   :caption: Native read/drain: :ref:`qnx-dispatch-server-read`; admission context: :ref:`qnx-dispatch-server-acceptance`.

``Notify`` rejects an oversized payload with ``EMSGSIZE``, then takes
``send_mutex_``. An empty payload with an active ping event attempts direct
ping delivery and returns success without consuming a notification element.
This path bypasses the outgoing FIFO.

Otherwise the method returns ``ENOBUFS`` when the pool is empty. With a free
record, it removes that record from the pool, copies the payload and appends
it to the FIFO. An active select event triggers an unchecked delivery attempt;
without one, the record simply remains queued. The engine's later event
registration can announce already-queued data through ``HasSomethingToRead``.

Thus a notification, including an empty one, can queue before OCB/event
registration when storage is available. If no record is available and no
active-ping fast path applies, even an empty notification returns ``ENOBUFS``.
FIFO ordering describes queued records, not the bypassing ping path.

.. _qnx-dispatch-server-read:

Native Read and Queue Retirement
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: qnx_dispatch_server_read_activity.puml
   :caption: Reply admission: :ref:`qnx-dispatch-server-reply`; notification admission: :ref:`qnx-dispatch-server-notify`.

``HasSomethingToRead`` locks ``send_mutex_`` and returns whether the FIFO is
non-empty. ``ProcessReadRequest`` holds that same mutex for the full native
reply attempt and record retirement. It returns ``EAGAIN`` for an empty queue.

For a queued record it supplies two I/O vectors: the code byte and payload.
It computes their total size and casts it to ``std::int32_t`` for the native
read-byte count without a further range or requested-read-size check here.
After ``MsgReplyv``, whose result is ignored, it removes the whole record.
A notification element returns to the front of the free pool; the embedded
reply element remains outside both lists. The method returns
``_RESMGR_NOREPLY`` and adds no partial-reply recovery or native-error retry.

.. _qnx-dispatch-server-destruction:

Disconnection and Destruction
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. uml:: qnx_dispatch_server_destroy_connection_activity.puml
   :caption: Listener shutdown: :ref:`qnx-dispatch-server-listening`.

``ProcessDisconnect`` resets ``self_``, synchronously destroying an accepted
connection without further member access in that wrapper. Admission failures
also destroy their pending connection through the local unique owner.

The destructor first calls ``GetQnxDispatchServer`` through the native OCB
binding, before checking optional ``UserData``. If data is engaged, it invokes
the current handler's ``OnDisconnect`` or the server's stored disconnect
callback. It then clears FIFO and free-pool membership before member
destruction releases outgoing storage and owned user data. It does not take
``send_mutex_`` or flush queued records to the peer.

The public ``RequestDisconnect`` implementation is a separate empty method.
It does not invoke ``ProcessDisconnect``, schedule a native close, or send an
event. The two names must not be interpreted as the same control path.

.. _qnx-dispatch-server-execution-context:

Execution Context and Resource Bounds
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``IServer`` specifies unspecified callback threads with serialization for one
session. The ordinary engine path shown here uses its server worker; that
implementation path does not strengthen the public thread contract.

.. list-table:: Context and Storage Boundaries
   :header-rows: 1
   :widths: 32 68

   * - Entry or resource
     - Implemented boundary
   * - Listening calls and server destruction
     - Execute in their caller and delegate native attachment/detachment.
   * - Ordinary native open/input/read dispatch
     - Engine server-worker path; application callbacks execute inline where
       the selected handler or callable is invoked.
   * - ``Reply`` and ``Notify``
     - Execute in their callers. The diagrams show their distinct mutex
       coverage; neither adds a scheduling handoff.
   * - Connection destruction
     - Executes in its owner-release/native closure context. Application
       disconnect delivery precedes list clearing and member destruction.
   * - Reply storage
     - One owned element, reserved to ``max_reply_size``; no local pending-
       reply membership guard.
   * - Notification storage
     - Fixed element count from ``max_queued_notifies``, each reserved to
       ``max_notify_size``; empty active-ping delivery consumes no element.
   * - ``pre_alloc_connections`` and server ``max_queued_sends``
     - Do not control connection preallocation or an incoming queue in this
       implementation. Connections are allocated during admission.
   * - Native receive workspace
     - Engine dispatch-context allocation, separate from these outgoing buffers.

.. _qnx-dispatch-server-operation-inventory:

Operation Inventory
~~~~~~~~~~~~~~~~~~~

.. list-table:: Implemented Operations
   :header-rows: 1
   :widths: 40 60

   * - Operation
     - Description
   * - Server constructor/destructor; ``StartListening``, ``StopListening``
     - Configuration/shared-engine ownership and caller-side listening lifecycle.
   * - ``ProcessConnect``; connection constructor; ``AcceptConnection``
     - Native identity, pending allocation, admission, OCB binding and
       accepted self-ownership.
   * - ``GetClientIdentity``, ``GetUserData``, ``GetQnxDispatchServer``
     - References to stored identity/current optional data and native-bound
       server lookup, without new copies or validation.
   * - ``ProcessInput``
     - Current-variant routing for ``SEND``/``REQUEST`` and a boolean result
       currently discarded by the engine.
   * - ``Reply``, ``Notify``
     - Size checks, copying, FIFO/pool ownership and event-attempt paths.
   * - ``HasSomethingToRead``, ``ProcessReadRequest``
     - Locked readiness query, native reply attempt and record retirement.
   * - ``ProcessDisconnect``; connection destructor
     - Self-owner release, optional disconnect callback/handler delivery and
       intrusive-list cleanup before member destruction.
   * - ``RequestDisconnect``
     - Empty body; no native or queued disconnection action.
   * - ``SendMessage`` constructor
     - Initializes its intrusive element, allocator-backed vector and code;
       connection construction assigns protocol codes and reserves payloads.

The inventory describes implemented control flow and resource ownership.
It adds no callback validation, transport-delivery guarantee, thread
serialization beyond the public contract, or bounded completion deadline.
