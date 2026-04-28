# Service Discovery and Crash Recovery

## Introduction

On a first look both topics `Service Discovery` and `Crash Recovery` are completely separated topics, but it turns
out, that we will have lots of interdependencies between them! Therefore, and because we are implementing them in
parallel, they are considered a functional package.
We will start with the `Service Discovery` concept first and explicitly mark design decisions taken here
because of `Crash Recovery` needs. Then in the `Crash Recovery` chapters we will link back.

## Service Discovery

Here we are dealing with the process, where a service provider, makes itself visible to proxies (consumers) looking for
such a service instance. A service provider in `mw::com` is a service instance inheriting from a
`mw::com::impl::SkeletonBase` during `Skeleton::OfferService()`.

### Starting Point

In the initial stripped down CP-60 release of `mw::com` we used the `/dev/shm` file system managed by the OS for this
task.
This was very low effort, since service instance based on our `LoLa` (shared-mem) binding had to create and set up its
shared-memory objects anyhow before being able to communicate with potential consumers (proxies). So the simple
existence of shared-memory objects, was at the same time the publishing in the `Service Discovery`. Since these
shared-memory objects had service-type and instance-id encoded into their file name.

Main benefits of this approach:

- no separate discovery/daemon process, which
  - can crash and needs potentially complex recovery on its own, when being restarted
  - needs to be started BEFORE all other `mw::com`/`LoLa` apps
  - needs its own low-level IPC mechanism for communication with its clients (`LoLa` service consumers/providers)
- no big initial implementation effort
- portable code (no specific OS dependencies to Linux/QNX)

The basic architecture of our `Service Discovery` was:
The file system is used as the `Service Discovery` directory service. I.e.
the availability of a service instance is expressed by the existence of a file within a certain directory.
The file name expresses the service type and the instance ID.

We had the restriction, that we only supported synchronous one-shot `<Proxy>::FindService()` calls. These calls did a
(synchronous) lookup in the `/dev/shmem` file system for a specific name of a shm-object file, which represented a
specific service instance. Therefore, we were missing the following functionalities:

- Asynchronous supervision of newly starting or stopping service instances as required by the
  `<Proxy>::StartFindService()` API.
- Event subscription state wasn't updated at all, in case the providing service went down/stopped its offer.
- Consequently, auto-reconnect[^auto_reconnect] functionality wasn't implemented at all.

[^auto_reconnect]: a client gets automatically reconnected to a service instance after it went down and came up again.
Any subscriptions a client (proxy) made to events or fields of a service instance are renewed automatically, when the
service instance shows up again.

### Evolution

We will stick to the base architecture from our [Starting Point](#starting-point). I.e. use file system and existence of
files as service availability markers. What changes is the location. We will not use the `/dev/shmem`
(`/dev/shm` on Linux) file system in the future, but the `tmp` file system, because of the following reasons:

1. We want to be able to support all the missing features listed above, for this we need an efficient mechanism to
   supervise changes in the file system. This mechanism, which will be explained soon, isn't supported (depending on the
   OS) in all file systems! `/dev/shmem` is a very special file system, which lacks this mechanism at least in QNX.
2. Having the service visibility and availability coupled directly to the shm-objects of the service instance had the
   following downside:
   The shm-object was seen by potential consumers, directly after its creation. Even before the data structures inside
   were set up completely!
   We could not allow potential consumers to access the shm-object at this stage. The shm-object access layer
   (`SharedMemoryFactory` and `SharedMemoryResource`) took care of this via a separate lock-file.
   We consider, the combination of the above two mechanisms as a flawed approach.
   Signaling the service instance as available, but then blocking consumer access via a separate lock is weak.
   The service should simply **not** be detected as available until the service is fully usable in the first place!
   Needing to checking lock-files, is an architectural break and layer violation. Since these files are created by a
   different layer (`lib/memory/shared`).
   Besides that, we already failed miserably to even make sure, that the shm-object
   is completely setup, when the lock-file gets removed! This is because of a `LoLa` internal layering. The
   skeleton creates the shm-objects, but part of its initialization is delegated to members (events/fields/methods) of
   the skeleton! Long story short, having more control over service instance visibility and availability turned out to
   be essential for a robust crash recovery. Thus, strong coupling between the shm-objects and the provided service
   instance is undesirable.

### Required variants of service instance supervision

The find-semantics to be supported for our `mw::com::impl::lola::Proxy` are strongly coupled to the `mw::com` and
`ara::com` API semantics:

- `FindService` for a specific single service instance
  - synchronously/one-shot
  - asynchronously/continuously
- `FindService` for a number of service instances (FindAny instance/wildcard search)
  - synchronously/one-shot
  - asynchronously/continuously

Additionally, we need a supervision for service availability even **after** the user has done all synchronous
`FindService` calls and already stopped all (asynchronous) `StartFindService` jobs.
Reason: After the user has successfully created a proxy instance from one of the hits or results of one of the
`FindService` or `StartFindService` API variants, this proxy instance requires some supervision, whether its remote service
(skeleton) is still being offered or not!
This supervision is needed to update the subscription state of the events and fields contained within the proxy instance
as well as to drive the mandatory "auto-reconnect" functionality in case the service instance went down and came up
again.

