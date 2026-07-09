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

// TODO: Need to discuss:
// 1.Get and Set methods for field it is enabled based on tag, do we want to keep same kind of mechanism
// or by default we will enable for user,
// -> My suggestion is we can keep it default enable as of now,
// and later we can add tag based mechanism if required because Interface side we need to check how we can do this

// Note: We are using the event related trait as a base trait for whereever we have same common
// APIs or functionality, as of now there are derived from concept crate but
// we will create a module which will have common trait for event and field which will be used by both event and field as a super trait and
// for this we need to create marker trait for event.

use crate::*;
use std::fmt::Debug;

#[allow(dead_code)]

// Temp for build test
// We will remove this once memory layout of same created in rust side like SamplePtr.
pub struct MethodReturnTypePtr<T: CommData + Debug> {
    pub value: T,
    pub status: Result<()>,
}

/// FieldSubscriber trait is used to subscribe to a field and get the value of the field.
/// It provides the `get` and `set` methods to get and set the value of the field.
/// It derived from `com_api_concept::Subscriber` trait which provides the `subscribe` method to create a field subscription.
/// `FieldMethods` trait is used to provide the `get` and `set` methods for the field instance which can be used before subscription.
/// Event related APIs follow the same restriction for concurrent access.
pub trait FieldSubscriber<T: CommData + Debug, R: Runtime + ?Sized>:
    com_api_concept::Subscriber<T, R, Subscription: FieldSubscription<T, R>>
{
    type FieldMethodsInstance: FieldMethods<T, R>;

    fn get_field_method_instance(&self) -> Self::FieldMethodsInstance;
}

/// FieldSubscriber trait is provides the receiving APIs for the field subscription and
/// it is derived from `com_api_concept::Subscription` trait which provides the receiving APIs for the field subscription.
/// Additional methods which the field subscription provides are added in this trait.
/// `FieldMethods` trait is used to provide the `get` and `set` methods for the field subscription.
pub trait FieldSubscription<T: CommData + Debug, R: Runtime + ?Sized>:
    com_api_concept::Subscription<T, R>
{
    /// Returns the number of new samples a call to try_receive (given parameter max_num_samples
    /// doesn't restrict it) would currently provide.
    /// How many new sample available for the user of this field subscription to receive.
    fn get_num_new_samples_available(&self) -> Result<usize>;

    /// Get the number of samples that can still be received by the user of this field.
    /// This is for checking the capacity of the field subscription and to avoid overflow of the field subscription limit.
    fn get_free_sample_count(&self) -> Result<usize>;
}

/// FieldMethods trait provides the `get` and `set` methods for the field instance which can be used before subscription.
/// The `get` method is used to get the current value of the field, and the `set` method is used to set the value of the field.
pub trait FieldMethods<T: CommData + Debug, R: Runtime + ?Sized> {
    ///Get the current value of the field.
    ///
    /// #returns
    /// Return the result of `MethodReturnTypePtr<T>` which contains the current value of the field.
    fn get(&self) -> Result<MethodReturnTypePtr<T>>;

    ///Set the value of the field.
    ///
    /// # Parameters
    /// * `value` - The value to set for the field.
    ///
    /// # Returns
    /// Return the result of `MethodReturnTypePtr<T>` which contains the status of the set operation.
    /// with the current value of the field.
    // TODO: should we pass value by reference as underlying API is taking value as reference.
    fn set(&self, value: T) -> Result<MethodReturnTypePtr<T>>;
}

/// FieldPublisher trait is used to publish a field and update the value of the field.
//  Note:  We can not use publisher trait from event because that contains the Send Method which is not correct semantic for field.
pub trait FieldPublisher<T: CommData + Debug, R: Runtime + ?Sized>: Clone {
    type FieldSample<'a>: FieldSampleMut<T>
    where
        Self: 'a;

    type SampleMaybeUninit<'a>: SampleMaybeUninit<T, SampleMut = Self::FieldSample<'a>> + 'a
    where
        Self: 'a;

    /// Create a new publisher for the specified event source.
    fn new(identifier: &str, instance_info: R::ProviderInfo) -> Result<Self>
    where
        Self: Sized;

    /// Get the allocate sample ptr for the field publisher.
    fn allocate(&self) -> Result<Self::SampleMaybeUninit<'_>>;

    /// Update the value of the field with the provided value.
    /// This is not zero-copy API.
    fn update(&self, value: &T) -> Result<()>;

    /// This API will return a future whenever the field value is set by using `set' method of the field subscriber.
    /// Future will be resolved with value.
    fn register_set_handler<'a>(&self, callback: impl Fn(&T) + Send + 'a) -> Result<()>;
}

/// FieldSampleMut trait is used to update the value of the field sample for zero-copy API.
pub trait FieldSampleMut<T>: com_api_concept::SampleMut<T>
where
    T: CommData + Debug,
{
    fn update(self) -> Result<()>;
}
