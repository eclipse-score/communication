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

use core::marker::PhantomData;
use std::path::{Path, PathBuf};

use crate::{
    LolaConsumerInfo, LolaProviderInfo, Publisher, SampleConsumerDiscovery, SampleProducerBuilder,
    SubscribableImpl,
};
use com_api_concept::{
    Builder, CommData, FindServiceSpecifier, InstanceSpecifier, Interface, Result, Runtime,
};

pub struct LolaRuntimeImpl {}

impl Runtime for LolaRuntimeImpl {
    type ServiceDiscovery<I: Interface> = SampleConsumerDiscovery<I>;
    type Subscriber<T: CommData> = SubscribableImpl<T>;
    type ProducerBuilder<I: Interface> = SampleProducerBuilder<I>;
    type Publisher<T: CommData> = Publisher<T>;
    type ProviderInfo = LolaProviderInfo;
    type ConsumerInfo = LolaConsumerInfo;

    fn find_service<I: Interface>(
        &self,
        instance_specifier: FindServiceSpecifier,
    ) -> Self::ServiceDiscovery<I> {
        SampleConsumerDiscovery {
            instance_specifier: match instance_specifier {
                FindServiceSpecifier::Any => todo!(), // TODO:[eclipse-score/communication/issues/133]Add error msg or panic like "ANY not supported by Lola"
                FindServiceSpecifier::Specific(spec) => spec,
            },
            _interface: PhantomData,
        }
    }

    fn producer_builder<I: Interface>(
        &self,
        instance_specifier: InstanceSpecifier,
    ) -> Self::ProducerBuilder<I> {
        SampleProducerBuilder::new(self, instance_specifier)
    }
}

pub struct RuntimeBuilderImpl {
    config_path: Option<PathBuf>,
}

impl Builder<LolaRuntimeImpl> for RuntimeBuilderImpl {
    fn build(self) -> Result<LolaRuntimeImpl> {
        mw_com::initialize(self.config_path.as_deref());
        Ok(LolaRuntimeImpl {})
    }
}

impl com_api_concept::RuntimeBuilder<LolaRuntimeImpl> for RuntimeBuilderImpl {
    fn load_config(&mut self, config: &Path) -> &mut Self {
        self.config_path = Some(config.to_path_buf());
        self
    }
}

impl Default for RuntimeBuilderImpl {
    fn default() -> Self {
        Self::new()
    }
}

impl RuntimeBuilderImpl {
    /// Creates a new instance of the default implementation of the com layer
    pub fn new() -> Self {
        Self { config_path: None }
    }
}
