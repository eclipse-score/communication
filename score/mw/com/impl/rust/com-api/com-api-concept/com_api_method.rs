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

/// Placeholder for allocated method arguments in zero-copy API
// TODO: Remove this once memory layout implementation is added in rust side, same like samplePtr.
pub struct MethodInArgPtr<Args> {
    _phantom: core::marker::PhantomData<Args>,
}

// TODO: Do we need add the MethodArgs trait bound to MethodInArgsMaybeUninit?
pub trait MethodInArgsMaybeUninit<Args> {
    fn write(self, args: Args) -> MethodInArgPtr<Args>;

    unsafe fn assume_init(self) -> MethodInArgPtr<Args>;
}

pub trait MethodHandler<Args: MethodArgs, Return: CommData, R: Runtime + ?Sized> {
    fn new(method_name: &str, instance_info: R::ProviderInfo) -> Result<Self>
    where
        Self: Sized;

    fn register_handler<F>(&self, handler: F) -> Result<()>
    where
        F: MethodHandlerCall<Args, Return>;
}

pub trait MethodCaller<Args: MethodArgs, Return: CommData, R: Runtime + ?Sized> {
    type MethodInArgsMaybeUninit<'a>: MethodInArgsMaybeUninit<Args> + 'a
    where
        Self: 'a;
    fn new(method_name: &str, instance_info: R::ConsumerInfo) -> Result<Self>
    where
        Self: Sized;

    fn call_with_copy(&self, args: Args) -> Result<Return>;

    fn allocate(&self) -> Result<Self::MethodInArgsMaybeUninit<'_>>;
    //zero copy call
    fn call(&self, args: MethodInArgPtr<Args>) -> Result<Return>;
}

/// This trait is marker trait for method argument tuples.
pub trait MethodArgs: CommData {}

/// Implement MethodArgs for empty args (no arguments)
impl MethodArgs for () {}

/// Implement MethodArgs for tuples of CommData types, allowing up to 4 arguments.
impl<T: CommData> MethodArgs for (T,) {}
impl<T1: CommData, T2: CommData> MethodArgs for (T1, T2) {}
impl<T1: CommData, T2: CommData, T3: CommData> MethodArgs for (T1, T2, T3) {}
impl<T1: CommData, T2: CommData, T3: CommData, T4: CommData> MethodArgs for (T1, T2, T3, T4) {}

/// This trait is used to define a method handler function that takes arguments of type `Args` and returns a value of type `Return`.
/// This is helpful for calling the method handler, we have defined supporting function `call`.
pub trait MethodHandlerCall<Args, Return>: Send + 'static {
    fn call(&mut self, args: Args) -> Return;
}

/// Implementation for zero-argument methods
impl<F, Return> MethodHandlerCall<(), Return> for F
where
    F: FnMut() -> Return + Send + 'static,
{
    fn call(&mut self, _args: ()) -> Return {
        (self)()
    }
}

/// Implementation for single-argument methods
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

impl<F, T1, T2, T3, Return> MethodHandlerCall<(T1, T2, T3), Return> for F
where
    F: FnMut(T1, T2, T3) -> Return + Send + 'static,
{
    fn call(&mut self, args: (T1, T2, T3)) -> Return {
        (self)(args.0, args.1, args.2)
    }
}

impl<F, T1, T2, T3, T4, Return> MethodHandlerCall<(T1, T2, T3, T4), Return> for F
where
    F: FnMut(T1, T2, T3, T4) -> Return + Send + 'static,
{
    fn call(&mut self, args: (T1, T2, T3, T4)) -> Return {
        (self)(args.0, args.1, args.2, args.3)
    }
}
