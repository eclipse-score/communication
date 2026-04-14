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
//! `SubscriberImpl`, `LolaConsumerDiscovery`, and `LolaConsumerBuilder`.
//! These components work together to enable consumers to discover services,
//! subscribe to events, and receive data samples in a type-safe manner.
//! The implementation ensures proper memory management and resource handling
//! through Rust's ownership model and FFI safety practices.

//lifetime warning for all the Sample struct impl block, it is required for the Sample struct
// event lifetime parameter
// and mentaining lifetime of instances and data reference
// As of supressing clippy::needless_lifetimes
//TODO: revist this once com-api is stable - Ticket-234827
#![allow(clippy::needless_lifetimes)]

use crate::Debug;
use core::future::Future;
use core::marker::PhantomData;
use core::mem::ManuallyDrop;
use core::ops::{Deref, DerefMut};
use core::panic;
use core::ptr::NonNull;
use futures::task::{AtomicWaker, Context, Poll};
use std::cmp::Ordering;
use std::pin::Pin;
use std::sync::atomic::{AtomicBool, AtomicUsize};
use std::sync::Arc;

use com_api_concept::{
    Builder, CommData, Consumer, ConsumerBuilder, ConsumerDescriptor, ConsumerFailedReason, Error,
    EventFailedReason, InstanceSpecifier, Interface, ReceiveFailedReason, Result, SampleContainer,
    ServiceDiscovery, ServiceFailedReason, Subscriber, Subscription,
};

use bridge_ffi_rs::*;

use crate::LolaRuntimeImpl;

#[derive(Clone, Debug)]
pub struct LolaConsumerInfo {
    handle_container: Arc<mw_com::proxy::HandleContainer>,
    handle_index: usize,
    interface_id: &'static str,
}

impl LolaConsumerInfo {
    /// Get a reference to the handle, guaranteed valid as long as this struct exists
    pub fn get_handle(&self) -> Option<&HandleType> {
        self.handle_container.get(self.handle_index)
    }
}

//TODO: Ticket-238828 this type should be merge with Sample<T>
//And sample_ptr_rs::SamplePtr<T> FFI function should be move in plumbing folder
//sample_ptr_rs module
#[derive(Debug)]
pub struct LolaBinding<T>
where
    T: CommData + Debug,
{
    data: ManuallyDrop<sample_ptr_rs::SamplePtr<T>>,
}

impl<T> Drop for LolaBinding<T>
where
    T: CommData + Debug,
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
    T: CommData + Debug,
{
    //we need unique id for each sample to implement Ord and Eq traits for sorting in
    // SampleContainer
    id: usize,
    inner: LolaBinding<T>,
}

pub static ID_COUNTER: AtomicUsize = AtomicUsize::new(0);

impl<T> Sample<T>
where
    T: CommData + Debug,
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
    T: CommData + Debug,
{
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.get_data()
    }
}

impl<T> com_api_concept::Sample<T> for Sample<T> where T: CommData + Debug {}

// Ordering traits for Sample<T> are using id field to provide total ordering
impl<T> PartialEq for Sample<T>
where
    T: CommData + Debug,
{
    fn eq(&self, other: &Self) -> bool {
        self.id == other.id
    }
}

impl<T> Eq for Sample<T> where T: CommData + Debug {}

impl<T> PartialOrd for Sample<T>
where
    T: CommData + Debug,
{
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl<T> Ord for Sample<T>
where
    T: CommData + Debug,
{
    fn cmp(&self, other: &Self) -> Ordering {
        self.id.cmp(&other.id)
    }
}

/// Manages the lifetime of the native proxy instance,
/// user should clone this to share between threads
/// Always use this struct to manage the proxy instance pointer
pub struct ProxyInstanceManager(Arc<NativeProxyBase>);

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
/// If any additional method is required to be added,
/// ensure that the safety of the proxy instance is maintained
/// And the lifetime is managed correctly
/// As it has Send and Sync unsafe impls,
/// it must not expose any mutable access to the proxy instance
/// Or must not provide any method to access the proxy instance directly
pub struct NativeProxyBase {
    proxy: NonNull<ProxyBase>, // Stores the proxy instance
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
            bridge_ffi_rs::destroy_proxy(self.proxy.as_ptr());
        }
    }
}

impl std::fmt::Debug for NativeProxyBase {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("NativeProxyBase").finish()
    }
}

impl NativeProxyBase {
    pub fn new(interface_id: &str, handle: &HandleType) -> Result<Self> {
        //SAFETY: It is safe to create the proxy because interface_id and handle are valid
        //Handle received at the time of get_avaible_instances called with correct interface_id
        let raw_proxy_ptr = unsafe { bridge_ffi_rs::create_proxy(interface_id, handle) };
        let proxy = std::ptr::NonNull::new(raw_proxy_ptr).ok_or(Error::ConsumerError(
            ConsumerFailedReason::ProxyCreationFailed,
        ))?;
        Ok(Self { proxy })
    }
}

