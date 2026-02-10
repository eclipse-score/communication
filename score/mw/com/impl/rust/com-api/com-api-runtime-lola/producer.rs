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

use core::marker::PhantomData;
use core::mem::ManuallyDrop;
use core::ops::{Deref, DerefMut};
use std::sync::Arc;

use com_api_concept::{
    Builder, CommData, Error, InstanceSpecifier, Interface, Producer, ProducerBuilder,
    ProviderInfo, Result,
};

use bridge_ffi_rs::*;

use crate::LolaRuntimeImpl;

#[derive(Clone, Debug)]
pub struct LolaProviderInfo {
    pub instance_specifier: InstanceSpecifier,
    pub interface_id: &'static str,
    pub skeleton_handle: SkeletonInstanceManager,
}

impl ProviderInfo for LolaProviderInfo {
    fn offer_service(&self) -> Result<()> {
        //SAFETY: it is safe as we are passing valid skeleton handle to offer service
        // the skeleton handle is created during building the provider info instance
        let status =
            unsafe { bridge_ffi_rs::skeleton_offer_service(self.skeleton_handle.0.handle) };
        if !status {
            return Err(Error::Fail);
        }
        Ok(())
    }

    fn stop_offer_service(&self) -> Result<()> {
        //SAFETY: it is safe as we are passing valid skeleton handle to stop offer service
        // the skeleton handle is created during building the provider info instance
        unsafe { bridge_ffi_rs::skeleton_stop_offer_service(self.skeleton_handle.0.handle) };
        Ok(())
    }
}

/// Wrapper that ensures FFI cleanup of allocatee_ptr.
/// SampleAllocateePtr is wrapped in ManuallyDrop to control when the cleanup occurs.
/// Safe to move between SampleMaybeUninit and SampleMut because
/// the Drop impl guarantees cleanup exactly once.
#[derive(Debug)]
pub struct AllocateePtrWrapper<T>
where
    T: CommData,
{
    pub inner: ManuallyDrop<sample_allocatee_ptr_rs::SampleAllocateePtr<T>>,
}

impl<T> Drop for AllocateePtrWrapper<T>
where
    T: CommData,
{
    fn drop(&mut self) {
        //SAFETY: It is safe to call the delete function because allocatee_ptr is valid
        //SampleAllocateePtr created by FFI
        unsafe {
            let mut allocatee_ptr = ManuallyDrop::take(&mut self.inner);
            bridge_ffi_rs::delete_allocatee_ptr(
                std::ptr::from_mut(&mut allocatee_ptr) as *mut std::ffi::c_void,
                T::ID,
            );
        }
    }
}

impl<T> AsRef<sample_allocatee_ptr_rs::SampleAllocateePtr<T>> for AllocateePtrWrapper<T>
where
    T: CommData,
{
    fn as_ref(&self) -> &sample_allocatee_ptr_rs::SampleAllocateePtr<T> {
        &self.inner
    }
}

#[derive(Debug)]
pub struct SampleMut<'a, T>
where
    T: CommData,
{
    pub skeleton_event: NativeSkeletonEventBase,
    pub allocatee_ptr: AllocateePtrWrapper<T>,
    pub lifetime: PhantomData<&'a T>,
}

impl<'a, T> SampleMut<'a, T>
where
    T: CommData,
{
    fn get_allocatee_data_ptr_const(&self) -> Option<&T> {
        //SAFETY: allocatee_ptr is valid which is created using get_allocatee_ptr() and
        // it will be again type casted to T type pointer in cpp side so valid to send as void pointer
        unsafe {
            let data_ptr = bridge_ffi_rs::get_allocatee_data_ptr(
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
            let data_ptr = bridge_ffi_rs::get_allocatee_data_ptr(
                std::ptr::from_mut(&mut (*self.allocatee_ptr.inner)) as *mut std::ffi::c_void,
                T::ID,
            );
            (data_ptr as *mut T).as_mut()
        }
    }
}

impl<'a, T> Deref for SampleMut<'a, T>
where
    T: CommData,
{
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.get_allocatee_data_ptr_const()
            .expect("Allocatee data pointer is null")
    }
}

impl<'a, T> DerefMut for SampleMut<'a, T>
where
    T: CommData,
{
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.get_allocatee_data_ptr_mut()
            .expect("Allocatee data pointer is null")
    }
}

