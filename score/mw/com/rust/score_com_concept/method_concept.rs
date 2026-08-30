/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/// For method as rust side does not have any varadic function argument support,
/// we are having tuple of arguments, so we can have any number of arguments (currently up to 8) without any extra boilerplate.
/// we have implemeted blanket implementation of MethodArgs, MethodArgsAllocate and
/// MethodCallInput traits for all supported arities (0–8 arguments) using `impl_all_arities!` macro in this crate.
/// This blanket implementation help in design to have any number of arguments
/// (currently up to 8) without runtime specific implementation for each arity.
/// This crate provides the necessary traits and types to support method calls in a communication API,
/// including handling of method arguments, allocation of uninitialized argument,
/// and invocation of methods with both copy and zero-copy semantics.
///
/// We want to follow the same semantics for method like c++ provide for method call,
/// and because of that we have added few supporting traits which help to create similar semantics for method call in rust side.
/// In the event and field we have `SampleMut` with that allocated memory can call send,
/// but in method call we can not use that approach as we do not have any common API to call send/update,
/// Method takes the argument whether it is by value or by zero-copy in same method function/method,
/// because of that we have added `invoke_with_copy` and `invoke_zero_copy` methods in MethodCaller trait.
/// Which is not user facing APIs but used by interface macro and supporting traits.
///
/// trait details:
/// `MethodHandler`: Producer side registration of method handlers,
/// this needs to be implemented by runtime for producer side method handler registration.
/// `MethodCaller`: Consumer side caller of methods, this needs to be implemented by runtime for consumer side method calls.
/// This trait provides methods for invoking methods with both copy and zero-copy semantics,
/// also handles allocation of uninitialized method arguments for zero-copy calls,
/// this is user facing API for consumer side method calls.
/// This trait is used by the interface macro to generate consumer methods for each interface method and
/// invoke the runtime specific method caller implementation.
/// `MethodInArgMaybeUninit`: This is the uninitialized type for a single method argument,
/// it is used in the zero-copy method call path.
/// `MethodInArgAllocator`: This is for runtime-specific method argument allocation,
/// it is used in the zero-copy method call path.
/// Which provide the allocate API for specific argument type and
/// return the uninitialized method argument type for that argument type.
/// `MethodReturnSample`: This is a trait for the return value of a method call on the consumer side,
/// it is used to provide `Deref<Target = T>` access to the return value,
/// `MethodInArgPtr`: This is a trait for the pointer type for a single method argument,
/// it is used in the zero-copy method call path.
/// `ZeroCopyArgs<P>`: Newtype wrapper for the zero-copy dispatch, returned by `MethodInArgMaybeUninit::write()`
///
/// Now below traits are marker / marker-like (because it is implemented for all supported arities) traits and
/// which no need to implement by runtime because blanket implementation is added in this crate.
///
/// `MethodArgs`: Marker trait for method argument tuples,
/// no need to implement by runtime because blanket implementation is added in this crate.
/// `MethodArgsPtrTuple`: Maps an Args tuple to the matching ZeroCopyArgs-wrapped pointer tuple for a given runtime R,
/// this is used to provide the PtrTuple type for the zero-copy call path,
/// it is a separate trait from MethodArgs because pointer types are runtime-specific.
/// `MethodArgsAllocate`: Maps an Args tuple type to the matching uninitialized method argument tuple for a specific runtime allocator A,
/// this is used to produce the uninitialized method arguments for zero-copy method call path.
/// `MethodCallInput`: Unified input for a method call accepted by the interface macro-generated consumer methods,
/// this is used to dispatch the method call to the appropriate runtime specific method -
/// caller implementation based on the type of the input arguments.
/// `MethodHandlerCall`: Callable handler function for a method with Args inputs and Return output,
/// this is used to register the handler function for a method on the producer side,
/// which can be a plain closure or any FnMut with the matching signature,
/// and it will automatically satisfy this trait,
/// so that the interface macro can generate the necessary code to register the handler function for each interface method on the producer side.
///
/// We have method_arities_macros.rs which is used to generate the blanket implementation of MethodArgs, MethodArgsAllocate and
/// MethodCallInput traits for all supported arities (0–8 arguments) are provided in this crate.
/// This macro also extend the `Reloc` and `CommData` traits for all supported arities (0–8 arguments) are provided in this crate.
/// Macro invocation happen in same file so no need to invoke from user side or runtime side.
/// Also we can not depend on `interface_macros` because this macro code expand in library crate and interface macro is used in user crate,
/// and library should be build before user crate, so we can not depend on that.
///
// TODO: Add a blocking `.wait()` convenience for method-call futures, for sync callers who don't
// want to bring their own async executor (similar to `futures::executor::block_on`).
use crate::concept::{CommData, Result, Runtime};
use core::future::Future;
use core::ops::Deref;

