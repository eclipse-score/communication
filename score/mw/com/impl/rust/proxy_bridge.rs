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
///! This module provides boilerplate for a generated bridge between the Rust and C++ code for the
///! proxy side of a service.
///!
///! It contains any code that does not depend on a user-defined type or can be made generic over
///! the user-defined type, e.g. when the type isn't relevant for the enclosing type's layout and
///! no type safety is needed on API level. For any actions that are type-dependent, the action
///! needs to be implemented by external code. This happens by implementing `EventOps` and
///! `ProxyOps` for the user-defined type. The implementation typically will call the respective
///! generated FFI functions to operate on the typed objects like the event itself or the sample
///! pointer. See the documentation of the respective traits for more details.

use std::ops::{Index, Deref};
use std::marker::PhantomData;
use std::ffi::CString;
use std::mem::{ManuallyDrop, drop};
use std::sync::Arc;
use std::path::Path;
use std::task::{Context, Poll};
use std::pin::Pin;
use std::fmt::{self, Debug, Formatter};

use futures::prelude::*;
use futures::task::AtomicWaker;

mod ffi {
    use std::marker::PhantomData;
    use std::mem::transmute;

    /// This type represents bmw::mw::com::InstanceSpecifier as an opaque struct.
    /// Note that this struct is empty as we only use references to it on Rust side.
    #[repr(C)]
    pub struct NativeInstanceSpecifier {
        _dummy: [u8; 0],
    }

    /// This type represents a
    /// ::bmw::mw::com::ServiceHandleContainer<::bmw::mw::com::impl::HandleType> as an opaque
    /// struct. Note that this struct is empty as we only use references to it on Rust side.
    #[repr(C)]
    pub(super) struct NativeHandleContainer {
        _dummy: [u8; 0],
    }

    /// This type represents bmw::mw::com::impl::ProxyWrapperClass as an opaque struct for any
    /// template argument, as the type isn't relevant when dealing with it as an opaque type.
    /// Note that this struct is empty as we only use references to it on Rust side.
    #[repr(C)]
    pub struct ProxyWrapperClass {
        _dummy: [u8; 0],
    }

    /// This type represents bmw::mw::com::impl::ProxyEventBase as an opaque struct.
    /// Note that this struct is empty as we only use references to it on Rust side.
    #[repr(C)]
    pub struct ProxyEventBase {
        _dummy: [u8; 0],
    }

    /// This type represents bmw::mw::com::impl::ProxyEvent as an opaque struct.
    /// Note that this struct is empty as we only use references to it on Rust side.
    #[repr(C)]
    pub struct ProxyEvent<T> {
        pub(crate) base: ProxyEventBase,
        _data: PhantomData<T>,
    }

    /// This type represents bmw::mw::com::impl::HandleType as an opaque struct.
    /// Note that this struct is empty as we only use references to it on Rust side.
    #[repr(C)]
    pub struct HandleType {
        _dummy: [u8; 0],
    }

    /// This struct represents a Rust fat pointer as used by type-erased trait objects. It is used
    /// to pass a reference to a callable that gets called when new data arrives.
    #[repr(C)]
    #[derive(Copy, Clone)]
    pub struct FatPtr {
        vtable: *const (),
        data: *mut (),
    }

    impl Into<*mut (dyn FnMut() + Send + 'static)> for FatPtr {
        fn into(self) -> *mut (dyn FnMut() + Send + 'static) {
            // SAFETY: Since we're transmuting into a pointer and using that pointer is unsafe
            // anyway, the `transmute` is not unsafe since it's a pure relabelling that, by itself,
            // does not introduce undefined behavior.
            unsafe {
                transmute(self)
            }
        }
    }

