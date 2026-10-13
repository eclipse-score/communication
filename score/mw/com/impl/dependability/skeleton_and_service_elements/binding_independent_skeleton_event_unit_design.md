# Event/Field communication in `LoLa`

### Skeleton side

On the skeleton side, we have `impl::SkeletonField<SampleType>`, which is just like `impl::SkeletonEvent` a class
template, with the `SampleType` (type of event/field value) as template argument. It inherits from
`impl::SkeletonFieldBase`, which reveals the composite nature, because it aggregates via member
`skeleton_event_dispatch_` the `impl::SkeletonEventBase`, where it dispatches all its event specific behavior parts to.
As soon as we have impl::SkeletonMethodBase. the `impl::SkeletonFieldBase` will be enriched with optional members
`skeleton_method_get_dispatch_`/`skeleton_method_set_dispatch_`!
