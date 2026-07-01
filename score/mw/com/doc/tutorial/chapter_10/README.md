# Chapter 10: Access control for service instances

Up to now in our tutorial we did not take care of access control to our service instances at all. Any consumer
application &ndash; running under whatever `uid`/`gid` &ndash; was able to consume the service instances our provider app
offered.

In real-world setups this is very unlikely to be acceptable. You do not want that an arbitrary application is able to
use a given service instance. Even leaving functional-safety concerns aside, you do not even want a service instance
provided in ASIL-QM quality to be consumed by arbitrary consumers. For a very simple reason: the provider has already
configured certain **resources** &ndash; think of `maxSubscribers` or `numberOfSampleSlots` used throughout the earlier
chapters. With this configuration the provider already made assumptions about *how many* consumers/subscribers will
consume a given event/field and *to which extent*. If some unknown/unexpected consumer showed up and subscribed to
certain service elements, it would occupy resources, which could lead to failures for the known/expected consumers
showing up later (their subscribe calls would be rejected because the resources are already exhausted). Thus, access
control is needed in real-world scenarios.

If you extend the discussion to functional-safety, the need for access control becomes even more obvious &ndash; to the
point that for ASIL-B applications, we **require** access control to be configured as part of our AoU (Assumptions of
Use).

### Access control in score::mw::com (and in the LoLa binding)

Access control is always something that has to be realized on the **binding layer**! Different technical bindings deploy
different access-control mechanisms. So the concrete question is: *what access-control mechanisms does our
LoLa/shared-memory binding provide?*

The LoLa binding uses two kinds of "communication channels" for which it has to provide access control:

- a **message-passing** layer, over which `score::mw::com` applications using the LoLa binding exchange messages, and
- **shared-memory objects**, to which both provider and consumer applications have access.

