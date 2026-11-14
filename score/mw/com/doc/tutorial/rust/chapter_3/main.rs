/*******************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * <https://www.apache.org/licenses/LICENSE-2.0>
 *
 * SPDX-License-Identifier: Apache-2.0
 ******************************************************************************/

//! Producer application of the Vehicle Monitor. Offers the service and publishes tire data.

mod producer;

use clap::Parser;
use producer::VehicleMonitorProducer;
use score_com::InstanceSpecifier;
use std::path::PathBuf;
use tutorial_common::{DEFAULT_CONFIG_PATH, SAMPLE_COUNT, SERVICE_INSTANCE_SPECIFIER};

use score_log as log;

/// Starting tire pressure value for the producer publish loop.
const INITIAL_TIRE_PRESSURE: f32 = 5.0;

#[derive(Parser)]
struct Arguments {
    #[arg(short, long, default_value = DEFAULT_CONFIG_PATH)]
    service_instance_manifest: PathBuf,
}

fn main() {
    tutorial_common::init_logging();
    let args = Arguments::parse();
    let runtime = tutorial_common::create_lola_runtime(&args.service_instance_manifest);

    let service_id = InstanceSpecifier::new(SERVICE_INSTANCE_SPECIFIER).expect("Failed to create InstanceSpecifier");
    let producer_monitor = VehicleMonitorProducer::new(&runtime, service_id).expect("Failed to create producer");
    log::info!("Producer created and offered successfully");
    producer_monitor.run_publish_loop(INITIAL_TIRE_PRESSURE, SAMPLE_COUNT);
    producer_monitor.unoffer();
    log::info!("Producer operation completed");
}
