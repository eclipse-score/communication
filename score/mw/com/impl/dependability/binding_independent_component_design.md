# Skeleton/Proxy Binding architecture

## Introduction

The following structural view shows, how the separation of generic/binding independent part of a proxy/skeleton from
its flexible/variable technical binding implementation is achieved. **Note**: It does **only** reflect the common use
case of strongly typed proxies and skeletons. The special case of "generic proxies" and "generic skeletons" are described in
[design extension for generic proxies](generic_proxy/README.md#) and [design extension for generic skeletons](generic_skeleton/README.md#) to not bloat this class diagram even more:

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


### Copy/Move support for binding independent and dependent proxies/skeletons

### Copy

Proxies and skeletons are **not** copyable! Neither on the binding independent nor binding specific level.
This is "by design" as proxy and skeleton instances are heavyweight, and it semantically makes **no sense** to create a
copy of such an instance:

- what would it mean to have a copy of a skeleton instance!? To which instance would a remote proxy talk to then? If the
  user updates an event on the 1st instance, but **not** on the copy, what does this mean for the remote proxy? Which
  update does it see?
- also on the proxy side copies are semantically _problematic_! Proxies and their resource-usage (`max-number-of-samples`
  used in subscribe calls) are reflected in the configuration done by the provider (in `LoLa` binding this regarding
  slot allocation resources in shared-memory). Doing simple copies would completely violate this approach!
  Note, that `LoLa`/`mw::com` allows to explicitly create multiple proxy instances for the same service instance. These
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

#### Binding independent level Registration of skeleton events/fields/methods at their parent skeleton

On construction, the generated skeleton (`DummySkeleton`) checks that all the bindings of its contained service elements
(events/fields/methods) are valid. This is done by calling the `AreBindingsValid()` on `SkeletonBase`. This will then 
iterate over the maps of references to its service elements described [below](#binding-independent-level-registration-of-skeleton-eventsfields-at-their-parent-skeleton) 
and check if each binding was successfully created. If this is not the case, the construction of the skeleton instance 
returns an error.

Due to our architectural constraints, the `impl::SkeletonBase` (the base class of the generated skeleton/`DummySkeleton`)
doesn't "know" its event/field/method children. Because events/fields/methods are the members of the generated skeleton
and not the `impl::SkeletonBase`. But for various functionalities, we require the `impl::SkeletonBase` to have access to
its children.
This is solved by a mechanism, where the event/field/method members of the generated skeleton class register themselves
at their parent skeleton, which they got by reference during their creation/`ctor` call. So in their `ctor` they are
calling `impl::SkeletonBase::RegisterEvent()`, `impl::SkeletonBase::RegisterField()` or
`impl::SkeletonBase::RegisterMethod()`, with their own `special reference` (see below) and name. So after creation of a
generated skeleton, its base class (`impl::SkeletonBase`) has all the event/field/method members stored in `protected`
members `events_`, `fields_` and `methods_`, so that in the future these could be even made `public`
accessible to the generated (user facing) skeleton. Since these user facing skeletons have discrete event/field/method
members anyhow, there is currently no need for such a generalized access.

So while we have this relation/accessibility of children from the "parent" skeleton on the binding **independent**
level, we **don't** have a symmetric setup on the binding **dependent** level. The binding specific implementation of
`SkeletonBinding`, where `impl::SkeletonBase` is dispatching to via the `pImpl` idiom doesn't "know" its corresponding
binding specific events, fields, which would be represented **both** as `SkeletonEventBinding` (as our field is a
composite, which dispatches to `impl::SkeletonEventBase`, which then &ndash; also via `pImpl` idiom &ndash; dispatches
to the binding specific `SkeletonEventBinding` [see here](#../todo)). Nor does it know its binding specific methods,
which would be represented as `SkeletonMethodBinding`.

This symmetry isn't currently needed as we don't have any use case yet, where on the binding dependent level our
`lola::Skeleton` (implementing `SkeletonBinding`) would need to call functionality on its events/fields!
I.e. all our use cases, where a skeleton has to delegate some functionality to its "children" (events/fields), happens
currently solely on the binding independent level.

**_Note_**: If this would need to change in the future, we would only store `SkeletonEventBinding` instances within an
implementation of `SkeletonBinding`, since we do not have (yet) a field specific class on binding level! To detect,
whether we have to deal with a "pure" event or the "event part" of a field, we would resort to checking the related
`ElementFqId`, which contains this differentiation.

#### How we handle moving of skeletons and their events/fields

In the previous section we have seen, that impl::SkeletonEvent, impl::SkeletonField and impl::SkeletonMethod register
themselves at their parent `impl::SkeletonBase` during their construction with a reference to their parent.
The special reference is obviously not a normal reference, but a `ReferenceToMoveable<SkeletonEventBase>::Reference` type
(or `ReferenceToMoveable<SkeletonFieldBase>::Reference` resp. `ReferenceToMoveable<SkeletonMethodBase>::Reference`).
Why is that? If we stored a normal reference to the event/field/method in the parent skeleton, we would have the
following problem: whenever the event/field/method instances get moved (which happens, when the user moves his outer
skeleton instance), the references within the `impl::SkeletonBase` need to be updated. To accomplish this, the
event/field/method instances would also need a reference to their parent () to do the update! This again complicates
things further! In case the `impl::SkeletonBase` moves (also happens, when the user moves his outer skeleton instance),
then also the parent reference has to be updated within all the event/field/method instances.

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
