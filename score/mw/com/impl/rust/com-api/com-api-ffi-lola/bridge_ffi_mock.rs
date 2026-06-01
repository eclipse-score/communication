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
//! The SharedMockBridge wrapper allows all clones to share the same underlying mock instance and expectations,
//! eliminating the need for complex clone expectation setup in tests.
//! Tests can create a SharedMockBridge with a configured MockFFIBridge, and all clones will automatically share the same expectations.
//! Example usage in a test:
//! ```rust,ignore
//! let mut mock = MockFFIBridge::new();
//! mock.expect_create_proxy()
//!     .returning(|_, _| Box::into_raw(Box::new(unsafe { std::mem::zeroed() }))); // this is just an example, configure expectations as needed
//! let bridge = SharedMockBridge::new(mock);
//! let clone1 = bridge.clone();
//! let clone2 = bridge.clone();
//! ```
//!
//! This mock back-end is used for unit testing of Lola-Runtime.

use bridge_ffi_rs::{
    FatPtr, FindServiceHandle, HandleType, InstanceSpecifier, NativeInstanceSpecifier, ProxyBase,
    ProxyEventBase, SkeletonBase, SkeletonEventBase,
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

        unsafe fn delete_allocatee_ptr(&self, allocatee_ptr: *mut std::ffi::c_void, type_name: &str);

        unsafe fn get_allocatee_data_ptr(
            &self,
            allocatee_ptr: *const std::ffi::c_void,
            type_name: &str,
        ) -> *mut std::ffi::c_void;

        unsafe fn skeleton_event_send_sample_allocatee(
            &self,
            event_ptr: *mut SkeletonEventBase,
            event_type: &str,
            allocatee_ptr: *const std::ffi::c_void,
        ) -> bool;

        unsafe fn sample_ptr_get(
            &self,
            sample_ptr: *const std::ffi::c_void,
            type_name: &str,
        ) -> *const std::ffi::c_void;

        unsafe fn sample_ptr_delete(&self, sample_ptr: *mut std::ffi::c_void, type_name: &str);

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
            event_type: &str,
            callback: &FatPtr,
            max_samples: u32,
        ) -> u32;

        unsafe fn skeleton_send_event(
            &self,
            event_ptr: *mut SkeletonEventBase,
            event_type: &str,
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
            event_type: &str,
        ) -> bool;

        unsafe fn clear_event_receive_handler(
            &self,
            proxy_event_ptr: *mut ProxyEventBase,
            event_type: &str,
        );

        unsafe fn start_find_service(
            &self,
            callback: &FatPtr,
            instance_spec: InstanceSpecifier,
        ) -> *mut FindServiceHandle;

        unsafe fn stop_find_service(&self, handle: *mut FindServiceHandle);
}
}

