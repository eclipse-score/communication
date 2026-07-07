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

//! This crate provides a mock implementation of the COM API for testing purposes.
//! It is meant to be used in conjunction with the `com-api` crate.
//! The mock implementation does not perform any real IPC and is not meant to be used in production.
//! It is only meant to be used for testing and development.

#![allow(dead_code)]
//lifetime warning for all the Sample struct impl block . it is required for the Sample struct
// event lifetime parameter
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
use futures::stream::{self, Stream};
use std::collections::VecDeque;
use std::path::Path;

use com_api_concept::{
    Builder, CommData, Consumer, ConsumerBuilder, ConsumerDescriptor, FieldMethods, FieldPublisher,
    FieldSampleMut, FieldSubscriber, FieldSubscription, FindServiceSpecifier, InstanceSpecifier,
    Interface, MethodReturnTypePtr, Producer, ProducerBuilder, ProviderInfo, Result, Runtime,
    SampleContainer, SampleMaybeUninit as SampleMaybeUninitTrait, ServiceDiscovery, Subscriber,
    Subscription,
};

pub struct MockRuntimeImpl {}

#[derive(Clone, Debug)]
pub struct MockProviderInfo {
    instance_specifier: InstanceSpecifier,
}

impl ProviderInfo for MockProviderInfo {
    fn offer_service(&self) -> Result<()> {
        todo!()
    }

    fn stop_offer_service(&self) -> Result<()> {
        todo!()
    }
}

#[derive(Clone, Debug)]
pub struct MockConsumerInfo {
    instance_specifier: InstanceSpecifier,
}

impl Runtime for MockRuntimeImpl {
    type ServiceDiscovery<I: Interface + Send> = MockConsumerDiscovery<I>;
    type Subscriber<T: CommData + Debug> = SubscribableImpl<T>;
    type ProducerBuilder<I: Interface> = MockProducerBuilder<I>;
    type Publisher<T: CommData + Debug> = Publisher<T>;
    type ProviderInfo = MockProviderInfo;
    type ConsumerInfo = MockConsumerInfo;
    type FieldSubscriber<T: CommData + Debug> = MockFieldSubscriber<T>;
    type FieldPublisher<T: CommData + Debug> = MockFieldPublisher<T>;

    fn find_service<I: Interface + Send>(
        &self,
        _instance_specifier: FindServiceSpecifier,
    ) -> Self::ServiceDiscovery<I> {
        MockConsumerDiscovery {
            _interface: PhantomData,
        }
    }

    fn producer_builder<I: Interface>(
        &self,
        instance_specifier: InstanceSpecifier,
    ) -> Self::ProducerBuilder<I> {
        MockProducerBuilder::new(self, instance_specifier)
    }
}

#[derive(Debug)]
struct MockEvent<T> {
    event: PhantomData<T>,
}

#[derive(Debug)]
struct MockBinding<'a, T>
where
    T: Send,
{
    data: *mut T,
    event: &'a MockEvent<T>,
}

unsafe impl<'a, T> Send for MockBinding<'a, T> where T: CommData + Debug {}

