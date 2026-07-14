# Chapter 11: Direct use of an InstanceIdentifier

In all previous chapters we always used an **`InstanceSpecifier`** to address a service instance &ndash; be it for
finding a service (`FindService`/`StartFindService`) or for creating a skeleton. In this chapter we look at the
**`InstanceIdentifier`** and a special &ndash; sometimes called *power-user* &ndash; use case where we use it
**directly**. Along the way we build a small asynchronous *queue-and-callback* communication pattern.

### InstanceSpecifier vs. InstanceIdentifier

As already explained in [chapter 1](../chapter_1/README.md), an `InstanceSpecifier` is basically a string that serves as
a **lookup key** into the `score::mw::com` configuration. It points at a concrete service instance in the
`serviceInstances` array &ndash; namely the one whose `instanceSpecifier` property matches. The configuration stored
behind that key (the service-type deployment plus the instance deployment) makes up the **`InstanceIdentifier`**.

There are exactly three usages of an `InstanceSpecifier` in the public `score::mw::com` API:

- **Service instance search** (`<ProxyClass>::FindService` or `<ProxyClass>::StartFindService`)
- **Skeleton creation**
- **`score::mw::com::runtime::ResolveInstanceIDs`**

The first two are the common ones we have been using since chapter 1. The third one is very specific and is the subject
of this chapter. Internally, the search and skeleton-creation APIs resolve the `InstanceSpecifier` into an
`InstanceIdentifier` (containing all configuration aspects) and then dispatch to overloads that directly take an
`InstanceIdentifier`. In other words: the `InstanceSpecifier` overloads are just *short-cuts* that first do the
`InstanceSpecifier` &rarr; `InstanceIdentifier` resolution and then call the `InstanceIdentifier` overload.

> Note: Surprisingly, a service instance in the configuration has an **array**-type property `instances`. This is also
> the reason why `ResolveInstanceIDs` returns a *collection* of `InstanceIdentifier`s. In the current state of
> `score::mw::com` we do **not** make use of this array semantics: the `instances` array always contains exactly one
> element. The idea behind allowing potentially multiple "technical" instances behind one "abstract" service instance is
> called **multi-binding** (e.g. providing the same service both over a local IPC binding like LoLa/SHM *and* over a
> network binding). For this tutorial we simply ignore the multi-binding case and always expect exactly one
> `InstanceIdentifier`.

### Why would you ever use the InstanceIdentifier overloads directly?

The short answer: **if the `InstanceIdentifier` does not exist in the configuration of your `score::mw::com`
application!** If it *is* in your configuration, you would simply use its `InstanceSpecifier` &ndash; that is more
convenient. But an `InstanceIdentifier` is **serializable**: it can be turned into a string (via `ToString()`), sent to
some recipient (e.g. another process), and there be turned back into an `InstanceIdentifier` (via
`InstanceIdentifier::Create()`) &ndash; **without that recipient having the instance in its own configuration**. This is
exactly what we exploit here.

### The use case: turning a long-running synchronous method into an asynchronous queue-and-callback

Imagine a service that offers a very long-running / work-intensive method. In the current state of `score::mw::com`, a
method call on a `ProxyMethod` is **blocking** &ndash; so calling such a method would block the consumer for a long
time. The consumer could work around this by dispatching the call to a separate thread, or wait for a future
`score::mw::com` extension offering an asynchronous `ProxyMethod` call API. But there is a third option that involves a
**redesign of the service interface**:

The (long running) method is transformed into a (short running) method whose only job is to **queue** the task for
execution (a semantic change!). When this redesigned method returns, it only signals that the task has been *queued* at
the provider side. The actual result (or just the notification that the operation finished) is delivered back to the
caller later, in the form of a **callback**.

For this to work:

- The consumer/caller itself has to **provide** a service instance implementing a service interface that defines this
  callback method.
- When the provider has finished the queued operation for a specific consumer, it needs to call the callback method on
  **exactly the service instance provided by that consumer**.
- Therefore the provider needs some *information*, within the initial (queueing) method call, that lets it find the
  right instance of this callback service.

