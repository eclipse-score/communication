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
mod ffi {
    use std::marker::PhantomData;

    /// This type represents bmw::mw::com::impl::SkeletonWrapperClass as an opaque struct for any
    /// template argument, as the type isn't relevant when dealing with it as an opaque type.
    /// Note that this struct is empty as we only use references to it on Rust side.
    #[repr(C)]
    pub struct SkeletonWrapperClass {
        _dummy: [u8; 0],
    }

    /// This type represents bmw::mw::com::impl::ProxyEventBase as an opaque struct.
    /// Note that this struct is empty as we only use references to it on Rust side.
    #[repr(C)]
    pub struct SkeletonEvent<T> {
        _dummy: [u8; 0],
        _data: PhantomData<T>,
    }
}

pub use ffi::SkeletonEvent;
pub use ffi::SkeletonWrapperClass;

pub enum UnOffered {}
pub enum Offered {}
pub trait OfferState {}
impl OfferState for UnOffered {}
impl OfferState for Offered {}

pub fn send_skeleton_event<F>(f: F) -> common::Result<()>
where
    F: FnOnce() -> bool,
{
    if f() {
        Ok(())
    } else {
        Err(())
    }
}
