# LoLa Skeleton and Service Elements Unit Design

## Registration of references to skeleton events/fields/methods at their parent skeleton

A `lola::Skeleton` is constructed via the `SkeletonBindingFactory` (TODO: add link) in the constructor of an `impl::SkeletonBase`.
As described in (TODO: add link to registration of service elements with parent in binding indep level), the `impl::SkeletonBase`
does not have access to its corresponding service elements since these are owned by the generated skeleton (`DummySkeleton`) which
is a derived class of `impl::SkeletonBase`. Therefore, we have no way of providing references to the service elements
on the `LoLa` level when constructing a `lola::Skeleton`.

** Events / Fields**

Currently, the `lola::Skeleton` does not need to access any `lola::SkeletonEvent`s. Therefore, we don't need any dynamic registration
of event bindings. All our use cases, where a skeleton has to delegate some functionality to its "children" (events/fields), happens
currently solely on the binding independent level.

**_Note_**: If this would need to change in the future, we would only store `SkeletonEventBinding` instances within an
implementation of `SkeletonBinding`, since we do not have (yet) a field specific class on binding level! To detect,
whether we have to deal with a "pure" event or the "event part" of a field, we would resort to checking the related
`ElementFqId`, which contains this differentiation.

** Methods **

For the method implementation on `LoLa` level, we decided to have one shared memory region per proxy instance, rather than per proxy
method instance (See details here TODO: add link to method docs). Therefore, the `lola::Skeleton` will open one shared memory region per
connected `lola::Proxy` instance. Since the `lola::Skeleton` is the owner of the shared memory, calls to a method to notify that
a `lola::Proxy` has subscribed / unsubscribed go via the `lola::Skeleton` which then calls down each `lola::SkeletonMethod` (with for
example pointers to that method's part of shared memory).

So for methods, the `lola::Skeleton` does need access to its `lola::SkeletonMethod`s. We therefore perform dynamic registration, similar
to what is done on the binding independent level in which each `lola::SkeletonMethod` calls `RegisterMethod` on its parent `lola::Skeleton`.
The `lola::Skeleton` stores a map of references to each `lola::SkeletonMethod` which are not copyable or movable so are guaranteed to always
be valid (except during destruction which is described here TODO: add link).


## Registration of skeleton events/fields for shared memory creation at their parent skeleton

Despite the last paragraph in the previous chapter, we are doing still "some registration" in our `LoLa`/shared-memory
binding from `lola::SkeletonEvent` at its parent `lola::Skeleton`!

This registration differs from the registration done on the binding independent layer explained above:

1. Registration does **NOT** take place at construction time of `lola::SkeletonEvent`, but during
   `lola::SkeletonEvent::PrepareOffer`. I.e. in the context of `impl::Skeleton::OfferService()`.
2. During this call to `lola::Skeleton::Register()` the `lola::Skeleton` does **NOT** store any references/links to the
  `lola::SkeletonEvent`, which is doing this call.

Instead, this registration approach via `lola::Skeleton::Register()` is done to allow the `lola::SkeletonEvent` to
set up its specific storage in shared memory! Since the `lola::Skeleton` is the owner/creator of the whole shared-memory
object, where all the events/fields/(later also methods) belonging to this `lola::Skeleton` are stored, the access to
the shared-memory object has to be done through the parent `lola::Skeleton` instance. As shown in the 2nd part of the
sequence diagram in [chapter for skeleton creation](../../../../dependability/skeleton_and_service_elements/binding_independent_skeleton_and_service_elements_component_design.md#skeleton-and-service-element-creation), the `lola::SkeletonEvent`s are enriching the
event maps prepared by the `lola::Skeleton` in `lola::Skeleton::PrepareOffer()` (and stored within shared-memory) with
their event specific storage. This job has to be done by/shifted to the `lola::SkeletonEvent` as it needs the event/
field type information, which only the `lola::SkeletonEvent` has (not the `lola::Skeleton`!). This is also the reason,
that `lola::Skeleton::Register()` is a method template. The `lola::SkeletonEvent` calls the method template
instantiation with its event/field type.

## Event/Field communication in `LoLa`

### Slot size calculation

The number of slots required for a field is dependent on the presence of `WithNotifier`, `WithGetter`, and `WithSetter`.
If `WithNotifier` is present, the proxy can subscribe and receive notifications, so the configured value of
`numberOfSampleSlots` is taken into account. The configured value should be calculated based on the total number of subscribers 
and the max number of samples they subscribe with (see [here for details](#sizing-of-event_slots-and-state_slots-vectors)).
If `WithNotifier` is not present, then `WithGetter` must be present (otherwise a consumer has no way of getting a field value).
In this case, `numberOfSampleSlots` should be configured according to the number of `SampleAllocateePtrs` (can be allocated via a
call to `Allocate()` on the `SkeletonField`) which can be made held while the maximum number of concurrent getter calls are in 
progress. E.g. we currently support 1 concurrent getter call, so `numberOfSampleSlots` of 0 would mean that `Allocate()` can not
be called while a getter call is in progress. `numberOfSampleSlots` of 1 would mean that `Allocate()` can be called once while a 
getter call is in progress, but the next call to `Allocate()` would return an error until the getter completes or the previous 
`SampleAllocateePtr` is released. This generalizes to `numberOfSampleSlots` of N.

In addition, if `WithGetter` is present, the number of slots will be increased by the number of concurrent getter calls that 
are allowed (currently 1), since each getter will reference (and thus prevent writing to the slot) while it copies the value 
to the setter method call. 

In addition, if `WithSetter` is present, the number of slots must be increased by 1, since the setter must call `Update()` 
to write the new value into the slot, and thus must reference the slot while it copies the value from the setter method call. 
These additional slots are added by the middleware implementation, and are not configurable by the user. 

Configured `numberOfIpcTracingSlots` come on top of the used slot count in all cases, because tracing holds a
reference to each traced sample until the trace call has completed.
