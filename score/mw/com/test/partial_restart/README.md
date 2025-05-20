<!--- 
*******************************************************************************>
Copyright (c) 2024 Contributors to the Eclipse Foundation
See the NOTICE file(s) distributed with this work for additional
information regarding copyright ownership.
This program and the accompanying materials are made available under the
terms of the Apache License Version 2.0 which is available at
https://www.apache.org/licenses/LICENSE-2.0
SPDX-License-Identifier: Apache-2.0 #
*******************************************************************************
 ---> 



## Partial Restart ITF strategy

The ITFs for Partial Restart shall verify, that a communication relation between a provided service instance (skeleton)
and a consumer instance (proxy), which are located in different processes, can be successfully re-established after one
side (provider or consumer) has been restarted after the communication relation has been set up once.
Thereby it shall not matter, whether the corresponding process did a crash-restart or a normal-restart!

### ITF variants

To cover the complete functionality implemented for `Partial Restart`, we will use the following ITF variations:

1. Provider/skeleton normal restart while having a connected/subscribed consumer/proxy
2. Provider/skeleton normal restart **without** a connected/subscribed consumer/proxy
3. Provider/skeleton crash-restart while having a connected/subscribed consumer/proxy
4. Provider/skeleton crash-restart **without** a connected/subscribed consumer/proxy
5. Consumer/proxy normal restart while being connected/subscribed to a provider/skeleton
6. Consumer/proxy crash-restart while being connected/subscribed to a provider/skeleton


Note, that during implementation and bug-hunting in field, we realized, that the pure re-start capability of processes
with proxy/skeleton instances is so deeply interwoven with the so called `proxy-auto-reconnect` feature, that it simply
doesn't make any sense to just test the stand-alone re-start capability of a lola process containing proxies and/or
skeletons. This is the reason, that all the ITF variants listed above expect an existing/established communication
relation, to also verify the `proxy-auto-reconnect` feature working properly!

### Connection resp. subscription setup

In all the ITFs listed above the connection/subscribe semantics between the proxy and skeleton not only contains a
subscription done by the poxy to a specific event of the skeleton, but also a registered `EventReceiveHandler` for this
event. As the existence of such an `EventReceiveHandler` needs additional functionality/code-paths in the restart:

- in case of restart of a provider not only the existing event-subscription of the proxy has to be "re-newed", but also
  the registration of event-update-info (`EventReceiveHandler`) has to be "re-newed". Both things work totally different
  in `LoLa`! While the subscription is persisted within the shared-memory across the restart of the provider (as the
  shared-memory objects remain intact), the event-update-notification wishes a proxy (event) has, are stored
  non-persistently at the provider side and must be explicitly re-newed by the consumer side, if it detects a provider
  restart.
- in case of restart of a consumer, it must be made sure, that the event-update-info at the provider side gets
  "re-newed" as the target address of the consumer to which the provider shall send update notifications has changed.

### Common Structure

All ITFs do have the following structure:

- A main process (started by the ITF framework), which serves the job as our `Controller` process
- a `Consumer` process (forked by the `Controller`), which contains a Proxy.
- a `Provider` process (forked by the `Controller`), which contains a Skeleton.

#### Checkpointing

When the `Consumer`/`Provider` have been started, they follow a certain sequence until they reach a `checkpoint` (a
certain expected state). If they have reached a `checkpoint`, they notify this to te `Controller`. Also, if they
encountered an error, which prevented them to reach the `checkpoint`, they notify the error to the `Controller`.
If they reached a `checkpoint` they wait for the `controller` to either trigger them to proceed to the next `checkpoint`
or to terminate (which they will do with a zero return-code).
If they detected an error and notified this to the `Controller`, they terminate with a non-zero error code.

### ITF 1 - Provider normal restart - connected Proxy

#### <a name="cpa1"></a> Consumer process activity

After start/being forked, it does the following sequence:

1. It triggers an async `StartFindService` search for the service-instance provided by the provider process (and blocks
   the test-sequence until it found one hit)
