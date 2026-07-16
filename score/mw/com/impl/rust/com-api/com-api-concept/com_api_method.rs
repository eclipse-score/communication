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

use crate::com_api_concept::*;
use core::fmt::Debug;

// This is a pointer type for a pre-allocated method argument. It is used in the zero-copy method call path.
// TODO: Remove this once memory layout implementation is added in rust side, same like samplePtr.
pub struct MethodInArgPtr<T> {
    pub _phantom: core::marker::PhantomData<T>,
}

/// This is the uninitialized type for a single method argument. It is used in the zero-copy method call path.
/// Allocate method returns a tuple of these uninitialized method types, which can then be written to and passed to the method call.
pub trait MethodInArgMaybeUninit<T> {
    /// Write a value into this pre-allocated method argument and return the initialized pointer.
    fn write(self, val: T) -> MethodInArgPtr<T>;

    /// Assume the method argument is already initialized and return the pointer.
    ///
    /// # Safety
    /// The caller must ensure the memory has been properly initialized before calling this.
    unsafe fn assume_init(self) -> MethodInArgPtr<T>;
}

/// This is for runtime-specific method argument allocation. It is used in the zero-copy method call path. 
pub trait MethodInArgAllocator {
    /// The concrete uninitialized method argument type this allocator produces for argument type `T`.
    type MethodInArgMaybeUninit<T: CommData>: MethodInArgMaybeUninit<T>;
    /// Produce a new uninitialized method argument for argument type `T`.
    fn allocate<T: CommData>() -> Self::MethodInArgMaybeUninit<T>;
}

/// Producer side registration of method handlers.
/// This is the interface that a producer implements to register handlers for its methods.
pub trait MethodHandler<Args: MethodArgs, Return: CommData, R: Runtime + ?Sized> {
    fn new(method_name: &str, instance_info: R::ProviderInfo) -> Result<Self>
    where
        Self: Sized;

    fn register_handler<F>(&self, handler: F) -> Result<()>
    where
        F: MethodHandlerCall<Args, Return>;
}

/// Consumer side caller of methods.
/// This is the interface that a consumer implements to call methods on a producer.
pub trait MethodCaller<Args: MethodArgs, Return: CommData, R: Runtime + ?Sized> {
    fn new(method_name: &str, instance_info: R::ConsumerInfo) -> Result<Self>
    where
        Self: Sized;

    fn invoke_with_copy(&self, args: Args) -> Result<Return>;

    /// Allocate uninitialized method arguments for a zero-copy method call.
    fn allocate(&self) -> Result<<Args as MethodArgsAllocate<R::MethodInArgAllocator>>::UninitTuple>
    where
        Args: MethodArgsAllocate<R::MethodInArgAllocator>;

    /// Perform a zero-copy method call using pre-written argument pointers.
    fn invoke_zero_copy(&self, ptrs: <Args as MethodArgs>::PtrTuple) -> Result<Return>;
}

// Below traits are supporting traits and which no need to implement by runtime.
// These are implemented in this crate and used by interface macro to generate consumer and producer code.
// And supporting methods with any number of arguments (currently up to 2) without any extra boilerplate.
// like varadic arguments in C++.

/// Marker trait for method argument tuples.
///
/// Carries `PtrTuple` - the matching tuple of `MethodInArgPtr<T>` used in the zero-copy call path.
/// For example, `(Tire, Tire)::PtrTuple = (MethodInArgPtr<Tire>, MethodInArgPtr<Tire>)`.
///
/// Runtimes do not implement this trait.
/// Blanket impls for all supported arities (0–2 arguments) are provided in this crate.
pub trait MethodArgs: CommData {
    type PtrTuple;
}

// TODO: This can be exapnded the limit of function argument can have as per clippy lint.
// Currently, it supports up to 2 argument.
// Also this need to be done with declarative macro to avoid code duplication.
impl MethodArgs for () {
    type PtrTuple = ();
}

impl<T: CommData> MethodArgs for (T,) {
    type PtrTuple = MethodInArgPtr<T>;
}

impl<T1: CommData, T2: CommData> MethodArgs for (T1, T2) {
    type PtrTuple = (MethodInArgPtr<T1>, MethodInArgPtr<T2>);
}

/// Maps an `Args` tuple type to the matching uninitialized method argument tuple for a specific runtime allocator `A`.
///
/// Given an allocator `A` (e.g. `LolaMethodInArgAllocator`) and an args tuple (e.g. `(Tire, Tire)`),
/// `UninitTuple` becomes `(A::MethodInArgMaybeUninit<Tire>, A::MethodInArgMaybeUninit<Tire>)`.
/// `MethodCaller::allocate()` calls `alloc_uninit()` internally to produce these uninitialized method arguments.
///
/// Runtimes do not implement this trait.
/// Blanket impls for all supported arities are provided in this crate.
/// Adding a new runtime allocator automatically gains all existing arities for free.
pub trait MethodArgsAllocate<A: MethodInArgAllocator>: MethodArgs {
    type UninitTuple;
    fn alloc_uninit() -> Self::UninitTuple;
}

// TODO: Replace these impls with a single declarative macro invocation
impl<A: MethodInArgAllocator> MethodArgsAllocate<A> for () {
    type UninitTuple = ();
    fn alloc_uninit() {}
}

