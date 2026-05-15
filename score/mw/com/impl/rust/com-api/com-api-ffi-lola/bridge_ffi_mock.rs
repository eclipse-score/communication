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
use bridge_ffi_rs::{
    FatPtr, FindServiceHandle, HandleContainer, HandleType, InstanceSpecifier,
    NativeHandleContainer, NativeInstanceSpecifier, ProxyBase, ProxyEventBase, SkeletonBase,
    SkeletonEventBase,
};
use std::cell::RefCell;

// Holds a heap-allocated pointer and its associated drop function.
// Used for type-erased heap allocations in the mock FFI bridge.
#[derive(Clone)]
struct BackingEntry {
    // Pointer to the heap-allocated data
    ptr: *mut std::ffi::c_void,
    // Drop function that knows how to properly free the data
    drop_fn: fn(*mut std::ffi::c_void),
}

// SAFETY: BackingEntry is safe to Send and Sync because,
// each test will typically use its own instance of MockFFIBridge and thus its own BackingEntry,
// so there is no shared mutable state across threads.
unsafe impl Send for BackingEntry {}
unsafe impl Sync for BackingEntry {}

impl std::fmt::Debug for BackingEntry {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("BackingEntry").finish()
    }
}

impl Drop for BackingEntry {
    fn drop(&mut self) {
        // SAFETY: The drop_fn knows the actual type of the pointer and was stored
        // when the pointer was allocated (in set_alloc_backing or set_sample_backing).
        // The drop_fn will properly free the heap allocation.
        (self.drop_fn)(self.ptr);
    }
}

// Mock FFI Bridge with instance state for testing.
//
// FFIBridge trait requires Clone - when Runtime creates a consumer/producer, it clones
// the bridge instance.
// RefCell<...> provides interior mutability: FFIBridge trait methods take &self (not &mut self)
// To match this signature while mutating internal state (e.g., storing/retrieving mock data),
// we need interior mutability.
#[derive(Debug, Clone)]
pub struct MockFFIBridge {
    //DATA_BACKING holds a pointer to the heap-allocated backing data and a drop function to free it.
    //We are using this when user calls get_allocatee_data_ptr to return a pointer to this backing data.
    data_backing: RefCell<Option<BackingEntry>>,
    //ALLOC_SIZE holds the size of the allocatee type for the next get_allocatee_ptr call, allowing
    //the mock to zero-fill the caller's slot correctly.
    alloc_size: RefCell<usize>,
    //SAMPLE_BACKING holds a pointer to the heap-allocated sample data and a drop function to free it.
    sample_backing: RefCell<Option<BackingEntry>>,
}

// SAFETY: MockFFIBridge is safe to Send and Sync because each test will typically use its own instance of MockFFIBridge,
// so there is no shared mutable state across threads.
unsafe impl Send for MockFFIBridge {}
unsafe impl Sync for MockFFIBridge {}

impl Default for MockFFIBridge {
    fn default() -> Self {
        Self {
            data_backing: RefCell::new(None),
            alloc_size: RefCell::new(0),
            sample_backing: RefCell::new(None),
        }
    }
}

impl bridge_ffi_rs::FFIBridge for MockFFIBridge {
    // Return value based on null-check of input pointers, simulating success/failure of FFI calls.
    // Also alloc_ptr is zero-filled if size is set.
    unsafe fn get_allocatee_ptr(
        &self,
        event_ptr: *mut SkeletonEventBase,
        allocatee_ptr: *mut std::ffi::c_void,
        _event_type: &str,
    ) -> bool {
        if event_ptr.is_null() || allocatee_ptr.is_null() {
            return false;
        }
        let size = *self.alloc_size.borrow();
        if size == 0 {
            return false;
        }
        unsafe { std::ptr::write_bytes(allocatee_ptr as *mut u8, 0, size) };
        true
    }

    // Deletes the heap-allocated backing data for the current allocatee, if any, and resets alloc_size.
    unsafe fn delete_allocatee_ptr(&self, _allocatee_ptr: *mut std::ffi::c_void, _type_name: &str) {
        if let Some(entry) = self.data_backing.borrow_mut().take() {
            (entry.drop_fn)(entry.ptr);
        }
        *self.alloc_size.borrow_mut() = 0;
    }

    // Returns a pointer to the heap-allocated backing data for the current allocatee, if any.
    unsafe fn get_allocatee_data_ptr(
        &self,
        _allocatee_ptr: *const std::ffi::c_void,
        _type_name: &str,
    ) -> *mut std::ffi::c_void {
        self.data_backing
            .borrow()
            .as_ref()
            .map(|entry| entry.ptr)
            .unwrap_or(std::ptr::null_mut())
    }

    // Returns true if event_ptr is non-null, simulating successful event sending.
    unsafe fn skeleton_event_send_sample_allocatee(
        &self,
        event_ptr: *mut SkeletonEventBase,
        _event_type: &str,
        _allocatee_ptr: *const std::ffi::c_void,
    ) -> bool {
        !event_ptr.is_null()
    }

