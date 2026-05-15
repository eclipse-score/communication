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

use bridge_ffi_rs::*;

#[derive(Debug, Clone, Default)]
/// unit struct representing the FFI bridge for Lola runtime
pub struct LolaFFIBridge;

///  Rust closure invocation for C++ callbacks
///
/// This function is called by C++ to invoke a Rust closure with a sample pointer.
/// The closure is reconstructed from the FatPtr trait object.
///
/// # Safety
/// - `ptr` must point to a valid FatPtr
/// - `sample_ptr` must point to valid placement-new storage containing SamplePtr<T>
#[unsafe(no_mangle)]
unsafe extern "C" fn mw_com_impl_call_dyn_ref_fnmut_sample(
    ptr: *const FatPtr,
    sample_ptr: *mut std::ffi::c_void,
) {
    if ptr.is_null() || sample_ptr.is_null() {
        return;
    }

    // Reconstruct the closure from FatPtr
    // SAFETY: ptr is guaranteed to point to a valid FatPtr that was created by the C++ side
    // and represents a valid dynamic trait object (dyn FnMut). The transmute is safe because
    // FatPtr is a binary representation of a trait object's fat pointer (data + vtable).
    // The caller must ensure the lifetime of the closure outlives this call.
    let callable: &mut dyn FnMut(*mut std::ffi::c_void) = unsafe { std::mem::transmute(*ptr) };

    // Invoke the closure with the void* sample pointer
    callable(sample_ptr);
}

/// Rust closure invocation for C++ callbacks for FindService
/// This function is called by C++ to invoke a Rust closure for finding services,
/// passing a vector of service handles.
/// This function invokes a Rust closure that takes a HandleContainer and a FindServiceHandle.
/// # Safety
/// - `ptr` must point to a valid FatPtr representing a closure of type
///   FnMut(HandleContainer, NativeFindServiceHandle).
/// - `service_handles` must point to valid NativeHandleContainer data that can be safely wrapped
///   in a HandleContainer.
/// - `find_service_handle` must point to valid FindServiceHandle data that can be safely wrapped
///   in a NativeFindServiceHandle.
#[unsafe(no_mangle)]
unsafe extern "C" fn mw_com_impl_call_dyn_ref_fnmut_find_service(
    ptr: *const FatPtr,
    service_handles: *mut NativeHandleContainer,
    find_service_handle: *mut FindServiceHandle,
) {
    if ptr.is_null() || service_handles.is_null() {
        return;
    }

    // Reconstruct the closure from FatPtr
    let callable: &mut dyn FnMut(HandleContainer, NativeFindServiceHandle) =
        unsafe { std::mem::transmute(*ptr) };

    // Invoke with correct types
    callable(
        HandleContainer::new(service_handles),
        NativeFindServiceHandle::new(find_service_handle),
    );
}

