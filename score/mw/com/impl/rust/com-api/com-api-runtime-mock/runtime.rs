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
//!
//! # Event routing
//!
//! Events are routed through a process-local registry keyed by
//! `(instance specifier, event name)`:
//!
//! - A producer-side [`MockPublisher`] pushes sent samples into the per-event queue
//!   (both via `Publisher::send()` and via the zero-copy
//!   `allocate()` / `write()` / `SampleMut::send()` path). No real IPC takes place.
//! - A consumer-side [`MockSubscriberImpl`] for the same instance and event name drains
//!   that queue via `try_receive` / `receive` / `cancellable_receive`.
//! - [`MockSubscriberImpl::add_data`] remains available for tests that want to inject
//!   samples directly without creating a publisher.
//!
//! # Mock limitations (by design, documented for test authors)
//!
//! - `CommData` does not provide `Clone`, so one value cannot be fanned out to several
//!   receivers. Events are therefore delivered through a single shared per-event queue:
//!   concurrent subscriptions of the same event steal from each other.
//! - Delivery is FIFO. The real LoLa binding delivers the newest samples first and may
//!   skip older ones (see the `Subscription` docs); tests relying on that ordering should
//!   not use the mock's ordering as a reference.
//! - Once the subscription capacity is reached, the oldest queued sample is dropped
//!   (ring semantics).
//! - `offer_service` / `stop_offer_service` are no-ops: the mock has no service
//!   discovery, consistent with `find_service` returning an empty instance list.

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
use core::task::Poll;
use futures::stream::{self, Stream};
use std::any::Any;
use std::collections::{HashMap, VecDeque};
use std::path::Path;
use std::sync::{Arc, Mutex, OnceLock};

use score_com_concept::{
    Builder, CommData, Consumer, ConsumerBuilder, ConsumerDescriptor, Error, EventFailedReason, FindServiceSpecifier,
    InstanceSpecifier, Interface, Producer, ProducerBuilder, ProviderInfo, Publisher, ReceiveFailedReason, Result,
    Runtime, RuntimeBuilder, Sample, SampleContainer, SampleMaybeUninit, SampleMut, ServiceDiscovery, Subscriber,
    Subscription,
};

pub struct MockRuntimeImpl {}

#[derive(Clone, Debug)]
pub struct MockProviderInfo {
    instance_specifier: InstanceSpecifier,
}

impl ProviderInfo for MockProviderInfo {
    /// No-op in the mock: there is no service discovery to announce to
    /// (`find_service` always returns an empty instance list).
    fn offer_service(&self) -> Result<()> {
        Ok(())
    }

    /// No-op in the mock, see [`MockProviderInfo::offer_service`].
    fn stop_offer_service(&self) -> Result<()> {
        Ok(())
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

    fn producer_builder<I: Interface>(&self, instance_specifier: InstanceSpecifier) -> Self::ProducerBuilder<I> {
        MockProducerBuilder::new(self, instance_specifier)
    }
}

// ---------------------------------------------------------------------------
// Process-local registry connecting mock publishers and subscriptions.
// ---------------------------------------------------------------------------

/// Registry key: `(instance specifier, event name)`.
type RegistryKey = (String, String);

/// Per-event mock state: pending samples and the subscription capacity.
struct EventState<T> {
    queue: VecDeque<T>,
    capacity: usize,
}

impl<T> EventState<T> {
    fn new() -> Self {
        Self {
            queue: VecDeque::new(),
            capacity: usize::MAX,
        }
    }
}

#[derive(Default)]
struct MockRegistry {
    events: Mutex<HashMap<RegistryKey, Arc<dyn Any + Send + Sync>>>,
}

fn registry() -> &'static MockRegistry {
    static REGISTRY: OnceLock<MockRegistry> = OnceLock::new();
    REGISTRY.get_or_init(MockRegistry::default)
}

fn make_key(instance: &InstanceSpecifier, endpoint: &str) -> RegistryKey {
    (instance.as_ref().to_string(), endpoint.to_string())
}

