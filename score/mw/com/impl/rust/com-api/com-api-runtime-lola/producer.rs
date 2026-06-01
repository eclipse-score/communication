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

//! This file implements the producer side of the COM API for LoLa runtime.
//! It defines the `Publisher` struct and its associated methods for allocating and sending samples.
//! It also includes the `SampleMut` and `SampleMaybeUninit` structs for handling sample data.
//! These components work together to enable producers to create and manage data samples
//! within the LoLa runtime environment.
//! This implementation relies on FFI calls to interact with the underlying C++ LoLa runtime.
//! The code ensures proper memory management and safety when dealing with FFI pointers
//! and data structures.
//! The `LolaProviderInfo` struct is used to manage provider information and service offering.
//! Overall, this file provides the necessary functionality for producers to operate
//! within the LoLa COM API runtime.

//lifetime warning for all the Sample struct impl block, it is required for the Sample struct event lifetime parameter
// and mentaining lifetime of instances and data reference
// As of supressing clippy::needless_lifetimes
//TODO: revist this once com-api is stable - Ticket-234827
#![allow(clippy::needless_lifetimes)]

use crate::Debug;
use core::marker::PhantomData;
use core::mem::ManuallyDrop;
use core::ops::{Deref, DerefMut};
use std::ptr::NonNull;
use std::sync::Arc;

use com_api_concept::{
    AllocationFailureReason, Builder, CommData, Error, EventFailedReason, InstanceSpecifier,
    Interface, Producer, ProducerBuilder, ProducerFailedReason, ProviderInfo, Result,
    ServiceFailedReason,
};

use bridge_ffi_rs::*;

use crate::LolaRuntimeImpl;

#[derive(Clone, Debug)]
pub struct LolaProviderInfo<B: FFIBridge> {
    //instance_specifier is currently not used in LolaProviderInfo but it will be used in future for better error handling and logging
    #[allow(unused)]
    instance_specifier: InstanceSpecifier,
    interface_id: &'static str,
    skeleton_handle: SkeletonInstanceManager<B>,
    // LolaFFIBridge (Production case) is a ZST, so cloning it is not overhead,
    // But if in future we add some state in the bridge type then we need to ensure that
    // it is properly cloned and does not cause any overhead.
    // In that case suggested to implement Arc for the type or FFIBridge itself.
    bridge: B,
}

impl<B: FFIBridge> ProviderInfo for LolaProviderInfo<B> {
    fn offer_service(&self) -> Result<()> {
        //SAFETY: it is safe as we are passing valid skeleton handle to offer service
        // the skeleton handle is created during building the provider info instance
        let status = unsafe {
            self.bridge
                .skeleton_offer_service(self.skeleton_handle.0.handle.as_ptr())
        };
        if !status {
            return Err(Error::ServiceError(ServiceFailedReason::OfferServiceFailed));
        }
        Ok(())
    }

    fn stop_offer_service(&self) -> Result<()> {
        //SAFETY: it is safe as we are passing valid skeleton handle to stop offer service
        // the skeleton handle is created during building the provider info instance
        unsafe {
            self.bridge
                .skeleton_stop_offer_service(self.skeleton_handle.0.handle.as_ptr())
        };
        Ok(())
    }
}

/// Wrapper that ensures FFI cleanup of allocatee_ptr.
/// SampleAllocateePtr is wrapped in ManuallyDrop to control when the cleanup occurs.
/// Safe to move between SampleMaybeUninit and SampleMut because
/// the Drop impl guarantees cleanup exactly once.
#[derive(Debug)]
pub struct AllocateePtrWrapper<T, B: FFIBridge>
where
    T: CommData + Debug,
{
    pub inner: ManuallyDrop<sample_allocatee_ptr_rs::SampleAllocateePtr<T>>,
    pub bridge: B,
}

