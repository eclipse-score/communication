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
use score_log as log;
use std::ffi::CString;
use std::path::Path;
use std::ptr::NonNull;

#[derive(Debug, Clone, Default)]
/// unit struct representing the FFI bridge for Lola runtime
pub struct LolaFFIBridge;

/// Called by C++ (`RustBoxedCallable<void>::invoke`) to fire a Rust `FnMut()` receive handler.
///
/// The pointer was originally created as `Box::into_raw(Box::new(handler) as Box<dyn FnMut() + Send + 'static>)`
/// and passed to C++ via `mw_com_impl_proxy_event_set_receive_handler`.
///
/// # Safety
/// `ptr` must point to a valid `FatPtr` whose payload is a live
/// `Box<dyn FnMut() + Send + 'static>` that has not yet been dropped.
#[unsafe(no_mangle)]
unsafe extern "C" fn mw_com_impl_call_dyn_fnmut(ptr: *const FatPtr) {
    // SAFETY: caller guarantees ptr is valid; transmute reconstructs the fat pointer.
    let dyn_fnmut: *mut (dyn FnMut() + Send + 'static) = unsafe { std::mem::transmute(*ptr) };
    // SAFETY: the box is still alive (C++ only calls dispose after all invocations finish).
    if std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| unsafe { (*dyn_fnmut)() })).is_err()
    {
        log::error!("Panic caught in mw_com_impl_call_dyn_fnmut: aborting to prevent unwind across FFI boundary");
        // Abort to prevent a Rust panic from unwinding across the C++ stack boundary.
        std::process::abort();
    }
}

