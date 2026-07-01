# Chapter 6: Strategies for Sizing sample slots

This chapter is about correctly sizing the `numberOfSampleSlots` property of an event (or field) in the provider's
`mw_com_config.json`.

`numberOfSampleSlots` is a property the **provider** of a service instance configures. But its correct value does not
depend on the provider alone &ndash; it depends on the **consumers**: on *how many* consumers (proxy instances) subscribe
to the event and with which `max_sample_count` they subscribe.

The `max_sample_count` a subscriber passes to `Subscribe()` expresses how many samples (in the form of `SamplePtr`s) that
subscriber wants to be able to **hold in parallel**.

### Why the sum of all `max_sample_count`s?

Consumers are, in general, **completely independent** of each other. The point in time when a consumer calls
`GetNewSamples()` and takes hold of a `SamplePtr`, and the point in time when it releases that `SamplePtr` again, is
completely decoupled from every other consumer.

Assume we have at most two subscribers for a given event: subscriber&nbsp;1 wants to hold up to **two** samples in
parallel, subscriber&nbsp;2 up to **three**. Then, because the two can hold their samples at completely independent
points in time, the provider must provide at least `2 + 3 = 5` sample slots. Consider these two extreme situations for
the underlying control-slot-vector (of size 5):

1. **Closely coupled:** consumer&nbsp;1 acquires new samples and keeps the `SamplePtr`s; shortly after, consumer&nbsp;2
   does the same and ends up referencing largely the *same* slots. We might then have two slots with a reference count of
   2 (referenced by both consumers), one slot with a reference count of 1 (only consumer&nbsp;2), and two remaining slots
   that are not referenced by either consumer. Such a not-referenced slot is either *uninitialized* (does not yet contain
   a valid sample), *in writing* (the provider is currently updating its data), or *unused* (contains a valid sample, but
   currently no subscriber references it).
2. **Completely detached:** at a specific point in time consumer&nbsp;1 references slots 0 and 3, while consumer&nbsp;2
   references slots 1, 2 and 4. Now **all five slots are simultaneously blocked** by the two consumers.

### Why the "+1"?

Situation&nbsp;2 above exposes a problem. If, in that instant, the provider wants to publish a new sample (via
`SkeletonEvent::Send()` / `Allocate()`), there is **no slot left**. The provider then gets an error from `Allocate()` or
`Send()`. This is bad for two reasons:

- The **provider** now has to do awkward error handling: it must distinguish "a real error happened" from "all slots are
  simply in use right now".
- The **consumers** suffer, too: while the provider was blocked (all slots occupied), every new sample it produced had to
  be *dropped*, because there was no slot to publish it into. So when a consumer later releases a `SamplePtr` and calls
  `GetNewSamples()` again, it will not get a new sample until the provider has become active again *after* that release.

Because of this, there is a **rule of thumb** for sizing `numberOfSampleSlots`:

> `numberOfSampleSlots = Sum of (max_sample_count of all subscribers) + 1`

With this small `+ 1` adjustment there is always **one** slot available for the provider to publish its newest sample
into &ndash; even in the extreme situation&nbsp;2. As a nice side effect, an error returned by `Allocate()` /
`Send()` can then no longer be caused by "no slot available", so it is always a *real* error.

### Files/artifacts used

The `bazel` project for this chapter is located in `score/mw/com/doc/tutorial/chapter_6`.

----------------------------------------

| File Name                                | Description                                                                          |
|------------------------------------------|--------------------------------------------------------------------------------------|
| `BUILD`                                  | This file contains bazel targets for this example.                                   |
| consumer/`consumer.cpp`                  | Implementation of the service consumer app (polling, keeps `SamplePtr`s).            |
| consumer/`consumer.h`                    | Header (empty - we just always want to have cpp/h pairs) of the service consumer.    |
| consumer/`logging.json`                  | This file contains the configuration for the logging system used by `score::mw::com` |
| consumer/client_1/`mw_com_config.json`   | `score::mw::com` configuration for consumer **instance 1** (`applicationID` 17).     |
| consumer/client_2/`mw_com_config.json`   | `score::mw::com` configuration for consumer **instance 2** (`applicationID` 18).     |
| provider/`provider.cpp`                  | Implementation of the service provider (taken over from chapter 2).                  |
| provider/`provider.h`                    | Header (empty) of the service provider.                                              |
| provider/`logging.json`                  | This file contains the configuration for the logging system used by `score::mw::com` |
| provider/`mw_com_config.json`            | `score::mw::com` configuration for the provider (`numberOfSampleSlots` = 10).         |
| `hello_world_service.cpp`                | This file is empty as the service interface is completely defined in the header.     |
| `hello_world_service.h`                  | This file contains the definition of the service interface.                          |

----------------------------------------

### Provider application

The provider is **taken over from chapter 2**: it creates and offers a single instance of the `HelloWorldService` and
cyclically (every 100&nbsp;ms) sends a "message" event update. Importantly, it already logs when it cannot allocate a
sample slot &ndash; which is exactly what we want to observe in this chapter:

```cpp
auto sample_allocatee_ptr = hello_world_service_instance.message.Allocate();
if (!sample_allocatee_ptr)
{
    std::cerr << "Failed to allocate sample_allocatee_ptr: " << sample_allocatee_ptr.error() << std::endl;
    continue;
}
```

The provider configuration is kept as in chapter 2, in particular `numberOfSampleSlots` is **10**:

```json
"events": [
  { "eventName": "message", "numberOfSampleSlots": 10, "maxSubscribers": 3 }
]
```

### Consumer application

The consumers in the previous chapters were a bit unrealistic: they released a received `SamplePtr` immediately at the
end of the `GetNewSamples()` callback, so they effectively never held more than one sample at a time. The consumer of
this chapter is more realistic:

