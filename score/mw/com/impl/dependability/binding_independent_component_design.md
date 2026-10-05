# Skeleton/Proxy Binding architecture

## Introduction

The following structural view shows, how the separation of generic/binding independent part of a proxy/skeleton from
its flexible/variable technical binding implementation is achieved. **Note**: It does **only** reflect the common use
case of strongly typed proxies and skeletons. The special case of "generic proxies" and "generic skeletons" are described in
[design extension for generic proxies](../../design/skeleton_proxy/generic_proxy/README.md#) and [design extension for generic skeletons](../../design/skeleton_proxy/generic_skeleton/README.md#) to not bloat this class diagram even more:

<a name="classdiagram"></a>

<img alt="SKELETON_BINDING_MODEL" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/design/skeleton_proxy/skeleton_binding_model.puml">

<img alt="PROXY_BINDING_MODEL" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/design/skeleton_proxy/proxy_binding_model.puml">


The overall structure foresees proxies (`DummyProxy`) and skeletons (`DummySkeleton`), which are generated from IDL.
Both inherit from a respective base class, where otherwise redundant code that can be reused by any proxy or skeleton is
shifted to.
A concrete instance of a proxy or skeleton is always bound to a specific technical binding. Nevertheless, we want to have
a clear separation between binding independent code and binding specific code. Thus, the interfaces `SkeletonBinding`
and `ProxyBinding` are introduced for binding specific implementations. Based on the provided
`InstanceIdentifier`, the proxy and skeletons need to decide which binding to construct. This decision is encapsulated
within the `.*BindingFactory`.

One central strategy is to **_generate as little code as possible from the IDL_** (arxml), which then has the consequence
that we have to resort to a generic templated C++ code base.

### Copy/Move support for binding independent proxies/skeletons and contained service elements

### Copy

Proxies and skeletons are **not** copyable! Neither on the binding independent nor binding specific level.
This is "by design" as proxy and skeleton instances are heavyweight, and it semantically makes no sense to create a
copy of such an instance:

- **Skeleton side**: What would it mean to have a copy of a skeleton instance? To which instance would a remote proxy talk to then? If the
  user updates an event on the 1st instance, but **not** on the copy, what does this mean for the remote proxy? Which
  update does it see?
- **Proxy side**: copies are semantically _problematic_! Proxies and their resource-usage (`max-sample-count`
  used in subscribe calls) are reflected in the configuration done by the provider. Doing simple copies would completely violate this approach!
  Note, that `mw::com` allows to explicitly create multiple proxy instances for the same service instance. These
  are **not** copies, but explicitly separate instances, which happen to interact with the same service instance.

### Move

Proxies and skeletons are movable on the binding independent level! This makes completely sense from the user API
perspective! Restricting this/disallowing this on the user facing binding independent level would just lead to a bad
API experience.

However, on the binding dependent level, we **don't** support `move` for proxies and skeletons! The main reasons:

- it isn't needed as a proxy/skeleton instance on binding level is created once and then owned by the binding
  independent proxy/skeleton via unique-ptr (see further discussion of `pImpl` idiom below).
- not supporting it makes our life much easier as we then can store references to binding specific proxy/skeleton
  instances without taking care to update such references after a move.

### Checking binding validity when contructing Proxy / Skeleton

On construction, the generated skeleton (`DummyProxy` / `DummySkeleton`) checks that all the bindings of its contained service elements
(events/fields/methods) are valid. This is done by calling the `AreBindingsValid()` on `ProxyBase` / `SkeletonBase`. This will then
iterate over the maps of references to its service elements (which are dynamically registered as described [below](#registration-of-proxy--skeleton-eventsfieldsmethods-at-their-parent-proxy--skeleton))
and check if each binding was successfully created. If this is not the case, the construction of the skeleton instance
returns an error.

### Registration of Proxy / Skeleton events/fields/methods at their parent Proxy / Skeleton

Due to our architectural constraints, the `impl::ProxyBase` / `impl::SkeletonBase` (the base classes of the generated
proxy / skeleton i.e. `DummyProxy` / `DummySkeleton`) doesn't "know" its event/field/method children. Because
events/fields/methods are the members of the generated proxy / skeleton and not the `impl::ProxyBase` / `impl::SkeletonBase`.
But for various functionalities, we require the `impl::ProxyBase` / `impl::SkeletonBase` to have access to its children.
This is solved by a mechanism, where the event/field/method members of the generated proxy / skeleton class register themselves
at their parent proxy / skeleton, which they get by reference in their constructor. In their constructor, they call
`RegisterEvent()`, `RegisterField()` or `RegisterMethod()` on their parent `impl::ProxyBase` / `impl::SkeletonBase`, with their own
`special reference` (see [below](#how-we-handle-moving-of-skeletons-and-their-eventsfields)) and name. So after creation of a
generated proxy / skeleton, its base class (`impl::SkeletonBase` / `impl::SkeletonBase`) has all the event/field/method members
stored in `protected` members `events_`, `fields_` and `methods_`. In the future, these could be even made publicly
accessible to the generated (user facing) proxy / skeleton. Since these user facing proxies / skeletons have discrete
event/field/method members anyhow, there is currently no need for such a generalized access.


Our solution to this issue is the following:
For each event/field/method instance, we store a `ReferenceToMoveable<T>::Reference` on the heap.
`T` is in this case one of:

- `SkeletonEventBase`
- `SkeletonFieldBase`
- `SkeletonMethodBase`

This "special reference" is created during the construction of the event/field/method instance and is passed to the
`impl::SkeletonBase` during the registration call by reference. But since the `ReferenceToMoveable<T>::Reference` is
stored on the heap and is **neither** copyable nor moveable, it doesn't get moved when the event/field/method is moved!
Instead, the `ReferenceToMoveable<T>::Reference` instance is updated with the new address of the moved event/field/method
instance within the move constructor/move assign op of the event/field/method. So the `impl::SkeletonBase` always has a
valid reference to the event/field/method instance, even if the user moves the outer skeleton instance as long as it
uses the `ReferenceToMoveable<T>::Reference::Get()` API to access the event/field/method instance!

The mechanism to provide such a "special reference" for  `impl::SkeletonEventBase`, `impl::SkeletonFieldBase` and
`impl::SkeletonMethodBase` is realized by inheriting from `EnableReferenceToMoveableFromThis<T>`, where `T` is one of
the above-mentioned types. making it a `CRTP` pattern!