/// Called by C++ (`RustBoxedCallable<void>::dispose`) to drop the boxed receive handler.
///
/// # Safety
/// `ptr` must point to the same `FatPtr` that was previously passed to
/// `mw_com_impl_proxy_event_set_receive_handler`, and must only be called once.
#[unsafe(no_mangle)]
unsafe extern "C" fn mw_com_impl_delete_boxed_fnmut(ptr: *mut FatPtr) {
    // SAFETY: caller guarantees ptr is valid and this is the single dispose call.
    let dyn_fnmut: *mut (dyn FnMut() + Send + 'static) = unsafe { std::mem::transmute(*ptr) };
    drop(unsafe { Box::from_raw(dyn_fnmut) });
}

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

    // Invoke the closure with the void* sample pointer.
    // catch_unwind prevents a Rust panic from unwinding across the C++ stack boundary.
    if std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| callable(sample_ptr))).is_err() {
        log::error!("Panic caught in mw_com_impl_call_dyn_ref_fnmut_sample: aborting to prevent unwind across FFI boundary");
        // Abort to prevent unwinding across FFI boundary
        std::process::abort();
    }
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

    // Invoke with correct types.
    // catch_unwind prevents a Rust panic from unwinding across the C++ stack boundary.
    if std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        callable(
            HandleContainer::new(service_handles),
            NativeFindServiceHandle::new(find_service_handle),
        )
    }))
    .is_err()
    {
        log::error!("Panic caught in mw_com_impl_call_dyn_ref_fnmut_find_service: aborting to prevent unwind across FFI boundary");
        // Abort to prevent unwinding across FFI boundary
        std::process::abort();
    }
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
    /// * `type_ops` - Pointer to TypeOperations instance
    /// * `callback` - FatPtr to callback function
    /// * `max_samples` - Maximum number of samples to retrieve
    ///
    /// # Returns
    /// Number of samples retrieved
    fn mw_com_type_registry_get_samples_from_event(
        event_ptr: *mut ProxyEventBase,
        type_ops: *const TypeOperations,
        callback: *const FatPtr,
        max_samples: u32,
    ) -> u32;

    /// Send data via skeleton event
    ///
    /// # Arguments
    /// * `event_ptr` - Opaque skeleton event pointer
    /// * `type_ops` - Pointer to TypeOperations instance
    /// * `data_ptr` - Pointer to event data
    ///
    /// # Returns
    /// None (void function)
    fn mw_com_skeleton_send_event(
        event_ptr: *mut SkeletonEventBase,
        type_ops: *const TypeOperations,
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
    /// * `type_ops` - Pointer to TypeOperations instance
    ///
    /// # Returns
    /// Pointer to sample data, or nullptr if type mismatch
    fn mw_com_get_sample_ptr(
        sample_ptr: *const std::ffi::c_void,
        type_ops: *const TypeOperations,
    ) -> *const std::ffi::c_void;

    /// Delete sample pointer of SamplePtr<T>'
    ///
    /// # Arguments
    /// * `sample_ptr` - Opaque sample pointer
    /// * `type_ops` - Pointer to TypeOperations instance
    fn mw_com_delete_sample_ptr(sample_ptr: *mut std::ffi::c_void, type_ops: *const TypeOperations);

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
        type_ops: *const TypeOperations,
    ) -> bool;

    /// Delete allocatee pointer of SampleAllocateePtr<T>
    ///
    /// # Arguments
    /// * `allocatee_ptr` - Pointer to SampleAllocateePtr<T>
    /// * `type_ops` - Pointer to TypeOperations instance
    fn mw_com_delete_allocatee_ptr(
        allocatee_ptr: *mut std::ffi::c_void,
        type_ops: *const TypeOperations,
    );

    /// Get allocatee data pointer from allocatee of specific type
    ///
    /// # Arguments
    /// * `allocatee_ptr` - Pointer to SampleAllocateePtr<T>
    /// * `type_ops` - Pointer to TypeOperations instance
    ///
    /// # Returns
    /// Pointer to allocatee data, or nullptr if type mismatch
    fn mw_com_get_allocatee_data_ptr(
        allocatee_ptr: *const std::ffi::c_void,
        type_ops: *const TypeOperations,
    ) -> *mut std::ffi::c_void;

    /// Send event via skeleton using allocatee pointer of specific type
    ///
    /// # Arguments
    /// * `event_ptr` - Opaque skeleton event pointer
    /// * `type_ops` - Pointer to TypeOperations instance
    /// * `allocatee_ptr` - Pointer to SampleAllocateePtr<T>
    ///
    /// # Returns
    /// True if event was sent successfully, false otherwise
    fn mw_com_skeleton_send_event_allocatee(
        event_ptr: *mut SkeletonEventBase,
        type_ops: *const TypeOperations,
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
    ) -> bool;

    /// Clear event receive handler for proxy event
    ///
    /// # Arguments
    /// * `proxy_event_ptr` - Opaque proxy event pointer
    fn mw_com_proxy_clear_event_receive_handler(proxy_event_ptr: *mut ProxyEventBase);

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

    /// Get type operations instance for a given interface and member
    ///
    /// # Arguments
    /// * `interface_id` - UTF-8 string view of interface ID
    /// * `member_name` - UTF-8 string view of member name (event name Id)
    ///
    /// # Returns
    /// Pointer to TypeOperations instance, or nullptr on failure
    fn mw_com_get_type_ops_instance(
        interface_id: StringView,
        member_name: StringView,
    ) -> *mut TypeOperations;

    /// Find available service instances and return a pointer to NativeHandleContainer
    ///
    /// # Arguments
    /// * `instance_specifier` - Pointer to NativeInstanceSpecifier for the service to find
    ///
    /// # Returns
    /// Pointer to NativeHandleContainer containing available service instances, or nullptr if none found
    fn mw_com_impl_find_service(
        instance_specifier: *mut NativeInstanceSpecifier,
    ) -> *mut NativeHandleContainer;

    /// Initialize the communication implementation with the provided configuration.
    ///
    /// # Arguments
    /// * `options` - Pointer to an array of UTF-8 strings representing configuration options
    /// * `len` - Number of configuration arguments in the array
    fn mw_com_impl_initialize(options: *mut *const std::ffi::c_char, len: i32);

    ///This function just for validating the size of SamplePtr<T> in C++ and Rust are same.
    fn mw_com_impl_sample_ptr_get_size() -> u32;
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
    ///
    /// Note: We cannot use SampleAllocateePtr<T> directly in the FFIBridge trait methods,
    /// because the underlying extern "C" functions use *mut c_void (C linkage has no generics).
    /// Type-specific operations are instead delegated to TypeOperationsManager, which resolves
    /// the concrete type at runtime via the C++ type registry.
    unsafe fn get_allocatee_ptr(
        &self,
        event_ptr: *mut SkeletonEventBase,
        allocatee_ptr: *mut std::ffi::c_void,
        type_ops: &TypeOperationsManager,
    ) -> bool {
        // SAFETY: event_ptr is valid which is created using create_skeleton()
        // and allocatee_ptr has enough memory allocated for the construction of allocatee instance.
        unsafe { mw_com_get_allocatee_ptr(event_ptr, allocatee_ptr, type_ops.as_ptr()) }
    }

    /// Delete allocatee pointer of SampleAllocateePtr<T>
    ///
    /// # Arguments
    /// * `allocatee_ptr` - type erased pointer to SampleAllocateePtr<T>
    /// * `type_ops` - Reference to TypeOperationsManager for the type
    ///
    /// # Safety
    /// `allocatee_ptr` must be a valid pointer previously returned by `get_allocatee_ptr`
    /// and `type_ops` must be the corresponding `TypeOperationsManager` for the type T.
    unsafe fn delete_allocatee_ptr(
        &self,
        allocatee_ptr: *mut std::ffi::c_void,
        type_ops: &TypeOperationsManager,
    ) {
        // SAFETY: allocatee_ptr is valid which is created using get_allocatee_ptr() and
        // type_ops provides the correct type operations for this allocatee.
        unsafe {
            mw_com_delete_allocatee_ptr(allocatee_ptr, type_ops.as_ptr());
        }
    }

    /// Get allocatee data pointer from allocatee of specific type
    ///
    /// # Arguments
    /// * `allocatee_ptr` - type erased pointer to SampleAllocateePtr<T>
    /// * `type_ops` - Reference to TypeOperationsManager for the type
    ///
    /// # Returns
    /// Pointer to allocatee data, or nullptr if type mismatch
    ///
    /// # Safety
    /// `allocatee_ptr` must be a valid pointer previously returned by `get_allocatee_ptr`
    /// and `type_ops` must be the corresponding `TypeOperationsManager` for type T.
    unsafe fn get_allocatee_data_ptr(
        &self,
        allocatee_ptr: *const std::ffi::c_void,
        type_ops: &TypeOperationsManager,
    ) -> *mut std::ffi::c_void {
        // SAFETY: allocatee_ptr is valid which is created using get_allocatee_ptr() and
        // type_ops provides the correct type operations for this allocatee.
        unsafe { mw_com_get_allocatee_data_ptr(allocatee_ptr, type_ops.as_ptr()) }
    }

    /// Send event via skeleton using allocatee pointer of specific type
    ///
    /// # Arguments
    /// * `event_ptr` - Opaque skeleton event pointer
    /// * `type_ops` - Reference to TypeOperationsManager for the type
    /// * `allocatee_ptr` - type erased pointer to SampleAllocateePtr<T>
    ///
    /// # Returns
    /// True if event was sent successfully, false otherwise
    ///
    /// # Safety
    /// `event_ptr` must be a valid pointer to a `SkeletonEventBase` obtained from
    /// `get_event_from_skeleton`. `allocatee_ptr` must be a valid `SampleAllocateePtr<T>`
    /// previously returned by `get_allocatee_ptr` for the same event and type T, and `type_ops`
    /// must be the corresponding `TypeOperationsManager` for type T.
    unsafe fn skeleton_event_send_sample_allocatee(
        &self,
        event_ptr: *mut SkeletonEventBase,
        type_ops: &TypeOperationsManager,
        allocatee_ptr: *const std::ffi::c_void,
    ) -> bool {
        // SAFETY: event_ptr is valid which is created using create_skeleton(), allocatee_ptr is
        // valid which is created using get_allocatee_ptr(), and type_ops provides the correct
        // type operations for this allocatee.
        unsafe { mw_com_skeleton_send_event_allocatee(event_ptr, type_ops.as_ptr(), allocatee_ptr) }
    }

    /// Get SamplePtr<T> data pointer
    ///
    /// # Arguments
    /// * `sample_ptr` - Opaque sample pointer
    /// * `type_ops` - Reference to TypeOperationsManager for the type
    ///
    /// # Returns
    /// Pointer to sample data, or nullptr if type mismatch
    ///
    /// # Safety
    /// `sample_ptr` must be a valid pointer to a `SamplePtr<T>` of the specified `type_name`.
    unsafe fn sample_ptr_get(
        &self,
        sample_ptr: *const std::ffi::c_void,
        type_ops: &TypeOperationsManager,
    ) -> *const std::ffi::c_void {
        // SAFETY: sample_ptr is guaranteed to be valid per the caller's contract.
        // The C++ implementation handles type checking and data retrieval safely using type_ops.
        unsafe { mw_com_get_sample_ptr(sample_ptr, type_ops.as_ptr()) }
    }

    /// Delete SamplePtr<T>
    ///
    /// # Arguments
    /// * `sample_ptr` - Opaque sample pointer
    /// * `type_ops` - Reference to TypeOperationsManager for the type
    ///
    /// # Safety
    /// `sample_ptr` must be a valid pointer to a `SamplePtr<T>` of the specified `type_name`,
    /// and must not be used after this call.
    unsafe fn sample_ptr_delete(
        &self,
        sample_ptr: *mut std::ffi::c_void,
        type_ops: &TypeOperationsManager,
    ) {
        // SAFETY: sample_ptr is guaranteed to be valid per the caller's contract.
        // The C++ implementation handles type checking and deletion safely using type_ops.
        unsafe {
            mw_com_delete_sample_ptr(sample_ptr, type_ops.as_ptr());
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
    /// * `type_ops` - Reference to TypeOperationsManager for the type
    /// * `callback` - FatPtr to callback function
    /// * `max_samples` - Maximum number of samples to retrieve
    ///
    /// # Returns
    /// Number of samples retrieved, or u32::MAX on error
    ///
    /// # Safety
    /// `event_ptr` must be a valid pointer to a `ProxyEventBase` obtained from
    /// `get_event_from_proxy`. `callback` must be a valid `FatPtr` referencing a callable
    /// compatible with `event_type`. The event must have been subscribed via `subscribe_to_event`.
    unsafe fn get_samples_from_event(
        &self,
        event_ptr: *mut ProxyEventBase,
        type_ops: &TypeOperationsManager,
        callback: &FatPtr,
        max_samples: u32,
    ) -> u32 {
        // SAFETY: event_ptr and callback are guaranteed to be valid per the caller's contract.
        // type_ops provides the correct type operations for sample handling.
        // The C++ implementation handles sample retrieval and callback invocation safely.
        unsafe {
            mw_com_type_registry_get_samples_from_event(
                event_ptr,
                type_ops.as_ptr(),
                callback,
                max_samples,
            )
        }
    }

    /// Unsafe wrapper around mw_com_skeleton_send_event
    ///
    /// # Arguments
    /// * `event_ptr` - Opaque skeleton event pointer
    /// * `type_ops` - Reference to TypeOperationsManager for the type
    /// * `data_ptr` - Pointer to event data of the matching type
    ///
    /// # Safety
    /// `event_ptr` must be a valid pointer to a `SkeletonEventBase` obtained from
    /// `get_event_from_skeleton`. `data_ptr` must point to valid data whose type matches
    /// `event_type` and must remain valid for the duration of this call.
    unsafe fn skeleton_send_event(
        &self,
        event_ptr: *mut SkeletonEventBase,
        type_ops: &TypeOperationsManager,
        data_ptr: *const std::ffi::c_void,
    ) -> bool {
        // SAFETY: event_ptr and data_ptr are guaranteed to be valid per the caller's contract.
        // type_ops provides the correct type operations for this event.
        // The C++ implementation handles type matching and data sending safely.
        unsafe { mw_com_skeleton_send_event(event_ptr, type_ops.as_ptr(), data_ptr) }
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
    /// `proxy_event_ptr` must be a valid pointer to a `ProxyEventBase` obtained from
    /// `get_event_from_proxy`. `handler` must be a valid `FatPtr` referencing a callable
    /// compatible with the receive-handler signature expected by the implementation.
    unsafe fn set_event_receive_handler(
        &self,
        proxy_event_ptr: *mut ProxyEventBase,
        handler: &FatPtr,
    ) -> bool {
        // SAFETY: proxy_event_ptr must be valid per the caller's contract,
        // and handler must be a valid FatPtr referencing a callable compatible with the callback
        // signature expected by the C++ implementation.
        unsafe { mw_com_proxy_set_event_receive_handler(proxy_event_ptr, handler) }
    }

    /// Unsafe wrapper around mw_com_proxy_clear_event_receive_handler
    ///
    /// # Arguments
    /// * `proxy_event_ptr` - Opaque proxy event pointer
    ///
    /// # Safety
    /// `proxy_event_ptr` must be a valid pointer to a `ProxyEventBase` obtained from
    /// `get_event_from_proxy`. `event_type` must match the type used when the handler was set.
    unsafe fn clear_event_receive_handler(&self, proxy_event_ptr: *mut ProxyEventBase) {
        // SAFETY: proxy_event_ptr must be valid per the caller's contract.
        unsafe {
            mw_com_proxy_clear_event_receive_handler(proxy_event_ptr);
        }
    }

    /// Wrapper around mw_com_start_find_service
    ///
    /// # Arguments
    /// * `callback` - Valid find-service callable, its invariant is guaranteed by `FindServiceCallable`.
    /// * `instance_spec` - InstanceSpecifier identifying the service instance to find
    ///
    /// # Returns
    /// Opaque handle pointer for the find service operation, or nullptr on failure
    fn start_find_service(
        &self,
        callback: &FindServiceCallable,
        instance_spec: InstanceSpecifier,
    ) -> *mut FindServiceHandle {
        // SAFETY: FindServiceCallable guarantees that the inner FatPtr is a valid callable
        // compatible with the find-service callback signature.
        unsafe { mw_com_start_find_service(callback.as_fat_ptr(), instance_spec.as_native()) }
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

    /// Get type operations instance for a given interface and member ID
    ///
    /// # Arguments
    /// * `interface_id` - Interface ID string
    /// * `member_name` - Member name string (e.g., event name)
    ///
    /// # Returns
    /// Option<TypeOperationsManager> instance wrapping the pointer to TypeOperations, or None if retrieval fails
    ///
    /// # Safety
    /// Caller must ensure that the provided interface_id and member_name correspond to
    /// a valid TypeOperations instance in the C++ registry. The returned TypeOperationsManager
    /// must not be used after the underlying TypeOperations instance is destroyed on the C++ side.
    unsafe fn get_type_ops_instance(
        &self,
        interface_id: &str,
        member_name: &str,
    ) -> Option<TypeOperationsManager> {
        let interface_id = StringView::from(interface_id);
        let member_name = StringView::from(member_name);
        // SAFETY: interface_id and member_name are valid ids that correspond to a TypeOperations instance in the C++ registry, as per the caller's contract.
        let ptr = unsafe { mw_com_get_type_ops_instance(interface_id, member_name) };
        NonNull::new(ptr).map(TypeOperationsManager::new)
    }

    /// Wrapper around mw_com_impl_find_service
    ///
    /// # Arguments
    /// * `instance_specifier` - InstanceSpecifier identifying the service instance to find
    ///
    /// # Returns
    /// Result containing a HandleContainer if the service was found, or an error if not found
    fn find_service(&self, instance_specifier: InstanceSpecifier) -> Result<HandleContainer, ()> {
        // SAFETY: instance_specifier.inner is a valid pointer.
        let container = unsafe { mw_com_impl_find_service(instance_specifier.as_native_mut()) };
        if container.is_null() {
            Err(())
        } else {
            Ok(HandleContainer::new(container))
        }
    }

    /// Initialize the Lola Runtime with provided configuration.
    ///
    /// # Arguments
    /// * `manifest_location` - Optional path to the service instance manifest file
    fn initialize(&self, manifest_location: Option<&Path>) {
        // Sanity-check that the Rust and C++ SamplePtr sizes agree.
        let c_size = unsafe { mw_com_impl_sample_ptr_get_size() } as usize;
        assert_eq!(
            c_size,
            std::mem::size_of::<sample_ptr_rs::SamplePtr<u32>>(),
            "SamplePtr size mismatch between Rust and C++"
        );

        let mut options = vec![CString::new("executable").unwrap()];
        if let Some(path) = manifest_location {
            options.push(CString::new("--service_instance_manifest").unwrap());
            options.push(CString::new(path.to_string_lossy().as_ref()).unwrap());
        }
        let mut ptrs: Vec<*const std::ffi::c_char> = options.iter().map(|s| s.as_ptr()).collect();
        // SAFETY: ptrs points to valid null-terminated C strings; len matches the vec length.
        unsafe { mw_com_impl_initialize(ptrs.as_mut_ptr(), ptrs.len() as i32) }
    }
}
