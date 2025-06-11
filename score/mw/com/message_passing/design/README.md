# Message Passing

We have the need for a more low-level but more performant solution for IPC in addition to more sophisticated
communication that is provided e.g. by `ara::com`. This is the case because we might need to communicate with components
which are no adaptive AUTOSAR components, but also when thinking on how to implement `ara::com` we might need a
low-level communication channel.

This is the idea of this Message Passing abstraction.

In general this is highly operating system specific. While we will implement a POSIX solution, it might be necessary,
for performance reasons, to also have OS specific implementations (e.g. QNX messaging).

## Design

You can find the proposed structured view as follows:

![Structural View](broken_link_k/swh/ddad_score/mw/com/message_passing/design/structured_view.uxf?ref=18c835c8d7b01056dd48f257c14f435795a48b7d)

The main idea behind it is quite simple. We have a producer of data (`Sender`) and a consumer of data (`Receiver`).
Between these two classes we create an **uni-directional** data exchange channel. We do this by adding the responsibility
to the `Receiver` to open respective communication channel. This way we can have **multiple** `Sender` that communicate
to one `Receiver` (**unidirectional n-to-1 communication relationship**).
_Note_: Even if the underlying IPC mechanism provides bidirectional and/or n-to-m communication this won't be exposed!

The communication paradigm is strictly **asynchronous** in the way, that a `Sender` doesn't get blocked (under normal
circumstances) by the processing of the sent message by the `Receiver`.
While an OS specific implementation of `Sender` and `Receiver` might fulfil this **asynchronous** behavior in general
and might therefore be non-blocking from the viewpoint of the `Sender`, it can be still blocking in certain error
conditions depending on the concrete implementation. Since we want to use this mechanism also for communication between
a higher ASIL level `Sender` and a lower ASIL level `Receiver`, where even in rare error cases blocking of the `Sender`
is not an option, we add a mandatory query-method to the `Sender` interface, to ask the implementation about its
non-blocking guarantees, so that users can then apply their own measures if needed.

A user of the `Receiver` first has to provide an identifier and executor.
The identifier is the common knowledge of the `Sender` and `Receiver` to identify a communication channel - normally
this will result to some path in the file-system.
The executor will be used to schedule one or more &ndash; depending on `Executor::maxConcurrencyLevel()` and OS specific
Receiver implementation hints &ndash; `message loop` tasks (or threads) , that will wait for a notification to appear
(aka a `Sender` is providing something), then extract the message from the underlying channel and call message
processor/callback, that the user of a `Receiver` did register for specific messages.

We distinguish between `ShortMessages` and `MediumMessages`, which are subclasses of `BaseMessage`. Later addition of
bigger/almost unrestricted message types is possible if needed.
The general idea is that the size of these messages is quite small - and thus their payload they can transmit. This is
the case to ensure the most efficient implementation on operating systems and the `ShortMessage` size is right now
influenced by the QNX pulses size. At the end the size is small enough for the operating system to implement super fast
exchange (e.g. via register reinterpretation). For most use-cases this is enough.

