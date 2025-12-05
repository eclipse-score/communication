## mw::com Configuration Concept

Following document contains a short documentation, how to configure a `mw::com` application.

### Central JSON File per App

The configuration is a json file, which has to adhere the following [schema](./ara_com_config_schema.json).
Typically, the file is expected to be named `mw_com_config.json` and being located in the applications `/etc` directory.
If these rules are followed, the configuration is automatically detected and loaded by the `mw::com::Runtime`.

If you want to deviate from default name/default path, you can do so, but then you have to explicitly load your configuration
by a call to `mw::com::runtime::InitializeRuntime(argc, argv)`, where `argv` needs to contain
`-service_instance_manifest /path/to/mw_com_config.json`.

### Useful example

An example configuration is located [here](./example/ara_com_config.json). In several parts of this documentation, we
reference to snippets of this example configuration, to illustrate things.

### What is configured

Generally the configuration contains the so-called `service-types`, the application has to deal with, either in the form,
that the application provides or consumes such a `service-type`. Additionally, it contains the specific `instances` of
these `service-types`. Because a `service-type` is just a type, but not a real instantiated thing, which can be provided
or consumed.

The sections, where `service-types` or `service-instances` are configured make up the main parts of a `mw::com`
configuration. Thus, they will occupy most of the space.
Then there are two smaller sections `global` and `tracing`, which configure settings, which are impacting **all** the
configured service instances!

### Structure

The structure in the json file reflects the dependencies, which the configuration sections might have. I.e. a configured
`service-instance` is of a certain `service-type`. Therefore, it references a `service-type`, so naturally the section
for the `service-types` comes before the section of `service-instances`.

#### Service Types

The section for the `service-types` is represented by the property `serviceTypes` in our json configuration object,
which is of a json array type.

The array characteristics you can see from this snippet from the example config - the opening `[` after `"serviceTypes"`:

    {
        "serviceTypes": [
          {
            "serviceTypeName": "/bmw/ncar/services/TirePressureService",
            ...
          },
          ...
        ],
        ...
    }

A `service-type` in `mw::com` is represented in code by a C++ class, which defines the so called `service-interface`.
So, the notions `service-type` and `service-interface` basically express the same abstraction level and are often used
interchangeable. On code level the notion `service-interface` has an even broader use as with `mw::com`, we encourage
the users to write down the "service-interface" definition in the form of a C++ struct, instead of generation from an
IDL. This C++ struct contains event and field members, which make up the `service-interface` signature.

The requirements, how to write a C++ `service-interface` definition, are documented in [traits.h](../traits.h), if you
look for the `doxygen` documentation of the class template `TheInterface`.

