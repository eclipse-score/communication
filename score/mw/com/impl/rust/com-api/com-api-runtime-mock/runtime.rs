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

//! In-process COM runtime for backend-free application tests.
//!
//! Each built runtime owns an isolated registry; cloning it intentionally shares that
//! registry. Services are keyed by instance and interface, events also by identifier
//! and concrete data type. Each subscription has a bounded FIFO and asynchronous wakeup.
//! Samples sent before subscription are not retained. Dropping/unsubscribing clears
//! only that subscription; dropping the last provider handle stops its offer.
//!
//! `CommData` guarantees Send, but neither Clone nor Sync. Therefore a single subscriber
//! can receive any CommData by ownership transfer. For multicast, tests must explicitly
//! call [`MockRuntimeImpl::register_cloneable_data`] for their Clone data types. Multiple
//! recipients without a registered copier return Error::EventError(score_com_concept::EventFailedReason::EventPublishFailed) before delivery.
//! No byte copying or added bound on the native Runtime trait is used.
//!
//! ```
//! use com_api_runtime_mock::RuntimeBuilderImpl;
//! use score_com_concept::Builder;
//! let runtime = RuntimeBuilderImpl::new().build().unwrap();
//! let shared = runtime.clone();
//! ```

use core::cmp::Ordering;
use core::fmt::Debug;
use core::future::Future;
use core::marker::PhantomData;
use core::mem::MaybeUninit;
use core::ops::{Deref, DerefMut};
use core::task::{Poll, Waker};
use std::any::{Any, TypeId};
use std::collections::{BTreeMap, BTreeSet, HashMap, VecDeque};
use std::path::Path;
use std::sync::atomic::{AtomicUsize, Ordering as AtomicOrdering};
use std::sync::{Arc, Mutex, Weak};

use futures::stream::{self, Stream};
use score_com_concept::{
    Builder, CommData, Consumer, ConsumerBuilder, ConsumerDescriptor, Error, FindServiceSpecifier, InstanceSpecifier,
    Interface, Producer, ProducerBuilder, ProviderInfo, Publisher, ReceiveFailedReason, Result, Runtime,
    RuntimeBuilder, Sample, SampleContainer, SampleMaybeUninit, SampleMut, ServiceDiscovery, Subscriber, Subscription,
};

type ServiceKey = (String, &'static str);
type EventKey = (ServiceKey, String, TypeId);
type Payload = Box<dyn Any + Send>;
type Copier = fn(&dyn Any) -> Payload;
type Registry = Arc<Mutex<MockRegistry>>;

struct QueuedSample {
    id: usize,
    data: Payload,
}
struct SubscriptionState {
    id: usize,
    capacity: usize,
    queue: VecDeque<QueuedSample>,
    waker: Option<Waker>,
    receiving: bool,
}

#[derive(Default)]
struct MockRegistry {
    offered: BTreeMap<ServiceKey, BTreeSet<usize>>,
    events: HashMap<EventKey, Vec<Weak<Mutex<SubscriptionState>>>>,
    copiers: HashMap<TypeId, Copier>,
}

static ID_COUNTER: AtomicUsize = AtomicUsize::new(1);
fn next_id() -> usize {
    ID_COUNTER.fetch_add(1, AtomicOrdering::Relaxed)
}
fn event_key(info: &MockConsumerInfo, identifier: &str, data_type: TypeId) -> EventKey {
    (
        (info.instance_specifier.as_ref().to_string(), info.interface),
        identifier.to_string(),
        data_type,
    )
}

/// Independently built runtimes are isolated. Clones share one testing environment.
#[derive(Clone, Default)]
pub struct MockRuntimeImpl {
    registry: Registry,
}
impl MockRuntimeImpl {
    /// Enables multicast without requiring Clone or Sync in the production COM traits.
    /// The test's Clone implementation runs outside all registry/queue locks.
    pub fn register_cloneable_data<T: CommData + Clone>(&self) {
        self.registry
            .lock()
            .expect("mock registry poisoned")
            .copiers
            .insert(TypeId::of::<T>(), |value| {
                Box::new(value.downcast_ref::<T>().expect("registered concrete type").clone())
            });
    }
}

struct ProviderRegistration {
    registry: Registry,
    key: ServiceKey,
    id: usize,
}
impl ProviderRegistration {
    fn stop(&self) {
        let mut registry = self.registry.lock().expect("mock registry poisoned");
        if let Some(owners) = registry.offered.get_mut(&self.key) {
            owners.remove(&self.id);
            if owners.is_empty() {
                registry.offered.remove(&self.key);
            }
        }
    }
}
impl Drop for ProviderRegistration {
    fn drop(&mut self) {
        self.stop();
    }
}

#[derive(Clone)]
pub struct MockProviderInfo {
    registration: Arc<ProviderRegistration>,
}
impl Debug for MockProviderInfo {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("MockProviderInfo")
            .field("service", &self.registration.key)
            .finish()
    }
}
impl ProviderInfo for MockProviderInfo {
    fn offer_service(&self) -> Result<()> {
        let info = &self.registration;
        info.registry
            .lock()
            .expect("mock registry poisoned")
            .offered
            .entry(info.key.clone())
            .or_default()
            .insert(info.id);
        Ok(())
    }
    fn stop_offer_service(&self) -> Result<()> {
        self.registration.stop();
        Ok(())
    }
}

#[derive(Clone)]
pub struct MockConsumerInfo {
    instance_specifier: InstanceSpecifier,
    interface: &'static str,
    registry: Registry,
}
impl Debug for MockConsumerInfo {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("MockConsumerInfo")
            .field("instance", &self.instance_specifier)
            .field("interface", &self.interface)
            .finish()
    }
}

