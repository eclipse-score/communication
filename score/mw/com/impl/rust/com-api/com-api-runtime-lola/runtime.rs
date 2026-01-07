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
use core::sync::atomic::AtomicUsize;
use std::collections::VecDeque;
use std::path::Path;
use std::sync::Arc;

use com_api_concept::{
    Builder, Consumer, ConsumerBuilder, ConsumerDescriptor, Error, FindServiceSpecifier,
    InstanceSpecifier, Interface, Producer, ProducerBuilder, Reloc, Result, Runtime,
    SampleContainer, ServiceDiscovery, Subscriber, Subscription,
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
    handle_container: Option<Arc<proxy_bridge_rs::HandleContainer>>,
    handle_index: usize,
    interface_id: &'static str,
}

impl LolaConsumerInfo {
    /// Get a reference to the handle, guaranteed valid as long as this struct exists
    pub fn get_handle(&self) -> Option<&HandleType> {
        self.handle_container
            .as_ref()
            .map(|c| &c[self.handle_index])
    }
}

unsafe impl Send for LolaConsumerInfo {}
unsafe impl Sync for LolaConsumerInfo {}

unsafe impl Send for LolaProviderInfo {}
unsafe impl Sync for LolaProviderInfo {}

impl Runtime for LolaRuntimeImpl {
    type ServiceDiscovery<I: Interface> = SampleConsumerDiscovery<I>;
    type Subscriber<T: Reloc + Send + Debug> = SubscribableImpl<T>;
    type ProducerBuilder<I: Interface> = SampleProducerBuilder<I>;
    type Publisher<T: Reloc + Send + Debug> = Publisher<T>;
    type ProviderInfo = LolaProviderInfo;
    type ConsumerInfo = LolaConsumerInfo;