**_Update/Note_**: After we've done a first implementation of Sender and Receiver for QNX, we learned, that usage of QNX
pulses would have implied certain restrictions, which finally led us to use "normal" IPC messages
(see [our QNX solution](#qnx-resourcemanager-framework)).

We have an identifier (`MessageBase::id`), which lets us know what kind of message we receive and then the possibility
for some context information. One example would be the message `SwitchPowerState` with id `0x42` and payload
`Off` (`0x00`). In cases, where more data needs to be transmitted, we could fall back to `MediumMessages`/unrestricted
messages, which would be of bigger size. Another advantage of having `ShortMessages` is, that we don't need to bother
about dynamic memory allocation. Additionally, every message contains the `PID` of the sending process. The idea is,
that based on this info, the `Receiver` could eventually set up a message passing back-channel to respond to the
`Sender`. Depending on the underlying IPC mechanism, the `PID` needs either to be explicitly transmitted or is already
implicitly contained.

Upon registration of a callback we will perform dynamic memory allocation, thus this functionality shall only be used
outside of real-time cycles. This is rather enforced by the fact that we will only start listening, after all possible
callbacks have been registered. At the end the `Receiver` will wait for the next message, check if we have a callback
registered and then invoke the respective callback. This way we provide maximum flexibility. We decide against
a polling mode, since data heavy applications should use `ara::com`.

The `Sender` will wait on construction until the `Receiver` opened the respective communication channel. Then he is able
to send data at any point in time. The `Receiver` will have some OS specific queue for incoming messages. We expect that
this queue is configured big enough for respective use-cases, thus that no messages will get lost.

You can find the respective sequence diagram here:
(Note that functions WaitWithTimeoutForChannel(), RegisterCallbackinMap() , CheckIfCallbackRegistered() and receive_and_process_next() are self calling mechanisms but not functions defined in the code.)

![Sequence View](broken_link_k/swh/ddad_score/mw/com/message_passing/design/sequence_view.uxf?ref=18c835c8d7b01056dd48f257c14f435795a48b7d)

### Detailed Design
#### Sender and Receiver Traits
The OS specifics of the message passing are encapsulated via template parametrization in the form of `ChannelTraits`.
I.e. the generic implementations of `Sender` and `Receiver`, which contain OS **independent** processing logic, have to
be specialized with an OS specific `ChannelTraits` implementation.
For the **Sender** the `ChannelTraits` have to provide:
* `using file_descriptor_type = sometype; /* behaves like a POSIX handle */`
* `static constexpr file_descriptor_type INVALID_FILE_DESCRIPTOR`
* `static score::cpp::expected<file_descriptor_type, score::os::Error> try_open(std::string_view identifier) noexcept`
* `static void close_sender(file_descriptor_type file_descriptor) noexcept`
* `static sometype_short prepare_payload(ShortMessage const& message) noexcept`
* `static sometype_medium prepare_payload(MediumMessage const& message) noexcept`
* `static score::cpp::expected_blank<score::os::Error> try_send(file_descriptor_type file_descriptor, sometype_short const& buffer) noexcept`
* `static score::cpp::expected_blank<score::os::Error> try_send(file_descriptor_type file_descriptor, sometype_medium const& buffer) noexcept`
* `static bool has_non_blocking_guarantee()`

For the **Receiver** the `ChannelTraits` have to provide:
* `static constexpr std::size_t kConcurrency /* amount of threads worth running */`
* `using file_descriptor_type = sometype /* behaves like a POSIX handle */`
* `static constexpr file_descriptor_type INVALID_FILE_DESCRIPTOR`
* `static score::cpp::expected<file_descriptor_type, score::os::Error> open_receiver(std::string_view identifier,
                                     const score::cpp::pmr::vector<uid_t>& allowed_uids,
                                     std::int32_t max_number_message_in_queue) noexcept`
* `static void close_receiver(file_descriptor_type file_descriptor,
                               std::string_view identifier) noexcept`
* `static void stop_receive(file_descriptor_type file_descriptor) noexcept`
* `template <typename ShortMessageProcessor, typename MediumMessageProcessor>
  static score::cpp::expected<bool, score::os::Error> receive_next(file_descriptor_type file_descriptor,
                             ShortMessageProcessor fShort,
                             MediumMessageProcessor fMedium) noexcept;`

#### Message Loop
Within `Receiver<ChannelTraits>::StartListening()` several message loop (see above) tasks get submitted to the given
`Executor`, which all execute `Receiver<ChannelTraits>::MessageLoop`.
Within the loop calls to an OS specific `ChannelTraits::receive_next()` are done until this call returns/reports back,
that reception has been stopped.
The semantics of `ChannelTraits::receive_next()` is to receive/extract the next message from the OS specific channel and
call the given message processor/callback with the received message.

Stopping of this OS specific reception is done via `ChannelTraits::stop_receive()`. Current implementations (POSIX Mqueue
and QNX IPC) solve this by sending a special stop message to the channel, where the thread executing the message loop
hangs/blocks in the `ChannelTraits::receive_next()` call. So if this special stop message is received,
`ChannelTraits::receive_next()` returns that reception has stopped and in this case the message loop ends.

`ChannelTraits::stop_receive()` itself is called via `stop_token` mechanism: I.e. the root decision to stop the message
loop is driven by the destruction of the enclosing Executor, which was given to the Receiver instance during construction.
Each call to `Receiver<ChannelTraits>::MessageLoop` by the different tasks/threads of the Executor gets a `stop_token`.
Only the first task/thread executing the message loop creates a `stop_callback` from the `stop_token` handed over by the
Executor. Within the `stop_callback` N times `ChannelTraits::stop_receive()` gets called, where N is the number of
submitted tasks/threads executing the message loop. Since `ChannelTraits::receive_next()` within all threads executing
the message loop, will exactly consume one `stop_message` and then exit its message loop/terminate, these N calls to
`ChannelTraits::stop_receive()` are enough to wake up/terminate **all** tasks/threads processing the message loop.

**_Note_**: The design decision to create only one `stop_callback` from the first task/thread, which calls N times
`ChannelTraits::stop_receive()` opposed to creating one `stop_token` within each task, which calls
`ChannelTraits::stop_receive()` exactly once, is due to **race condition** reasons: All `stop_callbacks`, which fire,
are sending the stop message to the same channel, and it is nondeterministic, which task/thread then consumes this stop
message and terminates! So it is very likely, that a thread consumes a stop message produced by the `stop_callback` from
another thread and therefore terminates, whereby destroying its own `stop_callback` before it has been triggered by the
underlying `stop_token`/`stop_source`! Which would mean, that this threads `ChannelTraits::stop_receive()` wouldn't get
called, which leads to one task/thread not getting stopped.

#### Concurrency within Receiver
The decision about the number of tasks to be submitted to the `Executor` for message loop processing is done at the
level of generic Receiver implementation. It thereby asks the OS specific receiver `ChannelTraits` about its "supported"
`kConcurrency` and therefore the formula for the number of message loop tasks to be submitted is:

Number of message loop tasks = `min(Executor.maxConcurrencyLevel(), ChannelTraits.kConcurrency)`.

Our current implementations of receiver `ChannelTraits` for POSIX MQueue and QNX define `ChannelTraits.kConcurrency` in
a way, that it is assured, that received messages are processed in-order (arrival time at the receiver) one after the
other. So right now we are restricting concurrency/parallelism! Even if the creator/user of a Receiver hands over an
Executor, which provides a high level of concurrency, **it won't be exploited**!

This restriction might help naive users, which send two messages A and B in this order, where A and B impose contrary
effects at the receiver and would then be puzzled in case afterwards the effect of message A is in place/manifested at
the receiver, because it has been effectively processed/executed **after** message B.

**@ToDo:** Discuss, whether this restriction should be removed and full parallelism/concurrency should be implemented at
the receiver side. Explicitly stating in the `ISender` interface definition, that reordering could take place!
I.e. messages sent on the same `Sender` are unrelated and might be effectively processed "out-of-order" in a
multi-threaded receiver. This implies, that a user of a Sender, who wants to impose a strict order in message processing
would need to spawn a reverse back-channel and apply a dedicated feedback protocol based on it.

#### Memory allocation
Sender does not allocate its own memory internally by itself, although it does consume one file handle from the OS pool.
When the Sender object is created, a custom memory resource can be used to provide the storage for this object inside
the sending process.

Receiver does allocate its own storage and requires an `score::cpp::pmr::polymorphic_allocator` to be passed as a parameter to
its constructor. In that regard, it implements the `std::uses_allocator` type trait requirements. The memory is used to
store (as copies) its `identifier` and `allowed_uids` parameters passed by reference, as well as for internal memory
allocation in the receiver `ChannelTraits` (for which the `score::cpp::pmr::polymorphic_allocator` passed to the
`ChannelTraits::open_receiver()` via its `allowed_uids` parameter will be reused; the allocator used by Receiver's
Executor can be different from this one). In addition, Receiver can allocate a limited amount of some OS resources.

## OS-Specifics
### POSIX MQueue
For the POSIX API we will rely on message queues. This way we ensure compatibility between Linux, QNX and any other
POSIX OS. This decision is based on the natural behavior of `MQueue` with our API and the fact that next to Shared Memory
`MQueue` is one of the fastest IPC mechanisms.

#### ChannelTraits impl.
The corresponding `ChannelTraits` implementations for `POSIX MQueue` (`MqueueSenderTraits`/`MqueueReceiverTraits`) are
straightforward:
* `file_descriptor_type` maps to mqueue descriptor `mqd_t`
* `try_open` and `open_receiver` rely on `mq_open()`, whereby using `kNonBlocking`/`kWriteOnly` for try_open on Sender
side
* `try_send` relies on `mq_send`
* `receive_next` relies on `mq_receive`
* `stop_receive` sends a special stop message to the `mqueue` via `mq_send`
* ...

#### Concurrency MQueue Receiver
Our `MqueueReceiverTraits` implementation defines `MqueueReceiverTraits.kConcurrency` to be 1. Therefore, only one
message loop will get submitted and the strict ordering stated [here](#concurrency-within-receiver) is assured.

#### Non-blocking guarantee
Since we use `kNonBlocking` on the Sender side, `Sender<MqueueSenderTraits>::HasNonBlockingGuarantee()` returns true.

**_Note_**: Since we typically need/want to have this non-blocking guarantee in case of ASIL-B setups, this might need
an additional check:
* in case of a non-qualified OS any "guarantee" is weak anyhow due to its "non-qualified nature". So this doesn't need
  further discussion.
* in case of a generally qualified OS, it has to be checked, whether also the `MQueue` impl. is qualified. E.g. for QNX
  it is not qualified!


### QNX ResourceManager framework
For QNX we implemented specific `ResmgrSenderTraits`/`ResmgrReceiverTraits`, because using its POSIX MQueue impl. was not
an option due to its non-qualified status (se above).
Our solution for QNX will use its synchronous message passing IPC based on `MsgSend`/`MsgReceive`/`MsgReply` APIs.

We didn't use those low-level APIs directly, but based our implementation on the QNX `ResourceManager` framework.
This influences heavily the Receiver side implementation (`ResmgrReceiverTraits`), because in our setup the Receiver is
the `ResourceManager`.

#### Sender side impl./traits
The Sender is just a resource user and its implementation of `ResmgrSenderTraits` boil down to
simple POSIX file descriptor I/O calls towards the resource (communication channel), which has a file system
representation:
* `try_open` maps to `open`
* `close_sender` maps to `close`
* `try_send` maps to `write`

#### Receiver side impl./traits
The receiver side implements the resource manager by using the API/abstractions provided by the QNX ResourceManager
framework. This framework encapsulates the lower-level IPC primitives used within QNX, where you can register handlers
for connection to channels (open) and for I/O (read/write/...).
There is a thorough documentation of the ResourceManager framework to be found
[here](https://www.qnx.com/developers/docs/7.1/#com.qnx.doc.neutrino.getting_started/topic/s1_resmgr.html)

As mentioned, the ResourceManager framework uses QNX IPC primitives, which are based on synchronous message passing!
This means, that the client sending a message (`MsgSend()`) gets blocked until the server (receiver) replies to this
message. (Specifically the client/sender can either be `RECEIVE_BLOCKED` or `REPLY_BLOCKED` - details can be found
[here](https://www.qnx.com/developers/docs/7.1/#com.qnx.doc.neutrino.sys_arch/topic/ipc_Sync_messaging.html).
Understanding this lower level mechanisms is important as the way we use/handle the ResourceManager framework (keyword
`early unblock`) has its roots in these lower layer mechanisms! I.e. even if we use a `O_NONBLOCK` flag in the `open`
call of our `ResmgrSenderTraits` implementation, it can still block, because of these underlying mechanisms!

Essentially the implementation of our `ResmgrReceiverTraits` implements a handler for the `write` POSIX call done by the
`ResmgrSenderTraits` to send a message. Other calls (`open`, `close`) are handled by the default implementations of the
ResourceManager framework.

Since we want to unblock the sender of a message **as fast as possible** (to be as close as possible to the behavior of
a non-blocking POSIX MQueue impl.), we don't process the message received via write I/O func and dispatched to our
corresponding handler `ResmgrReceiverTraits::io_write` by the ResourceManager framework **directly**, but put it in a
circular buffer for deferred processing. I.e. the handler `ResmgrReceiverTraits::io_write` returns directly after
queuing of the message and therefore instantly unblocks the Sender (from its `write` call) **before** processing the
message.

The processing of messages in the queue/buffer happens after the handler has returned. Therefore, our current QNX Receiver
implementation needs at least 2 threads (`ResmgrReceiverTraits::kConcurrency{2}`), i.e. expects, that the given Executor
also supports/has `maxConcurrencyLevel() >= 2` (see note below):
Initially 2 threads/tasks will be waiting in the `dispatch.dispatch_block(context_pointer)` within
`ResmgrReceiverTraits::receive_next`. The first one, which gets woken up from `dispatch_block()` and puts the message
into the queue will then &ndash; after unblocking the sender &ndash; access the queue and process the message. This
means very low latency as the same thread proceeds with the processing of the message queue exclusively. I.e. we have a
design, where only ONE thread at a time processes the message queue, while the other thread in this case does early-reply
the senders and fills the message queue. So we do not have a fixed assignment, which thread does which job, which under
normal load leads to the effect, that the same thread, which wakes up from `dispatch.dispatch_block(context_pointer)`
unblocks the sender and processes the message instantly, which leads to small latency in this typical case.
Error reporting towards the sender in case of full circular buffer is also done fast/immediately. I.e. the worker thread
trying to queue the message detects a full queue and replies early to the sender with an error.

#### Concurrency QNX Receiver
Our `ResmgrReceiverTraits` implementation defines `ChannelTraits.kConcurrency` to be 2. Therefore, at most two message
loops will get submitted and the strict ordering stated [here](#concurrency-within-receiver) is assured.
If you follow the explanation of the `ResmgrReceiverTraits` message processing algorithm above, you see, that the
additional thread required/needed by the `ResmgrReceiverTraits` opposed to `MqueueReceiverTraits` is because of the
early/fast unblocking feature needed in QNX (not needed in POSIX Mqueue).

If the user of a QNX receiver would hand over an `Executor` instance, which just has a `maxConcurrencyLevel` of 1,
the early/fast unblocking feature can not be provided, but right now, we **aren't** raising an error!

#### QNX abilities

As `ResmgrReceiverTraits` implementation publishes an identifier-derived channel name in the QNX namespace under the
`/mw_com/message_passing/` prefix, it needs a QNX-specific capability ("ability") to do so. The ability is called
`PROCMGR_AID_PATHSPACE` and is normally enabled for root processes and disabled for unprivileged processes. This
ability needs to be enabled before dropping the privilege. For free-running tests and applications running from
start-up scripts, `on -A nonroot,allow,pathspace ...` QNX command could be used to achieve this. For applications
started through the Vector stack, the following entry needs to be added to the application dictionary (at the same
level as the "user" entry) in the application's `app_config.json`:

```json
           "abilities" : [
               {
                   "variant": "base",
                   "config": [
                       {
                           "ability": "PROCMGR_AID_PATHSPACE",
                           "lock": false,
                           "inherit": true,
                           "user": "NON_ROOT"
                       }
                   ]
               }
           ],
```

## Valid identifiers

We allow only alpha-numerical strings. e.g. `my_identifier_42`. But not `my/identifier/42` since in most implementations
the identifier will be put somewhere in the operating system.

Note: as it is implemented at the moment, the identifier always starts with a slash char ('/'). The identifier length,
including the leading slash, is limited to 256 symbols. The application will abort during the construction of the
corresponding sender or receiver if a longer identifier is used.

## Access Management

There is no access control for `MQueue` implementation. Everyone can send messages to any `Receiver`. The
`allowed_uids` parameter of `MqueueReceiverTraits::open_receiver()` call is ignored.

For QNX `ResourceManager` implementation, the `allowed_uids` parameter of `MqueueReceiverTraits::open_receiver()` call
specifies the list of effective user IDs for the sending processes at the moment of `ResmgrSenderTraits::try_open()`
call, for which the access will be granted. Once the `Sender` has successfully opened the channel, the access to the
channel from the sending process will be kept as long as the channel is open even if the sending process changes its
effective UID later (which matches the behavior of a normal `open()` call in QNX or POSIX in general). If the list of
`allowed_uids` is empty, there is no access control for the corresponding `Receiver`.

## Wrapper for Non-blocking guarantee

In case an application using a `Sender` to send messages to a `Receiver` needs a strong guarantee, that the
`Sender.Send()` call doesn't block, while the OS specific `Sender` implementation can't guarantee this always
(`Sender.HasNonBlockingGuarantee()` returns false) we provide a wrapper around `Sender` in form of `NonBlockingSender`,
which provides the non-blocking guarantee.

## Involved Components and Dependencies

Implementation of `mw::com::message_passing` depends on the following components/libraries:

* amp (broken_link_g/swh/amp)
* `//platform/aas/lib/concurrency:concurrency`
* `//platform/aas/lib/memory:pmr_ring_buffer`
* `//platform/aas/lib/os:errno`
* `//platform/aas/lib/os:fcntl` (only in QNX platform case)
* `//platform/aas/lib/os:mqueue` (only in host/Linux platform case)
* `//platform/aas/lib/os:stat` (only in host/Linux platform case)
* `//platform/aas/lib/os:unistd` (only in QNX platform case)
* `//score/os/qnx:channel` (only in QNX platform case)
* `//score/os/qnx:iofunc` (only in QNX platform case)
* `//score/os/qnx:dispatch` (only in QNX platform case)
