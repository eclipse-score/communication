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
#![allow(clippy::missing_safety_doc)]

use bridge_ffi_rs::{
    FatPtr, FindServiceHandle, HandleContainer, HandleType, InstanceSpecifier,
    NativeHandleContainer, NativeInstanceSpecifier, ProxyBase, ProxyEventBase, SkeletonBase,
    SkeletonEventBase,
};
use std::cell::RefCell;

// Type alias for the heap-pointer + drop-glue pair stored in backing thread-locals.
type BackingEntry = (*mut std::ffi::c_void, fn(*mut std::ffi::c_void));

// The thread-local storage is used to hold the backing data for the mock FFI bridge.
thread_local! {
    //DATA_BACKING holds a pointer to the heap-allocated backing data and a drop function to free it.
    //We are using this when user calls get_allocatee_data_ptr to return a pointer to this backing data.
    static DATA_BACKING: RefCell<Option<BackingEntry>> = RefCell::new(None);
    //ALLOC_SIZE holds the size of the allocatee type for the next get_allocatee_ptr call, allowing
    //the mock to zero-fill the caller's slot correctly.
    static ALLOC_SIZE: RefCell<usize> = const { RefCell::new(0) };
    //SAMPLE_BACKING holds a pointer to the heap-allocated sample data and a drop function to free it.
    static SAMPLE_BACKING: RefCell<Option<BackingEntry>> = RefCell::new(None);
}

#[derive(Debug, Clone)]
pub struct MockFFIBridge;

impl bridge_ffi_rs::FFIBridge for MockFFIBridge {
    unsafe fn get_allocatee_ptr(
        event_ptr: *mut SkeletonEventBase,
        allocatee_ptr: *mut std::ffi::c_void,
        _event_type: &str,
    ) -> bool {
        if event_ptr.is_null() {
            return false;
        }
        let size = ALLOC_SIZE.with(|s| *s.borrow());
        if size == 0 {
            return false;
        }
        std::ptr::write_bytes(allocatee_ptr as *mut u8, 0, size);
        true
    }

    unsafe fn delete_allocatee_ptr(_allocatee_ptr: *mut std::ffi::c_void, _type_name: &str) {
        DATA_BACKING.with(|d| {
            if let Some((ptr, drop_fn)) = d.borrow_mut().take() {
                drop_fn(ptr);
            }
        });
        ALLOC_SIZE.with(|s| *s.borrow_mut() = 0);
    }

    unsafe fn get_allocatee_data_ptr(
        _allocatee_ptr: *const std::ffi::c_void,
        _type_name: &str,
    ) -> *mut std::ffi::c_void {
        DATA_BACKING.with(|d| {
            d.borrow()
                .as_ref()
                .map(|(ptr, _)| *ptr)
                .unwrap_or(std::ptr::null_mut())
        })
    }

    unsafe fn skeleton_event_send_sample_allocatee(
        event_ptr: *mut SkeletonEventBase,
        _event_type: &str,
        _allocatee_ptr: *const std::ffi::c_void,
    ) -> bool {
        !event_ptr.is_null()
    }

    unsafe fn sample_ptr_get(
        _sample_ptr: *const std::ffi::c_void,
        _type_name: &str,
    ) -> *const std::ffi::c_void {
        SAMPLE_BACKING.with(|s| {
            s.borrow()
                .as_ref()
                .map(|(ptr, _)| *ptr as *const std::ffi::c_void)
                .unwrap_or(std::ptr::null())
        })
    }

    unsafe fn sample_ptr_delete(_sample_ptr: *mut std::ffi::c_void, _type_name: &str) {
        SAMPLE_BACKING.with(|s| {
            if let Some((ptr, drop_fn)) = s.borrow_mut().take() {
                drop_fn(ptr);
            }
        });
    }

    unsafe fn skeleton_offer_service(skeleton_ptr: *mut SkeletonBase) -> bool {
        !skeleton_ptr.is_null()
    }

    unsafe fn skeleton_stop_offer_service(_skeleton_ptr: *mut SkeletonBase) {}

    unsafe fn create_proxy(_interface_id: &str, _handle_ptr: &HandleType) -> *mut ProxyBase {
        Box::into_raw(Box::new(std::mem::zeroed::<ProxyBase>()))
    }

    unsafe fn create_skeleton(
        _interface_id: &str,
        _instance_spec: *const NativeInstanceSpecifier,
    ) -> *mut SkeletonBase {
        Box::into_raw(Box::new(std::mem::zeroed::<SkeletonBase>()))
    }

    unsafe fn destroy_proxy(proxy_ptr: *mut ProxyBase) {
        if !proxy_ptr.is_null() {
            // SAFETY: proxy_ptr was allocated by create_proxy via Box::into_raw
            drop(Box::from_raw(proxy_ptr));
        }
    }

    unsafe fn destroy_skeleton(skeleton_ptr: *mut SkeletonBase) {
        if !skeleton_ptr.is_null() {
            // SAFETY: skeleton_ptr was allocated by create_skeleton via Box::into_raw
            drop(Box::from_raw(skeleton_ptr));
        }
    }

    unsafe fn get_event_from_proxy(
        proxy_ptr: *mut ProxyBase,
        _interface_id: &str,
        _event_id: &str,
    ) -> *mut ProxyEventBase {
        if proxy_ptr.is_null() {
            return std::ptr::null_mut();
        }
        // ProxyEventBase is a ZST opaque type; a non-null sentinel is sufficient.
        std::ptr::NonNull::<ProxyEventBase>::dangling().as_ptr()
    }