#[derive(Debug)]
enum SampleBinding<'a, T>
where
    T: CommData + Debug,
{
    Mock(MockBinding<'a, T>),
    Test(Box<T>),
}

#[derive(Debug)]
pub struct Sample<'a, T>
where
    T: CommData + Debug,
{
    id: usize,
    inner: SampleBinding<'a, T>,
}

static ID_COUNTER: AtomicUsize = AtomicUsize::new(0);

impl<'a, T> From<T> for Sample<'a, T>
where
    T: CommData + Debug,
{
    fn from(value: T) -> Self {
        Self {
            id: ID_COUNTER.fetch_add(1, std::sync::atomic::Ordering::Relaxed),
            inner: SampleBinding::Test(Box::new(value)),
        }
    }
}

impl<'a, T> Deref for Sample<'a, T>
where
    T: CommData + Debug,
{
    type Target = T;

    fn deref(&self) -> &Self::Target {
        match &self.inner {
            SampleBinding::Mock(_mock) => unimplemented!(),
            SampleBinding::Test(test) => test.as_ref(),
        }
    }
}

impl<'a, T> com_api_concept::Sample<T> for Sample<'a, T> where T: CommData + Debug {}

impl<'a, T> PartialEq for Sample<'a, T>
where
    T: CommData + Debug,
{
    fn eq(&self, other: &Self) -> bool {
        self.id == other.id
    }
}

impl<'a, T> Eq for Sample<'a, T> where T: CommData + Debug {}

impl<'a, T> PartialOrd for Sample<'a, T>
where
    T: CommData + Debug,
{
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl<'a, T> Ord for Sample<'a, T>
where
    T: CommData + Debug,
{
    fn cmp(&self, other: &Self) -> Ordering {
        self.id.cmp(&other.id)
    }
}

#[derive(Debug)]
pub struct SampleMut<'a, T>
where
    T: CommData + Debug,
{
    data: T,
    lifetime: PhantomData<&'a T>,
}

/// Base trait — no commit operation, just DerefMut access.
impl<'a, T> com_api_concept::SampleMut<T> for SampleMut<'a, T> where T: CommData + Debug {}

/// Event-specific extension: `send()` consumes and publishes the sample.
impl<'a, T> com_api_concept::EventSampleMut<T> for SampleMut<'a, T>
where
    T: CommData + Debug,
{
    fn send(self) -> com_api_concept::Result<()> {
        todo!()
    }
}

impl<'a, T> Deref for SampleMut<'a, T>
where
    T: CommData + Debug,
{
    type Target = T;

    fn deref(&self) -> &Self::Target {
        &self.data
    }
}

impl<'a, T> DerefMut for SampleMut<'a, T>
where
    T: CommData + Debug,
{
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.data
    }
}

#[derive(Debug)]
pub struct SampleMaybeUninit<'a, T>
where
    T: CommData + Debug,
{
    data: MaybeUninit<T>,
    lifetime: PhantomData<&'a T>,
}

impl<'a, T> com_api_concept::SampleMaybeUninit<T> for SampleMaybeUninit<'a, T>
where
    T: CommData + Debug,
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
    T: CommData + Debug,
{
    fn as_mut(&mut self) -> &mut core::mem::MaybeUninit<T> {
        &mut self.data
    }
}

#[derive(Debug)]
pub struct SubscribableImpl<T> {
    identifier: &'static str,
    instance_info: MockConsumerInfo,
    data: PhantomData<T>,
}

impl<T: CommData + Debug> Subscriber<T, MockRuntimeImpl> for SubscribableImpl<T> {
    type Subscription = SubscriberImpl<T>;
    fn new(
        identifier: &'static str,
        instance_info: MockConsumerInfo,
    ) -> com_api_concept::Result<Self> {
        Ok(Self {
            identifier,
            instance_info,
            data: PhantomData,
        })
    }
    fn subscribe(self, _max_num_samples: usize) -> com_api_concept::Result<Self::Subscription> {
        Ok(SubscriberImpl {
            identifier: self.identifier,
            instance_info: self.instance_info.clone(),
            data: VecDeque::new(),
        })
    }
}

#[derive(Debug)]
pub struct SubscriberImpl<T>
where
    T: CommData + Debug,
{
    identifier: &'static str,
    instance_info: MockConsumerInfo,
    data: VecDeque<T>,
}

impl<T> SubscriberImpl<T>
where
    T: CommData + Debug,
{
    pub fn add_data(&mut self, data: T) {
        self.data.push_front(data);
    }
}

impl<T> Subscription<T, MockRuntimeImpl> for SubscriberImpl<T>
where
    T: CommData + Debug,
{
    type Subscriber = SubscribableImpl<T>;
    type Sample<'a> = Sample<'a, T>;

    fn unsubscribe(self) -> Self::Subscriber {
        SubscribableImpl {
            identifier: self.identifier,
            instance_info: self.instance_info,
            data: PhantomData,
        }
    }

    fn try_receive<'a>(
        &'a self,
        _scratch: &'_ mut SampleContainer<Self::Sample<'a>>,
        _max_samples: usize,
    ) -> com_api_concept::Result<usize> {
        todo!()
    }

    #[allow(clippy::manual_async_fn)]
    fn cancellable_receive<'a>(
        &'a self,
        _scratch: SampleContainer<Self::Sample<'a>>,
        _new_samples: usize,
        _max_samples: usize,
        _cancellation: impl Future<Output = ()> + Send + 'static,
    ) -> impl Future<Output = (SampleContainer<Self::Sample<'a>>, Result<usize>)> + 'a {
        async { todo!() }
    }

    #[allow(clippy::manual_async_fn)]
    fn to_stream<'a>(&'a mut self) -> impl Stream<Item = Result<Self::Sample<'a>>> + Unpin + 'a {
        stream::empty()
    }
}