impl<'a, T> com_api_concept::SampleMut<T> for SampleMut<'a, T>
where
    T: CommData,
{
    fn send(self) -> Result<()> {
        //SAFETY: It is safe to send the sample because allocatee_ptr and skeleton_event are valid
        // allocatee_ptr is created by FFI and skeleton_event is valid as long as the parent skeleton instance is valid
        // We've taken ownership via self (consumed, not borrowed), and
        // FFI call will complete before drop run on AllocateePtrWrapper and NativeSkeletonEventBase
        let status = unsafe {
            bridge_ffi_rs::skeleton_event_send_sample_allocatee(
                self.skeleton_event.skeleton_event_ptr,
                T::ID,
                std::ptr::from_ref(self.allocatee_ptr.as_ref()) as *const std::ffi::c_void,
            )
        };
        if !status {
            return Err(Error::Fail);
        }
        Ok(())
    }
}

#[derive(Debug)]
pub struct SampleMaybeUninit<'a, T>
where
    T: CommData,
{
    pub skeleton_event: NativeSkeletonEventBase,
    pub allocatee_ptr: AllocateePtrWrapper<T>,
    pub lifetime: PhantomData<&'a T>,
}

impl<'a, T> SampleMaybeUninit<'a, T>
where
    T: CommData,
{
    fn get_allocatee_data_ptr(&self) -> Option<&mut core::mem::MaybeUninit<T>> {
        //SAFETY: allocatee_ptr is valid which is created using get_allocatee_ptr() and
        // it will be again type casted to T type pointer in cpp side so valid to send as void pointer
        let data_ptr = unsafe {
            bridge_ffi_rs::get_allocatee_data_ptr(
                std::ptr::from_ref(self.allocatee_ptr.as_ref()) as *const std::ffi::c_void,
                T::ID,
            ) as *mut core::mem::MaybeUninit<T>
        };
        unsafe { data_ptr.as_mut() }
    }
}

impl<'a, T> com_api_concept::SampleMaybeUninit<T> for SampleMaybeUninit<'a, T>
where
    T: CommData,
{
    type SampleMut = SampleMut<'a, T>;

    fn write(self, val: T) -> SampleMut<'a, T> {
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

    unsafe fn assume_init(self) -> SampleMut<'a, T> {
        SampleMut {
            skeleton_event: self.skeleton_event,
            allocatee_ptr: self.allocatee_ptr,
            lifetime: PhantomData,
        }
    }
}

impl<'a, T> AsMut<core::mem::MaybeUninit<T>> for SampleMaybeUninit<'a, T>
where
    T: CommData,
{
    fn as_mut(&mut self) -> &mut core::mem::MaybeUninit<T> {
        self.get_allocatee_data_ptr()
            .expect("Allocatee data pointer is null")
    }
}

/// Manages the lifetime of the native skeleton instance, user should clone this to share between threads
/// Always use this struct to manage the skeleton instance pointer
pub struct SkeletonInstanceManager(pub Arc<NativeSkeletonHandle>);

impl Clone for SkeletonInstanceManager {
    fn clone(&self) -> Self {
        Self(Arc::clone(&self.0))
    }
}

