# New Message Passing design

## The challenges

The initial implementation of the message passing library showed some shortcomings in the interface and overall design when it comes to practical use. In particular:

* The customers prefer bidirectional connection-based message passing interface, where one side ("Receiver", Server, Skeleton...) passively waits for a connection, while the other side ("Sender", Client, Proxy...) establishes a connection, which then becomes bidirectional.

* The connection initiation should be asynchronous, otherwise the customer application needs to provide a dedicated thread that establishes a connection without blocking the core application logic.

* There should be a way for a client to send a request and then wait for a response from the server. Ideally, both a synchronous send-and-wait-for-reply and an asynchronous callback-based interface should be supported.

* The Message Passing library design (and the design of the interfaces in particular) should be friendly to singleton-free implementations, to bounded monotonic memory allocation, and to resource mock injection for testing.

* The separation of the message types into "short message" and "medium message" turned out to be premature optimization. Instead, the server-side endpoint specifies the application-level message exchange protocol with particular maximum sizes for request and response messages, and the client-side endpoint shall be aware of this protocol and these maximum sizes when it initiates the connection.

* The statically typed protocol wrapper on top of the byte buffer messages may be beneficial, especially if is ASIL B qualified.

* The ability to pass shared memory region handles via IPC (possible with Unix Domain Sockets and native QNX messaging) may also prove beneficial.

## General approach

We are going to implement a uniform interface with generally the same functionality using several different underlying transport mechanisms (in particular, native IPC messaging on QNX, as being ASIL qualified, and Unix Domain Sockets on Linux, as being available for testing on host). As different transport mechanisms introduce different service discovery namespaces, in order to reduce confusion, it might be worth to make sure that only one transport mechanism (QNX native messaging) is used on target, for both ASIL and QM communications.

(We don't use POSIX mqueue anymore for Linux, as it is unidirectional, doesn't support sessions, hard to clean up, and requires elevated privileges for longer queues. As we need sessions, we will be using Unix Domain Sockets of a type that supports sessions, ideally SOCK_SEQPACKET, or, if we need wider POSIX compatibility, SOCK_STREAM with our own packetizer on top of it)

Hiding the particular OS-dependent transport mechanism from the users of Message Passing, we introduce and implement the following communication abstractions: *Server* (handling a set of *Server Connections*) and *Client Connection*.

*Server* is a named entity (corresponding to some name - *Service Identifier* - in the service address namespace) responsible for accepting inter-process traffic (asynchronous unidirectional messages, as well as bidirectional request-reply message exchanges) directed to such a name. A process that wants to initiate such traffic needs to create a *Client Connection* toward the corresponding *Service Identifier*. On the *Server* side, to each successfully established and currently active *Client Connection* toward this *Server* in the system corresponds its own *Server Connection* entity, serving as a *Server*-side endpoint of the communication channel.

There is no more than one *Server* instance at a time able to accept connections to a particular *Service Identifier*. Technically, it might be possible to have several *Server* instances with consequent lifetimes belonging to the same or different processes serve the same *Service Identifier* during the whole system lifetime, provided that no more than one such *Server* instance available at a particular time. Measures to prevent use of "illegitimate" service instances, if such prevention is desired, are outside the scope of this design (for QNX implementation, additional secpol-based restriction on the service address namespace can be imposed if needed).

A *Server* instance is supposed to implement some application-level *Service Protocol*, which determines the payloads and, in particular, maximum sizes for messages into one or another direction. It is outside of the scope of the Message Passing design to establish how this protocol is enforced and how both sides of the communication know which protocol to use; what is important, however, is that during initialization of each *Server* and of each *Client Connection*, the maximum message sizes in both directions should be available to the message passing library in order to preallocate any necessary storage.

## Message Passing client

### Client Connection state

The connection is active when both *Client Connection* and its corresponding *Server Connection* exist. That implies that both the client process and the server process are running. It is possible to start *Client Connection* when the corresponding *Server* is not yet running. The connection will switch to active when the *Server* is available. The client process will be notified via a callback running from a library thread when this switch happens. The user can also see if the connection is active by checking the state of the *Client Connection*.

On the client side, the *Client Connection* instance can be in one of the following states: *Starting*, *Ready*, *Stopping*, *Stopped*.