impl Runtime for MockRuntimeImpl {
    type ServiceDiscovery<I: Interface + Send> = MockConsumerDiscovery<I>;
    type Subscriber<T: CommData + Debug> = MockSubscribableImpl<T>;
    type ProducerBuilder<I: Interface> = MockProducerBuilder<I>;
    type Publisher<T: CommData + Debug> = MockPublisher<T>;
    type ProviderInfo = MockProviderInfo;
    type ConsumerInfo = MockConsumerInfo;
    fn find_service<I: Interface + Send>(&self, specifier: FindServiceSpecifier) -> Self::ServiceDiscovery<I> {
        MockConsumerDiscovery {
            specifier,
            registry: self.registry.clone(),
            interface: PhantomData,
        }
    }
    fn producer_builder<I: Interface>(&self, instance_specifier: InstanceSpecifier) -> Self::ProducerBuilder<I> {
        MockProducerBuilder {
            instance_specifier,
            registry: self.registry.clone(),
            interface: PhantomData,
        }
    }
}

/// A received sample owns its value, including Send values that are neither Clone nor Sync.
#[derive(Debug)]
pub struct MockSample<'a, T: CommData + Debug> {
    id: usize,
    subscription_id: usize,
    inner: Box<T>,
    lifetime: PhantomData<&'a ()>,
}
impl<T: CommData + Debug> From<T> for MockSample<'_, T> {
    fn from(value: T) -> Self {
        Self {
            id: next_id(),
            subscription_id: 0,
            inner: Box::new(value),
            lifetime: PhantomData,
        }
    }
}
impl<T: CommData + Debug> Deref for MockSample<'_, T> {
    type Target = T;
    fn deref(&self) -> &T {
        &self.inner
    }
}
impl<T: CommData + Debug> Sample<T> for MockSample<'_, T> {}
impl<T: CommData + Debug> PartialEq for MockSample<'_, T> {
    fn eq(&self, other: &Self) -> bool {
        self.id == other.id
    }
}
impl<T: CommData + Debug> Eq for MockSample<'_, T> {}
impl<T: CommData + Debug> PartialOrd for MockSample<'_, T> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}
impl<T: CommData + Debug> Ord for MockSample<'_, T> {
    fn cmp(&self, other: &Self) -> Ordering {
        self.id.cmp(&other.id)
    }
}

fn enqueue(state: &Arc<Mutex<SubscriptionState>>, mut sample: QueuedSample) {
    let (evicted, waker) = {
        let mut state = state.lock().expect("mock subscription poisoned");
        let evicted = if state.queue.len() == state.capacity {
            state.queue.pop_front()
        } else {
            None
        };
        sample.id = next_id();
        state.queue.push_back(sample);
        (evicted, state.waker.take())
    };
    // Value destructors and arbitrary executor wakeups run outside locks.
    drop(evicted);
    if let Some(waker) = waker {
        waker.wake();
    }
}
struct WakeRegistration<'a> {
    state: &'a Mutex<SubscriptionState>,
}
impl<'a> WakeRegistration<'a> {
    fn new(state: &'a Mutex<SubscriptionState>) -> Self {
        let mut guard = state.lock().expect("mock subscription poisoned");
        if guard.receiving {
            drop(guard);
            panic!("concurrent receives on one subscription are unsupported");
        }
        guard.receiving = true;
        Self { state }
    }
}
impl Drop for WakeRegistration<'_> {
    fn drop(&mut self) {
        let waker = {
            let mut state = self.state.lock().expect("mock subscription poisoned");
            state.receiving = false;
            state.waker.take()
        };
        drop(waker);
    }
}

fn publish<T: CommData + Debug>(provider: &MockProviderInfo, identifier: &str, value: T) -> Result<()> {
    let info = &provider.registration;
    let key = (info.key.clone(), identifier.to_string(), TypeId::of::<T>());
    let (recipients, copier) = {
        let mut registry = info.registry.lock().expect("mock registry poisoned");
        if !registry
            .offered
            .get(&info.key)
            .is_some_and(|owners| owners.contains(&info.id))
        {
            return Err(Error::EventError(
                score_com_concept::EventFailedReason::EventPublishFailed,
            ));
        }
        let copier = registry.copiers.get(&TypeId::of::<T>()).copied();
        let subscriptions = registry.events.entry(key).or_default();
        subscriptions.retain(|state| state.strong_count() > 0);
        (
            subscriptions.iter().filter_map(Weak::upgrade).collect::<Vec<_>>(),
            copier,
        )
    };
    if recipients.len() > 1 && copier.is_none() {
        return Err(Error::EventError(
            score_com_concept::EventFailedReason::EventPublishFailed,
        ));
    }
    let id = next_id();
    // Finish all potentially panicking Clone calls before any subscriber receives a value.
    let mut values: Vec<Payload> = Vec::new();
    if let Some(copier) = copier {
        for _ in 1..recipients.len() {
            values.push(copier(&value));
        }
    }
    values.push(Box::new(value));
    for (recipient, data) in recipients.into_iter().zip(values) {
        enqueue(&recipient, QueuedSample { id, data });
    }
    Ok(())
}

