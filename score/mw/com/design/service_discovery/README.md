# Service Discovery

The main goal of service discovery is to provide and require necessary services at runtime. Meaning, a skeleton
(service provider) will `offer` services, while a proxy (service consumer) will `find` services. In order to do so, a
central registry is necessary where currently offered services are stored. In contrast to service discovery at runtime,
the provided and required services could be connected at configuration / compile time. This would avoid the necessity to
perform service discovery, but comes with the drawback that the system fully relies on the exact service instance to
be present. This shall be avoided in order to allow a more dynamic deployment of adaptive Applications into the system.

## Use Cases / Customer Functions

First, all applicable use cases of the service discovery shall be noted.

For the skeleton side:

1. A service skeleton is able to offer a service. [SWS_CM_00101]
2. A service skeleton is able to stop offering a service. [SWS_CM_00111]
3. A service skeleton can crash, causing an implicit stop on offering the service.
4. A service skeleton can shut down, causing an implicit stop on offering the service.

For the proxy side:

1. A service proxy is able to find a currently offered service.
    1. Identifying the service instance by an `InstanceIdentifier`. [SWS_CM_00122]
    2. Identifying the service instance by an `InstanceSpecifier`. [SWS_CM_00622]
    3. Without identifying a specific service instance.

2. A service proxy is able to register a callback for invocation, if a service is found.
    1. Identifying the service instance by an `InstanceIdentifier`.
        1. Without a custom execution context. [SWS_CM_00123]
        2. With a custom execution context. [SWS_CM_11352]
    2. Identifying the service instance by an InstanceSpecifier.
        1. Without a custom execution context. [SWS_CM_00623]
        2. With a custom execution context. (Not specified in adaptive AUTOSAR)

3. A service proxy is able to stop finding a service. [SWS_CM_00125]

4. A service proxy can shut down, causing an implicit stopping of finding services.

On top of these functional use-cases, other use-cases exist that are linked to safety related separations of concerns.

5. A ASIL-QM application, is able to find services that are offered by ASIL-B applications.
6. A ASIL-B application, is able to find services that are offered by ASIL-QM applications.

## High-Level Architecture

The high-level architecture for the service discovery is
explained [here](../../../../docs/features/ipc/lola/partial_restart).
In summary, we utilize a folder on a non-persistent filesystem partition as our central registry. Each service instance
will be represented by a flag file, and the path towards these files represent all necessary identification properties
(e.g. service id).

## Implementation

The following section gives a written explanation for the structural view that is illustrated in the following diagram.

<img src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/design/service_discovery/structural_view.puml" />

All possible user interactions with service discovery related functionalities are service specific. Since an API user
should not bother about implementation specific representations of services (e.g. the service identifier),
the `mw::com` utilizes strong typing and enables access to service discovery methods only over specific service types.
The types (proxies and skeletons) are generated based on the provided adaptive AUTOSAR Meta-Model. Within the further
reasoning, these generated units are represented by a `DummyProxy` and `DummySkeleton`.

