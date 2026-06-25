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
// - C FFI implementations (mw_com_impl_*) are in proxy_bridge.cpp / registry_bridge_macro.cpp
// - FatPtr provides binary representation of dyn trait objects (data pointer + vtable pointer)
// - sample_ptr_rs is used for SamplePtr size validation in initialize()
//
// StringView:
// - StringView implementation is inspired by C++ std::string_view
// - It allows passing string data across FFI boundaries without requiring null termination
// - Enables efficient zero-copy string passing for interface_id, event_id, and type_name parameters

use core::fmt::{Debug, Formatter};
use core::marker::Unpin;
use std::ptr::NonNull;
use std::ffi::{c_char, CString};
use std::ops::Index;
use std::path::Path;

/// Opaque C++ void* pointer wrapper
pub type CVoidPtr = *const std::ffi::c_void;
pub type CMutVoidPtr = *mut std::ffi::c_void;

/// Fat pointer: binary representation of a Rust `dyn` trait object (vtable + data pointer).
/// Passed across FFI boundaries to represent type-erased Rust closures.
#[repr(C)]
#[derive(Copy, Clone)]
pub struct FatPtr {
    vtable: *const (),
    data: *mut (),
}

// SAFETY: FatPtr is a plain pair of pointers. Whether it is safe to send/share
// across threads depends entirely on the closure it points to — callers take that
// responsibility. We declare Send+Sync here so that types containing FatPtr can
// implement those bounds where their closure types warrant it.
unsafe impl Send for FatPtr {}
unsafe impl Sync for FatPtr {}

/// Opaque C++ type: `score::mw::com::impl::HandleType`.
#[repr(C)]
#[derive(Default)]
pub struct HandleType {
    _dummy: [u8; 0],
}

/// Opaque C++ type: `score::mw::com::ServiceHandleContainer<HandleType>`.
#[repr(C)]
#[derive(Default)]
pub struct NativeHandleContainer {
    _dummy: [u8; 0],
}

/// Opaque C++ type: `score::mw::com::InstanceSpecifier`.
#[repr(C)]
pub struct NativeInstanceSpecifier {
    _dummy: [u8; 0],
}

