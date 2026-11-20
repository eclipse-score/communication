/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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

//! LoLa runtime implementation of Rust method calls.
//!
//! Calls go through the type-erased C++ method bindings: in-args and return-value buffers,
//! `mw_com_proxy_method_do_call` and `mw_com_skeleton_method_register_handler`. A call is synchronous and
//! returned as an already resolved `Future`. The C++ binding has one call in flight per method
//! (`kCallQueueSize == 1`), so every call uses queue position 0.
//!
//! The C++ proxy method is not thread-safe, so each caller holds a mutex over the whole
//! buffer / DoCall / read-back sequence.
//!
//! Field get and set are still placeholders (https://github.com/eclipse-score/communication/issues/1622).
use core::any::TypeId;
use core::fmt::Debug;
use core::future::Future;
use core::marker::PhantomData;
use core::ops::Deref;
use core::ptr::NonNull;
use core::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Mutex, MutexGuard};

use bridge_ffi_rs::{FFIBridge, FatPtr, ProxyMethodBinding, SkeletonMethodBinding};
use score_com_concept::{
    CommData, Error, MethodArgs, MethodArgsAllocate, MethodArgsPtrTuple, MethodCaller, MethodFailedReason,
    MethodHandler, MethodHandlerCall, MethodInArgAllocator, MethodInArgMaybeUninit, MethodInArgPtr, MethodReturnSample,
    Result, Runtime, ZeroCopyArgs,
};
use score_log as log;

use crate::consumer::{LolaConsumerInfo, NativeProxyBase};
use crate::producer::LolaProviderInfo;
use crate::LolaRuntimeImpl;

/// Return value of a method call on the consumer side, copied out of the C++ return-value buffer.
pub struct LolaMethodReturnSample<T> {
    value: T,
}

impl<T> Deref for LolaMethodReturnSample<T> {
    type Target = T;
    fn deref(&self) -> &T {
        &self.value
    }
}

impl<T> MethodReturnSample<T> for LolaMethodReturnSample<T> {}

// The copy path (`invoke_with_copy`, `LolaMethodHandler::register_handler`) packs `Args` field by field
// with `score_com_concept::MethodArgsCodec`, matching the C++ `type_erased_storage.h` layout; a Rust
// tuple's own layout is not guaranteed to match it.

/// Boxed skeleton-method handler in the flattened FFI shape: quality type, then optional `(ptr, len)`
/// in-args and return-value buffers (see `mw_com_impl_call_method_handler`, bridge_ffi_lola.rs). `Fn + Sync`
/// because C++ may call it from several threads at once.
type BoxedMethodHandlerFn = Box<dyn Fn(u8, Option<(*mut u8, usize)>, Option<(*mut u8, usize)>) + Send + Sync + 'static>;

/// Whether `Return` is `()` (a void method). `Return` is generic in the concept traits, so this is a
/// `TypeId` check (`CommData: Reloc: 'static`).
fn is_void<Return: 'static>() -> bool {
    TypeId::of::<Return>() == TypeId::of::<()>()
}

/// Takes a caller's call lock. A poisoned lock is still usable: it guards no data, only the C++ call.
fn lock_calls(lock: &Mutex<()>) -> MutexGuard<'_, ()> {
    lock.lock().unwrap_or_else(|poisoned| poisoned.into_inner())
}

/// Owns its own `ProxyBase` (as each event subscriber in consumer.rs does) and the resolved
/// `ProxyMethodBinding*`.
pub struct LolaMethodCaller<Args: MethodArgs, Return: CommData, B: FFIBridge> {
    // Owned by the C++ proxy behind `_proxy`; only valid while `_proxy` is alive.
    binding: NonNull<ProxyMethodBinding>,
    _proxy: NativeProxyBase<B>,
    bridge: B,
    /// Held for each complete call on `binding`.
    call_lock: Mutex<()>,
    _phantom: PhantomData<(Args, Return)>,
}

// SAFETY: as for NativeProxyBase (consumer.rs): `binding` points into the C++ proxy owned by `_proxy`.
unsafe impl<Args: MethodArgs, Return: CommData, B: FFIBridge> Send for LolaMethodCaller<Args, Return, B> {}