pub struct Publisher<T> {
    _data: PhantomData<T>,
}

impl<T> Default for Publisher<T>
where
    T: CommData + Debug,
{
    fn default() -> Self {
        Self::new()
    }
}

impl<T> Publisher<T>
where
    T: CommData + Debug,
{
    #[must_use = "creating a Publisher without using it is likely a mistake; the publisher must be \
                  assigned or used in some way"]
    pub fn new() -> Self {
        Self { _data: PhantomData }
    }
}

impl<T> com_api_concept::Publisher<T, MockRuntimeImpl> for Publisher<T>
where
    T: CommData + Debug,
{
    // SampleMut<'a, T> implements EventSampleMut<T>, satisfying the CommittedSample bound.
    type CommittedSample<'a>
        = SampleMut<'a, T>
    where
        Self: 'a;

    type SampleMaybeUninit<'a>
        = SampleMaybeUninit<'a, T>
    where
        Self: 'a;

    fn allocate(&self) -> com_api_concept::Result<SampleMaybeUninit<'_, T>> {
        Ok(SampleMaybeUninit {
            data: MaybeUninit::uninit(),
            lifetime: PhantomData,
        })
    }

    fn new(_identifier: &str, _instance_info: MockProviderInfo) -> com_api_concept::Result<Self> {
        Ok(Self { _data: PhantomData })
    }
}

pub struct MockConsumerDiscovery<I> {
    _interface: PhantomData<I>,
}

impl<I> MockConsumerDiscovery<I> {
    fn new(_runtime: &MockRuntimeImpl, _instance_specifier: InstanceSpecifier) -> Self {
        Self {
            _interface: PhantomData,
        }
    }
}

impl<I: Interface + Send> ServiceDiscovery<I, MockRuntimeImpl> for MockConsumerDiscovery<I>
where
    MockConsumerBuilder<I>: ConsumerBuilder<I, MockRuntimeImpl>,
{
    type ConsumerBuilder = MockConsumerBuilder<I>;
    type ServiceEnumerator = Vec<MockConsumerBuilder<I>>;

    fn get_available_instances(&self) -> com_api_concept::Result<Self::ServiceEnumerator> {
        Ok(Vec::new())
    }

    #[allow(clippy::manual_async_fn)]
    fn get_available_instances_async(
        &self,
    ) -> impl Future<Output = com_api_concept::Result<Self::ServiceEnumerator>> + Send {
        async { Ok(Vec::new()) }
    }
}

pub struct MockProducerBuilder<I: Interface> {
    instance_specifier: InstanceSpecifier,
    _interface: PhantomData<I>,
}

impl<I: Interface> MockProducerBuilder<I> {
    fn new(_runtime: &MockRuntimeImpl, instance_specifier: InstanceSpecifier) -> Self {
        Self {
            instance_specifier,
            _interface: PhantomData,
        }
    }
}

impl<I: Interface> ProducerBuilder<I, MockRuntimeImpl> for MockProducerBuilder<I> {}

