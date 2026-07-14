# Chapter 7: Fields - events on steroids

Up to now our service interface only exposed an *event* (the "message" event). Starting with this chapter we use a
different service interface that exposes **fields** instead: the `TirePressureService`.

A **field** can be thought of as an *event on steroids*. In addition to the pure event semantics (notification of new
values via a notifier, exactly like an event) a field always carries a **current value**. This has two immediate
consequences that we will observe in this chapter:

- On the **provider** side: a field always has a value, therefore the provider has to supply an **initial value for every
  field** *before* it may offer the service. Omitting the initial value of any field makes `OfferService()` fail.
- On the **consumer** side: because a field always carries a current value, a consumer that subscribes to a field is
  guaranteed to receive that current value right after the subscription switches to `kSubscribed` &ndash; **even if the
  provider never updates the field afterwards**. For an event this is different: there a consumer only ever receives
  samples that are sent *after* it has subscribed.

Besides its notifier, a field can also offer a request/response style getter (`Get()`) and setter (`Set()`). Those are
enabled via the `WithGetter` / `WithSetter` tags and are the topic of a later chapter. In this chapter we only use the
**notifier** part of a field (tag `WithNotifier`), so that we can focus on the initial-value semantics and reuse the
event-driven reception we already know from chapter 5.

### The service interface

Instead of `hello_world_service.h` / `.cpp` this chapter introduces `tire_pressure_service.h` / `.cpp`. The
`TirePressureInterface` contains **four fields** of type `float` (the tire pressure in bar of each of the four wheels).
Each field is declared with the `WithNotifier` tag only:

```cpp
template <typename Trait>
class TirePressureInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    typename Trait::template Field<float, score::mw::com::WithNotifier> tire_pressure_front_left{
        *this, "tire_pressure_front_left"};
    typename Trait::template Field<float, score::mw::com::WithNotifier> tire_pressure_front_right{
        *this, "tire_pressure_front_right"};
    typename Trait::template Field<float, score::mw::com::WithNotifier> tire_pressure_rear_left{
        *this, "tire_pressure_rear_left"};
    typename Trait::template Field<float, score::mw::com::WithNotifier> tire_pressure_rear_right{
        *this, "tire_pressure_rear_right"};
};
```

A field takes its data type plus a pack of tags from `{WithGetter, WithSetter, WithNotifier}`. At least one of
`WithGetter` or `WithNotifier` must be present, otherwise the value would be invisible to consumers. Here we use
`WithNotifier`, which enables the notifier part of the proxy-side field API (`Subscribe()`, `GetNewSamples()`,
`SetReceiveHandler()`, `GetSubscriptionState()`, ...) &ndash; i.e. exactly the same subset we already used for events.

### Files/artifacts used

The `bazel` project for this chapter is located in `score/mw/com/doc/tutorial/chapter_7`. It contains the same set of
files as chapter 5, with the service interface renamed to `tire_pressure_service`:

----------------------------------------

| File Name                       | Description                                                                          |
|---------------------------------|--------------------------------------------------------------------------------------|
| `BUILD`                         | This file contains bazel targets for this example.                                   |
| consumer/`consumer.cpp`         | Implementation of the service consumer app. The `main()` for the consumer            |
| consumer/`consumer.h`           | Header (empty - we just always want to have cpp/h pairs) of the service consumer.    |
| consumer/`mw_com_config.json`   | This file contains the configuration for `score::mw::com` for the consumer app.      |
| consumer/`logging.json`         | This file contains the configuration for the logging system used by `score::mw::com` |
| provider/`provider.cpp`         | Implementation of the service provider.                                              |
| provider/`provider.h`           | Header (empty) of the service provider.                                              |
| provider/`logging.json`         | This file contains the configuration for the logging system used by `score::mw::com` |
| provider/`mw_com_config.json`   | This file contains the configuration for `score::mw::com` for the provider app.      |
| `tire_pressure_service.cpp`     | This file is empty as the service interface is completely defined in the header.     |
| `tire_pressure_service.h`       | This file contains the definition of the service interface (four fields).            |

----------------------------------------

### How to run the example

The steps are the same as in chapter 5. The consumer uses the **implicit** configuration loading, so it is started
**without** the `--service_instance_manifest` argument.

```bash
# Build the provider and consumer targets
bazel build //score/mw/com/doc/tutorial/chapter_7:provider-tar
bazel build //score/mw/com/doc/tutorial/chapter_7:consumer-tar
```

Extract both archives (e.g. in a tmp-directory):

```bash
mkdir -p /tmp/tutorial/chapter_7
tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_7/provider-tar.tar -C /tmp/tutorial/chapter_7/
tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_7/consumer-tar.tar -C /tmp/tutorial/chapter_7/
```