    impl From<*mut (dyn FnMut() + Send + 'static)> for FatPtr {
        fn from(ptr: *mut (dyn FnMut() + Send + 'static)) -> Self {
            // SAFETY: Since we're transmuting into a pair of pointers and using those pointers is
            // anyway an unsafe operation, the `transmute` is not unsafe since it's a pure
            // relabelling that does not introduce undefined behavior.
            unsafe {
                transmute(ptr)
            }
        }
    }

    extern {
        pub(super) fn mw_com_impl_instance_specifier_create(value: *const u8, len: u32) -> *mut NativeInstanceSpecifier;
        pub(super) fn mw_com_impl_instance_specifier_clone(instance_specifier: *const NativeInstanceSpecifier) -> *mut NativeInstanceSpecifier;
        pub(super) fn mw_com_impl_instance_specifier_delete(instance_specifier: *mut NativeInstanceSpecifier);
        pub(super) fn mw_com_impl_find_service(instance_specifier: *mut NativeInstanceSpecifier) -> *mut NativeHandleContainer;
        pub(super) fn mw_com_impl_handle_container_delete(container: *mut NativeHandleContainer);
        pub(super) fn mw_com_impl_handle_container_get_size(container: *const NativeHandleContainer) -> u32;
        pub(super) fn mw_com_impl_handle_container_get_handle_at(container: *const NativeHandleContainer, pos: u32) -> *const HandleType;
        pub(super) fn mw_com_impl_initialize(options: *const *const i8, len: u32);
        pub(super) fn mw_com_impl_sample_ptr_get_size() -> u32;
        pub(super) fn mw_com_impl_proxy_event_subscribe(proxy_event: *mut ProxyEventBase, max_num_events: u32) -> bool;
        pub(super) fn mw_com_impl_proxy_event_unsubscribe(proxy_event: *mut ProxyEventBase);
        pub(super) fn mw_com_impl_proxy_event_set_receive_handler(proxy_event: *mut ProxyEventBase, boxed_handler: *const FatPtr);
        pub(super) fn mw_com_impl_proxy_event_unset_receive_handler(proxy_event: *mut ProxyEventBase);
    }
}

/// This function is called by the callback infrastructure on C++ side to invoke the registered
/// Rust callback.
///
/// # Safety
///
/// The provided pointer is assumed to point to a callback set from the Rust side. This callback
/// will be an FnMut dynamic fat pointer. If this isn't true (or if the pointer is invalid), this
/// will invoke undefined behavior.
#[no_mangle]
unsafe extern fn mw_com_impl_call_dyn_fnmut(ptr: *mut ffi::FatPtr) {
    let dyn_fnmut: *mut (dyn FnMut() + Send + 'static) = (*ptr).into();
    (*dyn_fnmut)();
}

/// This function is called when the callback that was set from the Rust side is deleted.
///
/// # Safety
///
/// The provided pointer is assumed to point to a callback set from the Rust side. This callback
/// will be an FnMut dynamic fat pointer. If this isn't true (or if the pointer is invalid), this
/// will invoke undefined behavior.
#[no_mangle]
unsafe extern fn mw_com_impl_delete_boxed_fnmut(ptr: *mut ffi::FatPtr) {
    let dyn_fnmut = (*ptr).into();
    drop(Box::from_raw(dyn_fnmut));
}

// Re-export ffi types used in the public API of this module for when they're needed by the
// generated code.
pub use ffi::NativeInstanceSpecifier;

pub use ffi::HandleType;
pub use ffi::ProxyWrapperClass;
pub use ffi::ProxyEvent as NativeProxyEvent;

/// This trait is used to create and delete proxies.
///
/// This is usually done by calling the respective generated code on C++ side.
pub trait ProxyOps {
    /// Create an instance of the proxy using the given handle.
    ///
    /// If the proxy cannot be generated, a null pointer is returned.
    fn create(handle: &HandleType) -> *mut ProxyWrapperClass;

    /// Delete the given proxy instance.
    ///
    /// # Safety
    ///
    /// This method is only safe to use if the given pointer is a pointer returned by a call to
    /// `ProxyOps::create` of the same type and if the pointer hasn't been used for such a call
    /// before (meaning that it is still valid).
    unsafe fn delete(proxy: *mut ProxyWrapperClass);
}

/// This struct manages the lifetime of a C++ proxy that lives on the heap.
///
/// It will use the `ProxyOps` trait to create and delete the proxy. The trait is usually (minus
/// testing) implemented by the generated code on C++ side and just calls the FFI functions.
struct ProxyWrapperGuard<T: ProxyOps> {
    proxy: *mut ProxyWrapperClass,
    _type: std::marker::PhantomData<T>,
}

impl<T: ProxyOps> ProxyWrapperGuard<T> {
    /// Create a proxy using the given handle for the service instance.
    ///
    /// # Errors
    /// If the creation of the proxy fails, this function will return `Err(())`.
    pub fn new(handle: &HandleType) -> Result<Self, ()> {
        let proxy = <T as ProxyOps>::create(handle);
        if !proxy.is_null() {
            Ok(Self { proxy, _type: std::marker::PhantomData })
        } else {
            Err(())
        }
    }
}

impl<T: ProxyOps> Drop for ProxyWrapperGuard<T> {
    fn drop(&mut self) {
        // SAFETY: We're handing over the same pointer that we received from the create function.
        // Since this is the only place where we call delete, the pointer is also still valid.
        unsafe {
            <T as ProxyOps>::delete(self.proxy);
        }
    }
}

/// This struct manages the lifetime of a C++ proxy by dynamic refcounting.
///
/// Since we want to be able to freely extract events from proxies, each event needs a dynamic
/// reference to the proxy so that the proxy gets deleted once all events are gone. This struct
/// will use Arc for the refcouting and delete the pointer as soon as all references are gone.
///
/// This obviously allows a user to move different events to different parts of the program without
/// running into lifetime issues or having to track the proxy lifetime in its types.
///
/// However, it also has a more subtle aspect to it: It would not be easy to develop an ergonomic API
/// in case we wanted to use compile-time lifetime tracking as this would require the events to be
/// borrowed from the proxy type. Since a structure can't borrow from itself, we had to artificially
/// invent two structs, one is the real proxy and one is the event collection that borrows each event
/// from the proxy. So constructing that would require a two-step construction, plus it would require
/// the proxy to be stored somewhere else than the struct that enables access to the events, to not
/// run into the same self-referencing issue again.
pub struct ProxyManager<T: ProxyOps>(Arc<ProxyWrapperGuard<T>>);

impl<T: ProxyOps> Clone for ProxyManager<T> {
    fn clone(&self) -> Self {
        Self(Arc::clone(&self.0))
    }
}

impl<T: ProxyOps> ProxyManager<T> {
    /// Create a C++ proxy on the heap using `ProxyOps::create` and wrap it in an `Arc`.
    ///
    /// # Errors
    /// If the creation of the proxy fails, this function will return `Err(())`.
    pub fn new(handle: &HandleType) -> Result<Self, ()> {
        ProxyWrapperGuard::new(handle).map(|proxy| Self(Arc::new(proxy)))
    }

    /// Returns the pointer to the native proxy object that is managed by this struct. This pointer
    /// is only valid as long as the proxy is alive.
    ///
    /// Please note that this method returns a `*mut ProxyWrapperClass`. The caller must make sure
    /// that no aliasing happens despite that. The method itself is not marked unsafe as
    /// dereferencing the pointer is by itself an unsafe operation.
    pub fn get_native_proxy(&self) -> *mut ProxyWrapperClass {
        self.0.proxy
    }
}

/// This trait is intended to be implemented on all types that are used for event based
/// communication. It will be used in generated code to link actions that are type-dependent to
/// their generated counterparts on C++ side.
pub trait EventOps: Sized {
    /// Retrieve a reference to the data pointed to by the given sample pointer.
    fn get_sample_ref(ptr: &sample_ptr_rs::SamplePtr<Self>) -> &Self;

    /// Delete the given sample pointer.
    fn delete_sample_ptr(ptr: sample_ptr_rs::SamplePtr<Self>);

    /// Poll for new data on a certain event and return a sample pointer that represents the data,
    /// or `None` if there is no data currently.
    ///
    /// # Safety
    ///
    /// This method is only safe to be called when the given pointer is pointing to a valid event of
    /// the type `Self`.
    unsafe fn get_new_sample(proxy_event: *mut NativeProxyEvent<Self>) -> Option<sample_ptr_rs::SamplePtr<Self>>;
}

/// # Safety
///
/// This function must be called with a pointer to a valid event of the type `T`. The event behind
/// the pointer must not have been deleted already.
unsafe fn native_get_new_sample<'a, T: EventOps>(native: *mut ffi::ProxyEvent<T>) -> Option<crate::SamplePtr<'a, T>> {
    let sample_ptr = <T as EventOps>::get_new_sample(native);
    sample_ptr.map(|sample_ptr| {
        crate::SamplePtr { sample_ptr: ManuallyDrop::new(sample_ptr), _event: PhantomData }
    })
}

/// # Safety
///
/// This function must be called with a pointer to a valid event of the type `T`. The event behind
/// the pointer must not have been deleted already.
unsafe fn native_set_receive_handler<T: EventOps, F>(native: *mut NativeProxyEvent<T>, handler: F)
where
    F: FnMut() + Send + 'static,
{
    let boxed_handler = Box::new(handler) as Box<dyn FnMut() + Send + 'static>;
    let ptr = Box::into_raw(boxed_handler);
    let fat_ptr = ffi::FatPtr::from(ptr);
    // SAFETY: Since the pointer is unmodified from what we get from C++ and the FFI function
    // calls SetReceiveHandler on the event with the provided data, there is no undefined
    // behavior.
    unsafe {
        ffi::mw_com_impl_proxy_event_set_receive_handler(&mut (*native).base, &fat_ptr);
    }
}

/// Unset the receive handler, if any is set.
///
/// # Safety
///
/// This function must be called with a pointer to a valid event of the type `T`. The event behind
/// the pointer must not have been deleted already.
unsafe fn native_unset_receive_handler<T>(native: *mut NativeProxyEvent<T>) {
    // SAFETY: Since the pointer is unmodified from what we get from C++ and the FFI function
    // calls UnsetReceiveHandler on the event, there is no undefined behavior expected.
    unsafe {
        ffi::mw_com_impl_proxy_event_unset_receive_handler(&mut (*native).base);
    }
}

/// This struct represents a proxy event that can be used to subscribe to events and receive data.
pub struct ProxyEvent<T, P> {
    native: *mut ffi::ProxyEvent<T>,
    proxy: P,
    event_name: &'static str,
}

// SAFETY: There is no connection of any data to a particular thread, also not on C++ side.
// Therefore, it is safe to send this struct to another thread.
unsafe impl<T, P> Send for ProxyEvent<T, P> {}

impl<T, P> Debug for ProxyEvent<T, P> {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        f.debug_struct("ProxyEvent")
            .field("event_name", &self.event_name)
            .finish()
    }
}

impl<T: EventOps, P> ProxyEvent<T, P> {
    /// Create a new event, representing one event on C++ side and referencing the proxy to
    /// manage its lifetime.
    ///
    /// The provided callable's duty is to provide the event, given the proxy. It will be bound to
    /// the (generated) FFI function that is actually implementing event retrieval on C++ side.
    pub fn new<F>(proxy: P, event_name: &'static str, make_event: F) -> Self
    where
        F: FnOnce(&P) -> *mut ffi::ProxyEvent<T>,
    {
        let native = make_event(&proxy);
        Self {
            native,
            proxy,
            event_name,
        }
    }

    /// Subscribe to the event.
    ///
    /// # Errors
    ///
    /// If the subscription fails on C++ side, this error is propagated to the caller.
    pub fn subscribe(self, max_num_events: u32) -> Result<SubscribedProxyEvent<T, P>, (&'static str, Self)> {
        // SAFETY: Since the pointer is unmodified from what we get from C++ and the FFI function
        // calls Subscribe on the event, there is no undefined behavior expected.
        unsafe {
            if ffi::mw_com_impl_proxy_event_subscribe(&mut (*self.native).base, max_num_events) {
                Ok(SubscribedProxyEvent {
                    native: self.native,
                    proxy: self.proxy,
                    event_name: self.event_name,
                })
            } else {
                Err(("Unable to subscribe", self))
            }
        }
    }
}

#[must_use]
pub struct SubscribedProxyEvent<T, P> {
    native: *mut ffi::ProxyEvent<T>,
    proxy: P,
    event_name: &'static str,
}

// SAFETY: There is no connection of any data to a particular thread, also not on C++ side.
// Therefore, it is safe to send this struct to another thread.
unsafe impl<T, P> Send for SubscribedProxyEvent<T, P> {}

impl<T, P> Debug for SubscribedProxyEvent<T, P> {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        f.debug_struct("SubscribedProxyEvent")
            .field("event_name", &self.event_name)
            .finish()
    }
}

impl<T: EventOps, P: Clone> SubscribedProxyEvent<T, P> {
    /// Unsubscribe from the event.
    ///
    /// # Errors
    ///
    /// If the unsubscription fails on C++ side, this error is propagated to the caller.
    pub fn unsubscribe(self) -> ProxyEvent<T, P> {
        ProxyEvent {
            native: self.native,
            proxy: self.proxy.clone(),
            event_name: self.event_name,
        }
    }

