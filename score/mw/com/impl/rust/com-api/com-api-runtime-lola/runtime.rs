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

//! This crate provides a `LoLa` implementation of the COM API for testing purposes.
//! It is meant to be used in conjunction with the `com-api` crate.

#![allow(dead_code)]
//lifetime warning for all the Sample struct impl block . it is required for the Sample struct event lifetime parameter
// and mentaining lifetime of instances and data reference
// As of supressing clippy::needless_lifetimes
//TODO: revist this once com-api is stable - Ticket-234827
#![allow(clippy::needless_lifetimes)]

use core::cmp::Ordering;
use core::fmt::Debug;
use core::future::Future;
use core::marker::PhantomData;
use core::mem::{ManuallyDrop, MaybeUninit};
use core::ops::{Deref, DerefMut};
use std::collections::VecDeque;
use std::path::{Path, PathBuf};
use std::sync::atomic::AtomicUsize;
use std::sync::Arc;

use com_api_concept::{
    Builder, CommData, Consumer, ConsumerBuilder, ConsumerDescriptor, Error, FindServiceSpecifier,
    InstanceSpecifier, Interface, Producer, ProducerBuilder, ProviderInfo, Result, Runtime,
    SampleContainer, ServiceDiscovery, Subscriber, Subscription,
};

use generic_bridge_ffi_rs::*;

pub struct LolaRuntimeImpl {}

#[derive(Clone, Debug)]
pub struct LolaProviderInfo {
    instance_specifier: InstanceSpecifier,
    interface_id: &'static str,
    skeleton_handle: SkeletonInstanceManager,
}

impl ProviderInfo for LolaProviderInfo {
    fn offer_service(&self) -> Result<()> {
        //SAFETY: it is safe as we are passing valid skeleton handle to offer service
        // the skeleton handle is created during building the provider info instance
        let status =
            unsafe { generic_bridge_ffi_rs::skeleton_offer_service(self.skeleton_handle.0.handle) };
        if !status {
            return Err(Error::Fail);
        }
        Ok(())
    }

    fn stop_offer_service(&self) -> Result<()> {
        //SAFETY: it is safe as we are passing valid skeleton handle to stop offer service
        // the skeleton handle is created during building the provider info instance
        unsafe {
            generic_bridge_ffi_rs::skeleton_stop_offer_service(self.skeleton_handle.0.handle)
        };
        Ok(())
    }
}

#[derive(Clone, Debug)]
pub struct LolaConsumerInfo {
    instance_specifier: InstanceSpecifier,
    handle_container: Arc<proxy_bridge_rs::HandleContainer>,
    handle_index: usize,
    interface_id: &'static str,
}

impl LolaConsumerInfo {
    /// Get a reference to the handle, guaranteed valid as long as this struct exists
    pub fn get_handle(&self) -> Option<&HandleType> {
        self.handle_container.get(self.handle_index)
    }
}

impl Runtime for LolaRuntimeImpl {
    type ServiceDiscovery<I: Interface> = SampleConsumerDiscovery<I>;
    type Subscriber<T: CommData> = SubscribableImpl<T>;
    type ProducerBuilder<I: Interface> = SampleProducerBuilder<I>;
    type Publisher<T: CommData> = Publisher<T>;
    type ProviderInfo = LolaProviderInfo;
    type ConsumerInfo = LolaConsumerInfo;

    fn find_service<I: Interface>(
        &self,
        instance_specifier: FindServiceSpecifier,
    ) -> Self::ServiceDiscovery<I> {
        SampleConsumerDiscovery {
            instance_specifier: match instance_specifier {
                FindServiceSpecifier::Any => todo!(), // TODO:[eclipse-score/communication/issues/133]Add error msg or panic like "ANY not supported by Lola"
                FindServiceSpecifier::Specific(spec) => spec,
            },
            _interface: PhantomData,
        }
    }

    fn producer_builder<I: Interface>(
        &self,
        instance_specifier: InstanceSpecifier,
    ) -> Self::ProducerBuilder<I> {
        SampleProducerBuilder::new(self, instance_specifier)
    }
}

//TODO: Ticket-238828 this type should be merge with Sample<T>
//And sample_ptr_rs::SamplePtr<T> FFI function should be move in plumbing folder sample_ptr_rs module
#[derive(Debug)]
struct LolaBinding<T>
where
    T: CommData,
{
    data: ManuallyDrop<sample_ptr_rs::SamplePtr<T>>,
}