The linkage between the C++ representation of the `service-interface` in code and the related `service-type` in the json
configuration, which contains relevant configuration items for the C++ representation, is essentially based on unique
keys commonly used within C++ code and the json `service-type`. Since this mapping is done indirectly via the
`service-instances`, instantiating a `service-type`, this is shown later in chapter [Service InstancesService Instances](#service-instances).

##### Service Type Name

The mandatory "key" property of a `service-type` object within the `service-types` array is `serviceTypeName`.
It contains a string representation, which has to be unique. I.e. within a `mw_com_config.json` config, there must
**not** be two `service-type`s with the same `serviceTypeName`.
The string representation must be a valid C++ identifier, that starts with "/" and additional C++ identifiers can be
appended, each separated with a "/".

Later the `service-instances`, being typed by a specific `service-type`, will reference the type via this
`serviceTypeName`.

##### Versioning

The mandatory property `version` (containing a major/minor version part) is currently not being used. It has to formally
exist, but has no functional effect yet!

##### Bindings

Our `mw::com` architecture foresees, that a `service-type` might be provided via several technical transport mechanisms,
which we call a `binding`. The decision, which technical transport mechanisms to be used is finally taken by the
`service-instances`. I.e. a `service-instance` might be configured to support an ECU local
transport mechanism, like our shared-memory based mechanism, and also a network based mechanism (like `SOME/IP`) to
support cross-ECU communication.

However, the `service-type` on which the `service-instance` is based on, needs to support the technical transport
mechanism (`binding`) and needs to configure the instance `independent` parts of the binding.

Currently, the only supported binding is the shared-memory binding, which is represented as `SHM` in the json.
The other binding `SOME/IP`, which the schema allows, is only a placeholder right now.
In the corresponding snippet from our example configuration:

    "bindings": [
      {
        "binding": "SHM",
        "serviceId": 1234,
        "events": [
          {
            "eventName": "CurrentPressureFrontLeft",
            "eventId": 20
          }
        ],
       "fields": [
         {
           "fieldName": "CurrentTemperatureFrontLeft",
           "fieldId": 30
         }
       ],
       "methods": [
         {
           "methodName": "SetPressure",
           "methodId": 40
         }
       ]
      }
    ]

you see, that for the `service-type` being identified (via property `serviceTypeName`) as
"/bmw/ncar/services/TirePressureService", we have configured one binding of type `SHM`.

The first essential and mandatory property to be defined for a `binding` is the `serviceId`: It is a `binding` type
specific identification for the `service-type`. The `service-type` has a `binding` independent unique
identification via `serviceTypeName`. This is typically a lengthy string representation, but on the technical transport
layer a `binding`-specific more efficient representation is needed. In case of `SHM` `binding` this is an
unsigned integer (16 bit), which (like the `serviceTypeName`) needs to be unique in the communication setup. I.e. **all**
applications communicating via `SHM` `binding` need to agree on a unique assignment of `serviceId`s to `service-types`.
To keep track within BMW, we currently manage the `serviceId` namespace [here](broken_link_cf/pages/viewpage.action?spaceKey=psp&title=LoLa+service+IDs+used+within+BMW)

Then, for each service element (event, field, method), which the `service-type` (the `service-interface` in our C++
representation) contains, we need to configure a pair of name and id. So in our example the `service-type`
"/bmw/ncar/services/TirePressureService" has an event named "CurrentPressureFrontLeft", a field named
"CurrentTemperatureFrontLeft", and a method named "SetPressure".

Now it is time to have a look at the C++ representation of the `service-type`, which fits to our example:

```c++
    template <typename Trait>
    class TirePressureService : public Trait::Base
    {
        public:
        using Trait::Base::Base;

        typename Trait::template Event<TirePressureDataType> current_pressure_front_left_{*this, "CurrentPressureFrontLeft"};
        typename Trait::template Field<TemperatureDataType> current_temperature_front_left_{*this, "CurrentTemperatureFrontLeft"};
        typename Trait::template Method<SetPressureReturn(SetPressureInArgType)> set_pressure_{*this, "SetPressure"};
    };
```

As you see in the C++ code above, the event, field, and method members have assigned names in their `ctor`. In this case:

- "CurrentPressureFrontLeft"
- "CurrentTemperatureFrontLeft"
- "SetPressure"

these names have to be reflected in the event, field, and method `binding` elements! I.e. for each event, field, or method member in the
C++ representation of the `service-type`, a corresponding event, field, or method object in the `events`, `fields`, or `methods` arrays of
the `binding` object in the json configuration is required.

While the `eventName`, `fieldName`, and `methodName` in the event, field, and method object is prescribed by the C++ representation, the
`eventId`, `fieldId`, and `methodId` property can be chosen freely in the `binding` configuration. Again &ndash; like in the case of
`serviceId` &ndash; `eventId`, `fieldId`, and `methodId` are `binding` specific optimized identifications used instead of the event,
field, and method name. In case of a `SHM` `binding` these IDs have to be unsigned integers (16 bit) and an ID has to be unique
among all service elements (events, fields, and methods) within the service.

#### Service Instances

##### Overview

Previous chapter defined the `service-type`s, on which the `service-instances` we are now looking at, are based on.

The section for the `service-instances` is represented by the property `serviceInstances` in our json configuration
object, which is of a json array type.

The array characteristics you can see from this snippet from the example config - the opening `[` after `"serviceInstances"`:

```json
    "serviceInstances": [
        {
            "instanceSpecifier": "abc/abc/TirePressurePort",
            "serviceTypeName": "/bmw/ncar/services/TirePressureService",
            "version": {
                "major": 12,
                "minor": 34
            },
            "instances": [
                {
                    "instanceId": 1234,
                    "asil-level": "B",
                    "binding": "SHM",
                    "shm-size": 10000,
                    "control-asil-b-shm-size": 20000,
                    "control-qm-shm-size": 30000,
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft",
                            "numberOfSampleSlots": 50,
                            "maxSubscribers": 5,
                            "numberOfIpcTracingSlots": 0
                        }
                    ],
                    "fields": [
                        {
                            "fieldName": "CurrentTemperatureFrontLeft",
                            "numberOfSampleSlots": 60,
                            "maxSubscribers": 6,
                            "numberOfIpcTracingSlots": 7
                        }
                    ],
                    "allowedConsumer": {
                        "QM": [
                            42,
                            43
                        ],
                        "B": [
                            54,
                            55
                        ]
                    },
                    "allowedProvider": {
                        "QM": [
                            15
                        ],
                        "B": [
                            15
                        ]
                    }
                }
            ]
        }
    ],
```

##### instanceSpecifier

A `service-instance` object within the `serviceInstances` array of the json configuration has a unique, identifying
property `instanceSpecifier`, which is mandatory. This string is used from within the C++ code, to identify a
specific `service-instance`.
I.e., when creating a providing `service-instance` (skeleton wrapper class), this `instanceSpecifier` string has to be
given to the `Skeleton::Create()` API. In case of setting up a consuming service instance, this `instanceSpecifier`
string has to given to the `FindService`/`StartFindService` API to find the required service instance and afterward
create the proxy object(s) from the returned handle(s) (`HandleType`).

###### Role dependent view

So here we already see, that there are two different views on a `service-instance`, depending on, whether an
application wants to instantiate the provider or consumer side of a `service-instance`!
Depending on the role only a subset of configuration properties for the `service-instance` are used. Generally,
an application, which is creating the provider side of the `service-instance`, needs more configuration
properties within a service-instance json configuration object, than an application, which is "just" creating the
consumer side of the `service-instance`. [This](#overview-service-instance-property-existence-per-role) chapter provides
an overview about optional and mandatory properties depending on the roles.

Note, that this makes it currently impossible to check, whether a `mw_com_config.json` configuration is "valid" in the
context of a given `mw::com` application with only one schema.
The reason is, that some properties are defined as "optional" in the json schema, because they are optional or unused
for one of the role of the application.
E.g. an element might be unused for the consumer case but required by the provider. Thus, the true necessity of an
element can only be determined in context of the role of the `mw::com` application using the `mw_com_config.json`.

###### serviceTypeName

The second always mandatory property a `service-instance` json object has, is `serviceTypeName`. This property realizes
the link to the service-type and therefore needs to have a match within the `service-types` items (a `service-type` with
a matching `serviceTypeName`).

###### version

The `version` property is mandatory (although not being used for now) and should simply copy the `version` of the
referenced `service-type`.

###### instances

A given `service-instance`, uniquely identified by its `instanceSpecifier`, can have `1..N` "real" instances. I.e.
instances with a concrete binding specific identification and addressing.
This again has to do with the role specific view onto a `service-instance`:
In case of a provider side view (`instanceSpecifier` given to a `Skeleton::Create()` call), there must only be `one`
instance! In case of a consumer side view (`instanceSpecifier` given to a `Proxy::FindService()` call), there can be
`N > 1` "real" instances. With this approach you can express, that each of these configured "real" instances is a good
fit for the consumer side, when it does a `FindService` for the given `instanceSpecifier`, which aggregates these `N > 1`
"real" instances.

The items (instance objects) within the `instances` json array do have some `binding` independent and also `binding`
dependent properties.
`Binding` independent properties:

- `binding`: (mandatory) `binding` type used by the instance. Currently only `SHM` is supported.
- `instanceId`: (mandatory) The value or value-range depends on the chosen `binding`. In case of `SHM` binding it
  is an unsigned int (16 bit)
- `asil-level` : (mandatory) The ASIL level, which either the provided `service-instance` has, or the consumed
  `service-instance` expects. Setting this property to `B`, does imply the following constraint: The
  [global property `asil-level`](#asil-level) needs to be set to `B` as well. This is independent, whether this instance is seen
  from provider or consumer side view! From provider side perspective a `service-instance` can only be provided in `ASIL B`
  quality, when the whole `mw::com` application is tagged being `ASIL B`. From consumer side perspective you can only
  consume or access service-instances provided in `ASIL-B`, when the consuming `mw::com` application is tagged being
  `ASIL B`, due to freedom from interference (`FFI`) requirements! This is generally no problem, because a
  `service-instance` being provided in `ASIL B` quality is **automatically** provided in `ASIL QM` quality, too.
  I.e. a `mw::com` process with global `ASIL QM` quality can always consume a `service-instance` provided as `ASIL B` in
  `ASIL QM` quality.
- `permission-checks`: This property controls, how the following two properties `allowedConsumer` and `allowedProvider`
  behave. Both lists are lists of `uid`s. If a list is empty and the property is set to `file-permissions-on-empty`
  (default value), then `mw::com` falls back to simple user-group-other file access rights. This means, that the
  underlying shm-objects for CONTROL will be read-write for all (others) and DATA will be read for all.
  In case of `strict`, an empty `uid` list really means NO access, which typically makes no sense and is rather a
  misconfiguration.
- `allowedConsumer`: list of `uid`s per ASIL-level (`QM` or `B`), which is allowed to access or consume this
  `service-instance`. An empty list combined with `permission-checks` = `file-permissions-on-empty`, means no restriction.
  Everybody can consume this `service-instance`. **Caution**: In case of a service-instance provided with **ASIL-B**
  quality, it is mandatory to configure only applications in the `B` list (identified via a specific `uid`), which are
  also `ASIL-B` qualified! Otherwise, the whole `FFI` concept will break down.
- `allowedProvider`: list of `uid`s per ASIL-level (`QM` or `B`), which is allowed to provide this `service-instance`.
  An empty list combined with `permission-checks` = `file-permissions-on-empty`, means no restriction. Everybody can
  provide this `service-instance`. **Caution**: This is a safety-net for `ASIL-B` consumers, which must be sure, that the
  `service-instance`, they consume is for sure provided by a specific `uid`, which is known to be `ASIL-B` qualified!
  I.e. an ASIL-B consumer is required to configure only applications in the `B` list (identified via a specific
  `uid`), which are also `ASIL-B` qualified as allowed providers! Otherwise, the whole `FFI` concept will break down.

`Binding` dependent properties:

- `shm-size`: This is a `SHM` `binding` specific optional setting, how big (in bytes) the shared-memory object for DATA
  shall be sized. Normally this is not used as `mw::com` calculates the sizes for DATA and also for CONTROL
  shared-memory objects with some heuristics, configured in the  [global section](#shm-size-calc-mode).
  Using this property is not encouraged. It is rather a fallback, in case the preferred size calculation fails or in
  case the temporary heap-memory allocation done by the size calculation, needs to be avoided.
- `control-asil-b-shm-size`: This is a `SHM` `binding` specific optional setting, how big (in bytes) the shared-memory
  object for ASIL-B CONTROL shall be sized.
  Normally this is not used as `mw::com` calculates the sizes for DATA and also for CONTROL shared-memory objects with
  some heuristics, configured in the  [global section](#shm-size-calc-mode).
  Using this property is not encouraged. It is rather a fallback, in case the preferred size calculation fails or in
  case the temporary heap-memory allocation done by the size calculation, needs to be avoided.
- `control-qm-shm-size`: This is a `SHM` `binding` specific optional setting, how big (in bytes) the shared-memory
  object for QM CONTROL shall be sized.
  Normally this is not used as `mw::com` calculates the sizes for DATA and also for CONTROL shared-memory objects with
  some heuristics, configured in the  [global section](#shm-size-calc-mode).
  Using this property is not encouraged. It is rather a fallback, in case the preferred size calculation fails or in
  case the temporary heap-memory allocation done by the size calculation, needs to be avoided.

###### events and fields within an instance

Within the `service-instance` json object, there are further binding independent properties, which are json arrays:

- `events`
- `fields`

the `event` and `field` json objects, which are the items in the corresponding arrays are property-wise identical.
Additionally, there is the following constraint:
For each event or field enlisted in the `events` and `fields` arrays on the [service-type->bindings](#bindings) level of
the `service-type` a `service-instance` is based on, a corresponding instance specific event or field object has to exist.
And it has to match via `eventName` respectively `fieldName` to the corresponding event or field configuration on the
`service-type` level.

The properties of a field or an event object on the instance level are:

- `eventName`: (mandatory) - see above - needed to link to the event or field on `service-type` level.
- `numberOfSampleSlots`: How many slots shall be allocated to publish event or field samples in shared memory. This
  property is mandatory from provider side perspective, because the provider has to allocate the configured number of
  slots. The heuristics to come up with a sane number is typically as follows:
  The provider has to sum up the `maxSamples` numbers of all consumers subscribing to this event or field and then add `1`.
  This addition of `1` makes sure, that the provider can always emit a new sample, even if all the subscribing consumers
  are currently holding their `maxSamples` number announced in their subscribe call.
  Example: If there are three consumers for a given event E, where:
  - consumer 1 subscribes with `maxSamples` = 2 (indicating, that he wants to hold up to 2 samples in parallel)
  - consumer 2 subscribes with `maxSamples` = 2 (indicating, that he wants to hold up to 2 samples in parallel)
  - consumer 3 subscribes with `maxSamples` = 1 (indicating, that he wants to hold only 1 sample at a time)

  then `numberOfSampleSlots` should be configured as `6`: `(2 + 2+ 1) + 1`
  more generally this rule can be expressed with the following formula:

  $$N_{\mathrm{ss}} = 1 + \sum_{i=1}^{N_{\mathrm{cl}}} N_{i}^{\mathrm{MS}}$$

  with $N_{\mathrm{ss}}$ denoting `numberOfSampleSlots`, $N_{\mathrm{cl}}$ denoting number of clients and
  $N_{i}^{\mathrm{MS}}$ denoting the `maxSamples` configuration of `i`-th client.
  **Note**: During subscription the `maxSamples` value announced by a consumer is checked against the configured
  `numberOfSampleSlots`. I.e. The sum of all `maxSamples` values of all subscribing consumers must not exceed
  `numberOfSampleSlots`. Otherwise, the subscribe call will be rejected. However, this check is not ASIL level
  overarching. See explanation in the `maxSubscribers` section below.
- `maxSubscribers`: (mandatory on provider side) - how many consumers are allowed to subscribe to this event or field.
  This number is the combined number of `QM` and `ASIL-B` consumers.
  **Note**: The maximum number of subscribers can't currently be supervised ASIL level **overarching**. I.e. in case of
  a service instance provided in `ASIL-B` quality, a QM proxy during subscribe can only verify, that the number of QM
  subscribers doesn't exceed the configured `maxSubscribers`. The same applies to an `ASIL-B` proxy. It can only verify,
  that its subscription doesn't exceed the configured number `maxSubscribers` by ASIL-B subscribers. But there is no
  cross-check between QM and ASIL-B subscribers.
- `enforceMaxSamples`: (optional on provider side, default is `true`) - normally &ndash; this property being set to
  `true` &ndash; it is checked in every subscribe call to an event or field, that the call with its given sample count
  will **not** exceed the number configured in `numberOfSampleSlots`.
  E.g. if `numberOfSampleSlots` was configured as `2`,
  and we have already one consumer, who successfully subscribed for `2` samples, then a further subscribe call by a
  consumer to subscribe for `1` sample won't succeed. Being able to overwrite this default behaviour by setting the
  property to `false` provides a possibility to lower resource usage in specific use cases, but needs extreme **caution**.
  The general idea with configuration hints given for property `numberOfSampleSlots` and the default setting for
  `enforceMaxSamples` is, that consumers are completely independent of each other, with regard of how many of their
  announced `maxSamples` they hold at a given point in time and when they free such samples again. But this complete
  independence comes at the cost, that `numberOfSampleSlots` needs to be large, which occupies memory and also means
  that slot allocation might take a bit longer.
  If one knows at configuration time already the exact behaviour of the producer and consumers, the `numberOfSampleSlots`
  could be configured smaller.
  Example: Given we have an event E, which is consumed by two consumers, each subscribing with `maxSamples` = 1.
  According to our "rule" of a typical sane number for `numberOfSampleSlots`, we would configure `numberOfSampleSlots` =
  `(1 + 1) + 1 = 3`. But if we know scheduling and behaviour of producer and consumers and in our case we have a strict
  sequence:
  - Producer gets scheduled and emits one sample.
  - Consumer 1 gets scheduled, references the last emitted sample, processes it and releases the sample again
  - Consumer 2 gets scheduled, references the last emitted sample, processes it and releases the sample again

  then we can get away with `numberOfSampleSlots` set to just 1! I.e. each of the communication partners will in this
  restricted setup work on the same slot. To allow this optimization `enforceMaxSamples` must be set to `true`, otherwise
  the subscription of the 2nd consumer would already be rejected.
- `numberOfIpcTracingSlots`: (optional - default is 0) - `mw::com` supports tracing of data traffic (event and field
  data being exchanged). The tracing subsystem behaves like an additional consumer. I.e. it semantically also subscribes
  to an event or field and accesses samples for the time it takes to write the sample data to the trace output channel and
  then releases the sample again. Depending on the sample production rate of the provider and the rate in which the tracing
  subsystem can consume samples and write them to its output channel eventually more than one tracing specific sample slot
  is required not to lose data. The tracing subsystem typically runs in an efficient batch-mode, which means, once
  it is scheduled it can efficiently write out a whole batch of samples. Thus, increasing `numberOfIpcTracingSlots` to a
  certain value will reliably avoid losing samples in tracing.

  Note, that "internally" the configured value for `numberOfIpcTracingSlots` gets added to the configured value for
  `numberOfSampleSlots` to result in the overall number of sample slots allocated in shared-memory. However, the
  distinction into separate properties is required as those two aspects of normal communication consumers and
  tracing are different and the tracing subsystem has to explicitly know, how many slots/samples it is allowed to access
  in parallel at most. Furthermore, setting the value of `numberOfIpcTracingSlots` to 0 or not configuring it all,
  explicitly means, that tracing for this event or field is disabled.

###### methods within an instance

Within the `service-instance` json object, there is an additional binding independent property for methods:

- `methods`

For each method enlisted in the `methods` array on the [service-type->bindings](#bindings) level of the `service-type`,
a corresponding instance-specific method deployment must be provided. The method type configuration corresponds to the
instance-specific method deployment based on the `methodName`.

Example configuration:
```json
"methods": [
    {
        "methodName": "SetPressure",
        "queueSize": 20
    }
]
```

The properties of a method deployment object on the instance level are:

- `methodName`: (required) - The name of the method this configuration applies to. Must match a method name from the
  `methods` array in the service-type bindings definition.

- `queueSize`: (optional, default is 1) - Maximum number of pending method requests that can be queued on the server side
  (provider/skeleton) before new requests are rejected. This is relevant for provider side only.

  **Note**: Currently, only queue sizes of 1 are supported since we only provide an API for synchronous method calls.

#### Global Settings

The global section for the configuration of a `mw::com` application is represented by the property `global` in our json
configuration object, which is of a json object type.

```json
{
    ...
    "global": {
        "asil-level": "B",
        "applicationID": 1234,
        "queue-size": {
            "QM-receiver": 8,
            "B-receiver": 5,
            "B-sender": 12
        },
       "shm-size-calc-mode": "SIMULATION"
    },
    ...
}
```

It contains the following properties:

##### asil-level

With the `asil-level` property the ASIL level of the `mw::com` application itself gets configured. Default value is `QM`.
It is essential for an application with `ASIL B` safety requirements to set this correctly to `B`! Safety related
mechanisms like `bounds-checking` in case of shared-memory access are only turned on in case the property is configured
to `B`.

##### applicationID

This is an optional property that assigns a stable, unique identifier to the application. Its primary purpose is to enable robust crash-recovery.

- **Data Type**: `unsigned integer` (32-bit)
- **Default**: If this property is not provided, `mw::com` will fall back to using the process's real user ID (`uid`) as the identifier.
- **Usage**: It is **strongly recommended** to configure a unique `applicationID` for any production application that uses `mw::com`. Relying on the `uid` fallback is less robust, as system UIDs can be reused or changed for reasons unrelated to the application's identity. When the fallback is used, an informational message is logged to alert the user.

##### queue-size

The `queue-size` property configures the message queue sizes for `mw::com`s message-passing side-channel. I.e. certain
notifications between `mw::com` processes are exchanged via an OS specific `message_passing` infrastructure. Here we
have receive-queues separated by `ASIL level`. I.e., a `mw::com` application [configured as ASIL-B](#asil-level) will
have two message reception queues: One for messages received from `QM` applications, one for messages received from
`ASIL-B` applications.
A `mw::com` application configured as `ASIL-B` has additionally a send-queue for sending messages to `QM` applications,
which avoids, that an `ASIL-B` application gets blocked, when trying to send a message to an unresponsive `QM` `mw::com`
application.

Accordingly, there are the following config properties:

- `QM-receiver`: Queue size for incoming messages received from `QM` `mw::com` applications. `mw::com` applications
  [configured with any ASIL level](#asil-level) support this setting. As per config schema it is optional, as it does have
  a sensible default value. It needs to be increased in case overflows are detected, which is typically visible via error
  log messages on the sender side. I.e. by one of the remote applications sending messages to this application
- `B-receiver`: Queue size for incoming messages received from `ASIL-B` `mw::com` applications. Only `mw::com`
  applications [configured with ASIL-B level](#asil-level) support this setting, as only they have a separate `B-receiver`
  queue. Same applies here regarding default value and potential need for adaption like in the `QM-receiver` case.
- `B-sender`: Queue size for outgoing messages towards `QM` `mw::com` applications. Only `mw::com` applications
  [configured with ASIL-B level](#asil-level) support this setting. As per config schema it is optional, as it does have
  a sensible default value. It needs to be increased in case overflows are detected, which is visible via error
  log messages done by this application.

##### shm-size-calc-mode

`shm-size-calc-mode` is a property specific to the `SHM` binding. It supports currently only the value `SIMULATION`,
which is also the default value. It simulates the creation of the shared-memory objects for DATA and CONTROL on a
heap-memory backed memory-resource. Thus, it means heap allocation during startup. This allocated memory is then directly
freed again. The benefit is, that the simulation exactly measures with byte accuracy the memory needs for
the shared-memory objects, so we can create them once with the correct size, without any need to resize afterwards.

#### Tracing settings

A tracing specific section for the configuration of a `mw::com` application is represented by the property `tracing` in
our json configuration object, which is of a json object type.
The whole `tracing` section is optional. If it doesn't exist, then tracing functionality is disabled.

```json
{
    ...
    "tracing": {
      "enable": true,
      "applicationInstanceID": "ara_com_example",
      "traceFilterConfigPath": "./mw_com_trace_filter.json"
    },
    ...
}
```

It contains the following properties:

##### enable

The boolean property `enable` enables (`true`) or disables (`false`) tracing **globally** for the application. I.e.
independent of per event or field enabling per `numberOfIpcTracingSlots` settings > 0 (see [here](#events-and-fields-within-an-instance)) tracing can be
globally turned off and is so by default!

##### applicationInstanceID

Property `applicationInstanceID` is mandatory and identifies the trace output done by the application. If a `tracing`
section exists, the `applicationInstanceID` is mandatory, even in case tracing is [globally disabled](#enable).
Be sure to assign a system-wide unique `applicationInstanceID`, otherwise mapping the trace output to the correct
application will be impossible.

##### traceFilterConfigPath

Tracing needs a detailed filter configuration, which API events on which event or field instance shall be traced.
This information is held in a separate tracing specific filter configuration file. The property `traceFilterConfigPath`
gives the path for this filter configuration. If no path is given, it defaults to a defined path relative to the
applications home directory.
If there is no trace filter config found under the explicitly given `traceFilterConfigPath` or under the default path,
tracing will be globally disabled and this will be reported in the applications log messages.

#### Overview `service instance` property existence per role

Since especially for properties for the `service-instance` configuration, it depends on the role in which the configuration
is being used, whether a property is mandatory or optional or irrelevant, the following table gives an overview:

| Config Element                                                                                                               | Skeleton Role | Proxy Role | Comment                                                                                                                                                                               |
|------------------------------------------------------------------------------------------------------------------------------|---------------|------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| _serviceInstances.instances.instanceId_                                                                                      | required      | optional   | No instanceId on proxy means "any" id is ok in a FindService() call.                                                                                                                  |
| _serviceInstances.instances.asil-level_                                                                                      | required      | required   |                                                                                                                                                                                       |
| _serviceInstances.instances.binding_                                                                                         | required      | required   |                                                                                                                                                                                       |
| _serviceInstances.instances.shm-size_                                                                                        | optional      | -          | no value means, the skeleton calculates the shmem size on its own.                                                                                                                    |
| _serviceInstances.instances.control-asil-b-shm-size_                                                                         | optional      | -          | no value means, the skeleton calculates the shmem size on its own.                                                                                                                    |
| _serviceInstances.instances.control-qm-shm-size_                                                                             | optional      | -          | no value means, the skeleton calculates the shmem size on its own.                                                                                                                    |
| _serviceInstances.instances.allowedConsumer_                                                                                 | optional      | -          | if no _allowedConsumers_ are given at skeleton side, its shared-memory objects/messaging endpoints are created with no additional ACLs, so only basic ugo-access pattern is in place. |
| _serviceInstances.instances.allowedProvider_                                                                                 | -             | optional   | if no _allowedProviders_ are given at proxy side, we simply don't care/check, who is the provider.                                                                                    |
| _serviceInstances.instances.events.eventName_<br>_serviceInstances.instances.fields.fieldName_                               | required      | required   |                                                                                                                                                                                       |
| _serviceInstances.instances.events.numberOfSampleSlots_ <br> _serviceInstances.instances.fields.numberOfSampleSlots_         | required      | -          |                                                                                                                                                                                       |
| _serviceInstances.instances.events.maxSubscribers_ <br> _serviceInstances.instances.fields.maxSubscribers_                   | required      | -          |                                                                                                                                                                                       |
| _serviceInstances.instances.events.enforceMaxSamples_ <br> _serviceInstances.instances.fields.enforceMaxSamples_             | optional      | -          | if not given on skeleton side, defaults to true                                                                                                                                       |
| _serviceInstances.instances.events.numberOfIpcTracingSlots_ <br> _serviceInstances.instances.fields.numberOfIpcTracingSlots_ | optional      | -          | if not given on skeleton side, defaults to 0, which means tracing for this event is disabled.                                                                                         |