That last bullet is the actual challenge. In reality the provider could have many different consumers, each providing
its **own** instance of the callback service. So the provider cannot just do a "find-any" search for the callback
service &ndash; it would get many handles back and would not know which one to use. One *could* solve this with
additional complexity (each consumer hands over a unique id, the callback interface gets an extra `GetId()` method, the
provider does a find-any search, creates a proxy for each hit, calls `GetId()` on each until it finds the matching one,
and releases the rest). That is doable, but complex and inefficient.

What the provider really wants is a search for **one specific instance** &ndash; and the direct use of an
`InstanceIdentifier` gives exactly that. Here is how we solve it:

1. The queueing method gets an **additional parameter** (a fixed-string) representing the **serialized
   `InstanceIdentifier`** of the consumer's callback service instance.
2. Before calling the provider's queueing method, the consumer instantiates a **skeleton** for the callback interface
   and calls `OfferService()` on it.
3. The consumer then calls the provider's queueing method, setting the fixed-string to the serialized form of the
   `InstanceIdentifier` of the skeleton it created in step 2. It obtains that serialized form by calling
   `score::mw::com::runtime::ResolveInstanceIDs` with the `InstanceSpecifier` used in step 2 (which returns exactly one
   `InstanceIdentifier`) and then calling `ToString()` on it.
4. The provider, when receiving the queueing call, also gets the string representation of the callback
   `InstanceIdentifier`. It creates an `InstanceIdentifier` from it (via `InstanceIdentifier::Create()`) and does a
   `FindService` based on this `InstanceIdentifier`.
5. This returns exactly one handle, from which the provider creates the matching callback proxy instance.
6. When the queued task finishes, the provider calls the callback method on that callback service instance to notify the
   consumer that the asynchronous job has finished.

### The service interfaces

We define two service interfaces in `long_running_service.h`.

The provider provides a `LongRunningServiceInterface`. It contains a single method `PostLongRunningJob` with three
in-arguments and a `bool` result (reflecting whether the job could be queued):

1. a `std::uint32_t` &ndash; a dummy value standing in for a hypothetical job argument,
2. a `std::uint32_t` &ndash; the *job number*; the caller hands this over and expects to get it back in the
   `SetJobResult` method of the callback interface,
3. a fixed-string large enough to hold an `InstanceIdentifier::ToString()` representation.

The callback service is `LongRunningServiceResultInterface`. It contains a single method `SetJobResult` with two
in-arguments and **no** result (a `void` method):

1. a `std::uint32_t` &ndash; a dummy value representing the *job result*,
2. a `std::uint32_t` &ndash; the *job number* the result belongs to (it correlates with the job number of the
   `PostLongRunningJob` call).

The fixed-string type used to transport the serialized `InstanceIdentifier` is a plain `std::array<char, N>` (method
arguments are transported through shared memory and must be trivially copyable). We choose a generous capacity; the
consumer asserts at runtime that the serialized form actually fits:

```cpp
using SerializedInstanceIdentifier = std::array<char, 2048>;

static_assert(std::is_trivially_copyable_v<SerializedInstanceIdentifier>,
              "SerializedInstanceIdentifier must be trivially copyable to be usable as a method argument.");
```

The `LongRunningServiceInterface`:

```cpp
template <typename Trait>
class LongRunningServiceInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    typename Trait::template Method<bool(std::uint32_t, std::uint32_t, SerializedInstanceIdentifier)>
        post_long_running_job_{*this, "PostLongRunningJob"};
};
```

The `LongRunningServiceResultInterface`:

```cpp
template <typename Trait>
class LongRunningServiceResultInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    typename Trait::template Method<void(std::uint32_t, std::uint32_t)> set_job_result_{*this, "SetJobResult"};
};
```

And the proxy/skeleton aliases for both interfaces:

```cpp
using LongRunningServiceProxy = score::mw::com::AsProxy<LongRunningServiceInterface>;
using LongRunningServiceSkeleton = score::mw::com::AsSkeleton<LongRunningServiceInterface>;
using LongRunningServiceResultProxy = score::mw::com::AsProxy<LongRunningServiceResultInterface>;
using LongRunningServiceResultSkeleton = score::mw::com::AsSkeleton<LongRunningServiceResultInterface>;
```

### Files/artifacts used

The `bazel` project for this chapter is located in `score/mw/com/doc/tutorial/chapter_11` and contains the following
files:

----------------------------------------