#[derive(Debug)]
pub struct MockSampleMut<'a, T: CommData + Debug> {
    data: T,
    provider: MockProviderInfo,
    identifier: String,
    lifetime: PhantomData<&'a ()>,
}
impl<T: CommData + Debug> SampleMut<T> for MockSampleMut<'_, T> {
    fn send(self) -> Result<()> {
        publish(&self.provider, &self.identifier, self.data)
    }
}
impl<T: CommData + Debug> Deref for MockSampleMut<'_, T> {
    type Target = T;
    fn deref(&self) -> &T {
        &self.data
    }
}
impl<T: CommData + Debug> DerefMut for MockSampleMut<'_, T> {
    fn deref_mut(&mut self) -> &mut T {
        &mut self.data
    }
}

#[derive(Debug)]
pub struct MockSampleMaybeUninit<'a, T: CommData + Debug> {
    data: MaybeUninit<T>,
    provider: MockProviderInfo,
    identifier: String,
    lifetime: PhantomData<&'a ()>,
}
impl<'a, T: CommData + Debug> SampleMaybeUninit<T> for MockSampleMaybeUninit<'a, T> {
    type SampleMut = MockSampleMut<'a, T>;
    fn write(self, data: T) -> Self::SampleMut {
        MockSampleMut {
            data,
            provider: self.provider,
            identifier: self.identifier,
            lifetime: PhantomData,
        }
    }
    unsafe fn assume_init(self) -> Self::SampleMut {
        // SAFETY: the native trait's caller must fully initialize the MaybeUninit<T>.
        let data = unsafe { self.data.assume_init() };
        MockSampleMut {
            data,
            provider: self.provider,
            identifier: self.identifier,
            lifetime: PhantomData,
        }
    }
}
impl<T: CommData + Debug> AsMut<MaybeUninit<T>> for MockSampleMaybeUninit<'_, T> {
    fn as_mut(&mut self) -> &mut MaybeUninit<T> {
        &mut self.data
    }
}
#[derive(Debug)]
pub struct MockPublisher<T: CommData + Debug> {
    provider: MockProviderInfo,
    identifier: String,
    data: PhantomData<T>,
}
impl<T: CommData + Debug> Publisher<T, MockRuntimeImpl> for MockPublisher<T> {
    type SampleMaybeUninit<'a>
        = MockSampleMaybeUninit<'a, T>
    where
        Self: 'a;
    fn allocate(&self) -> Result<Self::SampleMaybeUninit<'_>> {
        Ok(MockSampleMaybeUninit {
            data: MaybeUninit::uninit(),
            provider: self.provider.clone(),
            identifier: self.identifier.clone(),
            lifetime: PhantomData,
        })
    }
    fn new(identifier: &str, provider: MockProviderInfo) -> Result<Self> {
        Ok(Self {
            provider,
            identifier: identifier.to_string(),
            data: PhantomData,
        })
    }
}

#[derive(Debug)]
pub struct MockSubscribableImpl<T: CommData + Debug> {
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
            return Err(Error::ReceiveError(ReceiveFailedReason::InputValueOutOfBounds {
                max: 0,
                requested: 0,
            }));
        }
        let state = Arc::new(Mutex::new(SubscriptionState {
            id: next_id(),
            capacity: max_num_samples,
            queue: VecDeque::new(),
            waker: None,
            receiving: false,
        }));
        let key = event_key(&self.instance_info, self.identifier, TypeId::of::<T>());
        self.instance_info
            .registry
            .lock()
            .expect("mock registry poisoned")
            .events
            .entry(key)
            .or_default()
            .push(Arc::downgrade(&state));
        Ok(MockSubscriberImpl {
            identifier: self.identifier,
            instance_info: self.instance_info,
            max_num_samples,
            state,
            data: PhantomData,
        })
    }
}

