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
// It allows pre-configuring responses for FFI calls and simulates success/failure based on the
// presence of pre-configured data or null-checks of input pointers.
// The mock maintains internal state for allocatee backing data, sample backing data, proxies, skeletons, and events.
// It Provide the ability to set up expected data and behavior for tests using setter methods of the MockFFIBridge struct.
// We have Skeleton, Proxy and Event unit structs as placeholders for the pointers returned by the mock, allowing us to track and manage these resources in tests
// and in future we can extend these structs to hold additional metadata if needed for more complex test scenarios.
// We ensure that all heap-allocated resources are properly freed by implementing Drop for BackingEntry and MockFFIBridgeState,
// and by using the drop functions stored in BackingEntry to free the correct types of data.
// This Mock can work as a real FFIBridge implementation and
// each test can configure the mock with the expected data and behavior for that test,
// allowing us to test the LolaRuntime and related components in isolation from the actual FFI layer.

// This mock back-end (FFIBridge implementation) is used for unit testing of Lola-Runtime
// So we do not want to expose this mock in the public API docs and crate level documentation, hence the `doc(hidden)` attribute.
#![doc(hidden)]

use bridge_ffi_rs::{
    FatPtr, FindServiceHandle, HandleContainer, HandleType, InstanceSpecifier,
    NativeInstanceSpecifier, ProxyBase, ProxyEventBase, SkeletonBase, SkeletonEventBase,
};
use std::collections::HashMap;
use std::ffi::c_void;
use std::mem::size_of;
use std::num::NonZeroUsize;
use std::sync::{Arc, Mutex};

// Unit types for mock objects, kept for future extensibility and clarity
struct Event {}

struct Proxy {}

struct Skeleton {}

// Holds a heap-allocated pointer and its associated drop function.
// Used for type-erased heap allocations in the mock FFI bridge.
#[derive(Clone)]
struct BackingEntry {
    // Pointer to the heap-allocated data
    ptr: *mut std::ffi::c_void,
    // Drop function that knows how to properly free the data
    drop_fn: fn(*mut std::ffi::c_void),
    // Size of the allocation
    size: usize,
}

// SAFETY: BackingEntry is safe to Send because the drop_fn ensures that the heap allocation is properly freed,
// and the pointer is only accessed through the mock's controlled methods which enforce thread safety via Mutex.
// No mutbale operation on the pointer is exposed to the caller,
// and the mock's internal methods ensure that any access to the pointer is thread-safe.
unsafe impl Send for BackingEntry {}

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
// Arc<Mutex<...>> provides thread-safe interior mutability: FFIBridge trait methods take &self (not &mut self)
// To match this signature while mutating internal state (e.g., storing/retrieving mock data),
// we need interior mutability. Arc<Mutex<...>> ensures thread safety at the cost of some performance,
// which is acceptable for test code where safety is more important than performance.
/// Thread-safe, test-isolated FFI bridge mock
#[derive(Debug, Clone, Default)]
pub struct MockFFIBridge {
    state: Arc<Mutex<MockFFIBridgeState>>,
}

#[derive(Debug, Default)]
struct MockFFIBridgeState {
    allocatee_backing: HashMap<String, BackingEntry>,
    sample_backing: HashMap<String, BackingEntry>,
    max_sample_count: Option<NonZeroUsize>,
    proxies: HashMap<String, *mut ProxyBase>,
    skeletons: HashMap<String, *mut SkeletonBase>,
    proxy_events: HashMap<String, *mut ProxyEventBase>,
    skeleton_events: HashMap<String, *mut SkeletonEventBase>,
}

// SAFETY: MockFFIBridgeState is safe to Send because all access to its internal state is protected by a Mutex,
// Implementation in runtime handles the concurrent access and mutable access.
unsafe impl Send for MockFFIBridgeState {}