impl<Args: MethodArgs, Return: CommData, B: FFIBridge> LolaMethodCaller<Args, Return, B> {
    /// Calls the method (queue position 0) once the in-args buffer is filled, and reads the return value.
    /// `_calls` is the `call_lock` guard, held since the in-args buffer was requested.
    fn do_call_and_read_return(&self, _calls: &MutexGuard<'_, ()>) -> Result<Return> {
        let return_ptr = if is_void::<Return>() {
            None
        } else {
            // SAFETY: self.binding is valid while _proxy is alive. Requested before DoCall, as
            // ProxyMethod<Signature>::operator() does.
            let (ptr, len) = unsafe {
                self.bridge
                    .proxy_method_get_return_value_buffer(self.binding.as_ptr(), 0)
            }
            .ok_or(Error::MethodError(MethodFailedReason::ReturnValueBufferUnavailable))?;
            // The buffer is sized for the C++ return type; a different size means the types disagree.
            if len != core::mem::size_of::<Return>() {
                log::error!("LolaMethodCaller: return-value buffer size does not match the Rust return type");
                return Err(Error::MethodError(MethodFailedReason::ReturnValueBufferUnavailable));
            }
            Some(ptr)
        };

        // SAFETY: self.binding is valid; the caller has filled any in-args buffer.
        let call_ok = unsafe { self.bridge.proxy_method_do_call(self.binding.as_ptr(), 0) };
        if !call_ok {
            return Err(Error::MethodError(MethodFailedReason::CallFailed));
        }

        let value = if let Some(ptr) = return_ptr {
            // SAFETY: a successful DoCall leaves a live Return in this buffer, which is size_of::<Return>()
            // bytes (checked above) and aligned by the C++ binding.
            unsafe { (ptr as *const Return).read() }
        } else {
            // is_void::<Return>() proved Return == (), so this is a same-type, zero-sized copy.
            let unit = ();
            unsafe { core::mem::transmute_copy::<(), Return>(&unit) }
        };
        Ok(value)
    }
}

impl<Args: MethodArgs, Return: CommData, B: FFIBridge> MethodCaller<Args, Return, LolaRuntimeImpl<B>>
    for LolaMethodCaller<Args, Return, B>
{
    fn new(method_name: &str, instance_info: LolaConsumerInfo<B>) -> Result<Self>
    where
        Self: Sized,
    {
        // One ProxyBase per caller, as LolaSubscribableImpl does per event subscriber.
        let handle = instance_info.get_handle().ok_or(Error::ConsumerError(
            score_com_concept::ConsumerFailedReason::ServiceHandleNotFound,
        ))?;
        let proxy = NativeProxyBase::new(instance_info.bridge(), instance_info.interface_id(), handle)?;

        // SAFETY: proxy.as_ptr() is a valid, just-created ProxyBase*.
        let raw_binding = unsafe {
            instance_info
                .bridge()
                .get_method_from_proxy(proxy.as_ptr(), instance_info.interface_id(), method_name)
        };
        let binding =
            NonNull::new(raw_binding).ok_or(Error::MethodError(MethodFailedReason::MethodCallerCreationFailed))?;

        Ok(Self {
            binding,
            bridge: instance_info.bridge().clone(),
            _proxy: proxy,
            call_lock: Mutex::new(()),
            _phantom: PhantomData,
        })
    }

    #[allow(clippy::manual_async_fn)]
    fn invoke_with_copy<'a>(&'a self, args: Args) -> impl Future<Output = Result<LolaMethodReturnSample<Return>>> + 'a {
        async move {
            // The body has no await point, so the guard is never held across one.
            let calls = lock_calls(&self.call_lock);
            let (size, _align) = Args::layout();
            if size > 0 {
                // SAFETY: self.binding is valid; only called for methods with in-arguments, as
                // GetInArgsBuffer requires.
                let (buf_ptr, buf_len) =
                    unsafe { self.bridge.proxy_method_get_in_args_buffer(self.binding.as_ptr(), 0) }
                        .ok_or(Error::MethodError(MethodFailedReason::InArgsBufferUnavailable))?;
                // The buffer is sized for the C++ argument types; a different size means the C++ and Rust
                // argument layouts disagree.
                if buf_len != size {
                    log::error!("LolaMethodCaller: in-args buffer size does not match the Rust argument types");
                    return Err(Error::MethodError(MethodFailedReason::InArgsBufferUnavailable));
                }
                // SAFETY: buf_ptr is valid for buf_len == size bytes and aligned by the C++ binding.
                unsafe { args.write_into(buf_ptr) };
            }
            let value = self.do_call_and_read_return(&calls)?;
            Ok(LolaMethodReturnSample { value })
        }
    }

    fn allocate(&self) -> Result<<Args as MethodArgsAllocate<LolaMethodInArgAllocator>>::UninitTuple>
    where
        Args: MethodArgsAllocate<LolaMethodInArgAllocator>,
    {
        // See LolaMethodInArgAllocator.
        Err(Error::MethodError(MethodFailedReason::ZeroCopyNotSupported))
    }

    #[allow(clippy::manual_async_fn)]
    fn invoke_zero_copy<'a>(
        &'a self,
        _ptrs: <Args as MethodArgsPtrTuple<LolaRuntimeImpl<B>>>::PtrTuple,
    ) -> impl Future<Output = Result<LolaMethodReturnSample<Return>>> + 'a
    where
        Args: MethodArgsPtrTuple<LolaRuntimeImpl<B>>,
    {
        // Unreachable through the public API (allocate() fails first), but must not touch any buffer.
        async move { Err(Error::MethodError(MethodFailedReason::ZeroCopyNotSupported)) }
    }
}