pub struct MockSubscriberImpl<T: CommData + Debug> {
    identifier: &'static str,
    instance_info: MockConsumerInfo,
    max_num_samples: usize,
    state: Arc<Mutex<SubscriptionState>>,
    data: PhantomData<T>,
}
impl<T: CommData + Debug> Drop for MockSubscriberImpl<T> {
    fn drop(&mut self) {
        let key = event_key(&self.instance_info, self.identifier, TypeId::of::<T>());
        let own = Arc::downgrade(&self.state);
        let mut registry = self.instance_info.registry.lock().expect("mock registry poisoned");
        if let Some(subscriptions) = registry.events.get_mut(&key) {
            subscriptions.retain(|state| state.strong_count() > 0 && !Weak::ptr_eq(state, &own));
            if subscriptions.is_empty() {
                registry.events.remove(&key);
            }
        }
    }
}
impl<T: CommData + Debug> Debug for MockSubscriberImpl<T> {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("MockSubscriberImpl")
            .field("identifier", &self.identifier)
            .finish()
    }
}
impl<T: CommData + Debug> MockSubscriberImpl<T> {
    /// Inject data into this subscription only, preserving isolation from other consumers.
    pub fn add_data(&self, data: T) {
        enqueue(
            &self.state,
            QueuedSample {
                id: next_id(),
                data: Box::new(data),
            },
        );
    }
    fn poll_sample<'a>(&'a self, cx: &mut core::task::Context<'_>) -> Poll<Result<MockSample<'a, T>>> {
        let mut state = self.state.lock().expect("mock subscription poisoned");
        if let Some(item) = state.queue.pop_front() {
            Poll::Ready(
                item.data
                    .downcast::<T>()
                    .map(|inner| MockSample {
                        id: item.id,
                        subscription_id: state.id,
                        inner,
                        lifetime: PhantomData,
                    })
                    .map_err(|_| Error::ReceiveError(ReceiveFailedReason::ReceiveError)),
            )
        } else {
            state.waker = Some(cx.waker().clone());
            Poll::Pending
        }
    }
}
impl<T: CommData + Debug> Subscription<T, MockRuntimeImpl> for MockSubscriberImpl<T> {
    type Subscriber = MockSubscribableImpl<T>;
    type Sample<'a>
        = MockSample<'a, T>
    where
        Self: 'a;
    fn unsubscribe(self) -> Self::Subscriber {
        MockSubscribableImpl {
            identifier: self.identifier,
            instance_info: self.instance_info.clone(),
            data: PhantomData,
        }
    }
    fn try_receive<'a>(&'a self, scratch: &mut SampleContainer<Self::Sample<'a>>, max_samples: usize) -> Result<usize> {
        if max_samples == 0 || max_samples > self.max_num_samples || max_samples > scratch.capacity() {
            return Err(Error::ReceiveError(ReceiveFailedReason::SampleCountOutOfBounds {
                max: self.max_num_samples.min(scratch.capacity()),
                requested: max_samples,
            }));
        }
        let mut state = self.state.lock().expect("mock subscription poisoned");
        // Inspect all existing sample owners without consuming input on an error.
        // SampleContainer exposes only data iteration, so rotate through its sample handles.
        let count = scratch.sample_count();
        let mut mixed = false;
        for _ in 0..count {
            let sample = scratch.pop_front().expect("known nonempty container");
            mixed |= sample.subscription_id != state.id;
            scratch.push_back(sample)?;
        }
        if mixed {
            return Err(Error::ReceiveError(ReceiveFailedReason::ReceiveError));
        }
        let mut removed = Vec::new();
        while scratch.sample_count() > max_samples {
            removed.push(scratch.pop_front());
        }
        let mut added = 0;
        while added < max_samples {
            let Some(item) = state.queue.pop_front() else { break };
            if scratch.sample_count() == max_samples {
                removed.push(scratch.pop_front());
            }
            let sample = MockSample {
                id: item.id,
                subscription_id: state.id,
                inner: item.data.downcast::<T>().expect("concrete event type"),
                lifetime: PhantomData,
            };
            // Capacity was validated before draining. Replacement discards the oldest
            // retained sample and keeps all newly received values in reception order.
            if let Err(error) = scratch.push_back(sample) {
                drop(state);
                drop(removed);
                return Err(error);
            }
            added += 1;
        }
        drop(state);
        drop(removed);
        Ok(added)
    }
    async fn cancellable_receive<'a>(
        &'a self,
        mut scratch: SampleContainer<Self::Sample<'a>>,
        new_samples: usize,
        max_samples: usize,
        cancellation: impl Future<Output = ()> + Send + 'static,
    ) -> (SampleContainer<Self::Sample<'a>>, Result<usize>) {
        if new_samples == 0
            || new_samples > max_samples
            || max_samples > self.max_num_samples
            || max_samples > scratch.capacity()
            || max_samples == 0
        {
            return (
                scratch,
                Err(Error::ReceiveError(ReceiveFailedReason::InputValueOutOfBounds {
                    max: max_samples.min(self.max_num_samples),
                    requested: new_samples,
                })),
            );
        }
        let _registration = WakeRegistration::new(&self.state);
        let mut cancellation = core::pin::pin!(cancellation);
        let mut added = 0;
        let result = core::future::poll_fn(|cx| {
            // Register before the receive probe: send racing with the probe either
            // leaves data in the queue or wakes this future. No lost wakeup window.
            self.state.lock().expect("mock subscription poisoned").waker = Some(cx.waker().clone());
            match self.try_receive(&mut scratch, max_samples) {
                Ok(count) => added += count,
                Err(error) => return Poll::Ready(Err(error)),
            }
            if added >= new_samples {
                return Poll::Ready(Ok(added));
            }
            if cancellation.as_mut().poll(cx).is_ready() {
                return Poll::Ready(Err(Error::ReceiveError(ReceiveFailedReason::Cancelled)));
            }
            Poll::Pending
        })
        .await;
        (scratch, result)
    }

    fn to_stream<'a>(&'a mut self) -> impl Stream<Item = Result<Self::Sample<'a>>> + Unpin + 'a {
        let subscription: &'a Self = self;
        let registration = WakeRegistration::new(&self.state);
        stream::poll_fn(move |cx| {
            let _ = &registration;
            subscription.poll_sample(cx).map(Some)
        })
    }
}

