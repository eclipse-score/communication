# Communication Paths used by LoLa

The following mechanisms are being used within `LoLa` for communication between components:

- Shared memory for event/field/method payload data (DATA segment)
- Shared memory for access synchronization for event/field/method payload data (CTRL segment)
- message-passing
- service-discovery
- synchronization files

In any setup, where communication between components residing in **different domains** (e.g. different OS instances on
different SoCs or different VMs) shall be supported, there needs to be a solution, how each of these mechanisms can be
realized.

## Shared Memory for DATA

Each `LoLa` service instance provider creates a shared memory object, which is writeable only by itself and readable by
its consumers.
It is partitioned into "slots", each slot being used to store the payload data of one event/field.
The provider instance updates the payload data in its slot whenever the corresponding event/field is updated.

### Requirements

- shared memory object is only writeable by the provider instance.
- shared memory object is only readable for a selected set of uids.
- shared memory object is identifiable by a unique name within the filesystem (e.g. /dev/shm/ on Linux) for provider and
  consumer instances.

### Remarks

In a cross-domain setup, where **no** memory sharing is possible between domains, a gateway instance on the consumer
side needs to create its own representative DATA shared memory object and payload data needs to be copied from the provider
side DATA shared memory object into the consumer side DATA shared memory object.

In a cross-domain setup, where memory sharing is supported between domains, it shall be exploited to keep costly data
copying at a minimum.

## Shared Memory for CTRL

Each `LoLa` service instance provider creates a shared memory object per ASIL level, which is read-writeable by itself and
also read-writeable by its consumers.
It is partitioned into the same number of "slots" as the related DATA shared memory object. Each slot represents the
current state of the related data slot in the DATA segment.

### Requirements

- shared memory object is read-writeable by the provider instance.
- shared memory object is read-writeable for a selected set of uids (allowed consumers)
- shared memory object is identifiable by a unique name within the filesystem (e.g. /dev/shm/ on Linux) for provider and
  consumer instances.

### Remarks

In a cross-domain setup, where **no** memory sharing is possible between domains, a gateway instance on the consumer
side needs to create its own representative CTRL shared memory object related to the DATA shared memory object it had
created [see remarks in DATA](#shared-memory-for-data).

In a cross-domain setup, where memory sharing is supported between domains, it might be exploited to share the CTRL shared
memory object between domains. But since this is not a data intensive communication path, there is no strong need to do
so. Thus, even if memory sharing is supported between domains, a gateway instance on the consumer side might still
create its own representative CTRL shared memory object related to the DATA shared memory object, which it shares with
the provider side. it had created.

## Message-Passing

`LoLa` uses a message-passing side-channel for event-update-notifications in event/field-communication. Additionally,
message-passing is used for partial-restart functionality, and it will be a core mechanism in the upcoming service-method
implementation.

The message-passing implementation used by `LoLa` is based on:

- Unix Domain Sockets on Linux
- Resource-manager implementation based on QNX native message-passing APIs on QNX

### Requirements

- A `LoLa` application shall be able to create a message-passing endpoint under a given name visible in the filesystem.
- This endpoint shall be visible to other `LoLa` applications.
- Other `LoLa` applications shall be able to connect to this endpoint using its name.
- A `LoLa` application creating such an endpoint shall be able to restrict access to this endpoint to a selected set of
  uids.

### Remarks

Providing message-passing functionality across domains is only necessary, in case of a setup, where the DATA and CTRL
shared memory objects are already shared between domains! Then an update happening on the provider side needs to be
notified to the consumer side via message-passing.

However, in a setup, where separate DATA and CTRL objects are created
on the consumer side and data copying takes place, the update notification gets semantically created on the consumer
side and therefore only requires intra-domain message-passing.

Message-passing in `LoLa` is not very data intensive. Thus, breaking up a message-passing communication path between
domains (where this is necessary) within each gateway instance shouldn't be a big issue performance-wise.

Example: In case we would have a cross-domain setup with support for memory sharing between domains and used it to share
DATA and CTRL shared memory objects between domains. If an event-update happens (and consumers in the other domain have
registered for update notifications), on the provider domain side a message-passing update-notification would be sent
and received by the provider side gateway instance. It would then "transport" this notification to the consumer side
domain, where the related gateway instance would receive it and then forward it again using `LoLa` message-passing
again. So, `LoLa` message-passing is broken up at the gateways on domain boundary and the inter-domain gap is bridged,
with whatever mechanism is suitable in this setup.

## Service-Discovery

`LoLa` uses a service-discovery mechanism to allow service instance consumers to find provided service instances.
Technically this is implemented using a file-system based approach, where each service instance is represented by a file
in a well-known directory structure. Changes in this directory structure are monitored using inotify.

### Requirements

- we are explicitly **not** requiring, that service-discovery works "seamlessly" across domains as this would require an
  entire filesystem structure being shared across domains!

### Remarks

For the cross-domain `LoLa` use case, we rather envision, that service-discovery offerings/stop-offerings are being
forwarded between domains by gateway instances. We expect, that the gateway instances [see message-passing](#message-passing)
have a basic mechanism to exchange information between their domains.
Thus, the detection of a new service instance offering on the provider side domain would be forwarded by the provider side
gateway instance to the consumer side gateway instance, which would then create a representative service instance file
in its own domain's service-discovery filesystem structure. The same applies for stop-offerings.

## Synchronization Files

`LoLa` uses two kinds of so-called marker-files per provided service-instance to manage life-cycle aspects:

- A service-existence marker-file is created by the provider instance under a given path/name
  representing the service-instance, when it starts offering the service-instance.

  **Since only the provider is accessing this file, it is not relevant for any cross-domain impact analysis!**

  It is deleted again, when it stops offering it. It gets exclusively `flocked` by the provider instance before service
  instance creation. This mechanism is used, to hinder a 2nd provider to provide the very same service-instance.
  Additionally, the OS cares for unlocking, when the provider instance crashes.
- A service-usage marker-file is created by the provider instance under a given path/name representing the
  service-instance, when it starts offering the service-instance. Each consumer instance using the service-instance
  `flock`s this file "shared", before accessing the service-instance. This mechanism is used to hinder the provider
  instance from unlinking DATA/CTRL shared-memory objects of the service-instance, while there are still consumers using
  it, which would impact their partial-restart capability.
  It is deleted again, when provider stops offering the service instance and no consumer holds a shared flock.

### Requirements

- we are explicitly **not** requiring, that synchronization files (in this case the service-usage marker-file) works
  "seamlessly" across domains as this would require an entire filesystem structure with OS specific `flock` support
  being shared across domains!

### Remarks

The gateway instance at the consumer side can create the expected service-usage marker-file in its own domain's
filesystem structure, when it is forwarding the service-instance offering from the provider side domain to the consumer
side domain. i.e. the gateway instance on the provider side will put a shared-flock on the service-usage marker-file in its own
domain's filesystem structure, while the gateway instance on the consumer side will create the service-usage marker-file
in its own domain's filesystem structure. Then a mechanism would be needed, that supervises the flock status on the
consumer side and informs the provider side gateway instance, when the shared-flock can be released.