// FFI declarations for C++ functions implemented in registry_bridge_macro.cpp
unsafe extern "C" {

    /// Subscribe to event to start receiving samples
    ///
    /// # Arguments
    /// * `event_ptr` - Opaque event pointer
    /// * `max_sample_count` - Maximum number of samples to buffer
    ///
    /// # Returns
    /// true if subscription successful, false otherwise
    fn mw_com_proxy_event_subscribe(event_ptr: *mut ProxyEventBase, max_sample_count: u32) -> bool;

    /// Get event pointer from proxy by event name
    ///
    /// # Arguments
    /// * `proxy_ptr` - Opaque proxy pointer
    /// * `interface_id` - UTF-8 C string of interface UID
    /// * `event_id` - UTF-8 C string of event name
    ///
    /// # Returns
    /// Opaque event pointer, or nullptr if event not found
    fn mw_com_get_event_from_proxy(
        proxy_ptr: *mut ProxyBase,
        interface_id: StringView,
        event_id: StringView,
    ) -> *mut ProxyEventBase;

    /// Get event pointer from skeleton by event name
    ///
    /// # Arguments
    /// * `skeleton_ptr` - Opaque skeleton pointer
    /// * `interface_id` - UTF-8 C string of interface UID
    /// * `event_id` - UTF-8 C string of event name
    ///
    /// # Returns
    /// Opaque event pointer, or nullptr if event not found
    fn mw_com_get_event_from_skeleton(
        skeleton_ptr: *mut SkeletonBase,
        interface_id: StringView,
        event_id: StringView,
    ) -> *mut SkeletonEventBase;

    /// Get new samples from an event
    ///
    /// # Arguments
    /// * `event_ptr` - Opaque event pointer
    /// * `event_type` - Event type name string
    /// * `callback` - FatPtr to callback function
    /// * `max_samples` - Maximum number of samples to retrieve
    ///
    /// # Returns
    /// Number of samples retrieved
    fn mw_com_type_registry_get_samples_from_event(
        event_ptr: *mut ProxyEventBase,
        event_type: StringView,
        callback: *const FatPtr,
        max_samples: u32,
    ) -> u32;

    /// Send data via skeleton event
    ///
    /// # Arguments
    /// * `event_ptr` - Opaque skeleton event pointer
    /// * `event_type` - Event type name string
    /// * `data_ptr` - Pointer to event data
    ///
    /// # Returns
    /// None (void function)
    fn mw_com_skeleton_send_event(
        event_ptr: *mut SkeletonEventBase,
        event_type: StringView,
        data_ptr: CVoidPtr,
    ) -> bool;

    /// Create proxy by UID and handle
    ///
    /// # Arguments
    /// * `interface_id` - UTF-8 C string of interface UID
    /// * `handle_ptr` - Opaque handle pointer
    ///
    /// # Returns
    /// Opaque proxy pointer, or nullptr if creation failed
    fn mw_com_create_proxy(interface_id: StringView, handle_ptr: &HandleType) -> *mut ProxyBase;

    /// Create skeleton by UID and instance specifier
    ///
    /// # Arguments
    /// * `interface_id` - UTF-8 C string of interface UID
    /// * `instance_spec` - Opaque instance specifier pointer
    ///
    /// # Returns
    /// Opaque skeleton pointer, or nullptr if creation failed
    fn mw_com_create_skeleton(
        interface_id: StringView,
        instance_spec: *const NativeInstanceSpecifier,
    ) -> *mut SkeletonBase;

    /// Destroy proxy
    ///
    /// # Arguments
    /// * `proxy_ptr` - Opaque proxy pointer
    ///
    /// # Returns
    /// None (void function)
    fn mw_com_destroy_proxy(proxy_ptr: *mut ProxyBase);

    /// Destroy skeleton
    ///
    /// # Arguments
    /// * `skeleton_ptr` - Opaque skeleton pointer
    ///
    /// # Returns
    /// None (void function)
    fn mw_com_destroy_skeleton(skeleton_ptr: *mut SkeletonBase);

    /// Offer service via skeleton
    ///
    /// # Arguments
    /// * `skeleton_ptr` - Opaque skeleton pointer
    ///
    /// # Returns
    /// true if offer successful, false otherwise
    fn mw_com_skeleton_offer_service(skeleton_ptr: *mut SkeletonBase) -> bool;

    /// Stop offering service via skeleton
    ///
    /// # Arguments
    /// * `skeleton_ptr` - Opaque skeleton pointer
    ///
    /// # Returns
    /// None (void function)
    fn mw_com_skeleton_stop_offer_service(skeleton_ptr: *mut SkeletonBase);

    /// Get sample data pointer from SamplePtr<T>
    ///
    /// # Arguments
    /// * `sample_ptr` - Opaque sample pointer
    /// * `type_name` - Type name string
    ///
    /// # Returns
    /// Pointer to sample data, or nullptr if type mismatch
    fn mw_com_get_sample_ptr(
        sample_ptr: *const std::ffi::c_void,
        type_name: StringView,
    ) -> *const std::ffi::c_void;

    /// Delete sample pointer of SamplePtr<T>'
    ///
    /// # Arguments
    /// * `sample_ptr` - Opaque sample pointer
    /// * `type_name` - Type name string
    fn mw_com_delete_sample_ptr(sample_ptr: *mut std::ffi::c_void, type_name: StringView);

    /// Get allocatee pointer from skeleton event of specific type
    ///
    /// # Arguments
    /// * `event_ptr` - Opaque skeleton event pointer
    /// * `allocatee_ptr` - Pointer to pre-allocated memory for allocatee
    /// * `event_type` - Type name string
    ///
    /// # Returns
    /// True if allocatee pointer was retrieved successfully, false otherwise
    fn mw_com_get_allocatee_ptr(
        event_ptr: *mut SkeletonEventBase,
        allocatee_ptr: *mut std::ffi::c_void,
        event_type: StringView,
    ) -> bool;

    /// Delete allocatee pointer of SampleAllocateePtr<T>
    ///
    /// # Arguments
    /// * `allocatee_ptr` - Pointer to SampleAllocateePtr<T>
    /// * `type_name` - Type name string
    fn mw_com_delete_allocatee_ptr(allocatee_ptr: *mut std::ffi::c_void, type_name: StringView);

    /// Get allocatee data pointer from allocatee of specific type
    ///
    /// # Arguments
    /// * `allocatee_ptr` - Pointer to SampleAllocateePtr<T>
    /// * `type_name` - Type name string
    ///
    /// # Returns
    /// Pointer to allocatee data, or nullptr if type mismatch
    fn mw_com_get_allocatee_data_ptr(
        allocatee_ptr: *const std::ffi::c_void,
        type_name: StringView,
    ) -> *mut std::ffi::c_void;

    /// Send event via skeleton using allocatee pointer of specific type
    ///
    /// # Arguments
    /// * `event_ptr` - Opaque skeleton event pointer
    /// * `event_type` - Type name string
    /// * `allocatee_ptr` - Pointer to SampleAllocateePtr<T>
    ///
    /// # Returns
    /// True if event was sent successfully, false otherwise
    fn mw_com_skeleton_send_event_allocatee(
        event_ptr: *mut SkeletonEventBase,
        event_type: StringView,
        allocatee_ptr: *const std::ffi::c_void,
    ) -> bool;

    /// Set event receive handler for proxy event
    ///
    /// # Arguments
    /// * `proxy_event_ptr` - Opaque proxy event pointer
    /// * `handler` - FatPtr to event receive handler function
    ///
    /// # Returns
    /// True if handler was set successfully, false otherwise
    fn mw_com_proxy_set_event_receive_handler(
        proxy_event_ptr: *mut ProxyEventBase,
        handler: *const FatPtr,
        event_type: StringView,
    ) -> bool;

    /// Clear event receive handler for proxy event
    ///
    /// # Arguments
    /// * `proxy_event_ptr` - Opaque proxy event pointer
    fn mw_com_proxy_clear_event_receive_handler(
        proxy_event_ptr: *mut ProxyEventBase,
        event_type: StringView,
    );

    /// Start finding services with callback for results
    ///
    /// # Arguments
    /// * `callback` - FatPtr to callback function that will be called with discovery results
    /// * `instance_spec` - Pointer to instance specifier for the service to find
    ///
    /// # Returns
    /// Opaque pointer to FindServiceHandle which can be used to stop the find operation
    fn mw_com_start_find_service(
        callback: *const FatPtr,
        instance_spec: *const NativeInstanceSpecifier,
    ) -> *mut FindServiceHandle;

    /// Stop finding services using the provided FindServiceHandle
    ///
    /// # Arguments
    /// * `handle` - Opaque pointer to FindServiceHandle returned by mw_com_start_find_service
    fn mw_com_stop_find_service(handle: *mut FindServiceHandle);

    /// Unsubscribe from event to stop receiving samples
    ///
    /// # Arguments
    /// * `event_ptr` - Opaque event pointer
    fn mw_com_proxy_event_unsubscribe(event_ptr: *mut ProxyEventBase);
}

