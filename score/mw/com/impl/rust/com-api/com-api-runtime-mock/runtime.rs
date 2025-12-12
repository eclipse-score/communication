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

use core::cmp::Ordering;
use core::fmt::Debug;
use core::future::Future;
use core::marker::PhantomData;
use core::mem::MaybeUninit;
use core::ops::{Deref, DerefMut};
use core::sync::atomic::AtomicUsize;
use std::collections::VecDeque;
use std::path::Path;

use com_api_concept::{
    Builder, Consumer, ConsumerBuilder, ConsumerDescriptor, InstanceSpecifier, Interface, FindServiceSpecifier, Reloc, Runtime,
    SampleContainer, ServiceDiscovery, Subscriber, Subscription, Producer, ProducerBuilder, Result,
};

pub struct MockRuntimeImpl {}

#[derive(Clone, Debug)]
pub struct MockProviderInfo {
    instance_specifier: InstanceSpecifier,
}

#[derive(Clone, Debug)]
pub struct MockConsumerInfo {
    instance_specifier: InstanceSpecifier,
}

impl Runtime for MockRuntimeImpl {
    type ServiceDiscovery<I: Interface> = SampleConsumerDiscovery<I>;
    type Subscriber<T: Reloc + Send + Debug> = SubscribableImpl<T>;
    type ProducerBuilder<I: Interface, P: Producer<Self, Interface = I>> = SampleProducerBuilder<I>;
    type Publisher<T: Reloc + Send + Debug> = Publisher<T>;
    type ProviderInfo = MockProviderInfo;
    type ConsumerInfo = MockConsumerInfo;

    fn find_service<I: Interface>(
        &self,
        _instance_specifier: FindServiceSpecifier,
    ) -> Self::ServiceDiscovery<I> {
        SampleConsumerDiscovery {
            _interface: PhantomData,
        }
    }

