# LoLa Binding Component Design

## Shared Memory Access

In the case of our shared memory implementation (also called `LoLa`), it is necessary to access an underlying, not yet
further specified shared memory region which is specific to each communication instance. A problem can arise if the same
process includes a skeleton and proxy side of the same service instance. In this case we have to ensure that the shared
memory region is not mapped twice into the process space. In order to do that, the
`memory::shared::MemoryResourceRegistry` will take care to check which paths have already been created and will return
or create the necessary resources if necessary.

The details, if and when a shared memory object created by a communication instance gets cleaned up, can be read in the
[detailed design of partial restart functionality](../../../../design/partial_restart/README.md#partial-restart-specific-extensions-to-skeletonpreparestopoffer)

## Notifications between skeleton and proxy

There are several notification like interactions between proxy and skeleton. E.g., proxy
instance allows to register an event specific callback, which gets invoked as soon as the skeleton side has updated this
event (sent a new sample). You see the according methods in `lola::ProxyEvent::SetReceiveHandler` and
`lola::SkeletonEvent::notifyRegisteredReceiveHandlers` (which gets called in the context of `lola::SkeletonEvent::Send`)
. How this interaction between `lola::SkeletonEvent` and `lola::ProxyEvent` instance, which are typically living in
different processes, is technically solved, is an implementation decision! There are two pragmatic options:

1. Also use the `EventDataControl` data structure within the shared-mem object, which is already used for
   the mutual update/sync of event slot states. I.e. extend `EventDataControl` with some kind of
   interprocess mutex and notification mechanism.
2. Use a different sideband channel/IPC mechanism the target OS provides, which might be even better suited than
   solution 1.

Solution 1. has the benefit of being **portable** among our POSIX targets, because it is just relying on POSIX
shared-mem functionalities. But the devil is in the detail of a complex implementation:

- a posix interprocess-mutex is not really cheap as it uses at least `futex()` syscalls on both ends (notifying and
  waiting side)
- Granularity of waiting/notification:
    - The "natural" design might be, to have one mutex per `lola::SkeletonEvent`. But this would also imply, that you
      have overall (per process or entire system) a potentially **awful large size** of threads waiting on the
      connected `lola::ProxyEvent` instances side (note, that we often have 1:N communication for events!). This might
      get out of hands quickly regarding resources.
    - You could/should fix that by creating mutexes on a higher/more coarse grain level. F.i. one mutex for all events
      of a
      `lola::Skeleton` instance. In this case some **additional complexity** comes up, which needs to be mastered:
      Before the skeleton instance notifies this mutex (which now represents update notification of multiple event
      instances), it has to somewhere place within the `EventDataControl`, which event got updated (kind of
      a condition variable). And since updates can happen concurrently/in parallel for several events it rather gets a
      complex, list like condition variable, where you need synchronized (lock-free?) access from different threads (
      threads creating new events on skeleton side, notified/woken up threads on proxy side).

The sideband channel in solution 2. could be implemented "portable" by using a POSIX standardized mechanism (e.g. a unix
domain socket). The benefit opposed to solution 1. would be, that it allows to combine a message channel, where the info
about the affected event can be transmitted with a built-in notification mechanism. When using OS specific optimized
mechanisms (e.g. QNX messaging/pulses) the solution would be even more efficient! Most likely more efficient than
solution 1, even if we invest man-months for creating sophisticated lock-free condition variables!

So right now we decided to use a separate message-passing mechanism implemented in `score::mw::com::message_passing` for
the following proxy/skeleton notification/interaction use cases:

- `lola::ProxyEvent` subscribes at `lola::SkeletonEvent` (Not true currently! `Subscribe` is now done directly by the
  `lola::ProxyEvent` updating a variable of type `EventSubscriptionControl` located in shared-memory. This brought up
  issue Ticket-174759, which eventually will bring us back to use `message_passing` (in a revised form) for subscribe
  again.
- `lola::ProxyEvent` unsubscribes at `lola::SkeletonEvent` (Not true currently, see above!)
- `lola::ProxyEvent` registers a notification on event update.
- `lola::ProxyEvent` unregisters a notification on event update.
- `lola::SkeletonEvent` notifies registered event notification.

The first interaction is more complex as it is an `RPC` like interaction and not a simple uni-directional message.
Our new `message_passing 2.0` under `aas/lib/message_passing` does support RPC style communication, which we will use,
once we resort back to sideband channel communication for `Subscribe` and `Unsubscribe`.

We encapsulate the low-level mechanisms provided by `score::mw::com::message_passing` via
class `lola::MessagePassingFacade`, which is shown in the following class model:

<img alt="LOLA_MESSAGE_PASSING_MODEL" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/impl/bindings/lola/messaging/dependability/lola_message_passing_model.puml">

### Event subscription

Event subscription is technically rather an RPC as already mentioned [here](#notifications-between-skeleton-and-proxy),
because the caller needs a feedback. As outlined above, we are currently **not** using message-passing for `subscribe`/
`unsubscribe`, but this will change **soon** (Ticket-174759). In this case this chapter will get updated accordingly.