| File Name                       | Description                                                                          |
|---------------------------------|--------------------------------------------------------------------------------------|
| `BUILD`                         | This file contains bazel targets for this example.                                   |
| consumer/`consumer.cpp`         | Implementation of the service consumer app. The `main()` for the consumer.           |
| consumer/`consumer.h`           | Header (empty - we just always want to have cpp/h pairs) of the service consumer.    |
| consumer/`mw_com_config.json`   | This file contains the configuration for `score::mw::com` for the consumer app.      |
| consumer/`logging.json`         | This file contains the configuration for the logging system used by `score::mw::com` |
| provider/`provider.cpp`         | Implementation of the service provider.                                              |
| provider/`provider.h`           | Header (empty) of the service provider.                                              |
| provider/`mw_com_config.json`   | This file contains the configuration for `score::mw::com` for the provider app.      |
| provider/`logging.json`         | This file contains the configuration for the logging system used by `score::mw::com` |
| `long_running_service.cpp`      | This file is empty as the service interfaces are completely defined in the header.   |
| `long_running_service.h`        | This file contains the definition of the two service interfaces.                     |

----------------------------------------

### Configuration

The **provider** configuration declares *only* the `LongRunningService` &ndash; the service type and the single instance
`MyLongRunningServiceInstance` that it provides. Note that it does **not** declare the callback service
(`LongRunningServiceResult`) at all &ndash; that is the whole point of this chapter: the provider will address the
callback instance purely via the `InstanceIdentifier` it receives at runtime.

```json
{
  "serviceTypes": [
    {
      "serviceTypeName": "/score/mw/com/tutorial/LongRunningService",
      "version": { "major": 1, "minor": 0 },
      "bindings": [
        {
          "binding": "SHM",
          "serviceId": 8715,
          "methods": [ { "methodName": "PostLongRunningJob", "methodId": 1 } ]
        }
      ]
    }
  ],
  "serviceInstances": [
    {
      "instanceSpecifier": "MyLongRunningServiceInstance",
      "serviceTypeName": "/score/mw/com/tutorial/LongRunningService",
      "version": { "major": 1, "minor": 0 },
      "instances": [
        {
          "instanceId": 1,
          "asil-level": "QM",
          "binding": "SHM",
          "methods": [ { "methodName": "PostLongRunningJob", "queueSize": 1 } ]
        }
      ]
    }
  ],
  "global": { "asil-level": "QM", "applicationID": 50 }
}
```

The **consumer** configuration declares *both* services: the `LongRunningServiceResult` (the callback service &ndash;
service type plus the instance `MyLongRunningServiceResultInstance` it provides/offers) **and** the `LongRunningService`
(service type plus the instance `MyLongRunningServiceInstance` it consumes; because a method is called on the proxy side,
the method is listed with a `queueSize`). The `serviceId`s and `methodId`s of the `LongRunningService` must match between
provider and consumer.

Provider and consumer are two separate applications, so they get different `applicationID`s (`50` and `51`).

### Provider application

Like the earlier provider examples, the provider relies on the runtime's **implicit, lazy initialization** &ndash; there
is no explicit `InitializeRuntime` call. It registers the `PostLongRunningJob` handler and offers the service. The
handler receives the `bool` return value
as its first out-parameter (by reference), followed by the three input arguments by const-reference. Crucially, it does
**not** run the job synchronously: it only makes sure the callback `InstanceIdentifier` is deserialized/cached and then
arms a timer. The `FindService`/proxy-creation and the actual callback call happen **later, on the timer thread**:

```cpp
const auto handler_result = service_instance.post_long_running_job_.RegisterHandler(
    [](bool& queued,
       const std::uint32_t& job_argument,
       const std::uint32_t& job_number,
       const SerializedInstanceIdentifier& serialized_callback_id) -> void {
        const std::string serialized_instance_id = ToSerializedString(serialized_callback_id);
        // ...
        if (!EnsureCallbackIdentifier(serialized_instance_id))
        {
            queued = false;
            return;
        }

        std::thread{[serialized_instance_id, job_number]() {
            // sleep 1..10s, then:
            auto callback_proxy = GetOrCreateCallbackProxy(serialized_instance_id);
            // ...
            const auto call_result = callback_proxy->set_job_result_(job_result, job_number);
        }}.detach();

        queued = true;
    });
```