impl FFIBridge for LolaFFIBridge {
    /// Get allocatee pointer from skeleton event of specific type
    ///
    /// # Arguments
    /// * `event_ptr` - Opaque skeleton event pointer
    /// * `allocatee_ptr` - Pointer to pre-allocated memory for allocatee
    /// * `event_type` - Type name string
    ///
    /// # Returns
    /// True if allocatee pointer was retrieved successfully, false otherwise
    ///
    /// # Safety
    /// event_ptr must point to a valid SkeletonEventBase,
    /// allocatee_ptr must point to valid memory for the allocatee,
    /// and event_type must be a valid UTF-8 string representing the event type.
    unsafe fn get_allocatee_ptr(
        &self,
        event_ptr: *mut SkeletonEventBase,
        allocatee_ptr: *mut std::ffi::c_void,
        event_type: &str,
    ) -> bool {
        // SAFETY: event_ptr is valid which is created using create_skeleton()
        // and allocatee_ptr has enough memory allocated for the construction of allocatee instance.
        let event_type = StringView::from(event_type);
        unsafe { mw_com_get_allocatee_ptr(event_ptr, allocatee_ptr, event_type) }
    }

    /// Delete allocatee pointer of SampleAllocateePtr<T>
    ///
    /// # Arguments
    /// * `allocatee_ptr` - Pointer to SampleAllocateePtr<T>
    /// * `type_name` - Type name string
    ///
    /// # Safety
    /// allocatee_ptr must point to a valid SampleAllocateePtr<T> of the specified type.
    unsafe fn delete_allocatee_ptr(&self, allocatee_ptr: *mut std::ffi::c_void, type_name: &str) {
        // SAFETY: allocatee_ptr is valid which is created using get_allocatee_ptr() and
        // type_name is constructed using static str.
        let type_name = StringView::from(type_name);
        unsafe {
            mw_com_delete_allocatee_ptr(allocatee_ptr, type_name);
        }
    }

