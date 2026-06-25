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

//! Common FFI types shared across `bridge_ffi` and its implementations.
//!
//! Contains the opaque C++ wrapper types for service discovery
//! (`InstanceSpecifier`, `HandleContainer`) together with the `extern "C"` declarations
//! needed to manage their lifetime.

use core::fmt::{Debug, Formatter};
use std::ops::Index;

/// Opaque C++ type: `score::mw::com::impl::HandleType`.
#[repr(C)]
#[derive(Default)]
pub struct HandleType {
    _dummy: [u8; 0],
}

/// Opaque C++ type: `score::mw::com::ServiceHandleContainer<HandleType>`.
#[repr(C)]
#[derive(Default)]
pub struct NativeHandleContainer {
    _dummy: [u8; 0],
}

/// Opaque C++ type: `score::mw::com::InstanceSpecifier`.
#[repr(C)]
pub struct NativeInstanceSpecifier {
    _dummy: [u8; 0],
}

unsafe extern "C" {
    pub(crate) fn mw_com_impl_instance_specifier_create(
        value: *const u8,
        len: u32,
    ) -> *mut NativeInstanceSpecifier;
    pub(crate) fn mw_com_impl_instance_specifier_clone(
        instance_specifier: *const NativeInstanceSpecifier,
    ) -> *mut NativeInstanceSpecifier;
    pub(crate) fn mw_com_impl_instance_specifier_delete(
        instance_specifier: *mut NativeInstanceSpecifier,
    );
    pub(crate) fn mw_com_impl_handle_container_delete(container: *mut NativeHandleContainer);
    pub(crate) fn mw_com_impl_handle_container_get_size(
        container: *const NativeHandleContainer,
    ) -> u32;
    pub(crate) fn mw_com_impl_handle_container_get_handle_at(
        container: *const NativeHandleContainer,
        pos: u32,
    ) -> *const HandleType;
}

/// Human-readable address of a service instance (e.g. `/vehicle/speed`).
/// Create via `InstanceSpecifier::try_from("/my/service")`.
pub struct InstanceSpecifier {
    pub(crate) inner: *mut NativeInstanceSpecifier,
}

impl InstanceSpecifier {
    /// Returns the raw native pointer. Valid as long as `self` is alive.
    pub fn as_native(&self) -> *const NativeInstanceSpecifier {
        self.inner
    }

    /// Returns a mutable raw native pointer. Valid as long as `self` is alive.
    pub fn as_native_mut(&self) -> *mut NativeInstanceSpecifier {
        self.inner
    }
}

impl TryFrom<&'_ str> for InstanceSpecifier {
    type Error = ();

    fn try_from(value: &'_ str) -> Result<Self, Self::Error> {
        // SAFETY: value points to a valid UTF-8 string; len matches the slice length.
        let inner =
            unsafe { mw_com_impl_instance_specifier_create(value.as_ptr(), value.len() as u32) };
        if inner.is_null() {
            Err(())
        } else {
            Ok(Self { inner })
        }
    }
}

impl Clone for InstanceSpecifier {
    fn clone(&self) -> Self {
        // SAFETY: self.inner was obtained from mw_com_impl_instance_specifier_create.
        let inner = unsafe { mw_com_impl_instance_specifier_clone(self.inner) };
        Self { inner }
    }
}

impl Drop for InstanceSpecifier {
    fn drop(&mut self) {
        // SAFETY: self.inner was obtained from mw_com_impl_instance_specifier_create.
        unsafe { mw_com_impl_instance_specifier_delete(self.inner) }
    }
}

/// Wrapper around a native C++ handle container returned by `find_service`.
pub struct HandleContainer {
    pub(crate) inner: *mut NativeHandleContainer,
}

// SAFETY: The pointer is heap-allocated by C++ and owned exclusively by this struct.
unsafe impl Send for HandleContainer {}
// SAFETY: No interior mutability; all access goes through shared references to immutable data.
unsafe impl Sync for HandleContainer {}

impl HandleContainer {
    /// Wraps a raw pointer returned by C++ FFI. Takes ownership.
    pub fn new(inner: *mut NativeHandleContainer) -> Self {
        Self { inner }
    }

    /// Number of handles in the container.
    #[allow(clippy::len_without_is_empty)]
    pub fn len(&self) -> usize {
        // SAFETY: self.inner is a valid pointer from mw_com_impl_find_service.
        unsafe { mw_com_impl_handle_container_get_size(self.inner) as usize }
    }

    /// Returns `true` if the container has no handles.
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Returns the first handle, or `None` if empty.
    pub fn first(&self) -> Option<&HandleType> {
        if !self.is_empty() {
            Some(self.index(0))
        } else {
            None
        }
    }

    /// Returns the handle at `index`, or `None` if out of bounds.
    pub fn get(&self, index: usize) -> Option<&HandleType> {
        if index < self.len() {
            Some(self.index(index))
        } else {
            None
        }
    }
}

impl Index<usize> for HandleContainer {
    type Output = HandleType;

    fn index(&self, index: usize) -> &Self::Output {
        assert!(index < self.len(), "HandleContainer index out of bounds");
        // SAFETY: index is within bounds; lifetime of result is tied to &self.
        unsafe {
            mw_com_impl_handle_container_get_handle_at(self.inner, index as u32)
                .as_ref()
                .expect("nullptr received as handle")
        }
    }
}

impl Drop for HandleContainer {
    fn drop(&mut self) {
        // SAFETY: self.inner was obtained from mw_com_impl_find_service.
        unsafe { mw_com_impl_handle_container_delete(self.inner) }
    }
}

impl Debug for HandleContainer {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("HandleContainer").finish()
    }
}