/// Trait for the return value of a method call on the consumer side.
// Mirrors `Sample<T>` in the event design
pub trait MethodReturnSample<T>: Deref<Target = T> {}

/// Marker trait for a fully-initialised, pre-allocated method argument.
/// Runtimes implement this with a concrete type that stores an FFI pointer
pub trait MethodInArgPtr<T> {}

/// Newtype wrapper returned by `MethodInArgMaybeUninit::write()`.
/// Passing a tuple of `ZeroCopyArgs<P>` to a consumer method selects the zero-copy call path.
/// `P` is the runtime-specific type that implements `MethodInArgPtr<T>`.
pub struct ZeroCopyArgs<P>(pub P);

/// Producer side registration of method handlers.
/// This is the interface that a producer implements to register handlers for its methods.
pub trait MethodHandler<Args: MethodArgs, Return: CommData, R: Runtime + ?Sized> {
    /// Create a new method handler for the given method name and instance info.
    ///
    /// # Arguments
    /// * `method_name` - The name of the method to handle.
    /// * `instance_info` - The provider instance info for the method handler.
    ///
    /// Returns a `Result` containing the new method handler or an error if the creation failed.
    fn new(method_name: &str, instance_info: R::ProviderInfo) -> Result<Self>
    where
        Self: Sized;

    /// Register a handler function for the method.
    /// which is automatically satisfied by any function or closure with the appropriate signature.
    ///
    /// # Arguments
    /// * `handler` - The handler function to register for the method,
    ///   which has to bound the `MethodHandlerCall<Args, Return>` trait blanket implementation.
    fn register_handler<F>(&self, handler: F)
    where
        F: MethodHandlerCall<Args, Return>;
}

/// Consumer side caller of methods.
/// This is the interface that a consumer implements to call methods on a producer.
/// In this trait we have two methods for method call, one is `invoke_with_copy` and another is `invoke_zero_copy`,
/// Which are not intended to be used by user directly,
/// but used by interface macro to generate consumer methods for each interface method.
/// This used by runtime to implement the specific implementation for method call.
/// Both call methods return a future so callers can `.await` the result.
pub trait MethodCaller<Args: MethodArgs, Return: CommData, R: Runtime + ?Sized> {
    /// Create a new method caller for the given method name and instance info.
    ///
    /// # Arguments
    /// * `method_name` - The name of the method to call.
    /// * `instance_info` - The consumer instance info for the method call.
    ///
    /// Returns a `Result` containing the new method caller or an error if the creation failed.
    fn new(method_name: &str, instance_info: R::ConsumerInfo) -> Result<Self>
    where
        Self: Sized;

    /// Invoke the method with copied arguments. This is the copy path for method calls.
    ///
    /// # Arguments
    /// * `args` - The method arguments to pass to the method call.
    ///
    /// Returns a future that resolves to a `Result` containing a `MethodReturnSample<Return>`
    /// which provides `Deref<Target = Return>` access to the return value,
    /// analogous to `Sample<T>` for events — allowing zero-copy access to the return data
    /// when the runtime backs it with shared memory.
    fn invoke_with_copy<'a>(
        &'a self,
        args: Args,
    ) -> impl Future<Output = Result<R::MethodReturnSample<Return>>> + 'a;

    /// Allocate uninitialized method arguments for a zero-copy method call.
    ///
    /// Returns a `Result` containing the uninitialized method arguments tuple or an error if the allocation failed.
    ///
    /// Note: This method returns the tuple of uninitialized method arguments for the given `Args` type,
    /// which can then be written individually and passed to the method.
    /// Here `Args` is a tuple of method argument types for the given method,
    /// and `UninitTuple` is the corresponding tuple of uninitialized method argument types for
    /// the given runtime's method argument allocator.
    /// e.g., for a method with signature `fn my_method(arg1: T1, arg2: T2) -> Return`,
    /// the `Args` type would be `(T1, T2)`,
    /// and the `UninitTuple` type would be `(A::MethodInArgMaybeUninit<T1>, A::MethodInArgMaybeUninit<T2>)`
    /// where `A` is the runtime's method argument allocator.
    fn allocate(
        &self,
    ) -> Result<<Args as MethodArgsAllocate<R::MethodInArgAllocator>>::UninitTuple>
    where
        Args: MethodArgsAllocate<R::MethodInArgAllocator>;

    /// Invoke the method with zero-copy arguments. This is the zero-copy path for method calls.
    ///
    /// # Arguments
    /// * `ptrs` - A tuple of `ZeroCopyArgs<R::MethodInArgPtr<T>>`-wrapped pointers, one per
    ///   method argument.  The runtime is responsible for destructuring the `ZeroCopyArgs`
    ///   wrappers to access the inner runtime-specific pointer.
    ///
    /// Returns a future that resolves to a `Result` containing a `MethodReturnSample<Return>`
    /// which provides `Deref<Target = Return>` access to the return value.
    fn invoke_zero_copy<'a>(
        &'a self,
        ptrs: <Args as MethodArgsPtrTuple<R>>::PtrTuple,
    ) -> impl Future<Output = Result<R::MethodReturnSample<Return>>> + 'a
    where
        Args: MethodArgsPtrTuple<R>;
}