/// This type contains the native proxy event pointer for a specific event identifier
/// Manages the lifetime of the ProxyEventBase pointer
/// Drop is not required as the proxy event lifetime is managed by proxy instance
/// It does not provide any mutable access to the underlying proxy event pointer
/// And the proxy event lifetime is managed safely through Drop of the parent proxy instance
/// user can get the raw pointer using 'get_proxy_event_base' method
pub struct NativeProxyEventBase {
    proxy_event_ptr: NonNull<ProxyEventBase>,
}

//SAFETY: NativeProxyEventBase is to send between threads because:
// It is created by FFI call and there is no state associated with the current thread.
// The skeleton event pointer can be safely moved to another thread without thread-local concerns.
// And the proxy event lifetime is managed safely through Drop of the parent proxy instance
// which ensures the proxy handle remains valid as long as events are in use
unsafe impl Send for NativeProxyEventBase {}

impl NativeProxyEventBase {
    pub fn new(proxy: &NonNull<ProxyBase>, interface_id: &str, identifier: &str) -> Result<Self> {
        //SAFETY: It is safe as we are passing valid proxy pointer and interface id to get event
        // proxy pointer is created during consumer creation
        let raw_event_ptr =
            unsafe { bridge_ffi_rs::get_event_from_proxy(proxy.as_ptr(), interface_id, identifier) };
        let proxy_event_ptr = std::ptr::NonNull::new(raw_event_ptr)
            .ok_or(Error::EventError(EventFailedReason::EventCreationFailed))?;
        Ok(Self { proxy_event_ptr })
    }

    /// Provides access to the underlying proxy event base.
    pub fn get_proxy_event_base(&self) -> &ProxyEventBase {
        // SAFETY: proxy_event_ptr is valid for the entire lifetime of NativeProxyEventBase
        // and was created by FFI during get_event_from_proxy()
        unsafe { self.proxy_event_ptr.as_ref() }
    }
}

impl std::fmt::Debug for NativeProxyEventBase {
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

impl<T: CommData + Debug> Subscriber<T, LolaRuntimeImpl> for SubscribableImpl<T> {
    type Subscription = SubscriberImpl<T>;
    fn new(identifier: &'static str, instance_info: LolaConsumerInfo) -> Result<Self> {
        let handle = instance_info.get_handle().ok_or(Error::ConsumerError(
            ConsumerFailedReason::ServiceHandleNotFound,
        ))?;
        let native_proxy = NativeProxyBase::new(instance_info.interface_id, handle)?;
        let proxy_instance = ProxyInstanceManager(Arc::new(native_proxy));
        Ok(Self {
            identifier,
            instance_info,
            proxy_instance,
            data: PhantomData,
        })
    }
    fn subscribe(self, max_num_samples: usize) -> Result<Self::Subscription> {
        let instance_info = self.instance_info.clone();
        let event_instance = NativeProxyEventBase::new(
            &self.proxy_instance.0.proxy,
            self.instance_info.interface_id,
            self.identifier,
        )?;
        //SAFETY: It is safe to subscribe to event because event_instance is valid
        // which was obtained from valid proxy instance
        let status = unsafe {
            bridge_ffi_rs::subscribe_to_event(
                std::ptr::from_ref(event_instance.get_proxy_event_base()) as *mut ProxyEventBase,
                max_num_samples.try_into().unwrap(),
            )
        };
        if !status {
            return Err(Error::EventError(EventFailedReason::EventNotAvailable));
        }
        // Store in SubscriberImpl with event, max_num_samples
        Ok(SubscriberImpl {
            event: ProxyEventManager::new(
                std::ptr::from_ref(event_instance.get_proxy_event_base()) as *mut ProxyEventBase,
            ),
            event_id: self.identifier,
            max_num_samples,
            instance_info,
            waker_storage: Arc::default(),
            async_init_status: std::sync::Once::new(),
            _proxy: self.proxy_instance.clone(),
            _phantom: PhantomData,
        })
    }
}

/// The ProxyEventManager struct manages the proxy event pointer and
/// ensures that concurrent receive calls are not allowed on the same subscriber instance.
struct ProxyEventManager {
    event: *mut ProxyEventBase,
    in_progress: AtomicBool,
}

//SAFETY: ProxyEventManager is safe to send between threads because:
// Pointer is created during subscription and
// it is valid as long as the subscriber instance is valid
// The proxy event lifetime is managed safely through Drop of the parent subscriber instance
// which ensures the proxy handle remains valid as long as events are in use
// However, it uses an AtomicBool to ensure that concurrent receive calls are not allowed on the
// same subscriber instance,
// which provides thread safety for receive operations.
unsafe impl Send for ProxyEventManager {}
unsafe impl Sync for ProxyEventManager {}

impl ProxyEventManager {
    /// Creates a new ProxyEventManager with the given proxy event pointer.
    pub fn new(event: *mut ProxyEventBase) -> Self {
        Self {
            event,
            in_progress: AtomicBool::new(false),
        }
    }
    /// Provides access to the proxy event pointer while ensuring that-
    /// concurrent receive calls are not allowed on the same subscriber instance.
    pub fn get_proxy_event(&self) -> ProxyEventManagerGuard<'_> {
        //Acquire the lock to ensure that only one receive call can access the proxy event at a time
        //Relaxed ordering is not sufficient here because we need to ensure that the in_progress
        // flag is updated before any receive call can access the proxy event
        if self
            .in_progress
            .swap(true, std::sync::atomic::Ordering::Acquire)
        {
            panic!("Concurrent receive calls are not allowed on the same subscriber instance");
        }
        ProxyEventManagerGuard { manager: self }
    }
}

