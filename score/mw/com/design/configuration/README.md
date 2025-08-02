# Configuration

The main configuration items we have to deal with in `mw::com` are mappings from "logical" service instances to real
existing service instances with their concrete used technical binding.
Currently, we prepare our configuration to support the following technical bindings:
- SOME/IP
- Shared Memory IPC

Of course, we do foresee arbitrary extensions.

## Service Type identification

AUTOSAR specification demands (SWS_CM_01010), that for every Service interface a corresponding class gets generated,
which contains members of the following types:
- `ServiceIdentifierType`
- `ServiceVersionType`

The exact type definitions are left to the implementer, but there are some strong hints to reflect the unique AUTOSAR
metamodel names.
So we decided to use the so called short-name-path of a service interface in the `ARXML` model to identify a service type.
So our implementation of `score::mw::com::ServiceIdentifierType` contains the fully-qualified service interface name in the form
of its short-name-path as its identifying name member.

_Important_: `score::mw::com::ServiceIdentifierType` is a fully binding independent identification of a service type.
The technical bindings might use complete different data types for identification of a specific service type.
Service versioning is still a very immature topic in AUTOSAR and especially in the SWS Communication Management. So
right now our implementation of `score::mw::com::ServiceVersionType` fulfills the minimal requirements by providing a major
and minor part each typed as `std::uint32_t` to fulfill minimal model compatibility.

## `ServiceTypeDeployment` vs `ServiceInstanceDeployment`
We have to clearly separate those two notions within our configuration.

