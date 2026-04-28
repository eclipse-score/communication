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

// This file provides a mock implementation of the FFIBridge trait for testing purposes.
// It is kind of stub code which we will change according to the needs of our tests.
// As of now, it just provide basic mock implementation and returning values based on
// input parameters. In future, we will enhance this mock implementation to verify
// all the test cases and scenarios.
#![doc(hidden)]

pub use bridge_ffi_rs::{
    CMutVoidPtr, CVoidPtr, FFIBridge, FatPtr, FindServiceHandle, HandleContainer, HandleType,
    InstanceSpecifier, NativeFindServiceHandle, NativeHandleContainer, NativeInstanceSpecifier,
    ProxyBase, ProxyEventBase, ProxyWrapperClass, SkeletonBase, SkeletonEventBase,
};

#[derive(Debug, Clone)]
pub struct MockFFIBridge;

impl bridge_ffi_rs::FFIBridge for MockFFIBridge {
    /// Mock implementation of get_allocatee_ptr.
    /// # Safety
    /// Returns true if event_ptr is non-null (mock behavior).
    unsafe fn get_allocatee_ptr(
        event_ptr: *mut SkeletonEventBase,
        _allocatee_ptr: *mut std::ffi::c_void,
        _event_type: &str,
    ) -> bool {
        !event_ptr.is_null()
    }

    /// Mock implementation of delete_allocatee_ptr.
    /// # Safety
    /// No-op for mock.
    unsafe fn delete_allocatee_ptr(_allocatee_ptr: *mut std::ffi::c_void, _type_name: &str) {}

    /// Mock implementation of get_allocatee_data_ptr.
    /// # Safety
    /// Returns the input pointer cast to mutable (mock behavior).
    unsafe fn get_allocatee_data_ptr(
        allocatee_ptr: *const std::ffi::c_void,
        _type_name: &str,
    ) -> *mut std::ffi::c_void {
        allocatee_ptr as *mut std::ffi::c_void
    }

    /// Mock implementation of skeleton_event_send_sample_allocatee.
    /// # Safety
    /// Returns true if event_ptr is non-null (mock behavior).
    unsafe fn skeleton_event_send_sample_allocatee(
        event_ptr: *mut SkeletonEventBase,
        _event_type: &str,
        _allocatee_ptr: *const std::ffi::c_void,
    ) -> bool {
        !event_ptr.is_null()
    }

    /// Mock implementation of sample_ptr_get.
    /// # Safety
    /// Returns the input pointer unchanged (mock behavior).
    unsafe fn sample_ptr_get(
        sample_ptr: *const std::ffi::c_void,
        _type_name: &str,
    ) -> *const std::ffi::c_void {
        sample_ptr
    }

    /// Mock implementation of sample_ptr_delete.
    /// # Safety
    /// No-op for mock.
    unsafe fn sample_ptr_delete(_sample_ptr: *mut std::ffi::c_void, _type_name: &str) {}

    /// Mock implementation of skeleton_offer_service.
    /// # Safety
    /// Returns true if skeleton_ptr is non-null (mock behavior).
    unsafe fn skeleton_offer_service(skeleton_ptr: *mut SkeletonBase) -> bool {
        !skeleton_ptr.is_null()
    }

    /// Mock implementation of skeleton_stop_offer_service.
    /// # Safety
    /// No-op for mock.
    unsafe fn skeleton_stop_offer_service(_skeleton_ptr: *mut SkeletonBase) {}

    /// Mock implementation of create_proxy.
    /// # Safety
    /// Allocates and returns a new ProxyBase pointer from heap.
    /// Caller must eventually destroy with destroy_proxy.
    unsafe fn create_proxy(_interface_id: &str, _handle_ptr: &HandleType) -> *mut ProxyBase {
        Box::into_raw(Box::new(std::mem::zeroed::<ProxyBase>()))
    }

    /// Mock implementation of create_skeleton.
    /// # Safety
    /// Allocates and returns a new SkeletonBase pointer from heap.
    /// Caller must eventually destroy with destroy_skeleton.
    unsafe fn create_skeleton(
        _interface_id: &str,
        _instance_spec: *const NativeInstanceSpecifier,
    ) -> *mut SkeletonBase {
        Box::into_raw(Box::new(std::mem::zeroed::<SkeletonBase>()))
    }

    /// Mock implementation of destroy_proxy.
    /// # Safety
    /// proxy_ptr must have been returned from create_proxy.
    /// Caller must not use proxy_ptr after this call.
    unsafe fn destroy_proxy(proxy_ptr: *mut ProxyBase) {
        if !proxy_ptr.is_null() {
            // SAFETY: proxy_ptr was allocated by create_proxy via Box::into_raw
            drop(Box::from_raw(proxy_ptr));
        }
    }

    /// Mock implementation of destroy_skeleton.
    /// # Safety
    /// skeleton_ptr must have been returned from create_skeleton.
    /// Caller must not use skeleton_ptr after this call.
    unsafe fn destroy_skeleton(skeleton_ptr: *mut SkeletonBase) {
        if !skeleton_ptr.is_null() {
            // SAFETY: skeleton_ptr was allocated by create_skeleton via Box::into_raw
            drop(Box::from_raw(skeleton_ptr));
        }
    }

