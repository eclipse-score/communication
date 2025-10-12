# Non-allocating future

In multithreaded environment, it often happens that one thread needs to wait for a result from another thread.
This is often solved by the Future design pattern, for which the C++ standard library provides `std::promise`
and `std::future` constructs. Unfortunately, their implementations tend to allocate OS resources and potentially heap
memory on every use and poorly suited for the environments in which system resource allocation outside the startup stage
is frowned upon. The `std::future` also needs to support the exception-propagation functionality, which we would like
to avoid.

The `NonAllocatingFuture` class template introduces a simple future-like object that can be used with an external
lock-like and an external signal-like object. `std::mutex` and `std::condition_variable` are the main targets for such
objects, but classes with the same interfaces and semantics can also be used. These objects are supposed to be created
only once and then reused for multiple `NonAllocatingFuture` objects sequentially and/or concurrently. Sharing the same
pair of objects for concurrent use in multiple futures is a supported use case, but will internally lead to spurious
wake-ups, which may affect the system performance and is only recommended for signals that don't happen too often.

The generic version of `NonAllocatingFuture` constructor accepts a reference to the value object. This value object
(as well as the lock-like and signal-like objects, of course), shall outlive the lifetime of the `NonAllocatingFuture`
object bound to it. If the user operations with the value objects (including copy or move assignment operators called
inside a user-called `UpdateValueMarkReady()`) do not involve system resource allocation, the use of
`NonAllocatingFuture` does not involve system resource allocation.

There is also the partial specialization of `NonAllocatingFuture` template for `void` value object, which is used only
to signal about the fact of some event happening and doesn't require an external value object.

The `NonAllocatingFuture` object serves the roles of both the Future object (via the methods `Wait()` and `GetValue()`)
and the Promise object (via the methods `UpdateValueMarkReady()`/`MarkReady()`), which means its lifetime
should encompass both uses. This is supported by its primary use case scenario, when the `NonAllocatingFuture` object
is created on the stack of the thread that will be waiting for the result, some other mechanism (a command queue, for
example) passes the reference to this object to another thread that is supposed to provide the result, then that other
thread is required to call `UpdateValueMarkReady()` or `MarkReady()` method exactly once and to stop accessing this
object by the reference it holds after this method returns, while the waiting thread is required to use the `Wait()`
method before destructing the object. Failure to follow this protocol in full may lead to UB or a deadlock. In
particular, for a command queue it means that each command in the queue that was supposed to signal a future must do so
no matter what was the reason for the removal of the command from the queue. In such cases, the value object type may
need to support a state that would indicate an "abnormal return" condition.

Other scenarios may also make sense, but care needs to be taken to develop a protocol that would ensure avoidance of UB
and deadlocks.

On the Promise side, it is not necessary to use assignment operator to modify the value object. A reference to the
object can be obtained with the `GetValueForUpdate()` method. For example, the following sequence can be used to update
a value object that supports the `push_back()` method and then mark it as ready:

```cpp
    future.GetValueForUpdate().push_back(something);
    future.MarkReady();
```

`NonAllocatingFuture` intentionally does not support `score::cpp::stop_token` because its current primary user,
message_passing, also does not support stop token use. Stop token support, if needed, can be manually implemented by the
user; care shall be taken to avoid hanging references and deadlocks.

The examples of usage of `NonAllocatingFuture` interface methods can be found in `non_allocating_future_test.cpp`.