impl<T, B: FFIBridge> Drop for AllocateePtrWrapper<T, B>
where
    T: CommData + Debug,
{
    fn drop(&mut self) {
        //SAFETY: It is safe to call the delete function because allocatee_ptr is valid
        //SampleAllocateePtr created by FFI
        unsafe {
            let mut allocatee_ptr = ManuallyDrop::take(&mut self.inner);
            self.bridge.delete_allocatee_ptr(
                std::ptr::from_mut(&mut allocatee_ptr) as *mut std::ffi::c_void,
                T::ID,
            );
        }
    }
}

impl<T, B: FFIBridge> AsRef<sample_allocatee_ptr_rs::SampleAllocateePtr<T>>
    for AllocateePtrWrapper<T, B>
where
    T: CommData + Debug,
{
    fn as_ref(&self) -> &sample_allocatee_ptr_rs::SampleAllocateePtr<T> {
        &self.inner
    }
}

#[derive(Debug)]
pub struct SampleMut<'a, T, B: FFIBridge>
where
    T: CommData + Debug,
{
    skeleton_event: NativeSkeletonEventBase,
    allocatee_ptr: AllocateePtrWrapper<T, B>,
    lifetime: PhantomData<&'a T>,
}

impl<'a, T, B: FFIBridge> SampleMut<'a, T, B>
where
    T: CommData + Debug,
{
    fn get_allocatee_data_ptr_const(&self) -> Option<&T> {
        //SAFETY: allocatee_ptr is valid which is created using get_allocatee_ptr() and
        // it will be again type casted to T type pointer in cpp side so valid to send as void pointer
        unsafe {
            let data_ptr = self.allocatee_ptr.bridge.get_allocatee_data_ptr(
                std::ptr::from_ref(&(*self.allocatee_ptr.inner)) as *const std::ffi::c_void,
                T::ID,
            );
            (data_ptr as *const T).as_ref()
        }
    }

    fn get_allocatee_data_ptr_mut(&mut self) -> Option<&mut T> {
        //SAFETY: allocatee_ptr is valid which is created using get_allocatee_ptr() and
        // it will be again type casted to T type pointer in cpp side so valid to send as void pointer
        unsafe {
            let data_ptr = self.allocatee_ptr.bridge.get_allocatee_data_ptr(
                std::ptr::from_mut(&mut (*self.allocatee_ptr.inner)) as *mut std::ffi::c_void,
                T::ID,
            );
            (data_ptr as *mut T).as_mut()
        }
    }
}

impl<'a, T, B: FFIBridge> Deref for SampleMut<'a, T, B>
where
    T: CommData + Debug,
{
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.get_allocatee_data_ptr_const()
            .expect("Allocatee data pointer is null")
    }
}

impl<'a, T, B: FFIBridge> DerefMut for SampleMut<'a, T, B>
where
    T: CommData + Debug,
{
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.get_allocatee_data_ptr_mut()
            .expect("Allocatee data pointer is null")
    }
}

impl<'a, T, B: FFIBridge> com_api_concept::SampleMut<T> for SampleMut<'a, T, B>
where
    T: CommData + Debug,
{
    fn send(self) -> Result<()> {
        //SAFETY: It is safe to send the sample because allocatee_ptr and skeleton_event are valid
        // allocatee_ptr is created by FFI and skeleton_event is valid as long as the parent skeleton instance is valid
        // We've taken ownership via self (consumed, not borrowed), and
        // FFI call will complete before drop run on AllocateePtrWrapper and NativeSkeletonEventBase
        let status = unsafe {
            self.allocatee_ptr
                .bridge
                .skeleton_event_send_sample_allocatee(
                    self.skeleton_event.skeleton_event_ptr.as_ptr(),
                    T::ID,
                    std::ptr::from_ref(self.allocatee_ptr.as_ref()) as *const std::ffi::c_void,
                )
        };
        if !status {
            return Err(Error::EventError(EventFailedReason::SendingDataFailed));
        }
        Ok(())
    }
}