    /// Retrieve a sample from the event.
    ///
    /// If there is no new sample available, this function will return `None`.
    pub fn get_new_sample(&self) -> Option<crate::SamplePtr<T>> {
        // SAFETY: This call is safe since the pointer is unmodified from what we get from C++.
        // Since this is the same pointer that we received during initialization (which is, by its
        // signature) of the same type, we're allowed to use it here safely.
        unsafe {
            native_get_new_sample(self.native)
        }
    }

    /// Set a callable that is called whenever there is new data incoming.
    ///
    /// Please note that the callable's lifetime is bound to the event's lifetime. So, if the event
    /// is kept alive inside the callable. you will end up with a circular dependency which will
    /// lead to a memory leak. Therefore, the circle needs to be broken somehow, either by unsetting
    /// the receive handler or by using a weak reference to the event.
    ///
    /// # Todo
    /// If the underlying C++ implementation makes sure that the callback is not called after the
    /// event got deleted, we might relax the lifetime from 'static to some lifetime that is
    /// limited by the lifetime of the event.
    pub fn set_receive_handler<F>(&mut self, handler: F)
    where
        F: FnMut() + Send + 'static,
    {
        // SAFETY: This call is safe since the pointer is unmodified from what we get from C++.
        // Since this is the same pointer that we received during initialization (which is, by its
        // signature) of the same type, we're allowed to use it here safely.
        unsafe { native_set_receive_handler(self.native, handler) }
    }

