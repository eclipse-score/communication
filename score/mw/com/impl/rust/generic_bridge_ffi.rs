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

use std::ffi::c_char;

/// Opaque C++ void* pointer wrapper
pub type CVoidPtr = *const std::ffi::c_void;
pub type CMutVoidPtr = *mut std::ffi::c_void;

pub use proxy_bridge_rs::FatPtr;
pub use proxy_bridge_rs::HandleType;
pub use proxy_bridge_rs::ProxyEventBase;
pub use proxy_bridge_rs::ProxyWrapperClass;

pub use proxy_bridge_rs::NativeInstanceSpecifier;

/// Opaque proxy base struct
#[repr(C)]
pub struct ProxyBase {
    dummy: [u8; 0],
}

/// Opaque skeleton base struct
#[repr(C)]
pub struct SkeletonBase {
    dummy: [u8; 0],
}

/// Opaque skeleton event base struct
#[repr(C)]
pub struct SkeletonEventBase {
    dummy: [u8; 0],
}

/// String view similar to C++'s std::string_view
/// Holds a pointer to a string and its length without requiring null termination
#[repr(C)]
pub struct StringView {
    data: *const c_char,
    len: u32,
}

impl From<&'_ str> for StringView {
    fn from(s: &str) -> Self {
        if s.is_empty() {
            StringView {
                data: std::ptr::null(),
                len: 0,
            }
        } else {
            StringView {
                data: s.as_ptr() as *const c_char,
                len: s.len() as u32,
            }
        }
    }
}

/// Generic Rust closure invocation for all types
///
/// This function is called by C++ to invoke a Rust closure with a sample pointer.
/// The closure is reconstructed from the FatPtr trait object.
///
/// # Safety
/// - `ptr` must point to a valid FatPtr
/// - `sample_ptr` must point to valid placement-new storage containing SamplePtr<T>
#[no_mangle]
pub unsafe extern "C" fn mw_com_impl_call_dyn_ref_fnmut_sample(
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
    let callable: &mut dyn FnMut(*mut std::ffi::c_void) = std::mem::transmute(*ptr);

    // Invoke the closure with the void* sample pointer
    callable(sample_ptr);
}

// FFI declarations for C++ generic event bridge functions
extern "C" {

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
    );

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
pub unsafe fn sample_ptr_get(
    sample_ptr: *const std::ffi::c_void,
    type_name: &str,
) -> *const std::ffi::c_void {
    // SAFETY: sample_ptr is guaranteed to be valid per the caller's contract.
    // The C++ implementation handles type checking and data retrieval safely.
    let type_name = StringView::from(type_name);
    mw_com_get_sample_ptr(sample_ptr, type_name)
}

