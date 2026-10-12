#### Extract type agnostic code

The [class diagram](../../impl/dependability/binding_independent_component_design.md#classdiagram) also shows, that our `LoLa` proxy event binding implementation (`lola::ProxyEvent`)
aggregates an object of type `lola::ProxyEventCommon`, to which it dispatches all its `SampleType` agnostic method
calls, it has to implement to fulfill its interface `ProxyEventBindingBase`.
The reason for this architectural decision is described in the [design extension for generic proxies](./generic_proxy/README.md)

Similarly, on the skeleton side, `lola::SkeletonEvent` aggregates an object of type `lola::SkeletonEventCommon`. This class encapsulates all `SampleTyp`e agnostic logic (such as interaction with `lola::Skeleton` for offering services, timestamp management, and notification handling). This allows both strongly typed skeletons and generic skeletons to share the same core implementation logic.