impl<T> Drop for LolaBinding<T>
where
    T: CommData,
{
    fn drop(&mut self) {
        //SAFETY: It is safe to call the delete function because data ptr is valid
        //SamplePtr created by FFI
        unsafe {
            let mut sample_ptr = ManuallyDrop::take(&mut self.data);
            generic_bridge_ffi_rs::sample_ptr_delete(
                std::ptr::from_mut(&mut sample_ptr) as *mut std::ffi::c_void,
                T::ID,
            );
        }
    }
}

/// Wrapper that ensures FFI cleanup of allocatee_ptr.
/// SampleAllocateePtr is wrapped in ManuallyDrop to control when the cleanup occurs.
/// Safe to move between SampleMaybeUninit and SampleMut because
/// the Drop impl guarantees cleanup exactly once.
#[derive(Debug)]
struct AllocateePtrWrapper<T>
where
    T: CommData,
{
    inner: ManuallyDrop<sample_allocatee_ptr_rs::SampleAllocateePtr<T>>,
}

impl<T> Drop for AllocateePtrWrapper<T>
where
    T: CommData,
{
    fn drop(&mut self) {
        //SAFETY: It is safe to call the delete function because allocatee_ptr is valid
        //SampleAllocateePtr created by FFI
        unsafe {
            let mut allocatee_ptr = ManuallyDrop::take(&mut self.inner);
            generic_bridge_ffi_rs::delete_allocatee_ptr(
                std::ptr::from_mut(&mut allocatee_ptr) as *mut std::ffi::c_void,
                T::ID,
            );
        }
    }
}

impl<T> AsRef<sample_allocatee_ptr_rs::SampleAllocateePtr<T>> for AllocateePtrWrapper<T>
where
    T: CommData,
{
    fn as_ref(&self) -> &sample_allocatee_ptr_rs::SampleAllocateePtr<T> {
        &self.inner
    }
}

#[derive(Debug)]
pub struct Sample<T>
where
    T: CommData,
{
    //we need unique id for each sample to implement Ord and Eq traits for sorting in SampleContainer
    id: usize,
    inner: LolaBinding<T>,
}

static ID_COUNTER: AtomicUsize = AtomicUsize::new(0);

impl<T> Sample<T>
where
    T: CommData,
{
    pub fn get_data(&self) -> &T {
        //SAFETY: It is safe to get the data pointer because SamplePtr is valid
        //and data is valid as long as SamplePtr is valid
        unsafe {
            let data_ptr = generic_bridge_ffi_rs::sample_ptr_get(
                std::ptr::from_ref(&(*self.inner.data)) as *const std::ffi::c_void,
                T::ID,
            );
            (data_ptr as *const T)
                .as_ref()
                .expect("Data pointer is null")
        }
    }
}

impl<T> Deref for Sample<T>
where
    T: CommData,
{
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.get_data()
    }
}

impl<T> com_api_concept::Sample<T> for Sample<T> where T: CommData {}

// Ordering traits for Sample<T> are using id field to provide total ordering
impl<T> PartialEq for Sample<T>
where
    T: CommData,
{
    fn eq(&self, other: &Self) -> bool {
        self.id == other.id
    }
}

impl<T> Eq for Sample<T> where T: CommData {}

impl<T> PartialOrd for Sample<T>
where
    T: CommData,
{
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl<T> Ord for Sample<T>
where
    T: CommData,
{
    fn cmp(&self, other: &Self) -> Ordering {
        self.id.cmp(&other.id)
    }
}

#[derive(Debug)]
pub struct SampleMut<'a, T>
where
    T: CommData,
{
    skeleton_event: NativeSkeletonEventBase,
    allocatee_ptr: AllocateePtrWrapper<T>,
    lifetime: PhantomData<&'a T>,
}

impl<'a, T> SampleMut<'a, T>
where
    T: CommData,
{
    fn get_allocatee_data_ptr_const(&self) -> Option<&T> {
        //SAFETY: allocatee_ptr is valid which is created using get_allocatee_ptr() and
        // it will be again type casted to T type pointer in cpp side so valid to send as void pointer
        unsafe {
            let data_ptr = generic_bridge_ffi_rs::get_allocatee_data_ptr(
                std::ptr::from_ref(&(*self.allocatee_ptr.inner)) as *const std::ffi::c_void,
                T::ID,
            );
            (data_ptr as *const T).as_ref()
        }
    }

    fn get_allocatee_data_ptr_mut(&mut self) -> Option<&mut T> {
        //SAFETY: allocatee_ptr is valid which is created using get_allocatee_ptr() and
        // it will be again type casted to T type pointer in cpp side so valid to send as void pointer
        unsafe {
            let data_ptr = generic_bridge_ffi_rs::get_allocatee_data_ptr(
                std::ptr::from_mut(&mut (*self.allocatee_ptr.inner)) as *mut std::ffi::c_void,
                T::ID,
            );
            (data_ptr as *mut T).as_mut()
        }
    }
}

impl<'a, T> Deref for SampleMut<'a, T>
where
    T: CommData,
{
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.get_allocatee_data_ptr_const()
            .expect("Allocatee data pointer is null")
    }
}