    /// Get allocatee data pointer from allocatee of specific type
    ///
    /// # Arguments
    /// * `allocatee_ptr` - Pointer to SampleAllocateePtr<T>
    /// * `type_name` - Type name string
    ///
    /// # Returns
    /// Pointer to allocatee data, or nullptr if type mismatch
    ///
    /// # Safety
    /// allocatee_ptr must point to a valid SampleAllocateePtr<T> of the specified type
    unsafe fn get_allocatee_data_ptr(
        &self,
        allocatee_ptr: *const std::ffi::c_void,
        type_name: &str,
    ) -> *mut std::ffi::c_void {
        // SAFETY: allocatee_ptr is valid which is created using get_allocatee_ptr() and
        // type_name is constructed using static str.
        let type_name = StringView::from(type_name);
        unsafe { mw_com_get_allocatee_data_ptr(allocatee_ptr, type_name) }
    }

    /// Send event via skeleton using allocatee pointer of specific type
    ///
    /// # Arguments
    /// * `event_ptr` - Opaque skeleton event pointer
    /// * `event_type` - Type name string
    /// * `allocatee_ptr` - Pointer to SampleAllocateePtr<T>
    ///
    /// # Returns
    /// True if event was sent successfully, false otherwise
    ///
    /// # Safety
    /// event_ptr must point to a valid SkeletonEventBase,
    /// allocatee_ptr must point to a valid SampleAllocateePtr<T>,
    /// and event_type must be a valid UTF-8 string representing the event type.
    unsafe fn skeleton_event_send_sample_allocatee(
        &self,
        event_ptr: *mut SkeletonEventBase,
        event_type: &str,
        allocatee_ptr: *const std::ffi::c_void,
    ) -> bool {
        // SAFETY: event_ptr is valid which is created using create_skeleton() and allocatee_ptr is
        // valid which is created using get_allocatee_ptr().
        let event_type = StringView::from(event_type);
        unsafe { mw_com_skeleton_send_event_allocatee(event_ptr, event_type, allocatee_ptr) }
    }