/// Opaque C++ type: `score::mw::com::impl::ProxyEventBase`.
#[repr(C)]
#[derive(Default)]
pub struct ProxyEventBase {
    _dummy: [u8; 0],
}

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
        type_ops: &TypeOperationsManager,
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

    /// Starts asynchronous service discovery. The `callback` invariant is encoded in
    /// `FindServiceCallable, check its `unsafe` constructor for the full contract.
    /// The returned handle must eventually be passed to `stop_find_service`.
    fn start_find_service(
        &self,
        callback: &FindServiceCallable,
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
    unsafe fn get_type_ops_instance(
        &self,
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

/// A type-safe wrapper around a `FatPtr` that has been verified to represent a valid
/// find-service callback (`FnMut(HandleContainer, NativeFindServiceHandle)`).
///
/// Constructing this type is `unsafe`, once constructed, passing it to
/// `FFIBridge::start_find_service` is safe because the invariant is encoded here.
pub struct FindServiceCallable {
    inner: FatPtr,
}

impl FindServiceCallable {
    /// Wraps `fat_ptr` in a `FindServiceCallable`.
    ///
    /// # Safety
    /// `fat_ptr` must be a valid, whose underlying closure has the signature
    /// `FnMut(HandleContainer, NativeFindServiceHandle)` and must remain valid for
    /// as long as the returned `FindServiceCallable` (and the find-service operation
    /// registered with it) is alive.
    pub unsafe fn new(fat_ptr: FatPtr) -> Self {
        Self { inner: fat_ptr }
    }

    /// Returns a reference to the inner `FatPtr`.
    pub fn as_fat_ptr(&self) -> &FatPtr {
        &self.inner
    }
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
#[derive(Default)]
pub struct TypeOperations {
    dummy: [u8; 0],
}

impl Debug for TypeOperations {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("TypeOperations").finish()
    }
}

/// This struct manages the pointer to TypeOperations instance retrieved from C++ registry.
#[derive(Copy, Clone)]
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
unsafe impl Send for TypeOperationsManager {}
unsafe impl Sync for TypeOperationsManager {}

impl Debug for TypeOperationsManager {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("TypeOperationsManager").finish()
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

unsafe extern "C" {
    fn mw_com_impl_instance_specifier_create(
        value: *const u8,
        len: u32,
    ) -> *mut NativeInstanceSpecifier;
    fn mw_com_impl_instance_specifier_clone(
        instance_specifier: *const NativeInstanceSpecifier,
    ) -> *mut NativeInstanceSpecifier;
    fn mw_com_impl_instance_specifier_delete(instance_specifier: *mut NativeInstanceSpecifier);
    fn mw_com_impl_find_service(
        instance_specifier: *mut NativeInstanceSpecifier,
    ) -> *mut NativeHandleContainer;
    fn mw_com_impl_handle_container_delete(container: *mut NativeHandleContainer);
    fn mw_com_impl_handle_container_get_size(container: *const NativeHandleContainer) -> u32;
    fn mw_com_impl_handle_container_get_handle_at(
        container: *const NativeHandleContainer,
        pos: u32,
    ) -> *const HandleType;
    fn mw_com_impl_initialize(options: *mut *const std::ffi::c_char, len: i32);
    fn mw_com_impl_sample_ptr_get_size() -> u32;
}

/// Human-readable address of a service instance (e.g. `/vehicle/speed`).
/// Create via `InstanceSpecifier::try_from("/my/service")`.
pub struct InstanceSpecifier {
    inner: *mut NativeInstanceSpecifier,
}

impl InstanceSpecifier {
    /// Returns the raw native pointer. Valid as long as `self` is alive.
    pub fn as_native(&self) -> *const NativeInstanceSpecifier {
        self.inner
    }
}

impl TryFrom<&'_ str> for InstanceSpecifier {
    type Error = ();

    fn try_from(value: &'_ str) -> Result<Self, Self::Error> {
        // SAFETY: value points to a valid UTF-8 string; len matches the slice length.
        let inner =
            unsafe { mw_com_impl_instance_specifier_create(value.as_ptr(), value.len() as u32) };
        if inner.is_null() { Err(()) } else { Ok(Self { inner }) }
    }
}

impl Clone for InstanceSpecifier {
    fn clone(&self) -> Self {
        // SAFETY: self.inner was obtained from mw_com_impl_instance_specifier_create.
        let inner = unsafe { mw_com_impl_instance_specifier_clone(self.inner) };
        Self { inner }
    }
}

impl Drop for InstanceSpecifier {
    fn drop(&mut self) {
        // SAFETY: self.inner was obtained from mw_com_impl_instance_specifier_create.
        unsafe { mw_com_impl_instance_specifier_delete(self.inner) }
    }
}

/// Wrapper around a native C++ handle container returned by `find_service`.
pub struct HandleContainer {
    inner: *mut NativeHandleContainer,
}

// SAFETY: The pointer is heap-allocated by C++ and owned exclusively by this struct.
unsafe impl Send for HandleContainer {}
// SAFETY: No interior mutability; all access goes through shared references to immutable data.
unsafe impl Sync for HandleContainer {}

impl HandleContainer {
    /// Wraps a raw pointer returned by C++ FFI. Takes ownership.
    pub fn new(inner: *mut NativeHandleContainer) -> Self {
        Self { inner }
    }

    /// Number of handles in the container.
    #[allow(clippy::len_without_is_empty)]
    pub fn len(&self) -> usize {
        // SAFETY: self.inner is a valid pointer from mw_com_impl_find_service.
        unsafe { mw_com_impl_handle_container_get_size(self.inner) as usize }
    }

    /// Returns `true` if the container has no handles.
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Returns the first handle, or `None` if empty.
    pub fn first(&self) -> Option<&HandleType> {
        if !self.is_empty() { Some(self.index(0)) } else { None }
    }

    /// Returns the handle at `index`, or `None` if out of bounds.
    pub fn get(&self, index: usize) -> Option<&HandleType> {
        if index < self.len() { Some(self.index(index)) } else { None }
    }
}

impl Index<usize> for HandleContainer {
    type Output = HandleType;

    fn index(&self, index: usize) -> &Self::Output {
        assert!(index < self.len(), "HandleContainer index out of bounds");
        // SAFETY: index is within bounds; lifetime of result is tied to &self.
        unsafe {
            mw_com_impl_handle_container_get_handle_at(self.inner, index as u32)
                .as_ref()
                .expect("nullptr received as handle")
        }
    }
}

impl Drop for HandleContainer {
    fn drop(&mut self) {
        // SAFETY: self.inner was obtained from mw_com_impl_find_service.
        unsafe { mw_com_impl_handle_container_delete(self.inner) }
    }
}

impl Debug for HandleContainer {
    fn fmt(&self, f: &mut Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("HandleContainer").finish()
    }
}

/// Find all service instances matching `instance_specifier`.
///
/// Returns `Err(())` when the search fails or yields no container.
#[allow(clippy::result_unit_err)]
pub fn find_service(instance_specifier: InstanceSpecifier) -> Result<HandleContainer, ()> {
    // SAFETY: instance_specifier.inner is a valid pointer.
    let container = unsafe { mw_com_impl_find_service(instance_specifier.inner) };
    if container.is_null() { Err(()) } else { Ok(HandleContainer { inner: container }) }
}

/// Initialize the `mw::com` subsystem.
///
/// Optionally accepts a path to the service-instance manifest. When omitted the
/// default location compiled into the middleware is used.
pub fn initialize(manifest_location: Option<&Path>) {
    // Sanity-check that the Rust and C++ SamplePtr sizes agree.
    let c_size =
        unsafe { mw_com_impl_sample_ptr_get_size() } as usize;
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
    let mut ptrs: Vec<*const std::ffi::c_char> =
        options.iter().map(|s| s.as_ptr()).collect();
    // SAFETY: ptrs points to valid null-terminated C strings; len matches the vec length.
    unsafe { mw_com_impl_initialize(ptrs.as_mut_ptr(), ptrs.len() as i32) }
}