    /// Unset the receive handler, if any is set.
    pub fn unset_receive_handler(&mut self) {
        // SAFETY: This call is safe since the pointer is unmodified from what we get from C++.
        // Since this is the same pointer that we received during initialization (which is, by its
        // signature) of the same type, we're allowed to use it here safely.
        unsafe { native_unset_receive_handler(self.native); }
    }

    pub fn as_stream(&mut self) -> Result<ProxyEventStream<T, P>, &'static str> {
        let waker_storage: Arc<AtomicWaker> = Arc::default();

        let callback_waker = Arc::clone(&waker_storage);
        let waker_callback = move || callback_waker.wake();
        // SAFETY: This call is safe since the pointer is unmodified from what we get from C++.
        // Since this is the same pointer that we received during initialization (which is, by its
        // signature) of the same type, we're allowed to use it here safely.
        unsafe { native_set_receive_handler(self.native, waker_callback); }

        Ok(ProxyEventStream {
            event: self,
            waker_storage,
        })
    }
}

impl<T, P> Drop for SubscribedProxyEvent<T, P> {
    fn drop(&mut self) {
        // SAFETY: Since the pointer is unmodified from what we get from C++ and the FFI function
        // calls Unsubscribe on the event, there is no undefined behavior.
        unsafe {
            ffi::mw_com_impl_proxy_event_unsubscribe(&mut (*self.native).base)
        }
    }
}

