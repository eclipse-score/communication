# Chapter 8: Methods - request/response communication

All previous chapters dealt with *data-centric* communication: the provider **pushes** data (events, fields) to the
consumers, which react to it. This chapter introduces the other communication style `score::mw::com` offers:
**methods** &ndash; a classic *request/response* interaction. Here the **consumer** actively **invokes an operation** on
the provider and (optionally) receives a return value. Think of it as a remote procedure call (RPC)
(in the case of our LoLa binding IN-args and Result are exchanged over shared memory).

To show this we introduce a new service interface, the `CalculatorService`, which offers two methods:

- `add` &ndash; adds two 32-bit integers and returns their 64-bit sum. Its arguments are cheap scalars, so we call it
  using the convenient **copy** call operator.
- `arithmetic_mean` &ndash; computes the arithmetic mean of a (potentially large) array of integers. Its argument is
  deliberately large, so we call it using the **zero-copy** call operator (allocate the argument in place, fill it,
  then invoke), avoiding an unnecessary copy of the array.

### The service interface

Instead of `hello_world_service.h` / `tire_pressure_service.h` this chapter introduces `calculator_service.h` / `.cpp`.
A method is declared as `Trait::template Method<ReturnType(ArgTypes...)>` &ndash; i.e. simply by its function
signature. The member is constructed with a reference to the enclosing interface (`*this`) and the method name:

```cpp
template <typename Trait>
class CalculatorInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    typename Trait::template Method<std::uint64_t(std::uint32_t, std::uint32_t)> add_{*this, "add"};
    typename Trait::template Method<std::uint32_t(IntegerArray)> arithmetic_mean_{*this, "arithmetic_mean"};
};
```

The second method takes a custom type `IntegerArray` as its argument:

```cpp
class IntegerArray
{
  public:
    static constexpr std::size_t max_elements = 1024U;

    IntegerArray() = default;
    explicit IntegerArray(std::size_t number_of_elements);

    void SetNumberOfElements(std::size_t number_of_elements);
    std::size_t GetNumberOfElements() const;

    std::array<std::uint32_t, max_elements> internal_array_{};

  private:
    std::size_t number_of_elements_{0U};
};
```

A few important properties of a method argument/return type &ndash; and how they shaped `IntegerArray`:

- **It must be trivially copyable.** Method arguments and return values are transported through shared memory, where the
  middleware (de)serializes them via a plain `memcpy`. Therefore a method data type must not own heap memory
  (no `std::vector`, `std::string`, ...) and must not have a user-defined copy constructor/destructor. `IntegerArray`
  fulfils this by aggregating only a `std::array` and a `std::size_t`. We even guard this with a
  `static_assert(std::is_trivially_copyable_v<IntegerArray>)`.
- **It must be default-constructible.** The zero-copy call path (`Allocate()`, see below) default-constructs the
  argument inside the shared-memory slot before handing a pointer to it back to the caller. Because we also declared the
  `explicit IntegerArray(std::size_t)` constructor, the implicit default constructor is suppressed, so we bring it back
  explicitly via `IntegerArray() = default;`.
- **Fixed capacity.** As the argument lives in a fixed-size shared-memory slot, the array has a fixed maximum capacity
  (`max_elements`). How many of the leading elements actually carry a meaningful value is tracked separately in
  `number_of_elements_`, so a caller can transport "up to `max_elements`" integers while the receiver only processes the
  first `number_of_elements_` of them.

### Files/artifacts used

The `bazel` project for this chapter is located in `score/mw/com/doc/tutorial/chapter_8`. It follows the same structure
as the previous chapters, with the service interface renamed to `calculator_service`:

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
| `calculator_service.cpp`        | This file is empty as the service interface is completely defined in the header.     |
| `calculator_service.h`          | This file contains the definition of the service interface (two methods).            |

----------------------------------------

### Configuration

Methods are configured similarly to events/fields, but under a `methods` array. In the **service type** binding each
method gets a `methodId`:

```json
"bindings": [
  {
    "binding": "SHM",
    "serviceId": 8713,
    "methods": [
      { "methodName": "add", "methodId": 1 },
      { "methodName": "arithmetic_mean", "methodId": 2 }
    ]
  }
]
```

In the **service instance** each method gets a `queueSize`, which sizes the request/response queue of that method:

```json
"instances": [
  {
    "instanceId": 1,
    "asil-level": "QM",
    "binding": "SHM",
    "methods": [
      { "methodName": "add", "queueSize": 1 },
      { "methodName": "arithmetic_mean", "queueSize": 1 }
    ]
  }
]
```

> Note: The current method implementation does not yet support a method call queue size greater than 1, so `queueSize`
> is set to `1` here.

> Note: In contrast to a field (where the consumer instance does **not** need to repeat the field deployment), a method
> **must** be listed in the `methods` array of the **consumer's** instance as well. Otherwise the proxy fails to set up
> the method binding (`Provided a method name which can not be found in LolaServiceInstanceDeployment`).
> This is because in the method case, the consumer side sets up the shared memory for the method related request/response
> queue.