* *Stopped* is the state in which the *Client Connection* instance is created as a C++ object, the final state to which the instance switches after disconnecting form the server. In the *Stopped* state, no messages will be sent, no replies will be received, and no user callbacks will be activated, except for the state change callback with the *Stopped* state as an argument (it is safe to destroy the connection object in this callback, as long as no concurrent or consequent access to the same object is initiated by the user). It is possible for the user to get the generic reason for the happened transition to the *Stopped* state by calling the `GetStopReason()` method. By using the `Start()` or the `Restart()` method, it is possible to bring the connection to the *Starting* state. For some stop reasons, it might be a feasible option to reconnect to the *Server* after previous disconnect, which - with proper OS-dependent transport mechanism - shall result in deterministic memory management, compared to deleting and recreating the *Client Connection* instance. For other reasons (such as insufficient permissions) the connection is unlikely to be successfully restarted.

* *Starting* is the state in which the *Client Connection* instance periodically tries to connect to the corresponding *Service*. Once the connection is successfully established, the connection state is switched to *Ready*. If the *Server* refuses the connection, if the *Client Connection* user explicitly stops the connection attempts by calling the `Stop()` method of the connection object or if the library detects its inability (due to lack of privileges or resources, for example) to check whether such a *Server* exist, the status is changed to *Stopping* or *Stopped*.

* *Ready* is the state in which the connection is active and the communication with the *Server* is possible. If the library detects that the *Server* is no longer accessible or if the user calls the `Stop()` method, the status is changed to *Stopping* or *Stopped*.

* *Stopping* is a transient state owing to the asynchronous nature of the library. In this state, the connection object is still used by some background activity of the library (including calling user callbacks). After this activity is finished, the connection will transition to the state *Stopped*. In the state *Stopping*, it's not yet safe to destroy the *Client Connection* object, otherwise the connection behaves as in the state *Stopped*.

Note: The deletion of the *Client Connection* instance as a C++ object is non-blocking only if the connection is in the *Stopped* state. It is safe to delete the *Client Connection* instance as the last operation of the state callback in the *Stopped* state. Alternatively, it is safe to delete the instance from the user's own threads, but if the user-provided callbacks are still running, the destructor of the instance will block until the callbacks return; beware of deadlocks.

### Client Connection callbacks

As the library callbacks are running on the same thread that is used internally for housekeeping purposes, it is not allowed to call any blocking operation of the Message Passing library during the execution of a *Client Connection* callback, as such an operation might otherwise result in a deadlock. The library provides the guarantee that such a call will safely fail.

### Message sending and delivery

By the core of the Message Passing library, message payload is treated as a byte sequence of a length from 0 to maximum allowed by the *Service Protocol*'s message size settings. From the *Client Connection* side, messages can be sent asynchronously in a single direction (fire-and-forget), synchronously with waiting for reply (a reply can also be 0-byte long), and asynchronously with a reply callback. No more than a single reply to each sent message is possible.

The messages sent in the *Ready* state of the *Client Connection* are delivered on the best effort basis. The messages of the same delivery type (the types being fire-and-forget or sent-with-reply) sent using the same *Client Connection* instance are delivered to the *Server* in the order they are sent; the serialization of delivery across the messages of the different delivery types is optional (configured at the *Client Connection* creation time) and relies on the use of the client-side asynchronous send queue. Message loss in the *Ready* state is an abnormal event in supposedly rare conditions (such as communication buffer overflow), but the service protocols shall address its possibility when necessary. Messages sent in the *Stopping* and *Stopped* states will be discarded; it is unspecified if sending messages in the *Starting* state may result in their delivery. In any case, as long as the *Client Connection* instance is already constructed and not yet being destructed, sending a message is a safe operation. Clients sending a message and then waiting for the reply will be notified with a corresponding error status if sending or replying was unsuccessful.

There is also a way for the *Server* to asynchronously send notification messages back to the client. A client shall register a corresponding callback for the *Client Connection*. The notification messages can only be sent after the *Server* received the first message from the *Client Connection*.