impl Debug for ProxyEventManager {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("ProxyEventManager")
            .field(
                "in_progress",
                &self.in_progress.load(std::sync::atomic::Ordering::Relaxed),
            )
            .finish()
    }
}

/// The ProxyEventManagerGuard struct provides safe access to the proxy event pointer
/// while ensuring that concurrent receive calls are not allowed on the same subscriber instance.
/// It implements Deref and DerefMut to allow access to the underlying ProxyEventBase pointer,
/// and it uses the Drop trait to reset the in_progress flag when the guard goes out of scope.
struct ProxyEventManagerGuard<'a> {
    manager: &'a ProxyEventManager,
}

impl<'a> Drop for ProxyEventManagerGuard<'a> {
    fn drop(&mut self) {
        self.manager
            .in_progress
            .store(false, std::sync::atomic::Ordering::Release);
    }
}

impl<'a> Deref for ProxyEventManagerGuard<'a> {
    type Target = ProxyEventBase;

    fn deref(&self) -> &Self::Target {
        unsafe { self.manager.event.as_ref().expect("Event pointer is null") }
    }
}

impl<'a> DerefMut for ProxyEventManagerGuard<'a> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        unsafe { self.manager.event.as_mut().expect("Event pointer is null") }
    }
}

/// The SubscriberImpl struct implements the Subscriber trait for LolaRuntimeImpl.
/// It manages the subscription to a specific event and provides methods to receive samples.
/// It uses the ProxyEventManager to manage access to
/// the proxy event pointer and ensure thread safety during receive operations.
/// It also manages the asynchronous initialization of the receive callback
/// and the waker storage for async notifications when new samples arrive.
#[derive(Debug)]
pub struct SubscriberImpl<T>
where
    T: CommData + Debug,
{
    event: ProxyEventManager,
    event_id: &'static str,
    max_num_samples: usize,
    instance_info: LolaConsumerInfo,
    waker_storage: Arc<AtomicWaker>,
    async_init_status: std::sync::Once,
    _proxy: ProxyInstanceManager,
    _phantom: PhantomData<T>,
}

impl<T: CommData + Debug> Drop for SubscriberImpl<T> {
    fn drop(&mut self) {
        // SAFETY: It is safe to unsubscribe from the event because the event pointer is valid
        // and was created during subscription.
        // Exculsive access to the event pointer is guaranteed by the ProxyEventManagerGuard.
        // Unset the receive handler to release the async callback,
        // and then unsubscribe from the event to clean up resources on the C++ side.
        let mut guard = self.event.get_proxy_event();
        unsafe {
            if self.async_init_status.is_completed()
            // Check if the async receive callback was initialized
            {
                bridge_ffi_rs::clear_event_receive_handler(guard.deref_mut(), T::ID);
            }
            bridge_ffi_rs::unsubscribe_to_event(guard.deref_mut());
        }
    }
}

impl<T: CommData + Debug> SubscriberImpl<T> {
    fn init_async_receive(&self, event_guard: &mut ProxyEventManagerGuard) -> Result<()> {
        let callback_waker = Arc::clone(&self.waker_storage);
        let waker_callback = move || {
            callback_waker.wake();
        };
        let boxed_handler = Box::new(waker_callback) as Box<dyn FnMut() + Send + 'static>;
        let ptr = Box::into_raw(boxed_handler);
        let fat_ptr: FatPtr = unsafe { std::mem::transmute(ptr) };

        // SAFETY: it is safe to set the event receive handler because event ptr is a valid
        // ProxyEventBase pointer.
        // The callback is a simple waker that wakes the future when new samples arrive,
        // and the lifetime of the callback is managed by Rust, it will not outlive the scope of
        // this function call.
        unsafe {
            bridge_ffi_rs::set_event_receive_handler(event_guard.deref_mut(), &fat_ptr, T::ID);
        }
        Ok(())
    }
}

