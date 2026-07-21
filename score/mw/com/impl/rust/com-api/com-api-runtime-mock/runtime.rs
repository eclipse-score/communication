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

use concept::{
    Builder, CommData, Consumer, ConsumerBuilder, ConsumerDescriptor, FindServiceSpecifier,
    InstanceSpecifier, Interface, Producer, ProducerBuilder, ProviderInfo, Publisher, Result,
    Runtime, RuntimeBuilder, Sample, SampleContainer, SampleMaybeUninit, SampleMut,
    ServiceDiscovery, Subscriber, Subscription,
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
    type Subscriber<T: CommData + Debug> = MockSubscribableImpl<T>;
    type ProducerBuilder<I: Interface> = MockProducerBuilder<I>;
    type Publisher<T: CommData + Debug> = MockPublisher<T>;
    type ProviderInfo = MockProviderInfo;
    type ConsumerInfo = MockConsumerInfo;

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
pub struct MockSample<'a, T>
where
    T: CommData + Debug,
{
    id: usize,
    inner: SampleBinding<'a, T>,
}

static ID_COUNTER: AtomicUsize = AtomicUsize::new(0);

impl<'a, T> From<T> for MockSample<'a, T>
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

impl<'a, T> Deref for MockSample<'a, T>
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

impl<'a, T> Sample<T> for MockSample<'a, T> where T: CommData + Debug {}

impl<'a, T> PartialEq for MockSample<'a, T>
where
    T: CommData + Debug,
{
    fn eq(&self, other: &Self) -> bool {
        self.id == other.id
    }
}

impl<'a, T> Eq for MockSample<'a, T> where T: CommData + Debug {}

impl<'a, T> PartialOrd for MockSample<'a, T>
where
    T: CommData + Debug,
{
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl<'a, T> Ord for MockSample<'a, T>
where
    T: CommData + Debug,
{
    fn cmp(&self, other: &Self) -> Ordering {
        self.id.cmp(&other.id)
    }
}

#[derive(Debug)]
pub struct MockSampleMut<'a, T>
where
    T: CommData + Debug,
{
    data: T,
    lifetime: PhantomData<&'a T>,
}

impl<'a, T> SampleMut<T> for MockSampleMut<'a, T>
where
    T: CommData + Debug,
{
    fn send(self) -> Result<()> {
        todo!()
    }
}

impl<'a, T> Deref for MockSampleMut<'a, T>
where
    T: CommData + Debug,
{
    type Target = T;

    fn deref(&self) -> &Self::Target {
        &self.data
    }
}

impl<'a, T> DerefMut for MockSampleMut<'a, T>
where
    T: CommData + Debug,
{
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.data
    }
}

#[derive(Debug)]
pub struct MockSampleMaybeUninit<'a, T>
where
    T: CommData + Debug,
{
    data: MaybeUninit<T>,
    lifetime: PhantomData<&'a T>,
}

impl<'a, T> SampleMaybeUninit<T> for MockSampleMaybeUninit<'a, T>
where
    T: CommData + Debug,
{
    type SampleMut = MockSampleMut<'a, T>;

    fn write(self, val: T) -> MockSampleMut<'a, T> {
        MockSampleMut {
            data: val,
            lifetime: PhantomData,
        }
    }

    unsafe fn assume_init(self) -> MockSampleMut<'a, T> {
        MockSampleMut {
            data: unsafe { self.data.assume_init() },
            lifetime: PhantomData,
        }
    }
}

impl<'a, T> AsMut<core::mem::MaybeUninit<T>> for MockSampleMaybeUninit<'a, T>
where
    T: CommData + Debug,
{
    fn as_mut(&mut self) -> &mut core::mem::MaybeUninit<T> {
        &mut self.data
    }
}

#[derive(Debug)]
pub struct MockSubscribableImpl<T> {
    identifier: &'static str,
    instance_info: MockConsumerInfo,
    data: PhantomData<T>,
}