/// A shared wrapper around MockFFIBridge that automatically shares expectations across clones.
///
/// This wrapper uses Arc<Mutex<MockFFIBridge>> internally, allowing all clones to share
/// the same underlying mock instance and its expectations. This eliminates the need for
/// complex clone expectation setup in tests.
///
/// # Usage
/// ```rust,ignore
/// let mut mock = MockFFIBridge::new();
/// mock.expect_create_proxy()
///     .returning(|_, _| Box::into_raw(Box::new(unsafe { std::mem::zeroed() })));
///
/// let bridge = SharedMockBridge::new(mock);
/// // All clones automatically share the same expectations
/// let clone1 = bridge.clone();
/// let clone2 = bridge.clone();
/// ```
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
            self.0
                .lock()
                .expect("Failed to acquire lock")
                .get_allocatee_ptr(event_ptr, allocatee_ptr, event_type)
        }
    }

    unsafe fn delete_allocatee_ptr(&self, allocatee_ptr: *mut std::ffi::c_void, type_name: &str) {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.0
                .lock()
                .expect("Failed to acquire lock")
                .delete_allocatee_ptr(allocatee_ptr, type_name)
        }
    }

    unsafe fn get_allocatee_data_ptr(
        &self,
        allocatee_ptr: *const std::ffi::c_void,
        type_name: &str,
    ) -> *mut std::ffi::c_void {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.0
                .lock()
                .expect("Failed to acquire lock")
                .get_allocatee_data_ptr(allocatee_ptr, type_name)
        }
    }

    unsafe fn skeleton_event_send_sample_allocatee(
        &self,
        event_ptr: *mut SkeletonEventBase,
        event_type: &str,
        allocatee_ptr: *const std::ffi::c_void,
    ) -> bool {
        unsafe {
            self.0
                .lock()
                .expect("Failed to acquire lock")
                .skeleton_event_send_sample_allocatee(event_ptr, event_type, allocatee_ptr)
        }
    }

    unsafe fn sample_ptr_get(
        &self,
        sample_ptr: *const std::ffi::c_void,
        type_name: &str,
    ) -> *const std::ffi::c_void {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.0
                .lock()
                .expect("Failed to acquire lock")
                .sample_ptr_get(sample_ptr, type_name)
        }
    }

    unsafe fn sample_ptr_delete(&self, sample_ptr: *mut std::ffi::c_void, type_name: &str) {
        unsafe {
            self.0
                .lock()
                .expect("Failed to acquire lock")
                .sample_ptr_delete(sample_ptr, type_name)
        }
    }

    unsafe fn skeleton_offer_service(&self, skeleton_ptr: *mut SkeletonBase) -> bool {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.0
                .lock()
                .expect("Failed to acquire lock")
                .skeleton_offer_service(skeleton_ptr)
        }
    }

    unsafe fn skeleton_stop_offer_service(&self, skeleton_ptr: *mut SkeletonBase) {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.0
                .lock()
                .expect("Failed to acquire lock")
                .skeleton_stop_offer_service(skeleton_ptr)
        }
    }

    unsafe fn create_proxy(&self, interface_id: &str, handle_ptr: &HandleType) -> *mut ProxyBase {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.0
                .lock()
                .expect("Failed to acquire lock")
                .create_proxy(interface_id, handle_ptr)
        }
    }

    unsafe fn create_skeleton(
        &self,
        interface_id: &str,
        instance_spec: *const NativeInstanceSpecifier,
    ) -> *mut SkeletonBase {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.0
                .lock()
                .expect("Failed to acquire lock")
                .create_skeleton(interface_id, instance_spec)
        }
    }

    unsafe fn destroy_proxy(&self, proxy_ptr: *mut ProxyBase) {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.0
                .lock()
                .expect("Failed to acquire lock")
                .destroy_proxy(proxy_ptr)
        }
    }

    unsafe fn destroy_skeleton(&self, skeleton_ptr: *mut SkeletonBase) {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.0
                .lock()
                .expect("Failed to acquire lock")
                .destroy_skeleton(skeleton_ptr)
        }
    }

    unsafe fn get_event_from_proxy(
        &self,
        proxy_ptr: *mut ProxyBase,
        interface_id: &str,
        event_id: &str,
    ) -> *mut ProxyEventBase {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.0
                .lock()
                .expect("Failed to acquire lock")
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
            self.0
                .lock()
                .expect("Failed to acquire lock")
                .get_event_from_skeleton(skeleton_ptr, interface_id, event_id)
        }
    }

    unsafe fn subscribe_to_event(
        &self,
        event_ptr: *mut ProxyEventBase,
        max_num_samples: u32,
    ) -> bool {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.0
                .lock()
                .expect("Failed to acquire lock")
                .subscribe_to_event(event_ptr, max_num_samples)
        }
    }

    unsafe fn unsubscribe_to_event(&self, event_ptr: *mut ProxyEventBase) {
        unsafe {
            self.0
                .lock()
                .expect("Failed to acquire lock")
                .unsubscribe_to_event(event_ptr)
        }
    }

    unsafe fn get_samples_from_event(
        &self,
        event_ptr: *mut ProxyEventBase,
        event_type: &str,
        callback: &FatPtr,
        max_num_samples: u32,
    ) -> u32 {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.0
                .lock()
                .expect("Failed to acquire lock")
                .get_samples_from_event(event_ptr, event_type, callback, max_num_samples)
        }
    }

    unsafe fn skeleton_send_event(
        &self,
        event_ptr: *mut SkeletonEventBase,
        event_type: &str,
        data_ptr: *const std::ffi::c_void,
    ) -> bool {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.0
                .lock()
                .expect("Failed to acquire lock")
                .skeleton_send_event(event_ptr, event_type, data_ptr)
        }
    }

    unsafe fn set_event_receive_handler(
        &self,
        event_ptr: *mut ProxyEventBase,
        callback: &FatPtr,
        event_type: &str,
    ) -> bool {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.0
                .lock()
                .expect("Failed to acquire lock")
                .set_event_receive_handler(event_ptr, callback, event_type)
        }
    }

    unsafe fn clear_event_receive_handler(&self, event_ptr: *mut ProxyEventBase, event_type: &str) {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.0
                .lock()
                .expect("Failed to acquire lock")
                .clear_event_receive_handler(event_ptr, event_type)
        }
    }

    unsafe fn start_find_service(
        &self,
        callback: &FatPtr,
        instance_spec: InstanceSpecifier,
    ) -> *mut FindServiceHandle {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.0
                .lock()
                .expect("Failed to acquire lock")
                .start_find_service(callback, instance_spec)
        }
    }

    unsafe fn stop_find_service(&self, find_service_handle: *mut FindServiceHandle) {
        //Safety: This is just forwarding the call to the inner mock, which is expected to be configured correctly in tests using mockall's expectations.
        unsafe {
            self.0
                .lock()
                .expect("Failed to acquire lock")
                .stop_find_service(find_service_handle)
        }
    }
}