impl<'a, T> DerefMut for SampleMut<'a, T>
where
    T: CommData,
{
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.get_allocatee_data_ptr_mut()
            .expect("Allocatee data pointer is null")
    }
}

impl<'a, T> com_api_concept::SampleMut<T> for SampleMut<'a, T>
where
    T: CommData,
{
    type Sample = Sample<T>;

    fn into_sample(self) -> Self::Sample {
        todo!()
    }

    fn send(self) -> com_api_concept::Result<()> {
        //SAFETY: It is safe to send the sample because allocatee_ptr and skeleton_event are valid
        // allocatee_ptr is created by FFI and skeleton_event is valid as long as the parent skeleton instance is valid
        // We've taken ownership via self (consumed, not borrowed), and
        // FFI call will complete before drop run on AllocateePtrWrapper and NativeSkeletonEventBase
        let status = unsafe {
            generic_bridge_ffi_rs::skeleton_event_send_sample_allocatee(
                self.skeleton_event.skeleton_event_ptr,
                T::ID,
                std::ptr::from_ref(self.allocatee_ptr.as_ref()) as *const std::ffi::c_void,
            )
        };
        if !status {
            return Err(Error::Fail);
        }
        Ok(())
    }
}

#[derive(Debug)]
pub struct SampleMaybeUninit<'a, T>
where
    T: CommData,
{
    skeleton_event: NativeSkeletonEventBase,
    allocatee_ptr: AllocateePtrWrapper<T>,
    lifetime: PhantomData<&'a T>,
}

impl<'a, T> SampleMaybeUninit<'a, T>
where
    T: CommData,
{
    fn get_allocatee_data_ptr(&mut self) -> Option<&mut core::mem::MaybeUninit<T>> {
        //SAFETY: allocatee_ptr is valid which is created using get_allocatee_ptr() and
        // it will be again type casted to T type pointer in cpp side so valid to send as void pointer
        let data_ptr = unsafe {
            generic_bridge_ffi_rs::get_allocatee_data_ptr(
                std::ptr::from_mut(&mut (*self.allocatee_ptr.inner)) as *mut std::ffi::c_void,
                T::ID,
            ) as *mut core::mem::MaybeUninit<T>
        };
        unsafe { data_ptr.as_mut() }
    }
}

impl<'a, T> com_api_concept::SampleMaybeUninit<T> for SampleMaybeUninit<'a, T>
where
    T: CommData,
{
    type SampleMut = SampleMut<'a, T>;

    fn write(self, val: T) -> SampleMut<'a, T> {
        let mut this = self;
        {
            let data_ptr = this
                .get_allocatee_data_ptr()
                .expect("Allocatee data pointer is null");

            //It is safe to write the value because data_ptr is valid
            // and we are writing the value of type T which is same as allocatee_ptr type
            data_ptr.write(val);
        }

        SampleMut {
            skeleton_event: this.skeleton_event,
            allocatee_ptr: this.allocatee_ptr,
            lifetime: PhantomData,
        }
    }

    unsafe fn assume_init(self) -> SampleMut<'a, T> {
        SampleMut {
            skeleton_event: self.skeleton_event,
            allocatee_ptr: self.allocatee_ptr,
            lifetime: PhantomData,
        }
    }
}

impl<'a, T> AsMut<core::mem::MaybeUninit<T>> for SampleMaybeUninit<'a, T>
where
    T: CommData,
{
    fn as_mut(&mut self) -> &mut core::mem::MaybeUninit<T> {
        self.get_allocatee_data_ptr()
            .expect("Allocatee data pointer is null")
    }
}