impl<T> Subscription<T, LolaRuntimeImpl> for SubscriberImpl<T>
where
    T: CommData + Debug,
{
    type Subscriber = SubscribableImpl<T>;
    type Sample<'a> = Sample<T>;

    /// The unsubscribe method consumes the subscription and returns the subscribable instance.
    /// Calling `unsubscribe` while a `SampleContainer` holding samples whose lifetime
    /// is tied to the subscription is still in scope so it must not compile.
    ///
    /// `try_receive` fills the container with `S::Sample<'a>` where `'a` is the
    /// borrow lifetime of `self`.  The borrow checker therefore prevents moving
    /// `self` (via `unsubscribe`) while the container and the samples it may hold
    /// are still in scope.
    ///
    /// ``` compile_fail
    /// use com_api_concept::{CommData, SampleContainer, Subscription};
    /// use com_api_runtime_lola::LolaRuntimeImpl;
    ///
    /// fn demonstrate_sample_container_lifetime_borrow<T, S>(sub: S)
    /// where
    ///     T: CommData + std::fmt::Debug,
    ///     S: Subscription<T, LolaRuntimeImpl>,
    /// {
    ///     let mut container = SampleContainer::new(2);
    ///     let _ = sub.try_receive(&mut container, 2);
    ///     sub.unsubscribe(); // ERROR: cannot move `sub` while `container` borrows it
    ///     drop(container);
    /// }
    /// ```
    fn unsubscribe(self) -> Self::Subscriber {
        //Unsubscribe FFI call will be triggered in Drop implementation of SubscriberImpl.
        SubscribableImpl {
            identifier: self.event_id,
            instance_info: self.instance_info.clone(),
            proxy_instance: self._proxy.clone(),
            data: PhantomData,
        }
    }

    fn try_receive<'a>(
        &'a self,
        scratch: &'_ mut SampleContainer<Self::Sample<'a>>,
        max_samples: usize,
    ) -> Result<usize> {
        try_receive_samples::<T>(
            self.event.get_proxy_event().deref_mut(),
            scratch,
            self.max_num_samples,
            max_samples,
        )
    }

    // Cannot use `async fn` because the trait mandates `-> impl Future + 'a`,
    // requiring the returned future to be explicitly bound to the lifetime of `&self`.
    #[allow(clippy::manual_async_fn)]
    fn receive<'a>(
        &'a self,
        scratch: SampleContainer<Self::Sample<'a>>,
        new_samples: usize,
        max_samples: usize,
    ) -> impl Future<Output = Result<SampleContainer<Self::Sample<'a>>>> + 'a {
        async move {
            if max_samples > self.max_num_samples || new_samples > self.max_num_samples {
                return Err(Error::ReceiveError(
                    ReceiveFailedReason::SampleCountOutOfBounds {
                        max: self.max_num_samples,
                        requested: max_samples.max(new_samples),
                    },
                ));
            }
            // Get the event guard to ensure no concurrent receive calls
            // on the same subscriber instance.
            let mut event_guard = self.event.get_proxy_event();
            // Initialize the async receive callback only once when the first receive call is made
            // We are using std::sync::Once to ensure that the callback is set only once.
            self.async_init_status.call_once(|| {
                self.init_async_receive(&mut event_guard)
                    .expect("Failed to initialize async receive callback");
            });
            ReceiveFuture {
                event_guard: Some(event_guard),
                waker_storage: Arc::clone(&self.waker_storage),
                max_num_samples: self.max_num_samples,
                scratch: Some(scratch),
                new_samples,
                max_samples,
                total_received: 0,
            }
            .await
        }
    }
}

// The ReceiveFuture struct encapsulates the state and logic for asynchronously receiving samples
// from the proxy event. It holds a reference to the proxy event manager,
// a waker storage for async notifications, and parameters for managing the receive operation.
// The Future implementation for ReceiveFuture defines the polling logic,
// which attempts to receive samples and manages the state of the receive operation.
struct ReceiveFuture<'a, T: CommData + Debug> {
    event_guard: Option<ProxyEventManagerGuard<'a>>,
    waker_storage: Arc<AtomicWaker>,
    max_num_samples: usize,
    scratch: Option<SampleContainer<Sample<T>>>,
    new_samples: usize,
    max_samples: usize,
    total_received: usize,
}

impl<'a, T: CommData + Debug> Future for ReceiveFuture<'a, T> {
    type Output = Result<SampleContainer<Sample<T>>>;