    unsafe fn get_event_from_skeleton(
        _skeleton_ptr: *mut SkeletonBase,
        _interface_id: &str,
        _event_id: &str,
    ) -> *mut SkeletonEventBase {
        // SkeletonEventBase is a ZST opaque type; a non-null sentinel is sufficient.
        std::ptr::NonNull::<SkeletonEventBase>::dangling().as_ptr()
    }

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

    unsafe fn skeleton_send_event(
        event_ptr: *mut SkeletonEventBase,
        _event_type: &str,
        _data_ptr: *const std::ffi::c_void,
    ) -> bool {
        !event_ptr.is_null()
    }

    unsafe fn subscribe_to_event(event_ptr: *mut ProxyEventBase, _max_sample_count: u32) -> bool {
        !event_ptr.is_null()
    }

    unsafe fn unsubscribe_to_event(_event_ptr: *mut ProxyEventBase) {}

    unsafe fn set_event_receive_handler(
        proxy_event_ptr: *mut ProxyEventBase,
        _handler: &FatPtr,
        _event_type: &str,
    ) -> bool {
        !proxy_event_ptr.is_null()
    }

    unsafe fn clear_event_receive_handler(
        _proxy_event_ptr: *mut ProxyEventBase,
        _event_type: &str,
    ) {
    }

    unsafe fn start_find_service(
        _callback: &FatPtr,
        _instance_spec: InstanceSpecifier,
    ) -> *mut FindServiceHandle {
        Box::into_raw(Box::new(std::mem::zeroed::<FindServiceHandle>()))
    }

    unsafe fn stop_find_service(handle: *mut FindServiceHandle) {
        if !handle.is_null() {
            // SAFETY: handle was allocated by start_find_service via Box::into_raw
            drop(Box::from_raw(handle));
        }
    }

    unsafe fn handle_container_size(_container: &HandleContainer) -> usize {
        0
    }

    unsafe fn handle_container_get_at(
        _container: &HandleContainer,
        _index: usize,
    ) -> Option<&HandleType> {
        None
    }
}

impl MockFFIBridge {
    // The handle container is backed by a heap-allocated zeroed stub so the pointer
    // is stable and non-dangling.  An extra `Arc` clone is intentionally forgotten
    // inside this function so the strong count is permanently pinned above zero —
    // meaning `HandleContainer::drop` (which calls real C++) is never invoked
    // regardless of how many clones the caller makes or drops.
    pub fn make_handle_container() -> std::sync::Arc<HandleContainer> {
        // A dangling non-null sentinel: the inner pointer is never passed to C++ because
        // MockFFIBridge::handle_container_size / handle_container_get_at always return 0 / None.
        let ptr = std::ptr::NonNull::<NativeHandleContainer>::dangling().as_ptr();
        let hc = std::sync::Arc::new(HandleContainer::new(ptr));
        // Prevent HandleContainer::drop (which would call real C++ delete) by pinning the refcount.
        std::mem::forget(std::sync::Arc::clone(&hc));
        hc
    }

    // `ProxyEventBase` is a ZST opaque type, a dangling non-null pointer is the
    // canonical representation for "valid but empty" in the mock.
    pub fn make_proxy_event_ptr() -> *mut ProxyEventBase {
        std::ptr::NonNull::<ProxyEventBase>::dangling().as_ptr()
    }

    // Registers type `T` as the backing type for the next `get_allocatee_ptr` /
    // `get_allocatee_data_ptr` cycle.
    // Heap-allocates a `MaybeUninit<T>` buffer whose address is returned by
    // `get_allocatee_data_ptr`, and records `size_of::<SampleAllocateePtr<T>>()` so that
    // `get_allocatee_ptr` can zero-fill the caller's slot (making `assume_init()` safe).
    pub fn set_alloc_backing<T: 'static>() {
        use sample_allocatee_ptr_rs::SampleAllocateePtr;
        let data_ptr =
            Box::into_raw(Box::new(std::mem::MaybeUninit::<T>::uninit())) as *mut std::ffi::c_void;
        // Drop glue: frees the MaybeUninit<T> box without running T's destructor.
        let drop_fn: fn(*mut std::ffi::c_void) = |p| {
            // SAFETY: p was allocated as Box<MaybeUninit<T>> by set_alloc_backing::<T>.
            drop(unsafe { Box::from_raw(p as *mut std::mem::MaybeUninit<T>) });
        };
        // Replace any previous backing (prevents leaks on repeated calls).
        DATA_BACKING.with(|d| {
            if let Some((old_ptr, old_drop)) = d.borrow_mut().replace((data_ptr, drop_fn)) {
                old_drop(old_ptr);
            }
        });
        ALLOC_SIZE.with(|s| *s.borrow_mut() = std::mem::size_of::<SampleAllocateePtr<T>>());
    }

    // Registers `data` as the backing value for the next `sample_ptr_get` /
    // `sample_ptr_delete` cycle.
    //
    // Heap-allocates `data` and stores a typed drop-glue so `sample_ptr_delete`
    // can free it without knowing `T`. `sample_ptr_get` returns the pointer to
    // the heap `T`, making `get_data()` return a valid `&T`.
    pub fn set_sample_backing<T: 'static>(data: T) {
        let ptr = Box::into_raw(Box::new(data)) as *mut std::ffi::c_void;
        let drop_fn: fn(*mut std::ffi::c_void) = |p| {
            // SAFETY: p was allocated as Box<T> by set_sample_backing::<T>.
            drop(unsafe { Box::from_raw(p as *mut T) });
        };
        // Replace any previous backing (prevents leaks on repeated calls).
        SAMPLE_BACKING.with(|s| {
            if let Some((old_ptr, old_drop)) = s.borrow_mut().replace((ptr, drop_fn)) {
                old_drop(old_ptr);
            }
        });
    }
}
