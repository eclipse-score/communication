# Implementation-time decisions

The items below are intentionally left open and must be decided when the LoLa runtime is implemented.

---

## 1. Public `FieldPublisher::register_get_handler`

- **Concern.** C++ `SkeletonField` has no public user get handler. `RegisterGetHandler()` is private and answers with the latest value. A custom Rust handler can return a value that differs from what subscribers received through `update()`.
- **Also.** The get handler is sync (`Fn() -> T`) while the set handler is async. The two are asymmetric.
- **Decide when.** LoLa field producer implementation.

## 2. `subscribe(self)` versus the `get_x(&self)` and `set_x(&self)` wrappers

- **Concern.** `subscribe` takes the field consumer by value. The generated wrappers borrow the whole consumer, so they cannot be used once a field has been moved by `subscribe`. Only the raw `{name}_get` and `{name}_set` callers survive the partial move. The mixed example carries a `TODO: Ordering workaround`.
- **C++ reference.** `ProxyField::Subscribe()` does not consume the field.
- **Decide when.** LoLa field consumer implementation.

## 3. Set handler semantics

- **Concern.** The C++ handler is `void(FieldType&)`. It edits the value in place, runs under a per-field mutex, and the middleware then calls `Send()` and can return an error to the proxy. The Rust handler is `Fn(T) -> Fut<Output = T>`.
- **To decide.**
  - Error path so that a handler can reject a value. The handler result type becomes `Result<T>` and maps to `score::Result` and `MethodErrc`.
  - Serialization of set calls per field, as in C++ (one slot, no extra allocation). The runtime must keep this guarantee even if the engine later dispatches concurrently.
  - Document that the returned value is published to subscribers (the C++ `Send`).
  - Executor hook for the future. LoLa invokes handlers synchronously on the message passing thread, so a `block_on` fallback can deadlock if the future needs an executor on that thread. The signature of the `spawn(future)` hook is still TBD.
- **Decide when.** LoLa field producer and method runtime implementation.

## 4. Method handler return path

- **Concern.** C++ handler is `void(ReturnType&, const Args&...)` and writes straight into the reply buffer. The Rust handler returns the value from a `'static` future, which costs an extra copy on the producer side and prevents borrowing the arguments inside the async body.
- **Decide when.** LoLa method handler implementation.

## 5. Error handling for methods and fields

- **Application errors.** `#[repr(C)]` tagged result mapped to `score::Result` and `MethodErrc`.
- **Consumer side errors.** Provider stops offering, timeout, dropped pending call.
- **Related.** End-to-end protection affects the error model.
- **Decide when.** Together with items 3 and 4.

## 6. Zero-copy argument safety

- **Concern.** `assume_init()` on an unwritten argument slot is undefined behavior once real shared memory backs it.
- **Decide when.** LoLa method caller implementation, when argument pointers refer to shared memory.

## 7. Runtime behavior of unimplemented runtimes

- **Concern.** Every Field and Method type in the Mock and LoLa runtimes is `todo!()`. An interface containing a Field or Method panics at consumer creation with these runtimes.
- **Decide when.** Runtime implementation. Until then keep these interfaces out of the default example binary.