    fn poll(mut self: Pin<&mut Self>, ctx: &mut Context<'_>) -> Poll<Self::Output> {
        // Extract all immutable values upfront to avoid borrow conflicts with self in the callback
        let max_samples = self.max_samples;
        let new_samples = self.new_samples;
        let max_num_samples = self.max_num_samples;
        let total_received = self.total_received;

        // Register the current waker to be notified when new samples arrive via FFI callback
        self.waker_storage.register(ctx.waker());

        let samples_received = {
            // Temporarily take ownership of scratch to avoid borrow issues
            if let Some(mut scratch) = self.scratch.take() {
                if let Some(event_guard) = self.event_guard.as_mut() {
                    let result = try_receive_samples::<T>(
                        event_guard.deref_mut(),
                        &mut scratch,
                        max_num_samples,
                        max_samples - total_received,
                    );
                    self.scratch = Some(scratch);
                    result
                } else {
                    Err(Error::ReceiveError(ReceiveFailedReason::ReceiveError))
                }
            } else {
                Err(Error::ReceiveError(ReceiveFailedReason::BufferUnavailable))
            }
        };
        match samples_received {
            Ok(count) => {
                self.total_received += count;

                // Check if we've received enough samples
                if self.total_received >= new_samples {
                    //event_guard will be dropped here, allowing new receive calls to access the
                    // proxy event
                    self.event_guard = None;
                    return Poll::Ready(Ok(self
                        .scratch
                        .take()
                        .expect("SampleContainer is not available when returning Future result")));
                }
                // Have some samples but not enough yet, wait for more via waker
                Poll::Pending
            }
            Err(e) => Poll::Ready(Err(e)),
        }
    }
}

/// Holds the shared state populated asynchronously by the C++ service discovery callback.
///
/// `handles` is wrapped in `Option` because:
/// - It is populated asynchronously when the C++ callback fires, not synchronously.
/// - Before the callback completes, `handles` must represent "not yet available" → `None`.
/// - After the callback completes, `handles` contains `Some(ServiceHandleContainer)`.
/// - `poll()` takes ownership via `.take()` to prevent double-processing of the same result.
///
/// `find_handle` is intentionally NOT stored here — it is owned directly by
/// `ServiceDiscoveryFuture`.
/// Rationale:
/// - `start_find_service` returns it synchronously, guaranteeing availability for cleanup.
/// - Storing in callback would create a race: if the future drops before the callback fires,
///   `find_handle` remains `None` → `stop_find_service` is never called → C++ discovery leaks.
/// - C++ passes the same handle both as return value and callback argument; we use only the
///   return value to eliminate double-write races and ensure deterministic cleanup.
struct DiscoveryStateData {
    handles: Option<mw_com::proxy::HandleContainer>,
}

impl std::fmt::Debug for DiscoveryStateData {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("DiscoveryStateData")
            .field("handles", &self.handles)
            .finish()
    }
}

pub struct LolaConsumerDiscovery<I> {
    pub instance_specifier: InstanceSpecifier,
    pub _interface: PhantomData<I>,
}

impl<I: Interface> LolaConsumerDiscovery<I> {
    pub fn new(_runtime: &LolaRuntimeImpl, instance_specifier: InstanceSpecifier) -> Self {
        Self {
            instance_specifier,
            _interface: PhantomData,
        }
    }
}