One possible extensions of the library functionality that might be interesting to the users is passing shared memory handles between processes (native in Unix Domain Socket messaging; using `shm_create_handle()` in QNX). It is not in the scope of the first release of the new library functionality.

### Client Connection creation and shared resources

It is expected that in some use cases, a user will want to create multiple similar *Client Connections* to multiple *Servers*. Such connections may share their resources (in particular, the background thread and the OSAL wrappers). To make the creation of multiple connections possible, we introduce a *Client Factory* object. It is responsible for encapsulating the particular OS-dependent transport mechanism implementation, its configuration parameters, and the various shared resources needed for the implementation, such as the background thread, the command queue for the background thread, the memory resource for PMR-aware parts of the implementation, and so on. It can also be useful for providing a mock implementation for testing purposes.

The *Client Factory* object and the *Client Connection* objects produced by it are shared owners of the resources the *Client Factory* encapsulates. The shared resources will not be deallocated until both the *Client Factory* and all the *Client Connection* objects are destructed.

## Message Passing server

### Memory allocation

The *Server* object is supposed to handle multiple connections at the same time. Memory handling by the *Server* object is supposed to be compatible with (or implemented by) a pool memory resource with a monotonic buffer resource as an upstream, at least for use in safe applications that serve requests from unsafe applications.

### Server Connection

The user of the *Server* interface communicates with the *Clients* using *Server Connection* objects. There is one such object per every established connection, plus one provisional object for a connection being established. The object's lifetime is managed by the Message Passing library itself. For the library user, it is allowed to keep a pointer/reference to a *Server Connection* object until return from the disconnection callback with this object's reference as an argument. It is not allowed to keep the pointer/reference to the provisional object if the connection is rejected by the library user's connection callback.

The *Server Connection* interface of a provisional object provides access to the method `GetClientIdentity()`, which returns information that can be useful for access control (TODO: TBD). Additional methods are available for established connections: `GetUserData()`, `Notify()`, `Reply()`, and `RequestDisconnect()`. These methods are thread-safe and can be called from non-callback threads is needed, although care needs to be taken not to call them after the corresponding disconnection callback.

### Server Connection initiation

When a client requests a connection and there is enough memory in the *Server* object to accept another connection, a user-provided connection callback is called. The callback can check the client identity and either accept the connection or reject it with an error code. An error code equivalent to `EAGAIN` will direct the client to attempt to re-establish the connection later. This could be useful, for example, to drop some less-important connections and serve the more important incoming one when the connection limit in the user's part of the *Server* functionality implementation is reached.

To accept the connection, the callback returns a *User Data* object. The format of the object is still TODO: TBD; it looks like it can be a variant of `void*`, `std::uintptr_t`, and `score::cpp::pmr::unique_ptr<IConnectionHandler>`, where the former two can be used to reference user-managed storage (albeit likely with some MISRA exceptions) and the latter one manages the user data object lifetime internally in the Message Passing library (the destructor for the `IConnectionHandler` object will be called after the disconnection callback).

### Server callbacks

There are the following user-registered *Server* callbacks that handle the connections:

* Connection callback handles connection requests as described in the Server Connection initiation section of the document.

* Disconnection callback handles disconnection events. Disconnection events only happen on the connections accepted by the connection callback.

* Sent message callback handles fire-and-forget messages sent by the client.

* Sent-with-reply message callback handles messages sent with reply request. The reply does not need to be immediately provided by the callback (although this is advised when possible); the *Server Connection* does not process other sent-with-reply messages from the client until the current such message in processing is replied to.

If the *User Data* object holds an instance of `IConnectionHandler` implementation, then the corresponding virtual methods of this implementation are called instead of the *Server*-wide callbacks. This, of course, doesn't apply to the connection callback.

All the callbacks for the same *Server* instance are called strictly sequentially, with no concurrence. However, the callbacks belonging to different *Server* instances can be called concurrently.

## Implementation considerations

### Client-Server communication protocol

For the purpose of describing the connection setup and teardown and the message exchange, the communication between the Client and the Server can be abstracted _as if_ it implemented as communication via Unix Domain Socket of SOCK_SEQPACKET type. The actual implementation does not need to follow the described communication scheme exactly, as long as the results for the user seem to be the same. In particular, the actual implementation may combine multiple messages in one or both directions into one underlying OS messaging transaction, in order to reduce the number of context switches.