/// Manages the lifetime of the native skeleton instance, user should clone this to share between threads
/// Always use this struct to manage the skeleton instance pointer
struct SkeletonInstanceManager(Arc<NativeSkeletonHandle>);

impl Clone for SkeletonInstanceManager {
    fn clone(&self) -> Self {
        Self(Arc::clone(&self.0))
    }
}

impl Debug for SkeletonInstanceManager {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("SkeletonInstanceManager").finish()
    }
}

/// This type contains the native skeleton handle for a specific service instance
/// Manages the lifetime of the SkeletonBase pointer with new and drop implementations
/// It does not provide any mutable access to the underlying skeleton handle
/// And it does not provide any method to access the skeleton handle directly
/// Or to perform any operation on it
/// If any additional method is required to be added, ensure that the safety of the skeleton handle is maintained
/// And the lifetime is managed correctly
/// As it has Send and Sync unsafe impls, it must not expose any mutable access to the skeleton handle
struct NativeSkeletonHandle {
    handle: *mut SkeletonBase,
}

//SAFETY: NativeSkeletonHandle is safe to share between threads because:
// It is created by FFI call and no mutable access is provided to the underlying skeleton handle
// Access is controlled through Arc which provides atomic reference counting
// The skeleton lifetime is managed safely through new and Drop
unsafe impl Sync for NativeSkeletonHandle {}
unsafe impl Send for NativeSkeletonHandle {}

impl NativeSkeletonHandle {
    fn new(interface_id: &str, instance_specifier: &proxy_bridge_rs::InstanceSpecifier) -> Self {
        //SAFETY: It is safe as we are passing valid type id and instance specifier to create skeleton
        let handle = unsafe {
            generic_bridge_ffi_rs::create_skeleton(interface_id, instance_specifier.as_native())
        };
        Self { handle }
    }
}

impl Drop for NativeSkeletonHandle {
    fn drop(&mut self) {
        //SAFETY: It is safe as we are passing valid skeleton handle to destroy skeleton
        // the handle was created using create_skeleton
        unsafe {
            generic_bridge_ffi_rs::destroy_skeleton(self.handle);
        }
    }
}

/// This type conains the skeleton event pointer for a specific event identifier
/// Manages the lifetime of the SkeletonEventBase pointer
/// Drop is not required as the skeleton event lifetime is managed by skeleton instance
struct NativeSkeletonEventBase {
    skeleton_event_ptr: *mut SkeletonEventBase,
}

//SAFETY: NativeSkeletonEventBase is safe to send between threads because:
// It is created by FFI call and there is no state associated with the current thread.
// The skeleton event pointer can be safely moved to another thread without thread-local concerns.
// And the skeleton event lifetime is managed safely through Drop of the parent skeleton instance
unsafe impl Send for NativeSkeletonEventBase {}

impl NativeSkeletonEventBase {
    fn new(instance_info: &LolaProviderInfo, identifier: &str) -> Self {
        //SAFETY: It is safe as we are passing valid skeleton handle and interface id to get event
        // skeleton handle is created during producer offer call
        let skeleton_event_ptr = unsafe {
            generic_bridge_ffi_rs::get_event_from_skeleton(
                instance_info.skeleton_handle.0.handle,
                instance_info.interface_id,
                identifier,
            )
        };
        Self { skeleton_event_ptr }
    }
}

impl Clone for NativeSkeletonEventBase {
    fn clone(&self) -> Self {
        Self {
            skeleton_event_ptr: self.skeleton_event_ptr,
        }
    }
}

impl Debug for NativeSkeletonEventBase {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("NativeSkeletonEventBase").finish()
    }
}

/// Manages the lifetime of the native proxy instance, user should clone this to share between threads
/// Always use this struct to manage the proxy instance pointer
struct ProxyInstanceManager(Arc<NativeProxyBase>);

impl Clone for ProxyInstanceManager {
    fn clone(&self) -> Self {
        Self(Arc::clone(&self.0))
    }
}

impl Debug for ProxyInstanceManager {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("ProxyInstanceManager").finish()
    }
}

/// This type contains the native proxy instance for a specific service instance
/// Manages the lifetime of the ProxyBase pointer with new and drop implementations
/// It does not provide any mutable access to the underlying proxy instance
/// And it does not provide any method to access the proxy instance directly
/// Or to perform any operation on it
/// If any additional method is required to be added, ensure that the safety of the proxy instance is maintained
/// And the lifetime is managed correctly
/// As it has Send and Sync unsafe impls, it must not expose any mutable access to the proxy instance
/// Or must not provide any method to access the proxy instance directly
struct NativeProxyBase {
    proxy: *mut ProxyBase, // Stores the proxy instance
}