impl std::fmt::Debug for SkeletonInstanceManager {
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
pub struct NativeSkeletonHandle {
    pub handle: *mut SkeletonBase,
}

//SAFETY: NativeSkeletonHandle is safe to share between threads because:
// It is created by FFI call and no mutable access is provided to the underlying skeleton handle
// Access is controlled through Arc which provides atomic reference counting
// The skeleton lifetime is managed safely through new and Drop
unsafe impl Sync for NativeSkeletonHandle {}
unsafe impl Send for NativeSkeletonHandle {}

impl NativeSkeletonHandle {
    pub fn new(interface_id: &str, instance_specifier: &mw_com::InstanceSpecifier) -> Self {
        //SAFETY: It is safe as we are passing valid type id and instance specifier to create skeleton
        let handle =
            unsafe { bridge_ffi_rs::create_skeleton(interface_id, instance_specifier.as_native()) };
        Self { handle }
    }
}

impl Drop for NativeSkeletonHandle {
    fn drop(&mut self) {
        //SAFETY: It is safe as we are passing valid skeleton handle to destroy skeleton
        // the handle was created using create_skeleton
        unsafe {
            bridge_ffi_rs::destroy_skeleton(self.handle);
        }
    }
}

/// This type contains the skeleton event pointer for a specific event identifier
/// Manages the lifetime of the SkeletonEventBase pointer
/// Drop is not required as the skeleton event lifetime is managed by skeleton instance
pub struct NativeSkeletonEventBase {
    pub skeleton_event_ptr: *mut SkeletonEventBase,
}

//SAFETY: NativeSkeletonEventBase is safe to send between threads because:
// It is created by FFI call and there is no state associated with the current thread.
// The skeleton event pointer can be safely moved to another thread without thread-local concerns.
// And the skeleton event lifetime is managed safely through Drop of the parent skeleton instance
unsafe impl Send for NativeSkeletonEventBase {}

impl NativeSkeletonEventBase {
    pub fn new(instance_info: &LolaProviderInfo, identifier: &str) -> Self {
        //SAFETY: It is safe as we are passing valid skeleton handle and interface id to get event
        // skeleton handle is created during producer offer call
        let skeleton_event_ptr = unsafe {
            bridge_ffi_rs::get_event_from_skeleton(
                instance_info.skeleton_handle.0.handle,
                instance_info.interface_id,
                identifier,
            )
        };
        Self { skeleton_event_ptr }
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
pub struct Publisher<T> {
    pub identifier: String,
    pub skeleton_event: NativeSkeletonEventBase,
    pub _data: PhantomData<T>,
    pub _skeleton_instance: SkeletonInstanceManager,
}

impl<T> com_api_concept::Publisher<T, LolaRuntimeImpl> for Publisher<T>
where
    T: CommData,
{
    type SampleMaybeUninit<'a>
        = SampleMaybeUninit<'a, T>
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
            let status = bridge_ffi_rs::get_allocatee_ptr(
                self.skeleton_event.skeleton_event_ptr,
                sample.as_mut_ptr() as *mut std::ffi::c_void,
                T::ID,
            );
            if !status {
                return Err(Error::Fail);
            }
            sample.assume_init()
        };

        Ok(SampleMaybeUninit {
            skeleton_event: self.skeleton_event.clone(),
            allocatee_ptr: AllocateePtrWrapper {
                inner: ManuallyDrop::new(allocatee_ptr),
            },
            lifetime: PhantomData,
        })
    }

    fn new(identifier: &str, instance_info: LolaProviderInfo) -> Result<Self> {
        let skeleton_event = NativeSkeletonEventBase::new(&instance_info, identifier);

        Ok(Self {
            identifier: identifier.to_string(),
            skeleton_event,
            _data: PhantomData,
            _skeleton_instance: instance_info.skeleton_handle.clone(),
        })
    }
}

pub struct SampleProducerBuilder<I: Interface> {
    pub instance_specifier: InstanceSpecifier,
    pub _interface: PhantomData<I>,
}

impl<I: Interface> SampleProducerBuilder<I> {
    pub fn new(_runtime: &LolaRuntimeImpl, instance_specifier: InstanceSpecifier) -> Self {
        Self {
            instance_specifier,
            _interface: PhantomData,
        }
    }
}

impl<I: Interface> ProducerBuilder<I, LolaRuntimeImpl> for SampleProducerBuilder<I> {}

impl<I: Interface> Builder<I::Producer<LolaRuntimeImpl>> for SampleProducerBuilder<I> {
    fn build(self) -> Result<I::Producer<LolaRuntimeImpl>> {
        let instance_specifier_runtime =
            mw_com::InstanceSpecifier::try_from(self.instance_specifier.as_ref())
                .map_err(|_| Error::Fail)?;

        let skeleton_handle =
            NativeSkeletonHandle::new(I::INTERFACE_ID, &instance_specifier_runtime);

        let instance_info = LolaProviderInfo {
            instance_specifier: self.instance_specifier,
            interface_id: I::INTERFACE_ID,
            skeleton_handle: SkeletonInstanceManager(Arc::new(skeleton_handle)),
        };

        I::Producer::new(instance_info).map_err(|_| Error::Fail)
    }
}

#[cfg(test)]
mod test {

    #[test]
    fn send_stuff() {
        let test_publisher = super::Publisher::<u32>::new();
        let sample = test_publisher.allocate().expect("Couldn't allocate sample");
        let sample = sample.write(42);
        sample.send().expect("Send failed for sample");
    }
}