2. Creates a proxy instance for the found service.
3. Subscribes for a specific event with maxSamples = N.
4. Registers an EventReceiveHandler for the subscribed event
5. Expects to get N (config parameter) event update notifications and in each notification it accesses the new event sample. Store the SamplePtr in a vector. Store the data pointed by SamplePtrs. If vector size reached N than check point is reached.
Depending on the timing, the following can happen: 
    * the provider sends two new samples one after the other and sends two update notifications to the consumer/proxy
    * when the consumer/proxy wakes up and calls GetNewSamples() he already finds the TWO new samples in the 1st call and then if he gets the 2nd notification, he will get NO new samples! This timing specific behavior has to be handled/adressed correctly!
6. `Checkpoint` (1) reached - notify to Controller.
7. Wait for `Controller` trigger to get to next checkpoint/or finish. If proceed trigger received, resume sequence.
8. Supervise subscription state of the subscribed event. After `checkpoint` (1) was reached, it is expected to be
   `subscribed`. As soon as state switches to `subscription-pending`, the `checkpoint` (2) has been reached - notify to
   `Controller`.
9. Wait for `Controller` trigger to get to next checkpoint/or finish. If proceed trigger received, resume sequence.
10. Supervise subscription state of the subscribed event. After `checkpoint` (2) was reached, it is expected to be
   `subscription-pending`. As soon as state switches to `subscribed`, the `checkpoint` (3) has been reached - notify to
   `Controller`.
11. Wait for `Controller` trigger to get to next checkpoint/or finish. If proceed trigger received, resume sequence.
12. Check stored the data pointed by SamplePtrs for integrity. 
13. Clear all stored SamplePtrs.
14. Repeat step (5).
15. `Checkpoint` (4) reached - notify to `Controller`.
16. Wait for `Controller` trigger to finish.

#### <a name="ppa1"></a> Provider process activity

After start/being forked:

1. It creates its service instance (skeleton).
2. It offers it.`Checkpoint` (1) is reached when service is offered - notify to `Controller`.
3. It starts cyclically sending a specific event.
4. Wait for `Controller` trigger to get to next `checkpoint`. If trigger received, resume sequence.
5. Stop sending events. Calls StopOffer on the service instance (skeleton)
6. `Checkpoint` (2) reached - notify to `Controller`.
7. Wait for `Controller` trigger to terminate.

#### Controller process activity

1. Fork `consumer` process
2. Fork `provider` process
3. Wait `maxWaitTime` for notification of `provider`, that it reached `checkpoint` (1).
4. Wait `maxWaitTime` for notification of `consumer`, that it reached `checkpoint` (1).
5. Trigger `consumer` for proceeding to next `checkpoint`. (leads to `consumer` waiting for switch to
   `subscription-pending`)
6. Trigger `provider` for proceeding to next `checkpoint` (leads to StopOffer).
7. Wait `maxWaitTime` for notification of `provider`, that it reached `checkpoint` (2) (StopOffer being called).
8. Wait `maxWaitTime` for notification of `consumer`, that it reached `checkpoint` (2) (state switched to
   `subscription-pending`).
9. Trigger `provider` to terminate.
10. Wait on `provider` termination via `waitpid()`.
11. Trigger `consumer` for proceeding to next checkpoint. (leads to consumer waiting for switch to `subscribed`).
12. (Re)fork the `provider` process.
13. Wait `maxWaitTime` for notification of `provider`, that it reached `checkpoint` (1).
14. Wait `maxWaitTime` for notification of `consumer`, that it reached `checkpoint` (3) (state switched to `subscribed`).
15. Trigger `consumer` for proceeding to next `checkpoint`. (leads to `consumer` receiving N event samples).
16. Wait `maxWaitTime` for notification of `consumer`, that it reached `checkpoint` (4) (N samples received).
17. Trigger `consumer` to terminate.
18. Wait on `consumer` termination via `waitpid()`.
19. Trigger `provider` to terminate.
20. Wait on `provider` termination via `waitpid()`.

### ITF 2 - Provider normal restart - **without** connected Proxy