pub struct MockConsumerDiscovery<I> {
    specifier: FindServiceSpecifier,
    registry: Registry,
    interface: PhantomData<I>,
}
impl<I: Interface + Send> ServiceDiscovery<I, MockRuntimeImpl> for MockConsumerDiscovery<I> {
    type ConsumerBuilder = MockConsumerBuilder<I>;
    type ServiceEnumerator = Vec<MockConsumerBuilder<I>>;
    fn get_available_instances(&self) -> Result<Self::ServiceEnumerator> {
        let registry = self.registry.lock().expect("mock registry poisoned");
        Ok(registry
            .offered
            .keys()
            .filter(|(instance, interface)| {
                *interface == I::INTERFACE_ID
                    && match &self.specifier {
                        FindServiceSpecifier::Any => true,
                        FindServiceSpecifier::Specific(specifier) => instance == specifier.as_ref(),
                    }
            })
            .map(|(specifier, _)| MockConsumerBuilder {
                instance_specifier: InstanceSpecifier::new(specifier.clone()).expect("offered valid specifier"),
                registry: self.registry.clone(),
                interface: PhantomData,
            })
            .collect())
    }
    fn get_available_instances_async(&self) -> impl Future<Output = Result<Self::ServiceEnumerator>> + Send {
        core::future::ready(self.get_available_instances())
    }
}
pub struct MockProducerBuilder<I: Interface> {
    instance_specifier: InstanceSpecifier,
    registry: Registry,
    interface: PhantomData<I>,
}
impl<I: Interface> ProducerBuilder<I, MockRuntimeImpl> for MockProducerBuilder<I> {}
impl<I: Interface> Builder<I::Producer<MockRuntimeImpl>> for MockProducerBuilder<I> {
    fn build(self) -> Result<I::Producer<MockRuntimeImpl>> {
        I::Producer::new(MockProviderInfo {
            registration: Arc::new(ProviderRegistration {
                registry: self.registry,
                key: (self.instance_specifier.as_ref().to_string(), I::INTERFACE_ID),
                id: next_id(),
            }),
        })
    }
}
pub struct MockConsumerBuilder<I: Interface> {
    instance_specifier: InstanceSpecifier,
    registry: Registry,
    interface: PhantomData<I>,
}
impl<I: Interface> ConsumerDescriptor<MockRuntimeImpl> for MockConsumerBuilder<I> {
    fn get_instance_specifier(&self) -> &InstanceSpecifier {
        &self.instance_specifier
    }
}
impl<I: Interface> ConsumerBuilder<I, MockRuntimeImpl> for MockConsumerBuilder<I> {}
impl<I: Interface> Builder<I::Consumer<MockRuntimeImpl>> for MockConsumerBuilder<I> {
    fn build(self) -> Result<I::Consumer<MockRuntimeImpl>> {
        Ok(Consumer::new(MockConsumerInfo {
            instance_specifier: self.instance_specifier,
            interface: I::INTERFACE_ID,
            registry: self.registry,
        }))
    }
}
#[derive(Default)]
pub struct RuntimeBuilderImpl {}
impl Builder<MockRuntimeImpl> for RuntimeBuilderImpl {
    fn build(self) -> Result<MockRuntimeImpl> {
        Ok(MockRuntimeImpl::default())
    }
}
impl RuntimeBuilder<MockRuntimeImpl> for RuntimeBuilderImpl {
    fn load_config(&mut self, _config: &Path) -> &mut Self {
        self
    }
}
impl RuntimeBuilderImpl {
    pub fn new() -> Self {
        Self::default()
    }
}

#[cfg(test)]
mod tests {
    use core::marker::PhantomData;
    use futures::{FutureExt, Stream, StreamExt};
    use std::sync::{
        atomic::{AtomicUsize, Ordering},
        Arc,
    };

    use crate::{MockConsumerInfo, MockRuntimeImpl, MockSubscribableImpl, RuntimeBuilderImpl};
    use score_com_concept::{
        Builder, CommData, Consumer, FindServiceSpecifier, InstanceSpecifier, Interface, OfferedProducer, Producer,
        ProviderInfo, Publisher, Reloc, Result, Runtime, SampleContainer, ServiceDiscovery, Subscriber, Subscription,
    };

    #[derive(Clone, Debug, Reloc, CommData)]
    #[repr(C)]
    struct TestData {
        value: u32,
    }

    struct TestInterface;

    struct TestConsumer<R: Runtime + ?Sized> {
        event: R::Subscriber<TestData>,
    }

    impl Interface for TestInterface {
        const INTERFACE_ID: &'static str = "mock_test::TestInterface";
        type Consumer<R: Runtime + ?Sized> = TestConsumer<R>;
        type Producer<R: Runtime + ?Sized> = TestProducer<R>;
    }

    impl<R: Runtime + ?Sized> Consumer<R> for TestConsumer<R> {
        fn new(instance_info: R::ConsumerInfo) -> Self {
            Self {
                event: <R::Subscriber<TestData> as Subscriber<TestData, R>>::new("event", instance_info)
                    .expect("failed to create mock subscriber"),
            }
        }
    }

    struct TestProducer<R: Runtime + ?Sized> {
        instance_info: R::ProviderInfo,
        _runtime: PhantomData<R>,
    }

    impl<R: Runtime + ?Sized> Producer<R> for TestProducer<R> {
        type Interface = TestInterface;
        type OfferedProducer = TestOfferedProducer<R>;

        fn offer(self) -> Result<Self::OfferedProducer> {
            let event = <R::Publisher<TestData> as Publisher<TestData, R>>::new("event", self.instance_info.clone())
                .expect("failed to create mock publisher");
            self.instance_info.offer_service()?;
            Ok(TestOfferedProducer {
                event,
                instance_info: self.instance_info,
            })
        }

        fn new(instance_info: R::ProviderInfo) -> Result<Self> {
            Ok(Self {
                instance_info,
                _runtime: PhantomData,
            })
        }
    }

    struct TestOfferedProducer<R: Runtime + ?Sized> {
        event: R::Publisher<TestData>,
        instance_info: R::ProviderInfo,
    }

    impl<R: Runtime + ?Sized> OfferedProducer<R> for TestOfferedProducer<R> {
        type Interface = TestInterface;
        type Producer = TestProducer<R>;

        fn unoffer(self) -> Result<Self::Producer> {
            self.instance_info.stop_offer_service()?;
            Ok(TestProducer {
                instance_info: self.instance_info,
                _runtime: PhantomData,
            })
        }
    }