    /// Get SamplePtr<T> data pointer
    ///
    /// # Arguments
    /// * `sample_ptr` - Opaque sample pointer
    /// * `type_name` - Type name string
    ///
    /// # Returns
    /// Pointer to sample data, or nullptr if type mismatch
    /// # Safety
    /// sample_ptr must point to a valid SamplePtr<T> of the specified type.
    unsafe fn sample_ptr_get(
        &self,
        sample_ptr: *const std::ffi::c_void,
        type_name: &str,
    ) -> *const std::ffi::c_void {
        // SAFETY: sample_ptr is guaranteed to be valid per the caller's contract.
        // The C++ implementation handles type checking and data retrieval safely.
        let type_name = StringView::from(type_name);
        unsafe { mw_com_get_sample_ptr(sample_ptr, type_name) }
    }

    /// Delete SamplePtr<T>
    ///
    /// # Arguments
    /// * `sample_ptr` - Opaque sample pointer
    /// * `type_name` - Type name string
    /// # Safety
    /// sample_ptr must point to a valid SamplePtr<T> of the specified type.
    unsafe fn sample_ptr_delete(&self, sample_ptr: *mut std::ffi::c_void, type_name: &str) {
        // SAFETY: sample_ptr is guaranteed to be valid per the caller's contract.
        // The C++ implementation handles type checking and deletion safely.
        let type_name = StringView::from(type_name);
        unsafe {
            mw_com_delete_sample_ptr(sample_ptr, type_name);
        }
    }

    /// Unsafe wrapper around mw_com_skeleton_offer_service
    ///
    /// # Arguments
    /// * `skeleton_ptr` - Opaque skeleton pointer
    ///
    /// # Returns
    /// true if offer was successful, false otherwise
    ///
    /// # Safety
    /// skeleton_ptr must be a valid pointer to a SkeletonBase previously created
    /// with create_skeleton().
    /// The pointer must remain valid for the duration of this call.
    unsafe fn skeleton_offer_service(&self, skeleton_ptr: *mut SkeletonBase) -> bool {
        // SAFETY: skeleton_ptr is guaranteed to be valid per the caller's contract.
        // The C++ implementation handles service offering safely.
        unsafe { mw_com_skeleton_offer_service(skeleton_ptr) }
    }

    /// Unsafe wrapper around mw_com_skeleton_stop_offer_service
    ///
    /// # Arguments
    /// * `skeleton_ptr` - Opaque skeleton pointer
    ///
    /// # Safety
    /// skeleton_ptr must be a valid pointer to a SkeletonBase previously created
    /// with create_skeleton().
    /// The pointer must remain valid for the duration of this call.
    unsafe fn skeleton_stop_offer_service(&self, skeleton_ptr: *mut SkeletonBase) {
        // SAFETY: skeleton_ptr is guaranteed to be valid per the caller's contract.
        // The C++ implementation handles stopping the service safely.
        unsafe {
            mw_com_skeleton_stop_offer_service(skeleton_ptr);
        }
    }

    /// Unsafe wrapper around mw_com_create_proxy
    ///
    /// # Arguments
    /// * `interface_id` - Interface UID string
    /// * `handle_ptr` - Opaque handle pointer
    ///
    /// # Returns
    /// Opaque proxy pointer, or nullptr if creation failed
    ///
    /// # Safety
    /// handle_ptr must be a valid reference to a HandleType.
    /// The returned pointer must eventually be destroyed via destroy_proxy().
    unsafe fn create_proxy(&self, interface_id: &str, handle_ptr: &HandleType) -> *mut ProxyBase {
        // SAFETY: interface_id is a valid string reference and handle_ptr is guaranteed to be valid
        // per the caller's contract.
        // The C++ implementation creates and returns a valid proxy pointer or nullptr on failure.
        let c_uid = StringView::from(interface_id);
        unsafe { mw_com_create_proxy(c_uid, handle_ptr) }
    }

    /// Unsafe wrapper around mw_com_create_skeleton
    ///
    /// # Arguments
    /// * `interface_id` - Interface UID string
    /// * `instance_spec` - Opaque instance specifier pointer
    ///
    /// # Returns
    /// Opaque skeleton pointer, or nullptr if creation failed
    ///
    /// # Safety
    /// instance_spec must be a valid NativeInstanceSpecifier.
    /// The returned pointer must eventually be destroyed via destroy_skeleton().
    unsafe fn create_skeleton(
        &self,
        interface_id: &str,
        instance_spec: *const NativeInstanceSpecifier,
    ) -> *mut SkeletonBase {
        // SAFETY: interface_id is a valid string reference and instance_spec is guaranteed to be
        // valid per the caller's contract.
        // The C++ implementation creates and returns a valid skeleton pointer or nullptr on failure.
        let c_uid = StringView::from(interface_id);
        unsafe { mw_com_create_skeleton(c_uid, instance_spec) }
    }