#### <a name="cpa2"></a> Consumer process activity

Consumer process activity in this case is drastically stripped down compared to [previous ITF](#cpa1).
It isn't creating any proxy at all. It is just using `StartFindService`/service discovery to detect provider side
offerings.

After start/being forked, it does the following sequence:

1. It triggers an async `StartFindService` search for the service-instance provided by the provider process (and blocks
   the test-sequence until it found one hit)
2. `Checkpoint` (1) reached - notify to Controller.
3. Wait for `Controller` trigger to get to next checkpoint/or finish. If proceed trigger received, resume sequence.
4. It waits until the service instance has disappeared again.
5. `Checkpoint` (2) reached - notify to Controller.
6. Wait for `Controller` trigger to get to next checkpoint/or finish. If proceed trigger received, resume sequence.
7. It waits until the service instance has been found.
8. `Checkpoint` (3) reached - notify to Controller.
9. Wait for `Controller` trigger to finish.

#### <a name="ppa2"></a> Provider process activity

Provider process activity in this case is identical to [previous ITF 1](#ppa1).

#### Controller process activity

1. Fork `consumer` process
2. Fork `provider` process
3. Wait `maxWaitTime` for notification of `provider`, that it reached `checkpoint` (1).
4. Wait `maxWaitTime` for notification of `consumer`, that it reached `checkpoint` (1).
5. Trigger `consumer` for proceeding to next `checkpoint`. (leads to `consumer` waiting for service instance to
   disappear.)
6. Trigger `provider` for proceeding to next `checkpoint` (leads to StopOffer).
7. Wait `maxWaitTime` for notification of `provider`, that it reached `checkpoint` (2) (StopOffer being called).
8. Wait `maxWaitTime` for notification of `consumer`, that it reached `checkpoint` (2) (service instance has
   disappeared).
9. Trigger `provider` to terminate.
10. Wait on `provider` termination via `waitpid()`.
11. Trigger `consumer` for proceeding to next checkpoint. (leads to `consumer` waiting for service instance to
    appear.).
12. (Re)fork the `provider` process.
13. Wait `maxWaitTime` for notification of `provider`, that it reached `checkpoint` (1).
14. Wait `maxWaitTime` for notification of `consumer`, that it reached `checkpoint` (3) (service appeared again).
15. Trigger `consumer` to terminate.
16. Wait on `consumer` termination via `waitpid()`.
17. Trigger `provider` to terminate.
18. Wait on `provider` termination via `waitpid()`.

### ITF 3 - Provider crash restart - connected Proxy

This ITF is almost identical to [ITF 1](#itf-1---provider-normal-restart---connected-proxy).

#### Consumer process activity

Consumer process activity in this case is exactly identical to the activity in the [previous ITF 1](#cpa1).

#### <a name="ppa3"></a> Provider process activity

Provider process activity in this case is almost identical to the activity in the [previous ITF 1](#ppa1).
But only the 1st 4 steps are of interest, after step 4 the provider will get killed anyhow. So the same implementation
of provider as in [ITF 1](#itf-1---provider-normal-restart) can be used.

After start/being forked:

1. It creates its service instance (skeleton).
2. It offers it.
3. It starts cyclically sending a specific event.
4. `Checkpoint` (1) reached as soon as N event samples have been successfully sent - notify to `Controller`.

#### Controller process activity

1. Fork `consumer` process
2. Fork `provider` process
3. Wait `maxWaitTime` for notification of `provider`, that it reached `checkpoint` (1).
4. Wait `maxWaitTime` for notification of `consumer`, that it reached `checkpoint` (1).
5. Trigger `consumer` for proceeding to next `checkpoint`. (leads to `consumer` waiting for event subscribe-state to
   switch to `subscription-pending`)
6. Kill `provider` process.
7. Wait on `provider` termination via `waitpid()`.
8. (Re)fork the `provider` process.
9. Wait `maxWaitTime` for notification of `provider`, that it reached `checkpoint` (1).
10. Wait `maxWaitTime` for notification of `consumer`, that it reached `checkpoint` (2) (state switched to
    `subscription-pending`).
11. Trigger `consumer` for proceeding to next checkpoint. (leads to consumer waiting for switch to `subscribed`).
12. Wait `maxWaitTime` for notification of `consumer`, that it reached `checkpoint` (3) (state switched to `subscribed`).
13. Trigger `consumer` for proceeding to next `checkpoint`. (leads to `consumer` receiving N event samples).
14. Wait `maxWaitTime` for notification of `consumer`, that it reached `checkpoint` (4) (N samples received).
15. Trigger `consumer` to terminate.
16. Wait on `consumer` termination via `waitpid()`.
17. Trigger `provider` to terminate.
18. Wait on `provider` termination via `waitpid()`.


### ITF 4 - Provider crash restart - **without** connected Proxy

This ITF is almost identical to [ITF 2](#itf-2---provider-normal-restart---without-connected-proxy).

#### Consumer process activity

Consumer process activity in this case is exactly identical to the activity in the [previous ITF 2](#cpa2).

#### Provider process activity

Provider process activity in this case is identical to the activity in the [previous ITF 3](#ppa3).

#### Controller process activity

1. Fork `consumer` process
2. Fork `provider` process
3. Wait `maxWaitTime` for notification of `provider`, that it reached `checkpoint` (1).
4. Wait `maxWaitTime` for notification of `consumer`, that it reached `checkpoint` (1).
5. Trigger `consumer` for proceeding to next `checkpoint`. (leads to `consumer` waiting for service instance to
   disappear.)
6. Kill `provider` process.
7. Wait on `provider` termination via `waitpid()`.
8. (Re)fork the `provider` process.
9. Wait `maxWaitTime` for notification of `provider`, that it reached `checkpoint` (1).
10. Wait `maxWaitTime` for notification of `consumer`, that it reached `checkpoint` (2) (service instance has
   disappeared).
11. Trigger `consumer` for proceeding to next checkpoint. (leads to `consumer` waiting for service instance to
    appear again).
12. Wait `maxWaitTime` for notification of `consumer`, that it reached `checkpoint` (3) (service appeared again).
13. Trigger `consumer` to terminate.
14. Wait on `consumer` termination via `waitpid()`.
15. Trigger `provider` to terminate.
16. Wait on `provider` termination via `waitpid()`.

### ITF 5 - Provider crash restart - **without** connected Proxy
Description will be added in .

### ITF 6 - Provider crash restart - **without** connected Proxy
Description will be added in .

### ITF 7 - partial_restart/check_number_of_allocations
Requirement 31295746: After restarting a Skeleton, it shall be able to allocate the configured amount of SampleAllocateePtr as before the restart. 

Below are the steps for testing this requirement.
1. Create a Controller.
2. Controller creates a provider.
3. Provider allocates the maximum number of samples allowed by the configuration.
4. Controller kills provider.
5. Controller respawn provider.
6. Test is successful if respawn provider is able to allocate the maximum number of samples allowed by the configuration.

### ITF 8 - partial_restart/proxy_restart_shall_not_affect_other_proxies (Normal restart)
Requirement 30571867: "A restarting Proxy, must not acquire additional resources on the provider side in a way, that other consumer applications might be negatively affected" 

Below are the steps for testing this requirement.
1. Create a Controller.
2. Controller creates a provider.
3. Provider offers Service and Send samples.
4. Controller creates first consumer.
5. First consumer fetches configured number of samples.
6. Controller creates second consumer.
7. Second consumer fetches configured number of samples.
8. Controller wait for first consumer to finish and restarts it N times. 
9. Test is successful if second consumer is able to create proxy, subscribe and fetch configured number of samples.

### ITF 9 - partial_restart/proxy_restart_shall_not_affect_other_proxies (Crash restart)
Requirement 30571867: "A restarting Proxy, must not acquire additional resources on the provider side in a way, that other consumer applications might be negatively affected" 

This test  follows steps defined in ITF8. Only step 8 is different 
Controller kills and spawn first consumer N times. 
