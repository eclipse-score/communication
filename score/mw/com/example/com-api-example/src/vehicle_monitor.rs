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

use com_api::{Interface, Producer, Runtime, Subscriber};
use com_api_gen::{Exhaust, Tire, VehicleInterface};
use futures::channel::oneshot;
use std::sync::{mpsc, OnceLock};

// Type aliases for generated consumer and offered producer types for the Vehicle interface
// VehicleConsumer is the consumer type generated for the Vehicle interface, parameterized by the runtime R
pub type VehicleConsumer<R> = <VehicleInterface as Interface>::Consumer<R>;
// VehicleOfferedProducer is the offered producer type generated for the Vehicle interface, parameterized by the runtime R
pub type VehicleOfferedProducer<R> =
    <<VehicleInterface as Interface>::Producer<R> as Producer<R>>::OfferedProducer;

// Example struct demonstrating composition with VehicleConsumer
pub struct VehicleMonitorProducer<R: Runtime> {
    pub(crate) producer: VehicleOfferedProducer<R>,
}

pub struct VehicleMonitorConsumer<R: Runtime> {
    pub(crate) tire_subscriber:
        <<R as Runtime>::Subscriber<Tire> as Subscriber<Tire, R>>::Subscription,
    pub(crate) _exhaust_subscriber:
        <<R as Runtime>::Subscriber<Exhaust> as Subscriber<Exhaust, R>>::Subscription,
    // The thread is spawned on the first cancellable receive call and exits when the struct is dropped.
    pub(crate) timer_tx: OnceLock<mpsc::SyncSender<oneshot::Sender<()>>>,
}
