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

//! Field producer implementation for Lola runtime.

use core::fmt::Debug;
use core::marker::PhantomData;
use core::todo;

use bridge_ffi_rs::FFIBridge;
use score_com_concept::{
    CommData, FieldPublisher, FieldSampleMut, Result, SampleMaybeUninit as SampleMaybeUninitTrait,
};

use crate::LolaProviderInfo;
use crate::LolaRuntimeImpl;

pub struct LolaFieldPublisher<T, B: FFIBridge> {
    _data: PhantomData<T>,
    _bridge: PhantomData<B>,
}

#[derive(Debug)]
pub struct LolaFieldSampleMut<T> {
    _data: PhantomData<T>,
}

impl<T: CommData + Debug> core::ops::Deref for LolaFieldSampleMut<T> {
    type Target = T;
    fn deref(&self) -> &T {
        todo!()
    }
}
impl<T: CommData + Debug> core::ops::DerefMut for LolaFieldSampleMut<T> {
    fn deref_mut(&mut self) -> &mut T {
        todo!()
    }
}

impl<T: CommData + Debug> score_com_concept::SampleMut<T> for LolaFieldSampleMut<T> {}

impl<T: CommData + Debug> FieldSampleMut<T> for LolaFieldSampleMut<T> {
    fn update(self) -> Result<()> {
        todo!()
    }
}

#[derive(Debug)]
pub struct LolaFieldSampleMaybeUninit<'a, T> {
    _data: core::mem::MaybeUninit<T>,
    _lt: PhantomData<&'a T>,
}

impl<'a, T: CommData + Debug> AsMut<core::mem::MaybeUninit<T>>
    for LolaFieldSampleMaybeUninit<'a, T>
{
    fn as_mut(&mut self) -> &mut core::mem::MaybeUninit<T> {
        &mut self._data
    }
}
impl<'a, T: CommData + Debug> SampleMaybeUninitTrait<T> for LolaFieldSampleMaybeUninit<'a, T> {
    type SampleMut = LolaFieldSampleMut<T>;
    unsafe fn assume_init(self) -> LolaFieldSampleMut<T> {
        todo!()
    }
    fn write(self, _value: T) -> LolaFieldSampleMut<T> {
        todo!()
    }
}

impl<T: CommData + Debug, B: FFIBridge> FieldPublisher<T, LolaRuntimeImpl<B>>
    for LolaFieldPublisher<T, B>
{
    type SampleMaybeUninit<'a>
        = LolaFieldSampleMaybeUninit<'a, T>
    where
        Self: 'a;

    fn new(_identifier: &'static str, _instance_info: LolaProviderInfo<B>) -> Result<Self> {
        todo!()
    }
    fn allocate(&self) -> Result<Self::SampleMaybeUninit<'_>> {
        todo!()
    }
    fn update(&self, _value: T) -> Result<()> {
        todo!()
    }
    fn register_set_handler(&self, _callback: impl Fn(T) -> T + Send + 'static) {
        // When the middleware receives a set request from a consumer:
        // - Invoke the callback with the proposed value; the callback may validate or modify it.
        // - Use the returned value as the final accepted field value to store and send to
        //   subscribers.
        // Execution model (thread pool vs. async task pool) to be decided at implementation time.
        todo!()
    }

    fn register_get_handler(&self, _callback: impl Fn() -> T + Send + 'static) {
        // When the middleware receives a get request from a consumer:
        // - Invoke the callback and return its result to the consumer.
        // Default behaviour (if not registered): return the last Update()d value.
        todo!()
    }
}