In this abstract protocol, after the connection is established on both endpoints (*Client Connection* and *Server Connection*), the communication is happening through a bidirectional packet-oriented pipe that keeps the order of the packets. The buffering of the packets on the fly is implemented either by the OS itself (Linux) or at the endpoints (QNX). These abstract packets consist of two parts: the header (specifying the type of the packet) and the payload (the user-provided message corresponding to the type of the packet).

There are two types of packets sent by the *Client Connection* endpoint:

* `SEND` - corresponds to the fire-and-forget message.
* `REQUEST` - corresponds to the request message that expects a reply.

There are two types of packets sent by the *Server Connection* endpoint:

* `REPLY` - corresponds to the server reply to the request message.
* `NOTIFY` - corresponds to the notification message sent by the server.

As the *Server* does not start to process the next request on a given server connection before replying to the previous one, `REQUEST` and `REPLY` messages are naturally matched inside the connection by their ordering and don't need any help in the form of sequence numbers etc.. The communication errors of the message passing library itself are detected at the transport layer and are not a part of the abstract protocol. As the mismatch of maximum packet sizes between the endpoints cannot always be detected in the assumed actual Linux implementation, an additional negotiation phase may be needed during connection establishment there.

### Client-side implementation

The library keeps an internal thread to implement asynchronous communications for multiple *Client Connections*, including connection setup, connection status callbacks, asynchronous send queues, server replies, and server notifications. The thread runs a select-type waiting loop (using `poll()` for Linux and `dispatch` with `select_attach()` for QNX), where it processes requests for connects and stops, connection attempt timeouts, and asynchronous sends from *Client Connections*, as well as the communication events (incoming packets, ready-for-write events, connection status changes) from the other endpoint of the connection. This thread functionality may warrant separation into its own library for reuse elsewhere in the platform.

Each *Client Connection* object encapsulates all incremental non-system memory resources needed for its own functionality, at least for the QNX implementation. In some scenarios, that may require it to be an element of an intrusive linked list. If some library code for *Client Connection* requires synchronization with the internal library thread, such synchronization is preferably implemented with atomics; if this is infeasible, mutexes can be used instead, but then it is important that such a mutex is only held by the library thread for a bounded time.

We don't expect to be able to preallocate all the needed system memory resources. We rely on `ENOMEM` error for handling of system memory allocation limits on QNX.

### Server-side implementation

Here, we describe the QNX implementation. Linux implementation can be much simpler, as it can rely on the socket buffers and does not need to provide safety guarantees.

A *Server* instance runs on a library thread that watches for incoming messages and dispatches the corresponding callbacks. In the scenario where a safe server communicates with a safe client, a second thread can be used to provide the non-blocking guarantee - i.e. the guarantee that processing of asynchronous send calls in safe clients will take bounded time not dependent on the duration of user-defined callbacks in the server. If one of the threads is currently processing a callback, the other one will receive the message, store it in the server-side processing queue, and then unblock the waiting client. In case of the client thread having a higher priority than the server threads, we are counting on the QNX server boost to increase the priority of the queueing thread and thus to avoid priority inversion.

There is a common message queue implemented as a ring buffer of a construction-time configurable size. In addition, there is one message slot (TODO: or a configurable size queue?) per *Server Connection* which is intended to be filled with the arriving message when the ring buffer is full (which should normally be an extraordinary situation, but we try to support a graceful degradation there while keeping the safety guarantees). This ensures that the client can queue at least one message even when the ring buffer is full, but also makes it easier to wake up only the needed minimum of the clients waiting for ready-for-write events when the buffer gets free slots again. When the ring buffer is not full, we refill it with the messages occupying the slots of the *Server Connections* in the round-robin fashion. We don't reorder the message processing (asynchronous for the clients anyway) by the priorities of the client threads, at least in the initial release.

Each *Server Connection* also contains a slot for a single `REPLY` message and a configurable-size queue for `NOTIFY` messages. The client receives a ready-for-read event when at least one of them becomes non-empty.