    /// Unsafe wrapper around mw_com_destroy_proxy
    ///
    /// # Arguments
    /// * `proxy_ptr` - Opaque proxy pointer to destroy
    ///
    /// # Safety
    /// proxy_ptr must be a valid pointer returned from create_proxy() that has not been destroyed yet.
    /// The caller must ensure no other references to this proxy exist after calling this function.
    unsafe fn destroy_proxy(&self, proxy_ptr: *mut ProxyBase) {
        // SAFETY: proxy_ptr is guaranteed to be valid per the caller's contract.
        // The C++ implementation safely deallocates the proxy.
        unsafe {
            mw_com_destroy_proxy(proxy_ptr);
        }
    }

    /// Unsafe wrapper around mw_com_destroy_skeleton
    ///
    /// # Arguments
    /// * `skeleton_ptr` - Opaque skeleton pointer to destroy
    ///
    /// # Safety
    /// skeleton_ptr must be a valid pointer returned from create_skeleton()-
    /// that has not been destroyed yet.
    /// The caller must ensure no other references to this skeleton exist after calling this function.
    unsafe fn destroy_skeleton(&self, skeleton_ptr: *mut SkeletonBase) {
        // SAFETY: skeleton_ptr is guaranteed to be valid per the caller's contract.
        // The C++ implementation safely deallocates the skeleton.
        unsafe {
            mw_com_destroy_skeleton(skeleton_ptr);
        }
    }

    /// Unsafe wrapper around mw_com_get_event_from_proxy
    ///
    /// # Arguments
    /// * `proxy_ptr` - Opaque proxy pointer
    /// * `interface_id` - Interface UID string
    /// * `event_id` - Event name string
    ///
    /// # Returns
    /// Opaque event pointer, or nullptr if not found
    ///
    /// # Safety
    /// proxy_ptr must be a valid pointer to a ProxyBase previously created with create_proxy().
    /// The returned pointer remains valid only as long as the proxy remains alive.
    unsafe fn get_event_from_proxy(
        &self,
        proxy_ptr: *mut ProxyBase,
        interface_id: &str,
        event_id: &str,
    ) -> *mut ProxyEventBase {
        // SAFETY: proxy_ptr is guaranteed to be valid per the caller's contract.
        // The C++ implementation returns a valid pointer or nullptr if the event is not found.
        let c_id = StringView::from(interface_id);
        let c_name = StringView::from(event_id);
        unsafe { mw_com_get_event_from_proxy(proxy_ptr, c_id, c_name) }
    }

    /// Unsafe wrapper around mw_com_get_event_from_skeleton
    ///
    /// # Arguments
    /// * `skeleton_ptr` - Opaque skeleton pointer
    /// * `interface_id` - Interface UID string
    /// * `event_id` - Event name string
    ///
    /// # Returns
    /// Opaque event pointer, or nullptr if not found
    ///
    /// # Safety
    /// skeleton_ptr must be a valid pointer to a SkeletonBase previously created
    /// with create_skeleton().
    /// The returned pointer remains valid only as long as the skeleton remains alive.
    unsafe fn get_event_from_skeleton(
        &self,
        skeleton_ptr: *mut SkeletonBase,
        interface_id: &str,
        event_id: &str,
    ) -> *mut SkeletonEventBase {
        // SAFETY: skeleton_ptr is guaranteed to be valid per the caller's contract.
        // The C++ implementation returns a valid pointer or nullptr if the event is not found.
        let c_id = StringView::from(interface_id);
        let c_name = StringView::from(event_id);
        unsafe { mw_com_get_event_from_skeleton(skeleton_ptr, c_id, c_name) }
    }