/// Uninhabited: no value of the zero-copy types below can exist.
enum Never {}

/// Initialized zero-copy method argument. Cannot be constructed, see `LolaMethodInArgAllocator`.
pub struct LolaMethodInArgPtr<T> {
    _never: Never,
    _phantom: PhantomData<T>,
}

impl<T> MethodInArgPtr<T> for LolaMethodInArgPtr<T> {}

/// Not yet initialized zero-copy method argument. Cannot be constructed, see `LolaMethodInArgAllocator`.
pub struct LolaMethodInArgMaybeUninit<T> {
    never: Never,
    _phantom: PhantomData<T>,
}

impl<T> MethodInArgMaybeUninit<T> for LolaMethodInArgMaybeUninit<T> {
    type Ptr = LolaMethodInArgPtr<T>;

    fn write(self, _val: T) -> ZeroCopyArgs<LolaMethodInArgPtr<T>> {
        match self.never {}
    }

    unsafe fn assume_init(self) -> ZeroCopyArgs<LolaMethodInArgPtr<T>> {
        match self.never {}
    }
}

/// Zero-copy method arguments are not supported yet: a slot would point into the C++ in-args buffer
/// without being tied to the caller's borrow, and that buffer is reused by the next call. Until the
/// concept traits can express that lifetime, `allocate()` always fails and this type cannot be
/// constructed; it exists for `Runtime::MethodInArgAllocator`.
pub struct LolaMethodInArgAllocator {
    never: Never,
}

impl MethodInArgAllocator for LolaMethodInArgAllocator {
    type MethodInArgPtr<T: CommData> = LolaMethodInArgPtr<T>;
    type MethodInArgMaybeUninit<T: CommData> = LolaMethodInArgMaybeUninit<T>;

    fn allocate<T: CommData>(&self) -> LolaMethodInArgMaybeUninit<T> {
        match self.never {}
    }
}

/// Logs and aborts when a handler buffer from C++ does not match `Args`/`Return`, mirroring the
/// assertions of the typed C++ `SkeletonMethod` handler. Never unwinds into C++.
fn abort_on_handler_buffer_mismatch(buffer: &str) -> ! {
    log::error!(
        "LolaMethodHandler: {} buffer missing or not sized for the Rust method signature; aborting",
        buffer
    );
    std::process::abort();
}

/// Keeps the shared skeleton alive (via `LolaProviderInfo`) and holds the resolved
/// `SkeletonMethodBinding*`.
pub struct LolaMethodHandler<Args: MethodArgs, Return: CommData, B: FFIBridge> {
    binding: NonNull<SkeletonMethodBinding>,
    _provider: LolaProviderInfo<B>,
    bridge: B,
    /// Set once a handler is handed to C++; later registrations are refused.
    handler_registered: AtomicBool,
    _phantom: PhantomData<(Args, Return)>,
}

// SAFETY: as for NativeSkeletonHandle (producer.rs): `binding` points into the C++ skeleton kept alive by
// `_provider`. Sync because the registered handler may run on another C++ thread.
unsafe impl<Args: MethodArgs, Return: CommData, B: FFIBridge> Send for LolaMethodHandler<Args, Return, B> {}
unsafe impl<Args: MethodArgs, Return: CommData, B: FFIBridge> Sync for LolaMethodHandler<Args, Return, B> {}

