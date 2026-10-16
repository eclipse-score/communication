# Binding-Independent Sample Management Pointers Unit Design

## Realization of `SamplePtr` and `SampleAllocateePtr`

`mw::com` defines two `std::unique_ptr` like pointers for accessing sample data of an event:

* `score::mw::com::SamplePtr` is the smartpointer used on the proxy side to access an event sample
* `score::mw::com::SampleAllocateePtr` is the smartpointer used on the skeleton side in case of usage of the zero-copy API
  path to fill event-data "in-place".

## Implementation of reference counting in SamplePtr

When subscribing to an event on proxy side, it is required to also state the maximum number of samples the application
will reference at any time. Since it is crucial for the memory management of some bindings (including the shared memory
binding provided by LoLa) to rely on this number, there is a mechanism that enforces this limit in situations where the
application (either deliberately or by accident) requests more samples on a call to `GetNewSamples` than it is entitled
to use.

This mechanism is implemented inside the binding-independent `SamplePtr` class. However, the binding-specific
implementations of SamplePtr also need to be aware of this infrastructure since only they know exactly how many samples
in the end are handed over to the application, and thus they need to make single allocations for each sample that could
potentially be sent to the application.

The reference counting is divided into three tightly coupled classes, namely `SampleReferenceTracker`,
`TrackerGuardFactory` and `SampleReferenceGuard`. They represent the different stages of sample reference allocation:

* `SampleReferenceTracker` is associated with a `ProxyEvent` and keeps track of the overall number of free samples.
* `TrackerGuardFactory` represents a reserved number of samples, governed by the user's desired number of samples he/she
  wants to retrieve in a call to `GetNewSamples`. It will produce instances of `SampleReferenceGuard` and it will hand
  back unused samples in case the binding isn't able to return the number of samples the user requested.
* `SampleReferenceGuard` represents a single sample that has been handed out to the user. This class is tied to the
  lifetime of a SamplePtr; on destruction of the `SamplePtr`, `SampleReferenceGuard` will hand back the reference to its
  associated `SampleReferenceTracker`.

Since it is possible that both acquisition of new samples and dropping of old samples happen in different threads, the
central counter buried inside `SampleReferenceTracker` is atomic.

This sequence diagram depicts the mechanics behind the reference counting on proxy side:

<img alt="SAMPLEPTR_REF_COUNTING" src="https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/eclipse-score/communication/refs/heads/main/score/mw/com/impl/dependability/sample_management_pointers/sampleptr_ref_counting.puml">