impl<T: CommData, A: MethodInArgAllocator> MethodArgsAllocate<A> for (T,) {
    type UninitTuple = A::MethodInArgMaybeUninit<T>;
    fn alloc_uninit() -> Self::UninitTuple {
        A::allocate::<T>()
    }
}

impl<T1: CommData, T2: CommData, A: MethodInArgAllocator> MethodArgsAllocate<A> for (T1, T2) {
    type UninitTuple = (A::MethodInArgMaybeUninit<T1>, A::MethodInArgMaybeUninit<T2>);
    fn alloc_uninit() -> Self::UninitTuple {
        (A::allocate::<T1>(), A::allocate::<T2>())
    }
}

/// Unified input for a method call accepted by the interface macro-generated consumer methods.
///
/// Allows the `interface!` macro to generate exactly a single consumer method per interface method
/// instead of two separate copy and zero-copy methods. Implemented for:
/// - `Args` itself - dispatches to `invoke_with_copy` (copy path)
/// - `MethodInArgPtr<T>,...` - dispatches to `invoke_zero_copy`
///
/// The compiler selects the right impl purely from the type passed at the call site, no runtime branching.
///
/// Runtimes do not implement this trait.
/// All impls are provided in this crate. Adding a new runtime only requires implementing
/// `MethodCaller`, the dispatch through `MethodCallInput` works automatically.
pub trait MethodCallInput<Args: MethodArgs, Return: CommData + Debug, R: Runtime + ?Sized> {
    fn invoke(self, caller: &R::MethodCaller<Args, Return>) -> Result<Return>
    where
        R::MethodCaller<Args, Return>: MethodCaller<Args, Return, R>;
}

// TODO: Replace these impls with a single declarative macro invocation
/// Copy path: pass `Args` directly.
impl<Args, Return, R> MethodCallInput<Args, Return, R> for Args
where
    Args: MethodArgs + CommData,
    Return: CommData + Debug,
    R: Runtime + ?Sized,
    R::MethodCaller<Args, Return>: MethodCaller<Args, Return, R>,
{
    fn invoke(self, caller: &R::MethodCaller<Args, Return>) -> Result<Return>
    where
        R::MethodCaller<Args, Return>: MethodCaller<Args, Return, R>,
    {
        <R::MethodCaller<Args, Return> as MethodCaller<Args, Return, R>>::invoke_with_copy(
            caller, self,
        )
    }
}

/// Zero-copy path for single-argument methods: pass `MethodInArgPtr<T>` directly.
impl<T, Return, R> MethodCallInput<(T,), Return, R> for MethodInArgPtr<T>
where
    T: CommData,
    Return: CommData + Debug,
    R: Runtime + ?Sized,
    R::MethodCaller<(T,), Return>: MethodCaller<(T,), Return, R>,
{
    fn invoke(self, caller: &R::MethodCaller<(T,), Return>) -> Result<Return>
    where
        R::MethodCaller<(T,), Return>: MethodCaller<(T,), Return, R>,
    {
        <R::MethodCaller<(T,), Return> as MethodCaller<(T,), Return, R>>::invoke_zero_copy(
            caller, self,
        )
    }
}

/// Zero-copy path for two-argument methods.
impl<T1, T2, Return, R> MethodCallInput<(T1, T2), Return, R>
    for (MethodInArgPtr<T1>, MethodInArgPtr<T2>)
where
    T1: CommData,
    T2: CommData,
    Return: CommData + Debug,
    R: Runtime + ?Sized,
    R::MethodCaller<(T1, T2), Return>: MethodCaller<(T1, T2), Return, R>,
{
    fn invoke(self, caller: &R::MethodCaller<(T1, T2), Return>) -> Result<Return>
    where
        R::MethodCaller<(T1, T2), Return>: MethodCaller<(T1, T2), Return, R>,
    {
        <R::MethodCaller<(T1, T2), Return> as MethodCaller<(T1, T2), Return, R>>::invoke_zero_copy(
            caller, self,
        )
    }
}

/// Callable handler function for a method with `Args` inputs and `Return` output.
///
/// Application code on the producer side passes a plain closure to `MethodHandler::register_handler`;
/// any `FnMut` with the matching signature automatically satisfies this trait.
///
/// Runtimes do not implement this trait.
/// Blanket impls for all supported arities are provided in this crate so that closures
/// just work without any extra boilerplate.
pub trait MethodHandlerCall<Args, Return>: Send + 'static {
    fn call(&mut self, args: Args) -> Return;
}

// TODO: Replace these impls with a single declarative macro invocation
impl<F, Return> MethodHandlerCall<(), Return> for F
where
    F: FnMut() -> Return + Send + 'static,
{
    fn call(&mut self, _args: ()) -> Return {
        (self)()
    }
}

impl<F, T1, Return> MethodHandlerCall<(T1,), Return> for F
where
    F: FnMut(T1) -> Return + Send + 'static,
{
    fn call(&mut self, args: (T1,)) -> Return {
        (self)(args.0)
    }
}

impl<F, T1, T2, Return> MethodHandlerCall<(T1, T2), Return> for F
where
    F: FnMut(T1, T2) -> Return + Send + 'static,
{
    fn call(&mut self, args: (T1, T2)) -> Return {
        (self)(args.0, args.1)
    }
}
