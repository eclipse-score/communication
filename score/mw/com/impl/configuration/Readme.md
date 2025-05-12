# Configuration concept

Our LoLa (`mw::com`) communication middleware is configured based on a `json` configuration file according to the
[following schema](ara_com_config_schema.json).

The content of such a configuration is _universal_/_generic_. This means, that a user configures service types/service
instances without specifying, whether he is going to instantiate them in the form/role of a skeleton (provider) or a
proxy (consumer)! This decision is deferred to the access to the config in the C++ code. Only then the user decides,
whether he is going to spawn a skeleton or proxy or even both from a given configuration.

## Consequences for schema and config parsing

Since there are some configuration elements/properties, which:

* are **only** related to **one** side (skeleton or proxy)
* are related to **both** sides, but are optional on one side and mandatory on the other only common needs of
  proxies/skeletons can be checked at this config parsing stage.

So the success of the parsing/schema validation, does **not** tell you, whether the desired proxy/skeleton instances can
get created from the config. There a **second step** is applied, during construction of a proxy/skeleton instance in C++
code, where it is checked, whether the related config artifacts are valid/sufficient to create the concrete role
(skeleton/proxy) from it.

**Note**: Properties of service types are currently all mandatory/required independent of role (proxy/skeleton).

## Overview `service instance` property existence per role

| Config Element                       | Skeleton Role | Proxy Role | Comment                                                                                                                                                                               |
|--------------------------------------|---------------|------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| _serviceInstances.instances.instanceId_ | required      | required   | in future will be optional in case of proxy. No instanceId on proxy then means "any" id is ok.                                                                                        |
| _serviceInstances.instances.asil-level_ | required      | required   ||
| _serviceInstances.instances.binding_ | required      | required   ||
| _serviceInstances.instances.shm-size_ | optional      | -          | no value means, the skeleton calculates the shmem size on its own.                                                                                                                    |
| _serviceInstances.instances.allowedConsumer_ | optional      | -          | if no _allowedConsumers_ are given at skeleton side, its shared-memory objects/messaging endpoints are created with no additional ACLs, so only basic ugo-access pattern is in place. |
| _serviceInstances.instances.allowedProvider_ | -             | optional   | if no _allowedProviders_ are given at proxy side, we simply don't care/check, who is the provider.                                                                                    |
| _serviceInstances.instances.events.eventName_ | required      | required   ||
| _serviceInstances.instances.events.maxSamples_ | required      | -   ||
| _serviceInstances.instances.events.maxSubscribers_ | required      | -   ||
| _serviceInstances.instances.events.maxConcurrentAllocations_ | optional      | -   | currently not implemented. If provided, the parser will terminate. |
| _serviceInstances.instances.events.enforceMaxSamples_ | optional      | -   | if not given on skeleton side, defaults to true |