The `DummyProxy` or `DummySkeleton` should not contain any specific logic for performing service discovery operations.
This is caused by the fact that this logic is generic and can be placed into a single responsibility, that is then used
by either one of them, not to forget the many other service types that are generated. This functionality is described
in [Binding independent Service Discovery](#binding-independent-service-discovery). Since the user has multiple ways to
identify what exact service instance shall be used, this needs to be resolved into a common internal representation.
This is further described in the next
paragraph [Identification of Service Instances](#identification-of-service-instances).

Summing up it can be said that the responsibility of our proxies and skeletons is to:

1. Identify the internal representation of the service instance in question
2. Delegate service discovery operation to the binding independent operation

It shall be noted that in order to operate in an object-oriented manner, instead of based on free functions, a `Runtime`
as singleton is introduced, that holds all necessary objects over the whole process lifetime.

### Identification of Service Instances

As already mentioned in the use cases, the user of the `mw::com`-API has multiple possibilities to specify which
specific instance of a service he is either offering or subscribing to. These different `mw::com` specific identifiers
need to be translated into an implementation specific representation. From now on called `InstanceIdentifier`. A service
instance can be only uniquely identified by a 3-tuple of service identifier, instance identifier and version number. For
our implementation purposes, we will need to extend this 3-tuple with a few more attributes. One needs be which binding
shall be used (e.g. SHM, SOME/IP, ANY) aka `BindingType` and one needs to be which kind of quality this services
expects (e.g. ASIL-QM, ASIL-B) aka `QualityType`. Latter is necessary to ensure that an ASIL-QM process does not fake an
offer an ASIL-B service. This scenario is better described in
[Partitioned Service Registry for ASIL compliance](#partitioned-service-registry-for-asil-compliance).

The translation from an `score::mw::com::InstanceIdentifier` or `InstanceSpecifier` is owned by the
`InstanceResolver`. Its design and dependencies are further described in [here](../configuration/README.md).

One last thing to clarify on the identification of service instances is the address space. Saying, how many different
services and their respective instances are allowed, and what address schemas do apply on them. In order to apply auto
generated numeric identifiers based on the unique service names in the adaptive AUTOSAR Meta Model, a big (std::
uint64_t) space for the service identifier is chosen. The other identifiers can be in a mid-range space, since they
won't be auto generated and need to be specified explicitly.

### Binding independent Service Discovery

The service discovery itself is highly binding specific, but in order to support multiple bindings in parallel, a
binding independent abstraction is introduced. This abstraction (`ServiceDiscovery`) is constructed and owned by the
`Runtime`. The necessary `ServiceDiscoveryClients` are requested via the binding independent and then binding dependent
runtime instances. Multi-Binding service discovery is not supported at this stage - but could easily be implemented at a
later point, by defining in which priority the service discovery clients shall be executed.

The current designs foresees two different binding implementations:

1. One based on shared memory (see [Shared Memory Service Registry](#shared-memory-service-registry))
2. One based on VSome/IP (see [SOME/IP Service Registry](#someip-service-registry))

As shown in [Sequential View](./sequence_view.uxf?ref=18c835c8d7b01056dd48f257c14f435795a48b7d) the `DummyProxy` interacts directly with the
`ServiceDiscovery` which directly dispatches the requests - based on the `InstanceIdentifier` - to either one or both
bindings.

<img src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/design/service_discovery/sequence_view.puml" />

In the special case of starting an asynchronous search for service instances (aka `StartFindService()`) a unique handle
(`FindServiceHandle`) needs to be returned, to enable the user to later stop this search. This identifier needs to be
unique over multiple bindings and is thus managed by the binding independent instance. After that the unique id and all
other parameters are forwarded to either bindings. It shall be noted, that this means that the callback provided
to `StartFindService` can be invoked in parallel by multiple threads and thus needs to be implemented thread safe by the
user of the `mw::com`-API.

Since the `FindServiceHandler` cannot be copied and thus given to the binding dependent `ServiceDiscoveryClients`,
we need to store it in the `ServiceDiscovery` and reference to it via the `FindServiceHandle`. To implement this,
another callback is given to the `ServiceDiscoveryClient`. Once a `ServiceDiscoveryClient` finds a specific service
instance, it needs to associate that with the respective `FindServiceHandle` and then invoke the provided `OnFound`
callback.
The callback will then construct the necessary `HandleTypes`, find the right `FindServiceHandler` in
the `ServiceDiscovery`
and invoke it. What should be especially noted is, that in case of `ANY` semantics, the `InstanceIdentifier` provided
to the `HandleType` needs to be bound to a specific setting. For example an instance id `ANY` would need to be replaced
with a found one - for example _42_. The same is true for the binding type. Otherwise, a later construction of the Proxy 
will fail.

### `LoLa` Service Discovery

The service discovery for `LoLa` utilizes an OS mechanism called `inotify` for service discovery.
The main idea was already presented in the high level architecture and the summary.

#### Worker Thread

The main building block of the `ServiceDiscoveryClient` is a long-running thread.
This thread is henceforth called worker thread.
It spins in an endless loop for the full lifetime of the `ServiceDiscoveryClient`, repeating the following tasks:

1. Wait for events generated by `inotify`
2. Update ongoing searches and their associated watches
3. Update database of available instances based on received events
4. Call `OnFound` callbacks of searches impacted by step three

To keep the synchronization logic of the `ServiceDiscoveryClient` simple, a single recursive mutex (`worker_mutex_`)
is used. This mutex is locked by the worker thread throughout tasks 2-4.

#### Finding Services

##### StartFindService

Any search for a service starts with locking the `worker_mutex_`, followed by the creation of `inotify` watches.
The directory structure for service discovery spans two levels (service id, instance id).
Searches for a specific service instance are placed on the level of instance id.
Searches for any instance of a service are placed on the level of service id.
`inotify` watches have two restrictions:

1. They can only be placed on existing directories. Hence, the necessary path for the watch is created when
   required.
2. They are not recursive. If a watch of a search is placed on the level of service id, for every instance id directory
   created within, a new watch is created and added to the existing search.

`inotify` only provides events for actions that happened after a watch was created and until it was deleted.
This means, whenever creating a new watch, the current state of the filesystem must be taken into account.
Hence, after adding a watch, the `ServiceDiscoveryClient` searches for all currently available flag files beneath the
directory on which the watch was placed.

Since every new search request adds at least one watch, there is always an initial search of the filesystem. If this
search yielded at least one available instance, the `OnFound` callback is called synchronously with the handle for the
found instances.
This allows a fast-paced discovery of already offered service instances. Irrespective on whether an instance was found
or not, the search request is stored in the set `new_search_requests_`.
Using a different store for the invocations of `StartFindService` has the benefit of avoiding deadlocks between user
threads and the worker thread. The worker thread will update the actual ongoing search requests (`search_requests_`)
in step two (see above).

##### `StopFindService`

Stopping a search (`StopFindService`) consists of:

1. Storing the `FindServiceHandle` in `obsolete_search_requests_`.
   Using a different store for the invocations of `StopFindService` has the benefit of avoiding deadlocks between user
   threads and the worker thread. The worker thread will update the actual ongoing search requests (`search_requests_`)
   in step two (see above).
2. This is followed by waiting for ongoing invocations of `OnFound` callbacks to finalize.
   To avoid deadlocks, the wait is skipped if `StopFindService` is called from within an `OnFound` callback.

##### Basic Assurances

To summarize the above, the following basic assurances are provided:

- `FindServiceHandlers` are only called within the lifetime of the `ServiceDiscoveryClient`
- `FindServiceHandlers` are called in a serialized fashion
- `StartFindService` may be called from within a `FindServiceHandler`
- `StopFindService` may be called from within a `FindServiceHandler`
- When `StopFindService` is called from outside a `FindServiceHandler`, it blocks until concurrent invocations of
  `FindServiceHandlers` have returned (if there are any).
- The `FindServiceHandler` associated to a search is no longer called once the call to `StopFindService` for this search
  has returned

#### Offering Services

Service offers and stopping them is also looped through the `ServiceDiscoveryClient`.
This way, the logic of offering and finding offers is close together, which helps keep them in sync.
Publishing a service offer boils down to the creation of one or more flag files in the appropriate directory.
A separate flag file is created for each ASIL level of the service offer. This allows partial stops of the offer.

It is imperative, that the flag files have a unique name that changes with every offer.
Otherwise, `inotify` may drop some events and corrupt the service discovery.

The full path of a flag file looks as follows: `<sd>/mw_com_lola/<service_id>/<instance_id>/<PID>_<asil>_<unique_seed>`

- `<sd>` The base service discovery directory (OS-dependent)
- `<service_id>` The id of the service
- `<instance_id>` The id of the instance
- `<PID>` The process id
- `<asil>` The ASIL level of the offer (`asil-b` / `asil-qm`)
- `<unique_seed>` A seed unique over offers in the same application run and different application runs.

To put the flag file, the full path must be created.
These directories must have file permissions set to be read-, write-, executable by everybody.
The flag file must be readable by all and should only be writable by the user offering the service.

Because of the POSIX API, creating a directory with the correct permissions can not be performed in a threadsafe atomic
manner.
Hence, simultaneously starting a search and offering an instance for the same service, may lead to race conditions.
This is mitigated by adding several retries for the creation of the directory structure.

#### Delay of the Service Discovery

On QNX `inotify` events are cyclically polled by the resource manager (fsevmgr).
The delay is configurable as
an [io-blk parameter](https://www.qnx.com/developers/docs/7.1/#com.qnx.doc.neutrino.utilities/topic/i/io-blk.so.html).
It can be specified using argument `fse-period=msecs`.

Please
see [here](broken_link_g/swh/xpad-shared/pull/25644/files#diff-efef05fd44fdac6a87092251ce97754b11cbd41612a3f3eb6791531e7c4d0a9fR10)
for how to add this parameter to io-blk.