    /// Mock implementation of get_event_from_proxy.
    /// # Safety
    /// proxy_ptr must be non-null for non-null return.
    /// Returned pointer remains valid only while proxy is alive.
    unsafe fn get_event_from_proxy(
        proxy_ptr: *mut ProxyBase,
        _interface_id: &str,
        _event_id: &str,
    ) -> *mut ProxyEventBase {
        if proxy_ptr.is_null() {
            return std::ptr::null_mut();
        }
        // ProxyEventBase is a ZST opaque type. The runtime never frees event pointers
        // (NativeProxyEventBase has no Drop); returning a non-null sentinel is sufficient.
        std::ptr::NonNull::<ProxyEventBase>::dangling().as_ptr()
    }

    /// Mock implementation of get_event_from_skeleton.
    /// # Safety
    /// Returns a dangling pointer sentinel (mock behavior). Valid only for null checks.
    unsafe fn get_event_from_skeleton(
        _skeleton_ptr: *mut SkeletonBase,
        _interface_id: &str,
        _event_id: &str,
    ) -> *mut SkeletonEventBase {
        // SkeletonEventBase is a ZST opaque type. The runtime never frees event pointers
        // (NativeSkeletonEventBase has no Drop); returning a non-null sentinel is sufficient.
        std::ptr::NonNull::<SkeletonEventBase>::dangling().as_ptr()
    }

    /// Mock implementation of get_samples_from_event.
    /// # Safety
    /// event_ptr must be non-null or returns u32::MAX (mock behavior).
    unsafe fn get_samples_from_event(
        event_ptr: *mut ProxyEventBase,
        _event_type: &str,
        _callback: &FatPtr,
        _max_samples: u32,
    ) -> u32 {
        if event_ptr.is_null() {
            return u32::MAX;
        }
        0
    }

    /// Mock implementation of skeleton_send_event.
    /// # Safety
    /// Returns true if event_ptr is non-null (mock behavior).
    unsafe fn skeleton_send_event(
        event_ptr: *mut SkeletonEventBase,
        _event_type: &str,
        _data_ptr: *const std::ffi::c_void,
    ) -> bool {
        !event_ptr.is_null()
    }

    /// Mock implementation of subscribe_to_event.
    /// # Safety
    /// Returns true if event_ptr is non-null (mock behavior).
    unsafe fn subscribe_to_event(event_ptr: *mut ProxyEventBase, _max_sample_count: u32) -> bool {
        !event_ptr.is_null()
    }

    /// Mock implementation of unsubscribe_to_event.
    /// # Safety
    /// No-op for mock.
    unsafe fn unsubscribe_to_event(_event_ptr: *mut ProxyEventBase) {}

    /// Mock implementation of set_event_receive_handler.
    /// # Safety
    /// Returns true if proxy_event_ptr is non-null (mock behavior).
    unsafe fn set_event_receive_handler(
        proxy_event_ptr: *mut ProxyEventBase,
        _handler: &FatPtr,
        _event_type: &str,
    ) -> bool {
        !proxy_event_ptr.is_null()
    }

    /// Mock implementation of clear_event_receive_handler.
    /// # Safety
    /// No-op for mock.
    unsafe fn clear_event_receive_handler(
        _proxy_event_ptr: *mut ProxyEventBase,
        _event_type: &str,
    ) {
    }

    /// Mock implementation of start_find_service.
    /// # Safety
    /// Allocates and returns a new FindServiceHandle pointer.
    /// Caller must eventually destroy with stop_find_service.
    unsafe fn start_find_service(
        _callback: &FatPtr,
        _instance_spec: InstanceSpecifier,
    ) -> *mut FindServiceHandle {
        Box::into_raw(Box::new(std::mem::zeroed::<FindServiceHandle>()))
    }

    /// Mock implementation of stop_find_service.
    /// # Safety
    /// handle must have been returned from start_find_service.
    /// Caller must not use handle after this call.
    unsafe fn stop_find_service(handle: *mut FindServiceHandle) {
        if !handle.is_null() {
            // SAFETY: handle was allocated by start_find_service via Box::into_raw
            drop(Box::from_raw(handle));
        }
    }
}

impl MockFFIBridge {
    /// The handle container is backed by a heap-allocated zeroed stub so the pointer
    /// is stable and non-dangling.  An extra `Arc` clone is intentionally forgotten
    /// inside this function so the strong count is permanently pinned above zero —
    /// meaning `HandleContainer::drop` (which calls real C++) is never invoked
    /// regardless of how many clones the caller makes or drops.
    pub fn make_handle_container() -> std::sync::Arc<HandleContainer> {
        // A zeroed usize gives a stable heap pointer.
        let ptr = Box::leak(Box::new(0usize)) as *mut usize as *mut NativeHandleContainer;
        let hc = std::sync::Arc::new(HandleContainer::new(ptr));
        //To avoid the drop of HandleContainer which will call the real C++ destructor.
        std::mem::forget(std::sync::Arc::clone(&hc));
        hc
    }

    /// `ProxyEventBase` is a ZST opaque type, a dangling non-null pointer is the
    /// canonical representation for "valid but empty" in the mock.
    pub fn make_proxy_event_ptr() -> *mut ProxyEventBase {
        std::ptr::NonNull::<ProxyEventBase>::dangling().as_ptr()
    }
}
