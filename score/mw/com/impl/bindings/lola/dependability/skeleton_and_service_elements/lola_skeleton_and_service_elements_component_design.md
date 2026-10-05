#### LoLa binding level Registration of skeleton events/fields at their parent skeleton

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