impl MockFFIBridge {
    pub fn new() -> Self {
        Self {
            state: Arc::new(Mutex::new(MockFFIBridgeState::default())),
        }
    }
    /// Pre-configure heap-allocated backing data for an allocatee of a specific event type.
    pub fn set_allocatee_backing<T: 'static>(&self, event_type: &str, value: T) {
        let boxed = Box::new(value);
        let ptr = Box::into_raw(boxed) as *mut c_void;

        let entry = BackingEntry {
            ptr,
            drop_fn: |p| unsafe {
                drop(Box::from_raw(p as *mut T));
            },
            size: size_of::<T>(),
        };

        let mut state = self.state.lock().expect("Failed to lock state");
        state
            .allocatee_backing
            .insert(event_type.to_string(), entry);
    }

    /// Pre-configure heap-allocated backing data for a sample of a specific type.
    pub fn set_sample_backing<T: 'static>(&self, type_name: &str, value: T) {
        let boxed = Box::new(value);
        let ptr = Box::into_raw(boxed) as *mut c_void;

        let entry = BackingEntry {
            ptr,
            drop_fn: |p| unsafe {
                drop(Box::from_raw(p as *mut T));
            },
            size: size_of::<T>(),
        };

        let mut state = self.state.lock().expect("Failed to lock state");
        state.sample_backing.insert(type_name.to_string(), entry);
    }

    /// Configure the maximum sample count.
    pub fn set_max_sample_count(&self, count: Option<NonZeroUsize>) {
        self.state
            .lock()
            .expect("Failed to lock state")
            .max_sample_count = count;
    }

    /// Pre-configure a proxy for a specific interface type.
    /// Returns the pointer that will be returned by create_proxy for this interface_id.
    pub fn set_proxy(&self, interface_id: &str) -> *mut ProxyBase {
        let proxy = Box::new(Proxy {});
        let ptr = Box::into_raw(proxy) as *mut ProxyBase;
        self.state
            .lock()
            .expect("Failed to lock state")
            .proxies
            .insert(interface_id.to_string(), ptr);
        ptr
    }

    /// Pre-configure a skeleton for a specific interface type.
    /// Returns the pointer that will be returned by create_skeleton for this interface_id.
    pub fn set_skeleton(&self, interface_id: &str) -> *mut SkeletonBase {
        let skeleton = Box::new(Skeleton {});
        let ptr = Box::into_raw(skeleton) as *mut SkeletonBase;
        self.state
            .lock()
            .expect("Failed to lock state")
            .skeletons
            .insert(interface_id.to_string(), ptr);
        ptr
    }

    /// Pre-configure a proxy event.
    /// Returns the pointer that can be used for proxy event operations.
    pub fn set_proxy_event(&self, interface_id: &str, event_id: &str) -> *mut ProxyEventBase {
        let event = Box::new(Event {});
        let ptr = Box::into_raw(event) as *mut ProxyEventBase;
        let event_key = format!("{}::{}", interface_id, event_id);
        self.state
            .lock()
            .expect("Failed to lock state")
            .proxy_events
            .insert(event_key.to_string(), ptr);
        ptr
    }

    /// Pre-configure a skeleton event.
    /// Returns the pointer that will be returned by create_skeleton_event for this interface_id and event_id.
    pub fn set_skeleton_event(&self, interface_id: &str, event_id: &str) -> *mut SkeletonEventBase {
        let event = Box::new(Event {});
        let ptr = Box::into_raw(event) as *mut SkeletonEventBase;
        let event_key = format!("{}::{}", interface_id, event_id);
        self.state
            .lock()
            .expect("Failed to lock state")
            .skeleton_events
            .insert(event_key.to_string(), ptr);
        ptr
    }
}

