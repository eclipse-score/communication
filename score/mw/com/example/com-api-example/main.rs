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

// This demo app writing and reading tire pressure data using producer and consumer respectively.
// It is demonstrating the composition of consumer and producer in one struct,
// but they can be used separately as well.
// The example is using Lola runtime, but it can be used with any runtime by changing the runtime initialization part.
// Note: The example is using unwrap and panic in some places for simplicity,
// but it is recommended to handle errors properly in production code.

use clap::Parser;
use std::path::PathBuf;

use com_api::{
    Builder, InstanceSpecifier, LolaRuntimeBuilderImpl, OfferedProducer, Runtime, RuntimeBuilder,
    Subscription,
};

use com_api_example::{VehicleMonitorConsumer, VehicleMonitorProducer};

use com_api_gen::Tire;

#[derive(Parser)]
struct Arguments {
    #[arg(
        short,
        long,
        default_value = "./score/mw/com/example/com-api-example/etc/mw_com_config.json"
    )]
    service_instance_manifest: PathBuf,
}

// Run the example with the specified runtime
fn run_with_runtime<R: Runtime>(name: &str, runtime: &R) {
    println!("\n=== Running with {name} runtime ===");

    let service_id = InstanceSpecifier::new("/Vehicle/Service1/Instance")
        .expect("Failed to create InstanceSpecifier");
    let producer_monitor = VehicleMonitorProducer::new(runtime, service_id.clone());
    let consumer_monitor = VehicleMonitorConsumer::new(runtime, service_id);

    let tire_pressure = 5.0;
    println!("Setting tire pressure to {tire_pressure}");
    for i in 0..5 {
        match producer_monitor.write_tire_data(Tire {
            pressure: tire_pressure + i as f32,
        }) {
            Ok(_) => (),
            Err(e) => {
                eprintln!("Failed to write tire data: {:?}", e);
                continue;
            }
        }
        let tire_data = match consumer_monitor.read_tire_data() {
            Ok(data) => data,
            Err(e) => {
                eprintln!("Failed to read tire data: {:?}", e);
                continue;
            }
        };
        println!("{tire_data}");
    }
    //unoffer returns producer back, so if needed it can be used further
    match producer_monitor.producer.unoffer() {
        Ok(_) => println!("Successfully unoffered the service"),
        Err(e) => eprintln!("Failed to unoffer: {:?}", e),
    }

    //UnSubscribe the event
    let _ = consumer_monitor.tire_subscriber.unsubscribe();
    println!("=== {name} runtime completed ===\n");
}

// Initialize Lola runtime builder with configuration
fn init_lola_runtime_builder(config_path: &std::path::Path) -> LolaRuntimeBuilderImpl {
    let mut lola_runtime_builder = LolaRuntimeBuilderImpl::new();
    if config_path.exists() {
        lola_runtime_builder.load_config(config_path);
    } else {
        eprintln!(
            "Provided config path does not exist: {}",
            config_path.display()
        );
    }
    lola_runtime_builder
}

fn main() {
    let args = Arguments::parse();
    let lola_runtime_builder = init_lola_runtime_builder(&args.service_instance_manifest);
    let lola_runtime = lola_runtime_builder.build().unwrap();
    run_with_runtime("Lola", &lola_runtime);
}
