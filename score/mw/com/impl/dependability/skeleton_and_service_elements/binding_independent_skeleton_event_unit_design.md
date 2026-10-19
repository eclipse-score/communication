# Binding-Independent Skeleton Event Unit Design

## Event/Field communication in `LoLa`

On the skeleton side, we have `impl::SkeletonField<SampleType>`, which is just like `impl::SkeletonEvent` a class
template, with the `SampleType` (type of event/field value) as template argument. It inherits from
`impl::SkeletonFieldBase`, which reveals the composite nature, because it aggregates via member
`skeleton_event_dispatch_` the `impl::SkeletonEventBase`, where it dispatches all its event specific behavior parts to.
As soon as we have impl::SkeletonMethodBase. the `impl::SkeletonFieldBase` will be enriched with optional members
`skeleton_method_get_dispatch_`/`skeleton_method_set_dispatch_`!

### Skeleton side Activities for Event Access

The API entry point to events on the server/skeleton side is

- either the `Event::Send(SampleType&)` API
- or the `Event::Allocate()`/`Event::Send(SampleAllocateePtr<SampleType>&&)` API combination

In any case a memory-allocation (rather a free event slot search) needs to be done. The general activity to be done in
this case is pictured in the following activity diagram:

<img alt="LOLA_EVENT_ALLOCATE_ACTIVITY" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/impl/bindings/lola/dependability/event_data_control/lola_event_allocate_activity.puml">

In case of a mixed criticality setup with an ASIL-B provider/skeleton and both - ASIL-B and ASIL-QM consumers, where
discrete control structures are needed to separate ASIL-B/QM, the activities to be taken by the ASIL-B skeleton are as
follows:

<img alt="LOLA_EVENT_ALLOCATE_MIXED_CRITICAL_ACTIVITY" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/impl/bindings/lola/dependability/event_data_control/lola_event_allocate_mixed_critical_activity.puml">

During this activity a more complex sub-activity is executed/referenced, to achieve a coherent change of slot states in
both control structures (containing a rollback mechanism). This activity is modeled separately here:

<img alt="LOLA_COHERENT_SET_WRITING_ACTIVITY" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/impl/bindings/lola/dependability/event_data_control/lola_coherent_set_writing_activity.puml">