### Representation of service availability within the `tmp` file system

Offering a service instance, i.e., notifying its availability, shall be done by creating a file under
`/<tmp>/mw_com_lola/<service id>/<instance id>/<PID provider>_<QM|ASIL-B>_<some_unique_part>`. Where the filename shall
start with the `PID` of the offering process followed by an underscore and then a part, which tells, whether the
service instance is offered in `ASIL-B` quality and therefore automatically also with `QM` or in `QM` only and a unique
part.

The `PID` is an essential element as `LoLa` processes communicate via message_passing. Each `LoLa` process has one
receiver endpoint, which is identified by a specific name containing the `PID`. Currently, `LoLa` exchanges the `PID` by
placing it in a defined location in the shared-memory. In the future it can be very helpful, if a client/consumer
directly gets the `PID` of the provider via `Service Discovery` without the need to first mount a shm-object.

The `QM`/`ASIL-B` part is also essential as a consumer is not searching only for a specific service-id/instance-id, but
also has requirements regarding the provided safety-level!

The need for the trailing unique part in the filename has to do with [notification mechanism](#notification-mechanism)
used to implement our `Service Discovery` and is explained in-depth in this chapter.
The trailing unique part can either be created via some monotonic counter/timestamp or an OS built-in feature like
[mkstemp()](https://www.qnx.com/developers/docs/7.1/#com.qnx.doc.neutrino.lib_ref/topic/m/mkstemp.html) can be used.

Example: In case we have a service instance of a `mw::com`/`LoLa` service, with the service-id **2376** and an
instance-id **3** and the PID of the process containing the service instance is **766** and the provider offers the
service in `ASIL-B` quality (and thereby implicitly also in `QM` quality), then the LoLa skeleton instance
providing this service, will create a file `/<tmp>/mw_com_lola/2376/3/766_ASIL-B_237765429`
during `Skeleton::FinalizeOffer()`.
The reason, that the service-id and instance-id is represented in form of a directory and then a concrete instance is
represented by a file within the `<instance id>` directory is **performance**, which will be detailed in chapter
notification-mechanism.

#### Requirements for `/<tmp>`

There is a reason, why we used angular brackets for `<tmp>`! The concrete location for <tmp> depends on the operating
system! Our concrete requirements for the `/<tmp>` location in the file system are:

1. it shall allow us to create subdirectories,
2. it shall support `flock()` operation,
3. it shall support `inotify()`,
4. it shall be cleaned-up during restart, or not be persistent at all.

While under Linux these requirements are typically all met by the file system mounted under `/tmp` location, it is
different under QNX. In QNX `/tmp` is typically just a link to `/dev/shmem` and the ``/dev/shmem`` file system is quite
restricted and doesn't support most of our requirements!
So the solution for QNX will be:

- Create a `io-blk.so` based file system on a `devb-ramdevice`

The usage of `io-blk.so` provides support for requirements 1. - 3., while basing it on `devb-ramdevice` gives us:

- automatic cleanup after reset/reboot
- fast access (as it is completely RAM based)

The amount of RAM/memory allocated to the `devb-ramdevice` doesn't need to be large as we are only creating
"marker files" with minimal size.

### Notification Mechanism

As laid out [here](#required-variants-of-service-instance-supervision) we need to continuously monitor the service
availability (or the unavailability) for various use cases. One solution could be to cyclically poll/read the directory
structure under `/<tmp>/mw_com_lola/`, but since every `mw::com`/`LoLa` application in the system would do such a high
frequent polling of this directory content, this would get very inefficient soon.

So instead we will use the `inotify` mechanism, which allows registering for notification of certain events on directory
or file level in file systems, which support it. The usage of this mechanism is portable as Linux and QNX both support
it with identical APIs.

- [inotify under Linux](https://man7.org/linux/man-pages/man7/inotify.7.html)
- [inotify under QNX](https://www.qnx.com/developers/docs/7.1/#com.qnx.doc.neutrino.lib_ref/topic/i/inotify_init.html)

Which exact file system type in both OS do support `inotify` differs. F.i. one important difference is, that
under Linux also the `/dev/shm` file system is supported, while this is not supported in QNX. This is btw the reason, why
we require the usage of the `tmp` file system.

#### Central inotify instance per LoLa process

`mw::com`/`LoLa` shall set up one `inotify` instance (via `inotify_init()`) in case there is at least one proxy instance
within the application.
Note: In a `LoLa` app context, where only servers/skeletons exist, no supervision of `/<tmp>/mw_com_lola/` is needed at
all, because only proxies/consumers rely on `FindService`/auto-reconnect mechanisms, which need such a notification.

#### Notifications on file level

Create and delete (`IN_CREATE` and `IN_DELETE`) notifications on the file level shall be supervised for a file
representing exactly a specific service instance after a proxy instance has been created for this service instance.
This is needed to monitor subscription states and drive "auto-reconnect" of the related proxy instance.
This means for each created proxy instance a supervision has to be added (`inotify_add_watch`) to get notified, when the
file gets deleted signalling a "stop offer" (this leads to subscription loss on the proxy side) and if it gets created
again (which then drives the "reconnect" and renewal of subscriptions).
This notification shall be removed again (`inotify_rm_watch`), when the proxy instance gets destroyed.

#### Notifications on directory level

Create and delete (`IN_CREATE`/`IN_DELETE`) notifications on directory level shall be supervised in case of a
continuously `Find`/`FindAll` action taking place [see here](#required-variants-of-service-instance-supervision).

##### FindConcreteInstance Search

If a Proxy made a `FindService` call for an `InstanceSpecifier` or `InstanceIdentifier` being mapped in the
configuration to a "concrete instance id", then a supervision of the directory
`/<tmp>/mw_com_lola/<service id>/<instance id>` needs to take place.
In case this directory does not yet exist, the proxy side `service discovery` instance has to create it.
Then this directory shall be supervised by adding watches for `IN_CREATE` or `IN_DELETE`. Which respectively detect
creation or deletion of files matching the given
[filename pattern](#representation-of-service-availability-within-the-tmp-file-system), and notify about a service
instance offer or stop-offer, of the concrete instance searched for.

##### FindAnyInstance Search

If a Proxy made a `FindService` call for an `InstanceSpecifier` or `InstanceIdentifier` being mapped in the
configuration to "any instance id", then a supervision of the directory `/<tmp>/mw_com_lola/<service id>` needs to take
place. That means, whenever an `<instance id>` specific sub-dir gets created within the specific `<service id>`
directory, the `inotify` based watch-supervision has to get woken up, detect this new `<instance id>` specific
subdirectory and check, whether a file exists within this directory with the given pattern described in
[this chapter](#representation-of-service-availability-within-the-tmp-file-system).
If such a file representing a specific instance id of a given service id with a given quality matches, then a hit for
the given `FindAny` search is generated and the user provided `FindServiceHandler` has to get called.
At the same time, when the creation of a specific instance id subdirectory has been detected also a supervision
for this new directory has to be additionally added to the supervision just like in the
[FindConcreteInstance Search](#findconcreteinstance-search) case.

So - the general idea for the extensive subdir creation is, to set up efficient supervisions for the concrete find use
cases and to avoid unnecessary wake-ups of the thread per LoLa process, which supervises the added watches.

#### Offer - StopOffer - Offer sequence

When looking at a real-world use case, where a given LoLa process has the following sequence during its lifetime:

1. Offer Service with `<service id>` and `<instance id>`: Directory structure under `/<tmp>/<service id>/<instance id>`
   gets created (if not yet created by a proxy side service discovery, which is searching exactly for `<service id>`
   and `<instance id>`). Then the marker file `<PID>_<QM|ASIL-B_<unique part>` gets created within this directory.
2. Stop Offer Service: The marker file `<PID>_<QM|ASIL-B_<unique part>` gets deleted from this directory.
3. Re-Offer Service with `<service id>` and `<instance id>`: The marker file `<PID>_<QM|ASIL-B_<unique part>` gets
   created within this directory. Note, that the "unique part" now changed! This is important for potential `inotify`
   listeners: Depending on, when they listen or read out the change-events from the `inotify` subsystem a reciprocal
   operation (remove file A, create file A) could be simply filtered out! This would mean, that the "listeners" would
   miss the stop-offer or re-offer toggling, which isn't acceptable! The differing "unique part" avoids this problem!

#### Contract for user provided FindServiceHandler calls

A user provided `FindServiceHandler` needs to be called, whenever the current matches for the FindPattern change. I.e.,
whenever a service instance shows up newly or disappears again. This needs some state management on the
`service discovery` side per LoLa process.

#### Threading needs for Notifications

As described in [Central inotify instance per LoLa process](#central-inotify-instance-per-lola-process), we only have
at max one `inotify` instance per `LoLa` app. For this we need one single worker thread, which listens on the
corresponding `fd` created from `inotify_init()` via poll/epoll/select. When a notification happens, this thread will
process it and in this context calls:

- either into `mw::com` (`LoLa`) specific functionality
  - updating subscription state of events and fields
  - or "auto-reconnects" to the newly showing up skeleton instance
- or also into user layer code:
  - `StartFindServiceHandler` provided by user
  - `SubscriptionStateChange` handler provided by user

This has the following implications:

- as long as this thread executes such functionality, it can't react to any new incoming `inotify` notifications
- if it is busy for too long, before again listening on `inotify` notifications, it could lose such notifications (event
  queue overflow - see **Limitations and caveats** in the Linux related link in
  [Notification Mechanism](#notification-mechanism))
- there is the need for a clear AoU and documentation towards the user, that the runtime of his callbacks has a severe
  impact here. We currently don't plan to spawn a new execution context for these user callbacks and handlers.

### Handling of existing service instance marker files

In case a `mw::com`/`LoLa` skeleton detects on `Skeleton::OfferService()`, that a corresponding file under
`/<tmp>/mw_com_lola/<service id>/<instance id>` already exists, there can be two reasons:

1. This service instance marker file is a leftover of a previous run of an app containing the corresponding service
   instance, but this app has crashed, without removing the service instance marker file.
2. There is already another instance of the very same service type/instance id running on the ECU.

In both cases the `<PID>` part of the marker file will most likely be different.
There are mechanisms to prohibit (2), which are explained in our `Crash Recovery` mechanisms! This means,
that we expect, that only (1) can be the reason, and therefore it is safe, that the newly being offered service instance
(`lola::Skeleton`) "takes over" this service instance marker file.
This basically means a deletion of this marker file and a re-creation exactly as described
[here](#offer---stopoffer---offer-sequence).

## Crash Recovery

`Crash Recovery` means, that we can have a number of `mw::com`/`LoLa` applications communicating with each other via
proxy and skeleton instances within the applications, and if any of those applications crashes, we can restart this
application, so it is able to rejoin this communication group.

Initially we had no support for this! The crash of one app in such a communicating group required the restart of the
whole group (which typically boiled down to an ECU restart).

### Challenges

The reasons for these rather harsh initial constraints were:

1. In case an application containing a proxy instance crashes, it leaves the shared-memory object for `CONTROL` in an
   undefined state. I.e. it has a certain amount of event/field slots occupied/referenced. When this app now starts up
   a 2nd time and maps the same `CONTROL` shm-object again, it couldn't correct/withdraw its allocations from the 1st
   run. If it would then directly re-subscribe with some max-sample-values to events/fields without withdrawal of
   previous references it would either get an error, because event/field slots are already exhausted or even worse: It
   would get those resources at the expense of other apps (their proxies), which start up later and then do not get
   needed resources.
2. In case an application containing a skeleton instance crashes, it leaves its shared memory objects for `DATA` and
   `CONTROL` in `/dev/shm` (`/dev/shmem`). When the crashed application restarts and encounters, that the shm-objects it
   wants to create already exists, it is unclear, whether it is a left-over from a previous crash of the very same
   application or whether there is a 2nd application running, which provides exactly the same service instance! From a
   functional-safety perspective, this is a severe issue! So a simple "take over" of existing shm-objects under the
   assumption, that they are left-overs, while in reality there is another app providing the same service instance
   already would be a severe safety issue.
   Just for "completeness": If it would be without a doubt, that the shm-objects are left-overs, then taking them over/
   re-initialize them again would be doable for the skeleton side due to the nature of the data-structures located
   within the shm-objects! Details are explained [further down](#cleanup-of-shm-objects-of-previous-crash)

   A crashed skeleton instance also takes with it some important information about its consumers:

    - Current numbers of subscribers per event/field (needed to decide whether new/further subscriptions are possible)
    - max-sample-counts of those subscriptions (needed to decide whether new/further subscriptions are possible)
    - which proxy/proxy-side field/event needs to get an update notification on changes

   This information isn't stored within shm-objects, so there is some other approach needed to recover this state, which
   is laid out [here…](#recovery-of-subscription-status).

Based on this, there are exactly two things, which have to be solved, to allow the restart of proxies/skeletons and
rejoining existing communication relations:

1. A proxy instance needs to sync/cleanup its slot occupation/references in `CONTROL` shm-object
2. A skeleton instance needs to correctly identify, whether a 2nd instance with the same instance id is currently active

Since a solution for (2) is easier to achieve, we start with it.

### Solution for service provider crash

#### Detection of concurrently active service instance

The idea to unambiguously detect, whether there is already another instance for the very same service instance active
or not and therefore existing shm-objects in `/dev/shm` (`/dev/shmem`) are:

- left-overs of a regularly stopped service instance (which might also leave its shm-objects in the future according to
  [adapted behaviour](#new-unlink-semantics-for-shm-objects))
- left-overs of a crashed service instance
- currently in-use by an active service instance

is to use OS mechanisms to "mark files" in a way, that this "marking" is automatically withdrawn by the OS as soon as
the application having marked it, exits/crashes.

The mechanism to be used here is `flock`:

- [Linux manpage](https://man7.org/linux/man-pages/man2/flock.2.html)
- [ONX manpage](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/f/flock.html)

It provides a mechanism to place a locks (unique or shared) on a file, which gets automatically revoked by the OS in
case the process, which applied the lock, dies.

We are intentionally **not** locking the shm-objects itself for the following reasons:

1. shm-objects under `QNX` are located in `/dev/shmem`, which is a file-system, which doesn't support `flock`.
2. we do have two or three (in case of ASIL-B) shm-objects per service instance. So it is semantically unclear, how we
   should use this locking approach via `flock` exactly (lock all shm-objects, only one specific, what happens, when we
   detect  partial locking, ...?)
3. Most important: We need the locking mechanism explicitly on the shm-objects for a slightly different mechanism, which
   is described [here](#usage-indicator-of-shm-objects) and would clash.
4. This locking mechanism described here is by no means "binding specific"! I.e. the necessity to avoid duplicated
   offerings and to distinguish a restart-after-crash from a normal restart is a functionality, which should be provided
   by all bindings (and if possible even binding independently)! Using locking on shm-binding specific artefacts is
   contradicting to some extent.

Instead, we use one lock file per service instance under `/<tmp>/mw_com_lola`, where the name pattern is:
`<service id>_<instance id>_lock`

The idea is to use this lock-file exactly as you would use e.g. a `std::lock`:
When trying to offer a service instance the `mw::com` skeleton first tries to acquire its lock-file. If this already
fails (indicating, that another active instance within the ECU is already offering the very same service instance), the
offering is aborted. If it succeeds the whole offering process (which further down is very binding specific) gets
executed. The lock-file is only withdrawn, when `StopOffer()` gets called explicitly or implicitly by destruction of the
skeleton or by a crash of the app/process, where then the OS withdraws the lock on the file.

The `OfferService` activity for a `lola::skeleton` is shown here in more detail:

![Activity Diagram View](http://www.plantuml.com/plantuml/proxy?cache=no&src=https://raw.githubusercontent.com/eclipse-score/communication/master/score/mw/com/dependability/software_architectural_design/partial_restart/assets/offer_service.puml)

**Explanation**:
The skeleton instance tries to create the LOCK file under `/<tmp>/mw_com_lola`. If it gets another error than `EEXIST`,
it already aborts the service offering with a failure. In both cases &ndash; file already exists or creation was
successful &ndash; the skeleton tries to ****exclusively**** lock the file via `flock`. If this fails, it is a clear
sign, that a 2nd skeleton is already actively offering the same service instance, so the offering is aborted with
failure. If lock acquisition succeeds the skeleton instance proceeds with its actions needed before making the service
instance visible to potential clients/proxies via service-discovery: In case of our `LoLa`/shared-mem binding this
comprises either creation of the related shm-objects or taking them over from a previously stopped or even crashed
service instance according to
[Cleanup of shm-objects of previous crash](#cleanup-of-shm-objects-of-previous-crash).
Finally, if all this is done, it publishes its service-instance to the service-discovery via creation of the
corresponding markup file under `/<tmp>/LoLa/<service id>/<instance id>`.

#### Cleanup of shm-objects of previous crash

There are the two types of shm-objects we have per service instance: `DATA` and `CONTROL`. From the latter there could
be one or two instances depending on the ASIL level, the service instance supports.
A shm-object for `DATA` can be re-used "as is" from a newly starting service provider, which detects left-overs from a
previously regularly stopped (see [here](#new-unlink-semantics-for-shm-objects)) or crashed service instance.
Shm-objects for `CONTROL` have to be "cleaned up" first, when taking them over. The previous skeleton instance might
have marked some event/field state slots as being `IN_WRITING`, before it was writing new event/field data into the
`DATA` shm-object and then have been crashed before it was updating state slot with a new/valid timestamp again.
Cleanup in the `CONTROL` done by the skeleton instance is simple: Iterate over all status slots of events/fields and
change any state from `IN_WRITING` to `INVALID`.

Note, that this take-over of existing shm-objects happens also in the "good case" without a service instance provider
crash. In this case iteration over all the status slots of events/fields will just never find a slot in state
`IN_WRITING`! So there needs to be no distinction regarding functionality: Taking over of shm-objects left over from a
regularly stopped service vs a crashed service.

#### Recovery of subscription status

The restarted skeleton instance, directly after having taken over the shm-objects from a previously crashed (or
regularly stopped) service instance, has no knowledge about how many subscribers, with what `max-sample-count`, the
previous skeleton instance before the crash actually had.
This was already mentioned in the [challenges overview](#challenges) under (2).

Since this information is not maintained in the shm-objects, it needs to be setup/recovered after the shm-objects of
the crashed service instance have been taken over. The mechanism, to accomplish this, relies on service discovery! I.e.
the provider side/skeleton instance just makes sure to toggle its service availability (of its recovered service
instance): Directly after having recovered/taken over the shm-objects, it removes the service-discovery visibility of
the service instance (in case it was still there because of a crash) by removing the service instance marker file and
recreates it, which has been already described in
[Handling of existing service instance marker files](#handling-of-existing-service-instance-marker-files).

#### New unlink semantics for shm-objects

Previously a service instance when going down (i.e. a `lola::Skeleton` in `PrepareStopOffer()`) unconditionally called
`SharedMemoryFactory::Remove(path)`, which lead to unlinking of the shm-objects from the file system. This had the
following effect:

- the shm-object wasn't visible in `/dev/shm` (`/dev/shmem`) anymore. So newly upcoming proxies couldn't mmap/subscribe
  to this service instance anymore.
- proxies (applications with these proxies), which already had opened/mem-mapped those shm-objects were unaffected! They
  still could access the mapped memory (which is intended!). They would obviously not get any data updates in this
  shm-object as the producer has already gone. Only, when the last proxy having this shm-object open, has been destroyed
  and closed its shm-object descriptor, the OS can free/reclaim the underlying memory resources.

For our extended service-discovery and restarting capability we now change this slightly: The `lola::Skeleton` shall
only conditionally call `SharedMemoryFactory::Remove(path)` (and therefore unlink the shm-object) if there are currently
no proxies using this shm-object. This change is needed for normal restart behavior (stop-offer and then offer again)
of a service instance: We require a providing service instance (`lola::Skeleton`) to re-use its shm-objects from a
previous Offer() for a new Offer(). So &ndash; as pointed out in
[Cleanup of shm-objects of previous crash](#cleanup-of-shm-objects-of-previous-crash) &ndash; taking over existing
shm-objects at `lola::Skeleton::PrepareOffer()` is now a general behavior and not only a crash-recovery functionality.
The need for this comes from the proxy side: If we have a proxy, which is "connected" to a skeleton instance at one
point in time, has already subscribed to an e.g. event and has even received `SamplePtrs` from this event, which point
into the shm-object, you want to keep exactly those shm-objects!

If the skeleton stops offering and goes down regularly, the proxy shall still be able to access/follow the `SamplePtrs`
he is holding! And if the provider/skeleton re-offers again, the proxy shall silently "auto-reconnect" as per the
AUTOSAR specification. But this means, that the `lola::Skeleton` must not create new shm-objects in a new offer, but
re-use the existing one! Because a proxy can't open/mmap two versions of the (semantically) same shm-objects.
That means, the proxy could only switch to the usage of the new shm-objects, in case he hasn't any remaining
`SamplePtrs` pointing into the "old" shm-objects! This however, would mean, that we force the user to destroy
any `SamplePtrs` he holds after the subscription-state changes to support the "auto-reconnect", which is impossible,
because the idea of "auto-reconnect" is to work **seamlessly**.

##### Usage indicator of shm-objects

So to conditionally decide, whether the `lola::Skeleton` shall call `SharedMemoryFactory::Remove(path)` (and therefore
`unlink` and potentially release resources), some kind of usage indicator is needed.
For this we again use the `flock` mechanism! `LoLa` proxies, when being constructed and then opening and mem-mapping the
shm-objects of the providing service instance shall place a `shared-lock` via `flock` on the `service-instance-usage`
marker file. This has been created by the skeleton next to the `service-instance-existence` marker file
(see [here](#detection-of-concurrently-active-service-instance)) under
`/<tmp>/mw_com_lola/<service id>_<instance id>_usage`.
So the skeleton has during stop-offer to check whether it can place a unique lock via `flock` on the
`service-instance-usage` marker file. If it is successful, then no proxy is currently using the service instance and
the skeleton can unlink the shm-objects related to it.

**Note**: Since it is generally possible (although a rather pathological case) to have several proxy instances in a
process, which are proxies for the same service instance, the `flock` usage has to be coordinated in case several proxy
instances use the same file-descriptors of the same shm-objects in this case!

The usage of shared-locks on `service-instance-usage` marker file to decide on the skeleton side, when to call `unlink`
or not is shown in the following activity diagram for "stop offer of a service instance":

![Activity Diagram View](http://www.plantuml.com/plantuml/proxy?cache=no&src=https://raw.githubusercontent.com/eclipse-score/communication/master/score/mw/com/dependability/software_architectural_design/partial_restart/assets/stop_offer_service.puml)

### Solution for service consumer crash

As noted in [Challenges under (2)](#challenges) the central problem to be solved, when a proxy instance (the app
containing it) crashes/terminates non-gracefully, is to restore the state in the `CONTROL` shm-object during the restart
of this proxy instance.

This essentially means: Before doing any event/field subscriptions again, directly after proxy creation and mem-mapping
the `CONTROL` shm-object again, the proxy has to withdraw/revert all the effective slot-refcount increments it had done
in its previous run. "Effective" here means remaining increments after all increments/decrements done on a given slot.

To be able to do this, the proxy has to "persistently" note down, what slot-refcount increments it has effectively done.

#### Transaction Log

We call this "noting down" of all the slot-refcount increments and decrements a transaction log. The resemblance to
database systems and their notion of transactions/transaction logs is intended! Because we also - like in database
systems - want to be able to roll back transactions. I.e. a transaction in this case is a specific slot refcount
increment or decrement. The sum of all those slot refcount increments/decrements is the transaction log.

This is exactly what happens, when a proxy restarts, mem-maps the `CONTROL` shm-object and then rolls back the
transaction log! From last entry to first entry, exactly in the reverse direction.

##### Writing the transaction log

Such a transaction log is written by a proxy instance per each event/field, which has a related `lola::EventDataControl`
structure in the following way:

- Whenever a slot index is going to be referenced (refcount increased) in the context of
  `lola::EventDataControl::ReferenceNextEvent()` **before** the increment is done a transaction entry is created and
  being marked as `TX-BEGIN`.
- When the specific slot index has been incremented, this slot index info together with the increment action gets added
  to transaction entry and the transaction entry gets marked as `TX-END`.

Semantically equivalent action happens on decrements:

- Whenever a slot index is going to be de-referenced (refcount decremented) in the context of
  `lola::EventDataControl::DereferenceEvent()` **before** the decrement is done a transaction entry is created and being
  marked as `TX-BEGIN`.
- When the specific slot index has been decremented, this slot index info together with the decrement action gets added
  to transaction entry and the transaction entry gets marked as `TX-END`.

##### Rolling back the transaction log

When a proxy instance restarts, mem-maps the `CONTROL` shm-object, it first has to check all its transaction logs of all
events/fields of the service instance: If it detects a corrupt (not completely written) transaction log entry, recovery
by a transaction log roll back isn't possible! I.e. the proxy instance creation shall fail.

Reason: If the proxy finds an incomplete transaction log entry, it is unclear, when exactly during the writing of this
entry the application/proxy in the previous run crashed! **Before** or **after** making the slot refcount change in the
`CONTROL` shm-object. And therefore the state can't be correctly reset!

If all transaction log entries are complete/intact, the proxy has to roll back each entry from the newest to the oldest.
The steps to be done to roll back an entry are:

1. From the current entry remove the `TX-END` marker.
2. Do the inverse operation to the operation noted in the current entry:
    - In case there was an action "increment slot 5", then apply a "decrement slot 5" in `lola::EventDataControl`
    - In case there was an action "decrement slot 5", then apply an "increment slot 5" in `lola::EventDataControl`

3. delete the complete entry, as it has now successfully rolled back.

**Note**: The reason for step 1 is, that with this step we mark the entry intentionally as incomplete/corrupt! So if we
crash again during this rollback application, we make sure to leave the service instance in an invalid state, so that
this gets detected in the next re-start and hinders this proxy instance to resume successfully!

If all transaction logs of the proxy instance have been rolled back successfully, the proxy instance can be successfully
created (`Proxy::Create()` succeeds).

#### Allocation and (optimized) structure of the transaction log

Transaction logs shall be placed within the `CONTROL` shm-object to which they logically belong. Each event/field
within the `CONTROL` shm-object has a set of transaction logs assigned (for each proxy subscribed to the event/field)
This solves the following issues:

- the logs have to be "persistent" over client/consumer/proxy crashes. Placing it into the provider side created
  `CONTROL` shm-object fulfills this and doesn't require new specific files/locations per proxy to be maintained.
- access should be fast and not need explicit slow I/O flushing, which would be needed, in case we would place the
  transaction logs in regular files.

The transaction logs belonging to a specific proxy instance shall be identified by `uid` of the proxy-process and
the `instance specifier` of the proxy instance within the process (to support the pathological/seldom case of multiple
proxy instances within one process for the same service instance).

Since the transaction logs are to be placed in `CONTROL` shm-object created by the provider, he has to allocate the
needed space.
Therefore, we need an estimation of upper bound of storage need. This requires some optimization from the abstract
"layout" described in [Writing the transaction log](#writing-the-transaction-log), where such a log could grow arbitrarily in size, if for each
slot-ref-count increment/decrement a new transaction entry would be created. Because of some properties of the
ref-counting mechanisms in DataControl:

- every slot-index can be **only** referenced **once** by a proxy instance
- upper bound of slots is given in the configuration (`numberOfSampleSlots`)
- maximum number of subscribers/proxy instances is given in the configuration (`maxSubscribers`)

the following (aggressive) optimizations are possible:

1. Space for `maxSubscribers` transaction logs has to be pre-allocated.
2. Transaction log for a given proxy instance has a fixed size of `numberOfSampleSlots` entries and is therefore
   organized as a fixed size array `tx_log[numberOfSampleSlots]`.
3. The index of the array represents the slot-index.
4. An array element just consists of 2 bytes: 1st byte represents `TX-BEGIN`, 2nd byte represents `TX-END`.
5. A value of 0 for `TX-BEGIN` or `TX-END` means NO ``TX-BEGIN`` resp. `TX-END` happened.
6. A value of 1 for `TX-BEGIN` or `TX-END` means a `TX-BEGIN` resp. `TX-END` happened.
7. Initially all array entries are 0, 0.
8. In an implementation we could even further optimize for space by using `bitfields`.

The transaction logic during a slot-index refcount increment, based on this optimization, is then as follows:

- If `lola::EventDataControl::ReferenceNextEvent()` is going to atomically increment slot 3:
  - 1st byte of tx_log[3] gets set to 1
  - then atomic increment in `lola::EventDataControl` happens
  - 2nd byte of tx_log[3] gets set to 1

The transaction logic during a slot-index refcount decrement is then as follows:

- If `lola::EventDataControl::DereferenceEvent()` is going to atomically decrement slot 3:
  - 1st byte of tx_log[3] gets set to 0 (was 1 before, because a previous increment transaction has happened)
  - then atomic decrement in `lola::EventDataControl` happens
  - 2nd byte of tx_log[3] gets set to 0

So effectively a completed dereference-transaction wipes out a previous reference-transaction!

**Note**: We reversed the order of setting `TX-BEGIN`/`TX-END` in the decrement transaction only for the sake of knowing
ndash when we see a corrupted transaction log ndash whether an increment or decrement transaction stalled! I.e. if we
find a transaction log entry with `TX-BEGIN` 1 and `TX-END` 0, we know, that an increment transaction stalled, whereas
if we see a `TX-BEGIN` 0 and a `TX-END` 1, we know, that a decrement transaction stalled.

##### Sample of initial transaction log

In case we have a `numberOfSampleSlots` value of **10** for a given event, then the transaction log of ALL
`maxSubscribers` proxy instances looks as follows:

```c++
tx_log_<event_id>_<subscriber_no>[10] = {
{0, 0},
{0, 0}
{0, 0}
{0, 0}
{0, 0}
{0, 0}
{0, 0}
{0, 0}
{0, 0}
{0, 0}
```

The transaction log would also look **exactly the same** after a row of slot-ref increments/decrements,
if the proxy instance has reached a state again, where he isn't currently referencing any event/field samples.

##### Sample of correct transaction log

As in the example before, we again have a transaction log of a proxy instance, but this time it contains some (valid)
transaction entries:

```c++
tx_log_<event_id>_<subscriber_no>[10] = {
{0, 0},
{0, 0}
{1, 1}
{0, 0}
{0, 0}
{0, 0}
{1, 1}
{0, 0}
{0, 0}
{1, 1}
```

So in this case, we have a transaction log containing three increment transaction entries for slot-index 2, 6 and 9.
So when doing a rollback of this log, these slot-indexes would be decremented by 1.

##### Sample of corrupt/invalid transaction log

As in the example before, we again have a transaction log of a proxy instance, but this time it contains an invalid
transaction entry at index 6, which is a clear sign, that the proxy (app) crashed during incrementing slot index 6:

```c++
tx_log_<event_id>_<subscriber_no>[10] = {
{0, 0},
{0, 0}
{1, 1}
{0, 0}
{0, 0}
{0, 0}
{1, 0}
{0, 0}
{0, 0}
{1, 1}
```

##### Essential further optimizations

1. We shall place the transaction-logs (which are small because of our optimizations shown in the previous chapter) in
   direct "neighborhood" of the `lola::EventDataControl` &ndash; maybe as an additional member! Simply, because from now
   on, every write-access to the control-slots for an event/field by a consumer are accompanied by two write accesses to
   the event/field related We aim for having it in the same memory-page (or even better directly adjacent cash lines)!
2. The transaction-log write and shared state update shall be as efficient (minimum amount of needed instructions) and
   compact (not doing anything other in between) as possible, to minimize the likelihood of a crash during such a
   transaction, which would lead to an incomplete/corrupt transaction-log entry, which would hinder a restart of this
   proxy instance.

#### Withdraw subscriptions

Proxies have subscribed with a certain `max_sample_count` at the skeleton events/fields before their crash. This has
two effects to the state of the provider/skeleton side:

1. The number of available slots for subscription has been reduced.
2. The skeleton side process had been in contact with the proxy side process **before** its crash and created a
   `message_passing` sender for the PID the proxy side process had before the crash.

So all subscriptions been done by proxies of a process `before` the crash have to be withdrawn (unsubscribe has to be
called) in the restart. So for each "initialized transaction log", which is found in the CONTROL section assigned to the
uid of the crashed/restarting process and unsubscribe() has to be done. Since unsubscribe() needs to contain the
`max_sample_count` of the previous subscribe, this information needs also be placed in the tx-log!
Additionally, the PID of the restarting process has changed. But since the provider/skeleton side process has stored a
`message_passing` sender instance for the old PID of the consumer process before the crash, this has to be cleaned up
also: This shall be solved with an extension to the existing `unsubscribe message`, which is sent from the proxy side
to the skeleton side: It shall contain additionally an optional "old PID" value. If the skeleton side process receives
such an extended `unsubscribe message`, it can remove old subscription artifacts stored under the "old PID".
Therefore, a proxy has also to store its current PID within the `CONTROL` section, so it can then be used for the
extended `unsubscribe` in the crash recovery case.
Since a crashed proxy application can have multiple associated transaction-logs (for each event/field it used from the
provided service instance) its `UID->PID` mapping isn't stored in the transaction-log, but one level above in the
`lola::ServiceDataControl` object, which is the root-object of the `CONTROL` shm-object.
So the idea is, that proxy instances register themselves after having mem-mapped the `CONTROL` shm-object within
`lola::ServiceDataControl`.
Since this is a potential concurrent write-access to a location in shared-memory, care has to be taken regarding
synchronization. Since we &ndash; for now &ndash; avoided to use mutex (pthread-mutex to guarantee portable
inter-process synchronization) based synchronization/locking as it scales badly from experience with QNX, the
synchronization should preferably be done with lock-free/atomics based algorithms/data-structures (like in other places
in `LoLa`).
