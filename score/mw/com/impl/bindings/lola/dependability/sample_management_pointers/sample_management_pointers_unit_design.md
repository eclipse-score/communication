# LoLa Sample Management Pointers Unit Design

## Realization of `SamplePtr` and `SampleAllocateePtr`

Our implementation of `score::mw::com::SamplePtr` resp. `score::mw::com::SampleAllocateePtr` hold a binding specific
implementation of such a smartpointer. So in case of `LoLa` binding, we have `lola::SamplePtr`
/`lola::SampleAllocateePtr`, which both hold a reference to a `ConsumerEventDataControlLocalView`. This reference is
needed as the destructors alter state of event slots in the `EventDataStorage<SampleType>` (decreasing refcount or
state/timestamp change). Specifically, `lola::SamplePtr` uses `lola::SlotDecrementer` (an RAII helper) which holds a
pointer to `ConsumerEventDataControlLocalView` and calls `DereferenceEvent()` on destruction. `lola::SampleAllocateePtr`
holds a `ConsumerEventDataControlLocalView*` used for IPC-tracing (skeleton-side slot referencing for trace output).

