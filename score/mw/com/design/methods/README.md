# Service Methods in LoLa

## Introduction

Method calls are &ndash; opposed to event communication &ndash; **bidirectional**: The client/caller sends IN-arguments
of the call to the server and the server sends back OUT-arguments/result back to the caller.
I.e. opposed to the event communication case, where only the service provider is creating "data", which gets exchanged
via a specific DATA shm-object being read-only for all clients (QM and ASIL-B), we have two separate data paths.

Other important differences:

- Method calls are always a 1:1 communication! No 1:N support for data-distribution in any direction (IN/OUT) needed.
- It has some further scheduling implications (discussed in chapter
  [Importance of SendWaitReply usage](#importance-of-sendwaitreply-usage))

## Deviations/Extensions for `ara::com` service-method API

### Skeleton side signature of service-method call

While the `ara::com` standard requires the service-method to be a pure virtual method of an abstract service skeleton
base class, that has to be overridden/implemented by the user in a derived class, we rather vote for an architecture,
where the skeleton class is **not** abstract and the user (instead of deriving from an abstract/generated class)
registers per service method a handler/callback, which handles the related service method call.

This is the proposed signature of the handler to be registered by the user:

`score::Result<OUT-arg data type> (const IN-arg1_type&, const IN-arg2_type&, ..., const IN-argN_type&)`

### Proxy side API signature of service-method call

#### synchronous vs asynchronous API

`ara::com` always mandates a `Future` based/async call signature on the caller/proxy side.
However, we will initially only provide a **synchronous** signature and call semantics on the proxy side!
There are two main drivers for this:

1. There are cases, where the user exactly wants this "synchronous" semantics. And he wants it **very efficiently**
   implemented (with minimal latency for the whole method call). I.e. in a way that after the proxy side thread has
   provided the IN-args and triggered the call, it gets  blocked, the `OS` instantly schedules the skeleton side thread,
   which executes the method and provides OUT-args and then unblocks the proxy side thread of the caller again. A
   synchronous signature allows to exploit the full benefit of priority inheritance from caller context to server/callee
   context discussed [here](#importance-of-sendwaitreply-usage).
2. The user is still able to detach/make the call **asynchronous**, by spawning a separate execution context (thread),
   in which he can call the "synchronous" signature. But here he has full control over the execution context.

Later, if needed, we can still come up with an additional asynchronous signature. This might make sense in the following
cases:

- there is a high method-call frequency for a specific method of a given service instance. Calls would get queued/
  serialized at the skeleton side, so an efficient caller-callee rescheduling round-trip will not take place anyhow. We
  would just run into a situation of a high number of blocked/waiting proxy side caller-threads.
- users have many call locations, where they want to have asynchronicity (call expected to be long-running and/or result
  is only needed deferred). If they create a thread for each call, this would eventually pile up/become inefficient
  (yes the user could apply a thread-pool then - but providing an async call-API in `mw::com` would add comfort)

It is beneficial, that we can shift asynchronous method-call signatures to a later point in time as it isn't clear yet,
how the C++ signatures should look like:
In upcoming C++ `std::future`/`std::promise` is almost dead. Being superseded with things like:

- coroutines
- executers/executer framework
- ...

So we then should think from scratch, what our async method call signature shall look like.

#### Initial synchronous API

The proxy class for a given service interface shall provide a member for each service-method the interface contains.
The name of the member shall reflect the service-method name (thus being unique per service interface).
The type of such a proxy-side method class is implementation defined, but needs to provide a call operator (`()`).
The concrete signature of this call operator depends on the service-method declared within the service-interface.
It shall adhere to the following pattern:

`score::Result<OUT-arg data type> (const IN-arg1_type&, const IN-arg2_type&, ..., const IN-argN_type&)`

This means, that:

- all `IN-args` are handed over by const-refs
- the return is a `score::Result` containing:
  - either an error-code
  - or containing a `struct` aggregating all `OUT-args` by value

Note: Either we require the user to only provide a single `OUT-arg` in a service-method definition, or we allow him to
define multiple `OUT-args`, which are then aggregated into a single struct type by `mw::com`.
This signature exactly matches the signature of the service-method handler registered by the user on the provider/
skeleton side.

### Zero-copy extensions

`ara::com` does **not** support zero-copy semantics for methods/method calls. This is bad as users might come up with
the same (large) data types `IN` or `OUT args` of service methods, they are using with event/field communication.
Zero-copy is the main performance asset, when dealing with large data types, so we should support it also with methods!

Adding zero-copy semantics to the method-call API should be done similar to event/field communication:

- add a 2nd call signature on the proxy side method class beside the [non-zero-copy](#initial-synchronous-api),
  taking the `IN-args` as `MethodInArgsAllocateePtr<T>` (`T` being a struct aggregating all `IN-args` by value).
- `OUT-args`/return value shall be returned via a `score::Result<MethodOutArgsPtr<T>>` (`T` being a struct aggregating all
  `OUT-args` by value - i.e. the same aggregation type as used in the non-zero-copy API)
- On the skeleton/callee side, the zero-copy overload of the user provided handler changes the signature to:
  `score::ResultBlank (const IN-arg1_type&, const IN-arg2_type&, ..., const IN-argN_type&, OUT-args&)`.
  I.e. the handler registered at the provider side will in the zero-copy case get a pre-allocated `OUT-args&`, where to
  put its out-args and returns an error (if there is one) by a `score::ResultBlank` return.

@ToDo: With a zero-copy/shared-memory based infrastructure, we can also support **real** INOUT args! On semantic level
AUTOSAR/`ara::com` did support INOUT args, but in any NON shared-memory based implementation this boils down to
(inefficiently) splitting up INOUT args into a pair of IN and OUT arg, which get independently transported (duplicated)
between caller and callee. In LoLa we could do much more efficiently! I.e. an INOUT parameter in a service-method will
not only be a semantic idea/notion, but really a performance booster if used accordingly!

## Call parameter and result exchange via shared memory

In LoLa the call parameters (`IN-args`) and the result of the call (`OUT-args`/return-value) will be exchanged via
shared memory. We will place both &ndash; `IN-args` and `OUT-args`/return-value &ndash;  in the same shared memory
object, which therefore needs read-write access for proxy and skeleton side (similar like our `CTRL`-shm-object used for
event/field communication). We call this `METHOD`-shm-object (beside our existing `DATA` and `CTRL` shm-objects)

### shared memory object per proxy instance

We propose to have one such `METHOD`-shm-object per proxy communicating with a service instance. This
`METHOD`-shm-object contains a [call-queue](#call-queue-per-service-method) for each service-method of the underlying
service-interface.

The reason for separating `METHOD`-shm-object per proxy is **security**. We don't want, that proxies from unrelated
processes share the same `METHOD`-shm-object, because then they could introspect the message-call traffic between the
provider process and further/other consumer/proxy processes violating security concepts.

_Note_: If "beneficial", we could aggregate the call-queues of different proxies for the same service instance into one
`METHOD`-shm-object, **IF** the proxy instances are located in the **same process**! It could be "beneficial", because
of resource-usage or easier partial restart handling.

#### Creation and Naming of `METHOD`-shm-object

The proxy instance shall create the `METHOD`-shm-object during `lola::Proxy::Create()`.
The name of the shm object shall (standard `LoLa` pattern) contain:

- `LoLa` service ID
- `LoLa` instance ID
- `methods` key-word
- `PID` of the creating process

thus, we expect something like this under `/dev/shmem`:
`lola-methods-0000000000006432-00001-620425`

### call queue per service method

For each ervice-method a service interface supports, there will be a call queue in the `METHOD`-shm-object. Its default
size is 1 and might be configured differently by the user.
However, the need for a larger queue will seldom exist as the call queue exists already on the granularity of a proxy
instance! Since `mw::com` APIs on service-element instance level can't be called in parallel as per our AoU, this means,
that our `ProxyEvent`/`ProxyField` instances as a whole are **not** reentrant. Therefore, a service-method call API is
also **not** reentrant.
Larger queue size only makes sense/can be supported, when we implement asynchronous API call semantics as discussed
[here](#proxy-side-signature-of-service-method-call).

**Decision: For now, we do not support a queue-size larger than 1.**

The **overall queue size** for a service-method from the perspective of the provider/skeleton side consists of all the
sizes of the queues of the proxies located in the set of related `METHOD`-shm-objects.
To correctly order the incoming calls from queues from different `METHOD`-shm-objects, the skeleton side has to care for
according to [call queue management](#call-queue-management).

A queue entry within such a call queue logically contains:

- a "slot"/space for all the aggregated IN-args (of type T)
- a "slot"/space for all the aggregated OUT-args and return value (of type U)

### Call queue management

As for now with our current decision/restriction to [only support synchronous call API](#proxy-side-signature-of-service-method-call)
and single-element call-queue per proxy, we will leave the complete queue-management to the `OS`, which is explained
below.

#### Initiating call of method

A proxy side call to the [synchronous call API](#proxy-side-signature-of-service-method-call) will:

1. Fill the `IN-args` of the single element queue in the `METHOD`-shm-object related to the proxy instance. Note, that
   in the case of `AllocateInArgs()` (see [here](#zero-copy-extensions)), this happens even before the call.
2. Notify the skeleton side about the call via `message_passing` side channel.

The notification (2) towards the server shall contain the information in which `METHOD`-shm-object the server shall find
the call `IN-args` and at which position (right now we only have one element/position, but in future this can change.)
I.e., the notification (2) contains the necessary info for the skeleton/server side, to identify the `METHOD`-shm-object,
which has been created by the proxy under the name defined [here](#creation-and-naming-of-method-shm-object).
The notification (2) of the call towards the server shall be blocking and only resume, when the server replies back,
which means, that the call has been finished/processed.

For this concept it is crucial, that we have our new msg-reply abstraction provided by `message_passing`, which is
explained in [Importance of SendWaitReply usage](#importance-of-sendwaitreply-usage).

#### Processing call of method

The skeleton/server side is receiving/listening on the `message_passing` side-channel for incoming call notifications.
On this `mesage_passing` receiver endpoint notification from all proxy instances (of different processes), which want to
call service-methods of skeletons residing in the same process come in and are queued by the `OS`! So we get the
queueing by call arrival time done by the `OS` automatically.

**_ToBeDiscussed_**: Shall we re-use the same single receiver endpoint, we do have now per `LoLa` process (which does
mainly event/field update notification handling) or should we create an additional/separate endpoint for the method
calls? I think a specific-endpoint makes sense for decoupling and under the impression, that we will get longer blocking
on this endpoint ...

When the call notification message has been received on the server/skeleton side, the contained information about the
location of call `IN-args` (`METHOD`-shm-object) is evaluated. If the related `METHOD`-shm-object has not yet been
`opened`/`mmapped` by the skeleton/server side, it gets `opened` and `mmapped` first.

**_Note_**: This "deferred"/lazy opening/`mmapping` might introduce some latency on the 1st service-method call of a given
proxy-process (when the server side has not yet mounted the related `METHOD`-shm-object). If this shall be avoided, we
have to find/define a different point in time where the skeleton/server side can do this.

Then the user provided service-method will be called with the `IN-args`/`OUT-args` referring to the correct location
within the related `METHOD`-shm-object. This call takes still place in the context of the `message_passing` receiver
thread.

When the user provided service-method-handler returns, it has already updated/set the `OUT-args` in the
`METHOD`-shm-object.
Then a reply-message is sent back via `message_passing` to the caller and the skeleton side is done with the processing
of the method call and the thread will again wait for reception of a new call message.

#### Concluding call of method

The caller/proxy side, which was blocked in [Initiating call of method](#initiating-call-of-method) gets unblocked by
the replying server side. In the non-zero-copy API case, it will return the `score::Result` with a copy of the `OUT-args`
from the `METHOD`-shm-object location to the calling user code. Otherwise, the `score::Result` will contain a
`MethodOutArgsPtr<T>` pointing to the `OUT-args` in `METHOD`-shm-object location.

## Event based vs polling based method call execution

`ara::com` explicitly differentiates between `event based` method call execution and a `polling based` variant on the
skeleton/server side. The former means, that the consumer/proxy side notifies the provider/skeleton side about a new
call via an "event". This is our `default behavior`, where we use our `message_passing` side-channel, which is described
in [call queue management](#call-queue-management).

**Initially, we won't support a polling based solution**.

A polling based approach means, that we don't have any "notification" via an event-mechanism from the caller to the
callee/server at all! The caller would in this case "simply" put the call `IN-args` in the queue in `METHOD`-shm-object
**without** notifying the skeleton/server, but rely on the skeleton/server to poll/check the queues cyclically.

If later the requirement for a polling based approach comes up, we 1st have to understand, what would be the reasoning
for it. If the idea is to be more in control over the processing time spent for method calls triggered by remote proxies,
it shall be discussed, whether we could simply "throttle" the thread (lower priority) listening on the `message_passing`
receiver/server endpoint.

## Importance of SendWaitReply usage

With our existing event communication in place, we could have designed service method calls simply as a composition of
bidirectional event communication. A current `LoLa` user in need to have something like a method call would have gone in
this direction: One event is `IN-args` from proxy to skeleton and the other event is `OUT-args` from skeleton to proxy.
Totally decoupled and completely asynchronous.
It is just syntactically very ugly and error-prone to care for matching both unidirectional event communications to
each other accordingly.

But there is a very important non-functional feature, which users want to have, when dealing with (cross process)
service method calls: Low Latency (this is `LoLa`)!

Our signatures for the service-method call on `mw::com` C++ API level imply complete asynchronicity:

- caller gets a future as the call result.
- callee fulfills the future at some point from a completely decouple context

and a "simulation" of a method call by the composition of bidirectional event communication will typically lead to an
extremely high latency:

- Some thread in the caller/proxy process gets scheduled, which create/stores the IN-args event in shared-memory and
  sends a unidirectional notification message to the callee/skeleton process.
- because of this notification reception a thread gets scheduled at the callee/skeleton process side, to process the
  method call, which then stores the OUT-args/return value in shared-memory and sends a unidirectional notification
  message to the caller/proxy process.
- again because of this notification reception a thread gets scheduled at the caller/proxy process side, to process the
  method call result.

All these scheduling transitions between threads of the participating processes are not seamless/low-latency! Because
the priority of the different threads with respect to each other is undefined. So the latency between one thread
sending a notification to another thread until this receiving thread then gets scheduled might be very large leading
to a very large latency for the complete service-method call!

What the user (at least in `event based` mode) wants to have:

- the proxy side thread calling the service-method has a given (high) priority
- when it calls via message_passing the notification on the callee side to process the method call, the current thread
  priority shall be taken over (inherited) by the callee side reception/processing thread
- and again on the caller side the thread processing the call result shall also inherit the same priority so that in
  the end there is no scheduling latency involved.

The `SendWaitReply` API in `message_passing` 2.0 is optimized in the `QNX` case exactly for this very efficient
priority inheritance mechanism and should therefore be used!

## Partial Restart implications

Compared to the challenges we have with partial restart in case of event/field communication, this gets much easier here,
as there isn't a complex shared state between a skeleton and N proxies!
Each consumer/proxy process has its own `METHOD`-shm-object! There are the following crash/recovery cases:

### Proxy (Creator/Owner of `METHOD`-shm-object) crashes

If the proxy process, who created the `METHOD`-shm-object, crashes after a message call had been initiated, then two
things will happen:

1. After the skeleton/server has set the OUT-Args in the shm-object, it will message-reply to the proxy process and this
   will simply fail. There is no need to clean-up anything for the skeleton/server side
2. When the proxy process restarts, it will create a new `METHOD`-shm-object
   (see [naming of `METHOD`-shm-object](#creation-and-naming-of-method-shm-object)) and notify it to the skeleton/server.
3. The skeleton/server can then detect old/unused `METHOD`-shm-objects from crashed proxy processes and close/unlink
   them to clean up resources.

If the crash happens before a call or during populating the IN-args of the call it is no problem at all, since the
skeleton/server is not even involved in a call. The restarting proxy-process will simply create a new
`METHOD`-shm-object. See (2) above.

### Skeleton (User of `METHOD`-shm-object) crashes

If the skeleton/server crashes **after** the proxy initiated the call via `message_passing`, but before replying back to
the caller, the caller/proxy will get an error from `message_passing` `SendWaitReply`. So it gets unblocked and knows,
that the skeleton/server crashed. It can then re-use/clean-up the `METHOD`-shm-object and use it for the next call, if
the skeleton/server is up again and can be contacted via `SendWaitReply` again.

Crashes before/after a service-method call will be detected by the proxy-side, when trying to initiate the next method
call via `SendWaitReply`. If this initiatíon already fails, the call gets aborted and an error gets reported to the
user.
