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

// Why dynamic FFI bridge instead of static macros which is already available in macros.rs -->
//
// DESIGN DECISION:  FFI Bridge for Compile-time decoupling of COM-API and Lola
//
// Problem with macros.rs approach:
// - macros.rs file provides static, compile-time bindings tightly coupled with specific runtime
//   implementations
// - This violates COM-API design principle of being independent of any specific runtime
// - We cannot invoke macros defined in runtime-specific crates (like Lola, Mock) from COM-API
//   library which is runtime-agnostic
// - macros.rs mixes two concerns: API abstraction (what to communicate)
//   and runtime binding (how to communicate)
//
// Why this file exists:
// - This file provides FFI bindings and bridge functions for COM API that work with Lola runtime
// - COM-API library is independent of any specific runtime implementation
// - bridge_ffi_rs.rs enables Rust COM-API to work with Lola's C++ implementation via FFI without
//   compile-time coupling
// - It uses type erasure and dynamic type resolution on C++ side to decouple compile-time
//   dependencies while still enabling type-safe communication
//
// Key design elements:
// - It defines opaque structs for Proxy and Skeleton base types (ProxyBase, SkeletonBase,
//   SkeletonEventBase)
// - Opaque types hide C++ implementation details and prevent accidental field access from Rust
// - It defines FFI functions for creating, destroying, and interacting with proxies and skeletons
// - String-based type identifiers (interface_id, event_id, event_type) enable runtime type checking
// - It provides Rust closure invocation function (mw_com_impl_call_dyn_ref_fnmut_sample) for
//   callbacks from C++
// - This allows C++ code to call back into Rust closures in type-erased manner using
//   FatPtr trait objects
// - Overall, this file serves as the FFI bridge layer for COM-API functionality
//
// How it works with C++:
// - extern "C" functions defined here are called via Lola APIs implemented in C++
// - FFI for C++ functions are implemented in registry_bridge_macro.cpp, which uses the type
//   registry to resolve operations at runtime
// - FFI C++ implementations are in registry_bridge_macro.cpp file
// - C++ side maintains type registry built by macros defined in registry_bridge_macro.h file
// - Registry is populated at application build time via-
//   auto-generated code for each interface and type
// - When Rust calls FFI functions with type name strings, C++ resolves type at runtime via registry
// - This enables type-safe communication without C++ needing to know Rust types at compile time
//
// Dependencies:
// - It relies on proxy_bridge_rs crate for FatPtr and ProxyWrapperClass definitions
// - FatPtr provides binary representation of dyn trait objects (data pointer + vtable pointer)
// - ProxyWrapperClass and related types provide necessary abstractions for
//   proxy and skeleton handling
//
// StringView:
// - StringView implementation is inspired by C++ std::string_view
// - It allows passing string data across FFI boundaries without requiring null termination
// - Enables efficient zero-copy string passing for interface_id, event_id, and type_name parameters

use core::fmt::{Debug, Formatter};
use core::marker::Unpin;
use std::ffi::c_char;
use std::ptr::NonNull;

/// Opaque C++ void* pointer wrapper
pub type CVoidPtr = *const std::ffi::c_void;
pub type CMutVoidPtr = *mut std::ffi::c_void;

pub use mw_com::proxy::FatPtr;
pub use mw_com::proxy::HandleContainer;
pub use mw_com::proxy::HandleType;
pub use mw_com::proxy::NativeHandleContainer;
pub use mw_com::proxy::NativeInstanceSpecifier;
pub use mw_com::proxy::ProxyEventBase;
pub use mw_com::proxy::ProxyWrapperClass;
pub use mw_com::InstanceSpecifier;