As opposed to the *Client Connection* objects, the *Server Connection* objects are not created by the user. However, the user can refuse the incoming connections based on some criteria, for example, on the amount of already existing connections. In any case, being able to pre-allocate memory for the expected maximum of the connection objects would be great.

### Use of threads

In both Linux and QNX implementations, it is possible to serve all the *Client Connection* and *Server* instances inside a process with a single thread (or a single pair of threads). It is also possible to separate the use of threads between different instances by using different factories to create them.

Some processes may benefit from having larger shared thread pools and processing the incoming messages concurrently, provided that the library is not responsible for the synchronized access from the user's provided callbacks to the user's resources and that such synchronization is cheap when implemented by the user. The library support for these thread pools may be implemented later, it is not in the scope of the first release of the new library functionality.

## Safety concerns for QNX implementation

### General scenarios

We can consider the following scenarios:

* safe client to safe server: the server shall use two threads. The client does not need the client-side asynchronous send buffer unless it wants to strictly serialize the delivery across the fire-and-forget and sent-with-reply message types. The amount of memory needed on the server relies on the expected amount of well-behaving clients.

* QM client to QM server: the server may use one thread. The client does not need the client-side asynchronous send buffer unless it wants to strictly serialize the delivery across the fire-and-forget and sent-with-reply message types. The memory allocation is not a safety concern.

* safe client to QM server: the server may use one thread. The client shall rely on the client-side asynchronous send buffer for the non-blocking guarantee. The memory allocation on the QM server side is not a safety concern.

* QM client to safe server: the server may use one thread. The client does not need the client-side asynchronous send buffer unless it wants to strictly serialize the delivery across the fire-and-forget and sent-with-reply message types. The server shall explicitly limit its memory use (including the number of open connections) in order to protect itself from a misbehaving client opening too many connections.

### Timings

In general, we don't guarantee timings. We may, however, support watchdog-friendly interfaces that allow the library user to go into a safe state if the timing assumptions are broken. In the case of non-periodic events, the producer of the event is generally responsible for enforcing such timing assumptions, by keeping a watchdog armed until the event is consumed. There may also be a way to pass this responsibility to the consumer of the event in a transactional way.

We expect that fire-and-forget messages won't need such guarantees (it looks like everything that would be needed in this case is better covered by send-with-reply messages anyway) and that send-with-reply messages already have this support by design. What is missing is the support for notification messages. It can be implemented in an application-level protocol, when a client explicitly confirms to the server that the notification message have been processed (or that the client has armed its own watchdog), which allows the server to disarm the server's watchdog. Alternatively, it can be implemented by a pair of client-side and server-side callbacks called when the notification message arrives to the client. In this scenario, the client user arms the client-side watchdog in the client-side callback, then the server user disarms the server-side watchdog in the server-side callback, then the library retrieves the message from the server and dispatches it for consumption on the client.

The support for such a couple of callbacks can be implemented later and is not in the scope of the first release of the new library functionality.

## Questions to QNX support

The following are the questions the QNX support, answers to whose can simplify the implementation (or even the design):

* Can we postpone `io_open()` by using the `_RESMGR_NOREPLY` return code? Doing that will allow us to serialize clients' `open()` calls into a single Resource Manager thread, so we can already refuse a connection based on the result of the user-provided callback, without allocating an OCB for it. --- Yes, it is possible. `ctp->rcvid` of the `_io_open()` call needs to be saved; either `MsgError()` or `MsgReply()` with manually crafted `_io_connect_ftype_reply` may be needed for this receive id later. Also,`unblock` in `resmgr_connect_funcs_t` may be needed to handle unblocking RECEIVE-blocked `open()` call.

* Is there a possibility of closing the connection (freeing the OCB and sending the wake-up pulse to the corresponding client-side `select`) just by the server-side Resource Manager functionality, without relying on the ability of the clients to cooperate? That would simplify our `RequestDisconnect()` implementation and make its functionality not reliant on good behavior of the corresponding client.

## Examples

### DataRouter: logging and tracing sources

