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
#[cfg(feature = "iceoryx")]
use iceoryx2_qnx8::prelude::ZeroCopySend;

use com_api::{
    Consumer, Interface, OfferedProducer, Producer, Publisher, Reloc, Runtime, Subscriber,
};

#[derive(Debug)]
pub struct Tire {}
unsafe impl Reloc for Tire {}
#[cfg(feature = "iceoryx")]
unsafe impl ZeroCopySend for Tire {}

#[derive(Debug)]
pub struct Exhaust {}
unsafe impl Reloc for Exhaust {}
#[cfg(feature = "iceoryx")]
unsafe impl ZeroCopySend for Exhaust {}

#[derive(Debug)]
pub struct VehicleInterface {}

/// Generic
impl Interface for VehicleInterface {
    const TYPE_ID: &'static str = "VehicleInterface";
    type Consumer<R: Runtime + ?Sized> = VehicleConsumer<R>;
    type Producer<R: Runtime + ?Sized> = VehicleProducer<R>;
}

pub struct VehicleConsumer<R: Runtime + ?Sized> {
    pub left_tire: R::Subscriber<Tire>,
    pub exhaust: R::Subscriber<Exhaust>,
}

impl<R: Runtime + ?Sized> Consumer<R> for VehicleConsumer<R> {
    fn new(instance_info: R::ConsumerInfo) -> Self {
        VehicleConsumer {
            left_tire: R::Subscriber::new("left_tire", instance_info.clone())
                .expect("Failed to create subscriber"),
            exhaust: R::Subscriber::new("exhaust", instance_info.clone())
                .expect("Failed to create subscriber"),
        }
    }
}

pub struct AnotherInterface {}

pub struct VehicleProducer<R: Runtime + ?Sized> {
    _runtime: core::marker::PhantomData<R>,
    instance_info: R::ProviderInfo,
}

impl<R: Runtime + ?Sized> Producer<R> for VehicleProducer<R> {
    type Interface = VehicleInterface;
    type OfferedProducer = VehicleOfferedProducer<R>;

    fn offer(self) -> com_api::Result<Self::OfferedProducer> {
        Ok(VehicleOfferedProducer {
            left_tire: R::Publisher::new("left_tire", self.instance_info.clone())
                .expect("Failed to create publisher"),
            exhaust: R::Publisher::new("exhaust", self.instance_info.clone())
                .expect("Failed to create publisher"),
            instance_info: self.instance_info,
        })
    }

    fn new(instance_info: R::ProviderInfo) -> com_api::Result<Self> {
        Ok(VehicleProducer {
            _runtime: core::marker::PhantomData,
            instance_info,
        })
    }
}

pub struct VehicleOfferedProducer<R: Runtime + ?Sized> {
    pub left_tire: R::Publisher<Tire>,
    pub exhaust: R::Publisher<Exhaust>,
    instance_info: R::ProviderInfo,
}

impl<R: Runtime + ?Sized> OfferedProducer<R> for VehicleOfferedProducer<R> {
    type Interface = VehicleInterface;
    type Producer = VehicleProducer<R>;

    fn unoffer(self) -> Self::Producer {
        VehicleProducer {
            _runtime: std::marker::PhantomData,
            instance_info: self.instance_info,
        }
    }
}
