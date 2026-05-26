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

// This is for internal use only, not part of public API, so we can hide it from crate level docs.
#![doc(hidden)]

//! Mock implementation of FFIBridge using mockall for type-safe, per-test configuration.
//!
//! This provides a mockall-based mock where tests can configure expectations locally.
//! Uses a hybrid approach: mockall for most methods, with a wrapper for lifetime constraints.
//!
//! # Components
//!
//! ## SharedMockBridge
//! Wrapper that allows all clones to share the same underlying mock instance and expectations,
//! eliminating the need for complex clone expectation setup in tests.
//!
//! ## MockPointerAllocator
//! Helper utility for managing FFI pointer allocations in tests. Eliminates the repetitive
//! `Arc<Mutex<Vec<Box<T>>>>` pattern and provides automatic cleanup tracking.
//!
//! # Usage
//! ```ignore
//! use bridge_ffi_mock::{MockFFIBridge, SharedMockBridge, MockPointerAllocator};
//!
//! let mut mock = MockFFIBridge::new();
//! // Use MockPointerAllocator for cleaner pointer management
//! let skeleton_alloc = MockPointerAllocator::<SkeletonBase>::new();
//! let skel = skeleton_alloc.clone();
//! mock.expect_create_skeleton()
//!     .returning(move |_, _| skel.allocate());
//!
//! let bridge = SharedMockBridge::new(mock);
//! // All clones automatically share the same expectations
//! let clone1 = bridge.clone();
//! ```
//!
//! # With Call Sequencing
//! Use `mockall::Sequence` to enforce call ordering:
//! ```ignore
//! use mockall::Sequence;
//!
//! let mut mock = MockFFIBridge::new();
//! let mut seq = Sequence::new();
//!
//! let skeleton_alloc = MockPointerAllocator::<SkeletonBase>::new();
//! let skel = skeleton_alloc.clone();
//! mock.expect_create_skeleton()
//!     .in_sequence(&mut seq)
//!     .returning(move |_, _| skel.allocate());
//!
//! let event_alloc = MockPointerAllocator::<SkeletonEventBase>::new();
//! let evt = event_alloc.clone();
//! mock.expect_get_event_from_skeleton()
//!     .in_sequence(&mut seq)
//!     .returning(move |_, _, _| evt.allocate());
//!
//! // Enforces: create_skeleton must be called before get_event_from_skeleton
//! ```
//!
//! This mock back-end is used for unit testing of Lola-Runtime.

use bridge_ffi_rs::
    FatPtr, FindServiceHandle, HandleType, InstanceSpecifier, NativeInstanceSpecifier, ProxyBase,
    ProxyEventBase, SkeletonBase, SkeletonEventBase, TypeOperationsManager,
};
use mockall::mock;