impl<Args: MethodArgs, Return: CommData, B: FFIBridge> LolaMethodHandler<Args, Return, B> {
    /// Registers `handler` unless one is already registered. `SkeletonMethod::RegisterHandler` replaces
    /// the stored handler without synchronization, and the replaced Rust box could never be freed, so only
    /// the first registration reaches C++. C++ keeps that handler across StopOfferService, so it is still
    /// served after an unoffer/offer cycle.
    fn try_register_handler<F>(&self, handler: F) -> Result<()>
    where
        F: MethodHandlerCall<Args, Return>,
    {
        if self.handler_registered.swap(true, Ordering::AcqRel) {
            return Err(Error::MethodError(MethodFailedReason::HandlerAlreadyRegistered));
        }

        let (args_size, _align) = Args::layout();
        let return_size = core::mem::size_of::<Return>();
        let void_return = is_void::<Return>();
        let boxed_handler: BoxedMethodHandlerFn = Box::new(move |_quality_type, in_args, return_buf| {
            let in_args_ptr = match in_args {
                Some((ptr, len)) if len == args_size => ptr as *const u8,
                None if args_size == 0 => core::ptr::null(),
                _ => abort_on_handler_buffer_mismatch("in-args"),
            };
            let return_ptr = match return_buf {
                Some((ptr, len)) if len == return_size => Some(ptr),
                None if void_return => None,
                _ => abort_on_handler_buffer_mismatch("return-value"),
            };
            // SAFETY: in_args_ptr is either valid for args_size bytes holding an Args value in the
            // MethodArgsCodec layout, or null with args_size == 0, which read_from never dereferences.
            let args = unsafe { Args::read_from(in_args_ptr) };
            // The C++ binding sends the reply when this callback returns, so the future is run here.
            let result: Return = futures::executor::block_on(handler.call(&args));
            if let Some(ptr) = return_ptr {
                // SAFETY: ptr is the live return-value buffer for this call, size_of::<Return>() bytes
                // (checked above) and aligned by the C++ binding.
                unsafe {
                    (ptr as *mut Return).write(result);
                }
            }
        });

        let raw = Box::into_raw(boxed_handler);
        // SAFETY: FatPtr is a binary-compatible reinterpretation of a `dyn Trait` fat pointer, as in the
        // event receive-handler path (consumer.rs).
        let fat_ptr: FatPtr = unsafe { core::mem::transmute(raw) };
        // SAFETY: self.binding is valid while self._provider is alive; fat_ptr references the closure
        // above. On success C++ keeps it for the process lifetime (SkeletonMethodBinding has no
        // unregister hook).
        let registered = unsafe {
            self.bridge
                .skeleton_method_register_handler(self.binding.as_ptr(), &fat_ptr)
        };
        if !registered {
            // C++ never took ownership, so free the closure here and allow another attempt.
            drop(unsafe { Box::from_raw(raw) });
            self.handler_registered.store(false, Ordering::Release);
            return Err(Error::MethodError(MethodFailedReason::HandlerRegistrationFailed));
        }
        Ok(())
    }
}

impl<Args: MethodArgs, Return: CommData, B: FFIBridge> MethodHandler<Args, Return, LolaRuntimeImpl<B>>
    for LolaMethodHandler<Args, Return, B>
{
    fn new(method_name: &str, instance_info: LolaProviderInfo<B>) -> Result<Self>
    where
        Self: Sized,
    {
        // SAFETY: skeleton_ptr() is valid while instance_info (kept as _provider) is alive.
        let raw_binding = unsafe {
            instance_info.bridge().get_method_from_skeleton(
                instance_info.skeleton_ptr(),
                instance_info.interface_id(),
                method_name,
            )
        };
        let binding =
            NonNull::new(raw_binding).ok_or(Error::MethodError(MethodFailedReason::MethodHandlerCreationFailed))?;

        Ok(Self {
            binding,
            bridge: instance_info.bridge().clone(),
            _provider: instance_info,
            handler_registered: AtomicBool::new(false),
            _phantom: PhantomData,
        })
    }

    fn register_handler<F>(&self, handler: F)
    where
        F: MethodHandlerCall<Args, Return>,
    {
        match self.try_register_handler(handler) {
            Ok(()) => {},
            Err(Error::MethodError(MethodFailedReason::HandlerAlreadyRegistered)) => log::warn!(
                "LolaMethodHandler::register_handler: a handler is already registered for this method; \
                 the new handler is ignored and the first one stays in use"
            ),
            Err(_) => log::error!(
                "LolaMethodHandler::register_handler: mw_com_skeleton_method_register_handler failed; \
                 the handler was NOT registered with the underlying binding"
            ),
        }
    }
}

/// Placeholder caller for field Get operations
/// It is distinct from `LolaMethodCaller<(), T>` because we may need to route field specific Method
/// via specific id file `Getter` to the Lola binding.
pub struct LolaFieldGetCaller<T: CommData + Debug, R: Runtime> {
    _phantom: core::marker::PhantomData<(T, R)>,
}