impl<I: Interface + Send> ServiceDiscovery<I, LolaRuntimeImpl> for LolaConsumerDiscovery<I>
where
    LolaConsumerBuilder<I>: ConsumerBuilder<I, LolaRuntimeImpl>,
{
    type ConsumerBuilder = LolaConsumerBuilder<I>;
    type ServiceEnumerator = Vec<LolaConsumerBuilder<I>>;

    fn get_available_instances(&self) -> Result<Self::ServiceEnumerator> {
        //If ANY Support is added in Lola, then we need to return all available instances
        //Once FFI layer error handling is in place (SWP-253124), we should convert this error to a proper FFI error instead of using map_err here
        let instance_specifier_lola =
            mw_com::InstanceSpecifier::try_from(self.instance_specifier.as_ref())
                .map_err(|_| Error::ServiceError(ServiceFailedReason::InstanceSpecifierInvalid))?;

        let service_handle = mw_com::proxy::find_service(instance_specifier_lola)
            .map_err(|_| Error::ServiceError(ServiceFailedReason::ServiceNotFound))?;

        let service_handle_arc = Arc::new(service_handle);
        let available_instances = (0..service_handle_arc.len())
            .map(|handle_index| {
                let instance_info = LolaConsumerInfo {
                    handle_container: Arc::clone(&service_handle_arc),
                    handle_index,
                    interface_id: I::INTERFACE_ID,
                };
                LolaConsumerBuilder {
                    instance_info,
                    _interface: PhantomData,
                }
            })
            .collect();
        Ok(available_instances)
    }
    /// This function initiates an asynchronous service discovery process and returns a future
    /// that resolves to the available service instances.
    /// It uses FFI to call the underlying C++ service discovery mechanism and sets up a callback
    /// to receive the discovery results.
    /// The future will poll for discovery results and return them once available, while also
    /// ensuring stop find service is called when the future is dropped to clean up resources.
    /// The implementation uses an AtomicWaker to wake up the future when discovery results are
    /// received from the C++ callback,
    /// and it manages the shared state of discovery results using a Mutex.
    fn get_available_instances_async(
        &self,
    ) -> impl Future<Output = Result<Self::ServiceEnumerator>> + Send {
        let instance_specifier = self.instance_specifier.clone();

        // Convert to Lola InstanceSpecifier early
        //Once FFI layer error handling is in place (SWP-253124), we should convert this error to a proper FFI error instead of using map_err here
        let instance_specifier_lola =
            mw_com::InstanceSpecifier::try_from(instance_specifier.as_ref())
                .map_err(|_| Error::ServiceError(ServiceFailedReason::InstanceSpecifierInvalid));

        let waker_storage = Arc::new(futures::task::AtomicWaker::new());

        let discovery_state = Arc::new(std::sync::Mutex::new(DiscoveryStateData { handles: None }));

        let state_ref = Arc::clone(&discovery_state);
        let waker_ref = Arc::clone(&waker_storage);

        // The C++ StartFindService API mandates the callback receives both:
        //   (ServiceHandleContainer<HandleType>, FindServiceHandle)
        // We store only `handles` here. The `find_handle` callback argument is
        // intentionally ignored because the same handle is already captured
        // synchronously from `start_find_service`'s return value and stored
        // directly in `ServiceDiscoveryFuture`, eliminating the double-write race.
        let discovery_callback = Box::new(
            move |handles: mw_com::proxy::HandleContainer,
                  _find_handle: bridge_ffi_rs::NativeFindServiceHandle| {
                if let Ok(mut state) = state_ref.lock() {
                    state.handles = Some(handles);
                }
                waker_ref.wake();
            },
        );

        let dyn_callback: Box<
            dyn FnMut(mw_com::proxy::HandleContainer, bridge_ffi_rs::NativeFindServiceHandle)
                + Send
                + 'static,
        > = discovery_callback;

        // SAFETY: it is safe to transmute the closure to a FatPtr because it has
        // the same representation in memory as a FnMut fat pointer
        let fat_ptr: FatPtr = unsafe { std::mem::transmute(dyn_callback) };

        let find_service_result = instance_specifier_lola.and_then(|spec| {
            // SAFETY: start_find_service is safe because fat_ptr is valid and
            // instance specifier is valid. The returned handle is stored
            // synchronously — before any async polling — guaranteeing
            // stop_find_service is always called in Drop.
            let raw_handle = unsafe { bridge_ffi_rs::start_find_service(&fat_ptr, spec) };
            if raw_handle.is_null() {
                Err(Error::ServiceError(
                    ServiceFailedReason::FailedToStartDiscovery,
                ))
            } else {
                // Single authoritative source of find_handle — return value only.
                // Callback's find_handle argument is ignored to prevent double-write.
                Ok(bridge_ffi_rs::NativeFindServiceHandle::new(raw_handle))
            }
        });
        async move {
            // Early return error if starting service discovery failed, to avoid awaiting on the
            // future when there is error in starting discovery
            let find_handle = find_service_result?;
            // Create and await the discovery future
            ServiceDiscoveryFuture {
                find_handle,
                discovery_state,
                waker_storage,
                _interface: PhantomData,
            }
            .await
        }
    }
}

/// Future that waits for service discovery to complete and then returns the available instances
/// It polls the shared state for discovery results and uses an AtomicWaker to wake up when
/// results are available
/// It also ensures that the find service is stopped when the future is dropped to clean up
/// resources.
/// Stop find service in Drop implementation to ensure that we clean up the find service if the
/// future is dropped before completion
struct ServiceDiscoveryFuture<I: Interface> {
    find_handle: bridge_ffi_rs::NativeFindServiceHandle,
    discovery_state: Arc<std::sync::Mutex<DiscoveryStateData>>,
    waker_storage: Arc<futures::task::AtomicWaker>,
    _interface: PhantomData<I>,
}

impl<I: Interface> Drop for ServiceDiscoveryFuture<I> {
    fn drop(&mut self) {
        // SAFETY: find_handle is always valid here — it was stored synchronously
        // from start_find_service return value before any async polling began.
        // This unconditional call ensures the C++ discovery operation is always
        // cleaned up, even when the future is dropped before the callback fires.
        unsafe {
            bridge_ffi_rs::stop_find_service(
                self.find_handle.as_mut() as *mut bridge_ffi_rs::FindServiceHandle
            );
        }
    }
}

impl<I: Interface> Future for ServiceDiscoveryFuture<I> {
    type Output = Result<Vec<LolaConsumerBuilder<I>>>;

