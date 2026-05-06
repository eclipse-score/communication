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

use crate::Debug;
use core::marker::PhantomData;
use std::path::{Path, PathBuf};

use crate::{
    LolaConsumerInfo, LolaProviderInfo, Publisher, LolaConsumerDiscovery, LolaProducerBuilder,
    SubscribableImpl,
};
use com_api_concept::{
    Builder, CommData, FindServiceSpecifier, InstanceSpecifier, Interface, Result, Runtime,
};

pub struct LolaRuntimeImpl {}

impl Runtime for LolaRuntimeImpl {
    type ServiceDiscovery<I: Interface + Send> = LolaConsumerDiscovery<I>;
    type Subscriber<T: CommData + Debug> = SubscribableImpl<T>;
    type ProducerBuilder<I: Interface> = LolaProducerBuilder<I>;
    type Publisher<T: CommData + Debug> = Publisher<T>;
    type ProviderInfo = LolaProviderInfo;
    type ConsumerInfo = LolaConsumerInfo;

    fn find_service<I: Interface + Send>(
        &self,
        instance_specifier: FindServiceSpecifier,
    ) -> Self::ServiceDiscovery<I> {
        LolaConsumerDiscovery {
            instance_specifier: match instance_specifier {
                FindServiceSpecifier::Any => panic!(
                    "FindServiceSpecifier::Any is not supported in LolaRuntimeImpl,
                Please use FindServiceSpecifier::Specific with a valid instance specifier."
                ),
                FindServiceSpecifier::Specific(spec) => spec,
            },
            _interface: PhantomData,
        }
    }

    fn producer_builder<I: Interface>(
        &self,
        instance_specifier: InstanceSpecifier,
    ) -> Self::ProducerBuilder<I> {
        LolaProducerBuilder::new(self, instance_specifier)
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