... then start the service-provider application in the 1st terminal:

```bash
cd /tmp/tutorial/chapter_7/opt/TirePressureServer
bin/provider
```

and the service-consumer in the 2nd terminal:

```bash
cd /tmp/tutorial/chapter_7/opt/TirePressureClient
bin/consumer
```

The provider first sets the **initial value** (`2.0` bar) of each field, offers the service and then waits 20&nbsp;s
before it starts updating the fields at randomized intervals:

```
Setting initial tire pressures (before offering the service):
  tire_pressure_front_left = 2 bar
  tire_pressure_front_right = 2 bar
  tire_pressure_rear_left = 2 bar
  tire_pressure_rear_right = 2 bar
OfferService: TirePressure service instance is now offered.
Waiting 20 s before updating the fields ...
Next tire pressure update in 170 ms.
Updating tire pressures:
  tire_pressure_front_left = 2.27283 bar
  ...
```

The consumer subscribes to all four fields and &ndash; thanks to the field's initial-value semantics &ndash; receives
the **initial value** (`2` bar) of each field **event-driven** right after the subscription became active, i.e. even
during the 20&nbsp;s in which the provider does not update anything. Afterwards it receives the randomized updates:

```
Found TirePressure service instance (instance id: { ... "instanceId": 1 ... }). Creating proxy.
Subscribed to field 'tire_pressure_front_left'.
Subscribed to field 'tire_pressure_front_right'.
Subscribed to field 'tire_pressure_rear_left'.
Subscribed to field 'tire_pressure_rear_right'.
Waiting for event-driven field updates ...
Field 'tire_pressure_front_left' update received (event-driven): 2 bar
Field 'tire_pressure_front_right' update received (event-driven): 2 bar
Field 'tire_pressure_rear_left' update received (event-driven): 2 bar
Field 'tire_pressure_rear_right' update received (event-driven): 2 bar
Field 'tire_pressure_front_left' update received (event-driven): 2.27283 bar
...
```

### Provider application

The provider creates a skeleton of type `AsSkeleton<TirePressureInterface>` and offers it. The essential difference to
the previous chapters (which offered an event) is that **before** the provider may offer the service, it has to supply an
**initial value for each field** via `Field::Update(const FieldType&)`. It is important to use exactly this overload of
`Update()` (the one taking the value by const-reference):

```cpp
// A field always has a current value. Therefore, BEFORE the provider may offer the service, it has to supply an
// initial value for *each* field via Field::Update(). Omitting this for any field would make OfferService() fail.
service_instance.tire_pressure_front_left.Update(2.0F);
service_instance.tire_pressure_front_right.Update(2.0F);
service_instance.tire_pressure_rear_left.Update(2.0F);
service_instance.tire_pressure_rear_right.Update(2.0F);

auto offer_result = service_instance.OfferService();
```

Under the hood, `Update()` behaves slightly differently depending on the offer state: as long as the service has not yet
been offered, `Update()` just **stores** the value as the field's initial value; the value is then actually published
(in case of `LoLa` binding into shared memory) the moment `OfferService()` sets everything up. After the service is
offered, `Update()` publishes the new value right away &ndash; exactly like sending an event sample.

After offering, the provider waits 20&nbsp;s (to give a consumer time to observe the initial values) and then updates the
four fields in a loop. Both the **cadence** (a random delay between 100&nbsp;ms and 3000&nbsp;ms) and the **values** (a
random pressure between `2.0` and `2.5` bar) are randomized:

```cpp
std::mt19937 random_engine{std::random_device{}()};
std::uniform_int_distribution<std::chrono::milliseconds::rep> delay_distribution{100, 3000};
std::uniform_real_distribution<float> pressure_distribution{2.0F, 2.5F};

while (g_running.load(std::memory_order_relaxed))
{
    std::this_thread::sleep_for(std::chrono::milliseconds{delay_distribution(random_engine)});
    // ...
    service_instance.tire_pressure_front_left.Update(pressure_distribution(random_engine));
    service_instance.tire_pressure_front_right.Update(pressure_distribution(random_engine));
    service_instance.tire_pressure_rear_left.Update(pressure_distribution(random_engine));
    service_instance.tire_pressure_rear_right.Update(pressure_distribution(random_engine));
}
```

### Consumer application

The consumer finds the `TirePressureService` instance via `StartFindService()` &ndash; exactly the same pattern as in
chapter 5: it creates the proxy from the found handle and stops the discovery right away from within the find-service
handler. It then, for **each** of the four fields, registers an `EventReceiveHandler` via `SetReceiveHandler()`
**before** subscribing. Because all four fields have identical type
(`ProxyField<float, WithNotifier>`), the set-up is factored into a small helper:

```cpp
void SetupField(TirePressureField& field, const char* const field_name)
{
    // Register the field-receive handler *before* subscribing. It is invoked whenever a new value of the field has
    // been received. Inside the handler, GetNewSamples() is guaranteed to provide at least one new sample.
    const auto set_receive_handler_result = field.SetReceiveHandler([&field, field_name]() noexcept {
        std::ignore = field.GetNewSamples(
            [field_name](auto&& sample) {
                std::cout << "Field '" << field_name << "' update received (event-driven): " << *sample.Get()
                          << " bar" << std::endl;
            },
            kMaxSampleCount);
    });
    // ... error handling (std::exit on failure) ...

    const auto subscribe_result = field.Subscribe(kMaxSampleCount);
    // ... error handling (std::exit on failure) ...
}
```

```cpp
// Inside the find-service handler, after the proxy has been stored at its stable location:
SetupField(proxy.value().tire_pressure_front_left, "tire_pressure_front_left");
SetupField(proxy.value().tire_pressure_front_right, "tire_pressure_front_right");
SetupField(proxy.value().tire_pressure_rear_left, "tire_pressure_rear_left");
SetupField(proxy.value().tire_pressure_rear_right, "tire_pressure_rear_right");
```

The important observation is: it is **expected** that each field-receive handler gets called (and `GetNewSamples()` finds
at least one new sample) **right after** the subscription became active &ndash; because of the **initial-value semantics**
of a field. A field, in contrast to an event, always has a current value; therefore the receive handler fires after a
successful subscription and switch to `kSubscribed`, delivering that current (here: the initial `2.0` bar) value. Only
afterwards, whenever the provider updates a field, the corresponding handler fires again.

As in chapter 5, the consumer is **purely event-driven**: there is no polling loop. The main thread just blocks on a
condition variable until a termination signal requests the shut down, and any unrecoverable set-up error terminates the
application immediately via `std::exit(EXIT_FAILURE)`.

> **A note on the receive handler's capture size:** `EventReceiveHandler` is a `score::cpp::callback<void(void)>` with a
> fixed inline storage (32 bytes). We therefore capture the field by pointer (`&field`, 8 bytes) and the field name as a
> string literal (`const char*`, 8 bytes) instead of, say, a `std::string` (which alone would already exceed the
> available storage).

### Configuration

The configuration now describes **fields** instead of events. In the service type binding each field gets a `fieldName`
and a unique `fieldId`; in the service instance each field gets its `numberOfSampleSlots` and `maxSubscribers`
(exactly analogous to how events were configured before). The relevant part of the provider
([provider/mw_com_config.json](provider/mw_com_config.json)) looks like this:

```json
"bindings": [
  {
    "binding": "SHM",
    "serviceId": 8712,
    "fields": [
      { "fieldName": "tire_pressure_front_left",  "fieldId": 1 },
      { "fieldName": "tire_pressure_front_right", "fieldId": 2 },
      { "fieldName": "tire_pressure_rear_left",   "fieldId": 3 },
      { "fieldName": "tire_pressure_rear_right",  "fieldId": 4 }
    ]
  }
]
```

The consumer ([consumer/mw_com_config.json](consumer/mw_com_config.json)) maps its `instanceSpecifier`
`MyTirePressureServiceInstance` to that service type **with** the explicit `instanceId` 1, so that the
`StartFindService()` call finds exactly the one instance the provider offers.

### Summary

A short summary of what we have learned in this chapter:

- A **field** is like an *event on steroids*: it has the same notifier semantics as an event, but in addition always
  carries a **current value**. On the proxy side its notifier API (`Subscribe()`, `GetNewSamples()`,
  `SetReceiveHandler()`, ...) is identical to that of an event.
- A field is declared via `Trait::template Field<DataType, Tags...>` with tags from `{WithGetter, WithSetter,
  WithNotifier}`. At least one of `WithGetter` / `WithNotifier` must be present. In this chapter we used `WithNotifier`.
- The provider must supply an **initial value for every field** via `Field::Update(const FieldType&)` **before** calling
  `OfferService()`; otherwise offering the service fails.
- Because a field always carries a current value, a consumer that subscribes to a field is guaranteed to receive that
  current value **right after the subscription switched to `kSubscribed`** &ndash; even if the provider never updates the
  field afterwards. This is the essential difference to an event, and it is why the field-receive handlers in this
  chapter fire once directly after subscription (delivering the initial value) and again on every provider update.