// Internal mockall-generated mock (without the lifetime method)
mock! {
    // This is type name for mock macro, it is not the same as FFIBridge trait,
    // This will produce a struct named MockFFIBridge which implements FFIBridge trait.
    #[derive(Debug)]
    pub FFIBridge {}

    impl Clone for FFIBridge {
        fn clone(&self) -> Self;
    }
    // SAFETY comment for unsafe function already provided in the trait definition, it is impl block of that trait, so we don't need to repeat it here.
    impl bridge_ffi_rs::FFIBridge for FFIBridge {
        unsafe fn get_allocatee_ptr(
            &self,
            event_ptr: *mut SkeletonEventBase,
            allocatee_ptr: *mut std::ffi::c_void,
            event_type: &str,
        ) -> bool;

        unsafe fn delete_allocatee_ptr(&self, allocatee_ptr: *mut std::ffi::c_void, type_ops: &TypeOperationsManager);

        unsafe fn get_allocatee_data_ptr(
            &self,
            allocatee_ptr: *const std::ffi::c_void,
            type_ops: &TypeOperationsManager,
        ) -> *mut std::ffi::c_void;

        unsafe fn skeleton_event_send_sample_allocatee(
            &self,
            event_ptr: *mut SkeletonEventBase,
            type_ops: &TypeOperationsManager,
            allocatee_ptr: *const std::ffi::c_void,
        ) -> bool;

        unsafe fn sample_ptr_get(
            &self,
            sample_ptr: *const std::ffi::c_void,
            type_ops: &TypeOperationsManager,
        ) -> *const std::ffi::c_void;

        unsafe fn sample_ptr_delete(&self, sample_ptr: *mut std::ffi::c_void, type_ops: &TypeOperationsManager);

        unsafe fn skeleton_offer_service(&self, skeleton_ptr: *mut SkeletonBase) -> bool;

        unsafe fn skeleton_stop_offer_service(&self, skeleton_ptr: *mut SkeletonBase);

        unsafe fn create_proxy(&self, interface_id: &str, handle_ptr: &HandleType) -> *mut ProxyBase;

        unsafe fn create_skeleton(
            &self,
            interface_id: &str,
            instance_spec: *const NativeInstanceSpecifier,
        ) -> *mut SkeletonBase;

        unsafe fn destroy_proxy(&self, proxy_ptr: *mut ProxyBase);

        unsafe fn destroy_skeleton(&self, skeleton_ptr: *mut SkeletonBase);

        unsafe fn get_event_from_proxy(
            &self,
            proxy_ptr: *mut ProxyBase,
            interface_id: &str,
            event_id: &str,
        ) -> *mut ProxyEventBase;

        unsafe fn get_event_from_skeleton(
            &self,
            skeleton_ptr: *mut SkeletonBase,
            interface_id: &str,
            event_id: &str,
        ) -> *mut SkeletonEventBase;

        unsafe fn get_samples_from_event(
            &self,
            event_ptr: *mut ProxyEventBase,
            type_ops: &TypeOperationsManager,
            callback: &FatPtr,
            max_samples: u32,
        ) -> u32;

        unsafe fn skeleton_send_event(
            &self,
            event_ptr: *mut SkeletonEventBase,
            type_ops: &TypeOperationsManager,
            data_ptr: *const std::ffi::c_void,
        ) -> bool;

        unsafe fn subscribe_to_event(
            &self,
            event_ptr: *mut ProxyEventBase,
            max_sample_count: u32,
        ) -> bool;

        unsafe fn unsubscribe_to_event(&self, event_ptr: *mut ProxyEventBase);

        unsafe fn set_event_receive_handler(
            &self,
            proxy_event_ptr: *mut ProxyEventBase,
            handler: &FatPtr,
        ) -> bool;

        unsafe fn clear_event_receive_handler(
            &self,
            proxy_event_ptr: *mut ProxyEventBase,
        );

        unsafe fn start_find_service(
            &self,
            callback: &FatPtr,
            instance_spec: InstanceSpecifier,
        ) -> *mut FindServiceHandle;

        unsafe fn stop_find_service(&self, handle: *mut FindServiceHandle);

        unsafe fn get_type_ops_instance(
        interface_id: &str,
        member_name: &str,) -> Option<TypeOperationsManager>;
}
}

/// A shared wrapper around MockFFIBridge that automatically shares expectations across clones.
///
/// This wrapper uses Arc<Mutex<MockFFIBridge>> internally, allowing all clones to share
/// the same underlying mock instance and its expectations. This eliminates the need for
/// complex clone expectation setup in tests.
#[derive(Debug, Default)]
pub struct SharedMockBridge(std::sync::Arc<std::sync::Mutex<MockFFIBridge>>);

impl SharedMockBridge {
    /// Create a new SharedMockBridge wrapping the given MockFFIBridge
    pub fn new(mock: MockFFIBridge) -> Self {
        Self(std::sync::Arc::new(std::sync::Mutex::new(mock)))
    }

    /// Access the inner MockFFIBridge for verification and assertions.
    ///
    /// This is useful for calling mockall's methods like `assert` on the inner mock after the test actions.
    pub fn with_mock<F, R>(&self, f: F) -> R
    where
        F: FnOnce(&mut MockFFIBridge) -> R,
    {
        let mut guard = self.0.lock().expect("Failed to acquire lock");
        f(&mut *guard)
    }

    fn locked(&self) -> std::sync::MutexGuard<'_, MockFFIBridge> {
        self.0.lock().expect("Failed to acquire lock")
    }
}

impl Clone for SharedMockBridge {
    fn clone(&self) -> Self {
        Self(std::sync::Arc::clone(&self.0))
    }
}