#[derive(Debug)]
pub struct SampleMaybeUninit<'a, T, B: FFIBridge>
where
    T: CommData + Debug,
{
    skeleton_event: NativeSkeletonEventBase,
    allocatee_ptr: AllocateePtrWrapper<T, B>,
    lifetime: PhantomData<&'a T>,
}

impl<'a, T, B: FFIBridge> SampleMaybeUninit<'a, T, B>
where
    T: CommData + Debug,
{
    fn get_allocatee_data_ptr(&mut self) -> Option<&mut core::mem::MaybeUninit<T>> {
        //SAFETY: allocatee_ptr is valid which is created using get_allocatee_ptr() and
        // it will be again type casted to T type pointer in cpp side so valid to send as void pointer
        let data_ptr = unsafe {
            self.allocatee_ptr.bridge.get_allocatee_data_ptr(
                std::ptr::from_ref(self.allocatee_ptr.as_ref()) as *const std::ffi::c_void,
                T::ID,
            ) as *mut core::mem::MaybeUninit<T>
        };
        unsafe { data_ptr.as_mut() }
    }
}

impl<'a, T, B: FFIBridge> com_api_concept::SampleMaybeUninit<T> for SampleMaybeUninit<'a, T, B>
where
    T: CommData + Debug,
{
    type SampleMut = SampleMut<'a, T, B>;

    fn write(mut self, val: T) -> SampleMut<'a, T, B> {
        let data_ptr = self
            .get_allocatee_data_ptr()
            .expect("Allocatee data pointer is null");

        //It is safe to write the value because data_ptr is valid
        // and we are writing the value of type T which is same as allocatee_ptr type
        data_ptr.write(val);

        SampleMut {
            skeleton_event: self.skeleton_event,
            allocatee_ptr: self.allocatee_ptr,
            lifetime: PhantomData,
        }
    }

    unsafe fn assume_init(self) -> SampleMut<'a, T, B> {
        SampleMut {
            skeleton_event: self.skeleton_event,
            allocatee_ptr: self.allocatee_ptr,
            lifetime: PhantomData,
        }
    }
}

impl<'a, T, B: FFIBridge> AsMut<core::mem::MaybeUninit<T>> for SampleMaybeUninit<'a, T, B>
where
    T: CommData + Debug,
{
    fn as_mut(&mut self) -> &mut core::mem::MaybeUninit<T> {
        self.get_allocatee_data_ptr()
            .expect("Allocatee data pointer is null")
    }
}

/// Manages the lifetime of the native skeleton instance, user should clone this to share between threads
/// Always use this struct to manage the skeleton instance pointer
pub struct SkeletonInstanceManager<B: FFIBridge>(pub Arc<NativeSkeletonHandle<B>>);

impl<B: FFIBridge> Clone for SkeletonInstanceManager<B> {
    fn clone(&self) -> Self {
        Self(Arc::clone(&self.0))
    }
}

impl<B: FFIBridge> std::fmt::Debug for SkeletonInstanceManager<B> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("SkeletonInstanceManager").finish()
    }
}

/// This type contains the native skeleton handle for a specific service instance
/// Manages the lifetime of the SkeletonBase pointer with new and drop implementations
/// It does not provide any mutable access to the underlying skeleton handle
/// And it does not provide any method to access the skeleton handle directly
/// Or to perform any operation on it
/// If any additional method is required to be added, ensure that the safety of the skeleton handle is maintained
/// And the lifetime is managed correctly
/// As it has Send and Sync unsafe impls, it must not expose any mutable access to the skeleton handle
pub struct NativeSkeletonHandle<B: FFIBridge> {
    handle: NonNull<SkeletonBase>,
    bridge: B,
}

//SAFETY: NativeSkeletonHandle is safe to share between threads because:
// It is created by FFI call and no mutable access is provided to the underlying skeleton handle
// Access is controlled through Arc which provides atomic reference counting
// The skeleton lifetime is managed safely through new and Drop
unsafe impl<B: FFIBridge> Sync for NativeSkeletonHandle<B> {}
unsafe impl<B: FFIBridge> Send for NativeSkeletonHandle<B> {}