- It uses the **polling** approach again (cyclically calling `GetNewSamples()`).
- It **keeps** the received `SamplePtr`s by *moving* them out of the callback into a container (`std::deque`) that lives
  in `main()`. It therefore holds up to `max_sample_count` samples in parallel.
- It takes the `max_sample_count` to use as a **command-line argument**.
- Whenever it already holds `max_sample_count` samples, it releases the **oldest** one before calling `GetNewSamples()`
  in the next cycle.

```cpp
// The container of currently held samples; the oldest sample is at the front.
std::deque<MessageSamplePtr> held_samples{};

while (g_running.load(std::memory_order_relaxed))
{
    std::this_thread::sleep_for(kPollInterval);

    // If we already hold the maximum, release the oldest before receiving a new one.
    if (held_samples.size() >= max_sample_count)
    {
        held_samples.pop_front();
    }

    const std::size_t free_capacity = max_sample_count - held_samples.size();
    std::ignore = event.GetNewSamples(
        [&held_samples](MessageSamplePtr sample) noexcept {
            // Keep the sample by moving it out of the callback into our container.
            held_samples.push_back(std::move(sample));
        },
        free_capacity);
}
```

The concrete `SamplePtr` type is deduced from the proxy's event member (its public `SampleType` alias), so that no
`score::mw::com::impl` type has to be named in user code:

```cpp
using MessageSampleType = decltype(HelloWorldProxy::message)::SampleType;
using MessageSamplePtr = score::mw::com::SamplePtr<MessageSampleType>;
```

The set-up consists of **two** consumer instances. They share the very same `consumer` binary and only differ in the
bundled `mw_com_config.json` &ndash; specifically its `applicationID` (17 for instance 1, 18 for instance 2) &ndash; so
that both can run side by side. This is why the `BUILD` file produces two application packages (`HelloWorldClient1`,
`HelloWorldClient2`) from the same binary.

### How to run the example

```bash
bazel build //score/mw/com/doc/tutorial/chapter_6:provider-tar
bazel build //score/mw/com/doc/tutorial/chapter_6:consumer1-tar
bazel build //score/mw/com/doc/tutorial/chapter_6:consumer2-tar
```

```bash
mkdir -p /tmp/tutorial/chapter_6
tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_6/provider-tar.tar  -C /tmp/tutorial/chapter_6/
tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_6/consumer1-tar.tar -C /tmp/tutorial/chapter_6/
tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_6/consumer2-tar.tar -C /tmp/tutorial/chapter_6/
```

Start the provider in the 1st terminal:

```bash
cd /tmp/tutorial/chapter_6/opt/HelloWorldServer
bin/provider
```

... and the two consumers (each with its own `max_sample_count`) in a 2nd and 3rd terminal, e.g.:

```bash
cd /tmp/tutorial/chapter_6/opt/HelloWorldClient1
bin/consumer 4
```

```bash
cd /tmp/tutorial/chapter_6/opt/HelloWorldClient2
bin/consumer 5
```

### Playing with the sizing

With `numberOfSampleSlots` fixed at **10**, you can now explore the three interesting variations:

- **`4` and `5` &ndash; rule of thumb satisfied.** The sum of the `max_sample_count`s is `4 + 5 = 9`, plus the safety
  margin of 1 gives 10, which is exactly `numberOfSampleSlots`. In this configuration you should **never** see the
  provider log that it could not allocate a sample slot.
- **`5` and `5` &ndash; missing the `+ 1`.** Both consumers can still subscribe (`5 + 5 = 10 =
  numberOfSampleSlots`), but the safety margin is gone. Now you can observe the provider logging `Failed to allocate
  sample_allocatee_ptr` &ndash; namely whenever the two consumers happen to reference **disjoint** slots (the extreme
  situation&nbsp;2 above), so that momentarily all 10 slots are blocked. Note that this is **timing dependent**: as long
  as the two consumers reference overlapping slots (situation&nbsp;1) everything looks fine; the allocation failures
  appear only once the independent timing of the two consumers drifts them into the disjoint case (this may take a
  little while).
- **`5` and `6` &ndash; oversubscription.** The sum `5 + 6 = 11` exceeds `numberOfSampleSlots` (10). Here, **at least
  one** of the two consumers (whichever subscribes last) gets an **error during `Subscribe()`**, because its
  `max_sample_count` request can no longer be satisfied by the remaining free slots:

  ```
  Subscribe was rejected by skeleton. Cannot complete SubscribeEvent() call due to Slot overflow ...
  Failed to subscribe with max_sample_count = 6: ... This typically means that the sum of the subscribers'
  max_sample_count exceeds the provider's configured numberOfSampleSlots. Terminating.
  ```

### Summary

A short summary of what we have learned in this chapter:

- `numberOfSampleSlots` is configured by the **provider**, but its correct value is dictated by the **consumers**: by
  how many subscribe and with which `max_sample_count`.
- `max_sample_count` (passed to `Subscribe()`) is the number of `SamplePtr`s a subscriber wants to be able to hold in
  parallel. Because subscribers are independent, in the worst case all of them hold their maximum at disjoint slots at
  the same time.
- Rule of thumb: **`numberOfSampleSlots = Sum of (max_sample_count of all subscribers) + 1`**. The `+ 1` guarantees the
  provider always has a free slot for its newest sample, so it never has to drop samples and an allocation error is
  always a *real* error.
- If `numberOfSampleSlots` equals the sum without the `+ 1`, all subscribers can still subscribe, but the provider may
  (timing dependent) fail to allocate a slot and drop samples.
- If a subscriber's `Subscribe()` would push the total requested slot count beyond `numberOfSampleSlots`, that
  `Subscribe()` call fails with a slot-overflow error.
