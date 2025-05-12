# Extensions to resp. deviations from the AUTOSAR `ara::com` spec

The following extensions have been added to our `mw::com` implementation, which are not part of the AUTOSAR standard.

## Query ProxyEvent instance for number newly available samples

### Type: Extension

The following API signature has been added to proxy side event (field) classes:

`Result<std::size_t> GetNumNewSamplesAvailable() const noexcept`

### Description

This API shall return the number of newly received samples since the last call to `GetNewSamples()`. I.e. the number of
samples the very next call to `GetNewSamples()` would provide to the caller, if not restricted by a lower value of
argument `max_num_samples` in call to `GetNewSamples()`.

Depending on the binding implementation, it might be more complex to differentiate between the two cases:

1. "there is at least 1 new sample available"
2. "there are exactly N new samples available"

as in the 1st case an implementation simply has to check whether something has arrived in its `receive-buffers`, while
in the 2nd case the `receive-buffers` have to be inspected more detailed. Since the runtime/latency of this API shall be
kept low, we **do** accept binding implementations, which report back a number N, where 1 <= N <= _really available new
samples_,

Our `LoLa` binding implementation always returns _really available new samples_ from this API call.

### Rationale

This API extension is useful in resource sensitive setups with preference to `polling mode`. In case the user wants to
call `Subscribe()` with a `max_sample_count` reflecting exactly the number of samples, he wants to hold resp. access in
parallel, he runs into the following challenges, when trying to update the samples (`max_sample_count`)
with newly received ones via `GetNewSamples()`:

1. either he disposes one of the held samples (`SamplePtr`) before calling `GetNewSamples()`, but in case there are no
   newly received samples, he ends up with less than `max_num_samples` samples after `GetNewSamples()`. I.e. he would
   have disposed one or more samples without getting new samples as a replacement, which isn't acceptable.
2. or he registers a so called `receive-handler`, which fires, whenever new samples got received. So after
   calling `GetNewSamples()`, the user can be sure to get at least one new sample. So disposing one (oldest) sample he
   holds before the call to `GetNewSamples()` could solve this problem. The downside here: This `receive-handler` gets
   fired async and doesn't fit well with an application, which is designed to operate in pure `polling mode` regarding
   event reception. Furthermore, in an ASIL-B application it might be required, that this notification
   via `receive-handler` is reliable under all circumstances (which is currently not the case with our `LoLa` binding
   implementation.
3. or he calls `Subscribe()` with a `max_sample_count` larger than what he needs to hold in parallel, so that there is
   no need to do a premature disposal of held SamplPtrs. But this has the downside of larger memory resources being
   occupied resp. reserved.

This API extension solves the problem as it factually gives the user the same functionality as in (2) but without the
asynchronicity and the loss of (in case of LoLa) ASIL-B/reliability. I.e. before each call to `GetNewSamples()` he can
check whether new/how many new samples will be available and therefore avoid disposing valuable `SamplPtrs`, without
getting replacements! 