### `ServiceTypeDeployment`
The `ServiceTypeDeployment` aka `Service Interface Deployment` in AUTOSAR speech, maps a binding independent
`ServiceType` to a concrete technical implementation.
The binding independent `ServiceType` is defined within AUTOSAR metamodel by its Service Interface and its
corresponding [service type identification](#service-type-identification).
So within the configuration we might need to express, how a certain abstract `ServiceType` shall be represented in a
`SOME/IP` or a `LoLa` binding. For instance, both bindings might use their own, distinct service ID
for identification and also the embedded service parts (events, fields, methods) might have different identification and
properties between a `SOME/IP` and a `LoLa` binding.

Note here, that these `ServiceTypeDeployment`s are independent of their concrete instances! E.g. an AUTOSAR service
`/a/b/c/InterfaceName` will be once mapped to a `SOME/IP` service id **_SIDn_** and this applies then to ALL instances of
this `/a/b/c/InterfaceName` service with a `SOME/IP` mapping!

### `ServiceInstanceDeployment`
The `ServiceInstanceDeployment` maps concrete instances (identified by an [Instance Specifier](#instance-specifiers)) of
a `ServiceType`, to a `ServiceTypeDeployment` and extends it with instance specific properties! Among those instance
specific properties are:
- binding specific instance identifier,
- ASIL level this instance is implemented for,
- instance specific tuning like maximum event storage ...

### Deployment time decisions
While AUTOSAR generally foresees, that `ServiceTypeDeployment` aka `Service Interface Deployment` is a
generation, i.e. pre-compile step, this is **not** true for our implementation! The amount of code, which is affected at
generation time is kept minimal and only applies to binding independent parts of `mw::com`. As this is required by the
`ara::com` specification.

Our technical binding implementation is configured during runtime, when both artifacts `ServiceTypeDeployment` and
`ServiceInstanceDeployment` are read from configuration.

### Responsibility for `ServiceTypeDeployment` and `ServiceInstanceDeployment`

The responsibility for `ServiceInstanceDeployment` is clearly assigned to the integrator of the ECU. The knowledge
about processes and applications is needed, in addition to the knowledge on how to package the `ServiceInstanceDeployment`
config artifacts with the applications.

For `ServiceTypeDeployment` this is not so clear. In case of `ServiceTypeDeployments` for local only communication,
which is the case of our `LoLa` binding, it is also the job of the ECU integrator, as it is only a local ECU
optimization. Without any effect to the boardnet. In case of `SOME/IP` `ServiceTypeDeployments`, it might be expected
for the future, that parts of `ServiceTypeDeployment` come from central toolchains (Symphony).

## Instance Specifiers

`InstanceSpecifier` is an AUTOSAR AP mechanism to specify some instance of a service type in a binding or deployment
independent way **within the source code**!
If you look at the underlying (`ARXML`) model of your software component, you express the provision of a specific
service instance or the requirement of a specific service instance with a P-port or R-port respectively, which is typed
by the service interface, from which the [service type identification](#service-type-identification) is deduced.
Such a port instance also has (like a service interface) a fully qualified name (expressed via a short-name-path),
which exactly reflects the `InstanceSpecifier`.

### Mapping to concrete (technical) instances
As mentioned [above](#responsibility-for-servicetypedeploymentserviceinstancedeployment), it is the task of the
integrator, to map those `InstanceSpecifier`s to concrete technical instances of the services.

We decided to implement this mapping description via a JSON format.
An example of such a mapping is shown here:

**Note:** The shared memory binding `LoLa` is denoted as `SHM`.

```javascript
{
    "serviceTypes": [
        {
            "serviceTypeName": "/bmw/ncar/services/TirePressureService",
            "serviceTypeId": 99999999999,
            "version": {
                "major": 12,
                "minor": 34
            },
            "bindings": [
                {
                    "binding": "SOME/IP",
                    "serviceId": 1234,
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft",
                            "eventId": 633
                        }
                    ],
                    "fields": []
                },
                {
                    "binding": "SHM",
                    "serviceId": 1234,
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft",
                            "eventId": 20
                        }
                    ],
                    "fields": []
                }
            ]
        }
    ],
        "serviceInstances": [
        {
            "instanceSpecifier": "abc/abc/TirePressurePort",
            "serviceTypeName": "/bmw/ncar/services/TirePressureService",
            "instances": [
                {
                    "instanceId": 1234,
                    "asil-level": "QM",
                    "binding": "SOME/IP"
                },
                {
                    "instanceId": 62,
                    "asil-level": "ASIL-B",
                    "binding": "SHM",
                    "events": [
                        {
                            "eventName": "CurrentPressureFrontLeft",
                            "maxSamples": 50,
                            "maxSubscribers": 5
                        }
                    ],
                    "fields": []
                }
            ]
        }
    ]
}
```
As you can see in this example configuration, it provides both: A `ServiceTypeDeployment` (`service_types`) in the upper
part and a `ServiceInstanceDeployment` (`service_instances`) in the lower part.

As you see in this example, we map the `InstanceSpecifier` (i.e. port) `"abc/abc/TirePressurePort"` to concrete service
instances.
What is **not** visible here: Whether `"abc/abc/TirePressurePort"` is a provided or required service instance. Both
could be possible, since we do support 1 to n mappings in both cases.
Here we have a mapping of `"abc/abc/TirePressurePort"` to two different concrete technical instances: The first one is a
SOME/IP based instance (so it is most likely used for inter ECU, network communication) and the second is a concrete
instance based on our shared memory IPC for ECU local communication.

#### C++ representation of configuration and mappings
The JSON representation of the configuration shown above gets read and parsed at application startup within call to one
of the static `score::mw::com::impl::Runtime::Initialize()` methods. We do provide various overloads, to either allow
initialization from a default manifest or configuration path, an explicit user provided path or even (for unit testing)
a directly handed over JSON.
The sequence during startup would look like this:

![SEQUENCE_STARTUP_VIEW]("broken_link_k/swh/safe-posix-platform/score/mw/com/design/configuration/sequence_startup_view.puml")

During this call a singleton instance of `score::mw::com::impl::Runtime` gets created, which gets the parsed/validated
configuration in the form of `score::mw::com::detail::Configuration`.
`score::mw::com::detail::Configuration` in essence holds two maps:
* one (`serviceTypes`), which holds the `ServiceTypeDeployment`s, where the key is the `ServiceIdentifierType`
* one (`instanceInstances`), which holds the `ServiceInstanceDeployment`s, where the key is the `InstanceSpecifier`. The
  `ServiceInstanceDeployment`s refer to/depend on the `ServiceTypeDeployment`s.

Details can be seen in the following class diagram:

![STRUCTURAL_VIEW]("broken_link_k/swh/safe-posix-platform/score/mw/com/design/configuration/structural_view.puml")