These basic building blocks can also be seen in the high-level architecture documentation, located
[here](https://eclipse-score.github.io/score/main/features/communication/ipc/docs/architecture/index.html).

> Note: Currently we are **not** doing any access control on the message-passing level. The reason is that this is
> sometimes not possible at all, depending on the application's configuration. Access control is therefore realized on
> the shared-memory objects.

Access control is configured at the level of a **service instance** in the `score::mw::com` configuration file. For a
specific service instance you can define two properties, each carrying a list of `uid`s separated by ASIL level:

- **`allowedConsumer`** defines which applications, in the role of a **consumer** (proxy), are allowed to access the
  service instance. This is defined at the **provider side** (provider-side configuration). In the LoLa binding the
  provider uses this list to restrict access to its shared-memory objects (via ACLs) to exactly the given `uid`s.
- **`allowedProvider`** defines which applications are expected/allowed, in the role of a **provider** (skeleton), to
  provide the service instance. This is defined at the **consumer side** (consumer-side configuration). With this
  setting a consumer can make sure that it only accepts a verified/specific provider to consume the service from: when
  opening/mapping the shared-memory objects, the consumer checks that they are owned by one of the configured
  `allowedProvider` `uid`s and terminates if that is not the case.

A few important rules:

- Only if a service instance is provided with `asil-level` = `B`, separate `uid` lists for `QM` and `B` may be given. If
  a service instance is tagged with `asil-level` = `QM`, only a `QM` list may be given in `allowedConsumer` /
  `allowedProvider`.
- If a property `allowedConsumer` or `allowedProvider` is **omitted**, this means **no restrictions** regarding `uid`s.
- If a property `allowedConsumer` or `allowedProvider` is an **empty list**, the meaning depends on the
  `permission-checks` property of the service instance (`file-permissions-on-empty` &ndash; the default &ndash; falls
  back to basic `ugo` file-system rights; `strict` means really no-one is allowed). See the
  [configuration documentation](../../../impl/configuration/README.md#serviceinstances) for the details.

### Files/artifacts used

We base our sample code on the code from [chapter 2](../chapter_2/README.md). The application code is **unchanged**; only
the two configuration files differ. The `bazel` project for this chapter is located in
`score/mw/com/doc/tutorial/chapter_10` and contains the following files:

----------------------------------------

| File Name                       | Description                                                                          |
|---------------------------------|--------------------------------------------------------------------------------------|
| `BUILD`                         | This file contains bazel targets for this example.                                   |
| consumer/`consumer.cpp`         | Implementation of the service consumer app. The `main()` for the consumer            |
| consumer/`consumer.h`           | Header (empty - we just always want to have cpp/h pairs) of the service consumer.    |
| consumer/`consumer_config.json` | This file contains the configuration for `score::mw::com` for the consumer app.      |
| consumer/`logging.json`         | This file contains the configuration for the logging system used by `score::mw::com` |
| provider/`provider.cpp`         | Implementation of the service provider.                                              |
| provider/`provider.h`           | Header (empty) of the service provider.                                              |
| provider/`logging.json`         | This file contains the configuration for the logging system used by `score::mw::com` |
| provider/`mw_com_config.json`   | This file contains the configuration for `score::mw::com` for the provider app.      |
| `hello_world_service.cpp`       | This file is empty as the service interface is completely defined in the header.     |
| `hello_world_service.h`         | This file contains the definition of the service interface.                          |

----------------------------------------

### Configuration

To show access control in action we adapt only the configuration of the provider and the consumer app.

On the **provider** side, we add an `allowedConsumer` property to the service instance `MyHelloWorldServiceInstance` and
grant `uid` **778** (as the single allowed consumer) in the `QM` list:

```json
{
  "instanceId": 1,
  "asil-level": "QM",
  "binding": "SHM",
  "allowedConsumer": {
    "QM": [
      778
    ]
  },
  "events": [
    {
      "eventName": "message",
      "numberOfSampleSlots": 10,
      "maxSubscribers": 3
    }
  ]
}
```

On the **consumer** side, we add an `allowedProvider` property to the service instance `MyHelloWorldServiceInstance` and
grant `uid` **777** (as the single accepted provider) in the `QM` list:

```json
{
  "instanceId": 1,
  "asil-level": "QM",
  "binding": "SHM",
  "allowedProvider": {
    "QM": [
      777
    ]
  }
}
```

Note that both service instances are tagged `asil-level` = `QM`, so only a `QM` list is allowed here. As explained
above, a `B` list would only be permitted (and required for freedom-from-interference) if the service instance were
provided in ASIL-B quality.

### Provider application

The code for the provider application is **unchanged** compared to chapter 2. Only its configuration differs (the added
`allowedConsumer` property).

### Consumer application

The code for the consumer application is **unchanged** compared to chapter 2. Only its configuration differs (the added
`allowedProvider` property).

### How to run the example

First build the two applications:

```bash
# Build the provider and consumer targets
bazel build //score/mw/com/doc/tutorial/chapter_10:provider-tar
bazel build //score/mw/com/doc/tutorial/chapter_10:consumer-tar
```

Extract both archives (e.g. in a tmp-directory):

```bash
mkdir -p /tmp/tutorial/chapter_10
tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_10/provider-tar.tar -C /tmp/tutorial/chapter_10/
tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_10/consumer-tar.tar -C /tmp/tutorial/chapter_10/
```

Unlike the previous chapters, we now need to start the provider and consumer under **specific user-ids**, matching the
`uid`s we configured (`777` for the provider, `778` for the consumer). The following steps require **root** privileges
(to create the users and to run a program as another user).

Create the two users with exactly the `uid`s used in the configuration:

```bash
# Create a user 'provider777' with uid 777 and a user 'consumer778' with uid 778.
sudo useradd --uid 777 --no-create-home --shell /usr/sbin/nologin provider777
sudo useradd --uid 778 --no-create-home --shell /usr/sbin/nologin consumer778
```

Make sure both users can read/execute the extracted applications (they live under `/tmp`, which is world-accessible by
default; adjust if you extracted elsewhere).

Now start the service-provider application as `uid` **777** in the 1st terminal:

```bash
cd /tmp/tutorial/chapter_10/opt/HelloWorldServer
sudo -u provider777 bin/provider
```

... and the service-consumer as `uid` **778** in the 2nd terminal:

```bash
cd /tmp/tutorial/chapter_10/opt/HelloWorldClient
sudo -u consumer778 bin/consumer --service_instance_manifest ./etc/consumer_config.json
```

With this **matching** setup everything works exactly as in chapter 2: the provider grants `uid` 778 access to its
shared-memory objects (via `allowedConsumer`), and the consumer accepts the provider because it runs under `uid` 777
(which is in the consumer's `allowedProvider` list). The consumer creates the proxy and receives the "message" events.

Now try the **mismatching** cases and observe how access control kicks in:

- Start the **consumer under a different `uid`** than 778 (e.g. under your own user, or under `provider777`). The
  provider's `allowedConsumer` ACL does not grant this `uid` access to the shared-memory objects, so the consumer is
  **not** able to open/map them and therefore **cannot create a proxy** to interact with the service instance.
- Start the **provider under a different `uid`** than 777 (e.g. under your own user, or under `consumer778`). Now the
  shared-memory objects are owned by an unexpected `uid`. The consumer &ndash; because it configured `allowedProvider` =
  `[777]` &ndash; detects that the objects are **not** owned by one of its accepted providers and refuses to use them,
  so again it **cannot create a proxy** to interact with the service instance.

In both mismatching cases the consumer fails to establish communication with the service instance, which is exactly the
access control we configured.

> Note: Running the two applications under the **same** `uid` (as we did in all earlier chapters) would only work if
> that `uid` happened to satisfy **both** lists (i.e. be `777` for the provider role *and* be `778` for the consumer
> role), which is impossible here since the lists are disjoint. This nicely illustrates that access control is enforced
> independently on both sides.

### Summary

A short summary of what we have learned in this chapter:

- In real-world (and especially functional-safety) setups you must **not** let arbitrary applications access your
  service instances. Access control is needed to protect the provider's pre-allocated resources (e.g. `maxSubscribers`,
  `numberOfSampleSlots`) and, for ASIL-B, to preserve freedom from interference (it is part of our AoU).
- Access control is always realized on the **binding layer**. For the LoLa binding it is enforced on the
  **shared-memory objects** (the message-passing layer currently has no access control).
- Access control is configured per **service instance** via two properties:
  - `allowedConsumer` (provider-side): the `uid`s allowed to *consume* the instance; enforced via ACLs on the provider's
    shared-memory objects.
  - `allowedProvider` (consumer-side): the `uid`s the consumer accepts as *provider*; the consumer checks the ownership
    of the shared-memory objects and terminates on a mismatch.
- The `uid` lists are given per ASIL level (`QM`, and `B` only for ASIL-B service instances). An **omitted** property
  means *no restriction*; an **empty** list is interpreted according to the `permission-checks` property.
- If the running `uid`s do not match the configured lists on either side, the consumer cannot create a proxy and
  communication is denied.
