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

use bridge_ffi_rs::FFIBridge;
use com_api_concept::{
    CommData, FieldPublisher, FieldSampleMut, Result, SampleMaybeUninit as SampleMaybeUninitTrait,
};

use crate::LolaProviderInfo;
use crate::LolaRuntimeImpl;

pub struct LolaFieldPublisher<T, B: FFIBridge> {
    _data: PhantomData<T>,
    _bridge: PhantomData<B>,
}

//TODO: Need to think about this clone, we added as Producer and OfferProducer both
//Type is using same field publisher to call the Update and set handler.
//As Set handler is async call so we need to keep producer active for that.
impl<T, B: FFIBridge> Clone for LolaFieldPublisher<T, B> {
    fn clone(&self) -> Self {
        LolaFieldPublisher {
            _data: PhantomData,
            _bridge: PhantomData,
        }
    }
}

impl<T, B: FFIBridge> Copy for LolaFieldPublisher<T, B> {}

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

impl<T: CommData + Debug> com_api_concept::SampleMut<T> for LolaFieldSampleMut<T> {}

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
    type FieldSample<'a>
        = LolaFieldSampleMut<T>
    where
        Self: 'a;
    type SampleMaybeUninit<'a>
        = LolaFieldSampleMaybeUninit<'a, T>
    where
        Self: 'a;

    fn new(_identifier: &str, _instance_info: LolaProviderInfo<B>) -> Result<Self> {
        todo!()
    }
    fn allocate(&self) -> Result<Self::SampleMaybeUninit<'_>> {
        todo!()
    }
    fn update(&self, _value: &T) -> Result<()> {
        todo!()
    }
    fn register_set_handler<'a>(&self) -> impl core::future::Future<Output = Result<&'a T>> + 'a {
        async { todo!() }
    }
}