//SAFETY: NativeProxyBase is safe to share between threads because:
// It is created by FFI call and no mutable access is provided
// Access is controlled through Arc which provides atomic reference counting
// The proxy lifetime is managed safely through Drop
// and it does not provide any mutable access to the underlying proxy instance
unsafe impl Send for NativeProxyBase {}
unsafe impl Sync for NativeProxyBase {}

impl Drop for NativeProxyBase {
    fn drop(&mut self) {
        //SAFETY: It is safe to destroy the proxy because it was created by FFI
        // and proxy pointer received at the time of create_proxy called
        unsafe {
            generic_bridge_ffi_rs::destroy_proxy(self.proxy);
        }
    }
}

impl Debug for NativeProxyBase {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("NativeProxyBase").finish()
    }
}

impl NativeProxyBase {
    fn new(interface_id: &str, handle: &HandleType) -> Self {
        //SAFETY: It is safe to create the proxy because interface_id and handle are valid
        //Handle received at the time of get_avaible_instances called with correct interface_id
        let proxy = unsafe { generic_bridge_ffi_rs::create_proxy(interface_id, handle) };
        Self { proxy }
    }
}

/// This type contains the native proxy event pointer for a specific event identifier
/// Manages the lifetime of the ProxyEventBase pointer
/// Drop is not required as the proxy event lifetime is managed by proxy instance
/// It does not provide any mutable access to the underlying proxy event pointer
/// And the proxy event lifetime is managed safely through Drop of the parent proxy instance
struct NativeProxyEventBase {
    proxy_event_ptr: *mut ProxyEventBase,
}

//SAFETY: NativeProxyEventBase is to send between threads because:
// It is created by FFI call and there is no state associated with the current thread.
// The skeleton event pointer can be safely moved to another thread without thread-local concerns.
// And the proxy event lifetime is managed safely through Drop of the parent proxy instance
// which ensures the proxy handle remains valid as long as events are in use
unsafe impl Send for NativeProxyEventBase {}

impl NativeProxyEventBase {
    fn new(proxy: *mut ProxyBase, interface_id: &str, identifier: &str) -> Self {
        //SAFETY: It is safe as we are passing valid proxy pointer and interface id to get event
        // proxy pointer is created during consumer creation
        let proxy_event_ptr =
            unsafe { generic_bridge_ffi_rs::get_event_from_proxy(proxy, interface_id, identifier) };
        Self { proxy_event_ptr }
    }
}

impl Debug for NativeProxyEventBase {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("NativeProxyEventBase").finish()
    }
}

#[derive(Debug)]
pub struct SubscribableImpl<T> {
    identifier: &'static str,
    instance_info: LolaConsumerInfo,
    proxy_instance: ProxyInstanceManager,
    data: PhantomData<T>,
}

impl<T: CommData> Subscriber<T, LolaRuntimeImpl> for SubscribableImpl<T> {
    type Subscription = SubscriberImpl<T>;
    fn new(
        identifier: &'static str,
        instance_info: LolaConsumerInfo,
    ) -> com_api_concept::Result<Self> {
        let handle = instance_info.get_handle().ok_or(Error::Fail)?;
        let native_proxy = NativeProxyBase::new(instance_info.interface_id, handle);
        let proxy_instance = ProxyInstanceManager(Arc::new(native_proxy));
        Ok(Self {
            identifier,
            instance_info,
            proxy_instance,
            data: PhantomData,
        })
    }
    fn subscribe(&self, max_num_samples: usize) -> com_api_concept::Result<Self::Subscription> {
        let instance_info = self.instance_info.clone();
        let event_instance = NativeProxyEventBase::new(
            self.proxy_instance.0.proxy,
            self.instance_info.interface_id,
            self.identifier,
        );

        //SAFETY: It is safe to subscribe to event because event_instance is valid
        // which was obtained from valid proxy instance
        let status = unsafe {
            generic_bridge_ffi_rs::subscribe_to_event(
                event_instance.proxy_event_ptr,
                max_num_samples.try_into().unwrap(),
            )
        };
        if !status {
            return Err(Error::Fail);
        }
        // Store in SubscriberImpl with event, max_num_samples
        Ok(SubscriberImpl {
            event: Some(event_instance),
            event_id: self.identifier,
            max_num_samples,
            data: VecDeque::new(),
            instance_info,
            _proxy: self.proxy_instance.clone(),
        })
    }
}