// Implement FFIBridge by forwarding all calls to the inner MockFFIBridge
// SAFETY comment for unsafe function already provided in the trait definition, it is impl block of that trait, so we don't need to repeat it here.
impl bridge_ffi_rs::FFIBridge for SharedMockBridge {
    unsafe fn get_allocatee_ptr(
        &self,
        event_ptr: *mut SkeletonEventBase,
        allocatee_ptr: *mut std::ffi::c_void,
        event_type: &str,
    ) -> bool {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.locked()
                .get_allocatee_ptr(event_ptr, allocatee_ptr, event_type)
        }
    }

    unsafe fn delete_allocatee_ptr(&self, allocatee_ptr: *mut std::ffi::c_void, type_ops: &TypeOperationsManager) {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe { self.locked().delete_allocatee_ptr(allocatee_ptr, type_ops) }
    }

    unsafe fn get_allocatee_data_ptr(
        &self,
        allocatee_ptr: *const std::ffi::c_void,
        type_ops: &TypeOperationsManager,
    ) -> *mut std::ffi::c_void {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.locked()
                .get_allocatee_data_ptr(allocatee_ptr, type_ops)
        }
    }

    unsafe fn skeleton_event_send_sample_allocatee(
        &self,
        event_ptr: *mut SkeletonEventBase,
        type_ops: &TypeOperationsManager,
        allocatee_ptr: *const std::ffi::c_void,
    ) -> bool {
        unsafe {
            self.locked()
                .skeleton_event_send_sample_allocatee(event_ptr, type_ops, allocatee_ptr)
        }
    }

    unsafe fn sample_ptr_get(
        &self,
        sample_ptr: *const std::ffi::c_void,
        type_ops: &TypeOperationsManager,
    ) -> *const std::ffi::c_void {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe { self.locked().sample_ptr_get(sample_ptr, type_ops) }
    }

    unsafe fn sample_ptr_delete(&self, sample_ptr: *mut std::ffi::c_void, type_ops: &TypeOperationsManager) {
        unsafe { self.locked().sample_ptr_delete(sample_ptr, type_ops) }
    }

    unsafe fn skeleton_offer_service(&self, skeleton_ptr: *mut SkeletonBase) -> bool {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe { self.locked().skeleton_offer_service(skeleton_ptr) }
    }

    unsafe fn skeleton_stop_offer_service(&self, skeleton_ptr: *mut SkeletonBase) {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe { self.locked().skeleton_stop_offer_service(skeleton_ptr) }
    }

    unsafe fn create_proxy(&self, interface_id: &str, handle_ptr: &HandleType) -> *mut ProxyBase {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe { self.locked().create_proxy(interface_id, handle_ptr) }
    }

    unsafe fn create_skeleton(
        &self,
        interface_id: &str,
        instance_spec: *const NativeInstanceSpecifier,
    ) -> *mut SkeletonBase {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe { self.locked().create_skeleton(interface_id, instance_spec) }
    }

    unsafe fn destroy_proxy(&self, proxy_ptr: *mut ProxyBase) {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe { self.locked().destroy_proxy(proxy_ptr) }
    }

    unsafe fn destroy_skeleton(&self, skeleton_ptr: *mut SkeletonBase) {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe { self.locked().destroy_skeleton(skeleton_ptr) }
    }

    unsafe fn get_event_from_proxy(
        &self,
        proxy_ptr: *mut ProxyBase,
        interface_id: &str,
        event_id: &str,
    ) -> *mut ProxyEventBase {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.locked()
                .get_event_from_proxy(proxy_ptr, interface_id, event_id)
        }
    }

    unsafe fn get_event_from_skeleton(
        &self,
        skeleton_ptr: *mut SkeletonBase,
        interface_id: &str,
        event_id: &str,
    ) -> *mut SkeletonEventBase {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.locked()
                .get_event_from_skeleton(skeleton_ptr, interface_id, event_id)
        }
    }

    unsafe fn subscribe_to_event(
        &self,
        event_ptr: *mut ProxyEventBase,
        max_num_samples: u32,
    ) -> bool {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe { self.locked().subscribe_to_event(event_ptr, max_num_samples) }
    }

    unsafe fn unsubscribe_to_event(&self, event_ptr: *mut ProxyEventBase) {
        unsafe { self.locked().unsubscribe_to_event(event_ptr) }
    }

    unsafe fn get_samples_from_event(
        &self,
        event_ptr: *mut ProxyEventBase,
        type_ops: &TypeOperationsManager,
        callback: &FatPtr,
        max_num_samples: u32,
    ) -> u32 {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.locked()
                .get_samples_from_event(event_ptr, type_ops, callback, max_num_samples)
        }
    }

    unsafe fn skeleton_send_event(
        &self,
        event_ptr: *mut SkeletonEventBase,
        type_ops: &TypeOperationsManager,
        data_ptr: *const std::ffi::c_void,
    ) -> bool {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.locked()
                .skeleton_send_event(event_ptr, type_ops, data_ptr)
        }
    }

    unsafe fn set_event_receive_handler(
        &self,
        event_ptr: *mut ProxyEventBase,
        callback: &FatPtr,
    ) -> bool {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.locked()
                .set_event_receive_handler(event_ptr, callback)
        }
    }

    unsafe fn clear_event_receive_handler(&self, event_ptr: *mut ProxyEventBase) {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.locked()
                .clear_event_receive_handler(event_ptr)
        }
    }

    unsafe fn start_find_service(
        &self,
        callback: &FatPtr,
        instance_spec: InstanceSpecifier,
    ) -> *mut FindServiceHandle {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe { self.locked().start_find_service(callback, instance_spec) }
    }

    unsafe fn stop_find_service(&self, find_service_handle: *mut FindServiceHandle) {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe { self.locked().stop_find_service(find_service_handle) }
    }

    unsafe fn get_type_ops_instance(
        interface_id: &str,
        member_name: &str,
    ) -> Option<TypeOperationsManager> {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.locked()
                .get_type_ops_instance(interface_id, member_name)
        }
    }
}