    fn find_service<I: Interface>(
        &self,
        instance_specifier: FindServiceSpecifier,
    ) -> Self::ServiceDiscovery<I> {
        SampleConsumerDiscovery {
            instance_info: LolaConsumerInfo {
                instance_specifier: match instance_specifier {
                    FindServiceSpecifier::Any => InstanceSpecifier::new("/").unwrap(),
                    FindServiceSpecifier::Specific(spec) => spec,
                },
                handle_container: None,
                handle_index: 0,
                interface_id: I::TYPE_ID,
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
    T: Send,
{
    // Pointer to the managed object rather than &T to avoid lifetime issues
    // Lifetime manage by C++ Lola side so we use raw pointer here
    // zero-copy consumer pattern
    //
    // DESIGN RATIONALE:
    // - Uses *const T instead of &'a T to avoid artificial Rust lifetime constraints
    // - C++ LoLa framework owns the data lifecycle, Rust cannot enforce lifetimes
    // - Raw pointer signals "external memory management" to maintainers
    //
    // SAFETY:
    // - Pointer validity guaranteed by C++ LoLa while Sample exists
    // - Dereferenced safely via Deref trait (unsafe { data.as_ref().unwrap() })
    // - Only 8 bytes stored per sample (pointer address, zero data copy)
    data: *const T,
}

unsafe impl<T> Send for LolaBinding<T> where T: Send {}

#[derive(Debug)]
enum SampleBinding<T>
where
    T: Send,
{
    Lola(LolaBinding<T>),
    Test(Box<T>),
}

#[derive(Debug)]
pub struct Sample<T>
where
    T: Reloc + Send + Debug,
{
    id: usize,
    inner: SampleBinding<T>,
}

static ID_COUNTER: AtomicUsize = AtomicUsize::new(0);

impl<T> From<T> for Sample<T>
where
    T: Reloc + Send + Debug,
{
    fn from(value: T) -> Self {
        Self {
            id: ID_COUNTER.fetch_add(1, std::sync::atomic::Ordering::Relaxed),
            inner: SampleBinding::Test(Box::new(value)),
        }
    }
}

impl<T> Deref for Sample<T>
where
    T: Reloc + Send + Debug,
{
    type Target = T;

    fn deref(&self) -> &Self::Target {
        match &self.inner {
            SampleBinding::Lola(lola) => unsafe { lola.data.as_ref().unwrap() },
            SampleBinding::Test(test) => test.as_ref(),
        }
    }
}

impl<T> com_api_concept::Sample<T> for Sample<T> where T: Send + Reloc + Debug {}

impl<T> PartialEq for Sample<T>
where
    T: Send + Reloc + Debug,
{
    fn eq(&self, other: &Self) -> bool {
        self.id == other.id
    }
}

impl<T> Eq for Sample<T> where T: Send + Reloc + Debug {}

impl<T> PartialOrd for Sample<T>
where
    T: Send + Reloc + Debug,
{
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl<T> Ord for Sample<T>
where
    T: Send + Reloc + Debug,
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
    T: Reloc + Send + Debug,
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
    T: Reloc + Send + Debug,
{
    data: MaybeUninit<T>,
    lifetime: PhantomData<&'a T>,
}

impl<'a, T> com_api_concept::SampleMaybeUninit<T> for SampleMaybeUninit<'a, T>
where
    T: Reloc + Send + Debug,
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
    T: Reloc + Send + Debug,
{
    fn as_mut(&mut self) -> &mut core::mem::MaybeUninit<T> {
        &mut self.data
    }
}

#[derive(Debug)]
pub struct SubscribableImpl<T> {
    identifier: String,
    instance_info: Option<LolaConsumerInfo>,
    proxy_instance: Option<*mut ProxyBase>, // Stores the proxy instance
    data: PhantomData<T>,
}

impl<T> Default for SubscribableImpl<T> {
    fn default() -> Self {
        Self {
            identifier: String::new(),
            instance_info: None,
            proxy_instance: None,
            data: PhantomData,
        }
    }
}

impl<T: Reloc + Send + Debug> Subscriber<T, LolaRuntimeImpl> for SubscribableImpl<T> {
    type Subscription = SubscriberImpl<T>;
    fn new(identifier: &str, instance_info: LolaConsumerInfo) -> com_api_concept::Result<Self> {
        let handle = instance_info.get_handle().ok_or(Error::Fail)?;
        let proxy =
            unsafe { generic_bridge_ffi_rs::create_proxy(instance_info.interface_id, handle) };

        Ok(Self {
            identifier: identifier.to_string(),
            instance_info: Some(instance_info),
            proxy_instance: Some(proxy),
            data: PhantomData,
        })
    }
    fn subscribe(&self, max_num_samples: usize) -> com_api_concept::Result<Self::Subscription> {
        let instance_info = self.instance_info.as_ref().ok_or(Error::Fail)?;
        let event_instance = unsafe {
            generic_bridge_ffi_rs::get_event_from_proxy(
                self.proxy_instance.unwrap(),
                instance_info.interface_id,
                &self.identifier,
            )
        };

        let _status = unsafe {
            generic_bridge_ffi_rs::subscribe_to_event(
                event_instance,
                max_num_samples.try_into().unwrap(),
            )
        };
        // Store in SubscriberImpl with event, max_num_samples
        Ok(SubscriberImpl {
            event: Some(event_instance),
            event_type: self.identifier.clone(),
            max_num_samples,
            data: VecDeque::new(),
        })
    }
}

#[derive(Default, Debug)]
pub struct SubscriberImpl<T>
where
    T: Reloc + Send + Debug,
{
    event: Option<*mut ProxyEventBase>,
    event_type: String,
    max_num_samples: usize,
    data: VecDeque<T>,
}

impl<T> SubscriberImpl<T>
where
    T: Reloc + Send + Debug,
{
    #[must_use = "creating a SubscriberImpl without using it is likely a mistake; the subscriber must be assigned or used in some way"]
    pub fn new() -> Self {
        Self {
            event: None,
            event_type: String::new(),
            max_num_samples: 0,
            data: VecDeque::new(),
        }
    }

    pub fn add_data(&mut self, data: T) {
        self.data.push_front(data);
    }

    fn type_name_short() -> &'static str {
        std::any::type_name::<T>().split("::").last().unwrap_or("")
    }
}

impl<T> Subscription<T, LolaRuntimeImpl> for SubscriberImpl<T>
where
    T: Reloc + Send + Debug,
{
    type Subscriber = SubscribableImpl<T>;
    type Sample<'a>
        = Sample<T>
    where
        T: 'a;

    fn unsubscribe(self) -> Self::Subscriber {
        SubscribableImpl::default()
    }

    fn try_receive<'a>(
        &'a self,
        scratch: &'_ mut SampleContainer<Self::Sample<'a>>,
        _max_samples: usize,
    ) -> com_api_concept::Result<usize> {
        if let Some(event) = self.event {
            let mut callback = |raw_sample: *mut sample_ptr_rs::SamplePtr<T>| {
                if !raw_sample.is_null() {
                    // Read the sample pointer from the raw pointer
                    let sample_ptr = unsafe { std::ptr::read(raw_sample) };

                    let wrapped_sample = Sample {
                        id: ID_COUNTER.fetch_add(1, std::sync::atomic::Ordering::Relaxed),
                        inner: SampleBinding::Lola(LolaBinding {
                            // Get reference to the managed object
                            data: unsafe { sample_ptr.get_managed_object() },
                        }),
                    };
                    // Note: We can't propagate errors from the callback, so we silently drop on error
                    let _ = scratch.push_back(wrapped_sample);
                }
            };

            // Convert closure to FatPtr for C++ callback
            let dyn_callback: &mut dyn FnMut(*mut sample_ptr_rs::SamplePtr<T>) = &mut callback;

            // SAFETY: Transmute trait object to FatPtr (both are two-pointer fat pointers)
            let fat_ptr: FatPtr = unsafe { std::mem::transmute(dyn_callback) };

            // Get samples from event using FFI
            let count = unsafe {
                generic_bridge_ffi_rs::get_samples_from_event(
                    event,
                    Self::type_name_short(),
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
    T: Reloc + Send + Debug,
{
    fn default() -> Self {
        Self::new()
    }
}

impl<T> Publisher<T>
where
    T: Reloc + Send + Debug,
{
    #[must_use = "creating a Publisher without using it is likely a mistake; the publisher must be assigned or used in some way"]
    pub fn new() -> Self {
        Self { _data: PhantomData }
    }
}

impl<T> com_api_concept::Publisher<T, LolaRuntimeImpl> for Publisher<T>
where
    T: Reloc + Send + Debug,
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
    instance_info: LolaConsumerInfo,
    _interface: PhantomData<I>,
}

impl<I: Interface> SampleConsumerDiscovery<I> {
    fn new(_runtime: &LolaRuntimeImpl, instance_specifier: InstanceSpecifier) -> Self {
        Self {
            instance_info: LolaConsumerInfo {
                instance_specifier: instance_specifier,
                handle_container: None,
                handle_index: 0,
                interface_id: I::TYPE_ID,
            },
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
        let mut available_instances = Vec::new();
        let instance_specifier_ = proxy_bridge_rs::InstanceSpecifier::try_from(
            self.instance_info.instance_specifier.as_ref(),
        )
        .map_err(|_| Error::Fail)?;

        let service_handle =
            proxy_bridge_rs::find_service(instance_specifier_).map_err(|_| Error::Fail)?;

        if service_handle.is_empty() {
            eprintln!("Runtime #No service instances found."); // TODO: Enable logging
            return Ok(available_instances);
        }
        let service_handle_arc = Arc::new(service_handle); // Wrap container in Arc

        let instance_info = LolaConsumerInfo {
            instance_specifier: self.instance_info.instance_specifier.clone(),
            handle_container: Some(service_handle_arc.clone()), // Store Arc
            handle_index: 0, // Assuming single handle for simplicity
            interface_id: I::TYPE_ID,
        };

        available_instances.push(SampleConsumerBuilder {
            instance_info,
            _interface: PhantomData,
        });
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
            interface_id: I::TYPE_ID,
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