impl<I: Interface> Builder<I::Producer<MockRuntimeImpl>> for MockProducerBuilder<I> {
    fn build(self) -> Result<I::Producer<MockRuntimeImpl>> {
        let instance_info = MockProviderInfo {
            instance_specifier: self.instance_specifier.clone(),
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

pub struct MockConsumerBuilder<I: Interface> {
    instance_specifier: InstanceSpecifier,
    _interface: PhantomData<I>,
}

impl<I: Interface> ConsumerDescriptor<MockRuntimeImpl> for MockConsumerBuilder<I> {
    fn get_instance_identifier(&self) -> &InstanceSpecifier {
        todo!()
    }
}

impl<I: Interface> ConsumerBuilder<I, MockRuntimeImpl> for MockConsumerBuilder<I> {}

impl<I: Interface> Builder<I::Consumer<MockRuntimeImpl>> for MockConsumerBuilder<I> {
    fn build(self) -> com_api_concept::Result<I::Consumer<MockRuntimeImpl>> {
        let instance_info = MockConsumerInfo {
            instance_specifier: self.instance_specifier.clone(),
        };

        Ok(Consumer::new(instance_info))
    }
}

pub struct RuntimeBuilderImpl {}

impl Builder<MockRuntimeImpl> for RuntimeBuilderImpl {
    fn build(self) -> com_api_concept::Result<MockRuntimeImpl> {
        Ok(MockRuntimeImpl {})
    }
}

/// Entry point for the default implementation for the com module of s-core
impl com_api_concept::RuntimeBuilder<MockRuntimeImpl> for RuntimeBuilderImpl {
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

/// Field subscriber type which implements the FieldSubscriber trait for Mock runtime.
pub struct MockFieldSubscriber<T: CommData + Debug> {
    identifier: &'static str,
    instance_info: MockConsumerInfo,
    _data: PhantomData<T>,
}

/// Marker implementation of FieldSubscriber trait.
impl<T: CommData + Debug> FieldSubscriber<T, MockRuntimeImpl> for MockFieldSubscriber<T> {}

/// Implementation of FieldMethods trait for MockFieldSubscriber.
impl<T: CommData + Debug> FieldMethods<T, MockRuntimeImpl> for MockFieldSubscriber<T> {
    fn get(&self) -> Result<MethodReturnTypePtr<T>> {
        todo!()
    }
    fn set(&self, _value: T) -> Result<MethodReturnTypePtr<T>> {
        todo!()
    }
}

/// Implementation of Subscriber trait for MockFieldSubscriber.
impl<T: CommData + Debug> Subscriber<T, MockRuntimeImpl> for MockFieldSubscriber<T> {
    type Subscription = MockFieldSubscription<T>;

    fn new(identifier: &'static str, instance_info: MockConsumerInfo) -> Result<Self> {
        Ok(Self {
            identifier,
            instance_info,
            _data: PhantomData,
        })
    }

    fn subscribe(self, _max_num_samples: usize) -> Result<Self::Subscription> {
        Ok(MockFieldSubscription {
            identifier: self.identifier,
            instance_info: self.instance_info,
            _data: PhantomData,
        })
    }
}

/// FieldSubscription type which provides data receiving APIs and unsubscribe method.
pub struct MockFieldSubscription<T: CommData + Debug> {
    identifier: &'static str,
    instance_info: MockConsumerInfo,
    _data: PhantomData<T>,
}

impl<T: CommData + Debug> FieldSubscription<T, MockRuntimeImpl> for MockFieldSubscription<T> {
    fn get_free_sample_count(&self) -> Result<usize> {
        todo!()
    }

    fn get_num_new_samples_available(&self) -> Result<usize> {
        todo!()
    }
}

/// Implementation of Subscription trait which provides receiving APIs.
impl<T: CommData + Debug> Subscription<T, MockRuntimeImpl> for MockFieldSubscription<T> {
    type Subscriber = MockFieldSubscriber<T>;
    type Sample<'a>
        = Sample<'a, T>
    where
        Self: 'a;

    fn unsubscribe(self) -> Self::Subscriber {
        MockFieldSubscriber {
            identifier: self.identifier,
            instance_info: self.instance_info,
            _data: PhantomData,
        }
    }

    fn try_receive<'a>(
        &'a self,
        _scratch: &'_ mut SampleContainer<Self::Sample<'a>>,
        _max_samples: usize,
    ) -> Result<usize> {
        todo!()
    }

    fn cancellable_receive<'a>(
        &'a self,
        _scratch: SampleContainer<Self::Sample<'a>>,
        _new_samples: usize,
        _max_samples: usize,
        _cancellation: impl Future<Output = ()> + Send + 'static,
    ) -> impl Future<Output = (SampleContainer<Self::Sample<'a>>, Result<usize>)> + 'a {
        async { todo!() }
    }

    fn to_stream<'a>(&'a mut self) -> impl Stream<Item = Result<Self::Sample<'a>>> + Unpin + 'a {
        stream::empty()
    }
}

/// Implementation of FieldMethods trait for MockFieldSubscription.
impl<T: CommData + Debug> FieldMethods<T, MockRuntimeImpl> for MockFieldSubscription<T> {
    fn get(&self) -> Result<MethodReturnTypePtr<T>> {
        todo!()
    }
    fn set(&self, _value: T) -> Result<MethodReturnTypePtr<T>> {
        todo!()
    }
}

/// Field publisher type for Mock runtime.
pub struct MockFieldPublisher<T: CommData + Debug> {
    _data: PhantomData<T>,
}

impl<T: CommData + Debug> Clone for MockFieldPublisher<T> {
    fn clone(&self) -> Self {
        MockFieldPublisher { _data: PhantomData }
    }
}

impl<T: CommData + Debug> Copy for MockFieldPublisher<T> {}

/// Field sample mutable type.
#[derive(Debug)]
pub struct MockFieldSampleMut<'a, T: CommData + Debug> {
    data: T,
    _lifetime: PhantomData<&'a T>,
}

impl<'a, T: CommData + Debug> Deref for MockFieldSampleMut<'a, T> {
    type Target = T;
    fn deref(&self) -> &T {
        &self.data
    }
}

impl<'a, T: CommData + Debug> DerefMut for MockFieldSampleMut<'a, T> {
    fn deref_mut(&mut self) -> &mut T {
        &mut self.data
    }
}

impl<'a, T: CommData + Debug> com_api_concept::SampleMut<T> for MockFieldSampleMut<'a, T> {}

impl<'a, T: CommData + Debug> FieldSampleMut<T> for MockFieldSampleMut<'a, T> {
    fn update(self) -> Result<()> {
        todo!()
    }
}

/// Field sample maybe uninit type.
#[derive(Debug)]
pub struct MockFieldSampleMaybeUninit<'a, T: CommData + Debug> {
    data: MaybeUninit<T>,
    _lifetime: PhantomData<&'a T>,
}