impl<T: CommData + Debug, R: Runtime> MethodCaller<(), T, R> for LolaFieldGetCaller<T, R> {
    fn new(_method_name: &str, _instance_info: R::ConsumerInfo) -> Result<Self>
    where
        Self: Sized,
    {
        Ok(LolaFieldGetCaller {
            _phantom: core::marker::PhantomData,
        })
    }

    #[allow(clippy::manual_async_fn)]
    fn invoke_with_copy<'a>(&'a self, _args: ()) -> impl Future<Output = Result<R::MethodReturnSample<T>>> + 'a {
        async move { todo!("Implement field get via MethodType::kGet") }
    }

    fn allocate(&self) -> Result<<() as MethodArgsAllocate<R::MethodInArgAllocator>>::UninitTuple>
    where
        (): MethodArgsAllocate<R::MethodInArgAllocator>,
    {
        todo!("Implement allocate for LolaFieldGetCaller")
    }

    #[allow(clippy::manual_async_fn)]
    fn invoke_zero_copy<'a>(
        &'a self,
        _ptrs: <() as MethodArgsPtrTuple<R>>::PtrTuple,
    ) -> impl Future<Output = Result<R::MethodReturnSample<T>>> + 'a
    where
        (): MethodArgsPtrTuple<R>,
    {
        async move { todo!("Implement zero-copy invoke for LolaFieldGetCaller if C++ side support is available") }
    }
}

/// Placeholder caller for field Set operations
/// It is distinct from `LolaMethodCaller<(T,), T>` because we may need to route field specific Method
/// via specific id file `Setter` to the Lola binding.
pub struct LolaFieldSetCaller<T: CommData + Debug, R: Runtime> {
    _phantom: core::marker::PhantomData<(T, R)>,
}

impl<T: CommData + Debug, R: Runtime> MethodCaller<(T,), T, R> for LolaFieldSetCaller<T, R> {
    fn new(_method_name: &str, _instance_info: R::ConsumerInfo) -> Result<Self>
    where
        Self: Sized,
    {
        Ok(LolaFieldSetCaller {
            _phantom: core::marker::PhantomData,
        })
    }

