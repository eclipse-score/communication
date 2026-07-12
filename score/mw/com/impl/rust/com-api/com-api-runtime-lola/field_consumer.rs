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

//! Field consumer implementation for Lola runtime.
//! It implements the field consumer related traits.
//! We are using the event related trait as a base trait for whereever we have same common
//! APIs or functionality, as of now there are derived from concept crate but
//! we will create a module which will have common trait for event and field which will be used by both event and field consumer/publisher.

use core::fmt::Debug;
use core::marker::PhantomData;
use core::todo;

use bridge_ffi_rs::FFIBridge;
use com_api_concept::{
    CommData, FieldSubscriber, FieldSubscription, MethodReturnTypePtr, Result, Runtime,
    SampleContainer, Subscriber, Subscription,
};

use crate::consumer::Sample;
use crate::{LolaConsumerInfo, LolaRuntimeImpl};

/// Field subscriber type which implements the FieldSubscriber trait for Lola runtime.
/// It will implement `subscribe` method to create a field subscription and `get` and `set` methods to get and set the value of the field.
pub struct LolaFieldSubscriber<T: CommData + Debug, B: FFIBridge> {
    _data: PhantomData<T>,
    _bridge: PhantomData<B>,
}

/// Marker implementation of FieldSubscriber trait.
impl<T: CommData + Debug, B: FFIBridge> FieldSubscriber<T, LolaRuntimeImpl<B>>
    for LolaFieldSubscriber<T, B>
{
    fn get(&self) -> Result<MethodReturnTypePtr<T>> {
        todo!()
    }
    fn set(&self, _value: T) -> Result<MethodReturnTypePtr<T>> {
        todo!()
    }
}

/// Implementation of Subscriber trait which provides `new` and `subscribe` methods for LolaFieldSubscriber.
impl<T: CommData + Debug, B: FFIBridge> Subscriber<T, LolaRuntimeImpl<B>>
    for LolaFieldSubscriber<T, B>
{
    type Subscription = LolaFieldSubscription<T, B>;

    fn new(_identifier: &'static str, _instance_info: LolaConsumerInfo<B>) -> Result<Self> {
        todo!()
    }

    fn subscribe(self, _max_num_samples: usize) -> Result<Self::Subscription> {
        todo!()
    }
}

/// FieldSubscription type which provides data receiving APIs and unsubscribe method.
pub struct LolaFieldSubscription<T: CommData + Debug, B: FFIBridge> {
    _data: PhantomData<T>,
    _bridge: PhantomData<B>,
}

impl<T: CommData + Debug, B: FFIBridge> FieldSubscription<T, LolaRuntimeImpl<B>>
    for LolaFieldSubscription<T, B>
{
    fn get_free_sample_count(&self) -> Result<usize> {
        todo!()
    }

    fn get_num_new_samples_available(&self) -> Result<usize> {
        todo!()
    }

    fn get(&self) -> impl Future<Output = Result<MethodReturnTypePtr<T>>> + Send {
        async { todo!() }
    }
    fn set(&self, _value: T) -> Result<MethodReturnTypePtr<T>> {
        todo!()
    }
}

/// Implementation of Subscription trait which provides receiving APIs.
impl<T: CommData + Debug, B: FFIBridge> Subscription<T, LolaRuntimeImpl<B>>
    for LolaFieldSubscription<T, B>
{
    type Subscriber = LolaFieldSubscriber<T, B>;
    type Sample<'a>
        = Sample<T, B>
    where
        Self: 'a;

    fn unsubscribe(self) -> Self::Subscriber {
        todo!()
    }

    fn try_receive<'a>(
        &'a self,
        _scratch: &'_ mut SampleContainer<Self::Sample<'a>>,
        _max_samples: usize,
    ) -> Result<usize> {
        todo!()
    }

    fn cancellable_receive<'a>(
        &'a self,
        _scratch: SampleContainer<Self::Sample<'a>>,
        _new_samples: usize,
        _max_samples: usize,
        _cancellation: impl core::future::Future<Output = ()> + Send + 'static,
    ) -> impl core::future::Future<Output = (SampleContainer<Self::Sample<'a>>, Result<usize>)> + 'a
    {
        async { todo!() }
    }

    fn to_stream<'a>(
        &'a mut self,
    ) -> impl futures::stream::Stream<Item = Result<Self::Sample<'a>>> + Unpin + 'a {
        futures::stream::empty()
    }
}