    // Returns a pointer to the heap-allocated sample data for the current sample from the mock, if any.
    unsafe fn sample_ptr_get(
        &self,
        _sample_ptr: *const std::ffi::c_void,
        _type_name: &str,
    ) -> *const std::ffi::c_void {
        self.sample_backing
            .borrow()
            .as_ref()
            .map(|entry| entry.ptr as *const std::ffi::c_void)
            .unwrap_or(std::ptr::null())
    }

    // Deletes the heap-allocated sample backing data for the current sample, if any.
    unsafe fn sample_ptr_delete(&self, _sample_ptr: *mut std::ffi::c_void, _type_name: &str) {
        if let Some(entry) = self.sample_backing.borrow_mut().take() {
            (entry.drop_fn)(entry.ptr);
        }
    }

    // The following methods simulate success/failure based on null-checks of input pointers.
    unsafe fn skeleton_offer_service(&self, skeleton_ptr: *mut SkeletonBase) -> bool {
        !skeleton_ptr.is_null()
    }

    // This mock does not track offered services, so stop_offer_service is a no-op.
    unsafe fn skeleton_stop_offer_service(&self, _skeleton_ptr: *mut SkeletonBase) {}

    // Creates a new ProxyBase instance and returns a pointer to it.
    // The actual content is not important for the mock, so we return zeroed instances.
    unsafe fn create_proxy(&self, _interface_id: &str, _handle_ptr: &HandleType) -> *mut ProxyBase {
        unsafe { Box::into_raw(Box::new(std::mem::zeroed::<ProxyBase>())) }
    }

    // Creates a new SkeletonBase instance and returns a pointer to it.
    // The actual content is not important for the mock, so we return zeroed instances.
    unsafe fn create_skeleton(
        &self,
        _interface_id: &str,
        _instance_spec: *const NativeInstanceSpecifier,
    ) -> *mut SkeletonBase {
        unsafe { Box::into_raw(Box::new(std::mem::zeroed::<SkeletonBase>())) }
    }

    // Destroys a ProxyBase instance created by create_proxy.
    unsafe fn destroy_proxy(&self, proxy_ptr: *mut ProxyBase) {
        if !proxy_ptr.is_null() {
            // SAFETY: proxy_ptr was allocated by create_proxy via Box::into_raw
            unsafe { drop(Box::from_raw(proxy_ptr)) };
        }
    }

    // Destroys a SkeletonBase instance created by create_skeleton.
    unsafe fn destroy_skeleton(&self, skeleton_ptr: *mut SkeletonBase) {
        if !skeleton_ptr.is_null() {
            // SAFETY: skeleton_ptr was allocated by create_skeleton via Box::into_raw
            unsafe { drop(Box::from_raw(skeleton_ptr)) };
        }
    }

    // The following methods return non-null sentinel pointers for events
    // if the input proxy/skeleton pointer is non-null, simulating successful event retrieval.
    unsafe fn get_event_from_proxy(
        &self,
        proxy_ptr: *mut ProxyBase,
        _interface_id: &str,
        _event_id: &str,
    ) -> *mut ProxyEventBase {
        if proxy_ptr.is_null() {
            return std::ptr::null_mut();
        }
        // ProxyEventBase is a ZST opaque type; a non-null sentinel is sufficient.
        // LIMITATION: The returned pointer is intentionally dangling and must NOT be
        // dereferenced. It is only valid as a non-null sentinel for null-checks in the
        // mock. Any test that dereferences this pointer will trigger undefined behavior.
        std::ptr::NonNull::<ProxyEventBase>::dangling().as_ptr()
    }

    // The following method returns a non-null sentinel pointer for the event
    // if the input skeleton pointer is non-null, simulating successful event retrieval.
    unsafe fn get_event_from_skeleton(
        &self,
        _skeleton_ptr: *mut SkeletonBase,
        _interface_id: &str,
        _event_id: &str,
    ) -> *mut SkeletonEventBase {
        // SkeletonEventBase is a ZST opaque type; a non-null sentinel is sufficient.
        // LIMITATION: The returned pointer is intentionally dangling and must NOT be
        // dereferenced. It is only valid as a non-null sentinel for null-checks in the
        // mock. Any test that dereferences this pointer will trigger undefined behavior.
        std::ptr::NonNull::<SkeletonEventBase>::dangling().as_ptr()
    }