Logging and tracing (L&T) sources are safe and/or QM clients. DataRouter is a QM server. An L&T client uses a dedicated thread to initiate the connection, to pass the setup message to the server (which currently includes the name for the mapping file with the bulk-transfer ring buffer to be mapped by DataRouter; in the initial version of the protocol, the file descriptor was passed, to which we could return in the following versions of message passing), and then to listen to DataRouter requests and to reply to them. Time to time, if the bulk-transfer ring is more than a watermark full, a wake-up message ("ping") could be sent to DataRouter asynchronously with no expectation of direct reply; however, it is known that no further ping message is meaningful unless/until DataRouter responds with a particular kind of request (Acquire Request) - or, actually, until the client responds to this request. There is no connection termination from any endpoint except when the endpoint crashes or shuts down normally. When the connection to the server is lost, the client tries to reconnect. When the connection to the client is lost, the server finishes processing of the bulk-transfer ring buffer and then releases its memory-mapped file, in parallel waiting for a new connection from the client.

In this case, a client creates a *Client Session* with a state change callback. If the callback is triggered in the *Ready* state, the client sends the fire-and-forget setup message from the callback. Then the server sends its requests with notification messages, to which the client responds with fire-and-forget replies. If the client also sends ping messages, they are in a fire-and-forget manner using client-side buffering. In this scenario, all the communication activity with the QM server happens on the dedicated library thread (except that at client shutdown, the client needs to wait for the *Stopped* state on its main thread). As the protocol is defined, without ping functionality, only one in-transit message queue slot needs to be reserved for the client messages either on the client side or on the server side in order to never see `EAGAIN` for the send calls in the connected state. With ping functionality, two slots will be enough when ping messages are only sent when they are useful. As there are no send-with-reply messages, no serialization across the fire-and-forget and send-and-reply message types is needed.

At the server side, one in-transit notification message queue slot is needed to be able to protect the user from the need to handle `EAGAIN` results of notification calls. It is also possible to instantiate the server with only one communication thread, although two threads would likely result in lower overall latency for the L&T subsystem.

### DataRouter: subscribers

DataRouter subscribers are QM clients of DataRouter as a QM server. A DataRouter subscriber client uses a dedicated thread to initiate connection, perform an initial setup send-with-reply message exchange, and then do a series of send-with-reply exchanges from its own queue of subscription commands. In addition, it may receive ping notification messages from the DataRouter server triggered by the subscription ring buffer watermarks. Protocol-wise, these ping notifications can be safely lost.

While this seems to be as a straightforward case for direct porting to the library thread with callback-driven connection setup and callback-driven send-with-reply messages, with non-serialized ping notification messages, there are two complications:

1. Currently, we are passing a shared memory file descriptor during the initial connection setup message exchange. Moreover, we are using it in an OS-aware way. While the QNX implementation can be reused, the Linux implementation already needs the Unix Domain Socket functionality to pass the descriptor exposed in the library interface - or needs to be rewritten to pass a filename instead.

2. Currently, the same client thread is used for the implementation of the client-server communication and for triggering a periodic (100ms) timer callback for user-defined processing ticks. This functionality, while is not hard to implement for both QNX and Linux, is still missing in the proposed library interface.

At the server side, about the same as for the L&T sources applies. Currently, these are two separate server instances in the DataRouter, but technically, both servers can be shared in one *Server* instance.

### DataRouter: diagnostics and control

In the DataRouter diagnostics and control communications, both the client (DataRouter Config) and the server (DataRouter) are QM applications. All the DataRouter Config communications can be synchronous: first it waits until the connection is established, then it sends synchronous send-with-reply requests when directed to read or change the server configuration. One client send message queue slot is enough to avoid the `EAGAIN` condition. It looks like it may be redundant to have a library thread separate from the main thread in this case, but as the same process will also be a L&T client, this thread will be needed anyway.

The diagnostics and control server in the DataRouter in the previous version of the library shares the same *Server* instance as the DataRouter subscriber server, and this can be kept the same way.

### DataRouter: configurable logging and tracing services

There are DataRouter (at the moment, not more than two) clients that combine the functionality of the L&T sources with the functionality of DataRouter subscribers. The subscriber functionality is used to receive the configuration of the source-side filtering for the L&T sources. At the moment, these clients are QM, and are implemented via a pair of the connections: one for the L&T client, another one for the subscriber. This can be kept as it is, or we can reimplement it as a single connection, and maybe even make it a safe one.