    #[allow(clippy::manual_async_fn)]
    fn invoke_with_copy<'a>(&'a self, _args: (T,)) -> impl Future<Output = Result<R::MethodReturnSample<T>>> + 'a {
        async move { todo!("Implement field set via MethodType::kSet") }
    }

    fn allocate(&self) -> Result<<(T,) as MethodArgsAllocate<R::MethodInArgAllocator>>::UninitTuple>
    where
        (T,): MethodArgsAllocate<R::MethodInArgAllocator>,
    {
        todo!("Implement allocate for LolaFieldSetCaller if C++ side support is available");
    }

    #[allow(clippy::manual_async_fn)]
    fn invoke_zero_copy<'a>(
        &'a self,
        _ptrs: <(T,) as MethodArgsPtrTuple<R>>::PtrTuple,
    ) -> impl Future<Output = Result<R::MethodReturnSample<T>>> + 'a
    where
        (T,): MethodArgsPtrTuple<R>,
    {
        async move { todo!("Implement zero-copy invoke for LolaFieldSetCaller if C++ side support is available") }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use std::sync::Arc;
    use std::task::{Context, Poll, Wake, Waker};

    use bridge_ffi_mock::{MockFFIBridge, MockPointerAllocator, SharedMockBridge};
    use bridge_ffi_rs::{HandleType, ProxyBase, SkeletonBase};
    use score_com_concept::MethodArgsCodec;

    use crate::producer::{NativeSkeletonHandle, SkeletonInstanceManager};

    #[derive(Debug, Clone, Copy, PartialEq, Eq)]
    #[repr(C)]
    struct MethodTestArg {
        value: i32,
    }

    unsafe impl score_com_concept::Reloc for MethodTestArg {}

    impl score_com_concept::CommData for MethodTestArg {
        const ID: &'static str = "MethodTestArg";
    }

    /// Same field name as `MethodTestArg` but a different size, to exercise the layout checks.
    #[derive(Debug, Clone, Copy, PartialEq, Eq)]
    #[repr(C)]
    struct MethodTestWideArg {
        value: i64,
    }

    unsafe impl score_com_concept::Reloc for MethodTestWideArg {}

    impl score_com_concept::CommData for MethodTestWideArg {
        const ID: &'static str = "MethodTestWideArg";
    }

    struct NoopWake;
    impl Wake for NoopWake {
        fn wake(self: Arc<Self>) {}
    }

    /// Polls a future once, expecting immediate completion. `invoke_with_copy`'s body has no real
    /// `.await` point (every FFI call inside it is synchronous), so it always resolves on the first
    /// poll — there is no real executor in this test, just enough of one to drive that single poll.
    fn block_on_ready<F: Future>(fut: F) -> F::Output {
        let waker = Waker::from(Arc::new(NoopWake));
        let mut cx = Context::from_waker(&waker);
        let mut fut = Box::pin(fut);
        match fut.as_mut().poll(&mut cx) {
            Poll::Ready(value) => value,
            Poll::Pending => panic!("invoke_with_copy should resolve on the first poll"),
        }
    }

    /// Copy-path round trip: `invoke_with_copy` serializes the arguments, the mocked
    /// `proxy_method_do_call` plays the C++ side (reads them back, writes a return value), and the
    /// caller reads the result. Only the FFI boundary is mocked.
    #[test]
    fn test_method_call_round_trip_copy_path() {
        let mut mock = MockFFIBridge::new();
        let mut seq = mockall::Sequence::new();

        let proxy_alloc = MockPointerAllocator::<ProxyBase>::new();
        let binding_alloc = MockPointerAllocator::<ProxyMethodBinding>::new();
        let prox = proxy_alloc.clone();

        mock.expect_create_proxy()
            .times(1)
            .in_sequence(&mut seq)
            .returning(move |_, _| prox.allocate());

        let in_args_size = <(MethodTestArg,) as MethodArgsCodec>::layout().0;
        let mut in_args_buf = vec![0u8; in_args_size];
        let mut return_buf = vec![0u8; core::mem::size_of::<MethodTestArg>()];
        // Captured as usize (not *mut u8/*mut MethodTestArg) because mockall's closures must be
        // Send, and raw pointers aren't — cast back to a pointer inside each closure instead.
        let in_args_addr = in_args_buf.as_mut_ptr() as usize;
        let in_args_len = in_args_buf.len();
        let return_addr = return_buf.as_mut_ptr() as usize;
        let return_len = return_buf.len();

        mock.expect_proxy_method_get_in_args_buffer()
            .times(1)
            .in_sequence(&mut seq)
            .returning(move |_, _| Some((in_args_addr as *mut u8, in_args_len)));
        mock.expect_proxy_method_get_return_value_buffer()
            .times(1)
            .in_sequence(&mut seq)
            .returning(move |_, _| Some((return_addr as *mut u8, return_len)));
        mock.expect_proxy_method_do_call()
            .times(1)
            .in_sequence(&mut seq)
            .returning(move |_, _| {
                // SAFETY: plays the C++ side of this call for the test: in_args_addr/return_addr were
                // just handed out (and are still live) via the two mocked buffer-accessors above, and
                // in_args_addr already holds a value invoke_with_copy serialized with this exact codec.
                let (arg,): (MethodTestArg,) =
                    unsafe { <(MethodTestArg,) as MethodArgsCodec>::read_from(in_args_addr as *const u8) };
                unsafe {
                    (return_addr as *mut MethodTestArg).write(MethodTestArg { value: arg.value * 2 });
                }
                true
            });

        let prox_cleanup = proxy_alloc.clone();
        mock.expect_destroy_proxy()
            .times(1)
            .in_sequence(&mut seq)
            .returning(move |ptr| {
                assert!(prox_cleanup.free(ptr), "destroy_proxy called with unknown pointer");
            });

        let bridge = SharedMockBridge::new(mock);
        let handle = HandleType::default();
        let proxy = NativeProxyBase::<SharedMockBridge>::new(&bridge, "TestInterface", &handle)
            .expect("NativeProxyBase::new should succeed with mock create_proxy");
        let binding = NonNull::new(binding_alloc.allocate()).expect("mock binding allocation should not fail");

        let caller = LolaMethodCaller::<(MethodTestArg,), MethodTestArg, SharedMockBridge> {
            binding,
            _proxy: proxy,
            bridge,
            call_lock: Mutex::new(()),
            _phantom: PhantomData,
        };

        let sample = block_on_ready(caller.invoke_with_copy((MethodTestArg { value: 21 },)))
            .expect("invoke_with_copy should succeed with a fully-mocked FFI round trip");
        assert_eq!(*sample, MethodTestArg { value: 42 });

        // Drops _proxy (NativeProxyBase), triggering destroy_proxy — must happen before checking
        // that the proxy pointer was freed.
        drop(caller);
        proxy_alloc.assert_all_freed();
    }

    fn make_caller<Args: MethodArgs, Return: CommData>(
        bridge: &SharedMockBridge,
        binding_alloc: &MockPointerAllocator<ProxyMethodBinding>,
    ) -> LolaMethodCaller<Args, Return, SharedMockBridge> {
        let proxy = NativeProxyBase::<SharedMockBridge>::new(bridge, "TestInterface", &HandleType::default())
            .expect("NativeProxyBase::new should succeed with mock create_proxy");
        LolaMethodCaller {
            binding: NonNull::new(binding_alloc.allocate()).expect("mock binding allocation should not fail"),
            _proxy: proxy,
            bridge: bridge.clone(),
            call_lock: Mutex::new(()),
            _phantom: PhantomData,
        }
    }

    /// Zero-copy calls are refused without touching the C++ in-args buffer: the mock has no buffer or
    /// call expectations, so any such FFI call would fail the test.
    #[test]
    fn test_method_zero_copy_is_not_supported() {
        let mut mock = MockFFIBridge::new();
        let proxy_alloc = MockPointerAllocator::<ProxyBase>::new();
        let binding_alloc = MockPointerAllocator::<ProxyMethodBinding>::new();
        let prox = proxy_alloc.clone();
        mock.expect_create_proxy()
            .times(2)
            .returning(move |_, _| prox.allocate());
        let prox_cleanup = proxy_alloc.clone();
        mock.expect_destroy_proxy().times(2).returning(move |ptr| {
            assert!(prox_cleanup.free(ptr), "destroy_proxy called with unknown pointer");
        });
        let bridge = SharedMockBridge::new(mock);

        let caller = make_caller::<(MethodTestArg,), MethodTestArg>(&bridge, &binding_alloc);
        assert!(matches!(
            caller.allocate(),
            Err(Error::MethodError(MethodFailedReason::ZeroCopyNotSupported))
        ));
        let unit_caller = make_caller::<(), MethodTestArg>(&bridge, &binding_alloc);
        assert!(matches!(
            unit_caller.allocate(),
            Err(Error::MethodError(MethodFailedReason::ZeroCopyNotSupported))
        ));
        assert!(matches!(
            block_on_ready(unit_caller.invoke_zero_copy(())),
            Err(Error::MethodError(MethodFailedReason::ZeroCopyNotSupported))
        ));

        drop(caller);
        drop(unit_caller);
        proxy_alloc.assert_all_freed();
    }

    /// In-args and return-value buffers whose size differs from the Rust `Args`/`Return` layout are
    /// rejected before anything is written or read, and the call is not made (no `proxy_method_do_call`
    /// expectation).
    #[test]
    fn test_method_call_rejects_buffer_size_mismatch() {
        let mut mock = MockFFIBridge::new();
        let proxy_alloc = MockPointerAllocator::<ProxyBase>::new();
        let binding_alloc = MockPointerAllocator::<ProxyMethodBinding>::new();
        let prox = proxy_alloc.clone();
        mock.expect_create_proxy()
            .times(1)
            .returning(move |_, _| prox.allocate());

        // C++ side: `MethodTestArg(MethodTestArg)`, i.e. 4-byte in-args and return buffers.
        let mut in_args_buf = vec![0u8; 8];
        let mut return_buf = vec![0u8; 4];
        let in_args_addr = in_args_buf.as_mut_ptr() as usize;
        let return_addr = return_buf.as_mut_ptr() as usize;
        let mut in_args_calls = 0;
        mock.expect_proxy_method_get_in_args_buffer()
            .times(2)
            .returning(move |_, _| {
                in_args_calls += 1;
                // First an 8-byte buffer for 4-byte Args, then a matching one.
                let len = if in_args_calls == 1 { 8 } else { 4 };
                Some((in_args_addr as *mut u8, len))
            });
        mock.expect_proxy_method_get_return_value_buffer()
            .times(1)
            .returning(move |_, _| Some((return_addr as *mut u8, 4)));

        let prox_cleanup = proxy_alloc.clone();
        mock.expect_destroy_proxy().times(1).returning(move |ptr| {
            assert!(prox_cleanup.free(ptr), "destroy_proxy called with unknown pointer");
        });
        let bridge = SharedMockBridge::new(mock);

        // Rust side expects an 8-byte return value.
        let caller = make_caller::<(MethodTestArg,), MethodTestWideArg>(&bridge, &binding_alloc);
        assert!(matches!(
            block_on_ready(caller.invoke_with_copy((MethodTestArg { value: 1 },))),
            Err(Error::MethodError(MethodFailedReason::InArgsBufferUnavailable))
        ));
        assert!(matches!(
            block_on_ready(caller.invoke_with_copy((MethodTestArg { value: 1 },))),
            Err(Error::MethodError(MethodFailedReason::ReturnValueBufferUnavailable))
        ));
        assert!(return_buf.iter().all(|&b| b == 0));

        drop(caller);
        proxy_alloc.assert_all_freed();
    }

    /// Only the first successful registration reaches C++: a failed attempt can be retried, a second
    /// registration after a successful one is refused without an FFI call (the mock allows exactly two
    /// calls), and the registered closure answers calls through shared references.
    #[test]
    fn test_method_handler_accepts_only_first_registration() {
        let mut mock = MockFFIBridge::new();
        let skeleton_alloc = MockPointerAllocator::<SkeletonBase>::new();
        let skel = skeleton_alloc.clone();
        mock.expect_create_skeleton()
            .times(1)
            .returning(move |_, _| skel.allocate());

        let captured: Arc<Mutex<Option<FatPtr>>> = Arc::new(Mutex::new(None));
        let cap = captured.clone();
        let mut register_calls = 0;
        mock.expect_skeleton_method_register_handler()
            .times(2)
            .returning(move |_binding, fat_ptr| {
                register_calls += 1;
                if register_calls == 1 {
                    return false;
                }
                *cap.lock().expect("lock") = Some(*fat_ptr);
                true
            });

        let skel_cleanup = skeleton_alloc.clone();
        mock.expect_destroy_skeleton().times(1).returning(move |ptr| {
            assert!(skel_cleanup.free(ptr), "destroy_skeleton called with unknown pointer");
        });

        let bridge = SharedMockBridge::new(mock);
        let spec = bridge_ffi_rs::InstanceSpecifier::try_from("/test_instance").expect("valid instance specifier");
        let skeleton_handle = NativeSkeletonHandle::<SharedMockBridge>::new(&bridge, "", &spec)
            .expect("create_skeleton mock should not fail");
        let provider = LolaProviderInfo::new_for_test(
            score_com_concept::InstanceSpecifier::new("/test_instance").expect("valid instance specifier"),
            "TestInterface",
            SkeletonInstanceManager(Arc::new(skeleton_handle)),
            bridge.clone(),
        );
        let binding_alloc = MockPointerAllocator::<SkeletonMethodBinding>::new();
        let handler = LolaMethodHandler::<(MethodTestArg,), MethodTestArg, SharedMockBridge> {
            binding: NonNull::new(binding_alloc.allocate()).expect("mock binding allocation should not fail"),
            _provider: provider,
            bridge: bridge.clone(),
            handler_registered: AtomicBool::new(false),
            _phantom: PhantomData,
        };

        let double = |arg: &MethodTestArg| core::future::ready(MethodTestArg { value: arg.value * 2 });
        assert!(matches!(
            handler.try_register_handler(double),
            Err(Error::MethodError(MethodFailedReason::HandlerRegistrationFailed))
        ));
        assert!(handler.try_register_handler(double).is_ok());
        assert!(matches!(
            handler.try_register_handler(|arg: &MethodTestArg| core::future::ready(*arg)),
            Err(Error::MethodError(MethodFailedReason::HandlerAlreadyRegistered))
        ));
        // Through the trait: logged and ignored, no FFI call.
        handler.register_handler(|arg: &MethodTestArg| core::future::ready(*arg));

        // Call the registered closure as mw_com_impl_call_method_handler (bridge_ffi_lola.rs) does.
        type Callback = dyn Fn(u8, Option<(*mut u8, usize)>, Option<(*mut u8, usize)>) + Send + Sync;
        let fat_ptr = captured
            .lock()
            .expect("lock")
            .take()
            .expect("a handler should be registered");
        // SAFETY: fat_ptr was created from a BoxedMethodHandlerFn by try_register_handler above.
        let callable: &Callback = unsafe { core::mem::transmute(fat_ptr) };
        let mut in_args = vec![0u8; <(MethodTestArg,) as MethodArgsCodec>::layout().0];
        // SAFETY: in_args holds exactly one (MethodTestArg,) in the MethodArgsCodec layout.
        unsafe { (MethodTestArg { value: 21 },).write_into(in_args.as_mut_ptr()) };
        let mut result = MethodTestArg { value: 0 };
        callable(
            0,
            Some((in_args.as_mut_ptr(), in_args.len())),
            Some((
                &mut result as *mut MethodTestArg as *mut u8,
                core::mem::size_of::<MethodTestArg>(),
            )),
        );
        assert_eq!(result, MethodTestArg { value: 42 });
        // SAFETY: no real C++ owner in this test, so the box is freed here.
        drop(unsafe { Box::from_raw(callable as *const Callback as *mut Callback) });

        drop(handler);
        skeleton_alloc.assert_all_freed();
    }
}