> Note: Each method deployment in the **consumer's** instance additionally supports an optional boolean `use` property
> (default `true`). It is purely a matter of **resource tweaking** and is only evaluated on the proxy/consumer side. If
> you *know* that a particular consumer will never call a given method, set `use` to `false` for that method &ndash; the
> proxy then skips setting up the resources for it, which for the LoLa binding means it does **not** allocate the
> shared-memory of that method's request/response queue. Leaving it at the default `true` keeps the method callable. This
> is the exact same idea &ndash; with the same default polarity &ndash; behind the `useGetIfAvailable` /
> `useSetIfAvailable` properties of a field getter/setter (see chapter 9): all three default to `true`, i.e. the
> getter/setter (just like a method) is enabled by default and you *opt out* of it via `useGetIfAvailable: false` /
> `useSetIfAvailable: false` when you *know* that a particular consumer will never call the field's getter/setter.

### Provider application

The provider creates a skeleton of type `AsSkeleton<CalculatorInterface>`, **registers a handler for every method** and
then offers the service. A handler is registered via `Method::RegisterHandler()`. With the current method API a handler
does **not** *return* the result. Instead, the return value is passed to the handler as its **first argument by
(mutable) reference** &ndash; i.e. as an *out-parameter* &ndash; followed by the (already deserialized) input arguments
by const-reference. The handler writes the result into that out-parameter and returns `void`. For a method signature
`ReturnType(ArgTypes...)` the expected handler signature therefore is `void(ReturnType&, const ArgTypes&...)`. Handlers
**must** be registered *before* the service is offered.

```cpp
service_instance.add_.RegisterHandler(
    [](std::uint64_t& result, const std::uint32_t& lhs, const std::uint32_t& rhs) -> void {
        result = static_cast<std::uint64_t>(lhs) + static_cast<std::uint64_t>(rhs);
    });

service_instance.arithmetic_mean_.RegisterHandler(
    [](std::uint32_t& result, const score::mw::com::tutorial::IntegerArray& array) -> void {
        const std::size_t count = array.GetNumberOfElements();
        std::uint64_t sum{0U};
        for (std::size_t index = 0U; index < count; ++index)
        {
            sum += array.internal_array_[index];
        }
        result = (count == 0U) ? 0U : static_cast<std::uint32_t>(sum / count);
    });

auto offer_result = service_instance.OfferService();
```

Note that the `arithmetic_mean` handler receives the large `IntegerArray` **by const-reference** &ndash; no copy of the
array happens on the provider side. A method provider is purely reactive: the registered handlers are invoked by the
middleware (on an internal thread) whenever a consumer calls a method. There is nothing to poll in `main()`, so &ndash;
just like the event-driven consumer of chapter 5 &ndash; the main thread simply blocks on a condition variable until a
termination signal (`SIGINT`/`SIGTERM`) requests the shut down; the signal handler sets a flag and notifies the
condition variable.

### Consumer application

The consumer finds the single service instance via `StartFindService()` (exactly the same pattern as in chapter 5) and,
once it has created the proxy, performs the two method calls. It stops the discovery from within the find-service
handler, as a single proxy is all it needs.

#### 1. Calling `add` with the copy call operator

For small, cheap-to-copy arguments the most convenient variant is the call operator that takes the arguments **by
const-reference**. The middleware allocates the argument slot, copies the arguments into it and performs the call:

```cpp
auto call_result = proxy.add_(lhs, rhs);   // operator()(const ArgTypes&... args)
if (!call_result.has_value())
{
    // handle error ...
}
// The result is returned as a MethodReturnTypePtr; dereference it to access the actual value.
std::uint64_t sum = *(call_result.value());
```

#### 2. Calling `arithmetic_mean` with the zero-copy call operator

The `arithmetic_mean` method was designed specifically to carry a **potentially large argument**, where avoiding
unnecessary copies matters. For this we use the second call-operator overload, which takes `MethodInArgPtr<ArgTypes>...`
&ndash; i.e. pointers to the arguments *already sitting in the shared-memory slot*. The workflow is:

1. Call `Allocate()` on the method. On success it returns a tuple of `MethodInArgPtr`, one per argument, each pointing
   right into the shared-memory slot the argument will be transported in.
2. Fill the argument **in place** through that pointer &ndash; no copy of the large array is made.
3. Invoke the method with the (moved-out) argument pointer(s).