impl<T: CommData + Debug> Subscriber<T, MockRuntimeImpl> for MockSubscribableImpl<T> {
    type Subscription = MockSubscriberImpl<T>;
    fn new(identifier: &'static str, instance_info: MockConsumerInfo) -> Result<Self> {
        Ok(Self {
            identifier,
            instance_info,
            data: PhantomData,
        })
    }
    fn subscribe(self, _max_num_samples: usize) -> Result<Self::Subscription> {
        Ok(MockSubscriberImpl {
            identifier: self.identifier,
            instance_info: self.instance_info.clone(),
            data: VecDeque::new(),
        })
    }
}

#[derive(Debug)]
pub struct MockSubscriberImpl<T>
where
    T: CommData + Debug,
{
    identifier: &'static str,
    instance_info: MockConsumerInfo,
    data: VecDeque<T>,
}

impl<T> MockSubscriberImpl<T>
where
    T: CommData + Debug,
{
    pub fn add_data(&mut self, data: T) {
        self.data.push_front(data);
    }
}

impl<T> Subscription<T, MockRuntimeImpl> for MockSubscriberImpl<T>
where
    T: CommData + Debug,
{
    type Subscriber = MockSubscribableImpl<T>;
    type Sample<'a> = MockSample<'a, T>;

    fn unsubscribe(self) -> Self::Subscriber {
        MockSubscribableImpl {
            identifier: self.identifier,
            instance_info: self.instance_info,
            data: PhantomData,
        }
    }

    fn try_receive<'a>(
        &'a self,
        _scratch: &'_ mut SampleContainer<Self::Sample<'a>>,
        _max_samples: usize,
    ) -> Result<usize> {
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

pub struct MockPublisher<T> {
    _data: PhantomData<T>,
}

impl<T> Default for MockPublisher<T>
where
    T: CommData + Debug,
{
    fn default() -> Self {
        Self::new()
    }
}

impl<T> MockPublisher<T>
where
    T: CommData + Debug,
{
    #[must_use = "creating a Publisher without using it is likely a mistake; the publisher must be \
                  assigned or used in some way"]
    pub fn new() -> Self {
        Self { _data: PhantomData }
    }
}

impl<T> Publisher<T, MockRuntimeImpl> for MockPublisher<T>
where
    T: CommData + Debug,
{
    type SampleMaybeUninit<'a>
        = MockSampleMaybeUninit<'a, T>
    where
        Self: 'a;

    fn allocate(&self) -> Result<Self::SampleMaybeUninit<'_>> {
        Ok(MockSampleMaybeUninit {
            data: MaybeUninit::uninit(),
            lifetime: PhantomData,
        })
    }

    fn new(_identifier: &str, _instance_info: MockProviderInfo) -> Result<Self> {
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

    fn get_available_instances(&self) -> Result<Self::ServiceEnumerator> {
        Ok(Vec::new())
    }

    #[allow(clippy::manual_async_fn)]
    fn get_available_instances_async(
        &self,
    ) -> impl Future<Output = Result<Self::ServiceEnumerator>> + Send {
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
    fn build(self) -> Result<I::Consumer<MockRuntimeImpl>> {
        let instance_info = MockConsumerInfo {
            instance_specifier: self.instance_specifier.clone(),
        };

        Ok(Consumer::new(instance_info))
    }
}

pub struct RuntimeBuilderImpl {}

impl Builder<MockRuntimeImpl> for RuntimeBuilderImpl {
    fn build(self) -> Result<MockRuntimeImpl> {
        Ok(MockRuntimeImpl {})
    }
}

/// Entry point for the default implementation for the com module of s-core
impl RuntimeBuilder<MockRuntimeImpl> for RuntimeBuilderImpl {
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
    use concept::{Publisher, SampleContainer, SampleMaybeUninit, SampleMut, Subscription};

    #[test]
    fn receive_stuff() {
        let test_subscriber = super::MockSubscriberImpl::<u32>::new();
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
        let test_subscriber = super::MockSubscriberImpl::<u32>::new();
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
        let test_publisher = super::MockPublisher::<u32>::new();
        let sample = test_publisher.allocate().expect("Couldn't allocate sample");
        let sample = sample.write(42);
        sample.send().expect("Send failed for sample");
    }
}
