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

//! This file implements the consumer side of the COM API for LoLa runtime.
//! It provides the necessary structures and traits to create consumers
//! that can subscribe to events and receive data samples.
//! It uses FFI to interact with the underlying C++ implementation.
//! Main components include `LolaConsumerInfo`, `SubscribableImpl`,
//! `SubscriberImpl`, `SampleConsumerDiscovery`, and `SampleConsumerBuilder`.
//! These components work together to enable consumers to discover services,
//! subscribe to events, and receive data samples in a type-safe manner.
//! The implementation ensures proper memory management and resource handling
//! through Rust's ownership model and FFI safety practices.

//lifetime warning for all the Sample struct impl block, it is required for the Sample struct event lifetime parameter
// and mentaining lifetime of instances and data reference
// As of supressing clippy::needless_lifetimes
//TODO: revist this once com-api is stable - Ticket-234827
#![allow(clippy::needless_lifetimes)]

use core::cmp::Ordering;
use core::future::Future;
use core::marker::PhantomData;
use core::mem::ManuallyDrop;
use core::ops::Deref;
use std::collections::VecDeque;
use std::sync::atomic::AtomicUsize;
use std::sync::Arc;

use com_api_concept::{
    Builder, CommData, Consumer, ConsumerBuilder, ConsumerDescriptor, Error, InstanceSpecifier,
    Interface, Result, SampleContainer, ServiceDiscovery, Subscriber, Subscription,
};

use bridge_ffi_rs::*;

use crate::LolaRuntimeImpl;

#[derive(Clone, Debug)]
pub struct LolaConsumerInfo {
    pub instance_specifier: InstanceSpecifier,
    pub handle_container: Arc<mw_com::proxy::HandleContainer>,
    pub handle_index: usize,
    pub interface_id: &'static str,
}

impl LolaConsumerInfo {
    /// Get a reference to the handle, guaranteed valid as long as this struct exists
    pub fn get_handle(&self) -> Option<&HandleType> {
        self.handle_container.get(self.handle_index)
    }
}

//TODO: Ticket-238828 this type should be merge with Sample<T>
//And sample_ptr_rs::SamplePtr<T> FFI function should be move in plumbing folder sample_ptr_rs module
#[derive(Debug)]
pub struct LolaBinding<T>
where
    T: CommData,
{
    pub data: ManuallyDrop<sample_ptr_rs::SamplePtr<T>>,
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
            bridge_ffi_rs::sample_ptr_delete(
                std::ptr::from_mut(&mut sample_ptr) as *mut std::ffi::c_void,
                T::ID,
            );
        }
    }
}

#[derive(Debug)]
pub struct Sample<T>
where
    T: CommData,
{
    //we need unique id for each sample to implement Ord and Eq traits for sorting in SampleContainer
    pub id: usize,
    pub inner: LolaBinding<T>,
}

pub static ID_COUNTER: AtomicUsize = AtomicUsize::new(0);

