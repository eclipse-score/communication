# Method communication in `LoLa`

## Introduction

Methods in `mw::com` are class templates parameterized with the `MethodReturnType` and the `MethodInArgType`s.
They differ between provider (skeleton) and consumer (proxy) side regarding their functionality.
`mw::com` proxies and skeletons contain members of the related template classes. The binding independent structural
model/class diagram is discussed in
[Skeleton/Proxy Binding architecture](../skeleton_proxy/README.md)

## Binding independent class template for Method on the proxy side

On the proxy side, the `mw::com::impl::ProxyMethod` class template provides the functionality to call remote methods on
the skeleton side:

### Template parameters

```cpp
template <typename ReturnType, typename... ArgTypes>
class ProxyMethod<ReturnType(ArgTypes...)> final
```
The method signature defines the template parameters. I.e. if you had a method with the signature

```cpp
int foo(double, std::array<char, 10>);
```
the related proxy method member would be of type

```cpp
mw::com::impl::ProxyMethod<int(double, std::array<char, 10>)>
```

Note, that the signature does not include `const` or reference qualifiers and just support data types, which are
generally supported by the `LoLa` communication framework. (see ScoreReq.AoU OnlyLoLaSupportedTypes).

### API

The `ProxyMethod` class template provides the following public API:

- An `Allocate()` method to allocate storage for the method in-args and the return value. This API is used for
  the zero-copy method call variant. It returns a `std::tuple<impl::MethodInArgPtr<ArgTypes>...>`. These
  `MethodInArgPtr` are then "filled" with the values of the in-args by the user, before the method is called.
  This API only exists if there is at least one in-arg (i.e. for methods with zero in-args, there is no
  `Allocate()` API).
- Various `operator()` overloads to call the remote method. These overloads support:
  - Calling the method with in-args (given there are in-args) passed by reference (lvalue or rvalue references). This is
    the simplest to use variant, but involves copies of the in-args. I.e. it internally uses the `Allocate()` API
    see above) and then copies the argument data from the provided references to the allocated storage, where the
    `MethodInArgPtr` are pointing to.
  - Calling the method with in-args (given there are in-args) passed by `MethodInArgPtr`. This is the zero-copy variant,
    where the user first allocates the in-arg storage via the `Allocate()` API (see above), fills the allocated storage
    via the returned `MethodInArgPtr` and then calls the method with these `MethodInArgPtr`.
  - Calling the method with zero in-args (i.e. methods without in-args). In this case, there is no in-arg data to be
    passed, so the method is just called without any in-args. To handle this case, there is a class template
    specialization of the `ProxyMethod` class template for methods without in-args.
- The return value (in case of a non-void method) of the method call is always returned via a
  `MethodReturnPtr<ReturnType>`, which provides access to the return value data. So the access to the return value is
  always zero-copy.

### Type erasure

While the `impl::ProxyMethod` class template is strongly typed regarding the method signature, the interface to the
binding layer `impl::ProxyMethodBinding` is type-erased. I.e. the binding layer just transports the method call data as
byte buffers without knowledge of the actual data types.
Thus, it is the responsibility of the `impl::ProxyMethod` class template to serialize/deserialize the in-arg and
return value data to/from byte buffers when calling the binding layer APIs. This functionality is provided by the helper
functions/data types in `mw/com/impl/util/type_erased_storage.h`.

The method in-args and the return value are handled separately regarding type erasure. I.e. all in-args of a method call
are serialized into a single type-erased byte buffer, while the return value is serialized into a separate
type-erased byte buffer.
The "aggregation" of N in-args data types into a type erased representation is done by the function template below:

```cpp
template <typename... Args>
constexpr DataTypeSizeInfo CreateDataTypeSizeInfoFromTypes()
```

What it technically does is to create a C++ struct from the provided `Args...` types, where each type is a member of the struct.
The result is then a `TypeErasedDataTypeInfo`, which describes the created struct as a single data type just containing
a size and an alignment requirement.

This `TypeErasedDataTypeInfo`s for the return type and the aggregated in-arguments is generated at compile time as
`static constexpr` members `type_erased_in_args_` and `type_erased_return_type_` in `impl::ProxyMethod`. They are
`std::optional`s, because for methods without in-args or with `void` return type, there is no type-erased data type info
needed.

\todo: Describe, how the type erased info is forwarded to the binding layer.

### Call queue handling

Currently, we are restricting the call-queue size for a method in a proxy instance to a single entry. Since we currently
only support synchronous method calls this is a sensible restriction. Nevertheless, the `impl::ProxyMethod` class
template provides the semantic functionality to handle larger call-queues, which might be used in future for
asynchronous method calls. This means: Although the physical implementation/storage of the call-queue is implemented
in the binding layer, the `impl::ProxyMethod` class template manages the call queue logically. It does so, because it
hands out `MethodInArgPtr` and `MethodReturnPtr` to the user, which are linked to specific logical call-queue entries.

This "linkage" is done via embedding a reference to a call-queue slot specific bool flag, which the `impl::ProxyMethod`
class template manages internally in its members `are_in_arg_ptrs_active_` and `is_return_type_ptr_active_`. This flag
indicates, whether the specific call-queue entry is currently in use.

When the `impl::ProxyMethod` class template hands out a `MethodInArgPtr` or `MethodReturnPtr` to the user, it calls the
ctor of `MethodInArgPtr`/`MethodInArgPtr` with a related reference to the corresponding flag in `are_in_arg_ptrs_active_`
and `is_return_type_ptr_active_` and the `ctor` sets the flag to `true`, indicating that the specific call-queue entry
is in use. Then the `dtor` of `MethodInArgPtr`/`MethodReturnPtr` sets the flag to `false` again, indicating that the
specific call-queue entry is free/available again.

There is some specific logic in case of void-methods. In this case the member `is_return_type_ptr_active_` is re-used
with a slightly different semantics. Here this array doesn't express whether a return value storage is in use (since there
is no return value), but whether a method call is currently in progress for the specific call-queue entry. This is a
preparation for future asynchronous void-method calls, which can be queued. Our current synchronous-only method calls
can't be queued.

## Binding interface for Method on the proxy side

\ToDo: Describe the binding interface for methods on the proxy side.
