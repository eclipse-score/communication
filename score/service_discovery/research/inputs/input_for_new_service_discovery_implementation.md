# Input for new Service Discovery Implementation

> Plain-text rendering of `Input for new Service Discovery Implementation.docx`, extracted
> verbatim (paragraph-by-paragraph) for greppability/diffability. The `.docx` in this same
> directory is the authoritative original; this file is a rendering of it, not a rewording.

The new SD shall have a central Daemon process, which holds the central registry about all
currently registered service instances.

Communication between the clients using the SD and the central Daemon shall be done based on the
existing message-passing abstraction (where we have implementations specifically for Linux and
QNX).

The "API" which the SD (central daemon) provides to the clients contains:

- Register a new service-instance. This is semantically an RPC and shall be realized via
  message-passing SendWaitReply-mechanism.
- Unregister a service-instance previously registered. Also, RPC semantic (SendWaitReply)
- FindService: A one-shot synchronous lookup for a service-instance of a given service-type (can be
  with explicit instance-id or any-id). Also, RPC semantic (SendWaitReply)
- StartFindService: Register a search for a given service-instance (with specific instance or
  any-instance). Also, RPC semantic (SendWaitReply). The reply to this RPC contains:
  - Current result of matching service-instances.
  - A "handle" to allow stopping of this search.
  - StartFindService (see above) will inform the caller via message-passing notification
    mechanism about any change in service-instance availability related to the search.
- StopFindService: This allows stopping a search started via StartFindService. Also, RPC semantic
  (SendWaitReply)

An score::mw::com application shall connect on message-passing level at startup with the daemon
and shall disconnect on shutdown. I.e. any connection-loss seen by the daemon shall be interpreted
as this score::mw::com application has been shutdown (crash or graceful shutdown)

Additionally, to the "obvious" SD related API above, we want the SD/daemon to also manage our
current mechanisms to:

- Make sure, that only one instance of a service-instance is created at a time (currently solved
  via flock based ServiceExistanceMarkerFile). This is semantically an exclusive-lock solution
  (normal mutex) on a service-instance.
- Monitor, whether a certain service-instance (mainly its shared-memory objects) is currently in
  use by applications in the role as client/consumer of this service-instance. (currently solved
  via flock based ServiceUsageMarkerFile). This is semantically a shared-mutex solution on a
  service-instance, where either shared or exclusive locks are placed on.

Ideas, how to solve this job mentioned in the bullet-point above:

- The exclusive-locks and the shared-locks are managed within the daemon now (not
  flock/filesystem anymore).
- The daemon provides an API to exclusive lock the creation of a service-instance, which the
  provider has to call before he Registers/Offers his service-instance. I.e. like today with the
  ServiceExistanceMarkerFile he will call it before going to create any shared-memory objects.
- If the "lock" call fails, because already another user of the daemon has done this lock, the
  provider fails creating a skeleton/will not try to create/re-open shm-objects.
- Since now the daemon manages both: The lock AND the registration, he can:
  - Withdraw the lock automatically, when the provider unregisters the service-instance
  - Withdraw the lock automatically and unregister the service-instance, when the daemon detects
    a disconnect of the provider on message-passing level
- Also, the shared-lock/mutex semantics shall be implemented by the daemon and according APIs
  provided to providers AND consumers! I.e. the provider needs an API to put an exclusive lock on
  the "shared-mutex", while consumers need an API for shared-lock access.
- Also, in this case the daemon shall take benefit of being able to deduce from the connection
  state of message-passing, when he can automatically unlock/free these shared mutexes.

In the start-up phase of the ECU, there will be intense usage of the SD: Providers and consumers
all starting up in a small time-window and all trying to interact with the daemon (Register/Offer
a service or search for services). Since we need feedback/return-values for most of the
interactions (see above), each interaction based on underlying message-passing means a roundtrip
between client and the SD daemon. We should foresee a "batching mechanism" of these calls. I.e. to
collect a bunch of API calls to the daemon, batch them into one message-passing call (having just
one roundtrip between client process and daemon process) and also return a set of
return-codes/results back. It should be analyzed, whether this "batching" should be done
transparently to the caller (in our case our LoLa API-level) – the caller does not see "batching"
in the API signatures or explicit/visible to the caller. I feel, that we should strive for former
(transparent) solution.

Access control: In our LoLa configuration model, we most of time already have a clear assignment,
which service-instance ID is allowed to be offered/provided by which UID! We should foresee a
mechanism, where we can provide this static information to the daemon at startup, so that the
daemon can deny a service-offer of a certain instance from the wrong UID. This would be a 2nd line
of defense on top of our existing shm-object/UID/ACL solution.
</content>