/// This is the uninitialized type for a single method argument. It is used in the zero-copy method call path.
/// Allocate method returns a tuple of these uninitialized method types, which can then be written to and passed to the method call.
///
/// # Note:
/// `MethodCaller::allocate()` returns a tuple of `MethodInArgMaybeUninit` values.  The current
/// API does not enforce at compile time that all method arguments in the tuple are written before
/// method is called. A user can call `assume_init()` on an unwritten method argument, which
/// is undefined behaviour once real shared memory backs these method arguments.
pub trait MethodInArgMaybeUninit<T> {
    /// The runtime-specific concrete pointer type produced after initialisation.
    // Mirrors `SampleMaybeUninit::SampleMut` in the event design.
    type Ptr: MethodInArgPtr<T>;

    /// Write a value into this pre-allocated method argument slot and return the initialised pointer.
    fn write(self, val: T) -> ZeroCopyArgs<Self::Ptr>;

    /// Assume the method argument slot is already initialised and return the pointer.
    ///
    /// # Safety
    /// The caller must ensure the memory has been properly initialized before calling this.
    unsafe fn assume_init(self) -> ZeroCopyArgs<Self::Ptr>;
}

/// This is for runtime-specific method argument allocation. It is used in the zero-copy method call path.
/// This trait provides the allocate API for specific argument type and return the uninitialized method argument type for that argument type.
pub trait MethodInArgAllocator {
    /// The runtime-specific concrete pointer type this allocator's uninit slots produce after initialisation.
    type MethodInArgPtr<T: CommData>: MethodInArgPtr<T>;

    /// The concrete uninitialized method argument type this allocator produces for argument type `T`.
    type MethodInArgMaybeUninit<T: CommData>: MethodInArgMaybeUninit<
        T,
        Ptr = Self::MethodInArgPtr<T>,
    >;

    /// Produce a new uninitialized method argument for argument type `T`.
    ///
    /// Returns a `MethodInArgMaybeUninit<T>` which can then be written to and passed to the method call.
    fn allocate<T: CommData>(&self) -> Self::MethodInArgMaybeUninit<T>;
}

// Below traits are supporting traits and which no need to implement by runtime.
// These are implemented in this crate and used by interface macro to generate consumer and producer code.
// And supporting methods with any number of arguments based on the `impl_all_arities!` macro in this crate.

/// Marker trait for method argument tuples.
///
/// Runtimes do not implement this trait.
/// Blanket impls for all supported arities (0–8 arguments) are provided in this crate.
pub trait MethodArgs: CommData {}

// Arity-0 unit tuple - special-cased here, arities 1+ are generated by
// `impl_all_arities!` in `method_arities_macros.rs`.
impl MethodArgs for () {}

/// Maps an `Args` tuple to the matching `ZeroCopyArgs`-wrapped pointer tuple for a given runtime `R`.
///
/// For runtime `R` and args `(Tire, Tire)`, `PtrTuple` becomes
/// `(ZeroCopyArgs<R::MethodInArgPtr<Tire>>, ZeroCopyArgs<R::MethodInArgPtr<Tire>>)` —
/// the types that the user produces from `write()` and passes to the consumer method,
/// and that `MethodCaller::invoke_zero_copy` receives directly.
///
/// This is a separate runtime-parameterised trait because pointer types are runtime-specific,
/// they live in `R::MethodInArgPtr<T>`, not in the concept crate.
///
/// Runtimes do not implement this trait.
/// Blanket impls for all supported arities are generated by `impl_all_arities!` in `method_arities_macros.rs`.
pub trait MethodArgsPtrTuple<R: Runtime + ?Sized>: MethodArgs {
    /// The tuple of runtime-specific pointer types for this args tuple.
    type PtrTuple;
}

// Arity-0 unit tuple - zero args means no pointers, regardless of runtime.
// Arities 1+ are generated by `impl_all_arities!` in `method_arities_macros.rs`.
impl<R: Runtime + ?Sized> MethodArgsPtrTuple<R> for () {
    type PtrTuple = ();
}

