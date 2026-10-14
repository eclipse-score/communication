### Proxy creation
The mechanism regarding binding abstraction resp. runtime binding selection on proxy side is almost identical to the
skeleton side. A small difference is, that the starting point of a proxy instance creation is a handle (`HandleType`).
This handle is returned by the service discovery and includes (depending on the technical binding) maybe even more
information than the InstanceIdentifier used as starting point on the skeleton side. So here the analog to the
`SkeletonBase`,`SkeletonBinding`and `SkeletonBindingFactory` are `ProxyBase`, `ProxyBinding`/`ProxyBindingFactory`.

So the user creates instances of generated proxy classes (referred to as `DummyProxy` in our class diagrams)
(see [Introduction](../../impl/dependability/binding_independent_component_design.md#introduction)) via static `<DummyProxy>::Create(HandleType)`. These instances inherit from
`ProxyBase`, which follows the same architectural `pImpl` paradigm as the skeleton side by dispatching to a binding
specific implementation of `ProxyBinding`.

`ProxyBase`/`ProxyBinding` do not contain any public/user facing instance methods beside the rather "technical"
`GetHandle()` method, simply because `ara::com` does not currently define functionality/APIs on proxy instance level.
On `ProxyBase` level we have two types of methods:

- public static/class methods for finding/stop finding service instances. These static methods dispatch to a binding
  specific implementation of service discovery (which gets retrieved via `Runtime::getInstance().GetServiceDiscovery()`)
- internal/implementation specific method `AreBindingsValid()`, which is a method being called **after** construction of
  a generated proxy instance, to detect, whether the instance can be successfully returned from
  `static Result<generated proxy class> <generated proxy class>::Create()` or not.

Similarly to the skeleton side, the `impl::ProxyBase` doesn't "know" its event/field/method children. Therefore,
the `impl::ProxyEventBase` registers itself with the `impl::ProxyBase` in the same way as `impl::SkeletonEventBase`
(as explained
[here on the skeleton side](#binding-independent-level-registration-of-skeleton-eventsfields-at-their-parent-skeleton). 
It will then call `impl::ProxyBase::GetConstructionResult()` and will return a valid proxy to the user if none of the 
bindings received an error from the binding factory when trying to construct the binding.