#[must_use]
pub struct ProxyEventStream<'a, T: EventOps, P> {
    event: &'a mut SubscribedProxyEvent<T, P>,
    waker_storage: Arc<AtomicWaker>,
}

impl<'a, T: EventOps, P> Stream for ProxyEventStream<'a, T, P> {
    type Item = SamplePtr<'a, T>;

    fn poll_next(self: Pin<&mut Self>, ctx: &mut Context<'_>) -> Poll<Option<Self::Item>> {
        self.waker_storage.register(ctx.waker());
        // SAFETY: This call is safe since the pointer is unmodified from what we get from C++.
        // Since this is the same pointer that we received during initialization (which is, by its
        // signature) of the same type, we're allowed to use it here safely.
        let sample_ptr = unsafe { native_get_new_sample(self.event.native) };
        match sample_ptr {
            Some(sample_ptr) => Poll::Ready(Some(sample_ptr)),
            None => {
                Poll::Pending
            }
        }
    }
}

impl<T: EventOps, P> Drop for ProxyEventStream<'_, T, P> {
    fn drop(&mut self) {
        // SAFETY: This call is safe since the pointer is unmodified from what we get from C++.
        // Since this is the same pointer that we received during initialization (which is, by its
        // signature) of the same type, we're allowed to use it here safely.
        unsafe { native_unset_receive_handler(self.event.native) };
    }
}