    /// Unsafe wrapper around mw_com_get_samples_from_event
    ///
    /// # Arguments
    /// * `event_ptr` - Opaque event pointer
    /// * `event_type` - Event type name string
    /// * `callback` - FatPtr to callback function
    /// * `max_samples` - Maximum number of samples to retrieve
    ///
    /// # Returns
    /// Number of samples retrieved, or u32::MAX on error
    ///
    /// # Safety
    /// event_ptr must be a valid pointer to a ProxyEventBase previously obtained
    /// from get_event_from_proxy().
    /// callback must be a valid FatPtr referencing a callable compatible with the event type.
    /// The event must have been subscribed to via subscribe_to_event() before calling this function.
    unsafe fn get_samples_from_event(
        &self,
        event_ptr: *mut ProxyEventBase,
        event_type: &str,
        callback: &FatPtr,
        max_samples: u32,
    ) -> u32 {
        // SAFETY: event_ptr, callback, and event_type are guaranteed
        // to be valid per the caller's contract.
        // The C++ implementation handles sample retrieval and callback invocation safely.
        let c_name = StringView::from(event_type);
        unsafe {
            mw_com_type_registry_get_samples_from_event(event_ptr, c_name, callback, max_samples)
        }
    }

    /// Unsafe wrapper around mw_com_skeleton_send_event
    ///
    /// # Arguments
    /// * `event_ptr` - Opaque skeleton event pointer
    /// * `event_type` - Event type name string
    /// * `data_ptr` - Pointer to event data of the matching type
    ///
    /// # Safety
    /// event_ptr must be a valid pointer to a SkeletonEventBase previously obtained
    /// from get_event_from_skeleton().
    /// data_ptr must point to valid data whose type matches the event_type.
    /// The lifetime of the data must extend through this function call.
    unsafe fn skeleton_send_event(
        &self,
        event_ptr: *mut SkeletonEventBase,
        event_type: &str,
        data_ptr: *const std::ffi::c_void,
    ) -> bool {
        // SAFETY: event_ptr and data_ptr are guaranteed to be valid per the caller's contract.
        // The C++ implementation handles type matching and data sending safely.
        let c_name = StringView::from(event_type);
        unsafe { mw_com_skeleton_send_event(event_ptr, c_name, data_ptr) }
    }

    /// Unsafe wrapper around mw_com_proxy_event_subscribe
    ///
    /// # Arguments
    /// * `event_ptr` - Opaque event pointer
    /// * `max_sample_count` - Maximum number of samples to buffer concurrently
    ///
    /// # Returns
    /// true if subscription was successful, false otherwise
    ///
    /// # Safety
    /// event_ptr must be a valid pointer to a ProxyEventBase previously obtained
    /// from get_event_from_proxy().
    /// This function must be called before attempting to retrieve samples via get_samples_from_event().
    unsafe fn subscribe_to_event(
        &self,
        event_ptr: *mut ProxyEventBase,
        max_sample_count: u32,
    ) -> bool {
        // SAFETY: event_ptr is guaranteed to be valid per the caller's contract.
        // The C++ implementation handles subscription and buffer allocation safely.
        unsafe { mw_com_proxy_event_subscribe(event_ptr, max_sample_count) }
    }

    /// Unsafe wrapper around mw_com_proxy_event_unsubscribe
    ///
    /// # Arguments
    /// * `event_ptr` - Opaque event pointer
    ///
    /// # Safety
    /// event_ptr must be a valid pointer to a ProxyEventBase previously obtained from get_event_from_proxy().
    /// This function should be called only when no `SamplePtr` is held by the user for this event.
    unsafe fn unsubscribe_to_event(&self, event_ptr: *mut ProxyEventBase) {
        // SAFETY: event_ptr is guaranteed to be valid per the caller's contract.
        // The C++ implementation handles unsubscription and buffer cleanup safely.
        unsafe {
            mw_com_proxy_event_unsubscribe(event_ptr);
        }
    }