impl<B: FFIBridge> NativeSkeletonHandle<B> {
    pub fn new(
        bridge: &B,
        interface_id: &str,
        instance_specifier: &mw_com::InstanceSpecifier,
    ) -> Result<Self> {
        //SAFETY: It is safe as we are passing valid type id and instance specifier to create skeleton
        let raw_handle =
            unsafe { bridge.create_skeleton(interface_id, instance_specifier.as_native()) };
        let handle = std::ptr::NonNull::new(raw_handle).ok_or(Error::ProducerError(
            ProducerFailedReason::SkeletonCreationFailed,
        ))?;
        Ok(Self {
            handle,
            bridge: bridge.clone(),
        })
    }
}

impl<B: FFIBridge> Drop for NativeSkeletonHandle<B> {
    fn drop(&mut self) {
        //SAFETY: It is safe as we are passing valid skeleton handle to destroy skeleton
        // the handle was created using create_skeleton
        unsafe {
            self.bridge.destroy_skeleton(self.handle.as_ptr());
        }
    }
}

/// This type contains the skeleton event pointer for a specific event identifier
/// Manages the lifetime of the SkeletonEventBase pointer
/// Drop is not required as the skeleton event lifetime is managed by skeleton instance
pub struct NativeSkeletonEventBase {
    skeleton_event_ptr: NonNull<SkeletonEventBase>,
}

//SAFETY: NativeSkeletonEventBase is safe to send between threads because:
// It is created by FFI call and there is no state associated with the current thread.
// The skeleton event pointer can be safely moved to another thread without thread-local concerns.
// And the skeleton event lifetime is managed safely through Drop of the parent skeleton instance
unsafe impl Send for NativeSkeletonEventBase {}

impl NativeSkeletonEventBase {
    pub fn new<B: FFIBridge>(
        instance_info: &LolaProviderInfo<B>,
        identifier: &str,
    ) -> Result<Self> {
        //SAFETY: It is safe as we are passing valid skeleton handle and interface id to get event
        // skeleton handle is created during producer offer call
        let raw_event_ptr = unsafe {
            instance_info.bridge.get_event_from_skeleton(
                instance_info.skeleton_handle.0.handle.as_ptr(),
                instance_info.interface_id,
                identifier,
            )
        };
        let skeleton_event_ptr = std::ptr::NonNull::new(raw_event_ptr).ok_or(
            Error::ProducerError(ProducerFailedReason::SkeletonCreationFailed),
        )?;
        Ok(Self { skeleton_event_ptr })
    }
}

impl Clone for NativeSkeletonEventBase {
    fn clone(&self) -> Self {
        Self {
            skeleton_event_ptr: self.skeleton_event_ptr,
        }
    }
}

impl std::fmt::Debug for NativeSkeletonEventBase {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.debug_struct("NativeSkeletonEventBase").finish()
    }
}

#[derive(Debug)]
pub struct Publisher<T, B: FFIBridge> {
    skeleton_event: NativeSkeletonEventBase,
    _data: PhantomData<T>,
    skeleton_instance: SkeletonInstanceManager<B>,
}