/// This type represents an instance specifier, which is a human-readable representation of a
/// service instance.
///
/// The main way to create this is by converting from a `str`.
pub struct InstanceSpecifier {
    inner: *mut ffi::NativeInstanceSpecifier,
}

impl InstanceSpecifier {
    pub fn as_native(&self) -> *const NativeInstanceSpecifier {
        self.inner
    }
}

impl TryFrom<&'_ str> for InstanceSpecifier {
    type Error = ();

    fn try_from(value: &'_ str) -> Result<Self, Self::Error> {
        // SAFETY: value points to a string and the length is the length of the string. Since the
        // FFI function respects those boundaries, the call is safe.
        let inner = unsafe {
            ffi::mw_com_impl_instance_specifier_create(value.as_ptr(), value.len() as u32)
        };
        if inner.is_null() {
            Err(())
        } else {
            Ok(Self {
                inner
            })
        }
    }
}

impl Clone for InstanceSpecifier {
    fn clone(&self) -> Self {
        // SAFETY: Since we only pass the pointer received by the create FFI function, the call is
        // safe.
        let inner = unsafe {
            ffi::mw_com_impl_instance_specifier_clone(self.inner)
        };
        Self {
            inner
        }
    }
}

impl Drop for InstanceSpecifier {
    fn drop(&mut self) {
        // SAFETY: Since we only pass the pointer received by the create FFI function, the call is
        // safe.
        unsafe {
            ffi::mw_com_impl_instance_specifier_delete(self.inner);
        }
    }
}

/// Wrapper around a native handle container that contains handles representing serivce instances.
///
/// This struct is created by the `find_service` function and is used to access the resulting
/// handles, if any.
///
/// # Todo
///
/// If needed, this struct could implement `Iterator`. However, since we only use the first handle
/// in the current sample code, we do not need that yet.
pub struct HandleContainer {
    inner: *mut ffi::NativeHandleContainer,
}

impl HandleContainer {
    /// Provides the number of handles in the container.
    pub fn len(&self) -> usize {
        // SAFETY: Since we only pass the pointer received by the create FFI function, the call is
        // safe.
        unsafe {
            ffi::mw_com_impl_handle_container_get_size(self.inner) as usize
        }
    }

    /// Returns the first handle in the container, or 'None' if the container is empty.
    pub fn first(&self) -> Option<&HandleType> {
        if self.len() > 0 {
            Some(self.index(0))
        } else {
            None
        }
    }
}

impl Index<usize> for HandleContainer {
    type Output = HandleType;