/// Run `f` with exclusive access to the event state stored under `key`,
/// creating the state on first use.
fn with_event_state<T, Rt>(key: &RegistryKey, f: impl FnOnce(&mut EventState<T>) -> Rt) -> Result<Rt>
where
    T: CommData + Debug,
{
    let mut map = registry().events.lock().expect("mock event registry poisoned");
    let entry = map
        .entry(key.clone())
        .or_insert_with(|| Arc::new(Mutex::new(EventState::<T>::new())));
    let state = entry
        .downcast_ref::<Mutex<EventState<T>>>()
        .ok_or(Error::EventError(EventFailedReason::EventCreationFailed))?;
    let mut guard = state.lock().expect("mock event state poisoned");
    Ok(f(&mut guard))
}

/// Push an event sample into the per-event queue (ring semantics: the oldest sample is
/// dropped once the subscription capacity is reached).
fn push_event_value<T>(key: &RegistryKey, value: T) -> Result<()>
where
    T: CommData + Debug,
{
    with_event_state::<T, _>(key, |state| {
        if state.queue.len() >= state.capacity {
            state.queue.pop_front();
        }
        state.queue.push_back(value);
    })
}

// ---------------------------------------------------------------------------
// Sample types.
// ---------------------------------------------------------------------------

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
    key: RegistryKey,
    lifetime: PhantomData<&'a T>,
}

impl<'a, T> SampleMut<T> for MockSampleMut<'a, T>
where
    T: CommData + Debug,
{
    /// Delivers the written sample to the per-event queue of this publisher's event,
    /// where subscriptions of the same instance and event name can receive it.
    fn send(self) -> Result<()> {
        push_event_value(&self.key, self.data)
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
    key: RegistryKey,
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
            key: self.key,
            lifetime: PhantomData,
        }
    }

    unsafe fn assume_init(self) -> MockSampleMut<'a, T> {
        MockSampleMut {
            data: unsafe { self.data.assume_init() },
            key: self.key,
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

// ---------------------------------------------------------------------------
// Subscriber / Subscription.
// ---------------------------------------------------------------------------

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
    fn subscribe(self, max_num_samples: usize) -> Result<Self::Subscription> {
        if max_num_samples == 0 {
            return Err(Error::EventError(EventFailedReason::InvalidMaxSamples));
        }
        let subscription = MockSubscriberImpl {
            identifier: self.identifier,
            instance_info: self.instance_info.clone(),
            data: PhantomData,
        };
        subscription.with_state(|state| state.capacity = max_num_samples)?;
        Ok(subscription)
    }
}

#[derive(Debug)]
pub struct MockSubscriberImpl<T>
where
    T: CommData + Debug,
{
    identifier: &'static str,
    instance_info: MockConsumerInfo,
    data: PhantomData<T>,
}

impl<T> MockSubscriberImpl<T>
where
    T: CommData + Debug,
{
    /// Test hook: inject a sample directly into this subscription's event queue, without
    /// going through a publisher.
    pub fn add_data(&mut self, data: T) {
        // The queue exists as soon as the subscription touched it in subscribe().
        let _ = push_event_value(&self.key(), data);
    }

    fn key(&self) -> RegistryKey {
        make_key(&self.instance_info.instance_specifier, self.identifier)
    }

    fn with_state<Rt>(&self, f: impl FnOnce(&mut EventState<T>) -> Rt) -> Result<Rt> {
        with_event_state::<T, Rt>(&self.key(), f)
    }

    /// Move up to `max_samples` pending event samples from the per-event queue into `scratch`.
    ///
    /// Note: if `scratch` runs out of capacity mid-drain, the sample that did not fit is
    /// dropped and the error is returned - size the container for `max_samples` in tests.
    fn drain_into<'x>(&self, scratch: &mut SampleContainer<MockSample<'x, T>>, max_samples: usize) -> Result<usize> {
        let mut delivered = 0;
        while delivered < max_samples {
            let value = match self.with_state(|state| state.queue.pop_front())? {
                Some(value) => value,
                None => break,
            };
            scratch.push_back(MockSample::from(value))?;
            delivered += 1;
        }
        Ok(delivered)
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

    /// Drains up to `max_samples` pending samples in FIFO order (the mock has no
    /// timestamp-based newest-first selection, unlike the LoLa binding).
    fn try_receive<'a>(
        &'a self,
        scratch: &'_ mut SampleContainer<Self::Sample<'a>>,
        max_samples: usize,
    ) -> Result<usize> {
        self.drain_into(scratch, max_samples)
    }

    /// Waits (by re-polling; the mock has no wakeup infrastructure) until at least
    /// `max(new_samples, 1)` samples are pending, then drains up to `max_samples` of them.
    /// Resolves with `ReceiveFailedReason::Cancelled` once `cancellation` completes.
    #[allow(clippy::manual_async_fn)]
    fn cancellable_receive<'a>(
        &'a self,
        scratch: SampleContainer<Self::Sample<'a>>,
        new_samples: usize,
        max_samples: usize,
        cancellation: impl Future<Output = ()> + Send + 'static,
    ) -> impl Future<Output = (SampleContainer<Self::Sample<'a>>, Result<usize>)> + 'a {
        async move {
            let mut scratch = scratch;
            let mut cancellation = Box::pin(cancellation);
            let required = new_samples.max(1);
            let result = futures::future::poll_fn(|cx| {
                if cancellation.as_mut().poll(cx).is_ready() {
                    return Poll::Ready(Err(Error::ReceiveError(ReceiveFailedReason::Cancelled)));
                }
                let available = match self.with_state(|state| state.queue.len()) {
                    Ok(available) => available,
                    Err(e) => return Poll::Ready(Err(e)),
                };
                if available >= required {
                    return Poll::Ready(self.drain_into(&mut scratch, max_samples));
                }
                // No wakeup infrastructure in the mock: request an immediate re-poll.
                cx.waker().wake_by_ref();
                Poll::Pending
            })
            .await;
            (scratch, result)
        }
    }

    #[allow(clippy::manual_async_fn)]
    fn to_stream<'a>(&'a mut self) -> impl Stream<Item = Result<Self::Sample<'a>>> + Unpin + 'a {
        stream::empty()
    }
}