/// FFIBridge trait defines the interface for FFI interactions between Rust COM-API and C++ Lola runtime.
/// This trait abstracts the FFI calls and allows for different implementations
/// e.g., Lola bridge, mock bridge for testing.
/// All this trait bound required because runtime types which use this bridge has these bounds,
/// an because of that this bridge also need to be bound by these traits to be used in runtime implementation.
// In bridge_ffi_rs crate (where FFIBridge trait is defined):
pub trait FFIBridge: Send + Sync + Clone + Debug + 'static + Unpin + Default {
    /// # Safety
    /// `event_ptr` must be a valid, non-null pointer to a `SkeletonEventBase` obtained from
    /// `get_event_from_skeleton`. `allocatee_ptr` must point to valid memory large enough to
    /// hold the allocatee instance for `event_type`.
    unsafe fn get_allocatee_ptr(
        &self,
        event_ptr: *mut SkeletonEventBase,
        allocatee_ptr: *mut std::ffi::c_void,
        type_ops: &TypeOperationsManager
    ) -> bool;

    /// # Safety
    /// `allocatee_ptr` must be a valid pointer previously returned by `get_allocatee_ptr`
    /// and `type_ops` must be the corresponding `TypeOperationsManager` for the type T.
    unsafe fn delete_allocatee_ptr(
        &self,
        allocatee_ptr: *mut std::ffi::c_void,
        type_ops: &TypeOperationsManager,
    );

    /// # Safety
    /// `allocatee_ptr` must be a valid pointer previously returned by `get_allocatee_ptr`
    /// and `type_ops` must be the corresponding `TypeOperationsManager` for the type T.
    unsafe fn get_allocatee_data_ptr(
        &self,
        allocatee_ptr: *const std::ffi::c_void,
        type_ops: &TypeOperationsManager,
    ) -> *mut std::ffi::c_void;

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
    ) -> bool;

    /// # Safety
    /// `sample_ptr` must be a valid pointer to a `SamplePtr<T>` of the specified `type_name`.
    unsafe fn sample_ptr_get(
        &self,
        sample_ptr: *const std::ffi::c_void,
        type_ops: &TypeOperationsManager,
    ) -> *const std::ffi::c_void;

    /// # Safety
    /// `sample_ptr` must be a valid pointer to a `SamplePtr<T>` of the specified `type_name`,
    /// and must not be used after this call.
    unsafe fn sample_ptr_delete(
        &self,
        sample_ptr: *mut std::ffi::c_void,
        type_ops: &TypeOperationsManager,
    );

    /// # Safety
    /// `skeleton_ptr` must be a valid, non-null pointer to a `SkeletonBase` previously created
    /// with `create_skeleton` and not yet destroyed.
    unsafe fn skeleton_offer_service(&self, skeleton_ptr: *mut SkeletonBase) -> bool;

    /// # Safety
    /// `skeleton_ptr` must be a valid, non-null pointer to a `SkeletonBase` previously created
    /// with `create_skeleton` and not yet destroyed.
    unsafe fn skeleton_stop_offer_service(&self, skeleton_ptr: *mut SkeletonBase);

    /// # Safety
    /// `handle_ptr` must be a valid reference to a `HandleType`. The returned pointer must
    /// eventually be destroyed via `destroy_proxy`.
    unsafe fn create_proxy(&self, interface_id: &str, handle_ptr: &HandleType) -> *mut ProxyBase;

    /// # Safety
    /// `instance_spec` must be a valid pointer to a `NativeInstanceSpecifier`. The returned
    /// pointer must eventually be destroyed via `destroy_skeleton`.
    unsafe fn create_skeleton(
        &self,
        interface_id: &str,
        instance_spec: *const NativeInstanceSpecifier,
    ) -> *mut SkeletonBase;

    /// # Safety
    /// `proxy_ptr` must be a valid pointer returned from `create_proxy` that has not been
    /// destroyed yet. No other references to this proxy may be used after this call.
    unsafe fn destroy_proxy(&self, proxy_ptr: *mut ProxyBase);

    /// # Safety
    /// `skeleton_ptr` must be a valid pointer returned from `create_skeleton` that has not
    /// been destroyed yet. No other references to this skeleton may be used after this call.
    unsafe fn destroy_skeleton(&self, skeleton_ptr: *mut SkeletonBase);

    /// # Safety
    /// `proxy_ptr` must be a valid pointer to a `ProxyBase` previously created with
    /// `create_proxy`. The returned pointer remains valid only as long as the proxy is alive.
    unsafe fn get_event_from_proxy(
        &self,
        proxy_ptr: *mut ProxyBase,
        interface_id: &str,
        event_id: &str,
    ) -> *mut ProxyEventBase;

    /// # Safety
    /// `skeleton_ptr` must be a valid pointer to a `SkeletonBase` previously created with
    /// `create_skeleton`. The returned pointer remains valid only as long as the skeleton is alive.
    unsafe fn get_event_from_skeleton(
        &self,
        skeleton_ptr: *mut SkeletonBase,
        interface_id: &str,
        event_id: &str,
    ) -> *mut SkeletonEventBase;

    /// # Safety
    /// `event_ptr` must be a valid pointer to a `ProxyEventBase` obtained from
    /// `get_event_from_proxy`, and the event must have been subscribed via `subscribe_to_event`.
    /// `callback` must be a valid `FatPtr` referencing a callable compatible with `event_type`.
    unsafe fn get_samples_from_event(
        &self,
        event_ptr: *mut ProxyEventBase,
        type_ops: &TypeOperationsManager,
        callback: &FatPtr,
        max_samples: u32,
    ) -> u32;

    /// # Safety
    /// `event_ptr` must be a valid pointer to a `SkeletonEventBase` obtained from
    /// `get_event_from_skeleton`. `data_ptr` must point to valid data whose type matches
    /// `event_type` and must remain valid for the duration of this call.
    unsafe fn skeleton_send_event(
        &self,
        event_ptr: *mut SkeletonEventBase,
        type_ops: &TypeOperationsManager,
        data_ptr: *const std::ffi::c_void,
    ) -> bool;

    /// # Safety
    /// `event_ptr` must be a valid pointer to a `ProxyEventBase` obtained from
    /// `get_event_from_proxy`. Must be called before `get_samples_from_event`.
    unsafe fn subscribe_to_event(
        &self,
        event_ptr: *mut ProxyEventBase,
        max_sample_count: u32,
    ) -> bool;

    /// # Safety
    /// `event_ptr` must be a valid pointer to a `ProxyEventBase` obtained from
    /// `get_event_from_proxy`. Must be called only when no `SamplePtr` for this event is
    /// still held by the caller.
    unsafe fn unsubscribe_to_event(&self, event_ptr: *mut ProxyEventBase);

    /// # Safety
    /// `proxy_event_ptr` must be a valid pointer to a `ProxyEventBase` obtained from
    /// `get_event_from_proxy`. `handler` must be a valid `FatPtr` referencing a callable
    /// compatible with the receive-handler signature expected by the implementation.
    unsafe fn set_event_receive_handler(
        &self,
        proxy_event_ptr: *mut ProxyEventBase,
        handler: &FatPtr,
    ) -> bool;

    /// # Safety
    /// `proxy_event_ptr` must be a valid pointer to a `ProxyEventBase` obtained from
    /// `get_event_from_proxy`. `event_type` must match the type used when the handler was set.
    unsafe fn clear_event_receive_handler(&self, proxy_event_ptr: *mut ProxyEventBase);

    /// # Safety
    /// `callback` must be a valid `FatPtr` referencing a callable compatible with the
    /// find-service callback signature. `instance_spec` must be a valid `InstanceSpecifier`.
    /// The returned handle must eventually be passed to `stop_find_service`.
    unsafe fn start_find_service(
        &self,
        callback: &FatPtr,
        instance_spec: InstanceSpecifier,
    ) -> *mut FindServiceHandle;

    /// # Safety
    /// `handle` must be a valid pointer returned from `start_find_service` that has not
    /// been stopped yet. The handle must not be used after this call.
    unsafe fn stop_find_service(&self, handle: *mut FindServiceHandle);

    /// # Safety
    /// Caller must ensure that the provided interface_id and member_name correspond to
    /// a valid TypeOperations instance in the C++ registry. The returned TypeOperationsManager
    /// must not be used after the underlying TypeOperations instance is destroyed on the C++ side.
    unsafe fn get_type_ops_instance(&self,
        interface_id: &str,
        member_name: &str,
    ) -> Option<TypeOperationsManager>;
}