    #[test]
    fn offer_publish_discover_receive_roundtrip() {
        let runtime = RuntimeBuilderImpl::new().build().expect("build mock runtime");
        let specifier = InstanceSpecifier::new("/mock_test/roundtrip").expect("valid instance specifier");

        let producer = runtime
            .producer_builder::<TestInterface>(specifier.clone())
            .build()
            .expect("build producer");
        let offered = producer.offer().expect("offer service");

        let discovery = runtime.find_service::<TestInterface>(FindServiceSpecifier::Specific(specifier));
        let consumers = discovery.get_available_instances().expect("discover offered instances");
        assert_eq!(consumers.len(), 1, "exactly one offered instance expected");

        let consumer = consumers
            .into_iter()
            .next()
            .expect("consumer builder")
            .build()
            .expect("build consumer");
        let subscription = consumer.event.subscribe(4).expect("subscribe to event");

        offered.event.send(TestData { value: 42 }).expect("publish sample");
        let mut container = SampleContainer::new(4);
        let received = subscription.try_receive(&mut container, 4).expect("receive sample");
        assert_eq!(received, 1);

        let sample = container.pop_front().expect("sample available");
        assert_eq!(sample.value, 42);

        let _ = offered.unoffer().expect("unoffer service");
    }

    #[test]
    fn manually_injected_data_is_received() {
        let instance_info = MockConsumerInfo {
            instance_specifier: InstanceSpecifier::new("/mock_test/manual").expect("valid instance specifier"),
            interface: TestInterface::INTERFACE_ID,
            registry: MockRuntimeImpl::default().registry,
        };
        let subscription =
            <MockSubscribableImpl<TestData> as Subscriber<TestData, MockRuntimeImpl>>::new("event", instance_info)
                .expect("create subscriber")
                .subscribe(2)
                .expect("subscribe to event");

        subscription.add_data(TestData { value: 7 });

        let mut container = SampleContainer::new(2);
        assert_eq!(subscription.try_receive(&mut container, 2).expect("receive sample"), 1);
        assert_eq!(container.pop_front().expect("sample available").value, 7);
    }