/// Helper for managing mock pointer allocations with automatic cleanup tracking,
/// eliminating the need for repetitive Arc<Mutex<Vec<Box<T>>>> patterns in tests.
/// We can use for any FFI pointer type (e.g., SkeletonBase, ProxyBase) by specifying T.
/// The allocator tracks all allocated pointers and provides a `free` method to clean them up,
/// which can be used in test assertions to ensure proper resource management.
#[derive(Debug)]
pub struct MockPointerAllocator<T> {
    allocations: std::sync::Arc<std::sync::Mutex<Vec<Box<T>>>>,
}

impl<T: Default> MockPointerAllocator<T> {
    /// Create a new pointer allocator for type `T`.
    pub fn new() -> Self {
        Self {
            allocations: std::sync::Arc::new(std::sync::Mutex::new(Vec::new())),
        }
    }

    fn locked(&self) -> std::sync::MutexGuard<'_, Vec<Box<T>>> {
        self.allocations.lock().expect("Failed to acquire lock")
    }

    /// Allocate a new heap-backed pointer with default value.
    ///
    /// # Returns
    /// A raw mutable pointer to a heap-allocated `T` with default value.
    pub fn allocate(&self) -> *mut T {
        let mut allocs = self.locked();
        allocs.push(Box::default());
        allocs
            .last_mut()
            .expect("Failed to allocate pointer")
            .as_mut() as *mut T
    }

    /// Free a previously allocated pointer.
    ///
    /// # Parameters
    /// - `ptr`: The raw pointer to free, previously returned by `allocate()`
    ///
    /// # Returns
    /// - `true` if the pointer was found and freed
    /// - `false` if the pointer was not found in tracked allocations
    pub fn free(&self, ptr: *mut T) -> bool {
        let mut allocs = self.locked();
        allocs
            .extract_if(.., |b| b.as_mut() as *mut T == ptr)
            .next()
            .is_some()
    }

    /// Assert that all allocations have been freed (count is 0).
    ///
    /// # Panics
    /// Panics if there are still tracked allocations.
    ///
    /// # Example
    /// ```ignore
    /// allocator.assert_all_freed(); // Instead of assert_eq!(allocator.count(), 0, ...)
    /// ```
    pub fn assert_all_freed(&self) {
        let count = self.locked().len();
        assert_eq!(
            count,
            0,
            "memory leak: {} pointer(s) of type {} not freed",
            count,
            std::any::type_name::<T>()
        );
    }
}

impl<T: Default> Clone for MockPointerAllocator<T> {
    fn clone(&self) -> Self {
        Self {
            allocations: std::sync::Arc::clone(&self.allocations),
        }
    }
}

impl<T: Default> Default for MockPointerAllocator<T> {
    fn default() -> Self {
        Self::new()
    }
}