impl Drop for MockFFIBridgeState {
    fn drop(&mut self) {
        // Cleanup any leaked resources
        unsafe {
            for (_key, ptr) in self.proxies.drain() {
                drop(Box::from_raw(ptr));
            }
            for (_key, ptr) in self.skeletons.drain() {
                drop(Box::from_raw(ptr));
            }
            for (_key, ptr) in self.proxy_events.drain() {
                drop(Box::from_raw(ptr));
            }
            for (_key, ptr) in self.skeleton_events.drain() {
                drop(Box::from_raw(ptr));
            }
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
        if event_ptr.is_null() {
            return false;
        }
        // If we have backing data for this allocatee, copy it into the provided pointer.
        let state = self.state.lock().expect("Failed to lock state");
        if let Some(entry) = state.allocatee_backing.get(_event_type) {
            unsafe {
                std::ptr::copy_nonoverlapping(entry.ptr, allocatee_ptr, entry.size);
            }
            return true;
        } else {
            return false;
        }
    }

    // Deletes the heap-allocated backing data for the current allocatee, if any, and resets alloc_size.
    unsafe fn delete_allocatee_ptr(&self, _allocatee_ptr: *mut std::ffi::c_void, type_name: &str) {
        let mut state = self.state.lock().expect("Failed to lock state");
        // Removing the entry from the HashMap will call BackingEntry::drop automatically
        state.allocatee_backing.remove(type_name);
    }

    // Returns a pointer to the heap-allocated backing data for the current allocatee, if any.
    unsafe fn get_allocatee_data_ptr(
        &self,
        _allocatee_ptr: *const std::ffi::c_void,
        type_name: &str,
    ) -> *mut std::ffi::c_void {
        let state = self.state.lock().expect("Failed to lock state");
        if let Some(entry) = state.allocatee_backing.get(type_name) {
            entry.ptr
        } else {
            std::ptr::null_mut()
        }
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
        let state = self.state.lock().expect("Failed to lock state");
        if let Some(entry) = state.sample_backing.get(_type_name) {
            entry.ptr
        } else {
            std::ptr::null()
        }
    }

    // Deletes the heap-allocated sample backing data for the current sample, if any.
    unsafe fn sample_ptr_delete(&self, _sample_ptr: *mut std::ffi::c_void, type_name: &str) {
        let mut state = self.state.lock().expect("Failed to lock state");
        // Removing the entry from the HashMap will call BackingEntry::drop automatically
        state.sample_backing.remove(type_name);
    }

    // The following methods simulate success/failure based on null-checks of input pointers.
    unsafe fn skeleton_offer_service(&self, skeleton_ptr: *mut SkeletonBase) -> bool {
        !skeleton_ptr.is_null()
    }

    // This mock does not track offered services, so stop_offer_service is a no-op.
    unsafe fn skeleton_stop_offer_service(&self, _skeleton_ptr: *mut SkeletonBase) {}

    // Creates a new ProxyBase instance and returns a pointer to it.
    // If a proxy was pre-configured via set_proxy for this interface_id, returns that.
    // Otherwise allocates a new one and tracks it.
    unsafe fn create_proxy(&self, interface_id: &str, _handle_ptr: &HandleType) -> *mut ProxyBase {
        let mut state = self.state.lock().expect("Failed to lock state");
        // Check if pre-configured
        if let Some(&ptr) = state.proxies.get(interface_id) {
            return ptr;
        }
        // Allocate new
        let proxy = Box::new(Proxy {});
        let ptr = Box::into_raw(proxy) as *mut ProxyBase;
        state.proxies.insert(interface_id.to_string(), ptr);
        ptr
    }

    // Creates a new SkeletonBase instance and returns a pointer to it.
    // If a skeleton was pre-configured via set_skeleton for this interface_id, returns that.
    // Otherwise allocates a new one and tracks it.
    unsafe fn create_skeleton(
        &self,
        interface_id: &str,
        _instance_spec: *const NativeInstanceSpecifier,
    ) -> *mut SkeletonBase {
        let mut state = self.state.lock().expect("Failed to lock state");
        // Check if pre-configured
        if let Some(&ptr) = state.skeletons.get(interface_id) {
            return ptr;
        }
        // Allocate new
        let skeleton = Box::new(Skeleton {});
        let ptr = Box::into_raw(skeleton) as *mut SkeletonBase;
        state.skeletons.insert(interface_id.to_string(), ptr);
        ptr
    }

    // Destroys a ProxyBase instance created by create_proxy.
    unsafe fn destroy_proxy(&self, proxy_ptr: *mut ProxyBase) {
        if !proxy_ptr.is_null() {
            let mut state = self.state.lock().expect("Failed to lock state");
            // Find and remove by value
            state.proxies.retain(|_k, &mut v| v != proxy_ptr);
            // Now safe to drop
            unsafe { drop(Box::from_raw(proxy_ptr)) };
        }
    }

    // Destroys a SkeletonBase instance created by create_skeleton.
    unsafe fn destroy_skeleton(&self, skeleton_ptr: *mut SkeletonBase) {
        if !skeleton_ptr.is_null() {
            let mut state = self.state.lock().expect("Failed to lock state");
            // Find and remove by value
            state.skeletons.retain(|_k, &mut v| v != skeleton_ptr);
            // Now safe to drop
            unsafe { drop(Box::from_raw(skeleton_ptr)) };
        }
    }

    // The following methods return non-null sentinel pointers for events
    // if the input proxy/skeleton pointer is non-null, simulating successful event retrieval.
    unsafe fn get_event_from_proxy(
        &self,
        proxy_ptr: *mut ProxyBase,
        interface_id: &str,
        event_id: &str,
    ) -> *mut ProxyEventBase {
        if proxy_ptr.is_null() {
            return std::ptr::null_mut();
        }
        let mut state = self.state.lock().expect("Failed to lock state");
        let event_key = format!("{}::{}", interface_id, event_id);
        // Check if pre-configured
        if let Some(&ptr) = state.proxy_events.get(&event_key) {
            return ptr;
        }
        // Allocate new
        let event = Box::new(Event {});
        let ptr = Box::into_raw(event) as *mut ProxyEventBase;
        state.proxy_events.insert(event_key, ptr);
        ptr
    }

    // The following method returns a non-null sentinel pointer for the event
    // if the input skeleton pointer is non-null, simulating successful event retrieval.
    unsafe fn get_event_from_skeleton(
        &self,
        skeleton_ptr: *mut SkeletonBase,
        interface_id: &str,
        event_id: &str,
    ) -> *mut SkeletonEventBase {
        if skeleton_ptr.is_null() {
            return std::ptr::null_mut();
        }
        let mut state = self.state.lock().expect("Failed to lock state");
        let event_key = format!("{}::{}", interface_id, event_id);
        // Check if pre-configured
        if let Some(&ptr) = state.skeleton_events.get(&event_key) {
            return ptr;
        }
        // Allocate new
        let event = Box::new(Event {});
        let ptr = Box::into_raw(event) as *mut SkeletonEventBase;
        state.skeleton_events.insert(event_key, ptr);
        ptr
    }

    // The following methods simulate event subscription and handling based on null-checks of input pointers.
    unsafe fn get_samples_from_event(
        &self,
        event_ptr: *mut ProxyEventBase,
        _event_type: &str,
        _callback: &FatPtr,
        _max_samples: u32,
    ) -> u32 {
        match self
            .state
            .lock()
            .expect("Failed to lock state")
            .max_sample_count
        {
            Some(count) if !event_ptr.is_null() => count.get() as u32,
            _ => 0,
        }
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
        Box::into_raw(Box::new(unsafe { std::mem::zeroed::<FindServiceHandle>() }))
    }

    // The following method simulates stopping service discovery. Since this is a mock, it simply drops the handle.
    unsafe fn stop_find_service(&self, handle: *mut FindServiceHandle) {
        if !handle.is_null() {
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