    fn setup(
        capacity: usize,
    ) -> (
        MockRuntimeImpl,
        TestOfferedProducer<MockRuntimeImpl>,
        crate::MockSubscriberImpl<TestData>,
    ) {
        let runtime = RuntimeBuilderImpl::new().build().unwrap();
        let specifier = InstanceSpecifier::new("/mock_test/setup").unwrap();
        let offered = runtime
            .producer_builder::<TestInterface>(specifier.clone())
            .build()
            .unwrap()
            .offer()
            .unwrap();
        let subscription = subscribe(&runtime, &specifier, capacity);
        (runtime, offered, subscription)
    }
    fn subscribe(
        runtime: &MockRuntimeImpl,
        specifier: &InstanceSpecifier,
        capacity: usize,
    ) -> crate::MockSubscriberImpl<TestData> {
        runtime
            .find_service::<TestInterface>(FindServiceSpecifier::Specific(specifier.clone()))
            .get_available_instances()
            .unwrap()
            .pop()
            .unwrap()
            .build()
            .unwrap()
            .event
            .subscribe(capacity)
            .unwrap()
    }
    #[test]
    fn independent_runtimes_and_shared_clones() {
        let (runtime, _offered, _subscription) = setup(2);
        let isolated = RuntimeBuilderImpl::new().build().unwrap();
        assert!(isolated
            .find_service::<TestInterface>(FindServiceSpecifier::Any)
            .get_available_instances()
            .unwrap()
            .is_empty());
        assert_eq!(
            runtime
                .clone()
                .find_service::<TestInterface>(FindServiceSpecifier::Any)
                .get_available_instances()
                .unwrap()
                .len(),
            1
        );
    }
    #[test]
    fn consumers_have_independent_queues_and_unsubscribe() {
        let (runtime, offered, first) = setup(4);
        runtime.register_cloneable_data::<TestData>();
        let second = subscribe(&runtime, &InstanceSpecifier::new("/mock_test/setup").unwrap(), 4);
        offered.event.send(TestData { value: 9 }).unwrap();
        let mut a = SampleContainer::new(4);
        let mut b = SampleContainer::new(4);
        assert_eq!(first.try_receive(&mut a, 4).unwrap(), 1);
        assert_eq!(second.try_receive(&mut b, 4).unwrap(), 1);
        assert_eq!(a.front().unwrap().value, 9);
        assert_eq!(b.front().unwrap().value, 9);
        drop(b);
        let subscriber = second.unsubscribe();
        offered.event.send(TestData { value: 10 }).unwrap();
        let second = subscriber.subscribe(4).unwrap();
        let mut b = SampleContainer::new(4);
        assert_eq!(second.try_receive(&mut b, 4).unwrap(), 0);
        assert_eq!(first.try_receive(&mut a, 4).unwrap(), 1);
        assert_eq!(a.iter().last().unwrap().value, 10);
    }
    #[test]
    fn unregistered_multicast_fails_before_delivery() {
        let (runtime, offered, first) = setup(2);
        let second = subscribe(&runtime, &InstanceSpecifier::new("/mock_test/setup").unwrap(), 2);
        assert!(matches!(
            offered.event.send(TestData { value: 9 }),
            Err(score_com_concept::Error::EventError(
                score_com_concept::EventFailedReason::EventPublishFailed
            ))
        ));
        let mut a = SampleContainer::new(2);
        let mut b = SampleContainer::new(2);
        assert_eq!(first.try_receive(&mut a, 2).unwrap(), 0);
        assert_eq!(second.try_receive(&mut b, 2).unwrap(), 0);
    }
    #[test]
    fn bounded_fifo_and_rolling_sample_container() {
        let (_runtime, offered, subscription) = setup(2);
        for value in 1..=3 {
            offered.event.send(TestData { value }).unwrap();
        }
        let mut container = SampleContainer::new(2);
        assert_eq!(subscription.try_receive(&mut container, 2).unwrap(), 2);
        assert_eq!(container.front().unwrap().value, 2);
        assert_eq!(container.iter().last().unwrap().value, 3);
        offered.event.send(TestData { value: 4 }).unwrap();
        assert_eq!(subscription.try_receive(&mut container, 2).unwrap(), 1);
        assert_eq!(container.front().unwrap().value, 3);
        assert_eq!(container.iter().last().unwrap().value, 4);
        assert_eq!(subscription.try_receive(&mut container, 1).unwrap(), 0);
        assert_eq!(container.sample_count(), 1);
        assert_eq!(container.front().unwrap().value, 4);
    }
    #[test]
    fn invalid_limits_and_small_container_preserve_queued_data() {
        let (_runtime, offered, subscription) = setup(2);
        offered.event.send(TestData { value: 7 }).unwrap();
        let mut small = SampleContainer::new(1);
        assert!(subscription.try_receive(&mut small, 2).is_err());
        assert!(subscription.try_receive(&mut small, 0).is_err());
        let mut container = SampleContainer::new(2);
        assert_eq!(subscription.try_receive(&mut container, 2).unwrap(), 1);
        assert_eq!(container.front().unwrap().value, 7);
        let bad = subscription.instance_info.clone();
        assert!(MockSubscribableImpl::<TestData>::new("event", bad)
            .unwrap()
            .subscribe(0)
            .is_err());
    }
    #[test]
    fn rejects_samples_from_a_different_subscription_without_consuming_input() {
        let (_a, offered_a, subscription_a) = setup(2);
        let (_b, offered_b, subscription_b) = setup(2);
        offered_a.event.send(TestData { value: 1 }).unwrap();
        offered_b.event.send(TestData { value: 2 }).unwrap();
        let mut container = SampleContainer::new(2);
        subscription_a.try_receive(&mut container, 2).unwrap();
        assert!(subscription_b.try_receive(&mut container, 2).is_err());
        assert_eq!(container.front().unwrap().value, 1);
        let mut valid = SampleContainer::new(2);
        assert_eq!(subscription_b.try_receive(&mut valid, 2).unwrap(), 1);
    }
    struct WakeCounter(AtomicUsize);
    impl futures::task::ArcWake for WakeCounter {
        fn wake_by_ref(arc: &Arc<Self>) {
            arc.0.fetch_add(1, Ordering::Relaxed);
        }
    }
    #[test]
    fn async_receive_is_woken_by_later_publication() {
        let (_runtime, offered, subscription) = setup(2);
        let counter = Arc::new(WakeCounter(AtomicUsize::new(0)));
        let waker = futures::task::waker(counter.clone());
        let mut cx = core::task::Context::from_waker(&waker);
        let future = subscription.receive(SampleContainer::new(2), 1, 2);
        let mut future = core::pin::pin!(future);
        assert!(core::future::Future::poll(future.as_mut(), &mut cx).is_pending());
        offered.event.send(TestData { value: 13 }).unwrap();
        assert_eq!(counter.0.load(Ordering::Relaxed), 1);
        let core::task::Poll::Ready((container, result)) = core::future::Future::poll(future.as_mut(), &mut cx) else {
            panic!("receive must complete after publication")
        };
        assert_eq!(result.unwrap(), 1);
        assert_eq!(container.front().unwrap().value, 13);
    }
    #[test]
    fn cancellation_returns_partial_container_and_invalid_threshold_fails() {
        let (_runtime, offered, subscription) = setup(2);
        offered.event.send(TestData { value: 1 }).unwrap();
        let (container, result) = subscription
            .cancellable_receive(SampleContainer::new(2), 2, 2, core::future::ready(()))
            .now_or_never()
            .unwrap();
        assert!(matches!(
            result,
            Err(score_com_concept::Error::ReceiveError(
                score_com_concept::ReceiveFailedReason::Cancelled
            ))
        ));
        assert_eq!(container.front().unwrap().value, 1);
        drop(container);
        assert!(subscription
            .receive(SampleContainer::new(2), 3, 2)
            .now_or_never()
            .unwrap()
            .1
            .is_err());
    }
    #[test]
    fn live_stream_waits_and_receives_later_publication() {
        let (_runtime, offered, mut subscription) = setup(2);
        let mut stream = subscription.to_stream();
        assert!(stream.next().now_or_never().is_none());
        offered.event.send(TestData { value: 17 }).unwrap();
        assert_eq!(stream.next().now_or_never().unwrap().unwrap().unwrap().value, 17);
        assert!(stream.next().now_or_never().is_none());
    }
    #[test]
    fn dropping_provider_and_unoffering_stop_discovery_and_publication() {
        let (runtime, offered, _subscription) = setup(2);
        drop(offered);
        assert!(runtime
            .find_service::<TestInterface>(FindServiceSpecifier::Any)
            .get_available_instances()
            .unwrap()
            .is_empty());
        let (_runtime, offered, subscription) = setup(2);
        offered.instance_info.stop_offer_service().unwrap();
        assert!(offered.event.send(TestData { value: 1 }).is_err());
        let mut container = SampleContainer::new(2);
        assert_eq!(subscription.try_receive(&mut container, 2).unwrap(), 0);
    }
    #[test]
    fn event_identifier_and_data_type_are_isolated() {
        let (_runtime, offered, subscription) = setup(2);
        let other = MockSubscribableImpl::<TestData>::new("other", subscription.instance_info.clone())
            .unwrap()
            .subscribe(2)
            .unwrap();
        offered.event.send(TestData { value: 1 }).unwrap();
        let mut container = SampleContainer::new(2);
        assert_eq!(other.try_receive(&mut container, 2).unwrap(), 0);
        assert_eq!(subscription.try_receive(&mut container, 2).unwrap(), 1);
    }
    #[test]
    fn injection_is_subscription_local_and_drop_reclaims_registration() {
        let (runtime, _offered, first) = setup(2);
        let second = subscribe(&runtime, &InstanceSpecifier::new("/mock_test/setup").unwrap(), 2);
        first.add_data(TestData { value: 6 });
        let mut b = SampleContainer::new(2);
        assert_eq!(second.try_receive(&mut b, 2).unwrap(), 0);
        drop(b);
        drop(second);
        drop(first);
        assert!(runtime.registry.lock().unwrap().events.is_empty());
    }