> Note: `InstanceIdentifier::Create()` (used further below) only works once the runtime configuration has been
> **locked**, i.e. after the runtime singleton has been constructed. Here this is always the case by the time the handler
> runs, because creating the skeleton and calling `OfferService()` above already triggered the (implicit) runtime
> initialization.

> Note: We deliberately do the `FindService`/proxy creation on the **timer thread**, not inside the method handler. The
> method handler runs on an internal middleware thread; performing the callback proxy setup (which itself needs
> message-passing round-trips to the consumer) from within that handler does not work reliably. Doing it on a separate
> thread &ndash; after the handler has already returned `queued = true` to the consumer &ndash; keeps the request path
> short and the setup robust.

The **direct `InstanceIdentifier` use** is concentrated in two helpers. `EnsureCallbackIdentifier` deserializes the
callback `InstanceIdentifier` exactly **once** per distinct serialized form (calling `InstanceIdentifier::Create()` more
than once for the same service-type deployment would terminate the process, because deserialization registers that
deployment in the runtime configuration):

```cpp
bool EnsureCallbackIdentifier(const std::string& serialized_instance_id)
{
    std::lock_guard<std::mutex> lock{g_callback_cache_mutex};
    if (g_callback_identifiers.find(serialized_instance_id) != g_callback_identifiers.end())
    {
        return true;
    }

    auto instance_identifier_result = score::mw::com::InstanceIdentifier::Create(std::string{serialized_instance_id});
    if (!instance_identifier_result.has_value())
    {
        return false;
    }
    g_callback_identifiers.emplace(serialized_instance_id, std::move(instance_identifier_result).value());
    return true;
}
```

`GetOrCreateCallbackProxy` then uses the (already deserialized and cached) `InstanceIdentifier` to do a **targeted**
`FindService` and creates/caches the proxy. Because the `InstanceIdentifier` fully describes the callback instance, this
returns exactly the one matching handle &ndash; no ambiguous "find-any" search is needed. The proxy is cached and reused
for all subsequent jobs of the same consumer:

```cpp
std::shared_ptr<LongRunningServiceResultProxy> GetOrCreateCallbackProxy(const std::string& serialized_instance_id)
{
    std::lock_guard<std::mutex> lock{g_callback_cache_mutex};

    const auto cached = g_callback_proxies.find(serialized_instance_id);
    if (cached != g_callback_proxies.end())
    {
        return cached->second;
    }

    const auto identifier = g_callback_identifiers.find(serialized_instance_id);
    // ...
    auto find_result = LongRunningServiceResultProxy::FindService(score::mw::com::InstanceIdentifier{identifier->second});
    // ...
    auto proxy_result = LongRunningServiceResultProxy::Create(find_result.value().front());
    // ...
    auto proxy = std::make_shared<LongRunningServiceResultProxy>(std::move(proxy_result).value());
    g_callback_proxies.emplace(serialized_instance_id, proxy);
    return proxy;
}
```

Like in the earlier provider examples, `main()` has no while-loop: after offering the service it simply blocks on a
condition variable until a termination signal requests the shut down.

### Consumer application

The consumer first creates, configures and offers its **own** callback service instance
(`LongRunningServiceResultInstance`). It registers the `SetJobResult` handler (a `void` method, so the handler only
receives the two input arguments and just logs them) *before* offering the service:

```cpp
auto result_skeleton = std::move(result_skeleton_result).value();

const auto set_result_handler_result = result_skeleton.set_job_result_.RegisterHandler(
    [](const std::uint32_t& job_result, const std::uint32_t& job_number) -> void {
        std::cout << "Callback SetJobResult received: job_number=" << job_number << " has result=" << job_result
                  << "." << std::endl;
    });
// ...
auto result_offer_result = result_skeleton.OfferService();
```

It then resolves the `InstanceIdentifier` of that callback instance via `ResolveInstanceIDs`, expects exactly one
result, and serializes it (via `ToString()`) into the fixed-size buffer it will pass in every `PostLongRunningJob` call:

```cpp
auto resolve_result = score::mw::com::runtime::ResolveInstanceIDs(result_specifier.value());
// ... assert has_value() and size() == 1 ...
const std::string_view serialized_callback_id_view = resolve_result.value().front().ToString();
const SerializedInstanceIdentifier serialized_callback_id =
    SerializeCallbackInstanceIdentifier(serialized_callback_id_view);
```