/// Maps an `Args` tuple type to the matching uninitialized method argument tuple for a specific runtime allocator `A`.
///
/// Given an allocator `A` (e.g. `LolaMethodInArgAllocator`) and an args tuple (e.g. `(Tire, Tire)`),
/// `UninitTuple` becomes `(A::MethodInArgMaybeUninit<Tire>, A::MethodInArgMaybeUninit<Tire>)`.
/// `MethodCaller::allocate()` calls `alloc_uninit()` internally to produce these uninitialized method arguments.
///
/// Runtimes do not implement this trait.
/// Blanket impls for all supported arities are provided in this crate.
pub trait MethodArgsAllocate<A: MethodInArgAllocator>: MethodArgs {
    type UninitTuple;
    /// Allocate uninitialized method arguments for all parameters using the provided allocator instance.
    ///
    /// Returns a tuple of uninitialized method arguments corresponding to the `Args` tuple type.
    fn alloc_uninit(allocator: &A) -> Self::UninitTuple;
}

// Arity-0 unit tuple - special-cased here; arities 1+ are generated by
// `impl_all_arities!` in `method_arities_macros.rs`.
impl<A: MethodInArgAllocator> MethodArgsAllocate<A> for () {
    type UninitTuple = ();
    fn alloc_uninit(_allocator: &A) {}
}

/// Unified input for a method call accepted by the interface macro-generated consumer methods.
///
/// Allows the `interface!` macro to generate exactly a single consumer method per interface method
/// instead of two separate copy and zero-copy methods. Implemented for:
/// - `Args` itself - dispatches to `invoke_with_copy` (copy path)
/// - `(ZeroCopyArgs<P1>, ...)` - dispatches to `invoke_zero_copy` (zero-copy path)
///
/// The compiler selects the right impl purely from the type passed at the call site, no runtime branching.
///
/// Runtimes do not implement this trait.
/// All impls are provided in this crate. Adding a new runtime only requires implementing
/// `MethodCaller`, the dispatch through `MethodCallInput` works automatically.
pub trait MethodCallInput<Args: MethodArgs, Return: CommData, R: Runtime + ?Sized> {
    /// Invoke the method with the given input, dispatching to the appropriate runtime-specific method caller.
    ///
    /// # Arguments
    /// * `caller` - The runtime-specific method caller to use for the invocation.
    ///
    /// Returns a future that resolves to a `Result<R::MethodReturnSample<Return>>`,
    /// providing `Deref<Target = Return>` access to the return value.
    fn invoke<'a>(
        self,
        caller: &'a R::MethodCaller<Args, Return>,
    ) -> impl Future<Output = Result<R::MethodReturnSample<Return>>> + 'a
    where
        R::MethodCaller<Args, Return>: MethodCaller<Args, Return, R> + 'a;
}

/// Copy path: blanket impl - arity-agnostic, no per-arity duplication needed.
/// pass `Args` directly.
impl<Args, Return, R> MethodCallInput<Args, Return, R> for Args
where
    Args: MethodArgs + CommData,
    Return: CommData,
    R: Runtime + ?Sized,
    R::MethodCaller<Args, Return>: MethodCaller<Args, Return, R>,
{
    fn invoke<'a>(
        self,
        caller: &'a R::MethodCaller<Args, Return>,
    ) -> impl Future<Output = Result<R::MethodReturnSample<Return>>> + 'a
    where
        R::MethodCaller<Args, Return>: MethodCaller<Args, Return, R> + 'a,
    {
        <R::MethodCaller<Args, Return> as MethodCaller<Args, Return, R>>::invoke_with_copy(
            caller, self,
        )
    }
}

// Zero-copy path: all arities 1+ are generated by `impl_all_arities!` in `method_arities.rs`.

/// Callable handler function for a method with `Args` inputs and `Return` output.
///
/// Application code on the producer side passes a plain closure to `MethodHandler::register_handler`;
/// any `Fn` with the matching signature automatically satisfies this trait.
/// Runtimes do not implement this trait.
/// Blanket impls for all supported arities are provided in this crate so that closures just work without any extra boilerplate.
///
/// Note: Arguments are passed by reference to avoid an extra copy/move of potentially large
/// argument types. The result is still returned by value.
pub trait MethodHandlerCall<Args, Return>: Send + Sync + 'static {
    /// Call the handler function with a reference to the arguments and return the result.
    fn call(&self, args: &Args) -> Return;
}

// Arity-0 unit tuple - special-cased here; arities 1+ are generated by
// `impl_all_arities!` in `method_arities_macros.rs`.
impl<F, Return> MethodHandlerCall<(), Return> for F
where
    F: Fn() -> Return + Send + Sync + 'static,
{
    fn call(&self, _args: &()) -> Return {
        (self)()
    }
}