    struct OtherInterface;
    impl Interface for OtherInterface {
        const INTERFACE_ID: &'static str = "mock_test::OtherInterface";
        type Consumer<R: Runtime + ?Sized> = TestConsumer<R>;
        type Producer<R: Runtime + ?Sized> = TestProducer<R>;
    }
    #[test]
    fn discovery_filters_interface_and_supports_async_snapshot() {
        let (runtime, _offered, _subscription) = setup(2);
        assert!(runtime
            .find_service::<OtherInterface>(FindServiceSpecifier::Any)
            .get_available_instances()
            .unwrap()
            .is_empty());
        assert_eq!(
            runtime
                .find_service::<TestInterface>(FindServiceSpecifier::Any)
                .get_available_instances_async()
                .now_or_never()
                .unwrap()
                .unwrap()
                .len(),
            1
        );
    }
    #[derive(Debug)]
    #[repr(C)]
    struct SendOnlyData {
        value: core::cell::Cell<u32>,
    }
    // SAFETY: This value contains only a Cell<u32>; moving its inline bytes does not
    // invalidate any address, and it meets the Send/Unpin/'static Reloc bounds.
    unsafe impl Reloc for SendOnlyData {}
    impl CommData for SendOnlyData {
        const ID: &'static str = "mock_test::SendOnlyData";
    }
    #[test]
    fn send_only_data_needs_neither_clone_nor_sync_and_types_do_not_mix() {
        let (_runtime, offered, normal) = setup(2);
        let send_only = MockSubscribableImpl::<SendOnlyData>::new("event", normal.instance_info.clone())
            .unwrap()
            .subscribe(2)
            .unwrap();
        let publisher = <crate::MockPublisher<SendOnlyData> as Publisher<SendOnlyData, MockRuntimeImpl>>::new(
            "event",
            offered.instance_info.clone(),
        )
        .unwrap();
        publisher
            .send(SendOnlyData {
                value: core::cell::Cell::new(21),
            })
            .unwrap();
        let mut normal_container = SampleContainer::new(2);
        assert_eq!(normal.try_receive(&mut normal_container, 2).unwrap(), 0);
        let mut container = SampleContainer::new(2);
        assert_eq!(send_only.try_receive(&mut container, 2).unwrap(), 1);
        assert_eq!(container.front().unwrap().value.get(), 21);
    }
    #[test]
    fn dropping_async_operation_unregisters_waker() {
        let (_runtime, offered, mut subscription) = setup(2);
        let counter = Arc::new(WakeCounter(AtomicUsize::new(0)));
        let waker = futures::task::waker(counter.clone());
        let mut cx = core::task::Context::from_waker(&waker);
        {
            let future = subscription.receive(SampleContainer::new(2), 1, 2);
            let mut future = core::pin::pin!(future);
            assert!(core::future::Future::poll(future.as_mut(), &mut cx).is_pending());
        }
        offered.event.send(TestData { value: 1 }).unwrap();
        assert_eq!(counter.0.load(Ordering::Relaxed), 0);
        {
            let mut container = SampleContainer::new(2);
            subscription.try_receive(&mut container, 2).unwrap();
        }
        {
            let mut stream = subscription.to_stream();
            assert!(core::pin::Pin::new(&mut stream).poll_next(&mut cx).is_pending());
        }
        offered.event.send(TestData { value: 2 }).unwrap();
        assert_eq!(counter.0.load(Ordering::Relaxed), 0);
    }
    #[test]
    fn concurrent_publishers_preserve_total_reception_order() {
        let (_runtime, offered, subscription) = setup(200);
        std::thread::scope(|scope| {
            for base in [0, 100] {
                let event = &offered.event;
                scope.spawn(move || {
                    for value in base..base + 100 {
                        event.send(TestData { value }).unwrap();
                    }
                });
            }
        });
        let mut container = SampleContainer::new(200);
        assert_eq!(subscription.try_receive(&mut container, 200).unwrap(), 200);
        let mut previous = container.pop_front().unwrap();
        while let Some(next) = container.pop_front() {
            assert!(previous < next);
            previous = next;
        }
    }

    #[test]
    fn overlapping_async_receives_panic_without_poisoning_subscription() {
        let (_runtime, offered, subscription) = setup(2);
        let future = subscription.receive(SampleContainer::new(2), 1, 2);
        let mut future = Box::pin(future);
        assert!(future.as_mut().now_or_never().is_none());
        assert!(std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            subscription.receive(SampleContainer::new(2), 1, 2).now_or_never()
        }))
        .is_err());
        drop(future);
        offered.event.send(TestData { value: 31 }).unwrap();
        let (container, result) = subscription
            .receive(SampleContainer::new(2), 1, 2)
            .now_or_never()
            .unwrap();
        assert_eq!(result.unwrap(), 1);
        assert_eq!(container.front().unwrap().value, 31);
    }
}
