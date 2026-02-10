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

use com_api::{
    CommData, Consumer, Interface, OfferedProducer, Producer, ProviderInfo, Publisher, Reloc,
    Runtime, Subscriber,
};

#[derive(Debug, Reloc)]
#[repr(C)]
pub struct Tire {
    pub pressure: f32,
}

impl CommData for Tire {
    const ID: &'static str = "Tire";
}

#[derive(Debug, Reloc)]
#[repr(C)]
pub struct Exhaust {}

impl CommData for Exhaust {
    const ID: &'static str = "Exhaust";
}

pub struct VehicleInterface {}

impl Interface for VehicleInterface {
    const INTERFACE_ID: &'static str = "VehicleInterface";
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
        let vehicle_offered_producer = VehicleOfferedProducer {
            left_tire: R::Publisher::new("left_tire", self.instance_info.clone())
                .expect("Failed to create publisher"),
            exhaust: R::Publisher::new("exhaust", self.instance_info.clone())
                .expect("Failed to create publisher"),
            instance_info: self.instance_info.clone(),
        };
        // Offer the service instance to make it discoverable
        // this is called after skeleton created using producer_builder API
        self.instance_info.offer_service()?;
        Ok(vehicle_offered_producer)
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

    fn unoffer(self) -> com_api::Result<Self::Producer> {
        let vehicle_producer = VehicleProducer {
            _runtime: std::marker::PhantomData,
            instance_info: self.instance_info.clone(),
        };
        // Stop offering the service instance to withdraw it from system availability
        self.instance_info.stop_offer_service()?;
        Ok(vehicle_producer)
    }
}