impl<T> Sample<T>
where
    T: CommData,
{
    pub fn get_data(&self) -> &T {
        //SAFETY: It is safe to get the data pointer because SamplePtr is valid
        //and data is valid as long as SamplePtr is valid
        unsafe {
            let data_ptr = bridge_ffi_rs::sample_ptr_get(
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

/// Manages the lifetime of the native proxy instance, user should clone this to share between threads
/// Always use this struct to manage the proxy instance pointer
pub struct ProxyInstanceManager(pub Arc<NativeProxyBase>);

impl Clone for ProxyInstanceManager {
    fn clone(&self) -> Self {
        Self(Arc::clone(&self.0))
    }
}

impl std::fmt::Debug for ProxyInstanceManager {
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
pub struct NativeProxyBase {
    pub proxy: *mut ProxyBase, // Stores the proxy instance
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
            bridge_ffi_rs::destroy_proxy(self.proxy);
        }
    }
}

impl std::fmt::Debug for NativeProxyBase {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("NativeProxyBase").finish()
    }
}

impl NativeProxyBase {
    pub fn new(interface_id: &str, handle: &HandleType) -> Self {
        //SAFETY: It is safe to create the proxy because interface_id and handle are valid
        //Handle received at the time of get_avaible_instances called with correct interface_id
        let proxy = unsafe { bridge_ffi_rs::create_proxy(interface_id, handle) };
        Self { proxy }
    }
}

/// This type contains the native proxy event pointer for a specific event identifier
/// Manages the lifetime of the ProxyEventBase pointer
/// Drop is not required as the proxy event lifetime is managed by proxy instance
/// It does not provide any mutable access to the underlying proxy event pointer
/// And the proxy event lifetime is managed safely through Drop of the parent proxy instance
pub struct NativeProxyEventBase {
    pub proxy_event_ptr: *mut ProxyEventBase,
}

//SAFETY: NativeProxyEventBase is to send between threads because:
// It is created by FFI call and there is no state associated with the current thread.
// The skeleton event pointer can be safely moved to another thread without thread-local concerns.
// And the proxy event lifetime is managed safely through Drop of the parent proxy instance
// which ensures the proxy handle remains valid as long as events are in use
unsafe impl Send for NativeProxyEventBase {}

impl NativeProxyEventBase {
    pub fn new(proxy: *mut ProxyBase, interface_id: &str, identifier: &str) -> Self {
        //SAFETY: It is safe as we are passing valid proxy pointer and interface id to get event
        // proxy pointer is created during consumer creation
        let proxy_event_ptr =
            unsafe { bridge_ffi_rs::get_event_from_proxy(proxy, interface_id, identifier) };
        Self { proxy_event_ptr }
    }
}

impl std::fmt::Debug for NativeProxyEventBase {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("NativeProxyEventBase").finish()
    }
}

#[derive(Debug)]
pub struct SubscribableImpl<T> {
    pub identifier: &'static str,
    pub instance_info: LolaConsumerInfo,
    pub proxy_instance: ProxyInstanceManager,
    pub data: PhantomData<T>,
}

impl<T: CommData> Subscriber<T, LolaRuntimeImpl> for SubscribableImpl<T> {
    type Subscription = SubscriberImpl<T>;
    fn new(identifier: &'static str, instance_info: LolaConsumerInfo) -> Result<Self> {
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
    fn subscribe(&self, max_num_samples: usize) -> Result<Self::Subscription> {
        let instance_info = self.instance_info.clone();
        let event_instance = NativeProxyEventBase::new(
            self.proxy_instance.0.proxy,
            self.instance_info.interface_id,
            self.identifier,
        );

        //SAFETY: It is safe to subscribe to event because event_instance is valid
        // which was obtained from valid proxy instance
        let status = unsafe {
            bridge_ffi_rs::subscribe_to_event(
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
    pub event: Option<NativeProxyEventBase>,
    pub event_id: &'static str,
    pub max_num_samples: usize,
    pub data: VecDeque<T>,
    pub instance_info: LolaConsumerInfo,
    pub _proxy: ProxyInstanceManager,
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
    ) -> Result<usize> {
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
                bridge_ffi_rs::get_samples_from_event(
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
    ) -> impl Future<Output = Result<usize>> + Send {
        async { todo!() }
    }
}

pub struct SampleConsumerDiscovery<I> {
    pub instance_specifier: InstanceSpecifier,
    pub _interface: PhantomData<I>,
}

impl<I: Interface> SampleConsumerDiscovery<I> {
    pub fn new(_runtime: &LolaRuntimeImpl, instance_specifier: InstanceSpecifier) -> Self {
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

    fn get_available_instances(&self) -> Result<Self::ServiceEnumerator> {
        //If ANY Support is added in Lola, then we need to return all available instances
        let instance_specifier_lola =
            mw_com::InstanceSpecifier::try_from(self.instance_specifier.as_ref())
                .map_err(|_| Error::Fail)?;

        let service_handle =
            mw_com::proxy::find_service(instance_specifier_lola).map_err(|_| Error::Fail)?;

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
    ) -> impl Future<Output = Result<Self::ServiceEnumerator>> + Send {
        async { Ok(Vec::new()) }
    }
}

impl<I: Interface> ConsumerBuilder<I, LolaRuntimeImpl> for SampleConsumerBuilder<I> {}

impl<I: Interface> Builder<I::Consumer<LolaRuntimeImpl>> for SampleConsumerBuilder<I> {
    fn build(self) -> Result<I::Consumer<LolaRuntimeImpl>> {
        Ok(Consumer::new(self.instance_info))
    }
}

pub struct SampleConsumerBuilder<I: Interface> {
    pub instance_info: LolaConsumerInfo,
    pub _interface: PhantomData<I>,
}

impl<I: Interface> ConsumerDescriptor<LolaRuntimeImpl> for SampleConsumerBuilder<I> {
    fn get_instance_identifier(&self) -> &InstanceSpecifier {
        //if InstanceSpecifier::ANY support enable by lola
        //then this API should get InstanceSpecifier from FFI Call
        todo!()
    }
}

#[cfg(test)]
mod test {
    use com_api_concept::{SampleContainer, Subscription};

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
}