Next it does a `StartFindService`-based search for the `LongRunningService` instance provided by our provider and, once
found, creates a proxy for it:

```cpp
auto find_service_handle_result =
    LongRunningServiceProxy::StartFindService(std::move(find_service_handler), service_specifier.value());
```

Finally it **cyclically** (every 500 ms) calls `PostLongRunningJob` on the proxy &ndash; setting the dummy argument to a
random number and the job number to a value starting at `0` and incremented for each call &ndash; until it receives a
kill/shutdown signal. The results are delivered asynchronously through the `SetJobResult` callback handler registered
above:

```cpp
auto call_result = proxy->post_long_running_job_(job_argument, job_number, serialized_callback_id);
// ... log the returned "queued" bool ...
++job_number;
```

### How to run the example

First build the two applications:

```bash
# Build the provider and consumer targets
bazel build //score/mw/com/doc/tutorial/chapter_11:provider-tar
bazel build //score/mw/com/doc/tutorial/chapter_11:consumer-tar
```

Extract both archives (e.g. in a tmp-directory):

```bash
mkdir -p /tmp/tutorial/chapter_11
tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_11/provider-tar.tar -C /tmp/tutorial/chapter_11/
tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_11/consumer-tar.tar -C /tmp/tutorial/chapter_11/
```

Start the service-provider application in the 1st terminal:

```bash
cd /tmp/tutorial/chapter_11/opt/LongRunningServer
bin/provider
```

... and the service-consumer in the 2nd terminal:

```bash
cd /tmp/tutorial/chapter_11/opt/LongRunningClient
bin/consumer
```

You will observe the consumer posting jobs every 500 ms (each getting `queued at provider: true`), and &ndash; after a
random 1-10 s delay per job &ndash; the corresponding `SetJobResult` callbacks arriving asynchronously, each carrying the
correct `job_number` back:

```text
Posting long running job_number=0 (job_argument=2586070691).
  PostLongRunningJob(job_number=0) queued at provider: true
...
Callback SetJobResult received: job_number=5 has result=1986113591.
Callback SetJobResult received: job_number=1 has result=1559372926.
...
```

Note how the callbacks do **not** arrive in job-number order &ndash; each job's timer has an independent random delay, so
the results come back in a different order than the jobs were posted. This nicely illustrates the asynchronous
queue-and-callback nature of the pattern.

> Note: For simplicity the provider uses one detached timer thread per job that may still be pending when the process is
> asked to shut down; the process just exits in that case. This is an acceptable tutorial simplification &ndash; a
> production implementation would manage those timers explicitly.

### Summary

A short summary of what we have learned in this chapter:

- An **`InstanceSpecifier`** is a lookup key into the configuration; the configuration behind it makes up an
  **`InstanceIdentifier`**. The `FindService`/`StartFindService` and skeleton-creation APIs internally resolve the
  `InstanceSpecifier` to an `InstanceIdentifier` and provide overloads that take the `InstanceIdentifier` directly.
- `score::mw::com::runtime::ResolveInstanceIDs` performs this resolution explicitly and returns a *collection* of
  `InstanceIdentifier`s (the `instances` array; always exactly one element in the current state &ndash; the
  multi-binding case is not used here).
- The direct-`InstanceIdentifier` overloads are useful when the `InstanceIdentifier` is **not in your own
  configuration**. An `InstanceIdentifier` is **serializable** (`ToString()`) and can be reconstructed elsewhere via
  `InstanceIdentifier::Create()` &ndash; even in an application that does not have that instance in its configuration.
- We used this to implement an asynchronous **queue-and-callback** pattern: the consumer offers a callback service
  instance, serializes its `InstanceIdentifier`, and passes it in the queueing method call; the provider deserializes
  it, does a **targeted** `FindService`, caches the resulting callback proxy, and later delivers the result via the
  callback method &ndash; addressing exactly the one callback instance that belongs to that consumer.
- Practical caveats: call `InstanceIdentifier::Create()` **once** per distinct identifier (cache the result), do the
  `FindService`/proxy setup **outside** the method-handler thread, and remember that `InstanceIdentifier::Create()` only
  works after the runtime configuration has been **locked** (which the implicit runtime initialization &ndash; triggered
  by skeleton creation / `OfferService` &ndash; already takes care of here).
