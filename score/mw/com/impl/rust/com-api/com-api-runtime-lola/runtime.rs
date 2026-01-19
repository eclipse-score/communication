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
use core::mem::MaybeUninit;
use core::ops::{Deref, DerefMut};
use std::collections::VecDeque;
use std::path::Path;
use std::sync::atomic::AtomicUsize;
use std::sync::Arc;

use com_api_concept::{
    Builder, Consumer, ConsumerBuilder, ConsumerDescriptor, Error, FindServiceSpecifier,
    InstanceSpecifier, Interface, Producer, ProducerBuilder, Reloc, Result, Runtime,
    SampleContainer, ServiceDiscovery, Subscriber, Subscription, TypeInfo,
};

use generic_bridge_ffi_rs::*;

pub struct LolaRuntimeImpl {}

#[derive(Clone, Debug)]
pub struct LolaProviderInfo {
    instance_specifier: InstanceSpecifier,
    interface_id: &'static str,
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
    type Subscriber<T: Reloc + Send + Debug + TypeInfo> = SubscribableImpl<T>;
    type ProducerBuilder<I: Interface> = SampleProducerBuilder<I>;
    type Publisher<T: Reloc + Send + Debug + TypeInfo> = Publisher<T>;
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

#[derive(Debug)]
struct LolaEvent<T> {
    event: PhantomData<T>,
}

#[derive(Debug)]
struct LolaBinding<T>
where
    T: Send + TypeInfo,
{
    //TODO: Ticket-237218/[eclipse-score/communication/issues/134] SamplePtr should be ManuallyDrop
    data: sample_ptr_rs::SamplePtr<T>,
}

impl<T> Drop for LolaBinding<T>
where
    T: Send + TypeInfo,
{
    fn drop(&mut self) {
        //SAFETY: It is safe to call the delete function because data ptr is valid
        //SamplePtr created by FFI
        unsafe {
            generic_bridge_ffi_rs::sample_ptr_delete(
                std::ptr::from_mut(&mut self.data) as *mut std::ffi::c_void,
                T::ID,
            );
        }
    }
}

#[derive(Debug)]
pub struct Sample<T>
where
    T: Reloc + Send + Debug + TypeInfo,
{
    //we need unique id for each sample to implement Ord and Eq traits for sorting in SampleContainer
    id: usize,
    inner: LolaBinding<T>,
}

static ID_COUNTER: AtomicUsize = AtomicUsize::new(0);

impl<T> Sample<T>
where
    T: Reloc + Send + Debug + TypeInfo,
{
    pub fn get_data(&self) -> &T {
        //SAFETY: It is safe to get the data pointer because SamplePtr is valid
        //and data is valid as long as SamplePtr is valid
        unsafe {
            let data_ptr = generic_bridge_ffi_rs::sample_ptr_get(
                std::ptr::from_ref(&self.inner.data) as *const std::ffi::c_void,
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
    T: Reloc + Send + Debug + TypeInfo,
{
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.get_data()
    }
}

impl<T> com_api_concept::Sample<T> for Sample<T> where T: Send + Reloc + Debug + TypeInfo {}

// Ordering traits for Sample<T> are using id field to provide total ordering
impl<T> PartialEq for Sample<T>
where
    T: Send + Reloc + Debug + TypeInfo,
{
    fn eq(&self, other: &Self) -> bool {
        self.id == other.id
    }
}

impl<T> Eq for Sample<T> where T: Send + Reloc + Debug + TypeInfo {}

impl<T> PartialOrd for Sample<T>
where
    T: Send + Reloc + Debug + TypeInfo,
{
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl<T> Ord for Sample<T>
where
    T: Send + Reloc + Debug + TypeInfo,
{
    fn cmp(&self, other: &Self) -> Ordering {
        self.id.cmp(&other.id)
    }
}

#[derive(Debug)]
pub struct SampleMut<'a, T>
where
    T: Reloc,
{
    data: T,
    lifetime: PhantomData<&'a T>,
}

impl<'a, T> com_api_concept::SampleMut<T> for SampleMut<'a, T>
where
    T: Reloc + Send + Debug + TypeInfo,
{
    type Sample = Sample<T>;

    fn into_sample(self) -> Self::Sample {
        todo!()
    }

    fn send(self) -> com_api_concept::Result<()> {
        todo!()
    }
}

impl<'a, T> Deref for SampleMut<'a, T>
where
    T: Reloc,
{
    type Target = T;

    fn deref(&self) -> &Self::Target {
        &self.data
    }
}

impl<'a, T> DerefMut for SampleMut<'a, T>
where
    T: Reloc,
{
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.data
    }
}

#[derive(Debug)]
pub struct SampleMaybeUninit<'a, T>
where
    T: Reloc + Send + Debug + TypeInfo,
{
    data: MaybeUninit<T>,
    lifetime: PhantomData<&'a T>,
}

impl<'a, T> com_api_concept::SampleMaybeUninit<T> for SampleMaybeUninit<'a, T>
where
    T: Reloc + Send + Debug + TypeInfo,
{
    type SampleMut = SampleMut<'a, T>;

    fn write(self, val: T) -> SampleMut<'a, T> {
        SampleMut {
            data: val,
            lifetime: PhantomData,
        }
    }

    unsafe fn assume_init(self) -> SampleMut<'a, T> {
        SampleMut {
            data: unsafe { self.data.assume_init() },
            lifetime: PhantomData,
        }
    }
}

impl<'a, T> AsMut<core::mem::MaybeUninit<T>> for SampleMaybeUninit<'a, T>
where
    T: Reloc + Send + Debug + TypeInfo,
{
    fn as_mut(&mut self) -> &mut core::mem::MaybeUninit<T> {
        &mut self.data
    }
}

// Manages the lifetime of the native proxy instance, user should clone this to share between threads
// Always use this struct to manage the proxy instance pointer
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

struct NativeProxyBase {
    proxy: *mut ProxyBase, // Stores the proxy instance
}

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

#[derive(Debug)]
pub struct SubscribableImpl<T> {
    identifier: &'static str,
    instance_info: LolaConsumerInfo,
    proxy_instance: ProxyInstanceManager,
    data: PhantomData<T>,
}

impl<T: Reloc + Send + Debug + TypeInfo> Subscriber<T, LolaRuntimeImpl> for SubscribableImpl<T> {
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
        //SAFETY: It is safe to get event from proxy because proxy_instance is valid
        // which was created during SubscribableImpl creation and instance_id is also same as proxy
        let event_instance = unsafe {
            generic_bridge_ffi_rs::get_event_from_proxy(
                self.proxy_instance.0.proxy,
                instance_info.interface_id,
                self.identifier,
            )
        };
        //SAFETY: It is safe to subscribe to event because event_instance is valid
        // which was obtained from valid proxy instance
        let status = unsafe {
            generic_bridge_ffi_rs::subscribe_to_event(
                event_instance,
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
    T: Reloc + Send + Debug + TypeInfo,
{
    //Safety: This can be used as raw pointer because it comes under proxy instance lifetime
    event: Option<*mut ProxyEventBase>,
    event_id: &'static str,
    max_num_samples: usize,
    data: VecDeque<T>,
    instance_info: LolaConsumerInfo,
    _proxy: ProxyInstanceManager,
}

impl<T> SubscriberImpl<T>
where
    T: Reloc + Send + Debug + TypeInfo,
{
    pub fn add_data(&mut self, data: T) {
        self.data.push_front(data);
    }
}

impl<T> Subscription<T, LolaRuntimeImpl> for SubscriberImpl<T>
where
    T: Reloc + Send + Debug + TypeInfo,
{
    type Subscriber = SubscribableImpl<T>;
    type Sample<'a>
        = Sample<T>
    where
        T: 'a;

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
        if let Some(event) = self.event {
            let mut callback = |raw_sample: *mut sample_ptr_rs::SamplePtr<T>| {
                if !raw_sample.is_null() {
                    //SAFETY: It is safe to read the sample pointer because
                    // raw_sample is valid pointer passed from FFI callback
                    // and raw_pointer is moved from FFI to Rust ownership here
                    let sample_ptr = unsafe { std::ptr::read(raw_sample) };

                    let wrapped_sample = Sample {
                        //Relaxed ordering is sufficient here as we just need a unique id for each sample
                        id: ID_COUNTER.fetch_add(1, std::sync::atomic::Ordering::Relaxed),
                        inner: LolaBinding { data: sample_ptr },
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
                    event,
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

pub struct Publisher<T> {
    _data: PhantomData<T>,
}

impl<T> Default for Publisher<T>
where
    T: Reloc + Send + Debug + TypeInfo,
{
    fn default() -> Self {
        Self::new()
    }
}

impl<T> Publisher<T>
where
    T: Reloc + Send + Debug + TypeInfo,
{
    #[must_use = "creating a Publisher without using it is likely a mistake; the publisher must be assigned or used in some way"]
    pub fn new() -> Self {
        Self { _data: PhantomData }
    }
}

impl<T> com_api_concept::Publisher<T, LolaRuntimeImpl> for Publisher<T>
where
    T: Reloc + Send + Debug + TypeInfo,
{
    type SampleMaybeUninit<'a>
        = SampleMaybeUninit<'a, T>
    where
        Self: 'a;

    fn allocate(&self) -> com_api_concept::Result<Self::SampleMaybeUninit<'_>> {
        Ok(SampleMaybeUninit {
            data: MaybeUninit::uninit(),
            lifetime: PhantomData,
        })
    }

    fn new(_identifier: &str, _instance_info: LolaProviderInfo) -> com_api_concept::Result<Self> {
        Ok(Self { _data: PhantomData })
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
        let instance_info = LolaProviderInfo {
            instance_specifier: self.instance_specifier.clone(),
            interface_id: I::INTERFACE_ID,
        };
        I::Producer::new(instance_info)
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

pub struct RuntimeBuilderImpl {}

impl Builder<LolaRuntimeImpl> for RuntimeBuilderImpl {
    fn build(self) -> com_api_concept::Result<LolaRuntimeImpl> {
        Ok(LolaRuntimeImpl {})
    }
}

/// Entry point for the default implementation for the com module of s-core
impl com_api_concept::RuntimeBuilder<LolaRuntimeImpl> for RuntimeBuilderImpl {
    fn load_config(&mut self, _config: &Path) -> &mut Self {
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
        Self {}
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