    fn poll(
        self: std::pin::Pin<&mut Self>,
        ctx: &mut std::task::Context<'_>,
    ) -> std::task::Poll<Self::Output> {
        // Register the waker so C++ callback can wake us up
        self.waker_storage.register(ctx.waker());

        // NOTE: Lock usage in poll() is acceptable here because:
        // The callback (running on C++ thread) and this poll (running on executor thread) both
        // access the same shared mutable state (discovery_state).
        // Without synchronization, the callback writing handles while poll reads
        // them could cause data races and undefined behavior across thread boundaries.
        // The lock duration is minimal - just reading two Option fields, not blocking operations.
        // In practice, contention is zero: callback runs once asynchronously,
        // poll spins until done.
        // The Mutex is necessary for memory safety, not just performance.
        let mut state_guard = self
            .discovery_state
            .lock()
            .expect("failed to acquire discovery_state lock");
        if let Some(service_handle) = state_guard.handles.take() {
            //create Arc for service handle to share between instances
            let service_handle_arc = Arc::new(service_handle);
            // Build the response from discovered handles
            let available_instances = (0..service_handle_arc.len())
                .map(|handle_index| {
                    let instance_info = LolaConsumerInfo {
                        handle_container: Arc::clone(&service_handle_arc),
                        handle_index,
                        interface_id: I::INTERFACE_ID,
                    };
                    LolaConsumerBuilder {
                        instance_info,
                        _interface: PhantomData,
                    }
                })
                .collect();
            return std::task::Poll::Ready(Ok(available_instances));
        }

        // Wait for discovery to complete - C++ callback will wake us
        std::task::Poll::Pending
    }
}

impl<I: Interface> ConsumerBuilder<I, LolaRuntimeImpl> for LolaConsumerBuilder<I> {}

impl<I: Interface> Builder<I::Consumer<LolaRuntimeImpl>> for LolaConsumerBuilder<I> {
    fn build(self) -> Result<I::Consumer<LolaRuntimeImpl>> {
        Ok(Consumer::new(self.instance_info))
    }
}

pub struct LolaConsumerBuilder<I: Interface> {
    pub instance_info: LolaConsumerInfo,
    pub _interface: PhantomData<I>,
}

impl<I: Interface> ConsumerDescriptor<LolaRuntimeImpl> for LolaConsumerBuilder<I> {
    fn get_instance_identifier(&self) -> &InstanceSpecifier {
        //if InstanceSpecifier::ANY support enable by lola
        //then this API should get InstanceSpecifier from FFI Call
        panic!(
            "InstanceSpecifier::ANY is not supported in LolaRuntimeImpl,
        Please use FindServiceSpecifier::Specific with a valid instance specifier."
        );
    }
}

/// Fetches available samples from a proxy event into the scratch buffer.
///
/// This is the standalone implementation of the sample-receive logic, shared by
/// `Subscription::try_receive` and `ReceiveFuture::poll`.
///
/// # Parameters
/// * `event` - Mutable reference to the proxy event to fetch samples from
/// * `scratch` - Mutable reference to the sample container
/// * `max_num_samples` - Maximum allowed samples for this subscription
/// * `max_samples` - How many samples to fetch in this call
fn try_receive_samples<T: CommData + Debug>(
    event: &mut ProxyEventBase,
    scratch: &mut SampleContainer<Sample<T>>,
    max_num_samples: usize,
    max_samples: usize,
) -> Result<usize> {
    if max_samples > max_num_samples {
        return Err(Error::ReceiveError(
            ReceiveFailedReason::SampleCountOutOfBounds {
                max: max_num_samples,
                requested: max_samples,
            },
        ));
    }
    // Create a callback that will be called by the C++ side for each new sample arrival
    let mut callback = create_sample_callback(scratch, max_samples);
    // Convert closure to FatPtr for C++ callback
    let dyn_callback: &mut dyn FnMut(*mut sample_ptr_rs::SamplePtr<T>) = &mut callback;
    // SAFETY: it is safe to transmute the closure reference to a FatPtr because
    // it has the same representation in memory like FnMut pointer
    let fat_ptr: FatPtr = unsafe { std::mem::transmute(dyn_callback) };
    // SAFETY: event is a valid ProxyEventBase pointer obtained during subscription.
    // The lifetime of the callback is managed by Rust and will not outlive this function call.
    let count = unsafe {
        bridge_ffi_rs::get_samples_from_event(
            event as *mut ProxyEventBase,
            T::ID,
            &fat_ptr,
            max_num_samples as u32,
        )
    };
    if count > max_num_samples as u32 {
        return Err(Error::ReceiveError(
            ReceiveFailedReason::SampleCountOutOfBounds {
                max: max_num_samples,
                requested: count as usize,
            },
        ));
    }
    Ok(count as usize)
}

