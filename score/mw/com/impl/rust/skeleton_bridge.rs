/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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
use std::sync::Arc;

mod ffi {
    use std::marker::PhantomData;

    /// This type represents score::mw::com::impl::SkeletonWrapperClass as an opaque struct for any
    /// template argument, as the type isn't relevant when dealing with it as an opaque type.
    /// Note that this struct is empty as we only use references to it on Rust side.
    #[repr(C)]
    pub struct SkeletonWrapperClass {
        _dummy: [u8; 0],
    }

    /// This type represents score::mw::com::impl::SkeletonEventBase as an opaque struct.
    /// Note that this struct is empty as we only use references to it on Rust side.
    #[repr(C)]
    pub struct SkeletonEvent<T> {
        _dummy: [u8; 0],
        _data: PhantomData<T>,
    }
}

pub use ffi::SkeletonEvent as NativeSkeletonEvent;
pub use ffi::SkeletonWrapperClass;

pub enum UnOffered {}
pub enum Offered {}
pub trait OfferState {}
impl OfferState for UnOffered {}
impl OfferState for Offered {}

pub trait SkeletonOps: Sized {
    fn send(&self, event: *mut ffi::SkeletonEvent<Self>) -> common::Result<()>;
}

pub struct SkeletonEvent<T: SkeletonOps, S: OfferState, L> {
    event: *mut ffi::SkeletonEvent<T>,
    _skeleton: Arc<L>,
    _marker: std::marker::PhantomData<S>,
}

impl<T: SkeletonOps, S: OfferState, L> SkeletonEvent<T, S, L> {
    pub fn new(event: *mut NativeSkeletonEvent<T>, skeleton: Arc<L>) -> Self {
        Self {
            event,
            _skeleton: skeleton,
            _marker: std::marker::PhantomData,
        }
    }
}

impl<T: SkeletonOps, L> SkeletonEvent<T, UnOffered, L> {
    pub fn offer(self) -> SkeletonEvent<T, Offered, L> {
        SkeletonEvent::<T, Offered, L> {
            event: self.event,
            _skeleton: self._skeleton,
            _marker: std::marker::PhantomData,
        }
    }
}

impl<T: SkeletonOps, L> SkeletonEvent<T, Offered, L> {
    pub fn send(&self, stamped_data: T) -> common::Result<()> {
        stamped_data.send(self.event)
    }

    pub fn stop_offer(self) -> SkeletonEvent<T, UnOffered, L> {
        SkeletonEvent::<T, UnOffered, L> {
            event: self.event,
            _skeleton: self._skeleton,
            _marker: std::marker::PhantomData,
        }
    }
}
