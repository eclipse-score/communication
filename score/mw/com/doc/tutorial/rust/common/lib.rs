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

//! Code shared by all Rust tutorial chapters: runtime setup, logging, and type aliases
//! for the generated Vehicle interface.

use com_api_gen::VehicleInterface;
use score_com::{Builder, Interface, LolaRuntimeBuilderImpl, LolaRuntimeImpl, Producer, RuntimeBuilder};
use stdout_logger::StdoutLoggerBuilder;

/// Instance specifier of the Vehicle service, as configured in `mw_com_config.json`.
pub const SERVICE_INSTANCE_SPECIFIER: &str = "/Vehicle/Service1/Instance";
/// Default location of the configuration file, relative to the Bazel runfiles root.
pub const DEFAULT_CONFIG_PATH: &str = "./score/mw/com/doc/tutorial/rust/common/etc/mw_com_config.json";
/// Number of samples published / received by the sample applications.
pub const SAMPLE_COUNT: usize = 5;

// [rust-type-aliases-start]
// Type aliases for generated consumer and offered producer types for the Vehicle interface
// VehicleConsumer is the consumer type generated for the Vehicle interface, parameterized by the runtime R
pub type VehicleConsumer<R> = <VehicleInterface as Interface>::Consumer<R>;
// VehicleOfferedProducer is the offered producer type generated for the Vehicle interface, parameterized by the runtime R
pub type VehicleOfferedProducer<R> = <<VehicleInterface as Interface>::Producer<R> as Producer<R>>::OfferedProducer;
// [rust-type-aliases-end]

// [rust-main-runtime-start]
/// Create the LoLa runtime from the given configuration file.
pub fn create_lola_runtime(config_path: &std::path::Path) -> LolaRuntimeImpl {
    assert!(
        config_path.exists(),
        "Config file not found: {}. Provide a valid path via --service-instance-manifest.",
        config_path.display()
    );
    let mut lola_runtime_builder = LolaRuntimeBuilderImpl::new();
    lola_runtime_builder.load_config(config_path);
    lola_runtime_builder.build().expect("Failed to build Lola runtime")
}
// [rust-main-runtime-end]

/// Log to stdout.
pub fn init_logging() {
    StdoutLoggerBuilder::new()
        .show_module(false)
        .show_file(true)
        .show_line(false)
        .set_as_default_logger();
}

// Compile-only demonstration of an interface mixing events, fields and methods.
pub mod mixed_monitor;