/// Opaque proxy base struct
#[repr(C)]
#[derive(Default)]
pub struct ProxyBase {
    dummy: [u8; 0],
}

/// Opaque skeleton base struct
#[repr(C)]
#[derive(Default)]
pub struct SkeletonBase {
    dummy: [u8; 0],
}

/// Handle type for find service operations
#[repr(C)]
pub struct FindServiceHandle {
    dummy: [u8; 0],
}

/// This struct wraps the raw pointer to FindServiceHandle returned
/// by C++ and provides safe access to it
pub struct NativeFindServiceHandle {
    handle: *mut FindServiceHandle,
}

impl NativeFindServiceHandle {
    /// Create a new NativeFindServiceHandle from a raw pointer to FindServiceHandle
    pub fn new(handle: *mut FindServiceHandle) -> Self {
        Self { handle }
    }
}

impl AsMut<FindServiceHandle> for NativeFindServiceHandle {
    fn as_mut(&mut self) -> &mut FindServiceHandle {
        // SAFETY: self.handle is guaranteed to be a valid pointer to FindServiceHandle as per
        // the contract of NativeFindServiceHandle.
        unsafe { &mut *self.handle }
    }
}

//SAFETY: NativeFindServiceHandle is safe to send between threads because
// It is created by FFI call and there is no state associated with the current thread.
// The pointer can be safely moved to another thread without thread-local concerns.
// And the lifetime is managed safely through ServiceDiscoveryFuture
// which ensures the handle is not used after discovery completes and
// the handle is cleaned up properly.
unsafe impl Send for NativeFindServiceHandle {}

/// Opaque skeleton event base struct
#[repr(C)]
#[derive(Default)]
pub struct SkeletonEventBase {
    dummy: [u8; 0],
}

impl Debug for SkeletonEventBase {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("SkeletonEventBase").finish()
    }
}

/// Opaque type operations struct
#[repr(C)]
pub struct TypeOperations {
    dummy: [u8; 0],
}

impl Debug for TypeOperations {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("TypeOperations").finish()
    }
}

/// This struct manages the pointer to TypeOperations instance retrieved from C++ registry.
pub struct TypeOperationsManager {
    inner: NonNull<TypeOperations>,
}

impl TypeOperationsManager {
    pub fn new(inner: NonNull<TypeOperations>) -> Self {
        Self { inner }
    }
    pub fn as_ptr(&self) -> *const TypeOperations {
        self.inner.as_ptr()
    }
}

// SAFETY: TypeOperationsManager can be sent and shared between threads because it does not provide
// any interior mutability and the instance is statically allocated and managed on the C++ side.
// Sharing the pointer across threads is safe as this is used with event instance of proxy or skeleton instance
// which already handles the concurrent scenario.
// Note: Sync is required by consumer SubscriberImpl type.
unsafe impl Send for TypeOperationsManager {}
unsafe impl Sync for TypeOperationsManager {}

impl Debug for TypeOperationsManager {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("TypeOperationsManager").finish()
    }
}

impl Clone for TypeOperationsManager {
    fn clone(&self) -> Self {
        TypeOperationsManager { inner: self.inner }
    }
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