impl<T, B: FFIBridge> com_api_concept::Publisher<T, LolaRuntimeImpl<B>> for Publisher<T, B>
where
    T: CommData + Debug,
{
    type SampleMaybeUninit<'a>
        = SampleMaybeUninit<'a, T, B>
    where
        Self: 'a;

    fn allocate<'a>(&'a self) -> Result<Self::SampleMaybeUninit<'a>> {
        //SAFETY: It is safe to get the allocatee ptr because skeleton_event is valid
        // skeleton_event is created during publisher creation and valid as long as publisher is valid
        // T::ID is valid as it is associated with CommData type
        // allocatee_ptr is same type pointer which is allocated for T type and
        // it will be constructed in cpp side and moved back to rust side
        let allocatee_ptr = unsafe {
            let mut sample =
                core::mem::MaybeUninit::<sample_allocatee_ptr_rs::SampleAllocateePtr<T>>::uninit();
            let status = self.skeleton_instance.0.bridge.get_allocatee_ptr(
                self.skeleton_event.skeleton_event_ptr.as_ptr(),
                sample.as_mut_ptr() as *mut std::ffi::c_void,
                T::ID,
            );
            if !status {
                return Err(Error::AllocateError(
                    AllocationFailureReason::AllocationToSharedMemoryFailed,
                ));
            }
            sample.assume_init()
        };

        Ok(SampleMaybeUninit {
            skeleton_event: self.skeleton_event.clone(),
            allocatee_ptr: AllocateePtrWrapper {
                inner: ManuallyDrop::new(allocatee_ptr),
                bridge: self.skeleton_instance.0.bridge.clone(),
            },
            lifetime: PhantomData,
        })
    }

    fn new(identifier: &str, instance_info: LolaProviderInfo<B>) -> Result<Self> {
        let skeleton_event = NativeSkeletonEventBase::new::<B>(&instance_info, identifier)?;
        Ok(Self {
            skeleton_event,
            _data: PhantomData,
            skeleton_instance: instance_info.skeleton_handle.clone(),
        })
    }
}

pub struct LolaProducerBuilder<I: Interface, B: FFIBridge> {
    pub instance_specifier: InstanceSpecifier,
    pub _interface: PhantomData<I>,
    pub bridge: B,
}

impl<I: Interface, B: FFIBridge> LolaProducerBuilder<I, B> {
    pub fn new(runtime: &LolaRuntimeImpl<B>, instance_specifier: InstanceSpecifier) -> Self {
        Self {
            instance_specifier,
            _interface: PhantomData,
            bridge: runtime.bridge.clone(),
        }
    }
}

impl<I: Interface, B: FFIBridge> ProducerBuilder<I, LolaRuntimeImpl<B>>
    for LolaProducerBuilder<I, B>
{
}