impl<'a, T: CommData + Debug> AsMut<MaybeUninit<T>> for MockFieldSampleMaybeUninit<'a, T> {
    fn as_mut(&mut self) -> &mut MaybeUninit<T> {
        &mut self.data
    }
}

impl<'a, T: CommData + Debug> SampleMaybeUninitTrait<T> for MockFieldSampleMaybeUninit<'a, T> {
    type SampleMut = MockFieldSampleMut<'a, T>;

    unsafe fn assume_init(self) -> MockFieldSampleMut<'a, T> {
        MockFieldSampleMut {
            data: unsafe { self.data.assume_init() },
            _lifetime: PhantomData,
        }
    }

    fn write(self, value: T) -> MockFieldSampleMut<'a, T> {
        MockFieldSampleMut {
            data: value,
            _lifetime: PhantomData,
        }
    }
}

impl<T: CommData + Debug> FieldPublisher<T, MockRuntimeImpl> for MockFieldPublisher<T> {
    type FieldSample<'a>
        = MockFieldSampleMut<'a, T>
    where
        Self: 'a;
    type SampleMaybeUninit<'a>
        = MockFieldSampleMaybeUninit<'a, T>
    where
        Self: 'a;

    fn new(_identifier: &str, _instance_info: MockProviderInfo) -> Result<Self> {
        Ok(Self { _data: PhantomData })
    }

    fn allocate(&self) -> Result<Self::SampleMaybeUninit<'_>> {
        Ok(MockFieldSampleMaybeUninit {
            data: MaybeUninit::uninit(),
            _lifetime: PhantomData,
        })
    }

    fn update(&self, _value: &T) -> Result<()> {
        todo!()
    }

    fn register_set_handler<'a>(&self) -> impl Future<Output = Result<&'a T>> + 'a {
        async { todo!() }
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
            let sample_buf = SampleContainer::new(1);
            let (_returned_buf, result) = test_subscriber.receive(sample_buf, 1, 1).await;
            match result {
                Ok(()) => {}
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