    fn producer_builder<I: Interface, P: Producer<Self, Interface = I>>(
        &self,
        instance_specifier: InstanceSpecifier,
    ) -> Self::ProducerBuilder<I, P> {
        SampleProducerBuilder::new(self, instance_specifier)
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

unsafe impl<T> Send for MockBinding<'_, T> where T: Send {}

#[derive(Debug)]
enum SampleBinding<'a, T>
where
    T: Send,
{
    Mock(MockBinding<'a, T>),
    Test(Box<T>),
}

#[derive(Debug)]
pub struct Sample<'a, T>
where
    T: Reloc + Send + Debug,
{
    id: usize,
    inner: SampleBinding<'a, T>,
}

static ID_COUNTER: AtomicUsize = AtomicUsize::new(0);

impl<T> From<T> for Sample<'_, T>
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

impl<T> Deref for Sample<'_, T>
where
    T: Reloc + Send + Debug,
{
    type Target = T;

    fn deref(&self) -> &Self::Target {
        match &self.inner {
            SampleBinding::Mock(_mock) => unimplemented!(),
            SampleBinding::Test(test) => test.as_ref(),
        }
    }
}

impl<T> com_api_concept::Sample<T> for Sample<'_, T> where T: Send + Reloc + Debug {}

impl<T> PartialEq for Sample<'_, T>
where
    T: Send + Reloc + Debug,
{
    fn eq(&self, other: &Self) -> bool {
        self.id == other.id
    }
}

impl<T> Eq for Sample<'_, T> where T: Send + Reloc + Debug {}

impl<T> PartialOrd for Sample<'_, T>
where
    T: Send + Reloc + Debug,
{
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl<T> Ord for Sample<'_, T>
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
    type Sample = Sample<'a, T>;

    fn into_sample(self) -> Self::Sample {
        todo!()
    }

    fn send(self) -> com_api_concept::Result<()> {
        todo!()
    }
}

impl<T> Deref for SampleMut<'_, T>
where
    T: Reloc,
{
    type Target = T;

    fn deref(&self) -> &Self::Target {
        &self.data
    }
}

impl<T> DerefMut for SampleMut<'_, T>
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

impl<T> AsMut<core::mem::MaybeUninit<T>> for SampleMaybeUninit<'_, T>
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
    instance_info: Option<MockConsumerInfo>,
    data: PhantomData<T>,
}

impl<T> Default for SubscribableImpl<T> {
    fn default() -> Self {
        Self {
            identifier: String::new(),
            instance_info: None,
            data: PhantomData,
        }
    }
}

impl<T: Reloc + Send + Debug> Subscriber<T, MockRuntimeImpl> for SubscribableImpl<T> {
    type Subscription = SubscriberImpl<T>;
    fn new(identifier: &str, instance_info: MockConsumerInfo) -> com_api_concept::Result<Self> {
        Ok(Self {
            identifier: identifier.to_string(),
            instance_info: Some(instance_info),
            data: PhantomData,
        })
    }
    fn subscribe(&self, _max_num_samples: usize) -> com_api_concept::Result<Self::Subscription> {
        Ok(SubscriberImpl::new())
    }
}

#[derive(Default,Debug)]
pub struct SubscriberImpl<T>
where
    T: Reloc + Send + Debug,
{
    data: VecDeque<T>,
}

impl<T> SubscriberImpl<T>
where
    T: Reloc + Send + Debug,
{
    pub fn new() -> Self {
        Self {
            data: Default::default(),
        }
    }

    pub fn add_data(&mut self, data: T) {
        self.data.push_front(data);
    }
}

impl<T> Subscription<T, MockRuntimeImpl> for SubscriberImpl<T>
where
    T: Reloc + Send + Debug,
{
    type Subscriber = SubscribableImpl<T>;
    type Sample<'a>
        = Sample<'a, T>
    where
        T: 'a;

    fn unsubscribe(self) -> Self::Subscriber {
        Default::default()
    }

    fn try_receive<'a>(
        &'a self,
        _scratch: &'_ mut SampleContainer<Self::Sample<'a>>,
        _max_samples: usize,
    ) -> com_api_concept::Result<usize> {
        todo!()
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
    pub fn new() -> Self {
        Self { _data: PhantomData }
    }
}

impl<T> com_api_concept::Publisher<T, MockRuntimeImpl> for Publisher<T>
where
    T: Reloc + Send + Debug,
{
    type SampleMaybeUninit<'a> = SampleMaybeUninit<'a, T> where Self: 'a;

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

pub struct SampleConsumerDiscovery<I> {
    _interface: PhantomData<I>,
}

impl<I> SampleConsumerDiscovery<I> {
    fn new(_runtime: &MockRuntimeImpl, _instance_specifier: InstanceSpecifier) -> Self {
        Self {
            _interface: PhantomData,
        }
    }
}

impl<I: Interface> ServiceDiscovery<I, MockRuntimeImpl> for SampleConsumerDiscovery<I>
where
    SampleConsumerBuilder<I>: ConsumerBuilder<I, MockRuntimeImpl>,
{
    type ConsumerBuilder = SampleConsumerBuilder<I>;
    type ServiceEnumerator = Vec<SampleConsumerBuilder<I>>;

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

pub struct SampleProducerBuilder<I: Interface> {
    instance_specifier: InstanceSpecifier,
    _interface: PhantomData<I>,
}

impl<I: Interface> SampleProducerBuilder<I> {
    fn new(_runtime: &MockRuntimeImpl, instance_specifier: InstanceSpecifier) -> Self {
        Self {
            instance_specifier,
            _interface: PhantomData,
        }
    }
}

impl<I: Interface, P: Producer<MockRuntimeImpl, Interface = I>> ProducerBuilder<I, P, MockRuntimeImpl> for SampleProducerBuilder<I> {}

impl<I: Interface, P: Producer<MockRuntimeImpl, Interface = I>> Builder<P> for SampleProducerBuilder<I> {
    fn build(self) -> Result<P> {
        todo!()
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
    instance_specifier: InstanceSpecifier,
    _interface: PhantomData<I>,
}

impl<I: Interface> ConsumerDescriptor<MockRuntimeImpl> for SampleConsumerBuilder<I> {
    fn get_instance_identifier(&self) -> &InstanceSpecifier {
        todo!()
    }
}

impl<I: Interface> ConsumerBuilder<I, MockRuntimeImpl> for SampleConsumerBuilder<I> {}

impl<I: Interface> Builder<I::Consumer<MockRuntimeImpl>> for SampleConsumerBuilder<I> {
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