impl<I: Interface, B: FFIBridge> Builder<I::Producer<LolaRuntimeImpl<B>>>
    for LolaProducerBuilder<I, B>
{
    fn build(self) -> Result<I::Producer<LolaRuntimeImpl<B>>> {
        //Once FFI layer error handling is in place (SWP-253124), we should convert this error to a proper FFI error instead of using map_err here
        let instance_specifier_runtime = mw_com::InstanceSpecifier::try_from(
            self.instance_specifier.as_ref(),
        )
        .map_err(|_| Error::ProducerError(ProducerFailedReason::InstanceSpecifierInvalid))?;

        let skeleton_handle = NativeSkeletonHandle::<B>::new(
            &self.bridge,
            I::INTERFACE_ID,
            &instance_specifier_runtime,
        )?;
        let instance_info = LolaProviderInfo {
            instance_specifier: self.instance_specifier,
            interface_id: I::INTERFACE_ID,
            skeleton_handle: SkeletonInstanceManager::<B>(Arc::new(skeleton_handle)),
            bridge: self.bridge,
        };

        I::Producer::new(instance_info)
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use bridge_ffi_mock::{MockFFIBridge, SharedMockBridge};
    use com_api_concept::InstanceSpecifier;
    use mockall::predicate::*;
    // Bring trait methods into scope without shadowing local struct names.
    use com_api_concept::Publisher as _;
    use com_api_concept::SampleMaybeUninit as _;
    use com_api_concept::SampleMut as _;

    #[derive(Debug)]
    #[repr(C)]
    struct TestData {
        value: i32,
    }

    unsafe impl com_api_concept::Reloc for TestData {}

    impl com_api_concept::CommData for TestData {
        const ID: &'static str = "TestData";
    }

    // Creates a `NativeSkeletonHandle<SharedMockBridge>` .
    fn make_skeleton_handle(bridge: &SharedMockBridge) -> NativeSkeletonHandle<SharedMockBridge> {
        let spec = mw_com::InstanceSpecifier::try_from("/test_instance")
            .expect("valid instance specifier");
        NativeSkeletonHandle::<SharedMockBridge>::new(&bridge, "", &spec)
            .expect("SharedMockBridge::create_skeleton should not fail")
    }

    // Creates a `LolaProviderInfo<SharedMockBridge>` with a valid heap-backed skeleton handle.
    fn make_provider_info(
        interface_id: &'static str,
        bridge: &SharedMockBridge,
    ) -> LolaProviderInfo<SharedMockBridge> {
        LolaProviderInfo {
            instance_specifier: InstanceSpecifier::new("/test_instance")
                .expect("valid instance specifier"),
            interface_id,
            skeleton_handle: SkeletonInstanceManager(Arc::new(make_skeleton_handle(&bridge))),
            bridge: bridge.clone(),
        }
    }

    #[test]
    fn test_provider_info_offer_service() {
        let mut mock = MockFFIBridge::new();
        mock.expect_create_skeleton()
            .returning(|_, _| Box::into_raw(Box::default()));
        mock.expect_skeleton_offer_service().returning(|_| true);
        mock.expect_skeleton_stop_offer_service().returning(|_| ());
        mock.expect_destroy_skeleton().returning(|_| ());
        let bridge = SharedMockBridge::new(mock);
        let provider_info = make_provider_info("TestInterface", &bridge);
        assert!(
            provider_info.offer_service().is_ok(),
            "offer_service should succeed when the mock returns a non-null skeleton sentinel"
        );
        assert!(
            provider_info.stop_offer_service().is_ok(),
            "stop_offer_service should always succeed in the mock"
        );
    }

    #[test]
    fn test_publisher_allocate_and_send() {
        let mut mock = MockFFIBridge::new();
        mock.expect_create_skeleton()
            .returning(|_, _| Box::into_raw(Box::default()));
        mock.expect_get_event_from_skeleton()
            .returning(|_, _, _| Box::into_raw(Box::default()));
        mock.expect_get_allocatee_ptr()
            .returning(|_, allocatee_ptr, _| {
                // SAFETY: Write a zeroed SampleAllocateePtr to the out-parameter for testing
                unsafe {
                    std::ptr::write(
                        allocatee_ptr as *mut sample_allocatee_ptr_rs::SampleAllocateePtr<TestData>,
                        std::mem::zeroed(),
                    );
                }
                true
            });
        mock.expect_get_allocatee_data_ptr().returning(move |_, _| {
            // Return a valid heap-allocated TestData pointer for the test to write to
            Box::into_raw(Box::new(TestData { value: 0 })) as *mut std::ffi::c_void
        });
        mock.expect_skeleton_event_send_sample_allocatee()
            .returning(|_, _, _| true);
        mock.expect_delete_allocatee_ptr().returning(|_, _| ());
        mock.expect_destroy_skeleton().returning(|_| ());

        let bridge = SharedMockBridge::new(mock);

        let spec = mw_com::InstanceSpecifier::try_from("/test_instance")
            .expect("valid instance specifier");
        let skeleton_handle =
            NativeSkeletonHandle::<SharedMockBridge>::new(&bridge, "TestData", &spec)
                .expect("SharedMockBridge::create_skeleton should not fail");

        let instance_info = LolaProviderInfo {
            instance_specifier: InstanceSpecifier::new("/test_instance")
                .expect("valid instance specifier"),
            interface_id: "TestData",
            skeleton_handle: SkeletonInstanceManager(Arc::new(skeleton_handle)),
            bridge: bridge.clone(),
        };

        let publisher: Publisher<TestData, SharedMockBridge> =
            Publisher::<TestData, SharedMockBridge>::new("TestEvent", instance_info)
                .expect("Failed to create publisher");
        let sample = publisher.allocate().expect("Failed to allocate sample");
        let test_data = TestData { value: 42 };
        let sample_mut = sample.write(test_data);
        assert!(sample_mut.send().is_ok(), "Failed to send sample");
    }
}