```cpp
// Step 1: allocate the argument storage inside the shared-memory slot.
auto allocate_result = proxy.arithmetic_mean_.Allocate();
if (!allocate_result.has_value())
{
    // handle error ...
}
auto& [array_ptr] = allocate_result.value();

// Step 2: fill the array in place (through the pointer into the shared-memory slot - no copy).
for (std::size_t index = 0U; index < 500U; ++index)
{
    array_ptr->internal_array_[index] = random_value();
}
array_ptr->SetNumberOfElements(500U);

// Step 3: invoke the method with the filled-in argument pointer.
auto call_result = proxy.arithmetic_mean_(std::move(array_ptr));   // operator()(MethodInArgPtr<ArgTypes>... args)
std::uint32_t mean = *(call_result.value());
```

The copy call operator (variant 1) is, under the hood, just a convenience wrapper: it internally calls `Allocate()`,
copies the passed arguments into the allocated storage and then forwards to the zero-copy operator (variant 2). For a
large argument this "extra copy into the allocated storage" is exactly what we want to avoid &ndash; hence the explicit
`Allocate()`/fill/call sequence.

### How to run the example

The steps are the same as in the previous chapters. The consumer uses the **implicit** configuration loading, so it is
started **without** the `--service_instance_manifest` argument.

```bash
# Build the provider and consumer targets
bazel build //score/mw/com/doc/tutorial/chapter_8:provider-tar
bazel build //score/mw/com/doc/tutorial/chapter_8:consumer-tar
```

Extract both archives (e.g. in a tmp-directory):

```bash
mkdir -p /tmp/tutorial/chapter_8
tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_8/provider-tar.tar -C /tmp/tutorial/chapter_8/
tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_8/consumer-tar.tar -C /tmp/tutorial/chapter_8/
```

... then start the service-provider application in the 1st terminal:

```bash
cd /tmp/tutorial/chapter_8/opt/CalculatorServer
bin/provider
```

and the service-consumer in the 2nd terminal:

```bash
cd /tmp/tutorial/chapter_8/opt/CalculatorClient
bin/consumer
```

The consumer finds the service, creates the proxy, performs both method calls and then terminates on its own:

```
Found Calculator service instance (instance id: { ... "instanceId": 1 ... }). Creating proxy.
Calling add(3438683060, 3685456342) using the copy call operator ...
add returned: 7124139402
Calling arithmetic_mean(...) using the zero-copy call operator ...
arithmetic_mean over 500 elements returned: 52
All method calls completed. Shutting down.
Calculator service consumer going down.
```

... while the provider logs the incoming calls (it keeps running until it receives `SIGINT`/`SIGTERM`):

```
OfferService: Calculator service instance is now offered. Waiting for method calls ...
add(3438683060, 3685456342) = 7124139402
arithmetic_mean over 500 elements = 52
```

Note how `add` correctly returns the 64-bit sum `7124139402`, which does not fit into a 32-bit integer &ndash; this is
why the method was declared to return `std::uint64_t` even though its arguments are `std::uint32_t`.

### Summary

A short summary of what we have learned in this chapter:

- Besides the *data-centric* communication of the previous chapters (events/fields, where the **provider pushes** data),
  `score::mw::com` also offers **methods** &ndash; a classic *request/response* interaction where the **consumer
  invokes** an operation on the provider and (optionally) receives a return value (think of it as an RPC over shared
  memory).
- A method is declared within the service-interface by its function signature via
  `Trait::template Method<ReturnType(ArgTypes...)>`.
- A method argument/return type must be **trivially copyable** (it is transported through shared memory via `memcpy`, so
  no heap-owning members, no user-defined copy constructor/destructor) and **default-constructible** (the zero-copy
  `Allocate()` path default-constructs it in the shared-memory slot).
- On the **provider** side the implementation of the method is supplied via `Method::RegisterHandler()`, **before**
  `OfferService()`.
  The handler does **not** return the result to avoid copying; instead the return value is passed as its
  **first argument by (mutable) reference** (an out-parameter), followed by the input arguments by const-reference &ndash;
  i.e. the expected handler signature for `ReturnType(ArgTypes...)` is `void(ReturnType&, const ArgTypes&...)`.
- On the **consumer** side a method member is callable via `operator()`. There are two variants:
  - the **copy** call operator `proxy.method(args...)` &ndash; convenient for small, cheap-to-copy arguments (it
    internally `Allocate()`s, copies the arguments in and calls);
  - the **zero-copy** call operator `proxy.method(MethodInArgPtr...)` &ndash; for (potentially large) arguments: first
    `Allocate()` the argument directly in the shared-memory slot, fill it **in place**, then invoke the method with the
    obtained pointer(s), avoiding an extra copy.
- A method call returns a `score::Result`; on success it wraps a `MethodReturnTypePtr<ReturnType>` which is dereferenced
  to access the actual return value.
- Methods are configured under a `methods` array: `methodId` per method in the **service type** binding and `queueSize`
  per method in the **service instance**. Unlike a field/event, a method **must** also be fully declared in the `methods`
  array of the **consumer's** configuration instance, because in the case of methods resources (`queueSize`) are
  allocated on the consumer side.
- The current method implementation does not yet support a method call `queueSize` greater than `1`.