#[derive(Debug)]
pub struct SubscriberImpl<T>
where
    T: CommData,
{
    //SAFETY: This can be used as raw pointer because it comes under proxy instance lifetime
    event: Option<NativeProxyEventBase>,
    event_id: &'static str,
    max_num_samples: usize,
    data: VecDeque<T>,
    instance_info: LolaConsumerInfo,
    _proxy: ProxyInstanceManager,
}

impl<T> SubscriberImpl<T>
where
    T: CommData,
{
    pub fn add_data(&mut self, data: T) {
        self.data.push_front(data);
    }
}

impl<T> Subscription<T, LolaRuntimeImpl> for SubscriberImpl<T>
where
    T: CommData,
{
    type Subscriber = SubscribableImpl<T>;
    type Sample<'a> = Sample<T>;

    fn unsubscribe(self) -> Self::Subscriber {
        SubscribableImpl {
            identifier: self.event_id,
            instance_info: self.instance_info,
            proxy_instance: self._proxy,
            data: PhantomData,
        }
    }

    fn try_receive<'a>(
        &'a self,
        scratch: &'_ mut SampleContainer<Self::Sample<'a>>,
        max_samples: usize,
    ) -> com_api_concept::Result<usize> {
        if max_samples > self.max_num_samples {
            return Err(Error::Fail);
        }
        if let Some(event) = &self.event {
            let mut callback = |raw_sample: *mut sample_ptr_rs::SamplePtr<T>| {
                if !raw_sample.is_null() {
                    //SAFETY: It is safe to read the sample pointer because
                    // raw_sample is valid pointer passed from FFI callback
                    // and raw_pointer is moved from FFI to Rust ownership here
                    let sample_ptr = unsafe { std::ptr::read(raw_sample) };

                    let wrapped_sample = Sample {
                        //Relaxed ordering is sufficient here as we just need a unique id for each sample
                        id: ID_COUNTER.fetch_add(1, std::sync::atomic::Ordering::Relaxed),
                        inner: LolaBinding {
                            data: ManuallyDrop::new(sample_ptr),
                        },
                    };
                    while scratch.sample_count() >= max_samples {
                        scratch.pop_front();
                    }
                    // After pop from SampleContainer to make room, push should always succeed, otherwise we lose the data
                    assert!(
                        scratch.push_back(wrapped_sample).is_ok(),
                        "Failed to push sample after making room in buffer"
                    );
                }
            };

            // Convert closure to FatPtr for C++ callback
            let dyn_callback: &mut dyn FnMut(*mut sample_ptr_rs::SamplePtr<T>) = &mut callback;

            // SAFETY: it is safe to transmute the closure reference to a FatPtr because
            // it has the same representation in memory like FnMut pointer
            let fat_ptr: FatPtr = unsafe { std::mem::transmute(dyn_callback) };

            // SAFETY: this call is safe because event ptr is a valid ProxyEventBase pointer
            // obtained during subscription
            // The lifetime of the callback is managed by Rust, and it will not outlive
            // the scope of this function call.
            let count = unsafe {
                generic_bridge_ffi_rs::get_samples_from_event(
                    event.proxy_event_ptr,
                    T::ID,
                    &fat_ptr,
                    self.max_num_samples as u32,
                )
            };
            if count > self.max_num_samples as u32 {
                return Err(Error::Fail);
            }
            Ok(count as usize)
        } else {
            Err(Error::Fail)
        }
    }

    #[allow(clippy::manual_async_fn)]
    fn receive<'a>(
        &'a self,
        _scratch: &'_ mut SampleContainer<Self::Sample<'a>>,
        _new_samples: usize,
        _max_samples: usize,
    ) -> impl Future<Output = com_api_concept::Result<usize>> + Send {
        async { todo!() }
    }
}

#[derive(Debug)]
pub struct Publisher<T> {
    identifier: String,
    skeleton_event: NativeSkeletonEventBase,
    _data: PhantomData<T>,
    _skeleton_instance: SkeletonInstanceManager,
}