    // The following methods simulate event subscription and handling based on null-checks of input pointers.
    unsafe fn get_samples_from_event(
        &self,
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

    // The following method simulates event sending based on null-check of the event pointer.
    unsafe fn skeleton_send_event(
        &self,
        event_ptr: *mut SkeletonEventBase,
        _event_type: &str,
        _data_ptr: *const std::ffi::c_void,
    ) -> bool {
        !event_ptr.is_null()
    }

    // The following methods simulate event subscription and
    // handler registration based on null-checks of input pointers.
    unsafe fn subscribe_to_event(
        &self,
        event_ptr: *mut ProxyEventBase,
        _max_sample_count: u32,
    ) -> bool {
        !event_ptr.is_null()
    }

    // The following method simulates event unsubscription. Since this is a mock,
    // it does not track subscriptions.
    unsafe fn unsubscribe_to_event(&self, _event_ptr: *mut ProxyEventBase) {}

    unsafe fn set_event_receive_handler(
        &self,
        proxy_event_ptr: *mut ProxyEventBase,
        _handler: &FatPtr,
        _event_type: &str,
    ) -> bool {
        !proxy_event_ptr.is_null()
    }

    // The following method simulates clearing the event receive handler. Since this is a mock,
    // it does not track handlers, so this is a no-op.
    unsafe fn clear_event_receive_handler(
        &self,
        _proxy_event_ptr: *mut ProxyEventBase,
        _event_type: &str,
    ) {
    }

    // The following methods simulate service discovery based on null-checks of input pointers.
    unsafe fn start_find_service(
        &self,
        _callback: &FatPtr,
        _instance_spec: InstanceSpecifier,
    ) -> *mut FindServiceHandle {
        unsafe { Box::into_raw(Box::new(std::mem::zeroed::<FindServiceHandle>())) }
    }

    unsafe fn stop_find_service(&self, handle: *mut FindServiceHandle) {
        if !handle.is_null() {
            // SAFETY: handle was allocated by start_find_service via Box::into_raw
            unsafe { drop(Box::from_raw(handle)) };
        }
    }

    // Returns the handle count of the container, which is always zero in this mock implementation.
    fn handle_container_size(&self, _container: &HandleContainer) -> usize {
        0
    }

    // Returns None for any index, since the container is always empty in this mock implementation.
    fn handle_container_get_at<'a>(
        &self,
        _container: &'a HandleContainer,
        _index: usize,
    ) -> Option<&'a HandleType> {
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
        // LIMITATION: `ptr` is intentionally dangling and must NOT be dereferenced.
        // It is safe only because the mock's handle_container_size / handle_container_get_at
        // implementations never expose or forward this pointer to any code that would read it.
        // Tests must not call any real C++ handle-container operations on the returned Arc.
        let ptr = std::ptr::NonNull::<NativeHandleContainer>::dangling().as_ptr();
        let hc = std::sync::Arc::new(HandleContainer::new(ptr));
        // Prevent HandleContainer::drop (which would call real C++ delete) by pinning the refcount.
        std::mem::forget(std::sync::Arc::clone(&hc));
        hc
    }

    // `ProxyEventBase` is a ZST opaque type, a dangling non-null pointer is the
    // canonical representation for "valid but empty" in the mock.
    //
    // LIMITATION: The returned pointer is intentionally dangling and must NOT be
    // dereferenced. Tests may only use it as a non-null sentinel (e.g., to satisfy
    // null-checks in other mock methods). Dereferencing it is undefined behavior.
    pub fn make_proxy_event_ptr() -> *mut ProxyEventBase {
        std::ptr::NonNull::<ProxyEventBase>::dangling().as_ptr()
    }

    // Registers type `T` as the backing type for the next `get_allocatee_ptr` /
    // `get_allocatee_data_ptr` cycle.
    // Heap-allocates a `MaybeUninit<T>` buffer whose address is returned by
    // `get_allocatee_data_ptr`, and records `size_of::<SampleAllocateePtr<T>>()` so that
    // `get_allocatee_ptr` can zero-fill the caller's slot (making `assume_init()` safe).
    pub fn set_alloc_backing<T: 'static>(&self) {
        use sample_allocatee_ptr_rs::SampleAllocateePtr;
        let data_ptr =
            Box::into_raw(Box::new(std::mem::MaybeUninit::<T>::uninit())) as *mut std::ffi::c_void;
        // Drop glue: frees the MaybeUninit<T> box without running T's destructor.
        let drop_fn: fn(*mut std::ffi::c_void) = |p| {
            // SAFETY: p was allocated as Box<MaybeUninit<T>> by set_alloc_backing::<T>.
            drop(unsafe { Box::from_raw(p as *mut std::mem::MaybeUninit<T>) });
        };
        // Replace any previous backing - Drop trait automatically cleans up the old entry
        *self.data_backing.borrow_mut() = Some(BackingEntry {
            ptr: data_ptr,
            drop_fn,
        });
        *self.alloc_size.borrow_mut() = std::mem::size_of::<SampleAllocateePtr<T>>();
    }

    // Registers `data` as the backing value for the next `sample_ptr_get` /
    // `sample_ptr_delete` cycle.
    //
    // Heap-allocates `data` and stores a typed drop-glue so `sample_ptr_delete`
    // can free it without knowing `T`. `sample_ptr_get` returns the pointer to
    // the heap `T`, making `get_data()` return a valid `&T`.
    pub fn set_sample_backing<T: 'static>(&self, data: T) {
        let ptr = Box::into_raw(Box::new(data)) as *mut std::ffi::c_void;
        let drop_fn: fn(*mut std::ffi::c_void) = |p| {
            // SAFETY: p was allocated as Box<T> by set_sample_backing::<T>.
            drop(unsafe { Box::from_raw(p as *mut T) });
        };
        // Replace any previous backing - Drop trait automatically cleans up the old entry
        *self.sample_backing.borrow_mut() = Some(BackingEntry { ptr, drop_fn });
    }
}