/// Creates a callback function for processing FFI samples into the sample container.
///
/// This callback is invoked by the C++ side for each new sample arrival.
/// It wraps raw sample pointers into Rust-managed Sample<T> objects and stores them
/// in the provided scratch buffer, maintaining the max_samples limit.
///
/// # Safety
/// The returned closure must not outlive the scope where `scratch` is valid.
/// The closure captures a mutable reference to `scratch`.
///
/// # Parameters
/// * `scratch` - Mutable reference to the sample container
/// * `max_samples` - Maximum number of samples to maintain in the container
pub fn create_sample_callback<'a, T: CommData + Debug>(
    scratch: &'a mut SampleContainer<Sample<T>>,
    max_samples: usize,
) -> impl FnMut(*mut sample_ptr_rs::SamplePtr<T>) + 'a {
    move |raw_sample: *mut sample_ptr_rs::SamplePtr<T>| {
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
            // After pop from SampleContainer to make room,
            // push should always succeed, otherwise we lose the data
            assert!(
                scratch.push_back(wrapped_sample).is_ok(),
                "Failed to push sample after making room in buffer"
            );
        }
    }
}

#[cfg(test)]
mod test {
    use com_api_concept::{
        CommData, Consumer, Interface, OfferedProducer, Producer, Result, Runtime, ServiceDiscovery,
    };

    #[derive(Debug, Clone, Copy)]
    #[repr(C)]
    struct TestData(u32);

    unsafe impl com_api_concept::Reloc for TestData {}

    impl CommData for TestData {
        const ID: &'static str = "TestData";
    }

    // Test Consumer implementation
    #[derive(Debug)]
    struct TestConsumer;

    impl<R: Runtime + ?Sized> Consumer<R> for TestConsumer {
        fn new(_: R::ConsumerInfo) -> Self {
            TestConsumer
        }
    }

    // Test Producer implementation
    #[derive(Debug)]
    struct TestProducer;

    impl<R: Runtime + ?Sized> Producer<R> for TestProducer {
        type Interface = TestVehicleInterface;
        type OfferedProducer = TestOfferedProducer;

        fn offer(self) -> Result<Self::OfferedProducer> {
            Ok(TestOfferedProducer)
        }

        fn new(_: R::ProviderInfo) -> Result<Self>
        where
            Self: Sized,
        {
            Ok(TestProducer)
        }
    }

    // Test OfferedProducer implementation
    struct TestOfferedProducer;

    impl<R: Runtime + ?Sized> OfferedProducer<R> for TestOfferedProducer {
        type Interface = TestVehicleInterface;
        type Producer = TestProducer;

        fn unoffer(self) -> Result<Self::Producer> {
            Ok(TestProducer)
        }
    }

    // Test Interface implementation
    struct TestVehicleInterface;

    impl Interface for TestVehicleInterface {
        const INTERFACE_ID: &'static str = "TestVehicleInterface";
        type Consumer<R: Runtime + ?Sized> = TestConsumer;
        type Producer<R: Runtime + ?Sized> = TestProducer;
    }

    #[test]
    fn test_comm_data_trait() {
        // Verify TestData implements CommData
        assert_eq!(TestData::ID, "TestData");
        let _data = TestData(42);
    }

    #[test]
    fn test_discovery_state_data_creation() {
        // Test that DiscoveryStateData can be created with None values
        let state = super::DiscoveryStateData { handles: None };
        assert!(state.handles.is_none());
    }

    #[test]
    fn test_discovery_state_data_debug() {
        // Test that DiscoveryStateData debug formatting works
        let state = super::DiscoveryStateData { handles: None };
        let debug_string = format!("{:?}", state);
        assert!(debug_string.contains("DiscoveryStateData"));
        assert!(debug_string.contains("handles"));
    }

    #[test]
    fn test_sample_consumer_discovery_creation() {
        // Test that LolaConsumerDiscovery can be created with valid interface
        let instance_specifier = com_api_concept::InstanceSpecifier::new("/test/vehicle")
            .expect("Failed to create instance specifier");

        let _discovery = super::LolaConsumerDiscovery::<TestVehicleInterface>::new(
            &super::super::LolaRuntimeImpl {},
            instance_specifier,
        );
        // Discovery created successfully
    }

    #[test]
    #[ignore] // Requires LoLa runtime initialization and configuration
    fn test_get_available_instances_method_exists() {
        // Test that get_available_instances() can be called on ServiceDiscovery trait
        let instance_specifier = com_api_concept::InstanceSpecifier::new("/test/vehicle")
            .expect("Failed to create instance specifier");

        let discovery = super::LolaConsumerDiscovery::<TestVehicleInterface>::new(
            &super::super::LolaRuntimeImpl {},
            instance_specifier,
        );

        // Call get_available_instances - verifies the method exists and is callable
        let _result = discovery.get_available_instances();
    }
}