    /// Unsafe wrapper around mw_com_proxy_set_event_receive_handler
    ///
    /// # Arguments
    /// * `proxy_event_ptr` - Opaque proxy event pointer
    /// * `handler` - FatPtr to event receive handler function
    ///
    /// # Returns
    /// true if handler was set successfully, false otherwise
    ///
    /// # Safety
    /// proxy_event_ptr must be a valid pointer to a ProxyEventBase previously obtained
    /// from get_event_from_proxy().
    /// handler must be a valid FatPtr which is used for notifying the proxy of incoming events.
    unsafe fn set_event_receive_handler(
        &self,
        proxy_event_ptr: *mut ProxyEventBase,
        handler: &FatPtr,
        event_type: &str,
    ) -> bool {
        // SAFETY: proxy_event_ptr must be valid per the caller's contract,
        // and handler must be a valid FatPtr referencing a callable compatible with the callback
        // signature expected by the C++ implementation.
        let c_name = StringView::from(event_type);
        unsafe { mw_com_proxy_set_event_receive_handler(proxy_event_ptr, handler, c_name) }
    }

    /// Unsafe wrapper around mw_com_proxy_clear_event_receive_handler
    ///
    /// # Arguments
    /// * `proxy_event_ptr` - Opaque proxy event pointer
    /// * `event_type` - Event type name string
    ///
    /// # Safety
    /// proxy_event_ptr must be a valid pointer to a ProxyEventBase previously obtained
    /// from get_event_from_proxy().
    /// event_type must be a valid string corresponding to the event type.
    unsafe fn clear_event_receive_handler(
        &self,
        proxy_event_ptr: *mut ProxyEventBase,
        event_type: &str,
    ) {
        // SAFETY: proxy_event_ptr must be valid per the caller's contract.
        let c_name = StringView::from(event_type);
        unsafe {
            mw_com_proxy_clear_event_receive_handler(proxy_event_ptr, c_name);
        }
    }

    /// Unsafe wrapper around mw_com_start_find_service
    ///
    /// # Arguments
    /// * `callback` - FatPtr to callback function to be invoked when services are found
    /// * `instance_spec` - InstanceSpecifier identifying the service instance to find
    ///
    /// # Returns
    /// Opaque handle pointer for the find service operation, or nullptr on failure
    ///
    /// # Safety
    /// callback must be a valid FatPtr referencing a callable compatible with the find service results.
    /// instance_spec must be a valid InstanceSpecifier for the service to find.
    unsafe fn start_find_service(
        &self,
        callback: &FatPtr,
        instance_spec: InstanceSpecifier,
    ) -> *mut FindServiceHandle {
        // SAFETY: callback and instance_spec are guaranteed to be valid per the caller's contract.
        // The C++ implementation handles the find service operation and callback invocation safely.
        unsafe { mw_com_start_find_service(callback, instance_spec.as_native()) }
    }

    /// Unsafe wrapper around mw_com_stop_find_service
    ///
    /// # Arguments
    /// * `handle` - Opaque handle pointer returned from start_find_service()
    ///
    /// # Safety
    /// handle must be a valid pointer returned from start_find_service() that has not been stopped yet,
    /// and the caller must ensure no further use of this handle after calling this function.
    unsafe fn stop_find_service(&self, handle: *mut FindServiceHandle) {
        // SAFETY: handle is valid per the caller's contract and has not been stopped yet.
        // The C++ implementation handles stopping the find service safely.
        unsafe {
            mw_com_stop_find_service(handle);
        }
    }

    /// Get the number of service handles in the container
    ///
    /// # Arguments
    /// * `container` - HandleContainer wrapping the native service handle container
    /// # Returns
    /// The number of service handles in the container
    fn handle_container_size(&self, container: &HandleContainer) -> usize {
        container.len()
    }
    /// Get a reference to the service handle at the specified index in the container
    ///
    /// # Arguments
    /// * `container` - HandleContainer wrapping the native service handle container
    /// * `index` - Index of the service handle to retrieve
    ///
    /// # Returns
    /// Some reference to the service handle at the specified index,
    /// or None if the index is out of bounds
    fn handle_container_get_at<'a>(
        &self,
        container: &'a HandleContainer,
        index: usize,
    ) -> Option<&'a HandleType> {
        container.get(index)
    }
}