// ---------------------------------------------------------------------------
// Publisher.
// ---------------------------------------------------------------------------

pub struct MockPublisher<T> {
    key: RegistryKey,
    _data: PhantomData<T>,
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
            key: self.key.clone(),
            lifetime: PhantomData,
        })
    }

    fn new(identifier: &str, instance_info: MockProviderInfo) -> Result<Self> {
        let key = make_key(&instance_info.instance_specifier, identifier);
        // Create the event state eagerly so samples published before any subscription are kept.
        with_event_state::<T, _>(&key, |_| ())?;
        Ok(Self {
            key,
            _data: PhantomData,
        })
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
    fn get_available_instances_async(&self) -> impl Future<Output = Result<Self::ServiceEnumerator>> + Send {
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
    fn get_instance_specifier(&self) -> &InstanceSpecifier {
        // For the mock runtime, simply return the stored instance specifier.
        &self.instance_specifier
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
    use super::*;
    use futures::executor::block_on;

    /// Simple relocatable test payload.
    ///
    /// `CommData` is implemented manually here (instead of via the derive macros) to keep
    /// this crate's test dependencies limited to `score_com_concept` + `futures`.
    #[derive(Debug, PartialEq)]
    struct Pressure {
        kpa: u32,
    }

    unsafe impl score_com_concept::Reloc for Pressure {}

    impl CommData for Pressure {
        const ID: &'static str = "com_api_runtime_mock::test::Pressure";
    }

    fn provider_info(path: &str) -> MockProviderInfo {
        MockProviderInfo {
            instance_specifier: InstanceSpecifier::new(path).expect("valid instance specifier"),
        }
    }

    fn consumer_info(path: &str) -> MockConsumerInfo {
        MockConsumerInfo {
            instance_specifier: InstanceSpecifier::new(path).expect("valid instance specifier"),
        }
    }

    fn make_publisher(identifier: &str, path: &str) -> MockPublisher<Pressure> {
        <MockPublisher<Pressure> as Publisher<Pressure, MockRuntimeImpl>>::new(identifier, provider_info(path))
            .expect("publisher creation")
    }

    fn make_subscription(identifier: &'static str, path: &str, max_samples: usize) -> MockSubscriberImpl<Pressure> {
        <MockSubscribableImpl<Pressure> as Subscriber<Pressure, MockRuntimeImpl>>::new(identifier, consumer_info(path))
            .expect("subscriber creation")
            .subscribe(max_samples)
            .expect("subscribe")
    }

    #[test]
    fn offer_service_is_accepted() {
        let info = provider_info("/mock_test/event_offer");
        assert!(info.offer_service().is_ok());
        assert!(info.stop_offer_service().is_ok());
    }

    #[test]
    fn published_sample_reaches_subscription() {
        const PATH: &str = "/mock_test/event_send";

        let publisher = make_publisher("tire_pressure", PATH);
        let subscription = make_subscription("tire_pressure", PATH, 4);

        // `Publisher::send` is the trait-provided allocate + write + send sequence.
        publisher.send(Pressure { kpa: 220 }).expect("send");

        let mut scratch = SampleContainer::new(4);
        let received = subscription.try_receive(&mut scratch, 4).expect("receive");
        assert_eq!(received, 1);
        assert_eq!(scratch.front::<Pressure>().expect("sample").kpa, 220);
    }

    #[test]
    fn zero_copy_allocate_write_send_reaches_subscription() {
        const PATH: &str = "/mock_test/event_zero_copy";

        let publisher = make_publisher("oil_pressure", PATH);
        let subscription = make_subscription("oil_pressure", PATH, 2);

        let sample = publisher.allocate().expect("allocate").write(Pressure { kpa: 300 });
        sample.send().expect("send");

        let mut scratch = SampleContainer::new(2);
        let received = subscription.try_receive(&mut scratch, 2).expect("receive");
        assert_eq!(received, 1);
        assert_eq!(scratch.front::<Pressure>().expect("sample").kpa, 300);
    }

    #[test]
    fn subscribe_with_zero_max_samples_fails() {
        const PATH: &str = "/mock_test/event_zero_max";

        let subscriber = <MockSubscribableImpl<Pressure> as Subscriber<Pressure, MockRuntimeImpl>>::new(
            "fuel_pressure",
            consumer_info(PATH),
        )
        .expect("subscriber creation");
        let result = subscriber.subscribe(0);
        assert!(matches!(
            result,
            Err(Error::EventError(EventFailedReason::InvalidMaxSamples))
        ));
    }

    #[test]
    fn queue_capacity_drops_oldest_sample() {
        const PATH: &str = "/mock_test/event_capacity";

        let publisher = make_publisher("brake_pressure", PATH);
        let subscription = make_subscription("brake_pressure", PATH, 2);

        for kpa in [1, 2, 3] {
            publisher.send(Pressure { kpa }).expect("send");
        }

        let mut scratch = SampleContainer::new(2);
        let received = subscription.try_receive(&mut scratch, 2).expect("receive");
        assert_eq!(received, 2);
        // Capacity 2 with ring semantics: the oldest sample (kpa=1) was dropped.
        assert_eq!(scratch.front::<Pressure>().expect("sample").kpa, 2);
    }

    #[test]
    fn add_data_test_hook_injects_samples() {
        const PATH: &str = "/mock_test/event_add_data";

        let mut subscription = make_subscription("coolant_pressure", PATH, 4);
        subscription.add_data(Pressure { kpa: 42 });

        let mut scratch = SampleContainer::new(4);
        let received = subscription.try_receive(&mut scratch, 4).expect("receive");
        assert_eq!(received, 1);
        assert_eq!(scratch.front::<Pressure>().expect("sample").kpa, 42);
    }

    #[test]
    fn receive_resolves_once_samples_arrive() {
        const PATH: &str = "/mock_test/event_receive";

        let publisher = make_publisher("intake_pressure", PATH);
        let subscription = make_subscription("intake_pressure", PATH, 2);

        publisher.send(Pressure { kpa: 150 }).expect("send");

        // `Subscription::receive` is the trait-provided wrapper around cancellable_receive
        // with a never-completing cancellation future.
        let (scratch, result) = block_on(subscription.receive(SampleContainer::new(2), 1, 2));
        assert_eq!(result.expect("receive"), 1);
        assert_eq!(scratch.front::<Pressure>().expect("sample").kpa, 150);
    }

    #[test]
    fn cancellable_receive_resolves_as_cancelled_without_data() {
        const PATH: &str = "/mock_test/event_cancel";

        let subscription = make_subscription("exhaust_pressure", PATH, 2);

        let (_scratch, result) = block_on(subscription.cancellable_receive(
            SampleContainer::new(2),
            1,
            2,
            futures::future::ready(()),
        ));
        assert!(matches!(
            result,
            Err(Error::ReceiveError(ReceiveFailedReason::Cancelled))
        ));
    }
}