/// Delete SamplePtr<T>
///
/// # Arguments
/// * `sample_ptr` - Opaque sample pointer
/// * `type_name` - Type name string
/// # Safety
/// sample_ptr must point to a valid SamplePtr<T> of the specified type.
pub unsafe fn sample_ptr_delete(sample_ptr: *mut std::ffi::c_void, type_name: &str) {
    // SAFETY: sample_ptr is guaranteed to be valid per the caller's contract.
    // The C++ implementation handles type checking and deletion safely.
    let type_name = StringView::from(type_name);
    mw_com_delete_sample_ptr(sample_ptr, type_name);
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
/// skeleton_ptr must be a valid pointer to a SkeletonBase previously created with create_skeleton().
/// The pointer must remain valid for the duration of this call.
pub unsafe fn skeleton_offer_service(skeleton_ptr: *mut SkeletonBase) -> bool {
    // SAFETY: skeleton_ptr is guaranteed to be valid per the caller's contract.
    // The C++ implementation handles service offering safely.
    mw_com_skeleton_offer_service(skeleton_ptr)
}

/// Unsafe wrapper around mw_com_skeleton_stop_offer_service
///
/// # Arguments
/// * `skeleton_ptr` - Opaque skeleton pointer
///
/// # Safety
/// skeleton_ptr must be a valid pointer to a SkeletonBase previously created with create_skeleton().
/// The pointer must remain valid for the duration of this call.
pub unsafe fn skeleton_stop_offer_service(skeleton_ptr: *mut SkeletonBase) {
    // SAFETY: skeleton_ptr is guaranteed to be valid per the caller's contract.
    // The C++ implementation handles stopping the service safely.
    mw_com_skeleton_stop_offer_service(skeleton_ptr);
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
pub unsafe fn create_proxy(interface_id: &str, handle_ptr: &HandleType) -> *mut ProxyBase {
    // SAFETY: interface_id is a valid string reference and handle_ptr is guaranteed to be valid per the caller's contract.
    // The C++ implementation creates and returns a valid proxy pointer or nullptr on failure.
    let c_uid = StringView::from(interface_id);
    mw_com_create_proxy(c_uid, handle_ptr)
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
pub unsafe fn create_skeleton(
    interface_id: &str,
    instance_spec: *const NativeInstanceSpecifier,
) -> *mut SkeletonBase {
    // SAFETY: interface_id is a valid string reference and instance_spec is guaranteed to be valid per the caller's contract.
    // The C++ implementation creates and returns a valid skeleton pointer or nullptr on failure.
    let c_uid = StringView::from(interface_id);
    mw_com_create_skeleton(c_uid, instance_spec)
}

/// Unsafe wrapper around mw_com_destroy_proxy
///
/// # Arguments
/// * `proxy_ptr` - Opaque proxy pointer to destroy
///
/// # Safety
/// proxy_ptr must be a valid pointer returned from create_proxy() that has not been destroyed yet.
/// The caller must ensure no other references to this proxy exist after calling this function.
pub unsafe fn destroy_proxy(proxy_ptr: *mut ProxyBase) {
    // SAFETY: proxy_ptr is guaranteed to be valid per the caller's contract.
    // The C++ implementation safely deallocates the proxy.
    mw_com_destroy_proxy(proxy_ptr);
}

/// Unsafe wrapper around mw_com_destroy_skeleton
///
/// # Arguments
/// * `skeleton_ptr` - Opaque skeleton pointer to destroy
///
/// # Safety
/// skeleton_ptr must be a valid pointer returned from create_skeleton() that has not been destroyed yet.
/// The caller must ensure no other references to this skeleton exist after calling this function.
pub unsafe fn destroy_skeleton(skeleton_ptr: *mut SkeletonBase) {
    // SAFETY: skeleton_ptr is guaranteed to be valid per the caller's contract.
    // The C++ implementation safely deallocates the skeleton.
    mw_com_destroy_skeleton(skeleton_ptr);
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
pub unsafe fn get_event_from_proxy(
    proxy_ptr: *mut ProxyBase,
    interface_id: &str,
    event_id: &str,
) -> *mut ProxyEventBase {
    // SAFETY: proxy_ptr is guaranteed to be valid per the caller's contract.
    // The C++ implementation returns a valid pointer or nullptr if the event is not found.
    let c_id = StringView::from(interface_id);
    let c_name = StringView::from(event_id);
    mw_com_get_event_from_proxy(proxy_ptr, c_id, c_name)
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
/// skeleton_ptr must be a valid pointer to a SkeletonBase previously created with create_skeleton().
/// The returned pointer remains valid only as long as the skeleton remains alive.
pub unsafe fn get_event_from_skeleton(
    skeleton_ptr: *mut SkeletonBase,
    interface_id: &str,
    event_id: &str,
) -> *mut SkeletonEventBase {
    // SAFETY: skeleton_ptr is guaranteed to be valid per the caller's contract.
    // The C++ implementation returns a valid pointer or nullptr if the event is not found.
    let c_id = StringView::from(interface_id);
    let c_name = StringView::from(event_id);
    mw_com_get_event_from_skeleton(skeleton_ptr, c_id, c_name)
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
/// event_ptr must be a valid pointer to a ProxyEventBase previously obtained from get_event_from_proxy().
/// callback must be a valid FatPtr referencing a callable compatible with the event type.
/// The event must have been subscribed to via subscribe_to_event() before calling this function.
pub unsafe fn get_samples_from_event(
    event_ptr: *mut ProxyEventBase,
    event_type: &str,
    callback: &FatPtr,
    max_samples: u32,
) -> u32 {
    // SAFETY: event_ptr, callback, and event_type are guaranteed to be valid per the caller's contract.
    // The C++ implementation handles sample retrieval and callback invocation safely.
    let c_name = StringView::from(event_type);
    mw_com_type_registry_get_samples_from_event(event_ptr, c_name, callback, max_samples)
}

/// Unsafe wrapper around mw_com_skeleton_send_event
///
/// # Arguments
/// * `event_ptr` - Opaque skeleton event pointer
/// * `event_type` - Event type name string
/// * `data_ptr` - Pointer to event data of the matching type
///
/// # Safety
/// event_ptr must be a valid pointer to a SkeletonEventBase previously obtained from get_event_from_skeleton().
/// data_ptr must point to valid data whose type matches the event_type.
/// The lifetime of the data must extend through this function call.
pub unsafe fn skeleton_send_event(
    event_ptr: *mut SkeletonEventBase,
    event_type: &str,
    data_ptr: *const std::ffi::c_void,
) {
    // SAFETY: event_ptr and data_ptr are guaranteed to be valid per the caller's contract.
    // The C++ implementation handles type matching and data sending safely.
    let c_name = StringView::from(event_type);
    mw_com_skeleton_send_event(event_ptr, c_name, data_ptr);
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
/// event_ptr must be a valid pointer to a ProxyEventBase previously obtained from get_event_from_proxy().
/// This function must be called before attempting to retrieve samples via get_samples_from_event().
pub unsafe fn subscribe_to_event(event_ptr: *mut ProxyEventBase, max_sample_count: u32) -> bool {
    // SAFETY: event_ptr is guaranteed to be valid per the caller's contract.
    // The C++ implementation handles subscription and buffer allocation safely.
    mw_com_proxy_event_subscribe(event_ptr, max_sample_count)
}
