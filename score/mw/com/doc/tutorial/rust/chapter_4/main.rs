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

//! Consumer application of the Vehicle Monitor using synchronous discovery and receive.
//! Start the producer of chapter 3 first, synchronous discovery does not wait for the service.

mod consumer;

use clap::Parser;
use consumer::VehicleMonitorConsumer;
use score_com::InstanceSpecifier;
use std::path::PathBuf;
use tutorial_common::{DEFAULT_CONFIG_PATH, SAMPLE_COUNT, SERVICE_INSTANCE_SPECIFIER};

use score_log as log;

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
    let consumer_monitor =
        VehicleMonitorConsumer::find_available_instances(&runtime, service_id).expect("Failed to create consumer");
    consumer_monitor
        .read_tire_data(SAMPLE_COUNT)
        .expect("Failed to read tire data synchronously");
    consumer_monitor.unsubscribe();
    log::info!("Consumer operation completed");
}