impl<T> com_api_concept::Publisher<T, LolaRuntimeImpl> for Publisher<T>
where
    T: CommData,
{
    type SampleMaybeUninit<'a>
        = SampleMaybeUninit<'a, T>
    where
        Self: 'a;

    fn allocate<'a>(&'a self) -> com_api_concept::Result<Self::SampleMaybeUninit<'a>> {
        //SAFETY: It is safe to get the allocatee ptr because skeleton_event is valid
        // skeleton_event is created during publisher creation and valid as long as publisher is valid
        // T::ID is valid as it is associated with CommData type
        // allocatee_ptr is same type pointer which is allocated for T type and
        // it will be constructed in cpp side and moved back to rust side
        let allocatee_ptr = unsafe {
            let mut sample =
                MaybeUninit::<sample_allocatee_ptr_rs::SampleAllocateePtr<T>>::uninit();
            let status = generic_bridge_ffi_rs::get_allocatee_ptr(
                self.skeleton_event.skeleton_event_ptr,
                sample.as_mut_ptr() as *mut std::ffi::c_void,
                T::ID,
            );
            if !status {
                return Err(Error::Fail);
            }
            sample.assume_init()
        };

        Ok(SampleMaybeUninit {
            skeleton_event: self.skeleton_event.clone(),
            allocatee_ptr: AllocateePtrWrapper {
                inner: ManuallyDrop::new(allocatee_ptr),
            },
            lifetime: PhantomData,
        })
    }

    fn new(identifier: &str, instance_info: LolaProviderInfo) -> com_api_concept::Result<Self> {
        let skeleton_event = NativeSkeletonEventBase::new(&instance_info, identifier);

        Ok(Self {
            identifier: identifier.to_string(),
            skeleton_event,
            _data: PhantomData,
            _skeleton_instance: instance_info.skeleton_handle.clone(),
        })
    }
}

pub struct SampleConsumerDiscovery<I> {
    instance_specifier: InstanceSpecifier,
    _interface: PhantomData<I>,
}

impl<I: Interface> SampleConsumerDiscovery<I> {
    fn new(_runtime: &LolaRuntimeImpl, instance_specifier: InstanceSpecifier) -> Self {
        Self {
            instance_specifier,
            _interface: PhantomData,
        }
    }
}

impl<I: Interface> ServiceDiscovery<I, LolaRuntimeImpl> for SampleConsumerDiscovery<I>
where
    SampleConsumerBuilder<I>: ConsumerBuilder<I, LolaRuntimeImpl>,
{
    type ConsumerBuilder = SampleConsumerBuilder<I>;
    type ServiceEnumerator = Vec<SampleConsumerBuilder<I>>;

    fn get_available_instances(&self) -> com_api_concept::Result<Self::ServiceEnumerator> {
        //If ANY Support is added in Lola, then we need to return all available instances
        let instance_specifier_lola =
            proxy_bridge_rs::InstanceSpecifier::try_from(self.instance_specifier.as_ref())
                .map_err(|_| Error::Fail)?;

        let service_handle =
            proxy_bridge_rs::find_service(instance_specifier_lola).map_err(|_| Error::Fail)?;

        let service_handle_arc = Arc::new(service_handle);
        let available_instances = (0..service_handle_arc.len())
            .map(|handle_index| {
                let instance_info = LolaConsumerInfo {
                    instance_specifier: self.instance_specifier.clone(),
                    handle_container: Arc::clone(&service_handle_arc),
                    handle_index,
                    interface_id: I::INTERFACE_ID,
                };
                SampleConsumerBuilder {
                    instance_info,
                    _interface: PhantomData,
                }
            })
            .collect();
        Ok(available_instances)
    }

    #[allow(clippy::manual_async_fn)]
    fn get_available_instances_async(
        &self,
    ) -> impl Future<Output = com_api_concept::Result<Self::ServiceEnumerator>> + Send {
        async { Ok(Vec::new()) }
    }
}

impl<I: Interface> ConsumerBuilder<I, LolaRuntimeImpl> for SampleConsumerBuilder<I> {}

impl<I: Interface> Builder<I::Consumer<LolaRuntimeImpl>> for SampleConsumerBuilder<I> {
    fn build(self) -> com_api_concept::Result<I::Consumer<LolaRuntimeImpl>> {
        Ok(Consumer::new(self.instance_info))
    }
}

pub struct SampleProducerBuilder<I: Interface> {
    instance_specifier: InstanceSpecifier,
    _interface: PhantomData<I>,
}