    fn index(&self, index: usize) -> &Self::Output {
        if self.len() <= index {
            panic!("index out of bounds");
        }
        // SAFETY: Since we only pass the pointer received by the create FFI function, the call is
        // safe. The call to as_ref is safe, as we know that the pointer is not dangling as long
        // as the container is still alive. Since the lifetime of the result is bound to the
        // lifetime of the borrow to self, this is guaranteed.
        unsafe {
            let native_handle = ffi::mw_com_impl_handle_container_get_handle_at(self.inner, index as u32);
            native_handle.as_ref().expect("nullptr received as handle")
        }
    }
}

impl Drop for HandleContainer {
    fn drop(&mut self) {
        // SAFETY: Since we only pass the pointer received by the create FFI function, the call is
        // safe.
        unsafe {
            ffi::mw_com_impl_handle_container_delete(self.inner);
        }
    }
}

/// Find a service instance by its instance specifier.
///
/// Returns a list of found instances, which can be empty in case there aren't any. If an error
/// occurred during the search or no container is returned, this function will return `Err(())`.
pub fn find_service(instance_specifier: InstanceSpecifier) -> Result<HandleContainer, ()> {
    // SAFETY: Since we only pass the pointer received by the create FFI function, the call is
    // safe.
    let native_hande_container = unsafe {
        ffi::mw_com_impl_find_service(instance_specifier.inner)
    };
    if !native_hande_container.is_null() {
        Ok(HandleContainer {
            inner: native_hande_container
        })
    } else {
        Err(())
    }
}

/// This struct represents a sample pointer, which is a pointer to data received on a subscribed
/// event.
#[repr(transparent)]
pub struct SamplePtr<'a, T>
where
    T: EventOps,
{
    sample_ptr: ManuallyDrop<sample_ptr_rs::SamplePtr<T>>,
    _event: PhantomData<&'a NativeProxyEvent<T>>,
}

impl<'a, T: EventOps> SamplePtr<'a, T> {
    /// Retrieve a reference to the data pointed to by the sample pointer.
    pub fn get_ref(&self) -> &T {
        <T as EventOps>::get_sample_ref(&self.sample_ptr)
    }
}

impl<'a, T> Drop for SamplePtr<'a, T>
where
    T: EventOps,
{
    fn drop(&mut self) {
        // SAFETY: Since we call `ManuallyDrop::take` in `drop` and we only use this method once
        // here, it is clear that it isn't used again. Therefore, the safety requirement of
        // the method is fulfilled and there is no undefined behavior.
        unsafe {
            <T as EventOps>::delete_sample_ptr(ManuallyDrop::take(&mut self.sample_ptr));
        }
    }
}

impl<'a, T> Deref for SamplePtr<'a, T>
where
    T: EventOps,
{
    type Target = T;
    fn deref(&self) -> &T {
        self.get_ref()
    }
}

/// This function returns the size of a SamplePtr, in bytes. Since the size of a Lola SamplePtr
/// doesn't depend on the type, an arbitrary instance is chosen to determine the size. See the
/// C++ implementation for the concrete type used.
fn get_sample_ptr_size() -> usize {
    // SAFETY: The function jut calls sizeof on a type and is therefore constexpr. No undefined
    // behavior exists.
    unsafe {
        ffi::mw_com_impl_sample_ptr_get_size() as usize
    }
}

/// Initialize the mw::com subsystem.
///
/// This call is optional. However, it will allow for modifying the configuration file location,
/// plus it will do sanity checks, e.g. if the size of the sample pointer is correct.
pub fn initialize(manifest_location: Option<&Path>) {
    assert_eq!(get_sample_ptr_size(), std::mem::size_of::<sample_ptr_rs::SamplePtr<u32>>());

    let mut options = vec![CString::new(b"executable").unwrap()];
    if let Some(manifest_location) = manifest_location {
        options.push(CString::new(b"-service_instance_manifest").unwrap());
        options.push(CString::new(manifest_location.to_string_lossy().as_ref()).unwrap());
    }

    let options_ptr = options.iter().map(|s| s.as_ptr()).collect::<Vec<_>>();
    unsafe {
        ffi::mw_com_impl_initialize(options_ptr.as_ptr(), options_ptr.len() as u32);
    }
}
