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

// TODO: https://github.com/eclipse-score/communication/issues/782
// Implement the method related interface for Lola runtime. This is a placeholder implementation for now.
// also implement required FFI interface to call the Lola runtime for method calls and registration of handlers.

/// All the struct and trait implementations are placeholders for now,
/// and will be implemented in future as per the requirements of the Lola runtime.
use core::future::Future;
use core::ops::Deref;
use score_com_concept::{
    CommData, MethodArgs, MethodArgsAllocate, MethodArgsPtrTuple, MethodCaller, MethodHandler,
    MethodHandlerCall, MethodInArgAllocator, MethodInArgMaybeUninit, MethodInArgPtr,
    MethodReturnSample, Result, Runtime, ZeroCopyArgs,
};

/// Placeholder return sample for a method call result.
/// Wraps the return value and provides `Deref<Target = T>` access,
/// mirroring how `Sample<T>` works for event data.
/// The real implementation will reference shared-memory backing (issue #782).
pub struct LolaMethodReturnSample<T> {
    value: T,
}

impl<T> Deref for LolaMethodReturnSample<T> {
    type Target = T;
    fn deref(&self) -> &T {
        &self.value
    }
}

impl<T> MethodReturnSample<T> for LolaMethodReturnSample<T> {}

pub struct LolaMethodHandler<Args: MethodArgs, Return: CommData, R: Runtime + ?Sized> {
    _phantom: core::marker::PhantomData<(Args, Return, R)>,
}

impl<Args: MethodArgs, Return: CommData, R: Runtime + ?Sized> MethodHandler<Args, Return, R>
    for LolaMethodHandler<Args, Return, R>
{
    fn new(_method_name: &str, _instance_info: R::ProviderInfo) -> Result<Self>
    where
        Self: Sized,
    {
        Ok(LolaMethodHandler {
            _phantom: core::marker::PhantomData,
        })
    }

    // This function should have the thread-pool or
    // async executor to handle the incoming method calls and dispatch them to the registered handler.
    // For now, we will just have a placeholder implementation.
    // So that concurrent method calls can happen on same consumer instance.
    // If two consumer call same methods,
    //which may happen then user should have synchronization mechanism in their handler implementation to handle concurrent calls.
    fn register_handler<F>(&self, _handler: F)
    where
        F: MethodHandlerCall<Args, Return>,
    {
        todo!("Implement the logic to register the handler with the underlying system");
    }
}

pub struct LolaMethodCaller<Args: MethodArgs, Return: CommData, R: Runtime> {
    _phantom: core::marker::PhantomData<(Args, Return, R)>,
}

impl<Args: MethodArgs, Return: CommData, R: Runtime> MethodCaller<Args, Return, R>
    for LolaMethodCaller<Args, Return, R>
{
    fn new(_method_name: &str, _instance_info: R::ConsumerInfo) -> Result<Self>
    where
        Self: Sized,
    {
        Ok(LolaMethodCaller {
            _phantom: core::marker::PhantomData,
        })
    }

    fn invoke_with_copy<'a>(
        &'a self,
        _args: Args,
    ) -> impl Future<Output = Result<R::MethodReturnSample<Return>>> + 'a {
        async move { todo!("Implement the logic to call the method with copied arguments") }
    }

    fn allocate(&self) -> Result<<Args as MethodArgsAllocate<R::MethodInArgAllocator>>::UninitTuple>
    where
        Args: MethodArgsAllocate<R::MethodInArgAllocator>,
    {
        todo!("Implement the logic to allocate argument slots using LolaMethodInArgAllocator");
    }

    fn invoke_zero_copy<'a>(
        &'a self,
        _ptrs: <Args as MethodArgsPtrTuple<R>>::PtrTuple,
    ) -> impl Future<Output = Result<R::MethodReturnSample<Return>>> + 'a
    where
        Args: MethodArgsPtrTuple<R>,
    {
        async move {
            todo!("Implement the logic to call the method with pre-allocated argument pointers")
        }
    }
}

// Lola placeholder for a single pre-allocated method argument slot.
pub struct LolaMethodInArgMaybeUninit<T> {
    _phantom: core::marker::PhantomData<T>,
}

/// Runtime-specific concrete type for a fully-initialised Lola method argument pointer.
pub struct LolaMethodInArgPtr<T> {
    _phantom: core::marker::PhantomData<T>,
}

impl<T> MethodInArgPtr<T> for LolaMethodInArgPtr<T> {}

impl<T> MethodInArgMaybeUninit<T> for LolaMethodInArgMaybeUninit<T> {
    type Ptr = LolaMethodInArgPtr<T>;

    fn write(self, _val: T) -> ZeroCopyArgs<LolaMethodInArgPtr<T>> {
        todo!("Implement write into Lola shared-memory slot");
    }

    unsafe fn assume_init(self) -> ZeroCopyArgs<LolaMethodInArgPtr<T>> {
        todo!("Implement assume_init for Lola shared-memory slot");
    }
}

// Lola placeholder allocator.
pub struct LolaMethodInArgAllocator;

impl MethodInArgAllocator for LolaMethodInArgAllocator {
    type MethodInArgPtr<T: CommData> = LolaMethodInArgPtr<T>;
    type MethodInArgMaybeUninit<T: CommData> = LolaMethodInArgMaybeUninit<T>;
    fn allocate<T: CommData>(&self) -> LolaMethodInArgMaybeUninit<T> {
        todo!("Implement allocation from the Lola shared-memory region via &self context");
    }
}