impl<I: Interface> SampleProducerBuilder<I> {
    fn new(_runtime: &LolaRuntimeImpl, instance_specifier: InstanceSpecifier) -> Self {
        Self {
            instance_specifier,
            _interface: PhantomData,
        }
    }
}

impl<I: Interface> ProducerBuilder<I, LolaRuntimeImpl> for SampleProducerBuilder<I> {}

impl<I: Interface> Builder<I::Producer<LolaRuntimeImpl>> for SampleProducerBuilder<I> {
    fn build(self) -> Result<I::Producer<LolaRuntimeImpl>> {
        let instance_specifier_runtime =
            proxy_bridge_rs::InstanceSpecifier::try_from(self.instance_specifier.as_ref())
                .map_err(|_| Error::Fail)?;

        let skeleton_handle =
            NativeSkeletonHandle::new(I::INTERFACE_ID, &instance_specifier_runtime);

        let instance_info = LolaProviderInfo {
            instance_specifier: self.instance_specifier,
            interface_id: I::INTERFACE_ID,
            skeleton_handle: SkeletonInstanceManager(Arc::new(skeleton_handle)),
        };

        I::Producer::new(instance_info).map_err(|_| Error::Fail)
    }
}

pub struct SampleConsumerDescriptor<I: Interface> {
    _interface: PhantomData<I>,
}

impl<I: Interface> Clone for SampleConsumerDescriptor<I> {
    fn clone(&self) -> Self {
        Self {
            _interface: PhantomData,
        }
    }
}

pub struct SampleConsumerBuilder<I: Interface> {
    instance_info: LolaConsumerInfo,
    _interface: PhantomData<I>,
}

impl<I: Interface> ConsumerDescriptor<LolaRuntimeImpl> for SampleConsumerBuilder<I> {
    fn get_instance_identifier(&self) -> &InstanceSpecifier {
        //if InstanceSpecifier::ANY support enable by lola
        //then this API should get InstanceSpecifier from FFI Call
        todo!()
    }
}

pub struct RuntimeBuilderImpl {
    config_path: Option<PathBuf>,
}

impl Builder<LolaRuntimeImpl> for RuntimeBuilderImpl {
    fn build(self) -> com_api_concept::Result<LolaRuntimeImpl> {
        proxy_bridge_rs::initialize(self.config_path.as_deref());
        Ok(LolaRuntimeImpl {})
    }
}

impl com_api_concept::RuntimeBuilder<LolaRuntimeImpl> for RuntimeBuilderImpl {
    fn load_config(&mut self, config: &Path) -> &mut Self {
        self.config_path = Some(config.to_path_buf());
        self
    }
}

impl Default for RuntimeBuilderImpl {
    fn default() -> Self {
        Self::new()
    }
}

impl RuntimeBuilderImpl {
    /// Creates a new instance of the default implementation of the com layer
    pub fn new() -> Self {
        Self { config_path: None }
    }
}

#[cfg(test)]
mod test {
    use com_api_concept::{Publisher, SampleContainer, SampleMaybeUninit, SampleMut, Subscription};

    #[test]
    fn receive_stuff() {
        let test_subscriber = super::SubscriberImpl::<u32>::new();
        for _ in 0..10 {
            let mut sample_buf = SampleContainer::new();
            let receive_result = test_subscriber.try_receive(&mut sample_buf, 1);
            match receive_result {
                Ok(0) => panic!("No sample received"),
                Ok(x) => {
                    println!(
                        "{} samples received: sample[0] = {}",
                        x,
                        *sample_buf.front().unwrap()
                    )
                }
                Err(e) => panic!("{:?}", e),
            }
        }
    }

    #[test]
    fn receive_async_stuff() {
        let test_subscriber = super::SubscriberImpl::<u32>::new();
        // block on an asynchronous reception of data from test_subscriber
        futures::executor::block_on(async {
            let mut sample_buf = SampleContainer::new();
            match test_subscriber.receive(&mut sample_buf, 1, 1).await {
                Ok(0) => panic!("No sample received"),
                Ok(x) => {
                    println!(
                        "{} samples received: sample[0] = {}",
                        x,
                        *sample_buf.front().unwrap()
                    )
                }
                Err(e) => panic!("{:?}", e),
            }
        })
    }

    #[test]
    fn send_stuff() {
        let test_publisher = super::Publisher::<u32>::new();
        let sample = test_publisher.allocate().expect("Couldn't allocate sample");
        let sample = sample.write(42);
        sample.send().expect("Send failed for sample");
    }
}
